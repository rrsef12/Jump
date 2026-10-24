"""Recent successful save destinations, using only Jump's owned native fixtures."""
import configparser
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import native_integration as n
import naming_fixture
import extended_integration as e
import naming_menu as menu


def run():
    out = n.ROOT / 'tests/output/recent-saves'
    out.mkdir(parents=True, exist_ok=True)
    cfg = out / 'settings.ini'
    delivery, assets = n.write_config(cfg)
    naming_fixture.install(cfg)
    u = n.u
    u.SwitchToThisWindow.argtypes = [w.HWND, w.BOOL]
    app = host = None
    checks = []

    def read_config():
        parser = configparser.ConfigParser()
        parser.read(cfg, encoding='utf-16')
        return parser

    def locations(kind='RecentSave'):
        parser = read_config()
        return [bytes.fromhex(parser[f'{kind}{i}']['path']).decode('utf-16-be')
                for i in range(parser['Jump'].getint(kind, fallback=0))]

    def choose_context(control, caption):
        u.PostMessageW(control, 0x7B, control, -1)
        popup = n.wait(lambda: next(iter(n.windows(app.pid, '#32768', True)), None), label='recent location context menu')
        handle = u.SendMessageW(popup, 0x1E1, 0, 0)
        owner, index = menu.find(handle, caption)
        rect = w.RECT(); assert u.GetMenuItemRect(None, owner, index, c.byref(rect))
        old = w.POINT(); u.GetCursorPos(c.byref(old))
        try:
            u.SetCursorPos((rect.left+rect.right)//2, (rect.top+rect.bottom)//2)
            time.sleep(.1); u.mouse_event(2,0,0,0,0); u.mouse_event(4,0,0,0,0)
            n.wait(lambda: not n.windows(app.pid, '#32768', True))
        finally:
            u.SetCursorPos(old.x, old.y)

    def start():
        process = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda: n.windows(process.pid, 'Jump.Control'))
        return process

    def stop():
        for controller in n.windows(app.pid, 'Jump.Control'):
            u.PostMessageW(controller, 0x10, 0, 0)
        app.wait(timeout=4)

    def launch(kind='modern'):
        nonlocal host
        host, dialog, initial = n.launch_host(kind)
        def attached():
            n.activate(dialog)
            return next(iter(n.windows(app.pid, 'Jump.Panel', True)), None)
        panel = n.wait(attached, label='helper attached to owned save dialog')
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == '原文件.txt')
        return dialog, panel, initial

    def prepare(path):
        if path.exists():
            assert path.read_text(encoding='utf-8') == 'Jump native integration test\n'
            path.unlink()

    def filled(panel, value):
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == value+'.txt' and n.panel_ready(panel))
    def native_name(dialog,panel,value):
        n.activate(dialog);last_write=-1.;attempts=0;stable=None
        def ready():
            nonlocal last_write,attempts,stable
            field=next(h for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit' and u.IsWindowVisible(h))
            now=time.monotonic()
            if n.text(field)!=value:
                stable=None
                if now-last_write>.3 and attempts<3:
                    text=c.create_unicode_buffer(value);u.SendMessageW(field,0xC,0,c.addressof(text));last_write=now;attempts+=1
                return False
            if stable is None:stable=now
            return now-stable>.2
        # The shell can reset the filename after its address bar already shows
        # the new folder. Reacquire its visible editor after that navigation turn.
        n.wait(ready,label='native filename settled after navigation');filled(panel,value)

    def edit_name(panel, value):
        n.activate(panel)
        preview = u.GetDlgItem(panel, 101)
        u.SendMessageW(preview, 0x201, 1, 8 | (8 << 16))
        u.SendMessageW(preview, 0x202, 0, 8 | (8 << 16))
        e.set_field(panel, 101, value)
        def synced():
            # The shell can restore another app's foreground after host replacement.
            # Reacquire only our fixture while its deferred native fill completes.
            n.activate(panel)
            return n.panel_ready(panel) and any(n.text(h) == value for h in n.children(dialog)
                                               if n.text(h, u.GetClassNameW) == 'Edit')
        n.wait(synced, label='owned save preview synchronized')

    def location_labels(panel):
        return [n.text(u.GetDlgItem(panel, ident)) for ident in range(160,169)
                if u.GetDlgItem(panel, ident) and u.IsWindowVisible(u.GetDlgItem(panel, ident))]

    try:
        app = start()
        dialog, panel, _ = launch()
        target = delivery/'最近_助手.txt'
        prepare(target)
        n.click(panel, caption='测试成品')
        n.wait(lambda: n.folder_is(dialog, delivery) and n.panel_ready(panel))
        edit_name(panel, '最近_助手')
        n.wait(lambda: any(n.text(h) == '最近_助手' for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit'))
        filled(panel, '最近_助手')
        u.SwitchToThisWindow(panel, True)
        n.click(panel, ident=106)
        host.wait(timeout=8)
        n.wait(lambda: locations() == [str(delivery)], label='fast Jump save is persisted after native close')
        assert target.read_text(encoding='utf-8') == 'Jump native integration test\n'
        checks.append('fast Jump save records the destination after the native dialog closes')

        stop(); app = start()
        dialog, panel, _ = launch()
        n.click(panel, ident=127)
        n.wait(lambda: location_labels(panel) == [delivery.name], label='verified recent destination remains reachable')
        assert locations() == [str(delivery)], 'opening a new save dialog must not replace the last save'
        checks.append('restart opens the Recent tab with the saved destination; initial directory does not overwrite it')

        # Navigate and type in the original native dialog, then use its Save button.
        target = assets/'最近_原生.txt'
        prepare(target)
        n.activate(dialog)
        edit = next(h for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit')
        buffer = c.create_unicode_buffer(str(assets) + '\\')
        u.SendMessageW(edit, 0xC, 0, c.addressof(buffer))
        u.PostMessageW(dialog, 0x111, 1, u.GetDlgItem(dialog, 1))
        n.wait(lambda: n.folder_is(dialog, assets))
        native_name(dialog,panel,'最近_原生')
        u.SwitchToThisWindow(dialog, True)
        u.PostMessageW(dialog, 0x111, 1, u.GetDlgItem(dialog, 1))
        host.wait(timeout=8)
        n.wait(lambda: locations() == [str(assets), str(delivery)], label='native Save is persisted')
        assert target.is_file()
        checks.append('navigation, typing and Save in the original native dialog also record the actual destination')

        dialog, panel, initial = launch('classic')
        target = initial/'最近_经典.txt'
        prepare(target)
        n.activate(dialog)
        edit = next(h for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit')
        buffer = c.create_unicode_buffer(str(initial) + '\\')
        u.SendMessageW(edit, 0xC, 0, c.addressof(buffer))
        u.PostMessageW(dialog, 0x111, 1, u.GetDlgItem(dialog, 1))
        n.wait(lambda: n.folder_is(dialog, initial))
        native_name(dialog,panel,'最近_经典')
        u.SwitchToThisWindow(dialog, True)
        u.PostMessageW(dialog, 0x111, 1, u.GetDlgItem(dialog, 1))
        host.wait(timeout=8)
        expected = [str(initial), str(assets), str(delivery)]
        n.wait(lambda: locations() == expected, label='classic native Save is persisted')
        assert target.is_file()
        checks.append('classic native Save records the destination as well')

        dialog, panel, _ = launch()
        n.click(panel, ident=121)
        n.click(panel, caption='测试成品')
        n.wait(lambda: n.folder_is(dialog, delivery) and n.panel_ready(panel))
        edit_name(panel, '最近_助手')  # Existing, unchanged file; Cancel must not count as saving.
        n.wait(lambda: any(n.text(h) == '最近_助手' for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit'))
        filled(panel, '最近_助手')
        u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2))
        host.wait(timeout=5)
        time.sleep(1.1)
        assert locations() == expected
        checks.append('visiting or cancelling an existing filename does not add or reorder recent saves')

        stop(); app = start()
        dialog, panel, _ = launch()
        assert locations() == expected
        n.click(panel, ident=127)
        labels = location_labels(panel)
        assert labels == [initial.name, assets.name, delivery.name], labels
        n.click(panel, ident=122)
        n.click(panel, ident=127)
        assert location_labels(panel) == labels
        e.capture(panel, out/'recent-after-restart.png')
        checks.append('save ordering survives another restart and is separate from browsing history')

        assert u.IsWindowVisible(u.GetDlgItem(panel,128)) and u.IsWindowEnabled(u.GetDlgItem(panel,128))
        name = n.text(u.GetDlgItem(panel,101)); directory = n.text(u.GetDlgItem(panel,113))
        # Attachment records history through the coalescing background writer.
        # Take the unchanged-history baseline only after this navigation is persisted.
        n.wait(lambda: locations('History') and locations('History')[0] == directory,
               label='current directory history persisted before recent-removal baseline')
        favorites, history = locations('Favorite'), locations('History')
        saved_records = read_config()['Jump'].getint('savedFiles')
        n.activate(panel); choose_context(u.GetDlgItem(panel,162), '从最近移除')
        n.wait(lambda: locations() == expected[:2])
        assert location_labels(panel) == [initial.name, assets.name]
        assert str(delivery) in locations('Favorite') and (delivery/'最近_助手.txt').is_file()
        assert locations('Favorite') == favorites and locations('History') == history
        checks.append('individual recent removal updates immediately while keeping the same folder in Favorites and retaining actual files and browsing history')

        n.click(panel, ident=121)
        assert not u.IsWindowVisible(u.GetDlgItem(panel,128))
        n.click(panel, ident=127)
        e.capture(panel, out/'recent-clear-action.png')
        # Simulate a real write failure: the list must stay visible and retryable.
        kernel = c.WinDLL('kernel32', use_last_error=True)
        kernel.GetFileAttributesW.argtypes = [w.LPCWSTR]; kernel.GetFileAttributesW.restype = w.DWORD
        kernel.SetFileAttributesW.argtypes = [w.LPCWSTR,w.DWORD]; kernel.SetFileAttributesW.restype = w.BOOL
        attributes = kernel.GetFileAttributesW(str(cfg)); assert attributes != 0xFFFFFFFF
        try:
            assert kernel.SetFileAttributesW(str(cfg), attributes | 1)
            n.click(panel, ident=128)
            assert locations() == expected[:2] and location_labels(panel) == [initial.name, assets.name]
            assert '未保存' in n.text(u.GetDlgItem(panel,114))
        finally:
            assert kernel.SetFileAttributesW(str(cfg), attributes)
        n.click(panel, ident=128)
        n.wait(lambda: locations() == [] and location_labels(panel) == [], label='recent list cleared and persisted')
        assert u.IsWindowVisible(u.GetDlgItem(panel,154)) and '暂无最近' in n.text(u.GetDlgItem(panel,154))
        assert not u.IsWindowEnabled(u.GetDlgItem(panel,128))
        assert locations('Favorite') == favorites and locations('History') == history
        assert read_config()['Jump'].getint('savedFiles') == saved_records
        assert n.text(u.GetDlgItem(panel,101)) == name and n.text(u.GetDlgItem(panel,113)) == directory
        assert n.folder_is(dialog, directory) and host.poll() is None
        assert all(path.is_file() for path in [delivery/'最近_助手.txt', assets/'最近_原生.txt', initial/'最近_经典.txt'])
        e.capture(panel, out/'recent-cleared.png')
        checks.append('Clear Recent rolls back on write failure and can be retried; success shows an empty list, disables clear and preserves current filename/directory, favorites, history and saved-file records')

        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2)); host.wait(timeout=5)
        stop(); app = start(); dialog,panel,new_initial = launch()
        n.click(panel,ident=127)
        assert locations() == [] and location_labels(panel) == []
        assert not u.IsWindowEnabled(u.GetDlgItem(panel,128))
        target = new_initial/'清理后保存.txt'; prepare(target)
        edit_name(panel, '清理后保存'); filled(panel,'清理后保存')
        n.activate(dialog); u.PostMessageW(dialog,0x111,1,u.GetDlgItem(dialog,1)); host.wait(timeout=8)
        n.wait(lambda: locations() == [str(target.parent)], label='next verified save starts a new recent list')
        assert target.is_file()
        checks.append('cleared recent records stay empty after restart and a subsequent verified save records its destination normally')
        report = {'passed': checks}
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    except Exception:
        print('RECENT', locations(), flush=True)
        print('FOREGROUND', n.text(u.GetForegroundWindow(), u.GetClassNameW), flush=True)
        if host:
            print('NATIVE',[(n.text(h),u.GetDlgCtrlID(h),bool(u.IsWindowVisible(h))) for dialog in n.windows(host.pid,'#32770') for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit'],flush=True)
        if app:
            for panel in n.windows(app.pid, 'Jump.Panel'):
                print('PANEL', n.text(panel), [(u.GetDlgCtrlID(h), n.text(h)) for h in n.children(panel)], flush=True)
        raise
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid, '#32770', True):
                u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2))
            try: host.wait(timeout=3)
            except subprocess.TimeoutExpired: host.terminate()
        if app and app.poll() is None:
            stop()


if __name__ == '__main__':
    run()
