"""Jump Houdini native-save adapter v2: preserve the original file chooser."""
import atexit
import ctypes
import functools
import hashlib
import json
import ntpath
import os
from pathlib import Path
import traceback
import hou
from PySide6 import QtCore, QtWidgets
import shiboken6
import jump_pipe

PROTOCOL = 'JUMP_SAVE_1'
_TITLES = {'Save As', '另存为'}
_filter = _timer = _server = _native = None
_allowed = set()
_last = None
_external = 0
_original_select = _guarded_select = None


def _candidates():
    if _external:
        return []
    return [w for w in QtWidgets.QApplication.topLevelWidgets()
            if w.isVisible() and w.objectName() == 'RE_Window' and w.windowTitle() in _TITLES]


def _pointer(widget):
    return shiboken6.getCppPointer(widget)[0]


def _read(widget):
    fields = _native.jump_houdini_inspect(_pointer(widget)).decode('utf-8').split('\n')
    if len(fields) != 4 or fields[0] != 'OK':
        raise ValueError(fields[-1] if fields else '无法读取 Houdini 原文件窗口')
    raw_name, raw_directory = fields[1:3]
    look_in = ntpath.normpath(hou.expandString(raw_directory))
    destination = ntpath.normpath(ntpath.join(look_in, hou.expandString(raw_name)))
    name = ntpath.basename(destination) if raw_name else ''
    extensions = ('.hip.gz', '.hiplc', '.hipnc', '.hip')
    extension = next((ext for ext in extensions if name.lower().endswith(ext)), '')
    if not extension:
        source = hou.hipFile.path().lower()
        extension = next((ext for ext in extensions if source.endswith(ext)), '.hip')
    return name, ntpath.dirname(destination) if raw_name else look_in, extension, look_in


def _snapshot(widget, submitted=False, navigated=False):
    name, directory, extension, _ = _read(widget)
    revision = hashlib.sha256(('Jump.Save.Houdini\n' + name + '\n' + directory + '\n' + extension).encode()).hexdigest()
    fields = [PROTOCOL, 'OK', '1', name, directory, extension, 'Houdini ' + extension,
              hou.hipFile.path(), '1' if submitted else '0', '1' if navigated else '0', revision, '1']
    if any('\n' in item or '\r' in item for item in fields):
        raise ValueError('路径包含不支持的换行符')
    return '\n'.join(fields)


def _serve(message, client):
    fields = message.split('\n')
    if len(fields) not in {8, 12} or fields[0] != PROTOCOL or fields[1] not in {'INSPECT', 'APPLY'}:
        raise ValueError('无效的 Houdini 请求')
    if jump_pipe.executable(client) not in _allowed:
        raise ValueError('请求不来自已配置的 Jump')
    browsers = [w for w in _candidates() if int(w.winId()) == int(fields[2])]
    if len(browsers) != 1:
        raise ValueError('原 Houdini 保存窗口已关闭或改变')
    widget = browsers[0]
    if fields[1] == 'INSPECT':
        if len(fields) != 8 or any(fields[i] for i in (3, 4, 5)) or fields[6] != '0':
            raise ValueError('无效的检查请求')
        return _snapshot(widget)
    hwnd, panel = int(fields[2]), int(fields[7])
    foreground = jump_pipe.user.GetForegroundWindow()
    if foreground != hwnd and not (foreground == panel and jump_pipe.pid(panel) == client
                                   and jump_pipe.window_class(panel) == 'Jump.Panel'):
        raise ValueError('前台已切换，未填入名称')
    if not jump_pipe.user.IsWindowEnabled(hwnd):
        raise ValueError('Houdini 正在等待其他窗口')
    current, directory, extension, look_in = _read(widget)
    requested, complete_name, requested_extension, submit = fields[3:7]
    current_snapshot = _snapshot(widget).split('\n')
    if len(fields) != 12 or requested_extension != '' or fields[8] != current_snapshot[3] or \
            fields[9] != current_snapshot[4] or fields[10] != current_snapshot[10]:
        raise ValueError('Houdini 保存目标已改变，请重试')
    if submit == '1' and (not complete_name or len(complete_name) > 240 or complete_name[-1] in '. ' or any(ord(c) < 32 or c in '\\/:*?"<>|' for c in complete_name)):
        raise ValueError('文件名无效')
    destination = requested or directory
    if not ntpath.isabs(destination) or not os.path.isdir(destination):
        raise ValueError('目录不存在')
    navigated = ntpath.normcase(ntpath.normpath(destination)) != ntpath.normcase(look_in)
    # Snapshot before enqueueing the original Accept action, which may destroy its chooser.
    if complete_name and not complete_name.lower().endswith(extension.lower()):
        raise ValueError('文件名扩展名已改变，请重试')
    revision = _snapshot(widget).split('\n')[10]
    reply = [PROTOCOL, 'OK', '1', complete_name, destination, extension, 'Houdini ' + extension,
             hou.hipFile.path(), '1' if submit == '1' else '0', '1' if navigated else '0', '', '1']
    result = _native.jump_houdini_apply(_pointer(widget), complete_name.encode('utf-8'),
        destination.replace('\\', '/').encode('utf-8') if navigated else b'', submit == '1').decode('utf-8')
    if not result.startswith('OK\n'):
        raise ValueError(result.split('\n')[-1])
    if submit != '1':
        revision = _snapshot(widget).split('\n')[10]
    reply[10] = revision
    return '\n'.join(reply)


