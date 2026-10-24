"""Acrylic and opaque fallback on owned dialogs, including native edit painting."""
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import uuid
from PIL import Image
import native_integration as n
import extended_integration as e

u = n.u
u.GetPropW.argtypes = [w.HWND, w.LPCWSTR]
u.GetPropW.restype = w.HANDLE
u.GetWindowLongW.argtypes = [w.HWND, c.c_int]
dwm = c.WinDLL('dwmapi')
dwm.DwmGetWindowAttribute.argtypes = [w.HWND, w.DWORD, c.c_void_p, w.DWORD]
gdi = c.WinDLL('gdi32')
gdi.GetPixel.argtypes = [w.HDC,c.c_int,c.c_int]; gdi.GetPixel.restype = w.DWORD
u.GetDC.argtypes = [w.HWND]; u.GetDC.restype = w.HDC
u.ReleaseDC.argtypes = [w.HWND,w.HDC]

def empty_surface(panel, samples, label):
    """Sample only our fixture on the composed desktop; PrintWindow misses alpha defects."""
    empty = u.GetDlgItem(panel,154); assert u.IsWindowVisible(empty)
    n.activate(panel); dwm.DwmFlush(); time.sleep(.07)
    rect = e.rect(empty); x = rect.left+8; y = (rect.top+rect.bottom)//2
    dc = u.GetDC(None)
    try:
        def rgb(value): return (value&255,(value>>8)&255,(value>>16)&255)
        inside = rgb(gdi.GetPixel(dc,x,y)); outside = rgb(gdi.GetPixel(dc,x,rect.top-3))
    finally: u.ReleaseDC(None,dc)
    samples.append({'state':label,'inside':inside,'outside':outside})
    assert max(abs(a-b) for a,b in zip(inside,outside)) <= 12, f'{label}: empty label painted a contrasting bar: {inside} vs {outside}'
    assert max(inside) < 65, f'{label}: empty label lost the dark surface: {inside}'


