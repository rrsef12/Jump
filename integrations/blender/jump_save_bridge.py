"""Jump Blender bridge v1: file-browser parameters only, on Blender's UI thread.

No sockets, background threads, scene edits, injected keys or replacement export
operators. IPC runs only while a modal file browser exists. The draw handler wakes
the timer when a browser opens; idle Blender has no repeating bridge timer.
"""
import ctypes
import hashlib
from ctypes import wintypes as wt
import json
import os
from pathlib import Path
import re

import bpy
from bpy.app.handlers import persistent

PROTOCOL = "JUMP_SAVE_1"
NOTIFY = 0x8000 + 11
PROPERTY = "Jump.SaveBridge.v1"
_pipe = None
_handler = None
_last_state = None
_allowed = set()
_enabled = False
_pending = b""
_pending_at = 0.0
_connected = False
_responded = False

kernel = ctypes.WinDLL("kernel32", use_last_error=True)
user = ctypes.WinDLL("user32", use_last_error=True)
kernel.CreateNamedPipeW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, wt.DWORD, wt.LPVOID]
kernel.CreateNamedPipeW.restype = wt.HANDLE
kernel.ConnectNamedPipe.argtypes = [wt.HANDLE, wt.LPVOID]
kernel.DisconnectNamedPipe.argtypes = [wt.HANDLE]
kernel.CloseHandle.argtypes = [wt.HANDLE]
kernel.ReadFile.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD, ctypes.POINTER(wt.DWORD), wt.LPVOID]
kernel.WriteFile.argtypes = kernel.ReadFile.argtypes
kernel.PeekNamedPipe.argtypes = [wt.HANDLE, wt.LPVOID, wt.DWORD, wt.LPVOID, ctypes.POINTER(wt.DWORD), wt.LPVOID]
kernel.GetNamedPipeClientProcessId.argtypes = [wt.HANDLE, ctypes.POINTER(wt.ULONG)]
kernel.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
kernel.OpenProcess.restype = wt.HANDLE
kernel.QueryFullProcessImageNameW.argtypes = [wt.HANDLE, wt.DWORD, wt.LPWSTR, ctypes.POINTER(wt.DWORD)]
user.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user.GetForegroundWindow.restype = wt.HWND
user.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user.GetClassNameW.argtypes = user.GetWindowTextW.argtypes
user.IsWindowVisible.argtypes = [wt.HWND]
user.IsWindowEnabled.argtypes = [wt.HWND]
user.PostMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
user.SetPropW.argtypes = [wt.HWND, wt.LPCWSTR, wt.HANDLE]
user.RemovePropW.argtypes = [wt.HWND, wt.LPCWSTR]
CALLBACK = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user.EnumWindows.argtypes = [CALLBACK, wt.LPARAM]


def _exe(pid):
    handle = kernel.OpenProcess(0x1000, False, pid)
    if not handle:
        return ""
    try:
        buffer = ctypes.create_unicode_buffer(32768)
        size = wt.DWORD(len(buffer))
        return os.path.normcase(buffer.value) if kernel.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)) else ""
    finally:
        kernel.CloseHandle(handle)


def _pid(hwnd):
    pid = wt.DWORD()
    user.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
    return pid.value


def _text(hwnd, cls=False):
    buffer = ctypes.create_unicode_buffer(512)
    (user.GetClassNameW if cls else user.GetWindowTextW)(hwnd, buffer, len(buffer))
    return buffer.value


def _windows():
    result = []
    @CALLBACK
    def visit(hwnd, _):
        if user.IsWindowVisible(hwnd) and _pid(hwnd) == os.getpid():
            result.append((hwnd, os.getpid(), _text(hwnd), _text(hwnd, True)))
        return True
    user.EnumWindows(visit, 0)
    return result


def _browsers():
    result = []
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type != 'FILE_BROWSER':
                continue
            space = area.spaces.active
            if space.browse_mode == 'FILES' and space.params and space.active_operator:
                result.append((window, area, space))
    return result


def _native_browser():
    names = {"Blender File View", "Blender文件视图", "Blender 文件视图"}
    windows = [w[0] for w in _windows() if w[1] == os.getpid() and w[2] in names]
    return windows[0] if len(windows) == 1 else None


def _extension(space):
    operator = space.active_operator
    identifier = operator.bl_idname
    if identifier in {'WM_OT_save_as_mainfile', 'WM_OT_save_mainfile', 'WM_OT_open_mainfile'}:
        return '.blend'
    if identifier == 'EXPORT_SCENE_OT_gltf':
        return '.glb' if operator.export_format == 'GLB' else '.gltf'
    patterns = re.findall(r'\*([.][A-Za-z0-9_.]+)', getattr(space.params, 'filter_glob', ''))
    matching = [ext for ext in patterns if space.params.filename.lower().endswith(ext.lower())]
    if matching:
        return max(matching, key=len)
    extension = getattr(operator, 'filename_ext', '')
    if extension and re.fullmatch(r'\.[A-Za-z0-9_.]+', extension):
        return extension
    if len(patterns) == 1:
        return patterns[0]
    return Path(space.params.filename).suffix


