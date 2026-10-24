"""Check actual button pixels and submission while an owned dialog is being filled."""
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import subprocess
import time
import uuid
import native_integration as n
import extended_integration as e

u=n.u
gdi=c.WinDLL('gdi32')
gdi.GetPixel.argtypes=[w.HDC,c.c_int,c.c_int];gdi.GetPixel.restype=w.DWORD
u.GetDC.argtypes=[w.HWND];u.GetDC.restype=w.HDC
u.ReleaseDC.argtypes=[w.HWND,w.HDC]

def pixel(hwnd):
    dc=u.GetDC(hwnd)
    try:return gdi.GetPixel(dc,12,12)
    finally:u.ReleaseDC(hwnd,dc)

def run():
    out=n.ROOT/'tests/output/fill-paint'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    cfg=out/'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nrecipes=0\n',encoding='utf-16')
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only',
                          '--test-fill-delay-ms','100'],creationflags=subprocess.CREATE_NO_WINDOW)
    hosts=[];checks=[]
    def command(panel,ident):u.SendMessageW(panel,0x111,ident,u.GetDlgItem(panel,ident) or 0)
    def launch():
        host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(out)],creationflags=subprocess.CREATE_NO_WINDOW)
        hosts.append(host)
        dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None));n.activate(dialog)
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None));n.wait(lambda:n.panel_ready(panel))
        n.activate(panel);command(panel,120);command(panel,784)
        n.wait(lambda:u.GetDlgItem(panel,800) and n.text(u.GetDlgItem(panel,800))=='工作')
        time.sleep(.2)
        return host,dialog,panel
    try:
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        host,dialog,panel=launch()
        assert not any(u.GetDlgItem(panel,i) for i in [106,107])
        assert all(u.GetDlgItem(panel,i) for i in [700,701])
        buttons={ident:u.GetDlgItem(panel,ident) for ident in [120]}
        baseline={ident:pixel(hwnd) for ident,hwnd in buttons.items()}
        samples=[]
        for choice in [800,801,802]:
            n.wait(lambda:n.panel_ready(panel));command(panel,choice)
            for i in range(80):
                samples.append({ident:(bool(u.IsWindowEnabled(hwnd)),pixel(hwnd)) for ident,hwnd in buttons.items()})
                time.sleep(.01)
        assert all(row[i][0] for row in samples for i in buttons),'Recommendation disabled during ordinary fill'
        assert all(row[i][1]==baseline[i] for row in samples for i in buttons),'Recommendation color changed during fill'
        checks.append('240 actual screen samples: Recommendation stays enabled with unchanged color across three fills; no footer buttons')
        n.wait(lambda:n.panel_ready(panel));e.capture(panel,out/'stable-buttons.png')
        command(panel,800)
        n.wait(lambda:not n.panel_ready(panel),label='background fill in progress')
        command(panel,106)
        host.wait(timeout=10)
        assert (out/'原文件_工作_v001.txt').is_file()
        checks.append('Save during fill submits once after the fill completes')
        host,dialog,panel=launch()
        command(panel,801);n.wait(lambda:not n.panel_ready(panel),label='fill before cancelled Save')
        command(panel,106)
        command(panel,802)
        n.wait(lambda:n.panel_ready(panel) and n.text(u.GetDlgItem(panel,101))=='原文件_交付_v001.txt')
        time.sleep(1)
        assert host.poll() is None and not (out/'原文件_审核_v001.txt').exists() and not (out/'原文件_交付_v001.txt').exists()
        checks.append('editing after queued Save cancels submission and leaves the dialog open')
        result={'passed':checks,'samples':len(samples),'output':str(out)}
        (out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(result,ensure_ascii=False,indent=2))
    except Exception:
        print('Failure output: '+str(out),flush=True)
        if 'samples' in locals():(out/'samples.json').write_text(json.dumps({'baseline':baseline,'samples':samples}),encoding='utf-8')
        raise
    finally:
        for host in hosts:
            if host.poll() is None:host.terminate();host.wait(timeout=5)
        for hwnd in n.windows(app.pid,'Jump.Control'):u.PostMessageW(hwnd,0x10,0,0)
        try:app.wait(timeout=5)
        except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)

if __name__=='__main__':run()
