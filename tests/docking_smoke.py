"""Measure visible docking gaps against our own native test dialog."""
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import native_integration as n

out = n.ROOT / 'tests' / 'output' / 'docking'
out.mkdir(parents=True, exist_ok=True)
cfg = out / 'settings.ini'
n.write_config(cfg)
u = n.u
u.GetWindowRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
u.SetWindowPos.argtypes = [w.HWND, w.HWND, c.c_int, c.c_int, c.c_int, c.c_int, w.UINT]
u.GetDpiForWindow.argtypes = [w.HWND]
dwm = c.WinDLL('dwmapi')
dwm.DwmGetWindowAttribute.argtypes = [w.HWND, w.DWORD, c.c_void_p, w.DWORD]

def visible(hwnd):
    rect = w.RECT()
    if dwm.DwmGetWindowAttribute(hwnd, 9, c.byref(rect), c.sizeof(rect)) < 0:
        u.GetWindowRect(hwnd, c.byref(rect))
    return rect

app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
host = None
try:
    control = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
    host, dialog, _ = n.launch_host()
    # Windows remembers the last native dialog position. Start our owned fixture
    # on the left so this check consistently exercises right-side docking first.
    u.SetWindowPos(dialog, None, 100, 100, 0, 0, 0x15)
    panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
    expected = round(u.GetDpiForWindow(dialog) * 3 / 96)
    n.wait(lambda: visible(panel).left - visible(dialog).right == expected, label='right visible gap')
    right_gap = visible(panel).left - visible(dialog).right
    assert u.GetForegroundWindow() == dialog
    width = u.GetSystemMetrics(0)
    r = w.RECT(); u.GetWindowRect(dialog, c.byref(r))
    u.SetWindowPos(dialog, None, width - (r.right-r.left) - 10, r.top, 0, 0, 0x15)
    n.wait(lambda: visible(dialog).left - visible(panel).right == expected, label='left visible gap after moving')
    left_gap = visible(dialog).left - visible(panel).right
    report = {'dpi': u.GetDpiForWindow(dialog), 'rightGapPixels': right_gap, 'leftGapPixels': left_gap, 'focusPreserved': u.GetForegroundWindow() == dialog}
    (out/'docking-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
finally:
    if host and host.poll() is None:
        for dialog in n.windows(host.pid, '#32770', True): u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2) or 0)
        try: host.wait(timeout=3)
        except subprocess.TimeoutExpired: host.terminate()
    for window in n.windows(app.pid, 'Jump.Control'): u.PostMessageW(window, 0x10, 0, 0)
    try: app.wait(timeout=3)
    except subprocess.TimeoutExpired: app.terminate()
