"""Exercise the actual per-user startup shortcut; always restore its original bytes."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid
import native_integration as n


def run(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, default=n.ROOT / 'build/Jump.exe')
    args = parser.parse_args(argv)
    executable = args.app.resolve()
    out = n.ROOT / 'tests/output/startup' / uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    shell = c.OleDLL('shell32')
    shell.SHGetFolderPathW.argtypes = [w.HWND, c.c_int, w.HANDLE, w.DWORD, w.LPWSTR]
    folder = c.create_unicode_buffer(32768)
    assert shell.SHGetFolderPathW(None, 7, None, 0, folder) == 0
    link = Path(folder.value) / 'Jump.lnk'
    original = link.read_bytes() if link.exists() else None
    if original is not None:
        (out / 'original-startup.lnk').write_bytes(original)
    cfg = out / 'settings.ini'
    app = None
    passed = []

    def shortcut(target=None):
        script = """
$ErrorActionPreference='Stop'
[Console]::OutputEncoding=[System.Text.UTF8Encoding]::new($false)
$shell=New-Object -ComObject WScript.Shell
$shortcut=$shell.CreateShortcut($env:JUMP_CHECK_LINK)
if ($env:JUMP_CHECK_TARGET) { $shortcut.TargetPath=$env:JUMP_CHECK_TARGET; $shortcut.Save() }
@{target=$shortcut.TargetPath; directory=$shortcut.WorkingDirectory; arguments=$shortcut.Arguments; show=$shortcut.WindowStyle} | ConvertTo-Json -Compress
"""
        env = dict(os.environ, JUMP_CHECK_LINK=str(link), JUMP_CHECK_TARGET=str(target or ''))
        result = subprocess.run(['powershell', '-NoProfile', '-Command', script],
                                env=env, capture_output=True, text=True, encoding='utf-8', check=True)
        return json.loads(result.stdout)

    def start():
        nonlocal app
        app = subprocess.Popen([str(executable), '--config', str(cfg), '--manage', '--test-ui', '--test-host-only'],
                               creationflags=subprocess.CREATE_NO_WINDOW)
        manager = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Manager', True)), None))
        n.manager_tab(manager, 3)
        return manager

    def stop():
        if app and app.poll() is None:
            for controller in n.windows(app.pid, 'Jump.Control'):
                n.u.PostMessageW(controller, 0x10, 0, 0)
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.terminate()
                app.wait(timeout=5)

    def save(manager, enabled):
        n.u.SendMessageW(n.u.GetDlgItem(manager, 224), 0xF1, int(enabled), 0)
        n.u.SendMessageW(manager, 0x111, 223, n.u.GetDlgItem(manager, 223))
        assert n.text(n.u.GetDlgItem(manager, 292)) == '选项已保存。'

    try:
        if link.exists():
            link.unlink()
        manager = start()
        assert n.u.SendMessageW(n.u.GetDlgItem(manager, 224), 0xF0, 0, 0) == 0
        save(manager, True)
        details = shortcut()
        assert Path(details['target']) == executable, details
        assert Path(details['directory']) == executable.parent and details['arguments'] == '', details
        passed.append('enable creates a real per-user shortcut targeting the current executable and working folder')
        stop()
        manager = start()
        assert n.u.SendMessageW(n.u.GetDlgItem(manager, 224), 0xF0, 0, 0) == 1
        shortcut(out / 'old-location/Jump.exe')
        save(manager, True)
        assert Path(shortcut()['target']) == executable
        passed.append('checked state survives restart; saving again repairs a stale portable executable path')
        # A locked destination must preserve the old link and leave the setting retryable.
        n.k.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p, w.DWORD, w.DWORD, w.HANDLE]
        n.k.CreateFileW.restype = w.HANDLE
        n.k.CloseHandle.argtypes = [w.HANDLE]
        before = link.read_bytes()
        handle = n.k.CreateFileW(str(link), 0x80000000, 1, None, 3, 0, None)
        assert handle not in (None, 0, c.c_void_p(-1).value)
        try:
            n.u.SendMessageW(manager, 0x111, 223, n.u.GetDlgItem(manager, 223))
            assert '开机运行设置失败' in n.text(n.u.GetDlgItem(manager, 292))
            assert link.read_bytes() == before
        finally:
            n.k.CloseHandle(handle)
        save(manager, True)
        assert not list(link.parent.glob(f'Jump-{app.pid}.tmp.lnk'))
        passed.append('locked startup link is preserved, reports failure, removes temporary files and permits retry')
        save(manager, False)
        assert not link.exists()
        stop()
        manager = start()
        assert n.u.SendMessageW(n.u.GetDlgItem(manager, 224), 0xF0, 0, 0) == 0
        passed.append('disable removes the shortcut and stays unchecked after restart')
    finally:
        stop()
        if original is None:
            if link.exists():
                link.unlink()
        else:
            link.write_bytes(original)
        assert (link.read_bytes() if link.exists() else None) == original
    report = {'passed': passed, 'app': str(executable), 'app_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
              'original_startup_restored': True, 'output': str(out)}
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    run()
