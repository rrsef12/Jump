"""Affix preset CRUD, order, drafts and direct entry points on owned fixtures."""
import configparser
from ctypes import wintypes as w
import json
import subprocess
import native_integration as n
import extended_integration as e


def run():
    out = n.ROOT/'tests/output/affix-manager'
    out.mkdir(parents=True, exist_ok=True)
    cfg = out/'settings.ini'
    n.write_config(cfg)
    u = n.u
    u.SwitchToThisWindow.argtypes = [w.HWND, w.BOOL]
    app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--manage', '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
    host = None
    checks = []

    def tab(manager, index):
        n.manager_tab(manager,index)

    def select(manager, index):
        control = u.GetDlgItem(manager, 202)
        u.SendMessageW(control, 0x186, index, 0)
        u.SendMessageW(manager, 0x111, 202 | (1 << 16), control)

    def values(kind):
        parser = configparser.ConfigParser()
        parser.read(cfg, encoding='utf-16')
        total = parser['Jump'].getint('prefixes' if kind == 'Prefix' else 'suffixes')
        result = []
        for i in range(total):
            encoded = parser[kind][str(i)]
            result.append(''.join(chr(int(encoded[j:j+4], 16)) for j in range(0, len(encoded), 4)))
        return result

    def add(manager, value):
        n.click(manager, ident=210)
        assert n.text(u.GetDlgItem(manager, 204)) == ''
        assert not u.IsWindowEnabled(u.GetDlgItem(manager, 207))
        e.set_field(manager, 204, value)
        assert u.IsWindowEnabled(u.GetDlgItem(manager, 207))
        n.click(manager, ident=207)
        assert u.IsWindowEnabled(u.GetDlgItem(manager, 207)), 'saved preset remains editable'
        assert u.IsWindowEnabled(u.GetDlgItem(manager, 208)), 'saved preset can be removed'

    def close_manager(manager, dialog=None, discard=False):
        u.PostMessageW(manager, 0x10, 0, 0)
        if discard:
            prompt = n.wait(lambda: next(iter(n.dialogs(app.pid, '未保存的编辑')), None))
            u.PostMessageW(prompt, 0x111, 6, u.GetDlgItem(prompt, 6) or 0)
        n.wait(lambda: not n.windows(app.pid, 'Jump.Manager'))
        if dialog: u.SwitchToThisWindow(dialog, True)

    try:
        control = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
        manager = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Manager', True)), None))
        tab(manager, 4)
        assert not u.GetDlgItem(manager, 203), 'affix form should only require the actual content'
        add(manager, '客户_')
        assert values('Prefix')[0] == '客户_' and len(values('Prefix')) == 7
        e.set_field(manager, 204, '客户B_'); n.click(manager, ident=207)
        assert values('Prefix')[0] == '客户B_' and len(values('Prefix')) == 7
        add(manager, '团队_')
        select(manager, 1)
        assert u.IsWindowEnabled(u.GetDlgItem(manager, 209)), 'selected preset can move up'
        n.click(manager, ident=209)
        assert values('Prefix')[:2] == ['客户B_', '团队_']
        n.click(manager, ident=208)
        assert values('Prefix')[0] == '团队_' and '客户B_' not in values('Prefix')
        add(manager, '项目 ')
        assert values('Prefix')[0] == '项目 ', 'prefix spaces must be preserved'
        checks.append('prefix presets support add, edit, move, delete and literal spaces')

        category=u.GetDlgItem(manager,252);u.SendMessageW(category,0x14E,1,0)
        u.SendMessageW(manager,0x111,252|(1<<16),category)
        add(manager, '_审核')
        e.set_field(manager, 204, '_复核'); n.click(manager, ident=207)
        assert values('Suffix')[0] == '_复核'
        add(manager, '_验收')
        select(manager, 1); n.click(manager, ident=209)
        assert values('Suffix')[:2] == ['_复核', '_验收']
        n.click(manager, ident=208)
        assert values('Suffix')[0] == '_验收' and '_复核' not in values('Suffix')
        add(manager, '_归档_{date}')
        assert '文件名_' in n.text(u.GetDlgItem(manager, 229)) and '{date}' not in n.text(u.GetDlgItem(manager, 229))
        checks.append('suffix CRUD and date-variable preview work independently of full names and recipes')

        before = cfg.read_bytes()
        n.click(manager, ident=210); e.set_field(manager, 204, '_未保存')
        close_manager(manager, discard=True)
        assert cfg.read_bytes() == before and '_未保存' not in values('Suffix')
        checks.append('closing an unsaved new-preset form leaves the stored configuration unchanged')
        u.PostMessageW(control, 0x10, 0, 0); app.wait(timeout=5)

        app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
        control = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
        host, dialog, _ = n.launch_host()
        u.SwitchToThisWindow(dialog, True)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        if not u.GetDlgItem(panel,179): n.click(panel, ident=142)
        assert u.GetDlgItem(panel,179)
        n.click(panel, ident=179)
        popup = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Flyout')), None))
        assert u.GetDlgItem(popup, 137) and not n.windows(app.pid, 'Jump.Manager')
        assert n.text(u.GetDlgItem(popup, 137)) == ''
        e.set_field(popup, 137, '直接_')
        e.capture(popup, out/'prefix-popup.png')
        n.click(popup, ident=138)
        assert values('Prefix')[0] == '直接_'
        n.click(panel, caption='直接_')
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == '直接_原文件_v001.txt' and n.panel_ready(panel))
        if not u.GetDlgItem(panel,180):n.click(panel,ident=142)
        n.click(panel, ident=180)
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Flyout',True)),None))
        u.SendMessageW(popup,0x111,149,u.GetDlgItem(popup,149) or 0)
        manager = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Manager', True)), None))
        assert u.SendMessageW(u.GetDlgItem(manager, 200), 0x130B, 0, 0) == 4
        assert u.SendMessageW(u.GetDlgItem(manager,252),0x147,0,0)==1
        assert not n.windows(app.pid, 'Jump.Rename')
        add(manager, '_通过')
        close_manager(manager, dialog)
        n.wait(lambda: u.IsWindowVisible(panel))
        if not u.GetDlgItem(panel,180):n.click(panel,ident=142)
        n.click(panel, caption='_通过')
        if not u.GetDlgItem(panel,180):n.click(panel,ident=142)
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == '直接_原文件_通过_v001.txt' and
               n.panel_ready(panel))
        assert any(n.text(h) == '直接_原文件_通过_v001' for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit')
        e.capture(panel, out/'direct-add.png')
        checks.append('presets survive restart; main-panel Add and Manage open the correct category and saved bubbles fill the native dialog')
        report = {'passed': checks}
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    except Exception:
        for window in n.windows(app.pid, 'Jump.Manager', True):
            print('MANAGER', [(u.GetDlgCtrlID(h), n.text(h)) for h in n.children(window)], flush=True)
            e.capture(window, out/'failure-manager.png')
        raise
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid, '#32770', True): u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2))
            try: host.wait(timeout=3)
            except subprocess.TimeoutExpired: host.terminate()
        if app.poll() is None:
            for control in n.windows(app.pid, 'Jump.Control'): u.PostMessageW(control, 0x10, 0, 0)
            try: app.wait(timeout=3)
            except subprocess.TimeoutExpired: app.terminate()


if __name__ == '__main__':
    run()
