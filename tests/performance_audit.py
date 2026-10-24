"""Bounded response/resource audit of Jump and its owned file-dialog host."""
import configparser
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import math
import os
import statistics
import subprocess
import time
import uuid

import native_integration as n
import extended_integration as e
from naming_fixture import pack, encode

u=n.u
u.SendMessageTimeoutW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM,w.UINT,w.UINT,c.POINTER(c.c_size_t)]
u.SendMessageTimeoutW.restype=w.LPARAM
u.RedrawWindow.argtypes=[w.HWND,c.c_void_p,w.HANDLE,w.UINT]
n.k.GetProcessHandleCount.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]
u.GetGuiResources.argtypes=[w.HANDLE,w.DWORD]
u.GetGuiResources.restype=w.DWORD

def wait(predicate, timeout=12):
    end=time.perf_counter()+timeout
    while time.perf_counter()<end:
        result=predicate()
        if result:return result
        time.sleep(.002)
    raise AssertionError('Audit state timeout')

def ping(hwnd):
    result=c.c_size_t()
    assert u.SendMessageTimeoutW(hwnd,0,0,0,2,3000,c.byref(result)), 'UI unresponsive for 3s'

def painted(hwnd):
    # Include processing one paint of this owned window; DWM/monitor presentation
    # and OS popup animations are outside this measurement.
    assert u.RedrawWindow(hwnd,None,None,0x100|0x80|1)
    ping(hwnd)

def command(hwnd,ident):
    u.PostMessageW(hwnd,0x111,ident,u.GetDlgItem(hwnd,ident) or 0)

def sync(hwnd,ident):
    u.SendMessageW(hwnd,0x111,ident,u.GetDlgItem(hwnd,ident) or 0)

def metrics(app):
    result=e.stats(app)
    handle=n.k.OpenProcess(0x410,False,app.pid)
    try:
        handles=w.DWORD();assert n.k.GetProcessHandleCount(handle,c.byref(handles))
        result.update(handles=handles.value,gdiObjects=u.GetGuiResources(handle,0),userObjects=u.GetGuiResources(handle,1))
    finally:n.k.CloseHandle(handle)
    return result

def summary(samples):
    ordered=sorted(samples)
    return {'n':len(samples),'medianMs':round(statistics.median(samples),2),
            'p95Ms':round(ordered[math.ceil(.95*len(ordered))-1],2),
            'maxMs':round(max(samples),2),'samplesMs':[round(x,2) for x in samples]}

def stress_config(path):
    cfg=configparser.ConfigParser()
    cfg['Jump']={'schema':'1','Favorite':'0','History':'0','recipes':'100','projects':'100',
                 'automatic':'1','modifiers':'-1','activeNamingScheme':encode('asset-v2')}
    for i in range(100):
        fields=[]
        for j in range(8):
            ident,label=[('project','项目名称'),('subject','主要名称'),('type','文件类别'),
                         ('variant','补充说明'),('stage','当前状态'),('date','日期'),
                         ('batch','批次'),('output','输出')][j]
            source='1' if j==0 else '0'
            meta=pack(['','','','','','',str(int(j in (1,2,4))),'0','0','0','0',source])
            options=[pack(['Option '+str(k),'Option'+str(k),'']) for k in range(20)] if j in (2,3,4,6,7) else []
            fields.append(pack([ident,label,'',str(int(j==1)),source,meta,*options]))
        definition=pack(['JNS2','audit-'+str(i),'压力方案 '+str(i),'_','1','0','0','0','3','0','0',pack(['subject','0','']),*fields])
        cfg['Recipe'+str(i)]={'name':encode('压力方案 '+str(i)),'value':encode('Asset'),
                            'complete':'1','namingOnly':'1','structure':encode(definition)}
        cfg['Project'+str(i)]={key:encode(value) for key,value in
                             {'id':'audit-'+str(i),'name':'Project '+str(i),'root':str(path.parent),
                              'work':'work','delivery':'delivery'}.items()}
    with path.open('w',encoding='utf-16') as file:cfg.write(file)

