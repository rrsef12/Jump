"""Behavioral checks for the generic UIA adapter, using only our owned custom windows."""
import configparser
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import uuid
import native_integration as n
import naming_fixture
import extended_integration as e

u = n.u


def run():
    out = n.ROOT / 'tests/output/generic' / uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg = out / 'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nmodifiers=-1\nrecipes=0\n', encoding='utf-16')
    naming_fixture.install(cfg)
    app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-ui', '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
    hosts = []
    checks = []

    def command(window, ident):
        u.PostMessageW(window, 0x111, ident, u.GetDlgItem(window, ident) or 0)

    def activate_fixture(window):
        # Restore an owned fixture if the shell minimized it during rapid host replacement.
        u.IsIconic.argtypes = [w.HWND]; u.ShowWindow.argtypes = [w.HWND, c.c_int]
        if u.IsIconic(window): u.ShowWindow(window, 9)
        u.SetForegroundWindow(window)
        n.activate(window)

    def config():
        result = configparser.ConfigParser(); result.read(cfg, encoding='utf-16'); return result

    def decode(raw):
        return ''.join(chr(int(raw[i:i+4], 16)) for i in range(0, len(raw), 4))

    def launch(mode, attached=True):
        folder = out / (mode + '-' + uuid.uuid4().hex[:6]); folder.mkdir()
        host = subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(folder), '--custom', mode], creationflags=subprocess.CREATE_NO_WINDOW)
        hosts.append(host); n._fixture_hosts.append(host)
        window = n.wait(lambda: next(iter(n.windows(host.pid, 'Jump.TestHost', True)), None))
        activate_fixture(window)
        if not attached:
            time.sleep(2)
            assert not n.windows(app.pid, 'Jump.Panel', True), mode
            return host, window, folder, None
        last_activation = time.monotonic()
        def attached_panel():
            nonlocal last_activation
            panel = next(iter(n.windows(app.pid, 'Jump.Panel', True)), None)
            if not panel and time.monotonic() - last_activation > .7:
                # Foreground may return to the shell after the previous host exits.
                activate_fixture(window); last_activation = time.monotonic()
            return panel
        panel = n.wait(attached_panel, label=mode+' attachment')
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == ('' if mode == 'empty' else 'original.asset'))
        return host, window, folder, panel

    def close(host, window):
        u.PostMessageW(window, 0x10, 0, 0); host.wait(timeout=4)
        n.wait(lambda: not n.windows(app.pid, 'Jump.Panel', True))

    def filled(panel, window, value):
        n.wait(lambda: n.text(u.GetDlgItem(window, 602)) == value and n.panel_ready(panel), label='fill '+value)

    def modal(title, pid=None):
        return n.wait(lambda: next((h for h in n.windows(pid or app.pid, '#32770', True) if n.text(h) == title), None), label=title)

    def dismiss_notice():
        notice = modal('Jump')
        button = next(h for h in n.children(notice) if n.text(h, u.GetClassNameW) == 'Button')
        command(notice, u.GetDlgCtrlID(button))
        n.wait(lambda: notice not in n.windows(app.pid, '#32770', True))

    try:
        controller = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
        host, window, folder, panel = launch('empty')
        assert not u.IsWindowEnabled(u.GetDlgItem(window, 1))
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定')
        assert u.IsWindowEnabled(u.GetDlgItem(window, 1)) and host.poll() is None
        close(host, window)
        checks.append('empty filename and disabled Save still attach automatically; filling enables the original Save')

        host, window, folder, _ = launch('unknown-title', attached=False)
        activate_fixture(window)
        u.PostMessageW(controller, 0x312, 3, 0)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None), label='explicit detect')
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定.asset')
        assert config()['Jump'].getint('filenameBindings', fallback=0) == 0
        close(host, window)
        checks.append('explicit detect attaches an unfamiliar title directly without clipboard or persistent binding')

        host, window, folder, panel = launch('auto')
        assert str(folder) in n.text(u.GetDlgItem(panel, 712))
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定.asset')
        command(panel, 106); time.sleep(.3)
        assert host.poll() is None and not (folder/'角色设定.asset').exists()
        assert config()['Jump'].getint('savedFiles', fallback=0) == 0
        checks.append('semantic name and separate directory detected; labels and Enter fill without saving')
        u.SendMessageW(panel,0x111,794,0); filled(panel, window, '角色设定_v001.asset')
        e.capture(panel, out/'generic-panel.png')
        occupied = folder/'角色设定_v001.asset'; occupied.write_text('other writer', encoding='utf-8')
        command(panel, 709)
        conflict = modal('确认编号变化'); command(conflict, 7)
        filled(panel, window, '角色设定_v002.asset')
        assert host.poll() is None and occupied.read_text(encoding='utf-8') == 'other writer'
        n.activate(window); command(panel, 709)
        n.wait(lambda: host.poll() is not None, label='explicit Invoke save')
        n.wait(lambda: config()['Jump'].getint('savedFiles', fallback=0) == 1, label='verified record')
        assert (folder/'角色设定_v002.asset').is_file()
        assert decode(config()['SavedFile0']['path']) == str(folder/'角色设定_v002.asset')
        checks.append('explicit submit rechecks number; rejected change does not save; verified actual file recorded')

        host, window, folder, panel = launch('auto')
        occupied = folder/'角色设定.asset'; occupied.write_text('keep', encoding='utf-8')
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定.asset')
        command(panel, 709)
        confirm = modal('Fixture overwrite', host.pid); command(confirm, 7)
        n.wait(lambda: confirm not in n.windows(host.pid, '#32770', True))
        assert occupied.read_text(encoding='utf-8') == 'keep'
        close(host, window)
        assert config()['Jump'].getint('savedFiles') == 1
        checks.append('original application overwrite confirmation retained; cancellation creates no record')

        host, window, folder, panel = launch('fullpath')
        n.click(panel, caption='角色参考'); filled(panel, window, str(folder/'角色参考.asset'))
        close(host, window)
        checks.append('full-path filename editor retains its directory')

        host, window, folder, panel = launch('redirect')
        e.set_field(panel, 101, '角色新名')
        command(panel, 709)
        n.wait(lambda: '状态发生变化' in n.text(u.GetDlgItem(panel, 114)), label='post-write destination guard')
        assert host.poll() is None and not (folder/'custom-result.txt').exists()
        assert not list(folder.rglob('*.asset'))
        close(host, window)
        checks.append('provider changing directory during SetValue prevents submission to a different destination')

        for mode in ['no-directory', 'ambiguous-directory']:
            host, window, folder, panel = launch(mode)
            assert '目录未确认' in n.text(u.GetDlgItem(panel, 712))
            assert '目录请在原窗口选择' in n.text(u.GetDlgItem(panel,892))
            e.capture(panel,out/(mode+'-capabilities.png'))
            u.SendMessageW(panel,0x111,794,0)
            n.wait(lambda: '无法读取保存目录' in n.text(u.GetDlgItem(panel, 114)))
            assert n.text(u.GetDlgItem(window, 602)) == 'original.asset'
            close(host, window)
        checks.append('unknown or ambiguous directories disable numbering without guessing')

        host, window, folder, panel = launch('ambiguous-save')
        command(panel, 709); time.sleep(.3)
        assert host.poll() is None and not (folder/'original.asset').exists()
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定.asset')
        close(host, window)
        checks.append('ambiguous save actions disable submission while naming remains usable')

        for mode in ['readonly', 'password', 'ambiguous']:
            host, window, folder, _ = launch(mode, attached=False)
            close(host, window)
        checks.append('readonly, password and ambiguous filename controls are not auto-attached')

        host, window, folder, _ = launch('manual', attached=False)
        # Host starts with its own filename field focused; simulate registered hotkey dispatch.
        activate_fixture(window); u.PostMessageW(controller, 0x312, 1, 0)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None), label='manual binding')
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定.asset')
        assert config()['Jump'].getint('filenameBindings') == 1
        close(host, window)
        host, window, folder, panel = launch('manual')
        close(host, window)
        checks.append('explicit binding persists semantic selector and reconnects to a new window instance')

        host, window, folder, panel = launch('stay-open')
        n.click(panel, caption='角色设定'); filled(panel, window, '角色设定.asset')
        command(panel, 709)
        n.wait(lambda: config()['Jump'].getint('savedFiles') == 2, label='verified save while exporter remains open')
        assert host.poll() is None and (folder/'角色设定.asset').is_file()
        close(host, window)
        checks.append('file verification starts after submission even if the custom exporter remains open')
        report={'passed': checks, 'artifacts': str(out)}
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        (out.parent/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    except Exception:
        state={'foregroundIsJump': u.GetForegroundWindow() in n.windows(app.pid),
               'ownedHosts': [{'running': h.poll() is None, 'windows': [
                   {'class': n.text(v, u.GetClassNameW), 'visible': bool(u.IsWindowVisible(v)),
                    'foreground': u.GetForegroundWindow() == v, 'minimized': bool(u.IsIconic(v))}
                   for v in n.windows(h.pid)]} for h in hosts if h.poll() is None]}
        (out/'failure-state.json').write_text(json.dumps(state, indent=2), encoding='utf-8')
        raise
    finally:
        for host in hosts:
            if host.poll() is None:
                for window in n.windows(host.pid): u.PostMessageW(window, 0x10, 0, 0)
                try: host.wait(timeout=3)
                except subprocess.TimeoutExpired: host.terminate(); host.wait(timeout=3)
        for window in n.windows(app.pid, 'Jump.Control'): u.PostMessageW(window, 0x10, 0, 0)
        try: app.wait(timeout=3)
        except subprocess.TimeoutExpired: app.terminate(); app.wait(timeout=3)


if __name__ == '__main__':
    run()
