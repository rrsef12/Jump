"""Owned fixtures for output fields and canvas-only application bridge."""
import json
from pathlib import Path
import subprocess
import sys
import time
import uuid
import native_integration as n
import naming_fixture
import extended_integration as e


def run():
    out = n.ROOT/'tests/output/universal-save'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    target = out/'交付 assets'; target.mkdir()
    checks = []
    app = host = None
    def stop(process):
        if not process or process.poll() is not None: return
        for h in n.windows(process.pid): n.u.PostMessageW(h, 0x10, 0, 0)
        try: process.wait(timeout=3)
        except subprocess.TimeoutExpired: process.terminate(); process.wait(timeout=3)
    try:
        for mode in ['fullpath', 'output-auto', 'output-empty', 'output-ignore', 'output-readonly', 'output-ambiguous', 'auto', 'bridge', 'bridge-late', 'bridge-ignore']:
            folder = out/mode; folder.mkdir()
            cfg = folder/'settings.ini'
            def enc(v): return ''.join(f'{ord(c):04X}' for c in str(v))
            cfg.write_text('[Jump]\nschema=1\ngroups=0\nmodifiers=-1\nFavorite=1\n[Favorite0]\nname='+enc('快速交付')+'\npath='+enc(target)+'\n', encoding='utf-16')
            naming_fixture.install(cfg)
            bridge = mode.startswith('bridge')
            if bridge:
                command = [sys.executable, str(n.ROOT/'integrations/universal/demo.py'), '--jump', str(n.ROOT/'build/Jump.exe'),
                           '--directory', str(folder), '--state', str(folder/'state.json')]
                if mode == 'bridge-ignore': command.append('--ignore-apply')
                if mode == 'bridge-late': command += ['--attach-delay', '1500']
            else:
                command = [str(n.ROOT/'build/Jump.TestHost.exe'), '--custom', mode, '--output', str(folder)]
            host = subprocess.Popen(command, creationflags=subprocess.CREATE_NO_WINDOW)
            n._fixture_hosts.append(host)
            window = n.wait(lambda: next((h for h in n.windows(host.pid, visible=True)
                                          if n.text(h) == ('Canvas output destination' if bridge else 'Export asset')), None))
            app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-ui', '--test-target-pid', str(host.pid)],
                                   creationflags=subprocess.CREATE_NO_WINDOW)
            n.activate(window)
            if mode == 'bridge-late':
                time.sleep(.3)
                assert not n.windows(app.pid, 'Jump.Panel', True), 'no pending save before registration'
            panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None), label=mode+' attaches')
            ready = lambda: bool(n.panel_ready(panel))
            n.wait(ready)
            def state():
                if bridge: return json.loads((folder/'state.json').read_text(encoding='utf-8'))
                raw = n.text(n.u.GetDlgItem(window, 602))
                return {'name': Path(raw).name, 'directory': str(Path(raw).parent) if mode == 'fullpath' else n.text(n.u.GetDlgItem(window, 601)), 'submissions': int((folder/'custom-result.txt').exists())}
            if mode in ('output-readonly', 'output-ambiguous', 'auto'):
                assert not n.u.GetDlgItem(panel, 113), mode+' must not claim directory navigation'
                n.click(panel, caption='角色设定')
                n.wait(lambda: state()['name'] == '角色设定.asset' and ready())
                assert state()['directory'] == str(folder)
                checks.append(mode+': unavailable/ambiguous/generic folder does not claim a navigation capability; naming works')
            else:
                assert n.u.GetDlgItem(panel, 113), mode+' exposes location shortcuts'
                n.click(panel, caption='快速交付')
                if mode.endswith('ignore'):
                    n.wait(lambda: any(s in n.text(n.u.GetDlgItem(panel,114)) for s in ('不一致', '未通过核验')), label=mode+' refusal')
                    assert state()['directory'] == str(folder) and state()['name'] == 'original.asset'
                    checks.append(mode+': ignored application changes report failure without Save')
                else:
                    n.wait(lambda: Path(state()['directory']) == target and n.panel_ready(panel), label=mode+' favorite destination')
                    assert state()['name'] == 'original.asset'
                    n.click(panel, caption='角色设定')
                    n.wait(lambda: state()['name'] == '角色设定.asset' and n.panel_ready(panel), label=mode+' name')
                    n.click(panel, ident=155); e.set_field(panel,156,str(folder))
                    n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,13,0)
                    n.wait(lambda: Path(state()['directory']) == folder and n.panel_ready(panel), label=mode+' typed path')
                    assert state()['name'] == '角色设定.asset'
                    n.click(panel, ident=155); e.set_field(panel,156,str(folder/'does-not-exist'))
                    n.u.SendMessageW(n.u.GetDlgItem(panel,156),0x100,13,0)
                    n.wait(lambda: '不存在' in n.text(n.u.GetDlgItem(panel,114)), label=mode+' invalid path')
                    assert Path(state()['directory']) == folder
                    e.capture(panel, folder/'panel.png')
                    checks.append(mode+': automatic attachment, favorite and typed locations, names/extensions retained, missing destination rejected')
            assert state()['submissions'] == 0 and not list(folder.glob('*.asset'))
            stop(host); stop(app)
        report = {'passed': checks, 'artifacts': str(out)}
        for path in [out/'report.json', out.parent/'report.json']:
            path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    except Exception:
        if app:
            for panel in n.windows(app.pid, 'Jump.Panel', True):
                print('PANEL', [(n.u.GetDlgCtrlID(h),n.text(h)) for h in n.children(panel) if n.u.GetDlgCtrlID(h) in (101,113,114,156)])
        raise
    finally:
        stop(host); stop(app)


if __name__ == '__main__': run()