def run(selected=None,cycles=20):
    out=n.ROOT/'tests/output/performance-audit'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    original=n.ROOT/'build/dev/settings.ini';original_hash=hashlib.sha256(original.read_bytes()).hexdigest()
    report={'build':'existing Debug /Od','executableSha256':hashlib.sha256((n.ROOT/'build/Jump.exe').read_bytes()).hexdigest(),
            'pollIntervalMs':2,'logicalProcessors':os.cpu_count(),'repeatCycles':cycles,
            'method':'Command to controls ready + synchronous owned-window paint; native fill separately includes actual filename readback. Cached Windows launch, not cold boot; no DWM presentation timing.',
            'scenarios':{},'output':str(out)}
    for mode in (selected,) if selected else ('current-copy','stress-100'):
        folder=out/mode;folder.mkdir();cfg=folder/'settings.ini'
        if mode=='current-copy':cfg.write_bytes(original.read_bytes())
        else:stress_config(cfg)
        parsed=configparser.ConfigParser();parsed.read(cfg,encoding='utf-16')
        parsed['Jump']['automatic']='1';parsed['Jump']['activeNamingScheme']=encode('asset-v2')
        parsed['Jump']['defaultNamingScheme']=encode('asset-v2');parsed['Jump']['applicationNamingSchemes']='0'
        with cfg.open('w',encoding='utf-16') as file:parsed.write(file)
        result={'configBytes':cfg.stat().st_size,'recipes':parsed.getint('Jump','recipes',fallback=0),
                'projects':parsed.getint('Jump','projects',fallback=0),'timings':{},'resource':{}}
        report['scenarios'][mode]=result
        samples={};breakdowns={};app=host=panel=dialog=manager=None
        def timed(name,action,ready=lambda:True,paint=None):
            started=time.perf_counter();value=action();returned=time.perf_counter();wait(ready);state_ready=time.perf_counter()
            if paint:painted(paint() if callable(paint) else paint)
            ended=time.perf_counter();elapsed=(ended-started)*1000
            samples.setdefault(name,[]).append(elapsed)
            breakdowns.setdefault(name,[]).append({'commandMs':round((returned-started)*1000,2),
                                                  'stateWaitMs':round((state_ready-returned)*1000,2),
                                                  'paintMs':round((ended-state_ready)*1000,2)})
            if elapsed>250:print(mode,name,round(elapsed,2),'ms',flush=True)
            return value
        def forms(title):return next(iter(n.dialogs(app.pid,title)),None)
        def open_manager():
            nonlocal manager
            n.activate(dialog)
            timed('settings.open',lambda:command(panel,112),lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None),
                  lambda:next(iter(n.windows(app.pid,'Jump.Manager',True))))
            manager=next(iter(n.windows(app.pid,'Jump.Manager',True)));n.activate(manager)
        def close_manager():
            u.PostMessageW(manager,0x10,0,0)
            wait(lambda:not u.IsWindow(manager));n.activate(dialog);wait(lambda:u.IsWindowVisible(panel));ping(panel)
        def section(index):
            control=u.GetDlgItem(manager,200);dpi=u.GetDpiForWindow(manager)
            point=60*dpi//96|((16*dpi//96+index*(32*dpi//96+3))<<16)
            def click():u.SendMessageW(control,0x201,1,point);u.SendMessageW(control,0x202,0,point)
            timed('section.'+str(index),click,lambda:u.SendMessageW(control,0x130B,0,0)==index,manager)
        def open_form(ident,title,name):
            timed(name,lambda:command(manager,ident),lambda:forms(title),manager)
            return forms(title)
        def close_form(form):
            command(form,2);wait(lambda:not u.IsWindow(form));ping(manager)
        def idle(name,seconds=3):
            before=metrics(app);start=time.perf_counter();delays=[]
            while time.perf_counter()-start<seconds:
                now=time.perf_counter();ping(n.windows(app.pid,'Jump.Control')[0]);delays.append((time.perf_counter()-now)*1000);time.sleep(.1)
            elapsed=time.perf_counter()-start;after=metrics(app)
            result['resource'][name]={'seconds':round(elapsed,2),'cpuOneCorePercent':round((after['cpuSeconds']-before['cpuSeconds'])/elapsed*100,2),
                                      'ioOperationsDelta':after['ioOperations']-before['ioOperations'],
                                      'uiPing':summary(delays),'after':after}
        def filled():
            preview=n.text(u.GetDlgItem(panel,101))
            return preview.endswith('.txt') and n.panel_ready(panel) and any(n.text(h,u.GetClassNameW)=='Edit' and n.text(h)==preview[:-4] for h in n.children(dialog))
        def native_click(ident,name):
            n.activate(dialog);wait(lambda:n.panel_ready(panel));old=n.text(u.GetDlgItem(panel,101))
            button=wait(lambda:u.GetDlgItem(panel,ident));assert u.IsWindowEnabled(button)
            rect=e.rect(button);previous=w.POINT();u.GetCursorPos(c.byref(previous))
            u.SetCursorPos((rect.left+rect.right)//2,(rect.top+rect.bottom)//2)
            def click():u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0)
            try:timed(name,click,lambda:n.text(u.GetDlgItem(panel,101))!=old and filled())
            finally:u.SetCursorPos(previous.x,previous.y)
        try:
            print('Starting',mode,flush=True)
            for launch in range(3):
                started=time.perf_counter()
                app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
                wait(lambda:n.windows(app.pid,'Jump.Control'));ping(n.windows(app.pid,'Jump.Control')[0])
                samples.setdefault('process.start',[]).append((time.perf_counter()-started)*1000)
                if launch<2:
                    u.PostMessageW(n.windows(app.pid,'Jump.Control')[0],0x10,0,0);app.wait(timeout=5)
            idle('hiddenIdle')
            started=time.perf_counter()
            host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(folder),'--name','Audit.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
            n._fixture_hosts.append(host);dialog=wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None))
            result['hostDialogLaunchMs']=round((time.perf_counter()-started)*1000,2);n.activate(dialog);started=time.perf_counter()
            panel=wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None));wait(filled);painted(panel)
            samples['saveDialog.attachAndFill']=[(time.perf_counter()-started)*1000]
            idle('panelIdle')
            for ident in (852,853,852,853,864,865,864,865):native_click(ident,'candidate.nativeFill')
            for i in range(5):
                n.activate(dialog);wait(lambda:n.panel_ready(panel))
                timed('undo.nativeFill',lambda:sync(panel,700),filled)
                timed('redo.nativeFill',lambda:sync(panel,701),filled)
            # Editor appears in place, then debounce + actual native readback.
            for i in range(4):
                n.activate(dialog);wait(lambda:n.panel_ready(panel))
                timed('nameEditor.open',lambda:sync(panel,831),lambda:u.GetDlgItem(panel,811),panel)
                control=u.GetDlgItem(panel,811);buffer=c.create_unicode_buffer('AuditName'+str(i))
                old=n.text(u.GetDlgItem(panel,101))
                timed('nameInput.nativeFill',lambda:u.SendMessageW(control,0xC,0,c.addressof(buffer)),
                      lambda:n.text(u.GetDlgItem(panel,101))!=old and filled())
                timed('nameEditor.confirmAndPersist',lambda:sync(panel,888),lambda:not u.GetDlgItem(panel,811),panel)
            for ident in (789,788)*4:
                n.activate(dialog);timed('fieldsView.switch',lambda:sync(panel,ident),paint=panel)
            for ident,name in ((790,'menu.naming'),(889,'menu.number'),(821,'menu.recentNames'),(822,'menu.fileCategory')):
                for _ in range(3):
                    n.activate(dialog);wait(lambda:n.panel_ready(panel))
                    n.activate(panel);time.sleep(.2)
                    pointer=w.POINT();u.GetCursorPos(c.byref(pointer));r=e.rect(panel);u.SetCursorPos(r.left+2,r.top+2)
                    started=time.perf_counter();command(panel,ident)
                    popup=wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
                    samples.setdefault(name,[]).append((time.perf_counter()-started)*1000)
                    u.PostMessageW(popup,0x100,0x1b,1)
                    wait(lambda:not n.windows(app.pid,'#32768',True));ping(panel)
                    u.SetCursorPos(pointer.x,pointer.y)
            for ident,name in ((142,'affix.expandCollapse'),(116,'replacement.expandCollapse')):
                for _ in range(4):
                    n.activate(dialog);timed(name,lambda:sync(panel,ident),paint=panel)
            open_manager()
            for _ in range(3):
                for index in range(9):section(index)
            idle('settingsIdle');section(0)
            for _ in range(3):
                form=open_form(207,'命名方案','schemeEditor.open')
                timed('rules.expand',lambda:sync(form,4100),lambda:u.IsWindowVisible(u.GetDlgItem(form,4004)),manager)
                for ident in (4021,4022,4020,4023,4023):timed('rules.tabSwitch',lambda:sync(form,ident),paint=manager)
                timed('choicesEditor.open',lambda:command(form,4080),lambda:next((h for h in n.dialogs(app.pid) if ' · 常用选项' in n.text(h)),None),manager)
                nested=next(h for h in n.dialogs(app.pid) if ' · 常用选项' in n.text(h));close_form(nested);close_form(form)
            section(6)
            # At the 100-project limit, use the same form via editing instead of
            # triggering the expected capacity notice from the New button.
            project_command,project_title=(210,'新建项目') if mode=='current-copy' else (207,'编辑项目')
            for _ in range(3):form=open_form(project_command,project_title,'projectEditor.open');close_form(form)
            section(2)
            # This page captures a workflow directly; it has no separate editor.
            if mode=='current-copy':
                for _ in range(3):timed('workflow.captureAndPersist',lambda:sync(manager,228),paint=manager)
            section(7)
            for _ in range(3):
                timed('importPicker.open',lambda:command(manager,286),lambda:next(iter(n.windows(app.pid,'#32770',True)),None))
                picker=next(iter(n.windows(app.pid,'#32770',True)));close_form(picker)
            section(8)
            for _ in range(3):timed('diagnostics.refresh',lambda:sync(manager,207),paint=manager)
            section(3)
            for _ in range(3):timed('settings.save',lambda:sync(manager,223),paint=manager)
            section(0);form=open_form(207,'命名方案','schemeEditor.open');close_form(form)
            result['resource']['beforeRepeatedWindows']=metrics(app)
            for iteration in range(cycles):
                section(3);section(0);form=open_form(207,'命名方案','schemeEditor.open');close_form(form)
                if iteration%5==4:result['resource']['afterCycles'+str(iteration+1)]=metrics(app)
            idle('afterRepeatedIdle',2)
            close_manager()
            for _ in range(3):open_manager();close_manager()
            result['resource']['final']=metrics(app)
        except Exception as error:
            result['error']=repr(error);raise
        finally:
            result['timings']={name:summary(values) for name,values in samples.items() if values}
            result['timingBreakdown']=breakdowns
            if host and host.poll() is None:host.terminate();host.wait(timeout=5)
            if app and app.poll() is None:
                for control in n.windows(app.pid,'Jump.Control'):u.PostMessageW(control,0x10,0,0)
                try:app.wait(timeout=5)
                except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)
            assert hashlib.sha256(original.read_bytes()).hexdigest()==original_hash,'Development configuration changed'
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps({'scenario':mode,'timings':result['timings'],'resource':result['resource'],'output':str(out)},ensure_ascii=False),flush=True)

if __name__=='__main__':
    args=argparse.ArgumentParser(description=__doc__)
    args.add_argument('--scenario',choices=['current-copy','stress-100'])
    args.add_argument('--cycles',type=int,choices=range(0,21),default=20,metavar='0-20')
    options=args.parse_args();run(options.scenario,options.cycles)
