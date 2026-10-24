"""Universal Jump save message pipe; serviced on the host application's UI thread."""
import ctypes
from ctypes import wintypes as wt
import os
import time

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
user = ctypes.WinDLL('user32', use_last_error=True)
for name, args in {
    'CreateNamedPipeW': [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, wt.LPVOID],
    'ConnectNamedPipe': [wt.HANDLE, wt.LPVOID], 'DisconnectNamedPipe': [wt.HANDLE], 'CloseHandle': [wt.HANDLE],
    'ReadFile': [wt.HANDLE, wt.LPVOID, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.LPVOID],
    'WriteFile': [wt.HANDLE, wt.LPVOID, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.LPVOID],
    'PeekNamedPipe': [wt.HANDLE, wt.LPVOID, wt.DWORD, wt.LPVOID, ctypes.POINTER(wt.DWORD), wt.LPVOID],
    'GetNamedPipeClientProcessId': [wt.HANDLE, ctypes.POINTER(wt.ULONG)],
    'OpenProcess': [wt.DWORD, wt.BOOL, wt.DWORD],
    'QueryFullProcessImageNameW': [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)],
}.items():
    getattr(kernel, name).argtypes = args
kernel.CreateNamedPipeW.restype = kernel.OpenProcess.restype = wt.HANDLE
user.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user.GetForegroundWindow.restype = wt.HWND
user.IsWindowEnabled.argtypes = [wt.HWND]
user.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
CALLBACK = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user.EnumWindows.argtypes = [CALLBACK, wt.LPARAM]


def pid(hwnd):
    value = wt.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(value))
    return value.value


def window_class(hwnd):
    buffer = ctypes.create_unicode_buffer(256)
    user.GetClassNameW(hwnd, buffer, len(buffer))
    return buffer.value


def executable(process):
    handle = kernel.OpenProcess(0x1000, False, process)
    if not handle:
        return ''
    try:
        text = ctypes.create_unicode_buffer(32768)
        size = wt.DWORD(len(text))
        return os.path.normcase(text.value) if kernel.QueryFullProcessImageNameW(handle, 0, text, ctypes.byref(size)) else ''
    finally:
        kernel.CloseHandle(handle)


def notify(hwnd, allowed):
    @CALLBACK
    def visit(controller, _):
        if window_class(controller) == 'Jump.Control' and executable(pid(controller)) in allowed:
            user.PostMessageW(controller, 0x8000 + 11, hwnd, os.getpid())
        return True
    user.EnumWindows(visit, 0)


class Server:
    def __init__(self, serve):
        self.serve = serve
        self.handle = None
        self.pending = b''
        self.connected = self.responded = False
        self.sent_at = 0.0
        self.connected_at = 0.0

    def close(self):
        if self.handle is not None:
            kernel.DisconnectNamedPipe(self.handle)
            kernel.CloseHandle(self.handle)
        self.handle = None
        self.pending = b''
        self.connected = self.responded = False

    def tick(self):
        if self.handle is None:
            self.handle = kernel.CreateNamedPipeW('\\\\.\\pipe\\Jump.Save.' + str(os.getpid()),
                3 | 0x80000, 4 | 2 | 1 | 8, 1, 65536, 65536, 0, None)
            if self.handle == ctypes.c_void_p(-1).value:
                self.handle = None
                return
        if not self.connected:
            connected = kernel.ConnectNamedPipe(self.handle, None)
            error = ctypes.get_last_error()
            self.connected = bool(connected or error == 535)
            if self.connected:
                self.connected_at = time.monotonic()
            if not self.connected and error in {109, 232}:
                kernel.DisconnectNamedPipe(self.handle)
        available = wt.DWORD()
        if self.connected and not kernel.PeekNamedPipe(self.handle, None, 0, None, ctypes.byref(available), None):
            self.close()
            return
        if not self.connected:
            return
        if time.monotonic() - self.connected_at > 4:
            self.close()
            return
        if not self.pending and not self.responded and available.value:
            buffer = ctypes.create_string_buffer(32768)
            read = wt.DWORD()
            if kernel.ReadFile(self.handle, buffer, len(buffer), ctypes.byref(read), None) and read.value:
                client = wt.ULONG()
                try:
                    if not kernel.GetNamedPipeClientProcessId(self.handle, ctypes.byref(client)):
                        raise ValueError('无法核验请求进程')
                    reply = self.serve(buffer.raw[:read.value].decode('utf-16-le'), client.value)
                except Exception as exc:
                    reply = 'JUMP_SAVE_1\nERROR\n' + str(exc).replace('\n', ' ').replace('\r', ' ')[:300]
                self.pending = reply.encode('utf-16-le')
                self.sent_at = time.monotonic()
            else:
                self.close()
                return
        if self.pending:
            written = wt.DWORD()
            if kernel.WriteFile(self.handle, self.pending, len(self.pending), ctypes.byref(written), None) and written.value == len(self.pending):
                self.pending = b''
                self.responded = True
            elif time.monotonic() - self.sent_at > 3:
                self.close()