def _snapshot(space, submitted=False, navigated=False):
    identifier = space.active_operator.bl_idname
    save = identifier.startswith('EXPORT_') or identifier in {
        'WM_OT_save_as_mainfile', 'WM_OT_save_mainfile', 'IMAGE_OT_save_as',
        'RENDER_OT_opengl', 'WM_OT_alembic_export', 'WM_OT_usd_export',
        'WM_OT_obj_export', 'WM_OT_ply_export', 'WM_OT_stl_export',
        'WM_OT_collada_export', 'WM_OT_gpencil_export_svg', 'WM_OT_grease_pencil_export_svg',
        'WM_OT_grease_pencil_export_pdf', 'WM_OT_gpencil_export_pdf'}
    # Other built-in/third-party exporters generally expose *_export IDs.
    save = save or identifier.lower().endswith('_export')
    extension = _extension(space)
    name = space.params.filename
    directory = os.fsdecode(space.params.directory)
    revision = hashlib.sha256(('Jump.Save.Blender\n' + name + '\n' + directory + '\n' + extension).encode()).hexdigest()
    values = [PROTOCOL, 'OK', '1' if save else '0', name, directory, extension, 'Blender ' + extension,
              bpy.data.filepath, '1' if submitted else '0', '1' if navigated else '0', revision, '1']
    if any('\n' in value or '\r' in value for value in values):
        raise ValueError('文件路径含不支持的换行符')
    return '\n'.join(values)


def _serve(message, client_pid):
    fields = message.split('\n')
    if len(fields) not in {8, 12} or fields[0] != PROTOCOL or fields[1] not in {'INSPECT', 'APPLY'}:
        raise ValueError('无效的桥接请求')
    if _exe(client_pid) not in _allowed:
        raise ValueError('请求不来自已配置的 Jump')
    hwnd, panel = int(fields[2]), int(fields[7])
    browsers = _browsers()
    if len(browsers) != 1 or hwnd != _native_browser() or _pid(hwnd) != os.getpid():
        raise ValueError('未找到唯一的 Blender 文件窗口')
    window, area, space = browsers[0]
    if fields[1] == 'INSPECT':
        if len(fields) != 8 or any(fields[i] for i in (3, 4, 5)) or fields[6] != '0':
            raise ValueError('无效的检查请求')
        return _snapshot(space)
    foreground = user.GetForegroundWindow()
    panel_valid = _pid(panel) == client_pid and _text(panel, True) == 'Jump.Panel'
    if foreground != hwnd and not (panel_valid and foreground == panel):
        raise ValueError('前台已切换，未执行操作')
    if not user.IsWindowEnabled(hwnd):
        raise ValueError('Blender 正在等待其他窗口，未执行操作')
    directory, name, requested_extension, submit = fields[3:7]
    current_snapshot = _snapshot(space)
    current_fields = current_snapshot.split('\n')
    if len(fields) != 12 or requested_extension != '' or fields[8] != current_fields[3] or \
            fields[9] != current_fields[4] or fields[10] != current_fields[10]:
        raise ValueError('Blender 保存目标已改变，请重试')
    extension = _extension(space)
    save = _snapshot(space).split('\n')[2] == '1'
    if submit == '1' and (not name or len(name) > 240 or any(c in name for c in '\\/:*?"<>|\r\n') or name[-1] in '. '):
        raise ValueError('文件名无效')
    if submit == '1' and not save:
        raise ValueError('此窗口不是保存或导出窗口')
    old_directory = os.fsdecode(space.params.directory)
    navigated = bool(directory and os.path.normcase(os.path.normpath(directory)) != os.path.normcase(os.path.normpath(old_directory)))
    if navigated and (not os.path.isabs(directory) or not os.path.isdir(directory)):
        raise ValueError('目录不存在')
    if navigated:
        space.params.directory = os.fsencode(os.path.join(directory, ''))
    if save:
        if name and not name.lower().endswith(extension.lower()):
            raise ValueError('文件名扩展名已改变，请重试')
        space.params.filename = name
    area.tag_redraw()
    if save and space.params.filename != name:
        raise ValueError('文件名填入核验失败，未提交')
    reply = _snapshot(space, submit == '1', navigated)
    if submit == '1':
        region = next(r for r in area.regions if r.type == 'WINDOW')
        with bpy.context.temp_override(window=window, area=area, region=region):
            result = bpy.ops.file.execute()
        if 'CANCELLED' in result:
            raise ValueError('Blender 未接受保存或导出操作')
    return reply


def _close_pipe():
    global _pipe, _pending, _connected, _responded
    if _pipe is not None:
        kernel.DisconnectNamedPipe(_pipe)
        kernel.CloseHandle(_pipe)
    _pipe = None
    _pending = b''
    _connected = _responded = False


