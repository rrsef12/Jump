"""Opt-in real Qt integration, using an explicitly supplied installed PySide runtime.

Example: python tests/qt_save_integration.py --python <python.exe> --qt-root <Qt DLL directory>
Uses owned dialogs and an isolated Jump configuration; never operates on user applications.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time
import uuid
import native_integration as n
import naming_fixture
import extended_integration as e


def run():
    parser = argparse.ArgumentParser()
    parser.add_argument('--python', required=True)
    parser.add_argument('--qt-root', required=True)
    args = parser.parse_args()
    out = n.ROOT/'tests/output/qt-save'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg = out/'settings.ini'
    target = out/'交付 assets'; target.mkdir()
    def enc(text): return ''.join(f'{ord(ch):04X}' for ch in str(text))
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nmodifiers=-1\nFavorite=1\n[Favorite0]\nname='+enc('快速交付')+'\npath='+enc(target)+'\n', encoding='utf-16')
    naming_fixture.install(cfg)
    app = None
    hosts = []
    checks = []
    try:
        for title, opening in [('Save Image', False), ('Asset destination', False), ('Open Image', True)]:
            folder = out/('open' if opening else title.replace(' ', '-'))
            folder.mkdir()
            command = [args.python, str(n.ROOT/'tests/qt_save_host.py'), '--output', str(folder),
                       '--qt-root', args.qt_root, '--title', title]
            if opening: command.append('--open')
            host = subprocess.Popen(command, creationflags=subprocess.CREATE_NO_WINDOW)
            hosts.append(host); n._fixture_hosts.append(host)
            app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-ui',
                                    '--test-target-pid', str(host.pid)], creationflags=subprocess.CREATE_NO_WINDOW)
            n.wait(lambda: n.windows(app.pid, 'Jump.Control'))
            dialog = n.wait(lambda: next((h for h in n.windows(host.pid, visible=True) if n.text(h) == title), None), label='Qt '+title)
            n.activate(dialog)
            def state():
                path = folder/'state.json'
                return json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
            n.wait(lambda: 'name' in state())
            assert state()['name'] == '' and not state()['saveEnabled']
            if opening:
                time.sleep(1.5)
                assert not n.windows(app.pid, 'Jump.Panel', True)
                checks.append('Qt Open dialog does not acquire save behavior')
            else:
                panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None), label='auto Qt attachment')
                assert str(folder) in n.text(n.u.GetDlgItem(panel, 113))
                assert n.text(n.u.GetDlgItem(panel, 101)).endswith('.png') and not n.u.GetDlgItem(panel,115)
                n.click(panel, caption='快速交付')
                n.wait(lambda: Path(state()['directory']) == target, label='favorite navigates with empty filename')
                n.wait(lambda: state()['name'] == '' and n.panel_ready(panel), label='empty filename restored')
                assert state()['filter'] == 'Portable Network Graphics files (*.png)'
                assert not (folder/'result.json').exists()
                n.click(panel, caption='角色设定')
                n.wait(lambda: state()['name'] == '角色设定.png' and state()['saveEnabled'] and n.panel_ready(panel), label='Qt direct fill')
                existing_file = target/'not-a-directory.png'; existing_file.write_bytes(b'keep')
                n.click(panel, ident=155); e.set_field(panel,156,str(existing_file))
                n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,13,0)
                n.wait(lambda: '目标目录不存在' in n.text(n.u.GetDlgItem(panel,114)),label='file target rejected before Invoke')
                assert Path(state()['directory']) == target and state()['name'] == '角色设定.png'
                assert existing_file.read_bytes() == b'keep' and not (folder/'result.json').exists()
                n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,27,0)
                (target/'角色设定_v001.png').write_bytes(b'owned numbering fixture')
                u.SendMessageW(panel,0x111,794,0)
                n.wait(lambda: state()['name'] == '角色设定_v002.png', label='Qt number uses real folder and filter')
                (folder/'角色设定_v006.png').write_bytes(b'owned numbering fixture')
                n.click(panel, ident=155); e.set_field(panel,156,str(folder))
                n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,13,0)
                n.wait(lambda: Path(state()['directory']) == folder and state()['name'] == '角色设定_v007.png',
                       label='typed path navigation recomputes destination counter')
                n.wait(lambda:n.panel_ready(panel))
                assert state()['filter'] == 'Portable Network Graphics files (*.png)'
                n.click(panel, ident=155); e.set_field(panel,156,str(folder/'missing'))
                n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,13,0)
                n.wait(lambda: '不存在' in n.text(n.u.GetDlgItem(panel,114)) or '无法读取' in n.text(n.u.GetDlgItem(panel,114)),label='invalid destination reported')
                assert Path(state()['directory']) == folder and state()['name'] == '角色设定_v007.png'
                e.capture(panel, folder/'panel.png')
                assert not (folder/'result.json').exists()
                checks.append(title+': favorites navigate with empty filename; typed directory preserves format and recalculates counter; nonexistent target rejected; no save submitted')
            n.u.PostMessageW(dialog, 0x10, 0, 0)
            host.wait(timeout=5)
            result = json.loads((folder/'result.json').read_text(encoding='utf-8'))
            assert not result['accepted']
            n.wait(lambda: not n.windows(app.pid, 'Jump.Panel', True))
            for h in n.windows(app.pid, 'Jump.Control'): n.u.PostMessageW(h, 0x10, 0, 0)
            app.wait(timeout=3)
        report = {'passed': checks, 'runtime': args.python, 'artifacts': str(out)}
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        (out.parent/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    except Exception:
        for panel in n.windows(app.pid,'Jump.Panel',True):
            print('PANEL',n.text(panel),[(n.u.GetDlgCtrlID(h),n.text(h)) for h in n.children(panel) if n.u.GetDlgCtrlID(h) in (101,113,114,156)])
        print('STATE',state())
        raise
    finally:
        for host in hosts:
            if host.poll() is None:
                host.terminate(); host.wait(timeout=3)
        for h in n.windows(app.pid, 'Jump.Control'): n.u.PostMessageW(h, 0x10, 0, 0)
        try: app.wait(timeout=3)
        except subprocess.TimeoutExpired: app.terminate(); app.wait(timeout=3)


if __name__ == '__main__':
    run()
