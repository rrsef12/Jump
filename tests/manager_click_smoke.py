"""Management menu commands open or restore the window on the first invocation."""
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import native_integration as n


def run():
    out = n.ROOT/'tests/output/manager-click'
    out.mkdir(parents=True, exist_ok=True)
    cfg = out/'settings.ini'
    n.write_config(cfg)
    u = n.u
    u.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
    u.SwitchToThisWindow.argtypes = [w.HWND, w.BOOL]
    u.IsIconic.argtypes = [w.HWND]
    u.ShowWindow.argtypes = [w.HWND, c.c_int]
    # Match dev.ps1 / startup shortcut. CREATE_NO_WINDOW alone does not reproduce
    # STARTF_USESHOWWINDOW/SW_HIDE swallowing the first ShowWindow request.
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'],
                           startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW)
    host = None
    checks = []

    def mouse_click(panel, ident):
        button = u.GetDlgItem(panel, ident)
        if not button and ident in (105, 112):
            # The menu sends this command after the user selects its item.
            u.SendMessageW(panel, 0x111, ident, 0)
            return
        assert button and u.IsWindowEnabled(button)
        rect = w.RECT()
        u.GetClientRect(button, c.byref(rect))
        point = (rect.right//2) | ((rect.bottom//2) << 16)
        activation = u.SendMessageW(panel, 0x21, panel, (0x201 << 16) | 1)
        if activation in (1, 2):  # Let Windows activate if WM_MOUSEACTIVATE requested it.
            u.SetForegroundWindow(panel)
        assert activation not in (2, 4), 'first mouse click was discarded'
        u.SendMessageW(button, 0x201, 1, point)
        time.sleep(.08)
        u.SendMessageW(button, 0x202, 0, point)

    try:
        controller = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
        host, dialog, _ = n.launch_host()
        n.activate(dialog)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        for ident, mode, expected in [(105, 0, 1), (112, 0, 0), (149, 1, 4), (149, 2, 4)]:
            u.SwitchToThisWindow(dialog, True)
            n.wait(lambda: u.IsWindowVisible(panel) and u.GetForegroundWindow() == dialog)
            if ident==149 and not u.GetDlgItem(panel,179):n.click(panel,ident=142)
            time.sleep(.3)
            if ident == 149:
                n.click(panel,ident=179 if mode==1 else 180)
                popup=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Flyout',True)),None))
                mouse_click(popup,149)
            else: mouse_click(panel, ident)
            time.sleep(.5)
            manager = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Manager', True)), None), label='manager after first click')
            assert u.GetForegroundWindow() == manager, 'first click did not bring management to foreground'
            assert u.SendMessageW(u.GetDlgItem(manager, 200), 0x130B, 0, 0) == expected
            assert not u.IsWindowVisible(panel), 'helper must hide while managing'
            checks.append({'entry': ident, 'mode': mode, 'firstClick': True})
            if ident == 105:
                for minimized in [False, True]:
                    if minimized:
                        u.ShowWindow(manager, 6)  # SW_MINIMIZE
                        n.wait(lambda: u.IsIconic(manager))
                        time.sleep(.3)
                    n.activate(dialog)
                    n.wait(lambda: u.IsWindowVisible(panel) and u.GetForegroundWindow() == dialog)
                    mouse_click(panel, ident)
                    n.wait(lambda: u.GetForegroundWindow() == manager and not u.IsIconic(manager),
                           label='single click restores existing management window')
                    assert n.windows(app.pid, 'Jump.Manager') == [manager], 'reuse the same window'
                    checks.append({'existingWindow': True, 'minimized': minimized, 'firstClick': True})
            u.PostMessageW(manager, 0x10, 0, 0)
            n.wait(lambda: not n.windows(app.pid, 'Jump.Manager'))
        (out/'report.json').write_text(json.dumps(checks, indent=2), encoding='utf-8')
        print(json.dumps(checks, indent=2))
    except Exception:
        print({'foregroundClass': n.text(u.GetForegroundWindow(), u.GetClassNameW),
               'managers': n.windows(app.pid, 'Jump.Manager')}, flush=True)
        raise
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid, '#32770', True):
                u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2) or 0)
            try: host.wait(timeout=3)
            except subprocess.TimeoutExpired: host.terminate()
        for controller in n.windows(app.pid, 'Jump.Control'):
            u.PostMessageW(controller, 0x10, 0, 0)
        try: app.wait(timeout=3)
        except subprocess.TimeoutExpired: app.terminate()


if __name__ == '__main__':
    run()