def _tick():
    global _last
    try:
        browsers = _candidates()
        if not browsers:
            if _last:
                jump_pipe.unnotify(_last[0])
            _timer.setInterval(250); _server.close(); _last = None
            return
        _timer.setInterval(50)
        _server.tick()
        if len(browsers) == 1:
            widget = browsers[0]
            try:
                snapshot = (int(widget.winId()), _snapshot(widget))
            except ValueError:
                return  # Same title on an unrelated chooser; never attach.
            if snapshot != _last:
                if _last:
                    jump_pipe.unnotify(_last[0])
                _last = snapshot
                jump_pipe.notify(snapshot[0], _allowed)
    except (RuntimeError, AttributeError):
        return


def _wake():
    if _filter is not None and _timer is not None and _candidates() and _timer.interval() != 50:
        _timer.start(50)
        _tick()


class NativeSaveFilter(QtCore.QObject):
    def eventFilter(self, obj, event):
        try:
            if _filter is not None and _timer is not None and _timer.interval() != 50 and event.type() in (
                    QtCore.QEvent.Show, QtCore.QEvent.WindowTitleChange, QtCore.QEvent.WindowActivate):
                if isinstance(obj, QtWidgets.QWidget) and obj.window().windowTitle() in _TITLES:
                    QtCore.QTimer.singleShot(0, _wake)
        except (RuntimeError, AttributeError):
            pass
        return False


def register():
    global _native, _allowed, _filter, _timer, _server, _original_select, _guarded_select
    if _filter is not None or os.name != 'nt':
        return
    try:
        options = json.loads(Path(__file__).with_suffix('.json').read_text(encoding='utf-8-sig'))
        if hou.applicationVersionString() != options['houdini_build']:
            return
        _native = ctypes.CDLL(str(Path(__file__).parent / options['native_library']))
        _native.jump_houdini_version.restype = ctypes.c_char_p
        if _native.jump_houdini_version().decode() != hou.applicationVersionString():
            return
        _native.jump_houdini_inspect.argtypes = [ctypes.c_void_p]
        _native.jump_houdini_inspect.restype = ctypes.c_char_p
        _native.jump_houdini_apply.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_bool]
        _native.jump_houdini_apply.restype = ctypes.c_char_p
        _allowed = {os.path.normcase(os.path.abspath(p)) for p in options['apps']}
        app = QtWidgets.QApplication.instance()
        _original_select = hou.ui.selectFile
        @functools.wraps(_original_select)
        def guarded(*args, **kwargs):
            global _external
            _external += 1
            try:
                return _original_select(*args, **kwargs)
            finally:
                _external -= 1
        _guarded_select = guarded
        hou.ui.selectFile = guarded
        _server = jump_pipe.Server(_serve)
        _filter = NativeSaveFilter(app)
        _timer = QtCore.QTimer(app); _timer.timeout.connect(_tick)
        app.installEventFilter(_filter)
        app.aboutToQuit.connect(unregister)
        atexit.register(unregister)
        # Houdini reuses custom RE windows without reliably emitting Qt Show events.
        # Cheap QWidget discovery stays active; native inspection/IPC run only while open.
        _timer.start(250)
        _wake()
    except (OSError, ValueError, KeyError, AttributeError):
        traceback.print_exc()


def unregister():
    global _filter, _timer, _last
    observer, timer = _filter, _timer
    _filter = _timer = _last = None
    app = QtWidgets.QApplication.instance()
    if observer is not None and app is not None:
        try:
            app.removeEventFilter(observer); app.aboutToQuit.disconnect(unregister); observer.deleteLater()
        except RuntimeError:
            pass
    if timer is not None:
        try:
            timer.stop(); timer.deleteLater()
        except RuntimeError:
            pass
    if _server is not None:
        _server.close()
    if hou.ui.selectFile is _guarded_select:
        hou.ui.selectFile = _original_select
    atexit.unregister(unregister)
