"""Localized shell folders: actual Downloads saves and read-only Desktop/Documents checks."""
import configparser
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import subprocess
import uuid
import native_integration as n
import naming_fixture
import extended_integration as e


def known_path(ident):
    shell = c.WinDLL('shell32')
    ole = c.WinDLL('ole32')
    shell.SHGetKnownFolderPath.argtypes = [c.c_void_p, w.DWORD, w.HANDLE, c.POINTER(c.c_void_p)]
    ole.CoTaskMemFree.argtypes = [c.c_void_p]
    guid = c.create_string_buffer(uuid.UUID(ident).bytes_le)
    value = c.c_void_p()
    assert shell.SHGetKnownFolderPath(guid, 0, None, c.byref(value)) == 0
    try: return Path(c.wstring_at(value))
    finally: ole.CoTaskMemFree(value)


def run():
    out = n.ROOT/'tests/output/known-folders'
    out.mkdir(parents=True, exist_ok=True)
    cfg = out/'settings.ini'
    n.write_config(cfg)
    naming_fixture.install(cfg)
    folders = {
        'downloads': known_path('374de290-123f-4565-9164-39c4925e467b'),
        'desktop': known_path('b4bfcc3a-db2c-424c-b029-7fe99a87c641'),
        'documents': known_path('fdd39ad0-238f-46af-adb4-6c85480369c7'),
    }
    app = host = None
    created = []
    checks = []
    u = n.u
    u.SwitchToThisWindow.argtypes = [w.HWND, w.BOOL]

    def start():
        process = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda: n.windows(process.pid, 'Jump.Control'))
        return process

    def stop():
        for controller in n.windows(app.pid, 'Jump.Control'):
            u.PostMessageW(controller, 0x10, 0, 0)
        app.wait(timeout=4)

    def recent():
        parser = configparser.ConfigParser()
        parser.read(cfg, encoding='utf-16')
        if parser['Jump'].getint('RecentSave', fallback=0) == 0: return ''
        raw = parser['RecentSave0']['path']
        return ''.join(chr(int(raw[i:i+4], 16)) for i in range(0, len(raw), 4))

    def launch(kind, markdown=False):
        nonlocal host
        print('OPEN', kind, markdown, flush=True)
        log = out/kind
        log.mkdir(exist_ok=True)
        args = [str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(log), '--known-folder', kind]
        if markdown: args += ['--markdown']
        host = subprocess.Popen(args, creationflags=subprocess.CREATE_NO_WINDOW)
        dialog = n.wait(lambda: next(iter(n.windows(host.pid, '#32770', True)), None))
        u.SwitchToThisWindow(dialog, True)
        try:
            panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        except Exception:
            print('WINDOWS', [(n.text(h,n.u.GetClassNameW),n.text(h)) for h in n.windows(app.pid)],
                  'FOREGROUND', n.text(u.GetForegroundWindow(),u.GetClassNameW), flush=True)
            raise
        n.wait(lambda: bool(u.GetDlgItem(panel, 113)))
        breadcrumbs = [n.text(h) for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'ToolbarWindow32']
        actual = n.text(u.GetDlgItem(panel, 113))
        assert actual.lower() == str(folders[kind]).lower(), {'folder': kind, 'expected': str(folders[kind]), 'actual': actual, 'breadcrumbs': breadcrumbs}
        assert n.panel_ready(panel), 'known-folder saves must be enabled'
        return dialog, panel

    try:
        app = start()
        for via_jump, extension in [(False, 'txt'), (True, 'md')]:
            dialog, panel = launch('downloads', extension == 'md')
            stem = 'Jump_下载回归_' + uuid.uuid4().hex[:12]
            target = folders['downloads']/(stem+'.'+extension)
            assert not target.exists()
            created.append(target)
            # Only the test host is edited; existing files in Downloads remain untouched.
            edit = next(h for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit')
            value = c.create_unicode_buffer(stem)
            u.SendMessageW(edit, 0xC, 0, c.addressof(value))
            n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == stem+'.'+extension)
            u.SwitchToThisWindow(panel if via_jump else dialog, True)
            if via_jump: n.click(panel, ident=106)
            else: u.PostMessageW(dialog, 0x111, 1, u.GetDlgItem(dialog, 1))
            host.wait(timeout=8)
            assert target.read_text(encoding='utf-8') == 'Jump native integration test\n'
            n.wait(lambda: recent().lower() == str(folders['downloads']).lower(), label='Downloads recorded as a recent save')
            checks.append(f'localized Downloads resolves its real path; {"Jump" if via_jump else "native"} Save writes .{extension} and persists the destination')

        stop(); app = start()
        dialog, panel = launch('downloads')
        assert any(n.text(h) == folders['downloads'].name for h in n.children(panel))
        e.capture(panel, out/'downloads-after-restart.png')
        u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2)); host.wait(timeout=4)
        checks.append('Downloads remains visible in Recent after restarting Jump')

        for kind in ('desktop', 'documents'):
            dialog, panel = launch(kind)
            u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2)); host.wait(timeout=4)
        assert recent().lower() == str(folders['downloads']).lower()
        checks.append('localized Desktop and Documents resolve without saving files or replacing the last save')
        report = {'passed': checks}
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid, '#32770', True):
                u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2))
            try: host.wait(timeout=3)
            except subprocess.TimeoutExpired: host.terminate()
        if app and app.poll() is None: stop()
        for target in created:
            if target.exists():
                assert target.read_text(encoding='utf-8') == 'Jump native integration test\n'
                target.unlink()


if __name__ == '__main__': run()
