import ctypes as c
from ctypes import wintypes as w
import sys
u = c.WinDLL('user32', use_last_error=True)
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetDlgCtrlID.argtypes = [w.HWND]
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
CB = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
def text(h, fn):
    b = c.create_unicode_buffer(4096); fn(h, b, len(b)); return b.value
@CB
def child(h, _):
    print(hex(h), text(h, u.GetClassNameW), u.GetDlgCtrlID(h), repr(text(h, u.GetWindowTextW)))
    return True
@CB
def top(h, _):
    pid = w.DWORD(); u.GetWindowThreadProcessId(h, c.byref(pid))
    if pid.value == int(sys.argv[1]):
        print('TOP', hex(h), text(h, u.GetClassNameW), repr(text(h, u.GetWindowTextW)))
        u.EnumChildWindows(h, child, 0)
        if '--close' in sys.argv and text(h, u.GetClassNameW) == 'Jump.Control':
            u.PostMessageW(h, 0x10, 0, 0)
    return True
u.EnumWindows(top, 0)
