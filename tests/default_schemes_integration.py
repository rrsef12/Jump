"""Defaults, per-application isolation and removal using two owned save hosts."""
import configparser
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import shutil
import subprocess
import time
import uuid
import native_integration as n
import extended_integration as e
import naming_menu as menu
from naming_fixture import install, encode

u=n.u
u.GetAncestor.argtypes=[w.HWND,c.c_uint];u.GetAncestor.restype=w.HWND


def run(designer_only=False):
    out=n.ROOT/'tests/output/default-schemes'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg=out/'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nmodifiers=-1\nrecipes=0\n',encoding='utf-16')
    install(cfg)
    seed=configparser.ConfigParser();seed.read(cfg,encoding='utf-16')
    del seed['Jump']['defaultNamingScheme']
    seed['Jump']['activeNamingScheme']=encode('game-assets-v1')
    with cfg.open('w',encoding='utf-16') as file:seed.write(file)
    first=n.ROOT/'build/Jump.TestHost.exe'
    second=out/'Paint.TestHost.exe';shutil.copy2(first,second)
    dev=n.ROOT/'build/dev/settings.ini'
    original_hash=hashlib.sha256(dev.read_bytes()).digest() if dev.exists() else None
    app=host=panel=dialog=None
    passed=[]

    def parser():
        result=configparser.ConfigParser();result.read(cfg,encoding='utf-16');return result

    def decoded(section,key):
        return bytes.fromhex(parser().get(section,key,fallback='')).decode('utf-16-be')

    def assignments():
        result={};data=parser()
        for i in range(data.getint('Jump','applicationNamingSchemes',fallback=0)):
            section='ApplicationNamingScheme'+str(i)
            result[decoded(section,'executable')]=decoded(section,'scheme')
        return result

    def stop():
        nonlocal app,host
        if app and app.poll() is None:
            for window in n.windows(app.pid,'Jump.Control'):u.PostMessageW(window,0x10,0,0)
            try:app.wait(timeout=5)
            except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)
        if host and host.poll() is None:host.terminate();host.wait(timeout=5)

    def start(executable,scheme):
        nonlocal app,host,panel,dialog
        host=subprocess.Popen([str(executable),'--output',str(out),'--name','Rock.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None))
        app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-target-pid',str(host.pid)],creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        def attached():
            n.activate(dialog)
            return next(iter(n.windows(app.pid,'Jump.Panel',True)),None)
        panel=n.wait(attached);assert_scheme(scheme)
        n.wait(lambda:parser().getint('Jump','History',fallback=0)>0)

    def assert_scheme(scheme):
        n.wait(lambda:n.panel_ready(panel) and n.text(u.GetDlgItem(panel,792))=='方案 · '+scheme,label='scheme '+scheme)

    def popup(window,anchor,path=None,disabled=(),capture=None):
        content_labels=['主要名称','项目名称','日期','文件类别','补充说明','当前状态','手动编号','固定文字…','自定义内容…']
        if path and len(path)==1 and path[0] in content_labels:path=['新增命名内容',path[0]]
        n.activate(u.GetAncestor(window,2))
        previous=w.POINT();u.GetCursorPos(c.byref(previous));rect=e.rect(window)
        u.SetCursorPos(rect.left+4,rect.top+4)
        u.PostMessageW(window,0x111,anchor,u.GetDlgItem(window,anchor))
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
        time.sleep(.1);current=u.SendMessageW(popup,0x1E1,0,0)
        for label in disabled:
            owner,index=menu.find(current,['新增命名内容',label]);assert u.GetMenuState(owner,index,0x400)&3,(label,'enabled')
        if capture:e.capture(popup,out/capture)
        if path is None:
            u.PostMessageW(window,0x1F,0,0);n.wait(lambda:not n.windows(app.pid,'#32768',True));u.SetCursorPos(previous.x,previous.y);return
        for depth,label in enumerate(path):
            owner,index=menu.find(current,label)
            assert not u.GetMenuState(owner,index,0x400)&3,(label,'disabled')
            selected_caption=menu.labels(owner)[index]
            child=menu.select_item(popup,owner,index)
            if depth+1<len(path):
                assert child
                popup=n.wait(lambda:next((h for h in n.windows(app.pid,'#32768',True) if u.SendMessageW(h,0x1E1,0,0)==child),None))
                current=child;time.sleep(.1)
        n.wait(lambda:not n.windows(app.pid,'#32768',True))
        time.sleep(.08)
        if path[-1]=='文件名格式…':n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(window,4002)))
        elif path[0]=='编辑已有内容' or '已有，点击编辑' in selected_caption:n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(window,4040)))
        u.SetCursorPos(previous.x,previous.y)

    def manage(scheme):
        n.activate(panel);u.PostMessageW(panel,0x111,112,0)
        manager=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None))
        n.activate(manager);control=u.GetDlgItem(manager,202)
        count=u.SendMessageW(control,0x18B,0,0)
        for i in range(count):
            text=c.create_unicode_buffer(300);u.SendMessageW(control,0x189,i,c.addressof(text))
            if text.value==scheme:
                u.SendMessageW(control,0x186,i,0);u.SendMessageW(manager,0x111,202|(1<<16),control);return manager
        raise AssertionError(('scheme missing',scheme))

    def edit_scheme(scheme,copy=False):
        manager=manage(scheme)
        u.PostMessageW(manager,0x111,254 if copy else 207,0)
        return n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None)),manager

    def application_items(form):
        combo=u.GetDlgItem(form,4141);result=[]
        for index in range(u.SendMessageW(combo,0x146,0,0)):
            value=c.create_unicode_buffer(300);u.SendMessageW(combo,0x148,index,c.addressof(value));result.append(value.value)
        return result

    def add_application(form,executable):
        if not u.IsWindowVisible(u.GetDlgItem(form,4141)):
            u.SendMessageW(form,0x111,4172,u.GetDlgItem(form,4172))
        u.PostMessageW(form,0x111,4143,u.GetDlgItem(form,4143))
        picker=n.wait(lambda:next(iter(n.windows(app.pid,'#32770',True)),None),label='owned exe picker')
        control=u.GetDlgItem(picker,1152)
        if not control:
            combo=u.GetDlgItem(picker,1148)
            control=next(h for h in n.children(combo or picker) if n.text(h,u.GetClassNameW)=='Edit' and u.IsWindowVisible(h))
        value=c.create_unicode_buffer(str(executable));u.SendMessageW(control,0xC,0,c.addressof(value));u.PostMessageW(picker,0x111,1,0)
        n.wait(lambda:not u.IsWindow(picker),label='exe selected')
        n.wait(lambda:executable.name.lower() in application_items(form),label='software added to draft')

    def close_editor(form,save=False):
        u.PostMessageW(form,0x111,1 if save else 2,0)
        n.wait(lambda:not u.IsWindow(form),label='scheme editor closed')

    def scope_editor(manager):
        u.PostMessageW(manager,0x111,256,u.GetDlgItem(manager,256))
        form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4173)))
        return form

    def global_default(manager):
        form=scope_editor(manager)
        u.SendMessageW(u.GetDlgItem(form,4173),0xF1,1,0)
        u.SendMessageW(form,0x111,4173,u.GetDlgItem(form,4173));close_editor(form,True)

    def application_default(manager,executable):
        form=scope_editor(manager);add_application(form,executable);close_editor(form,True)

    def panel_application_default(executable,remove=False):
        menu.select(panel,app.pid,'方案设置…',anchor=792)
        form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        u.SendMessageW(form,0x111,4172,u.GetDlgItem(form,4172))
        if remove:
            index=application_items(form).index(executable.name.lower())
            u.SendDlgItemMessageW(form,4141,0x14E,index,0)
            u.SendMessageW(form,0x111,4144,u.GetDlgItem(form,4144))
        else:add_application(form,executable)
        close_editor(form,True)
        n.wait(lambda:n.panel_ready(panel))

    try:
        start(first,'游戏引擎资产')
        if designer_only:
            initial=assignments();before=n.text(u.GetDlgItem(panel,101))
            form,manager=edit_scheme('适配器测试')
            assert not u.IsWindowVisible(u.GetDlgItem(form,4141))
            popup(form,4005,capture='unified-add-menu.png')
            def set_text(window,ident,value):
                buffer=c.create_unicode_buffer(value);u.SendMessageW(u.GetDlgItem(window,ident),0xC,0,c.addressof(buffer))
            def add_field(label,value=None):
                popup(form,4005,[label])
                if value is not None:
                    title='添加固定文字' if label=='固定文字…' else '添加命名内容'
                    entry=n.wait(lambda:next(iter(n.dialogs(app.pid,title)),None))
                    set_text(entry,100,value);close_editor(entry,True)
            # Existing content opens its editor instead of adding the same semantic field twice.
            add_field('主要名称');assert u.SendDlgItemMessageW(form,4004,0x18B,0,0)==1
            u.SendMessageW(form,0x111,4100,u.GetDlgItem(form,4100))
            add_field('项目名称');add_field('日期');add_field('固定文字…','Final');add_field('自定义内容…','LOD')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4304)))
            set_text(form,4304,'01')
            u.SendMessageW(form,0x111,4320,u.GetDlgItem(form,4320))
            add_field('主要名称')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4304)))
            preview=n.text(u.GetDlgItem(form,4083));assert preview.endswith('_Final_01_Rock'),preview
            add_field('补充说明');assert n.text(u.GetDlgItem(form,4225))=='补充说明'
            u.SendMessageW(form,0x111,4325,u.GetDlgItem(form,4325))
            add_field('文件类别');add_field('当前状态');add_field('手动编号')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4227)))
            assert u.IsWindowEnabled(u.GetDlgItem(form,4005))
            popup(form,4005,disabled=['固定文字…','自定义内容…','补充说明'])
            popup(form,4005,['手动编号'])
            assert u.SendDlgItemMessageW(form,4004,0x18B,0,0)==8
            u.SendMessageW(form,0x111,4021,u.GetDlgItem(form,4021))
            assert n.text(u.GetDlgItem(form,4062))=='3' and u.SendDlgItemMessageW(form,4061,0x147,0,0)==2
            u.SendMessageW(form,0x111,4100,u.GetDlgItem(form,4100))
            preview+='_001'
            assert n.text(u.GetDlgItem(form,4083))==preview
            close_editor(form,True)
            form,manager=edit_scheme('适配器测试')
            assert n.text(u.GetDlgItem(form,4083))==preview
            popup(form,4005,['编辑已有内容','1 · 项目名称'])
            for index,source in [(0,1),(1,2),(2,3),(3,0),(4,0)]:
                u.SendDlgItemMessageW(form,4004,0x186,index,0);u.SendMessageW(form,0x111,4004|(1<<16),u.GetDlgItem(form,4004))
                assert u.SendDlgItemMessageW(form,4041,0x147,0,0)==source
            for index in [5,6]:
                u.SendDlgItemMessageW(form,4004,0x186,index,0);u.SendMessageW(form,0x111,4004|(1<<16),u.GetDlgItem(form,4004))
                assert '0 项' not in n.text(u.GetDlgItem(form,4080))
            popup(form,4005,['文件名格式…'])
            assert u.IsWindowVisible(u.GetDlgItem(form,4002))
            u.SendMessageW(form,0x111,4100,u.GetDlgItem(form,4100))
            popup(form,4005,disabled=['自定义内容…'])
            add_application(form,first)
            assert n.text(u.GetDlgItem(form,4083))==preview
            set_text(form,4303,'草稿保留')
            u.SendMessageW(form,0x111,4145,u.GetDlgItem(form,4145))
            assert not u.IsWindowVisible(u.GetDlgItem(form,4141)) and n.text(u.GetDlgItem(form,4303))=='草稿保留'
            add_application(form,first)
            assert n.text(u.GetDlgItem(form,4303))=='草稿保留'
            close_editor(form);assert assignments()==initial
            passed.append('the compact menu merges repeated rule entries, existing presets edit without duplicates even at eight fields, new type/status/manual-number presets retain choices and padding after Save, and software layout preserves unsaved input')
            form,manager=edit_scheme('适配器测试');add_application(form,first);add_application(form,second);add_application(form,first)
            assert application_items(form)==['jump.testhost.exe','paint.testhost.exe']
            popup(form,4005,['编辑已有内容','1 · 项目名称'])
            # Each example also retains whether its optional software section is present.
            u.SendMessageW(form,0x111,4100,u.GetDlgItem(form,4100))
            u.SendDlgItemMessageW(form,4001,0x14E,1,0);u.SendMessageW(form,0x111,4001|(1<<16),u.GetDlgItem(form,4001))
            assert not u.IsWindowVisible(u.GetDlgItem(form,4141))
            u.SendDlgItemMessageW(form,4001,0x14E,0,0);u.SendMessageW(form,0x111,4001|(1<<16),u.GetDlgItem(form,4001))
            assert u.IsWindowVisible(u.GetDlgItem(form,4141)) and application_items(form)==['jump.testhost.exe','paint.testhost.exe']
            assert application_items(form)==['jump.testhost.exe','paint.testhost.exe']
            u.SendMessageW(form,0x111,4100,u.GetDlgItem(form,4100))
            assert assignments()==initial
            close_editor(form);assert assignments()==initial
            passed.append('the editor adds and deduplicates multiple real exe picks; both views retain draft associations; Cancel leaves settings unchanged')
            form,manager=edit_scheme('适配器测试');add_application(form,first);add_application(form,second)
            e.capture(form,out/'scheme-software-editor.png');close_editor(form,True)
            n.wait(lambda:assignments()=={'jump.testhost.exe':'adapter-fixture','paint.testhost.exe':'adapter-fixture'})
            assert n.text(u.GetDlgItem(panel,101))==before
            assert decoded('Jump','defaultNamingScheme')=='game-assets-v1'
            form,manager=edit_scheme('适配器测试',copy=True)
            assert application_items(form)==['未关联软件'] and not u.IsWindowVisible(u.GetDlgItem(form,4141));close_editor(form)
            stop();start(first,'适配器测试');stop();start(second,'适配器测试')
            passed.append('Save commits scheme and exe associations together; both programs automatically select it after restart; copying does not take over the original associations')
            form,manager=edit_scheme('贴图资源');add_application(form,second)
            assert '适配器测试' in n.text(u.GetDlgItem(form,4142)) and '保存后改用' in n.text(u.GetDlgItem(form,4142))
            assert assignments()['paint.testhost.exe']=='adapter-fixture'
            close_editor(form,True)
            n.wait(lambda:assignments()=={'jump.testhost.exe':'adapter-fixture','paint.testhost.exe':'game-textures-v1'},label='reassigned software committed')
            stop();start(second,'贴图资源')
            form,manager=edit_scheme('适配器测试')
            assert application_items(form)==['jump.testhost.exe']
            u.SendMessageW(form,0x111,4144,u.GetDlgItem(form,4144));assert application_items(form)==['未关联软件']
            close_editor(form);assert assignments()['jump.testhost.exe']=='adapter-fixture'
            form,manager=edit_scheme('适配器测试');u.SendMessageW(form,0x111,4145,u.GetDlgItem(form,4145));close_editor(form,True)
            n.wait(lambda:assignments()=={'paint.testhost.exe':'game-textures-v1'},label='software removal committed')
            stop();start(first,'游戏引擎资产')
            passed.append('reassignment displays the existing scheme before saving; removal can be cancelled or saved and falls back to the global default while preserving other programs')
            manager=manage('适配器测试');u.PostMessageW(manager,0x111,210,0)
            form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
            value=c.create_unicode_buffer('软件关联新方案');u.SendMessageW(u.GetDlgItem(form,4000),0xC,0,c.addressof(value))
            add_application(form,first);close_editor(form,True)
            n.wait(lambda:len(assignments())==2,label='new scheme software association committed')
            assert decoded('Jump','defaultNamingScheme')=='game-assets-v1'
            stop();start(first,'软件关联新方案')
            passed.append('a newly created scheme can add an exe before its first save and is automatically selected in that program without changing the global default')
            report={'passed':passed,'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        before=n.text(u.GetDlgItem(panel,101))
        manager=manage('贴图资源')
        global_default(manager)
        n.wait(lambda:decoded('Jump','defaultNamingScheme')=='game-textures-v1')
        assert n.text(u.GetDlgItem(panel,101))==before,'setting a default must preserve the current draft'
        assert '默认：全局' in n.text(u.GetDlgItem(manager,280))
        e.capture(manager,out/'default-manager.png')
        manager=manage('资产文件')
        application_default(manager,first)
        n.wait(lambda:assignments().get('jump.testhost.exe')=='asset-v2')
        u.PostMessageW(manager,0x10,0,0);n.wait(lambda:not u.IsWindow(manager))
        menu.select(panel,app.pid,'项目分类与编号',anchor=792)
        assert decoded('Jump','defaultNamingScheme')=='game-textures-v1'
        assert assignments()['jump.testhost.exe']=='asset-v2'
        passed.append('legacy startup choice migrates once; manager sets global and application defaults without altering the live draft; temporary switching preserves both defaults')
        stop();start(first,'资产文件')
        menu.select(panel,app.pid,'游戏引擎资产',anchor=792)
        panel_application_default(first)
        n.wait(lambda:assignments().get('jump.testhost.exe')=='game-assets-v1')
        stop();start(second,'贴图资源')
        menu.select(panel,app.pid,'项目分类与编号',anchor=792)
        panel_application_default(second)
        n.wait(lambda:assignments().get('paint.testhost.exe')=='game-project-v1')
        stop();start(first,'游戏引擎资产')
        panel_application_default(first,remove=True)
        n.wait(lambda:'jump.testhost.exe' not in assignments())
        assert assignments()['paint.testhost.exe']=='game-project-v1'
        assert_scheme('游戏引擎资产')
        stop();start(first,'贴图资源')
        passed.append('panel sets independent defaults for two executable names; restarts apply the correct override or global fallback; clearing one override keeps the other')
        stop();start(second,'项目分类与编号')
        manager=manage('适配器测试')
        global_default(manager)
        application_default(manager,second)
        n.wait(lambda:assignments().get('paint.testhost.exe')=='adapter-fixture')
        u.PostMessageW(manager,0x111,208,u.GetDlgItem(manager,208))
        n.wait(lambda:decoded('Jump','defaultNamingScheme')=='asset-v2' and not assignments())
        stop();start(second,'资产文件')
        passed.append('existing application assignments can be changed centrally; deleting a custom default removes its references and safely falls back to the asset scheme')
        report={'passed':passed,'output':str(out)}
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        if app:
            for window in n.windows(app.pid,visible=True):
                if n.text(window,u.GetClassNameW) in ('Jump.Panel','Jump.Manager','#32770'):e.capture(window,out/('failure-'+str(window)+'.png'))
        raise
    finally:
        stop()
        if original_hash is not None:assert hashlib.sha256(dev.read_bytes()).digest()==original_hash,'development configuration changed'


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--designer-only',action='store_true')
    run(designer_only=parser.parse_args().designer_only)
