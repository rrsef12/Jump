"""Native UI, persistence, geometry and idle resource checks against owned fixtures only."""
import configparser
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
from pathlib import Path
from PIL import Image
import native_integration as n

OUT=n.ROOT/'tests'/'output'/'extended'
OUT.mkdir(parents=True,exist_ok=True)
u=n.u
u.SetForegroundWindow.argtypes=[w.HWND]
u.SetFocus.argtypes=[w.HWND]
u.SetWindowTextW.argtypes=[w.HWND,w.LPCWSTR]
u.GetWindowRect.argtypes=[w.HWND,c.POINTER(w.RECT)]

def set_field(window, ident, value):
    if n.text(window,u.GetClassNameW)=='Jump.Panel':
        if ident==101:
            if not u.GetDlgItem(window,896):
                u.SendMessageW(window,0x111,755,0)
            n.wait(lambda:u.GetDlgItem(window,896),label='explicit full-name editor')
            ident=896
        edit=u.GetDlgItem(window,ident)
        u.SendMessageW(edit,0x201,1,8|(8<<16));u.SendMessageW(edit,0x202,0,8|(8<<16))
    buffer=c.create_unicode_buffer(value)
    assert u.SendMessageW(u.GetDlgItem(window,ident),0xC,0,c.addressof(buffer))
    n.wait(lambda:n.text(u.GetDlgItem(window,ident))==value,label='editor value '+str(ident))

def post_click(window,ident):
    u.PostMessageW(window,0x111,ident,u.GetDlgItem(window,ident))

def dismiss(pid):
    message=n.wait(lambda:next(iter(n.windows(pid,'#32770',True)),None),label='owned confirmation')
    button=next(h for h in n.children(message) if n.text(h,u.GetClassNameW)=='Button')
    u.PostMessageW(message,0x111,u.GetDlgCtrlID(button),button)
    n.wait(lambda:not n.windows(pid,'#32770',True),label='confirmation close')

def rect(hwnd):
    r=w.RECT();u.GetWindowRect(hwnd,c.byref(r));return r

def capture(hwnd,path):
    """Print only our own window, without grabbing unrelated screen contents."""
    gd=c.WinDLL('gdi32');u.GetWindowDC.argtypes=[w.HWND];u.GetWindowDC.restype=w.HDC
    gd.CreateCompatibleDC.argtypes=[w.HDC];gd.CreateCompatibleDC.restype=w.HDC
    gd.CreateCompatibleBitmap.argtypes=[w.HDC,c.c_int,c.c_int];gd.CreateCompatibleBitmap.restype=w.HANDLE
    gd.SelectObject.argtypes=[w.HDC,w.HANDLE];gd.SelectObject.restype=w.HANDLE
    gd.DeleteObject.argtypes=[w.HANDLE];gd.DeleteDC.argtypes=[w.HDC]
    u.ReleaseDC.argtypes=[w.HWND,w.HDC];u.PrintWindow.argtypes=[w.HWND,w.HDC,w.UINT]
    class BI(c.Structure):
        _fields_=[('size',w.DWORD),('width',w.LONG),('height',w.LONG),('planes',w.WORD),('bits',w.WORD),('compression',w.DWORD),('imageSize',w.DWORD),('x',w.LONG),('y',w.LONG),('colors',w.DWORD),('important',w.DWORD)]
    gd.GetDIBits.argtypes=[w.HDC,w.HANDLE,w.UINT,w.UINT,c.c_void_p,c.c_void_p,w.UINT]
    r=rect(hwnd);width=r.right-r.left;height=r.bottom-r.top
    dc=u.GetWindowDC(hwnd);mem=gd.CreateCompatibleDC(dc);bitmap=gd.CreateCompatibleBitmap(dc,width,height)
    old=gd.SelectObject(mem,bitmap);assert u.PrintWindow(hwnd,mem,2)
    gd.SelectObject(mem,old)
    info=BI(c.sizeof(BI),width,-height,1,32,0,0,0,0,0,0)
    bits=c.create_string_buffer(width*height*4)
    assert gd.GetDIBits(mem,bitmap,0,height,bits,c.byref(info),0)==height
    Image.frombytes('RGB',(width,height),bits.raw,'raw','BGRX').save(path)
    gd.DeleteObject(bitmap);gd.DeleteDC(mem);u.ReleaseDC(hwnd,dc)

