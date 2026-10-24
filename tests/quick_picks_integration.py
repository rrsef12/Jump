"""One-click naming and zero-input version continuation on owned save dialogs."""
import configparser
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import subprocess
import time
import uuid
import native_integration as n
import extended_integration as e
import naming_menu as menu
from fill_paint_integration import pixel

u=n.u

def run(rapid_only=False,native_history_only=False):
    out=n.ROOT/'tests/output/quick-picks'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    cfg=out/'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nrecipes=0\n',encoding='utf-16')
    if native_history_only:
        from naming_fixture import encode
        stored=configparser.ConfigParser();stored.read(cfg,encoding='utf-16')
        legacy=[('旧版原生名称',''),('其他项目名称','other-project')]+[('更早名称'+str(i),'') for i in range(9)]
        stored['Jump']['savedFiles']=str(len(legacy))
        for i,(value,project) in enumerate(legacy):
            stored['SavedFile'+str(i)]={'path':encode(str(out/(value+'.txt'))),
                'process':encode('Jump.TestHost.exe'),'projectId':encode(project)}
        with cfg.open('w',encoding='utf-16') as file:stored.write(file)
    args=[str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only']
    if rapid_only:args+=['--test-fill-delay-ms','300']
    app=subprocess.Popen(args,creationflags=subprocess.CREATE_NO_WINDOW)
    hosts=[];checks=[];fill_latency=[]
    def command(ident):
        n.activate(panel)
        u.SendMessageW(panel,0x111,ident,u.GetDlgItem(panel,ident) or 0)
    def set_text(ident,value):
        n.activate(panel)
        target=n.wait(lambda:u.GetDlgItem(panel,ident),label='editor '+str(ident))
        buffer=c.create_unicode_buffer(value);assert u.SendMessageW(target,0xC,0,c.addressof(buffer))
    def native_name():return next(n.text(h) for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit')
    def filled(value):n.wait(lambda:n.text(u.GetDlgItem(panel,101))==value+'.txt' and native_name()==value and n.panel_ready(panel),label='preview and native filename: '+value)
    def mouse_pick(ident,activate_dialog=True):
        if activate_dialog:n.activate(dialog)
        button=n.wait(lambda:u.GetDlgItem(panel,ident) if n.panel_ready(panel) else None,label='clickable control '+str(ident))
        assert u.IsWindowEnabled(button)
        rect=w.RECT();u.GetWindowRect(button,c.byref(rect))
        old=w.POINT();u.GetCursorPos(c.byref(old))
        u.SetCursorPos((rect.left+rect.right)//2,(rect.top+rect.bottom)//2)
        u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0);time.sleep(.04);u.SetCursorPos(old.x,old.y)
    def outside_click():
        n.activate(panel)
        rect=e.rect(panel);old=w.POINT();u.GetCursorPos(c.byref(old))
        u.SetCursorPos(rect.left+4,rect.top+40)
        u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0);time.sleep(.04);u.SetCursorPos(old.x,old.y)
    def timed_pick(ident,value):
        started=time.perf_counter();mouse_pick(ident);filled(value)
        fill_latency.append(round((time.perf_counter()-started)*1000,1))
    def no_inputs():return bool(u.GetDlgItem(panel,831) and u.GetDlgItem(panel,101) and u.GetDlgItem(panel,892) and n.panel_ready(panel)) and not any(u.GetDlgItem(panel,810+i) for i in range(8)) and not u.GetDlgItem(panel,760) and not any(u.GetDlgItem(panel,i) for i in [106,107]) and all(u.GetDlgItem(panel,i) for i in [700,701])
    def error_shown():
        hints=[u.GetDlgItem(panel,i) for i in [114,*range(900,912)]]
        assert any(h and u.IsWindowVisible(h) and n.text(h) for h in hints)
        assert not u.GetDlgItem(panel,106)
    def native_save():
        n.wait(lambda:n.panel_ready(panel))
        n.activate(dialog);n.click(dialog,ident=1)
    def native_set(value):
        n.activate(dialog)
        field=next(h for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit')
        buffer=c.create_unicode_buffer(value);u.SendMessageW(field,0xC,0,c.addressof(buffer))
        filled(value)
        n.wait(no_inputs,label='quick naming remains visible after native filename edit')
        assert u.GetForegroundWindow()==dialog, 'synchronizing a native edit must preserve its foreground window'
    def choose(ident,caption,exclude=(),disabled=()):
        if ident in [750,765]:
            group='撤销与还原' if caption in ['撤销命名组合','重做命名组合','还原本次名称与位置'] else '本次命名'
            menu.select(panel,app.pid,(group,caption),exclude=exclude,disabled=disabled)
            if caption=='直接修改完整文件名':n.wait(lambda:'整名编辑' in n.text(u.GetDlgItem(panel,792)))
            return
        n.wait(lambda:n.panel_ready(panel));n.activate(panel)
        old=w.POINT();u.GetCursorPos(c.byref(old));r=e.rect(panel);u.SetCursorPos(r.left+4,r.top+4)
        u.PostMessageW(panel,0x111,ident,0)
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None),label='owned menu')
        time.sleep(.25)  # Wait for the native popup animation before reading item rectangles.
        handle=u.SendMessageW(popup,0x1E1,0,0)
        u.GetMenuItemCount.argtypes=[c.c_void_p]
        u.GetMenuStringW.argtypes=[c.c_void_p,c.c_uint,c.c_wchar_p,c.c_int,c.c_uint]
        labels=[]
        for i in range(u.GetMenuItemCount(handle)):
            buffer=c.create_unicode_buffer(256);u.GetMenuStringW(handle,i,buffer,len(buffer),0x400);labels.append(buffer.value)
        assert not any(value in labels for value in exclude),labels
        u.GetMenuState.argtypes=[c.c_void_p,c.c_uint,c.c_uint]
        for value in disabled:assert u.GetMenuState(handle,labels.index(value),0x400)&3,labels
        menu.select_item(popup,handle,labels.index(caption))
        n.wait(lambda:not n.windows(app.pid,'#32768',True));u.SetCursorPos(old.x,old.y)
        if caption=='直接修改完整文件名':
            n.wait(lambda:n.text(u.GetDlgItem(panel,755))=='恢复分项填写',label='full name editor')
    def launch(saved=None,initial=None,open_dialog=False):
        args=[str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(out)]
        if saved:args+=['--source',str(saved),'--name',saved.name]
        elif initial:args+=['--name',initial]
        if open_dialog:args+=['--open']
        host=subprocess.Popen(args,creationflags=subprocess.CREATE_NO_WINDOW)
        hosts.append(host);n._fixture_hosts.append(host)
        dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None));n.activate(dialog)
        def attached():
            n.activate(dialog)
            return next(iter(n.windows(app.pid,'Jump.Panel',True)),None)
        panel=n.wait(attached,label='owned save helper');n.wait(lambda:n.panel_ready(panel))
        return host,dialog,panel
    def recorded(count):
        def ready():
            # Save verification updates the INI asynchronously; retry a read
            # that overlaps the writer instead of treating a partial file as final.
            try:
                p=configparser.ConfigParser();p.read(cfg,encoding='utf-16')
                return p.getint('Jump','savedFiles',fallback=0)>=count
            except (OSError,UnicodeError,ValueError,configparser.Error):return False
        n.wait(ready,label='verified save record')
    try:
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        host,dialog,panel=launch(initial='Rock.txt' if rapid_only else None)
        filled('Rock_v001' if rapid_only else '原文件_v001')
        if native_history_only:
            def subjects():
                parser=configparser.ConfigParser();parser.read(cfg,encoding='utf-16')
                return [bytes.fromhex(parser.get('RecentSubject'+str(i),'value')).decode('utf-16-be')
                        for i in range(parser.getint('Jump','recentSubjects',fallback=0))]
            def cancel():
                n.activate(dialog);u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
            recent=menu.select(panel,app.pid,anchor=821,capture=out/'recent-names-compact.png',dark=True)
            assert '旧版原生名称' in recent and '其他项目名称' not in recent,recent
            assert len(recent)==10 and recent[-2:]==['','管理记录'],recent
            assert not any(item in recent for item in ['最近主要名称','使用项目名称','使用当天日期','自定义…','编辑常用选项…','删除单个名称','清空最近名称']),recent
            checks.append('older verified native basenames are available without an extension; other projects remain isolated')
            for value in ['输入片段一','输入片段二','系统输入_最终']:native_set(value)
            assert subjects()==[],subjects()
            recent=menu.select(panel,app.pid,anchor=821)
            assert recent[0]=='系统输入_最终' and not any(v in recent for v in ['输入片段一','输入片段二','系统输入_最终.txt']),recent
            assert host.poll() is None and not (out/'系统输入_最终.txt').exists()
            checks.append('returning from native filename editing records only the latest complete name and makes it immediately selectable')
            native_set('直接保存_手动_v007');native_save();host.wait(timeout=10)
            assert (out/'直接保存_手动_v007.txt').is_file();recorded(len(legacy)+1)
            n.wait(lambda:'直接保存_手动_v007' in subjects(),label='native Save remembers its literal main name')
            checks.append('native Save records the verified literal main name without requiring a return to Jump')
            for controller in n.windows(app.pid,'Jump.Control'):u.PostMessageW(controller,0x10,0,0)
            app.wait(timeout=5)
            app=subprocess.Popen(args,creationflags=subprocess.CREATE_NO_WINDOW)
            n.wait(lambda:n.windows(app.pid,'Jump.Control'))
            host,dialog,panel=launch(initial='重新检查.txt');filled('重新检查_v001')
            recent=menu.select(panel,app.pid,anchor=821)
            assert recent[:2]==['直接保存_手动_v007','系统输入_最终'],recent
            menu.select(panel,app.pid,'系统输入_最终',anchor=821);filled('系统输入_最终_v001')
            command(852);filled('系统输入_最终_模型_v001');native_save();host.wait(timeout=10);recorded(len(legacy)+2)
            checks.append('native history survives restart and reuses the shared field pipeline; composed saves remember only their main name')
            host,dialog,panel=launch();filled('原文件_v001')
            recent=menu.select(panel,app.pid,anchor=821)
            assert recent[0]=='系统输入_最终' and '系统输入_最终_模型_v001' not in recent,recent
            menu.select(panel,app.pid,('管理记录','删除：旧版原生名称'),anchor=821)
            recent=menu.select(panel,app.pid,anchor=821)
            assert '旧版原生名称' not in recent and '系统输入_最终' in recent,recent
            filled('原文件_v001')
            native_set('只输入后取消');cancel()
            host,dialog,panel=launch();filled('原文件_v001')
            recent=menu.select(panel,app.pid,anchor=821)
            assert '只输入后取消' not in recent and '旧版原生名称' not in recent,recent
            assert not (out/'只输入后取消.txt').exists()
            menu.select(panel,app.pid,('管理记录','清空最近名称'),anchor=821);filled('原文件_v001')
            empty=menu.select(panel,app.pid,anchor=821)
            assert empty==['暂无最近名称'],empty
            assert subjects()==[],subjects()
            cancel()
            for controller in n.windows(app.pid,'Jump.Control'):u.PostMessageW(controller,0x10,0,0)
            app.wait(timeout=5)
            stored=configparser.ConfigParser();stored.read(cfg,encoding='utf-16')
            assert stored.getint('Jump','savedFiles')==len(legacy)+2
            removed=[bytes.fromhex(stored.get('RemovedSubject'+str(i),'value')).decode('utf-16-be')
                     for i in range(stored.getint('Jump','removedSubjects'))]
            assert '更早名称8' in removed and '其他项目名称' not in removed,removed
            app=subprocess.Popen(args,creationflags=subprocess.CREATE_NO_WINDOW)
            n.wait(lambda:n.windows(app.pid,'Jump.Control'))
            host,dialog,panel=launch();filled('原文件_v001')
            empty=menu.select(panel,app.pid,anchor=821)
            assert empty==['暂无最近名称'],empty
            assert (out/'直接保存_手动_v007.txt').is_file() and (out/'系统输入_最终_模型_v001.txt').is_file()
            checks.append('single deletion preserves other names and the current filename; clearing removes all project history beyond eight rows and remains empty after restart without deleting files or other project history')
            native_set('');cancel()
            checks.append('cancelled native edits and empty names do not enter history; removed legacy names stay removed after reopening')
            report={'passed':checks,'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        if rapid_only:
            u.GetPropW.argtypes=[w.HWND,w.LPCWSTR];u.GetPropW.restype=w.HANDLE
            def rapid_click(ident,selected=None):
                button=u.GetDlgItem(panel,ident);assert button and u.IsWindowEnabled(button)
                rect=e.rect(button);u.SetCursorPos((rect.left+rect.right)//2,(rect.top+rect.bottom)//2)
                started=time.perf_counter()
                u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0)
                if selected is not None:
                    try:n.wait(lambda:(u.GetPropW(button,'Jump.Selected')==2)==selected,timeout=.2,label='rapid selection '+str((ident,selected)))
                    except AssertionError:
                        e.capture(panel,out/'rapid-failure.png')
                        print({'click':len(fill_latency)+1,'control':ident,'selected':u.GetPropW(button,'Jump.Selected'),'preview':n.text(u.GetDlgItem(panel,101)),'foreground':n.text(u.GetForegroundWindow()),'button':n.text(button)},flush=True)
                        raise
                fill_latency.append(round((time.perf_counter()-started)*1000,1))
                time.sleep(.055)
            old=w.POINT();u.GetCursorPos(c.byref(old))
            try:
                n.activate(dialog)
                for _ in range(3):
                    rapid_click(852,True);rapid_click(852,False)
                filled('Rock_v001')
                checks.append('six real fast clicks toggle the same chip on every click while a 300 ms native fill is in flight; final native name is cleared')
                rapid_click(852,True);rapid_click(864,True);rapid_click(852,False);rapid_click(864,False)
                filled('Rock_v001')
                checks.append('rapid mixed-field add/remove keeps the last selection and native name, without saving')
                menu.select(panel,app.pid,('本次命名','显示全部字段'))
                command(782);n.wait(lambda:u.GetDlgItem(panel,800) and n.panel_ready(panel))
                rapid_click(800);rapid_click(800)
                filled('Rock_v001')
                assert n.text(u.GetDlgItem(panel,760))==''
                checks.append('the all-fields candidate uses the same rapid repeat-click cancellation')
                menu.select(panel,app.pid,('命名方案','游戏引擎资产'));filled('SM_Rock_A')
                n.activate(dialog);rapid_click(840,False);rapid_click(840,True)
                filled('SM_Rock_A')
                assert host.poll() is None and not list(out.glob('*.txt'))
                checks.append('rapid clicks also cancel and restore a required secondary field in another scheme; naming chips never submit the save')
                e.capture(panel,out/'rapid-final.png')
            finally:u.SetCursorPos(old.x,old.y)
            report={'passed':checks,'clickResponseMs':fill_latency,'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        assert no_inputs()
        assert not u.GetDlgItem(panel,832) and not u.GetDlgItem(panel,834), 'custom input belongs in More'
        assert all(u.GetDlgItem(panel,i) for i in [831,852,853,854,864,865,866])
        assert not any(u.GetDlgItem(panel,i) for i in [855,856,857,867,868,869])
        checks.append('default screen has no naming input boxes; type and stage choices are directly visible')
        assert all(u.GetDlgItem(panel,i) for i in [700,701,792,889,890,891,892,894,895])
        assert not u.GetDlgItem(panel,893)
        assert 'v001' in n.text(u.GetDlgItem(panel,889))
        assert n.text(u.GetDlgItem(panel,890))=='前后缀：关闭'
        assert n.text(u.GetDlgItem(panel,891))=='替换：关闭'
        assert u.IsWindowVisible(u.GetDlgItem(panel,894)) and not any(u.IsWindowVisible(u.GetDlgItem(panel,i)) for i in [890,891])
        assert n.text(u.GetDlgItem(panel,895))=='最终文件名'
        name_rect=e.rect(u.GetDlgItem(panel,831));arrow_rect=e.rect(u.GetDlgItem(panel,821))
        assert name_rect.right==arrow_rect.left and name_rect.top==arrow_rect.top and name_rect.bottom==arrow_rect.bottom
        assert '位置与命名可用' in n.text(u.GetDlgItem(panel,892))
        empty_subjects=menu.select(panel,app.pid,anchor=821,dark=True)
        assert empty_subjects==['暂无最近名称'],empty_subjects
        original_handles={i:u.GetDlgItem(panel,i) for i in [101,831]}
        menu.select(panel,app.pid,anchor=792,checked=['资产文件'],dark=True)
        menu.select(panel,app.pid,anchor=889,checked=['自动版本 vNNN'])
        menu.select(panel,app.pid,anchor=892,exclude=['使用引导…'])
        assert all(u.GetDlgItem(panel,i)==h for i,h in original_handles.items())
        menu.select(panel,app.pid,'关闭自动编号',anchor=889);filled('原文件')
        n.click(panel,ident=700);filled('原文件_v001')
        n.click(panel,ident=701);filled('原文件')
        menu.select(panel,app.pid,'自动版本 vNNN',anchor=889);filled('原文件_v001')
        assert host.poll() is None and not list(out.glob('*.txt'))
        checks.append('direct scheme/number/capability menus cancel without rebuilding; onboarding entry is absent; visible Undo/Redo change numbering without saving')
        assert n.text(u.GetDlgItem(panel,790))=='命名设置 ▾'
        assert not any(u.GetDlgItem(panel,i) for i in [750,765,776,777,788,789,791])
        header=tuple(getattr(e.rect(u.GetDlgItem(panel,790)),edge) for edge in ['left','top','right','bottom'])
        e.capture(panel,out/'unified-quick.png')
        root=menu.select(panel,app.pid,('本次命名','显示全部字段'),
            required=[791,757,3,4,789,755,142,116,779,793,794,795,700,701,107,753,110,702,189])
        assert root==['命名方案','本次命名','撤销与还原','更多工具'],root
        n.wait(lambda:all(u.GetDlgItem(panel,i) for i in [*range(780,786),700,701,101,892]) and n.panel_ready(panel),
               label='all fields and naming actions finished rebuilding')
        assert not any(u.GetDlgItem(panel,i) for i in [106,107])
        assert all(u.GetDlgItem(panel,i) for i in [700,701])
        assert header==tuple(getattr(e.rect(u.GetDlgItem(panel,790)),edge) for edge in ['left','top','right','bottom'])
        filled('原文件_v001')
        menu.select(panel,app.pid,('本次命名','显示全部字段'),checked=[('本次命名','显示全部字段'),('本次命名','自动版本 vNNN')]);n.wait(no_inputs)
        quick_handle=u.GetDlgItem(panel,831);menu.select(panel,app.pid,capture=out/'unified-settings.png',dark=True)
        assert u.GetDlgItem(panel,831)==quick_handle
        checks.append('Naming settings has exactly four categories; all naming commands remain available once, with field and numbering checkmarks; cancelling keeps controls')
        handles={i:u.GetDlgItem(panel,i) for i in [101,120,831,852,853,864,865,866]}
        n.click(panel,ident=852);filled('原文件_模型_v001')
        n.click(panel,ident=864);filled('原文件_模型_工作_v001')
        choose(824,'定稿');filled('原文件_模型_定稿_v001')
        n.click(panel,ident=866);filled('原文件_模型_v001')
        n.click(panel,ident=853);filled('原文件_材质_v001')
        assert no_inputs() and all(u.GetDlgItem(panel,i)==h for i,h in handles.items())
        checks.append('one click applies/replaces a type or stage; repeat click clears optional selection; no menus, editors or control reconstruction')
        choose(822,'角色参考',exclude=['定稿','审核']);filled('原文件_角色参考_v001')
        assert n.text(u.GetDlgItem(panel,854))=='角色参考'
        e.capture(panel,out/'selected-long-choice.png')
        command(854);filled('原文件_v001')
        command(852);filled('原文件_模型_v001')
        checks.append('uncommon choices remain available through More and become a visible toggle without a hidden selection')
        command(831);n.wait(lambda:u.GetDlgItem(panel,811))
        editor=u.GetDlgItem(panel,811)
        mouse_pick(821,False)
        n.wait(lambda:n.windows(app.pid,'#32768',True),label='combined subject arrow opened with the mouse')
        u.PostMessageW(panel,0x1F,0,0);n.wait(lambda:not n.windows(app.pid,'#32768',True))
        assert u.GetDlgItem(panel,811)==editor
        u.SendMessageW(editor,0x100,0x28,0)
        n.wait(lambda:n.windows(app.pid,'#32768',True),label='combined subject keyboard dropdown')
        u.PostMessageW(panel,0x1F,0,0);n.wait(lambda:not n.windows(app.pid,'#32768',True))
        assert u.GetDlgItem(panel,811)==editor
        assert sum(bool(u.GetDlgItem(panel,810+i)) for i in range(8))==1
        set_text(811,'守卫');filled('守卫_模型_v001')
        u.SendMessageW(u.GetDlgItem(panel,811),0x100,13,0);n.wait(no_inputs)
        assert '守卫' in n.text(u.GetDlgItem(panel,831)) and host.poll() is None
        checks.append('only explicit Rename opens one transient editor; typing applies directly and Enter hides it without saving')
        command(831);set_text(811,'守卫');filled('守卫_模型_v001');outside_click();n.wait(no_inputs)
        command(831);editor=u.GetDlgItem(panel,811);mouse_pick(790,False)
        n.wait(lambda:n.windows(app.pid,'#32768',True),label='settings opened by real click')
        u.PostMessageW(panel,0x1F,0,0);n.wait(lambda:not n.windows(app.pid,'#32768',True))
        time.sleep(.1);assert u.GetDlgItem(panel,811)==editor
        n.activate(dialog);n.wait(no_inputs)
        assert u.GetForegroundWindow()==dialog
        command(831);mouse_pick(127,False);n.wait(no_inputs)
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,128)),label='the outside tab click also completes its action')
        command(831);mouse_pick(865,False);filled('守卫_模型_审核_v001');n.wait(no_inputs)
        command(865);filled('守卫_模型_v001')
        command(831);set_text(811,'快速收起');outside_click();n.wait(no_inputs);filled('快速收起_模型_v001')
        command(700);filled('守卫_模型_v001')
        checks.append('blank area, location tab and another field dismiss valid quick input without losing the click; pending live input still fills and undo works')
        command(831);set_text(811,'守卫');filled('守卫_模型_v001')
        menu.select(panel,app.pid,('本次命名','显示全部字段'));n.wait(lambda:u.GetDlgItem(panel,781));command(781)
        assert n.text(u.GetDlgItem(panel,760))=='守卫'
        editor=u.GetDlgItem(panel,760);menu.select(panel,app.pid,checked=[('本次命名','显示全部字段')])
        assert u.GetDlgItem(panel,760)==editor
        mouse_pick(782,False);n.wait(lambda:n.text(u.GetDlgItem(panel,760))=='模型')
        time.sleep(.1);assert u.GetDlgItem(panel,760), 'an old blur must not close the newly selected field'
        mouse_pick(781,False);n.wait(lambda:n.text(u.GetDlgItem(panel,760))=='守卫')
        outside_click();n.wait(lambda:not u.GetDlgItem(panel,760));filled('守卫_模型_v001')
        command(781);assert u.GetDlgItem(panel,760)
        menu.select(panel,app.pid,('本次命名','显示全部字段'));n.wait(no_inputs);filled('守卫_模型_v001')
        checks.append('the unified field-view toggle retains valid input; menu cancellation keeps the active editor open')
        command(831);set_text(811,'bad/name')
        error_shown()
        outside_click();time.sleep(.15);assert u.GetDlgItem(panel,811)
        command(789);assert u.GetDlgItem(panel,811) and not u.GetDlgItem(panel,781)
        command(106);command(832)
        assert u.GetDlgItem(panel,811) and not u.GetDlgItem(panel,812) and host.poll() is None
        command(865);error_shown()
        u.SendMessageW(u.GetDlgItem(panel,811),0x100,27,0);filled('守卫_模型_审核_v001');assert no_inputs()
        checks.append('invalid subject cannot be hidden by switching editors or bypassed through a chip/Save; Escape restores the subject and preserves the selected stage')
        command(831);editor=u.GetDlgItem(panel,811)
        u.SendMessageW(editor,0x10D,0,0);set_text(811,'中文对象');u.SendMessageW(editor,0x100,13,0)
        assert not u.IsWindowEnabled(u.GetDlgItem(panel,700))
        command(700);assert u.GetDlgItem(panel,811)==editor
        time.sleep(.6);assert n.text(u.GetDlgItem(panel,101))=='守卫_模型_审核_v001.txt' and u.GetDlgItem(panel,811)==editor
        outside_click();time.sleep(.15);assert u.GetDlgItem(panel,811)==editor
        u.SendMessageW(editor,0x10E,0,0);filled('中文对象_模型_审核_v001')
        n.wait(no_inputs);command(700);filled('守卫_模型_审核_v001')
        checks.append('IME composition never applies partial text or closes the editor; outside dismissal resumes after composition commits')
        command(831);set_text(811,'取消的主体');filled('取消的主体_模型_审核_v001')
        u.SendMessageW(u.GetDlgItem(panel,811),0x100,27,0);filled('守卫_模型_审核_v001')
        command(864);filled('守卫_模型_工作_v001')
        command(700);filled('守卫_模型_审核_v001');command(701);filled('守卫_模型_工作_v001')
        command(865);filled('守卫_模型_审核_v001')
        checks.append('Escape keeps another field selection; undo and redo never resurrect the cancelled subject')
        command(831);set_text(811,'第一主体');filled('第一主体_模型_审核_v001')
        command(864);filled('第一主体_模型_工作_v001');n.wait(no_inputs)
        command(831);set_text(811,'第二主体');filled('第二主体_模型_工作_v001');command(888)
        command(700);filled('第一主体_模型_工作_v001')
        command(700);filled('第一主体_模型_审核_v001');command(700);filled('守卫_模型_审核_v001')
        checks.append('typing interrupted by a chip creates separate meaningful undo steps in order')
        subject_button=u.GetDlgItem(panel,831)
        subjects=menu.select(panel,app.pid,anchor=821,exclude=['取消的主体','bad/name'])
        assert subjects[:2]==['第二主体','第一主体'],subjects
        assert subjects.count('守卫')==1 and u.GetDlgItem(panel,831)==subject_button
        menu.select(panel,app.pid,'第一主体',anchor=821);filled('第一主体_模型_审核_v001')
        command(700);filled('守卫_模型_审核_v001')
        command(701);filled('第一主体_模型_审核_v001');command(700);filled('守卫_模型_审核_v001')
        subjects=menu.select(panel,app.pid,anchor=821,capture=out/'recent-subjects.png',dark=True)
        assert subjects[0]=='第一主体' and subjects.count('第一主体')==1,subjects
        assert no_inputs() and host.poll() is None and not list(out.glob('*.txt'))
        checks.append('subject dropdown reuses confirmed recent names newest first without changing type/stage/numbering; duplicates, cancelled/invalid drafts are excluded; cancel and Undo/Redo remain safe')
        choose(822,'自定义…');set_text(812,'自定义类型');filled('守卫_自定义类型_审核_v001')
        command(888);n.wait(no_inputs)
        assert n.text(u.GetDlgItem(panel,854))=='自定义类型'
        command(854);filled('守卫_审核_v001');command(700);filled('守卫_自定义类型_审核_v001')
        command(701);filled('守卫_审核_v001');command(852);filled('守卫_模型_审核_v001')
        checks.append('custom text becomes a visible reusable chip; toggle, undo and redo preserve consistency')
        choose(750,'直接修改完整文件名');n.activate(panel)
        preview=u.GetDlgItem(panel,896);u.SetFocus(preview)
        set_text(896,'TemporaryFullName_'*7);filled('TemporaryFullName_'*7)
        assert u.GetDlgItem(panel,896)==preview,'long full-name typing must preserve the focused edit control'
        set_text(896,'临时整名');filled('临时整名')
        choose(750,'恢复分项填写');filled('守卫_模型_审核_v001');assert no_inputs()
        menu.select(panel,app.pid,'关闭自动编号',anchor=889);filled('守卫_模型_审核')
        menu.select(panel,app.pid,'自动版本 vNNN',anchor=889);filled('守卫_模型_审核_v001')
        checks.append('full-name edit/recovery and optional numbering remain available without exposing fields by default')
        assert u.IsWindowVisible(u.GetDlgItem(panel,894))
        menu.select(panel,app.pid,'前后缀…',anchor=894,dark=True);n.wait(lambda:u.GetDlgItem(panel,186))
        set_text(186,'P_');filled('P_守卫_模型_审核_v001')
        e.set_field(panel,187,'_尾');filled('P_守卫_模型_审核_尾_v001')
        assert u.GetDlgItem(panel,186) and u.GetDlgItem(panel,187), 'moving inside one tool must not dismiss it'
        e.set_field(panel,187,'');filled('P_守卫_模型_审核_v001')
        set_text(186,'bad/');error_shown()
        outside_click();time.sleep(.15);assert u.GetDlgItem(panel,186)
        command(142);assert u.GetDlgItem(panel,186), 'invalid affix must stay visible'
        set_text(186,'P_');filled('P_守卫_模型_审核_v001');outside_click();n.wait(no_inputs)
        assert '•' in n.text(u.GetDlgItem(panel,790))
        assert 'P_' in n.text(u.GetDlgItem(panel,890))
        assert all(u.IsWindowVisible(u.GetDlgItem(panel,i)) for i in [890,891]) and not u.IsWindowVisible(u.GetDlgItem(panel,894))
        command(891);set_text(172,'守卫');set_text(173,'士兵');filled('P_士兵_模型_审核_v001')
        combo=u.GetDlgItem(panel,174);u.SendMessageW(combo,0x14F,1,0)
        time.sleep(.1);assert u.GetDlgItem(panel,172)
        u.SendMessageW(combo,0x14F,0,0);time.sleep(.1);assert u.GetDlgItem(panel,172)
        u.SendMessageW(combo,0x14E,6,0);u.SendMessageW(panel,0x111,174|(1<<16),combo)
        set_text(172,'[');outside_click();time.sleep(.1);assert u.GetDlgItem(panel,172), 'invalid regex must stay visible'
        set_text(172,'守卫');set_text(173,'士兵');filled('P_士兵_模型_审核_v001')
        outside_click();n.wait(no_inputs);assert '•' in n.text(u.GetDlgItem(panel,790))
        assert n.text(u.GetDlgItem(panel,891))!='替换：关闭'
        e.capture(panel,out/'active-rules.png')
        menu.select(panel,app.pid,('本次命名','清除修饰规则'),checked=[('本次命名','前后缀…'),('本次命名','替换 / 正则…')]);filled('守卫_模型_审核_v001')
        assert u.IsWindowVisible(u.GetDlgItem(panel,894)) and not any(u.IsWindowVisible(u.GetDlgItem(panel,i)) for i in [890,891])
        command(700);filled('P_士兵_模型_审核_v001');command(779);filled('守卫_模型_审核_v001')
        checks.append('optional rules open directly in the simple screen; invalid affixes block Save and hide; active rules remain visible and clear/undo together')
        command(142);n.wait(lambda:u.GetDlgItem(panel,187))
        n.click(panel,caption='_定稿');filled('守卫_模型_定稿_v001')
        assert n.text(u.GetDlgItem(panel,187))==''
        command(142);n.wait(no_inputs);command(865);filled('守卫_模型_审核_v001')
        checks.append('stage suffix presets share the stage field; choosing another stage replaces them without conflicting suffixes')
        choose(750,'还原本次名称与位置');filled('原文件');assert no_inputs()
        choose(750,'撤销命名组合');filled('守卫_模型_审核_v001');assert no_inputs()
        choose(750,'重做命名组合');filled('原文件');assert no_inputs()
        choose(750,'撤销命名组合');filled('守卫_模型_审核_v001');assert no_inputs()
        checks.append('both field views have no footer tools; Naming settings restores the original name and undo/redo restore fields and numbering')
        n.activate(panel);command(120);time.sleep(.15)
        buttons={i:u.GetDlgItem(panel,i) for i in [120]};baseline={i:pixel(h) for i,h in buttons.items()};samples=[]
        for ident in [864,865,864]:
            command(ident)
            for _ in range(80):
                samples.append({i:(bool(u.IsWindowEnabled(h)),pixel(h)) for i,h in buttons.items()});time.sleep(.01)
        assert all(s[i][0] and s[i][1]==baseline[i] for s in samples for i in buttons)
        choose(824,'定稿');filled('守卫_模型_定稿_v001');e.capture(panel,out/'quick-picks.png')
        checks.append('240 actual screen samples: Recommendation keeps its color and enabled state while switching chips')
        native_save();host.wait(timeout=10);saved=out/'守卫_模型_定稿_v001.txt';assert saved.is_file();recorded(1)
        host,dialog,panel=launch(saved)
        filled('守卫_模型_定稿_v002');assert no_inputs()
        assert '守卫' in n.text(u.GetDlgItem(panel,831))
        e.capture(panel,out/'continue-version.png')
        native_save();host.wait(timeout=10);saved2=out/'守卫_模型_定稿_v002.txt';assert saved2.is_file();recorded(2)
        checks.append('the original dialog Save writes the composed name and records its structure; reopening restores fields and advances the version automatically')
        host,dialog,panel=launch(saved2);filled('守卫_模型_定稿_v003')
        command(864);filled('守卫_模型_工作_v001');native_save()
        host.wait(timeout=10);assert (out/'守卫_模型_工作_v001.txt').is_file();assert saved.is_file() and saved2.is_file()
        checks.append('continuation with a different stage takes one stage click and native Save after synchronization; the intended final name is written without touching older versions')
        host,dialog,panel=launch(initial='原稿_定稿_20261018.txt')
        command(852);filled('原稿_定稿_20261018_模型_v001')
        choose(750,'还原本次名称与位置');filled('原稿_定稿_20261018');assert no_inputs()
        checks.append('an existing stage/date is preserved once; legacy split state cannot duplicate either when quick naming starts')
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        host,dialog,panel=launch()
        filled('原文件_v001');command(852);filled('原文件_模型_v001')
        command(865);filled('原文件_模型_审核_v001')
        command(142);set_text(186,'P_');filled('P_原文件_模型_审核_v001')
        command(142);command(831);set_text(811,'bad/name')
        error_shown()
        native_set('12314321421')
        assert all(u.GetDlgItem(panel,i) for i in [852,853,854,864,865,866])
        assert '12314321421' in n.text(u.GetDlgItem(panel,831))
        assert '•' not in n.text(u.GetDlgItem(panel,790))
        timed_pick(865,'12314321421_审核')
        assert u.GetForegroundWindow()==dialog, 'a chip must fill the native name without activating Jump'
        timed_pick(852,'12314321421_模型_审核')
        mouse_pick(864);mouse_pick(866);filled('12314321421_模型_交付')
        assert host.poll() is None, 'automatic fill must not submit the Save dialog'
        timed_pick(865,'12314321421_模型_审核')
        assert sorted(fill_latency)[len(fill_latency)//2]<250 and max(fill_latency)<450, fill_latency
        checks.append('native readback after real chip clicks has median latency below 250 ms with no fixed half-second wait')
        checks.append('real mouse clicks while the native dialog stays foreground automatically fill its filename; rapid picks keep the latest value without saving')
        native_save();host.wait(timeout=10)
        assert (out/'12314321421_模型_审核.txt').read_text(encoding='utf-8')=='Jump native integration test\n'
        checks.append('native numeric filename edits keep quick naming visible, replace stale fields/rules/invalid drafts and save the new name with working type/stage chips')
        host,dialog,panel=launch();filled('原文件_v001')
        native_set('');assert not u.GetDlgItem(panel,106)
        native_set('手动_定稿_20261018_v007')
        time.sleep(.7);assert n.text(u.GetDlgItem(panel,101))=='手动_定稿_20261018_v007.txt'
        command(853);filled('手动_定稿_20261018_v007_材质')
        command(700);filled('手动_定稿_20261018_v007');assert no_inputs()
        checks.append('clearing and retyping a native name keeps quick naming, preserves literal stage/date/version text and never automatically overwrites it')
        command(831);set_text(811,'bad/name');error_shown()
        choose(750,'还原本次名称与位置',disabled=['命名方案','本次命名','更多工具'])
        filled('原文件');assert no_inputs()
        choose(750,'撤销命名组合');filled('手动_定稿_20261018_v007');assert no_inputs()
        checks.append('Naming settings remains usable for invalid-draft recovery; unsafe actions are disabled, Restore clears the draft and Undo recovers the valid name')
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        host,dialog,panel=launch(initial='Rock.txt');filled('Rock_v001')
        menu.select(panel,app.pid,'贴图资源',anchor=792);filled('T_Rock_A_BC')
        assert '贴图资源' in n.text(u.GetDlgItem(panel,792))
        menu.select(panel,app.pid,'资产文件',anchor=792);filled('Rock_v001')
        command(831);editor=u.GetDlgItem(panel,811);long_subject='LongAsset_'*9+'Rock'
        set_text(811,long_subject);filled(long_subject+'_v001')
        assert u.GetDlgItem(panel,811)==editor,'a long live name must not recreate the focused editor'
        command(888);n.wait(no_inputs)
        assert n.text(u.GetDlgItem(panel,101))==long_subject+'_v001.txt'
        assert not u.GetDlgItem(panel,769) and not u.GetDlgItem(panel,115)
        assert e.rect(u.GetDlgItem(panel,101)).bottom-e.rect(u.GetDlgItem(panel,101)).top>32*u.GetDpiForWindow(panel)/96
        e.capture(panel,out/'long-name.png')
        limit=round(450*u.GetDpiForWindow(panel)/96)
        menu.select(panel,app.pid,anchor=821,dark=True,max_width=limit,tip_expected=long_subject,capture=out/'long-subject-menu.png')
        assert not u.GetDlgItem(panel,811)
        checks.append('the combined subject field opens by mouse and keyboard; menus use dark surfaces; inactive rules collapse and active rules remain visible; long menu names stay bounded with a complete tooltip')
        assert host.poll() is None
        checks.append('clicking the scheme title switches actual naming behavior; long names stay inspectable without replacing the active editor or submitting Save')
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        host,dialog,panel=launch(saved=saved2,open_dialog=True)
        assert not any(u.GetDlgItem(panel,i) for i in [101,106,750,765,831])
        time.sleep(.7)
        assert any(n.text(h)==saved2.name for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit')
        checks.append('remembered quick naming affects Save only; Open has no naming UI and never increments its filename')
        u.PostMessageW(dialog,0x111,2,u.GetDlgItem(dialog,2));host.wait(timeout=5)
        for h in n.windows(app.pid,'Jump.Control'):u.PostMessageW(h,0x10,0,0)
        app.wait(timeout=5)
        stored=configparser.ConfigParser();stored.read(cfg,encoding='utf-16')
        subject_count=stored.getint('Jump','recentSubjects')
        expected=[bytes.fromhex(stored.get('RecentSubject'+str(i),'value')).decode('utf-16-be') for i in range(subject_count)]
        assert 1<=len(expected)<=8 and '取消的主体' not in expected and 'bad/name' not in expected
        app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        host,dialog,panel=launch(initial='重启检查.txt');filled('重启检查_v001')
        subjects=menu.select(panel,app.pid,anchor=821)
        assert subjects[:len(expected)]==expected,subjects
        menu.select(panel,app.pid,expected[0],anchor=821);filled(expected[0]+'_v001')
        checks.append('up to eight recent subjects persist across an actual app restart and remain selectable in another save dialog')
        result={'passed':checks,'nativeFillLatencyMs':fill_latency,'output':str(out)};(out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(result,ensure_ascii=False,indent=2))
    except Exception:
        print('Failure output: '+str(out),flush=True)
        if 'panel' in locals():
            (out/'failure.json').write_text(json.dumps([(u.GetDlgCtrlID(h),n.text(h)) for h in n.children(panel)],ensure_ascii=False,indent=2),encoding='utf-8')
            if u.IsWindowVisible(panel):e.capture(panel,out/'failure.png')
        raise
    finally:
        for host in hosts:
            if host.poll() is None:host.terminate();host.wait(timeout=5)
        for h in n.windows(app.pid,'Jump.Control'):u.PostMessageW(h,0x10,0,0)
        try:app.wait(timeout=5)
        except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)

if __name__=='__main__':
    parser=argparse.ArgumentParser();group=parser.add_mutually_exclusive_group()
    group.add_argument('--rapid-only',action='store_true');group.add_argument('--native-history-only',action='store_true')
    options=parser.parse_args();run(rapid_only=options.rapid_only,native_history_only=options.native_history_only)