def _notify(hwnd, state):
    global _last_state
    if state == _last_state:
        return
    if _last_state and _last_state[0] != hwnd:
        user.RemovePropW(_last_state[0], PROPERTY)
    _last_state = state
    user.SetPropW(hwnd, PROPERTY, os.getpid())
    @CALLBACK
    def visit(controller, _):
        if _text(controller, True) == 'Jump.Control' and _exe(_pid(controller)) in _allowed:
            user.PostMessageW(controller, NOTIFY, hwnd, os.getpid())
        return True
    user.EnumWindows(visit, 0)


def _tick():
    global _pipe, _pending, _pending_at, _last_state, _connected, _responded
    import time
    if not _enabled or not _browsers():
        if _last_state:
            user.RemovePropW(_last_state[0], PROPERTY)
        _close_pipe(); _last_state = None
        return None
    if _pipe is None:
        # One local message pipe per Blender process, non-blocking on the UI thread.
        _pipe = kernel.CreateNamedPipeW('\\\\.\\pipe\\Jump.Save.' + str(os.getpid()),
                                      3 | 0x80000, 4 | 2 | 1 | 8, 1, 65536, 65536, 0, None)
        if _pipe == ctypes.c_void_p(-1).value:
            _pipe = None
            return 0.25
    if not _connected:
        connected = kernel.ConnectNamedPipe(_pipe, None)
        error = ctypes.get_last_error()
        _connected = bool(connected or error == 535)
        if not _connected and error in {109, 232}:
            kernel.DisconnectNamedPipe(_pipe)
    available = wt.DWORD()
    if _connected and not kernel.PeekNamedPipe(_pipe, None, 0, None, ctypes.byref(available), None):
        kernel.DisconnectNamedPipe(_pipe)
        _connected = _responded = False
        _pending = b''
    if _connected:
        if _pending:
            written = wt.DWORD()
            if kernel.WriteFile(_pipe, _pending, len(_pending), ctypes.byref(written), None):
                _pending = b''; _responded = True
            elif time.monotonic() - _pending_at > 3:
                _pending = b''; kernel.DisconnectNamedPipe(_pipe); _connected = _responded = False
        elif not _responded and available.value:
            buffer = ctypes.create_string_buffer(32768)
            read = wt.DWORD()
            if kernel.ReadFile(_pipe, buffer, len(buffer), ctypes.byref(read), None) and read.value:
                client_pid = wt.ULONG()
                try:
                    if not kernel.GetNamedPipeClientProcessId(_pipe, ctypes.byref(client_pid)):
                        raise ValueError('无法核验请求进程')
                    reply = _serve(buffer.raw[:read.value].decode('utf-16-le'), client_pid.value)
                except Exception as exc:
                    reply = PROTOCOL + '\nERROR\n' + str(exc).replace('\n', ' ')[:300]
                _pending = reply.encode('utf-16-le')
                _pending_at = time.monotonic()
                written = wt.DWORD()
                if kernel.WriteFile(_pipe, _pending, len(_pending), ctypes.byref(written), None):
                    _pending = b''; _responded = True
            elif ctypes.get_last_error() in {109, 232}:
                kernel.DisconnectNamedPipe(_pipe)
                _connected = _responded = False
    hwnd = _native_browser()
    browsers = _browsers()
    if hwnd and len(browsers) == 1:
        _notify(hwnd, (hwnd, _snapshot(browsers[0][2])))
    return 0.12


def _wake():
    if _enabled and not bpy.app.timers.is_registered(_tick):
        bpy.app.timers.register(_tick, first_interval=0.03)


def _draw():
    space = bpy.context.space_data
    if space and space.type == 'FILE_BROWSER' and space.browse_mode == 'FILES' and space.active_operator:
        _wake()


@persistent
def _loaded(_):
    _close_pipe()
    _wake()


def register():
    global _handler, _allowed, _enabled
    if bpy.app.background or os.name != 'nt' or _enabled:
        return
    config = Path(__file__).with_suffix('.json')
    _allowed = {os.path.normcase(os.path.abspath(p)) for p in json.loads(config.read_text(encoding='utf-8-sig'))['apps']}
    _enabled = True
    _handler = bpy.types.SpaceFileBrowser.draw_handler_add(_draw, (), 'WINDOW', 'POST_PIXEL')
    bpy.app.handlers.load_post.append(_loaded)
    _wake()


def unregister():
    global _enabled, _handler
    _enabled = False
    if bpy.app.timers.is_registered(_tick):
        bpy.app.timers.unregister(_tick)
    if _handler is not None:
        bpy.types.SpaceFileBrowser.draw_handler_remove(_handler, 'WINDOW')
    _handler = None
    if _loaded in bpy.app.handlers.load_post:
        bpy.app.handlers.load_post.remove(_loaded)
    _close_pipe()
    if _last_state:
        user.RemovePropW(_last_state[0], PROPERTY)
