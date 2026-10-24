"""Runs only against Jump and its own standard Windows file-dialog test host."""
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import subprocess
import time
import hashlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests' / 'output' / 'native'
OUT.mkdir(parents=True, exist_ok=True)
_fixture_hosts=[]
os.environ['PYTHONIOENCODING'] = 'utf-8'
u = c.WinDLL('user32', use_last_error=True)
k = c.WinDLL('kernel32', use_last_error=True)
# Foreground fixtures must never run concurrently in this checkout.
k.CreateMutexW.argtypes=[c.c_void_p,w.BOOL,w.LPCWSTR];k.CreateMutexW.restype=w.HANDLE
_fixture_mutex=k.CreateMutexW(None,True,'Local\\Jump.ForegroundChecks.'+hashlib.sha256(str(ROOT).lower().encode()).hexdigest()[:16])
if c.get_last_error()==183:
    raise RuntimeError('Another Jump foreground test is running. Run interactive checks serially.')
u.SwitchToThisWindow.argtypes=[w.HWND,w.BOOL]
# Holding the foreground mutex, so no other fixture is writing run folders.
import output_retention
output_retention.prune()

u.SetProcessDpiAwarenessContext.argtypes = [w.HANDLE]
u.SetProcessDpiAwarenessContext(c.c_void_p(-4 & ((1 << (c.sizeof(c.c_void_p)*8))-1)))
CB = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetDlgCtrlID.argtypes = [w.HWND]
u.GetDlgItem.argtypes = [w.HWND, c.c_int]
u.GetDlgItem.restype = w.HWND
u.GetForegroundWindow.restype = w.HWND
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.IsWindowVisible.argtypes = [w.HWND]
u.IsWindowEnabled.argtypes = [w.HWND]
u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.SendMessageW.restype = w.LPARAM

def text(hwnd, fn=u.GetWindowTextW):
    buffer = c.create_unicode_buffer(4096)
    if fn == u.GetWindowTextW:
        u.SendMessageW(hwnd, 0x000D, len(buffer), c.addressof(buffer))
    else:
        fn(hwnd, buffer, len(buffer))
    return buffer.value

def windows(pid, cls=None, visible=False):
    found = []
    @CB
    def each(hwnd, _):
        owner = w.DWORD()
        u.GetWindowThreadProcessId(hwnd, c.byref(owner))
        if owner.value == pid and (not cls or text(hwnd, u.GetClassNameW) == cls) and (not visible or u.IsWindowVisible(hwnd)):
            found.append(hwnd)
        return True
    u.EnumWindows(each, 0)
    return found

def children(hwnd):
    found = []
    @CB
    def each(child, _):
        found.append(child)
        return True
    u.EnumChildWindows(hwnd, each, 0)
    return found

def dialogs(pid, title=None):
    """Native dialogs and the settings center's visible child editors."""
    found=windows(pid,'#32770',True)
    for manager in windows(pid,'Jump.Manager',True):
        found += [h for h in children(manager) if text(h,u.GetClassNameW)=='#32770' and u.IsWindowVisible(h)]
    return [h for h in found if title is None or text(h)==title]

