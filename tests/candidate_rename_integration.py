"""Right-click candidate renaming on owned Save dialogs, including persistence."""
import configparser
import ctypes as c
import json
import subprocess
import time
import uuid
import native_integration as n
import extended_integration as e

u = n.u


def run():
    out = n.ROOT/'tests/output/candidate-rename'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg = out/'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nmodifiers=-1\nrecipes=0\n', encoding='utf-16')
    app = host = panel = dialog = None
    checks = []

    def start():
        nonlocal app, host, panel, dialog
        app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-ui', '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda: n.windows(app.pid, 'Jump.Control'))
        host = subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(out)], creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog = n.wait(lambda: next(iter(n.windows(host.pid, '#32770', True)), None))
        n.activate(dialog)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        filled('原文件_v001')

    def stop():
        nonlocal app, host
        if host and host.poll() is None:
            u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2))
            try: host.wait(timeout=5)
            except subprocess.TimeoutExpired: host.terminate(); host.wait(timeout=5)
        if app and app.poll() is None:
            for window in n.windows(app.pid, 'Jump.Control'): u.PostMessageW(window, 0x10, 0, 0)
            try: app.wait(timeout=5)
            except subprocess.TimeoutExpired: app.terminate(); app.wait(timeout=5)

    def filled(value):
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == value+'.txt' and n.panel_ready(panel) and
               any(n.text(h) == value for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit'), label=value)

    def click(ident):
        n.wait(lambda: n.panel_ready(panel))
        if ident in [788,789]:u.SendMessageW(panel,0x111,ident,0);return
        n.click(panel, ident=ident)

    def modal(title):
        return n.wait(lambda: next((h for h in n.windows(app.pid, '#32770', True) if n.text(h) == title), None), label=title)

    def open_rename(ident):
        n.wait(lambda: n.panel_ready(panel))
        button = u.GetDlgItem(panel, ident)
        assert button and u.IsWindowVisible(button)
        u.PostMessageW(button, 0x7B, button, -1)
        return modal('重命名常用选项')

    def rename(ident, value):
        form = open_rename(ident)
        e.set_field(form, 100, value)
        u.PostMessageW(u.GetDlgItem(form, 100), 0x100, 13, 0)  # Native Enter/default button.
        n.wait(lambda: not u.IsWindow(form), label='rename completed')
        n.wait(lambda: n.panel_ready(panel))

    def rejected(ident, value):
        form = open_rename(ident)
        e.set_field(form, 100, value)
        u.PostMessageW(form, 0x111, 1, 0)
        notice = modal('Jump')
        button = next(h for h in n.children(notice) if n.text(h) == '确定')
        u.PostMessageW(notice, 0x111, u.GetDlgCtrlID(button), button)
        form = modal('重命名常用选项')
        u.PostMessageW(form, 0x111, 2, 0)
        n.wait(lambda: not u.IsWindow(form))

    try:
        start()
        click(852); filled('原文件_模型_v001')
        form = open_rename(853)
        assert n.text(u.GetDlgItem(form, 100)) == '材质'
        e.capture(form, out/'rename-dialog.png')
        u.PostMessageW(u.GetDlgItem(form, 100), 0x100, 27, 0)  # Escape cancels without applying.
        n.wait(lambda: not u.IsWindow(form))
        filled('原文件_模型_v001')
        assert n.text(u.GetDlgItem(panel, 853)) == '材质'
        handles = {i: u.GetDlgItem(panel, i) for i in [852, 853, 854]}
        rename(853, '材质稿'); filled('原文件_模型_v001')
        assert n.text(u.GetDlgItem(panel, 853)) == '材质稿'
        assert all(u.GetDlgItem(panel, i) == h for i, h in handles.items())
        checks.append('right-click opens one rename form without selecting the candidate; Cancel changes nothing; unselected renames keep the current filename and controls')
        rename(852, '工作'); filled('原文件_工作_v001')
        rename(852, '模型定版'); filled('原文件_模型定版_v001')
        checks.append('renaming a selected quick candidate updates its label and the native filename; repeated renames replace the previous option')
        click(789); click(784)
        assert n.text(u.GetDlgItem(panel, 800)) == '工作', 'type renames must not rename the stage field'
        click(800); filled('原文件_模型定版_工作_v001')
        rename(800, '制作中'); filled('原文件_模型定版_制作中_v001')
        assert n.text(u.GetDlgItem(panel, 760)) == '制作中'
        assert n.text(u.GetDlgItem(panel, 800)) == '制作中'
        for value in ['bad/name', '', '制作中']:
            rejected(801, value)
            filled('原文件_模型定版_制作中_v001')
            assert n.text(u.GetDlgItem(panel, 801)) == '审核'
        checks.append('All fields candidates support right-click rename; the selected editor stays synchronized; invalid, blank and duplicate names are rejected')
        click(782); e.set_field(panel, 760, '材质')
        filled('原文件_材质_制作中_v001')
        assert n.text(u.GetDlgItem(panel, 800)) == '材质稿'
        rename(800, '材质成品'); filled('原文件_材质_制作中_v001')
        assert n.text(u.GetDlgItem(panel, 800)) == '材质成品'
        click(800); filled('原文件_材质成品_制作中_v001')
        checks.append('filtered candidate renaming targets the visible item and preserves an unrelated typed value until the candidate is selected')
        click(763); click(788)
        assert [n.text(u.GetDlgItem(panel, i)) for i in [852, 853, 864]] == ['模型定版', '材质成品', '制作中']
        e.capture(panel, out/'renamed-quick.png')
        stop(); start()
        assert [n.text(u.GetDlgItem(panel, i)) for i in [852, 853, 864]] == ['模型定版', '材质成品', '制作中']
        click(789); click(784)
        values = [n.text(u.GetDlgItem(panel, 800+i)) for i in range(6)]
        assert values[0] == '制作中' and '工作' not in values and len(values) == len(set(values))
        click(800); filled('原文件_制作中_v001')
        p = configparser.ConfigParser(); p.read(cfg, encoding='utf-16')
        assert p['Jump'].getint('candidateRenames') >= 4
        assert host.poll() is None
        checks.append('renames survive a full app restart in both views, remain scoped to the field and do not revive duplicate legacy candidates or submit Save')
        result = {'passed': checks, 'output': str(out)}
        (out/'report.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(result, ensure_ascii=False, indent=2))
    except Exception:
        print('Failure output: '+str(out), flush=True)
        if panel and u.IsWindowVisible(panel): e.capture(panel, out/'failure.png')
        raise
    finally:
        stop()


if __name__ == '__main__':
    run()
