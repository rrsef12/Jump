"""Windows save-destination SDK. Call tick() on the application's UI thread.

No input injection, clipboard, file writes or Save invocation. The application
supplies read/apply callbacks for its own pending save/export operation.
"""
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import threading
import uuid
import ctypes
from ctypes import wintypes as wt
from jump_save_pipe import Server, user, pid, executable, window_class, notify

PROPERTY = 'Jump.SaveBridge.v1'
PROTOCOL = 'JUMP_SAVE_1'
user.SetPropW.argtypes = [wt.HWND, wt.LPCWSTR, wt.HANDLE]
user.RemovePropW.argtypes = [wt.HWND, wt.LPCWSTR]
user.IsWindowVisible.argtypes = [wt.HWND]


@dataclass(frozen=True)
class Snapshot:
    directory: str
    name: str
    extension: str = ''
    format: str = ''
    source: str = ''


def _line(value):
    if not isinstance(value, str) or any(c in value for c in '\r\n\0') or len(value) > 8000:
        raise ValueError('Invalid bridge field')
    return value


def _filename(value):
    base = value.split('.')[0].lower()
    reserved = base in ('con', 'prn', 'aux', 'nul') or (len(base) == 4 and base[:3] in ('com', 'lpt') and base[3] in '123456789')
    return value == '' or (not reserved and len(value) <= 240 and value not in ('.', '..') and
        not any(ord(c) < 32 or c in '<>:"/\\|?*' for c in value) and not value.endswith((' ', '.')))


class Bridge:
    """One server per host process; attach only the active save/export window.

    read() -> Snapshot | None (None means no pending save operation).
    apply(directory, complete_filename) changes the pending destination only.
    allowed_jump_paths is an explicit allowlist of trusted Jump executables.
    """
    def __init__(self, allowed_jump_paths, submit=None):
        self.allowed = {os.path.normcase(str(Path(p).resolve())) for p in allowed_jump_paths}
        if not self.allowed:
            raise ValueError('An explicit Jump executable allowlist is required')
        self.thread = threading.get_ident()
        self.hwnd = None
        self.server = Server(self._serve)
        self.read = self.apply = None
        self.submit = submit
        self.session = ''

    def _ui_thread(self):
        if threading.get_ident() != self.thread:
            raise RuntimeError('Bridge must run on the application UI thread')

    def attach(self, hwnd, read, apply):
        self._ui_thread()
        self.detach()
        if pid(hwnd) != os.getpid():
            raise ValueError('Window must belong to this process')
        self.hwnd, self.read, self.apply = hwnd, read, apply
        self.session = uuid.uuid4().hex
        self._snapshot()
        if not user.SetPropW(hwnd, PROPERTY, os.getpid()):
            self.detach()
            raise OSError('Cannot advertise save bridge')
        notify(hwnd, self.allowed)

    def detach(self):
        self._ui_thread()
        if self.hwnd:
            user.RemovePropW(self.hwnd, PROPERTY)
            notify(self.hwnd, self.allowed)
        self.hwnd = self.read = self.apply = None
        self.server.close()

    def tick(self):
        self._ui_thread()
        if self.hwnd:
            if pid(self.hwnd) != os.getpid():
                self.detach()
            else:
                self.server.tick()

    def _snapshot(self):
        s = self.read()
        if not isinstance(s, Snapshot):
            raise ValueError('No active save operation')
        fields = [_line(s.name), _line(s.directory), _line(s.extension), _line(s.format), _line(s.source)]
        if not Path(s.directory).is_absolute() or not _filename(s.name):
            raise ValueError('Invalid save destination')
        if s.extension and (not s.extension.startswith('.') or not _filename('file' + s.extension)):
            raise ValueError('Invalid extension')
        revision = hashlib.sha256(('\n'.join([self.session] + fields)).encode('utf-8')).hexdigest()
        return s, revision

    def _serve(self, message, client):
        self._ui_thread()
        if executable(client) not in self.allowed:
            raise ValueError('Jump executable is not in the allowlist')
        fields = message.split('\n')
        if len(fields) not in (8, 12) or fields[0] != PROTOCOL or fields[1] not in ('INSPECT', 'APPLY'):
            raise ValueError('Invalid save bridge request')
        for field in fields:
            _line(field)
        target, panel = int(fields[2]), int(fields[7])
        if not self.hwnd or target != self.hwnd or pid(target) != os.getpid() or not user.IsWindowVisible(target) or not user.IsWindowEnabled(target):
            raise ValueError('Save window is no longer available')
        before, revision = self._snapshot()
        changed = False
        submitted = False
        if fields[1] == 'APPLY':
            if len(fields) != 12 or fields[6] not in ('0', '1') or fields[5] != '':
                raise ValueError('Invalid APPLY request')
            if fields[6] == '1' and self.submit is None:
                raise ValueError('This application does not expose an original Save action')
            if fields[8:] != [before.name, before.directory, before.extension, revision]:
                raise ValueError('保存目标已改变，请检查新的预览')
            foreground = user.GetForegroundWindow()
            if foreground != target and not (foreground == panel and pid(panel) == client and window_class(panel) == 'Jump.Panel'):
                raise ValueError('Foreground window changed')
            directory, name = fields[3:5]
            if not Path(directory).is_absolute() or not Path(directory).is_dir() or not _filename(name):
                raise ValueError('Invalid output directory or filename')
            changed = os.path.normcase(os.path.normpath(directory)) != os.path.normcase(os.path.normpath(before.directory))
            self.apply(directory, name)
            after, revision = self._snapshot()
            if (os.path.normcase(os.path.normpath(after.directory)) != os.path.normcase(os.path.normpath(directory)) or
                    after.name != name or after.extension != before.extension or after.format != before.format):
                raise ValueError('程序回读的输出位置或名称不一致；未提交保存')
            if fields[6] == '1':
                self.submit()
                submitted = True
        else:
            if len(fields) != 8 or any(fields[i] for i in (3, 4, 5)) or fields[6] != '0':
                raise ValueError('Invalid inspection request')
            after = before
        reply = '\n'.join([PROTOCOL, 'OK', '1', after.name, after.directory, after.extension,
                            after.format, after.source, '1' if submitted else '0',
                            '1' if changed else '0', revision, '1' if self.submit is not None else '0'])
        if len(reply.encode('utf-16-le')) > 32000:
            raise ValueError('Save snapshot too large')
        return reply