def run():
    out = n.ROOT/'tests/output/material'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    results = []
    for solid in [False, True]:
        mode = 'solid' if solid else 'system'
        cfg = out/(mode+'.ini')
        cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nrecipes=0\n', encoding='utf-16')
        args = [str(n.ROOT/'build/Jump.exe'), '--config', str(cfg), '--test-ui', '--test-host-only']
        if solid:
            args.append('--test-solid-material')
        app = subprocess.Popen(args, creationflags=subprocess.CREATE_NO_WINDOW)
        host = None
        try:
            n.wait(lambda: n.windows(app.pid, 'Jump.Control'))
            host = subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(out), '--name', 'Glass_测试.txt'], creationflags=subprocess.CREATE_NO_WINDOW)
            n._fixture_hosts.append(host)
            dialog = n.wait(lambda: next(iter(n.windows(host.pid, '#32770', True)), None))
            n.activate(dialog)
            panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
            def filled(value):
                n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == value+'.txt' and n.panel_ready(panel) and
                       any(n.text(h, u.GetClassNameW) == 'Edit' and n.text(h) == value for h in n.children(dialog)), label='native material fill: '+value)
            filled('Glass_测试_v001')
            active = bool(u.GetPropW(panel, 'Jump.Material'))
            backdrop = w.DWORD()
            supported = dwm.DwmGetWindowAttribute(panel, 38, c.byref(backdrop), c.sizeof(backdrop)) == 0
            assert not u.GetWindowLongW(panel, -20) & 0x80000, 'global window opacity would also fade text'
            if solid:
                assert not active and (not supported or backdrop.value == 1)
            if active:
                assert supported and backdrop.value == 3, 'must use the compositor transient acrylic surface'
            empty_samples = []
            n.click(panel,ident=127)
            empty_surface(panel,empty_samples,'recent-empty')
            empty = u.GetDlgItem(panel,154)
            # Native STATIC can draw through WM_SETTEXT/WM_ENABLE without WM_PAINT.
            text = c.create_unicode_buffer('暂无最近保存记录')
            u.SendMessageW(empty,0xC,0,c.addressof(text))
            empty_surface(panel,empty_samples,'recent-text-refresh')
            u.EnableWindow.argtypes=[w.HWND,w.BOOL]
            u.EnableWindow(empty,False);u.EnableWindow(empty,True)
            empty_surface(panel,empty_samples,'recent-reenabled')
            n.click(panel,ident=122);n.click(panel,ident=129)
            empty_surface(panel,empty_samples,'history-cleared')
            n.click(panel,ident=121)
            empty_surface(panel,empty_samples,'favorites-empty')
            e.capture(panel,out/(mode+'-empty.png'))
            assert not any(u.GetDlgItem(panel, i) for i in [106, 107])
            assert all(u.GetDlgItem(panel,i) for i in [700,701])
            n.click(panel, ident=831)
            editor = n.wait(lambda: u.GetDlgItem(panel, 811))
            e.set_field(panel, 811, '玻璃测试')
            filled('玻璃测试_v001')
            if active:
                alpha, flags, key = c.c_ubyte(), w.DWORD(), w.DWORD()
                u.GetLayeredWindowAttributes.argtypes = [w.HWND, c.POINTER(w.DWORD), c.POINTER(c.c_ubyte), c.POINTER(w.DWORD)]
                assert u.GetLayeredWindowAttributes(editor, c.byref(key), c.byref(alpha), c.byref(flags)) and alpha.value == 255
            # Redirected native editors must still receive real mouse input.
            hit = e.rect(editor)
            point = w.POINT((hit.left+hit.right)//2, (hit.top+hit.bottom)//2)
            u.WindowFromPoint.argtypes = [w.POINT]; u.WindowFromPoint.restype = w.HWND
            assert u.WindowFromPoint(point) == editor, 'editor must not pass mouse input through its opaque surface'
            u.SendMessageW(editor, 0xB1, 0, -1)
            old = w.POINT(); u.GetCursorPos(c.byref(old))
            u.SetCursorPos(point.x, point.y)
            u.mouse_event(2, 0, 0, 0, 0); u.mouse_event(4, 0, 0, 0, 0)
            u.SetCursorPos(old.x, old.y)
            def selection():
                start, end = w.DWORD(), w.DWORD()
                u.SendMessageW(editor, 0xB0, c.addressof(start), c.addressof(end))
                return start.value, end.value
            n.wait(lambda: selection()[0] == selection()[1], label='real editor mouse click')
            assert u.GetForegroundWindow() == panel
            u.SendMessageW(editor, 0xB1, 1, 3)  # EM_SETSEL
            u.InvalidateRect.argtypes = [w.HWND, c.c_void_p, w.BOOL]
            u.InvalidateRect(editor, None, False)
            u.UpdateWindow(editor)
            first, last = w.DWORD(), w.DWORD()
            u.SendMessageW(editor, 0xB0, c.addressof(first), c.addressof(last))
            assert (first.value, last.value) == (1, 3), 'material repaint must preserve native text selection'
            e.capture(panel, out/(mode+'-editor.png'))
            panel_rect, edit_rect = e.rect(panel), e.rect(editor)
            sample = Image.open(out/(mode+'-editor.png')).getpixel((edit_rect.right-panel_rect.left-4,
                (edit_rect.top+edit_rect.bottom)//2-panel_rect.top))
            assert max(abs(a-b) for a,b in zip(sample, (16,20,27))) <= 3, f'editor surface became translucent: {sample}'
            u.SendMessageW(editor, 0x100, 13, 0)
            n.wait(lambda: not u.GetDlgItem(panel, 811))
            n.click(panel, ident=852)
            filled('玻璃测试_模型_v001')
            e.capture(panel, out/(mode+'-panel.png'))
            # No permanent animation/render loop should survive a hover transition.
            button = u.GetDlgItem(panel, 852)
            u.SendMessageW(button, 0x200, 0, 8 | (8 << 16))
            u.SendMessageW(button, 0x2A3, 0, 0)
            time.sleep(.3)
            paint_before = u.SendMessageW(panel, 0x80C8, 0, 0)
            accessibility_before = u.SendMessageW(panel, 0x80C8, 1, 0)
            before = e.stats(app)
            time.sleep(.7)
            after = e.stats(app)
            cpu = after['cpuSeconds'] - before['cpuSeconds']
            paints = u.SendMessageW(panel, 0x80C8, 0, 0) - paint_before
            accessibility = u.SendMessageW(panel, 0x80C8, 1, 0) - accessibility_before
            # Accessibility clients can issue many WM_GETOBJECT requests while
            # inspecting this fixture. Check our actual rendering loop directly;
            # record total CPU separately, including those external queries.
            assert paints == 0, f'hover animation kept painting after completion: {paints}'
            assert host.poll() is None, 'painting and chip selection must never save'
            n.activate(dialog)
            n.click(dialog, ident=1)
            host.wait(timeout=10)
            assert (out/'玻璃测试_模型_v001.txt').is_file()
            # Use a separate filename for the next backend without overwrite UI.
            (out/'玻璃测试_模型_v001.txt').rename(out/(mode+'-saved.txt'))
            results.append({'mode': mode, 'acrylicActive': active, 'backdrop': backdrop.value if supported else None,
                            'idleCpuSeconds': round(cpu, 4), 'idlePaints': paints,
                            'accessibilityQueries': accessibility, 'nativeSelectionAndSave': True,
                            'emptySurfaceSamples':empty_samples})
        finally:
            if host and host.poll() is None:
                host.terminate(); host.wait(timeout=5)
            for window in n.windows(app.pid, 'Jump.Control'):
                u.PostMessageW(window, 0x10, 0, 0)
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.terminate(); app.wait(timeout=5)
    report = {'passed': results, 'output': str(out)}
    (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    run()
