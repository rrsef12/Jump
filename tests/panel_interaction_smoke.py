"""Interaction regressions using only Jump's owned native file-dialog fixtures."""
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import native_integration as n
import naming_fixture
import extended_integration as e


def run():
    out = n.ROOT / 'tests/output/panel'
    out.mkdir(parents=True, exist_ok=True)
    cfg = out / 'settings.ini'
    _, destination = n.write_config(cfg)
    naming_fixture.install(cfg)
    saved = destination / '连续编辑_完成.txt'
    # Remove only this test's known output, checking its content first.
    if saved.exists():
        assert saved.read_text(encoding='utf-8') == 'Jump native integration test\n'
        saved.unlink()
    u = n.u
    u.GetClientRect.argtypes = [w.HWND, c.POINTER(w.RECT)]
    u.IsWindow.argtypes = [w.HWND]
    class GuiThread(c.Structure):
        _fields_ = [('size', w.DWORD), ('flags', w.DWORD)] + \
            [(name, w.HWND) for name in ('active', 'focus', 'capture', 'menu', 'move', 'caret')] + [('rect', w.RECT)]
    u.GetGUIThreadInfo.argtypes = [w.DWORD, c.POINTER(GuiThread)]

    def focused():
        info = GuiThread()
        info.size = c.sizeof(info)
        assert u.GetGUIThreadInfo(u.GetWindowThreadProcessId(panel, None), c.byref(info))
        return info.focus

    def focus_preview():
        point = 8 | (8 << 16)
        u.SendMessageW(preview, 0x201, 1, point)
        u.SendMessageW(preview, 0x202, 0, point)
    app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-host-only'],
                           creationflags=subprocess.CREATE_NO_WINDOW)
    host = None
    checks = []
    try:
        control = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
        host, dialog, _ = n.launch_host()
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        time.sleep(.4)
        preview = u.GetDlgItem(panel, 101)
        button = next(h for h in n.children(panel) if n.text(h) == '角色设定')
        assert n.text(preview,u.GetClassNameW)=='Static'
        # A click at the right edge of a name is still an ordinary fill action.
        rect = w.RECT()
        u.GetClientRect(button, c.byref(rect))
        point = (rect.right - 4) | ((rect.bottom // 2) << 16)
        u.SetForegroundWindow(panel)
        u.SendMessageW(button, 0x201, 1, point)
        u.SendMessageW(button, 0x202, 0, point)
        n.wait(lambda: n.text(preview) == '角色设定.txt' and n.panel_ready(panel))
        time.sleep(.65)
        assert host.poll() is None and u.IsWindowVisible(dialog)
        assert u.GetDlgItem(panel, 101) == preview and u.IsWindow(button)
        checks.append('right edge of preset fills without submitting or rebuilding controls')

        # Edit while a real asynchronous directory navigation is running.
        u.SendMessageW(panel,0x111,755,0)
        preview=n.wait(lambda:u.GetDlgItem(panel,896),label='explicit full-name editor')
        initial_rect=e.rect(preview)
        result=u.GetDlgItem(panel,101)
        n.click(panel, caption='测试素材')
        assert u.IsWindowEnabled(preview), 'navigation disabled the filename input'
        u.SetForegroundWindow(panel)
        focus_preview()
        e.set_field(panel, 896, '连续编辑_完成')
        u.SendMessageW(preview, 0xB1, 4, 4)
        n.wait(lambda: n.folder_is(dialog, destination) and
               any(n.text(h) == '连续编辑_完成' for h in n.children(dialog)
                   if n.text(h, u.GetClassNameW) == 'Edit'), label='latest edit reaches host')
        time.sleep(.65)
        assert u.GetDlgItem(panel, 896) == preview and n.text(preview) == '连续编辑_完成'
        assert focused() == preview
        assert u.SendMessageW(preview, 0xB0, 0, 0) == 4 | (4 << 16), 'caret moved'
        after = e.rect(preview)
        assert tuple(getattr(after, x) for x in ('left', 'top', 'right', 'bottom')) == \
            tuple(getattr(initial_rect, x) for x in ('left', 'top', 'right', 'bottom'))
        checks.append('typing during navigation preserves newest text, focus, caret and save position')

        u.PostMessageW(control, 0x113, 2, 0)
        time.sleep(.4)
        assert u.GetDlgItem(panel, 896) == preview and u.GetDlgItem(panel,101)==result
        checks.append('native refresh preserves control handles')
        e.capture(panel, out/'assistant-side.png')

        # Keyboard focus moves through native controls and Enter submits from the filename.
        u.PostMessageW(preview, 0x100, 9, 1)
        u.PostMessageW(preview, 0x101, 9, 1)
        n.wait(lambda: focused() != preview, label='Tab moves focus')
        focus_preview()
        u.PostMessageW(preview, 0x100, 13, 1)
        u.PostMessageW(preview, 0x101, 13, 1)
        host.wait(timeout=8)
        assert host.returncode == 0 and saved.is_file()
        assert saved.read_text(encoding='utf-8') == 'Jump native integration test\n'
        checks.append('Tab navigation works and Enter saves the latest filename')
        (out/'panel-report.json').write_text(json.dumps({'passed': checks}, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'passed': checks}, ensure_ascii=False, indent=2))
    except Exception:
        if 'panel' in locals():
            print('DIAGNOSTICS', {'panel': n.text(panel), 'preview': n.text(u.GetDlgItem(panel, 101)),
                                 'foreground': n.text(u.GetForegroundWindow(), u.GetClassNameW),
                                 'host': [(n.text(h, u.GetClassNameW), n.text(h)) for h in n.children(dialog)
                                          if n.text(h, u.GetClassNameW) in ('Edit', 'ToolbarWindow32')]}, flush=True)
            e.capture(panel, out/'failure.png')
        raise
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid, '#32770', True):
                u.PostMessageW(dialog, 0x111, 2, u.GetDlgItem(dialog, 2))
            try:
                host.wait(timeout=3)
            except subprocess.TimeoutExpired:
                host.terminate()
        for control in n.windows(app.pid, 'Jump.Control'):
            u.PostMessageW(control, 0x10, 0, 0)
        try:
            app.wait(timeout=3)
        except subprocess.TimeoutExpired:
            app.terminate()


if __name__ == '__main__':
    run()
