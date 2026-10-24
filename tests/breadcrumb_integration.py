"""Foreground opt-in custom-address navigation checks; only owned fixtures."""
import argparse
import json
from pathlib import Path
import subprocess
import uuid
import native_integration as n
import extended_integration as e

def run():
    parser = argparse.ArgumentParser()
    parser.add_argument('--python', required=True)
    parser.add_argument('--qt-root', required=True)
    args = parser.parse_args()
    out = n.ROOT/'tests/output/breadcrumb'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    target = out/'交付 assets'
    target.mkdir()
    enc = lambda s: ''.join(f'{ord(ch):04X}' for ch in str(s))
    cfg = out/'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nmodifiers=-1\nFavorite=1\n[Favorite0]\nname='+enc('快速交付')+'\npath='+enc(target)+'\n', encoding='utf-16')
    app = None
    hosts = []
    checks = []
    try:
        for mode in ['normal', 'ignored', 'ambiguous']:
            ambiguous = mode == 'ambiguous'
            folder = out/mode
            folder.mkdir()
            command = [args.python, str(n.ROOT/'tests/breadcrumb_save_host.py'), '--qt-root', args.qt_root, '--output', str(folder)]
            if ambiguous: command.append('--ambiguous')
            if mode == 'ignored': command.append('--ignore-navigation')
            host = subprocess.Popen(command, creationflags=subprocess.CREATE_NO_WINDOW)
            hosts.append(host); n._fixture_hosts.append(host)
            app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-ui',
                                    '--test-target-pid', str(host.pid)], creationflags=subprocess.CREATE_NO_WINDOW)
            n.wait(lambda: n.windows(app.pid, 'Jump.Control'))
            dialog = n.wait(lambda: next((h for h in n.windows(host.pid, visible=True) if n.text(h)=='Save custom document'), None))
            n.activate(dialog)
            panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
            n.wait(lambda: n.text(n.u.GetDlgItem(panel,101))=='original.docx',label='fixture filename confirms attachment')
            last_state = {}
            def state():
                nonlocal last_state
                try: last_state = json.loads((folder/'state.json').read_text(encoding='utf-8'))
                except (OSError, json.JSONDecodeError): pass
                return last_state
            n.wait(lambda: 'name' in state())
            if ambiguous:
                assert not n.u.GetDlgItem(panel, 113), 'ambiguous address bars must not enable navigation'
                checks.append('ambiguous breadcrumb controls keep naming-only behavior')
            elif mode == 'ignored':
                n.click(panel, caption='快速交付')
                n.wait(lambda: '未通过核验' in n.text(n.u.GetDlgItem(panel,114)),label='address echo is not navigation')
                assert state()['directory']=='' and state()['submissions']==0 and state()['name']=='original.docx'
                assert str(target) not in n.text(n.u.GetDlgItem(panel,113))
                checks.append('a picker that accepts address text but does not navigate cannot report success or cache the destination')
            else:
                assert n.u.GetDlgItem(panel, 113), 'virtual root still enables quick navigation'
                n.click(panel, caption='快速交付')
                n.wait(lambda: state().get('directory') == str(target) and n.panel_ready(panel), label='virtual root to favorite')
                assert state()['name']=='original.docx' and state()['submissions']==0 and state()['addressOpens']>=2
                assert str(target) in n.text(n.u.GetDlgItem(panel, 113))
                checks.append('virtual root navigates through transient address editor; independent reread confirms actual directory')
                for invalid in [out/'missing', out/'settings.ini']:
                    opens = state()['addressOpens']
                    n.click(panel, ident=155); e.set_field(panel, 156, str(invalid))
                    n.u.SendMessageW(n.u.GetDlgItem(panel, 156), 0x100, 13, 0)
                    n.wait(lambda: '目标目录不存在' in n.text(n.u.GetDlgItem(panel,114)))
                    assert state()['addressOpens']==opens and state()['directory']==str(target) and state()['submissions']==0
                    n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,27,0)
                checks.append('nonexistent and existing-file destinations rejected before interacting with address bar')
                n.click(panel, ident=155); e.set_field(panel,156,str(folder))
                n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,13,0)
                n.wait(lambda: state()['directory']==str(folder) and n.panel_ready(panel),label='typed directory')
                assert state()['name']=='original.docx' and state()['submissions']==0
                checks.append('repeated typed navigation preserves filename and does not invoke Save')
                e.capture(panel, folder/'panel.png')
            n.u.PostMessageW(dialog,0x10,0,0)
            host.wait(timeout=5)
            n.wait(lambda: not n.windows(app.pid,'Jump.Panel',True))
            for h in n.windows(app.pid,'Jump.Control'): n.u.PostMessageW(h,0x10,0,0)
            app.wait(timeout=3)
        report = {'passed':checks, 'runtime':args.python, 'artifacts':str(out)}
        for path in [out/'report.json',out.parent/'report.json']:
            path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        e.capture(dialog, folder/'failure.png')
        for panel in n.windows(app.pid,'Jump.Panel',True):
            print('PANEL',[(n.u.GetDlgCtrlID(h),n.text(h)) for h in n.children(panel) if n.u.GetDlgCtrlID(h) in (101,113,114,156)])
        print('STATE',state())
        raise
    finally:
        for host in hosts:
            if host.poll() is None: host.terminate(); host.wait(timeout=3)
        for h in n.windows(app.pid,'Jump.Control'): n.u.PostMessageW(h,0x10,0,0)
        try: app.wait(timeout=3)
        except subprocess.TimeoutExpired: app.terminate(); app.wait(timeout=3)

if __name__=='__main__': run()
