"""History actions, path paste and verified-save notice on owned native hosts."""
import configparser
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import uuid
import sys
import win32clipboard as clipboard
import native_integration as n
import naming_fixture
import extended_integration as e

u = n.u

def run():
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
    out = n.ROOT / 'tests/output/convenience' / uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg = out / 'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nmodifiers=-1\nrecipes=0\n', encoding='utf-16')
    naming_fixture.install(cfg)
    delivery = out / '海港 交付'; delivery.mkdir()
    dotted = out / 'folder.with.dots'; dotted.mkdir()
    source = out / 'source.txt'; source.write_text('owned fixture', encoding='utf-8')
    app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
    host = None
    checks = []
    def open_clipboard():
        def available():
            try:
                clipboard.OpenClipboard(); return True
            except Exception:
                return False
        n.wait(available, timeout=3, label='clipboard available for owned fixture')
    open_clipboard()
    try:
        original_clipboard = []
        kind = clipboard.EnumClipboardFormats(0)
        while kind:
            try:
                value = clipboard.GetClipboardData(kind)
                if isinstance(value, (str,bytes)): original_clipboard.append((kind,value))
            except Exception: pass
            kind = clipboard.EnumClipboardFormats(kind)
    finally: clipboard.CloseClipboard()
    clipboard_sequence = None
    def command(window, ident):
        u.SendMessageW(window, 0x111, ident, u.GetDlgItem(window, ident) or 0)
    def read_config():
        p = configparser.ConfigParser(); p.read(cfg, encoding='utf-16'); return p
    def entries(kind):
        p = read_config()
        return [bytes.fromhex(p[f'{kind}{i}']['path']).decode('utf-16-be') for i in range(p['Jump'].getint(kind, fallback=0))]
    def launch():
        nonlocal host
        host = subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(out), '--source', str(source)], creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog = n.wait(lambda: next(iter(n.windows(host.pid, '#32770', True)), None))
        n.activate(dialog)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == '原文件.txt' and n.panel_ready(panel))
        return dialog, panel
    def paste(panel, value, ident=101):
        nonlocal clipboard_sequence
        n.activate(panel)
        if ident == 156: command(panel, 155)
        if ident == 101:
            if not u.GetDlgItem(panel,896): command(panel,755)
            n.wait(lambda:u.GetDlgItem(panel,896),label='explicit full-name paste editor')
            ident=896
        editor = u.GetDlgItem(panel, ident)
        u.SendMessageW(editor,0x201,1,8|(8<<16)); u.SendMessageW(editor,0x202,0,8|(8<<16))
        open_clipboard()
        try:
            clipboard.EmptyClipboard(); clipboard.SetClipboardText(value,13)
            assert clipboard.GetClipboardData(13) == value
        finally: clipboard.CloseClipboard()
        clipboard_sequence = clipboard.GetClipboardSequenceNumber()
        time.sleep(.1)
        u.SendMessageW(editor, 0x302, 0, 0)
    def filled(dialog, panel, directory, stem):
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == stem+'.txt' and n.panel_ready(panel) and
               n.text(u.GetDlgItem(panel,113)).casefold() == str(directory).casefold() and n.folder_is(dialog, directory) and
               '正在检查' not in n.text(u.GetDlgItem(panel,114)),
               label=f'filled {directory} / {stem}; status: '+n.text(u.GetDlgItem(panel,114)))
        assert host.poll() is None
    def choose_context(control, caption):
        u.PostMessageW(control, 0x7B, control, -1)
        menu = n.wait(lambda: next(iter(n.windows(app.pid, '#32768', True)), None), label='owned context menu')
        u.GetMenuItemCount.argtypes = [c.c_void_p]
        u.GetMenuStringW.argtypes = [c.c_void_p, c.c_uint, c.c_wchar_p, c.c_int, c.c_uint]
        handle = u.SendMessageW(menu, 0x1E1, 0, 0)
        labels = []
        for index in range(u.GetMenuItemCount(handle)):
            label = c.create_unicode_buffer(256)
            u.GetMenuStringW(handle, index, label, len(label), 0x400); labels.append(label.value)
        index = labels.index(caption)
        from ctypes import wintypes as w
        rect = w.RECT()
        u.GetMenuItemRect.argtypes = [w.HWND, c.c_void_p, c.c_uint, c.POINTER(w.RECT)]
        assert u.GetMenuItemRect(None, handle, index, c.byref(rect))
        old = w.POINT(); u.GetCursorPos(c.byref(old))
        u.SetCursorPos((rect.left+rect.right)//2,(rect.top+rect.bottom)//2)
        time.sleep(.12)
        u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0)
        time.sleep(.1); u.SetCursorPos(old.x,old.y)
        n.wait(lambda: not n.windows(app.pid, '#32768', True))
    try:
        n.wait(lambda: n.windows(app.pid, 'Jump.Control'))
        dialog, panel = launch()
        assert not u.GetDlgItem(panel, 123), 'Explorer locations now share Recommended'
        assert [n.text(u.GetDlgItem(panel, ident)) for ident in [120,121,127,122]] == ['推荐','收藏','最近','历史']
        for ident in [120,121,127,122]:
            command(panel,ident)
            assert n.text(u.GetDlgItem(panel,101)) == '原文件.txt' and n.folder_is(dialog,out)
        assert not any(n.text(h) == '浏览中' for h in n.children(panel))
        checks.append('four location tabs preserve the native name/directory; Explorer has no separate tab')
        paste(panel, '"'+str(delivery/'守卫_v012.txt')+'"')
        filled(dialog, panel, delivery, '守卫_v012')
        assert not (delivery/'守卫_v012.txt').exists() and not n.windows(app.pid, 'Jump.SavedNotice', True)
        checks.append('quoted Unicode full path splits destination/name; fill never saves or shows completion')

        paste(panel, str(delivery/'wrong.png'))
        n.wait(lambda: '当前格式' in n.text(u.GetDlgItem(panel,114)))
        filled(dialog, panel, delivery, '守卫_v012')
        assert n.text(u.GetDlgItem(panel,101)) == '守卫_v012.txt' and not u.GetDlgItem(panel,115)
        paste(panel, str(out/'missing'/'file.txt'))
        n.wait(lambda: '上级文件夹' in n.text(u.GetDlgItem(panel,114)))
        filled(dialog, panel, delivery, '守卫_v012')
        paste(panel, str(delivery/'CON.txt'))
        n.wait(lambda: any(text in n.text(u.GetDlgItem(panel,114)) for text in ['文件名无效','无法访问']))
        checks.append('format mismatch, missing parent and reserved filename preserve the previous destination and name')

        paste(panel, str(dotted), 156)
        filled(dialog, panel, dotted, '守卫_v012')
        paste(panel, str(delivery/'最终 名称.txt'), 156)
        filled(dialog, panel, delivery, '最终 名称')
        checks.append('path editor also accepts full filenames; existing dotted folders remain folders')

        command(panel, 122)
        n.wait(lambda: n.text(u.GetDlgItem(panel,160)) == delivery.name)
        n.activate(panel); choose_context(u.GetDlgItem(panel,160), '收藏文件夹')
        n.wait(lambda: str(delivery) in entries('Favorite'))
        choose_context(u.GetDlgItem(panel,160), '从历史移除')
        n.wait(lambda: str(delivery) not in entries('History'))
        # A native refresh of the current folder must not silently reinsert it.
        controller = n.windows(app.pid,'Jump.Control')[0]
        for _ in range(3):
            u.SendMessageW(controller, 0x113, 2, 0); time.sleep(.3)
        assert str(delivery) not in entries('History') and str(delivery) in entries('Favorite')
        filled(dialog, panel, delivery, '最终 名称')
        checks.append('right-click removal survives refresh and preserves both favorite and current target')

        for folder in [out, dotted, delivery]:
            paste(panel, str(folder), 156); filled(dialog,panel,folder,'最终 名称')
        command(panel, 122); command(panel, 153)
        popup = n.wait(lambda: next(iter(n.windows(app.pid,'Jump.Flyout',True)), None))
        choose_context(u.GetDlgItem(popup,146), '从历史移除')
        n.wait(lambda: str(delivery) not in entries('History'))
        command(popup, 2)
        checks.append('full-history search supports the same individual removal action')

        n.activate(panel); command(panel, 106); host.wait(timeout=10)
        saved = delivery/'最终 名称.txt'
        assert saved.read_text(encoding='utf-8') == 'Jump native integration test\n'
        notice = n.wait(lambda: next(iter(n.windows(app.pid,'Jump.SavedNotice',True)),None))
        assert n.text(u.GetDlgItem(notice,1)) == '已保存：最终 名称.txt'
        assert n.text(u.GetDlgItem(notice,2)) == str(saved)
        assert n.text(u.GetDlgItem(notice,3)) == '打开所在目录'
        assert u.GetForegroundWindow() != notice
        e.capture(notice,out/'verified-save.png')
        n.wait(lambda: not n.windows(app.pid,'Jump.SavedNotice',True), timeout=11, label='notice automatically dismissed')
        checks.append('actual write shows exact verified filename/path and open-folder action, without focus theft; notice expires')

        dialog,panel=launch()
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        time.sleep(.7)
        assert not n.windows(app.pid,'Jump.SavedNotice',True)
        assert read_config()['Jump'].getint('savedFiles') == 1
        checks.append('cancelled save produces neither completion notice nor an extra file record')

        dialog,panel=launch()
        for folder in [dotted,out,delivery]:
            paste(panel,str(folder),156); filled(dialog,panel,folder,'原文件')
        favorites,recent=entries('Favorite'),entries('RecentSave')
        assert recent==[str(delivery)]
        command(panel,122)
        assert u.IsWindowVisible(u.GetDlgItem(panel,129)) and u.IsWindowEnabled(u.GetDlgItem(panel,129))
        assert not u.IsWindowVisible(u.GetDlgItem(panel,128))
        assert u.IsWindowVisible(u.GetDlgItem(panel,153))
        e.capture(panel,out/'history-clear-action.png')
        before_history=entries('History')
        kernel=c.WinDLL('kernel32',use_last_error=True)
        kernel.GetFileAttributesW.argtypes=[w.LPCWSTR];kernel.GetFileAttributesW.restype=w.DWORD
        kernel.SetFileAttributesW.argtypes=[w.LPCWSTR,w.DWORD];kernel.SetFileAttributesW.restype=w.BOOL
        attributes=kernel.GetFileAttributesW(str(cfg));assert attributes!=0xFFFFFFFF
        try:
            assert kernel.SetFileAttributesW(str(cfg),attributes|1)
            command(panel,129)
            assert entries('History')==before_history and u.IsWindowVisible(u.GetDlgItem(panel,160))
            assert '未保存' in n.text(u.GetDlgItem(panel,114))
        finally:
            assert kernel.SetFileAttributesW(str(cfg),attributes)
        command(panel,129)
        assert entries('History')==[] and entries('Favorite')==favorites and entries('RecentSave')==recent
        assert u.IsWindowVisible(u.GetDlgItem(panel,154)) and '暂无浏览历史' in n.text(u.GetDlgItem(panel,154))
        assert not u.IsWindowEnabled(u.GetDlgItem(panel,129)) and not u.IsWindowVisible(u.GetDlgItem(panel,153))
        assert not any(u.IsWindowVisible(u.GetDlgItem(panel,i)) for i in range(160,169))
        filled(dialog,panel,delivery,'原文件')
        controller=n.windows(app.pid,'Jump.Control')[0]
        for _ in range(3):
            u.SendMessageW(controller,0x113,2,0);time.sleep(.3)
        assert entries('History')==[] and saved.read_text(encoding='utf-8')=='Jump native integration test\n'
        e.capture(panel,out/'history-cleared.png')
        command(panel,127)
        assert u.IsWindowVisible(u.GetDlgItem(panel,128)) and not u.IsWindowVisible(u.GetDlgItem(panel,129))
        command(panel,122)
        assert not u.IsWindowEnabled(u.GetDlgItem(panel,129))
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        u.PostMessageW(controller,0x10,0,0);app.wait(timeout=4)
        app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        assert entries('History')==[], 'reloading must preserve an empty history before any new visit'
        dialog,panel=launch();command(panel,122)
        assert entries('History') and u.IsWindowEnabled(u.GetDlgItem(panel,129))
        assert entries('Favorite')==favorites and entries('RecentSave')==recent
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        checks.append('one-click Clear History rolls back on write failure, clears both compact/full lists, preserves favorites/recent saves/current target/files, survives refresh and restart, and allows new visits to be recorded')
        report = {'passed':checks,'output':str(out)}
        (out.parent/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        for panel in n.windows(app.pid,'Jump.Panel'):
            print('DIAGNOSTICS',[(u.GetDlgCtrlID(h),n.text(h)) for h in n.children(panel)],flush=True)
        raise
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid,'#32770',True):u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2))
            try:host.wait(timeout=3)
            except subprocess.TimeoutExpired:host.terminate()
        for control in n.windows(app.pid,'Jump.Control'):u.PostMessageW(control,0x10,0,0)
        try:app.wait(timeout=4)
        except subprocess.TimeoutExpired:app.terminate()
        if clipboard_sequence is not None and clipboard.GetClipboardSequenceNumber() == clipboard_sequence:
            open_clipboard()
            try:
                clipboard.EmptyClipboard()
                for kind,value in original_clipboard: clipboard.SetClipboardData(kind,value)
            finally: clipboard.CloseClipboard()

if __name__ == '__main__': run()