def manager_tab(manager,index):
    control=u.GetDlgItem(manager,200)
    # Native button tabs wrap into one column; the common control adds three
    # physical pixels between its fixed-height rows.
    dpi=u.GetDpiForWindow(manager)
    point=(60*dpi//96)|((16*dpi//96+index*(32*dpi//96+3))<<16)
    u.SendMessageW(control,0x201,1,point);u.SendMessageW(control,0x202,0,point)
    wait(lambda:u.SendMessageW(control,0x130B,0,0)==index,label='settings section '+str(index))

def wait(predicate, timeout=8, label='condition'):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        result = predicate()
        if result:
            return result
        time.sleep(.035)
    raise AssertionError('Timed out waiting for ' + label)

def activate(window):
    def allowed():
        foreground=u.GetForegroundWindow()
        if foreground==window:return True
        if text(window,u.GetClassNameW)=='Jump.Panel':
            pid=w.DWORD();u.GetWindowThreadProcessId(foreground,c.byref(pid))
            return any(host.poll() is None and host.pid==pid.value for host in _fixture_hosts)
        return False
    last_attempt = -1.0
    def try_activate():
        nonlocal last_attempt
        if allowed(): return True
        if time.monotonic() - last_attempt < .7: return False
        last_attempt = time.monotonic()
        target=window
        if text(window,u.GetClassNameW)=='Jump.Panel':
            target=next((dialog for host in reversed(_fixture_hosts) if host.poll() is None
                         for dialog in windows(host.pid,visible=True)
                         if text(dialog,u.GetClassNameW) in ('#32770','Jump.TestHost')),window)
        # The shell can restore focus to the previous app just after an owned host
        # exits. Re-request our new fixture, instead of waiting on a single lost
        # activation; never send input to or close the unrelated foreground app.
        u.IsIconic.argtypes=[w.HWND];u.ShowWindow.argtypes=[w.HWND,c.c_int]
        u.SetForegroundWindow.argtypes=[w.HWND]
        if u.IsIconic(target):u.ShowWindow(target,9)
        if not u.SetForegroundWindow(target):u.SwitchToThisWindow(target,True)
        return allowed()
    wait(try_activate, label='owned fixture foreground')


def panel_ready(panel):
    # Path editing is disabled until the adapter has completed the current fill.
    directory = u.GetDlgItem(panel, 155)
    return bool(u.IsWindowEnabled(directory) if directory else u.IsWindowEnabled(u.GetDlgItem(panel,101)))


def click(panel, caption=None, ident=None, double=False):
    if text(panel,u.GetClassNameW) in ('Jump.Panel','Jump.Manager'):
        activate(panel)
    if ident == 106 and not u.GetDlgItem(panel, ident):
        assert not double
        # Invoke the existing save command; the final filename is display-only.
        u.SendMessageW(panel, 0x111, 106, 0)
        return
    # These commands now live in overflow menus rather than persistent buttons.
    if ident in {105, 107, 109, 110, 112, 116, 120, 121, 122, 127, 142,
                 138, 139} and not u.GetDlgItem(panel, ident):
        assert not double
        u.SendMessageW(panel, 0x111, ident, 0)
        return
    if caption is not None and text(panel, u.GetClassNameW) == 'Jump.Panel' and not any(text(h) == caption and text(h,u.GetClassNameW)=='Button' and u.IsWindowVisible(h) for h in children(panel)):
        owner = w.DWORD()
        u.GetWindowThreadProcessId(panel, c.byref(owner))
        for popup in windows(owner.value, 'Jump.Flyout'):
            u.SendMessageW(popup, 0x111, 2, 0)
        u.SendMessageW(panel, 0x111, 158, 0)
        popup = wait(lambda: next(iter(windows(owner.value, 'Jump.Flyout')), None))
        label = c.create_unicode_buffer(caption)
        index = u.SendMessageW(u.GetDlgItem(popup, 146), 0x1A2, -1, c.addressof(label))
        if index >= 0:
            assert not double
            u.SendMessageW(u.GetDlgItem(popup, 146), 0x186, index, 0)
            u.SendMessageW(popup, 0x111, 150, 0)
            return
        u.SendMessageW(popup, 0x111, 2, 0)
        raise AssertionError('No current location or naming button: '+caption)
    control = wait(lambda: u.GetDlgItem(panel, ident) if ident is not None else next((h for h in children(panel) if text(h) == caption and text(h,u.GetClassNameW)=='Button' and u.IsWindowVisible(h)), None), label='button '+str(caption or ident))
    assert control, (caption, ident)
    code = 5 if double else 0
    if double:
        # Exercise the actual button's first click followed by its double-click notification.
        point = 10 | (10 << 16)
        u.SendMessageW(control, 0x201, 1, point)
        u.SendMessageW(control, 0x202, 0, point)
        time.sleep(.2)
        u.SendMessageW(control, 0x203, 1, point)
        u.SendMessageW(control, 0x202, 0, point)
    else:
        u.SendMessageW(panel, 0x111, u.GetDlgCtrlID(control) | (code << 16), control)

def folder_is(dialog, folder):
    return any(str(folder).lower() in text(h).lower() for h in children(dialog) if text(h, u.GetClassNameW) == 'ToolbarWindow32')

def write_config(path):
    def enc(value):
        raw = value.encode('utf-16-le')
        return ''.join(f'{int.from_bytes(raw[i:i+2], "little"):04X}' for i in range(0, len(raw), 2))
    first = OUT / 'delivery'; second = OUT / 'assets'
    first.mkdir(exist_ok=True); second.mkdir(exist_ok=True)
    for extension in ['txt','md']:
        for stem in ['角色参考', '角色设定_角色参考']:
            fixture = first / (stem+'.'+extension)
            if fixture.exists() and fixture.read_text(encoding='utf-8') == 'Jump native integration test\n':
                fixture.unlink()
    content = '[Jump]\r\nschema=1\r\ngroups=0\r\nFavorite=2\r\nHistory=0\r\nmodifiers=-1\r\nrecipes=0\r\n'
    for i, (name, folder) in enumerate([('测试成品', first), ('测试素材', second)]):
        content += f'\r\n[Favorite{i}]\r\nname={enc(name)}\r\npath={enc(str(folder))}\r\n'
    path.write_text(content, encoding='utf-16')
    return first, second

def launch_host(kind='modern', extension='txt'):
    initial = OUT / ('initial-' + kind + '-' + extension)
    initial.mkdir(exist_ok=True)
    source = OUT / 'source' / 'source.txt'
    source.parent.mkdir(exist_ok=True)
    source.write_text('native test document', encoding='utf-8')
    args = [str(ROOT/'build'/'Jump.TestHost.exe'), '--output', str(initial), '--source', str(source)]
    if kind == 'classic': args.append('--classic')
    if kind == 'open': args.append('--open')
    if extension == 'md': args.append('--markdown')
    host = subprocess.Popen(args, creationflags=subprocess.CREATE_NO_WINDOW)
    _fixture_hosts.append(host)
    dialog = wait(lambda: next(iter(windows(host.pid, '#32770', True)), None), label='native file dialog')
    return host, dialog, initial