def stats(process):
    kernel=n.k;ps=c.WinDLL('psapi')
    kernel.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];kernel.OpenProcess.restype=w.HANDLE
    kernel.GetProcessTimes.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_void_p,c.c_void_p]
    kernel.GetProcessIoCounters.argtypes=[w.HANDLE,c.c_void_p];kernel.CloseHandle.argtypes=[w.HANDLE]
    class Memory(c.Structure):
        _fields_=[('size',w.DWORD),('faults',w.DWORD)]+[(name,c.c_size_t) for name in ['peakWorking','working','peakPaged','paged','peakNonpaged','nonpaged','pagefile','peakPagefile','private']]
    ps.GetProcessMemoryInfo.argtypes=[w.HANDLE,c.c_void_p,w.DWORD]
    h=kernel.OpenProcess(0x410,False,process.pid)
    times=[c.c_ulonglong() for _ in range(4)]
    assert kernel.GetProcessTimes(h,*[c.byref(x) for x in times])
    memory=Memory();memory.size=c.sizeof(memory);assert ps.GetProcessMemoryInfo(h,c.byref(memory),memory.size)
    io=(c.c_ulonglong*6)();assert kernel.GetProcessIoCounters(h,c.byref(io))
    kernel.CloseHandle(h)
    return {'cpuSeconds':(times[2].value+times[3].value)/1e7,'workingSetMiB':round(memory.working/1048576,2),'privateMiB':round(memory.private/1048576,2),'ioOperations':sum(io[:3])}

def run():
    cfg=OUT/'settings.ini';n.write_config(cfg)
    # One fresh configuration, with bottom placement, separate from the portable release.
    data=cfg.read_text(encoding='utf-16').replace('schema=1','schema=1\r\nlayout=2')
    cfg.write_text(data,encoding='utf-16')
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
    host=None;checks=[]
    try:
        control=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Control')),None))
        host,dialog,_=n.launch_host('modern','md')
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None))
        n.wait(lambda:n.text(u.GetDlgItem(panel,101))=='原文件_v001.md')
        n.click(panel,ident=852);n.wait(lambda:n.panel_ready(panel))
        n.click(panel,ident=864);n.wait(lambda:n.text(u.GetDlgItem(panel,101))=='原文件_模型_工作_v001.md' and n.panel_ready(panel))
        n.click(panel,ident=865);n.wait(lambda:n.text(u.GetDlgItem(panel,101))=='原文件_模型_审核_v001.md' and n.panel_ready(panel))
        checks.append('stage changes replace the previous stage')
        n.click(panel,ident=107);n.wait(lambda:n.text(u.GetDlgItem(panel,101))=='原文件.md' and n.panel_ready(panel))
        checks.append('undo restores the original stem and directory')
        time.sleep(.7)
        r=rect(panel);assert r.right-r.left>=344 and r.bottom-r.top<1000
        capture(panel,OUT/'assistant-bottom.png');capture(dialog,OUT/'native-save-dialog.png')
        checks.append('bottom layout fits the owned save dialog')
        n.click(panel,ident=112)
        manager=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None))
        assert u.GetDlgItem(manager,210) and not u.GetDlgItem(manager,201)
        assert u.SendMessageW(u.GetDlgItem(manager,200),0x1304,0,0)==9
        capture(manager,OUT/'scheme-manager.png')
        u.PostMessageW(manager,0x10,0,0);n.wait(lambda:not u.IsWindow(manager))
        u.SetForegroundWindow(dialog);n.wait(lambda:n.windows(app.pid,'Jump.Panel',True))
        checks.append('unified settings center opens and closes without changing the active filename')
        u.SendMessageW(panel,0x10,0,0);n.wait(lambda:not n.windows(app.pid,'Jump.Panel',True))
        u.SetForegroundWindow(dialog);time.sleep(.3);assert not n.windows(app.pid,'Jump.Panel',True)
        checks.append('closing the helper suppresses it for that dialog')
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        time.sleep(1);a=stats(app);started=time.monotonic();time.sleep(5);b=stats(app)
        idle={'sampleSeconds':round(time.monotonic()-started,2),'cpuSeconds':round(b['cpuSeconds']-a['cpuSeconds'],6),'workingSetMiB':b['workingSetMiB'],'privateMiB':b['privateMiB'],'ioOperations':b['ioOperations']-a['ioOperations']}
        assert idle['cpuSeconds']<.1 and idle['ioOperations']==0,idle
        checks.append('idle after dialog use has no measured file I/O')
        report={'passed':checks,'idle':idle}
        (OUT/'extended-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        for panel in n.windows(app.pid,'Jump.Panel'):
            print('DIAGNOSTICS',{'panel':n.text(panel),'preview':n.text(u.GetDlgItem(panel,101)),'foregroundClass':n.text(u.GetForegroundWindow(),u.GetClassNameW),'controls':[(u.GetDlgCtrlID(h),n.text(h)) for h in n.children(panel)]},flush=True)
            capture(panel,OUT/'failure.png')
        raise
    finally:
        if host and host.poll() is None:
            for dialog in n.windows(host.pid,'#32770',True):u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2))
            try:host.wait(timeout=3)
            except subprocess.TimeoutExpired:host.terminate()
        for window in n.windows(app.pid,'Jump.Control'):u.PostMessageW(window,0x10,0,0)
        try:app.wait(timeout=3)
        except subprocess.TimeoutExpired:app.terminate()

if __name__=='__main__':run()
