"""Configurable schemes on owned save dialogs: editing, composition and persistence."""
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
from naming_fixture import pack, encode

u=n.u
u.GetAncestor.argtypes=[w.HWND,c.c_uint];u.GetAncestor.restype=w.HWND

def run(catalog_only=False,rules_only=False,labels_only=False,help_only=False,assist_only=False,preview_only=False,controls_only=False):
    out=n.ROOT/'tests/output/naming-designer'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg=out/'settings.ini'
    cfg.write_text('[Jump]\nschema=1\ngroups=0\nFavorite=0\nHistory=0\nmodifiers=-1\nrecipes=0\n',encoding='utf-16')
    if labels_only:
        # An old saved copy must upgrade through the same path as every other
        # scheme, without replacing a label the user has already customized.
        fields=[]
        for index,(ident,label) in enumerate([('project','项目'),('subject','主体'),('type','类型'),
                                               ('variant','变体'),('stage','阶段'),('date','记录日期')]):
            source='1' if index==0 else '0'
            meta=pack(['','','','','','',str(int(index in (1,2,4))),'0','0','0','0',source])
            fields.append(pack([ident,label,'',str(int(index==1)),source,meta]))
        definition=pack(['JNS2','legacy-label-copy','旧方案副本','_','1','0','0','1','3','0','0',pack(['subject','0','']),*fields])
        seeded=configparser.ConfigParser();seeded.read(cfg,encoding='utf-16')
        seeded['Jump']['recipes']='1';seeded['Jump']['activeNamingScheme']=encode('legacy-label-copy')
        seeded['Recipe0']={'name':encode('旧方案副本'),'value':encode('Asset'),'complete':'1','namingOnly':'1','structure':encode(definition)}
        with cfg.open('w',encoding='utf-16') as file:seeded.write(file)
    app=host=panel=dialog=None
    checks=[]
    rule_navigation={}

    def filled(value):
        def ready():
            n.activate(dialog)
            return (n.text(u.GetDlgItem(panel,101))==value+'.txt' and n.panel_ready(panel) and
                    any(n.text(h,u.GetClassNameW)=='Edit' and n.text(h)==value for h in n.children(dialog)))
        n.wait(ready,label='native filename '+value)

    def start():
        nonlocal app,host,panel,dialog
        app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(out),'--name','Rock.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None))
        n.activate(dialog)
        def attached_panel():
            # The shell can restore the previous foreground app just after launch.
            n.activate(dialog)
            return next(iter(n.windows(app.pid,'Jump.Panel',True)),None)
        panel=n.wait(attached_panel,label='panel attached to the owned save dialog')
        n.wait(lambda:n.panel_ready(panel))
        # Automatic directory history is coalesced on the background writer;
        # take Cancel's configuration baseline after this initial record lands.
        n.wait(lambda:parser().getint('Jump','History',fallback=0)>0,label='initial history persisted')

    def stop():
        nonlocal app,host
        if host and host.poll() is None:
            host.terminate();host.wait(timeout=5)
        if app and app.poll() is None:
            for h in n.windows(app.pid,'Jump.Control'):u.PostMessageW(h,0x10,0,0)
            try:app.wait(timeout=5)
            except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)

    def command(ident):
        n.wait(lambda:n.panel_ready(panel));n.activate(panel)
        u.SendMessageW(panel,0x111,ident,u.GetDlgItem(panel,ident) or 0)

    def modal(title):
        return n.wait(lambda:next((h for h in n.dialogs(app.pid,title)),None),label=title)

    def designer():
        n.wait(lambda:n.panel_ready(panel));n.activate(panel)
        if not checks:menu.select(panel,app.pid,('命名方案','编辑 / 复制命名方案…'))
        else:u.PostMessageW(panel,0x111,791,0)
        return modal('命名方案')

    def choose(form,ident,index):
        if ident==4001 and not u.IsWindowVisible(u.GetDlgItem(form,4001)):button(form,4100)
        control=u.GetDlgItem(form,ident);assert control
        u.SendMessageW(control,0x14e,index,0)  # CB_SETCURSEL
        u.SendMessageW(form,0x111,ident|(1<<16),control)

    def field(form,index):
        if not u.IsWindowVisible(u.GetDlgItem(form,4004)):button(form,4100)
        control=u.GetDlgItem(form,4004)
        u.SendMessageW(control,0x186,index,0)
        u.SendMessageW(form,0x111,4004|(1<<16),control)
        rule_navigation[form]=(index,rule_navigation.get(form,(1,0))[1])

    def button(form,ident):
        if ident==4164:
            u.PostMessageW(form,0x111,ident,0);return
        if ident==4009:
            add_item(form,'复制当前方案');return
        if ident==4100 and not u.IsWindowVisible(u.GetDlgItem(form,4004)):
            index,page=rule_navigation.get(form,(1,0))
            edit_content(form,index,page);return
        if ident==4023:
            if not u.IsWindowVisible(u.GetDlgItem(form,4023)):
                add_item(form,'文件名格式…');return
        if 4020<=ident<=4023 and not u.IsWindowVisible(u.GetDlgItem(form,4004)):button(form,4100)
        u.SendMessageW(form,0x111,ident,u.GetDlgItem(form,ident))
        if 4020<=ident<=4022:rule_navigation[form]=(u.SendDlgItemMessageW(form,4004,0x188,0,0),ident-4020)

    def check(form,ident,value):
        u.SendDlgItemMessageW(form,ident,0xF1,int(value),0)
        button(form,ident)

    def text(form,ident,value):
        if ident==4093 and not u.IsWindowVisible(u.GetDlgItem(form,4093)):button(form,4100)
        n.wait(lambda:u.GetDlgItem(form,ident),label='editor '+str(ident))
        if form==panel:n.activate(panel)
        buffer=c.create_unicode_buffer(value)
        assert u.SendMessageW(u.GetDlgItem(form,ident),0xC,0,c.addressof(buffer))
        if n.text(u.GetDlgItem(form,ident),u.GetClassNameW)=='ComboBox':
            u.SendMessageW(form,0x111,ident|(5<<16),u.GetDlgItem(form,ident))
        n.wait(lambda:n.text(u.GetDlgItem(form,ident))==value,label='editor text '+value)

    def save(form):
        u.PostMessageW(form,0x111,1,0)
        n.wait(lambda:not u.IsWindow(form),label='designer saved')

    def add_item(form,label=None,disabled=(),capture=None):
        if isinstance(label,str) and label in ['主要名称','项目名称','日期','固定文字…','自定义内容…']:
            label=['新增命名内容',label]
        n.activate(u.GetAncestor(form,2))
        previous=w.POINT();u.GetCursorPos(c.byref(previous));rect=e.rect(form)
        u.SetCursorPos(rect.left+4,rect.top+4)
        u.PostMessageW(form,0x111,4005,0)
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
        current=u.SendMessageW(popup,0x1E1,0,0)
        if capture:e.capture(popup,out/capture)
        for name in disabled:
            owner,index=menu.find(current,['新增命名内容',name]);assert u.GetMenuState(owner,index,0x400)&3
        if label is None:u.PostMessageW(form,0x1F,0,0)
        else:
            path=[label] if isinstance(label,str) else label
            for depth,name in enumerate(path):
                owner,index=menu.find(current,name)
                assert not u.GetMenuState(owner,index,0x400)&3,(name,'disabled')
                child=menu.select_item(popup,owner,index)
                if depth+1<len(path):
                    assert child
                    popup=n.wait(lambda:next((h for h in n.windows(app.pid,'#32768',True) if u.SendMessageW(h,0x1E1,0,0)==child),None))
                    current=child;time.sleep(.1)
        n.wait(lambda:not n.windows(app.pid,'#32768',True))
        # The popup is destroyed before the owner's selected command finishes laying out.
        time.sleep(.08)
        if label is not None:
            if path[-1]=='文件名格式…':n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4002)) or n.text(u.GetDlgItem(form,4084)))
            elif path[0]=='编辑已有内容':n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4040)))
        u.SetCursorPos(previous.x,previous.y)
        if label is not None and path[0]=='编辑已有内容' and len(path)>1 and ' · ' in path[1]:
            rule_navigation[form]=(int(path[1].split(' · ')[0])-1,0)

    def edit_content(form,index,page=0):
        advanced=u.IsWindowVisible(u.GetDlgItem(form,4004))
        label=items(form,4004)[index] if advanced else n.text(u.GetDlgItem(form,4220+index))
        add_item(form,['编辑已有内容',str(index+1)+' · '+label])
        if page:button(form,4020+page)

    def add_field(form,label):
        add_item(form,'自定义内容…')
        entry=modal('添加命名内容');text(entry,100,label);save(entry)

    def bounds(form):
        client=w.RECT();u.GetClientRect(form,c.byref(client));origin=w.POINT();u.ClientToScreen(form,c.byref(origin))
        for control in n.children(form):
            if not u.IsWindowVisible(control):continue
            rect=e.rect(control)
            # Combo dropdowns extend past their closed control by native design.
            if n.text(control,u.GetClassNameW)=='ComboLBox':continue
            assert rect.left>=origin.x and rect.top>=origin.y,(u.GetDlgCtrlID(control),'above/left')
            assert rect.right<=origin.x+client.right and rect.bottom<=origin.y+client.bottom,(u.GetDlgCtrlID(control),'outside')

    def parser():
        p=configparser.ConfigParser();p.read(cfg,encoding='utf-16');return p

    def original_definition():
        p=parser()
        return {s:dict(p[s]) for s in p.sections() if s=='Recipe0' or s.startswith('Recipe0.Structure')}

    def recipe_structure(index):
        p=parser();section='Recipe'+str(index);parts=p[section].getint('structureParts',0)
        encoded=''.join(p[section+'.Structure'+str(i)]['value'] for i in range(parts)) if parts else p[section]['structure']
        return bytes.fromhex(encoded).decode('utf-16-be')

    def items(window, ident, combo=False):
        control=u.GetDlgItem(window,ident);values=[]
        for index in range(u.SendMessageW(control,0x146 if combo else 0x18B,0,0)):
            buffer=c.create_unicode_buffer(300)
            u.SendMessageW(control,0x148 if combo else 0x189,index,c.addressof(buffer));values.append(buffer.value)
        return values

    def select_row(manager, name):
        index=items(manager,202).index(name);control=u.GetDlgItem(manager,202)
        u.SendMessageW(control,0x186,index,0);u.SendMessageW(manager,0x111,202|(1<<16),control)

    def manage():
        command(112)
        return n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None))

    try:
        if assist_only or controls_only:
            model_check=subprocess.run([str(n.ROOT/'build/Jump.exe'),'--save-assist-check',str(out/'model')],timeout=15)
            assert model_check.returncode==0,(out/'model/failure.txt').read_text() if (out/'model/failure.txt').exists() else 'model checks failed'
        start();filled('Rock_v001')
        if controls_only:
            before=cfg.read_bytes();form=designer()
            for example in range(4):
                choose(form,4001,example)
                assert u.IsWindowVisible(u.GetDlgItem(form,4023))
                for i in range(4 if example else 6):assert u.IsWindowVisible(u.GetDlgItem(form,4340+i))
                button(form,4340);assert u.IsWindowVisible(u.GetDlgItem(form,4160))
                button(form,4100)
            choose(form,4001,1)
            assert '仅英文' in n.text(u.GetDlgItem(form,4241))
            e.capture(form,out/'visible-field-settings.png')
            button(form,4341);button(form,4021)
            assert u.SendDlgItemMessageW(form,4061,0x147,0,0)==1 and '关闭限制（允许中文）' in items(form,4061,True)
            u.PostMessageW(form,0x111,4150,0);trial=modal('文件名效果预览')
            editor=u.GetDlgItem(trial,4600);buffer=c.create_unicode_buffer('守卫.fbx')
            u.SendMessageW(editor,0xB1,0,-1);u.SendMessageW(editor,0xC2,1,c.addressof(buffer))
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(trial,4613)) and '仅允许英文字母' in items(trial,4601)[0])
            e.capture(trial,out/'preview-rule-shortcut.png');button(trial,4613);n.wait(lambda:not u.IsWindow(trial))
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4061)) and '主要名称' in n.text(u.GetDlgItem(form,4121)))
            choose(form,4061,0);text(form,4062,'2');text(form,4065,'LOD');choose(form,4060,1)
            button(form,4161)
            assert u.SendDlgItemMessageW(form,4061,0x147,0,0)==0 and n.text(u.GetDlgItem(form,4062))=='0' and n.text(u.GetDlgItem(form,4065))==''
            button(form,4020);text(form,4093,'守卫');assert '守卫' in n.text(u.GetDlgItem(form,4083))
            field(form,2);button(form,4021);choose(form,4061,2)
            # Even an incompatible default must allow navigation to the switch or fix.
            button(form,4020);check(form,4160,False)
            button(form,4100);assert '已关闭' in n.text(u.GetDlgItem(form,4242))
            button(form,4023);assert u.IsWindowVisible(u.GetDlgItem(form,4162)) and u.IsWindowVisible(u.GetDlgItem(form,4163))
            check(form,4162,False);check(form,4163,False)
            e.capture(form,out/'scheme-function-switches.png')
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before;filled('Rock_v001')
            checks.append('every built-in exposes all field settings and switches; preview errors link directly to their rule; disabling restrictions and clearing formatting work; invalid defaults never trap settings navigation; Cancel remains atomic')
            form=designer();choose(form,4001,1);text(form,4000,'控制设置验证')
            button(form,4341);button(form,4021);choose(form,4061,0)
            field(form,2);button(form,4020);check(form,4160,False)
            button(form,4023);button(form,4164);routes=modal('目录推荐')
            target=out/'Models';target.mkdir();choose(routes,4701,0);text(routes,4702,'SM');text(routes,4703,str(target));button(routes,4704);save(routes)
            assert '1' in n.text(u.GetDlgItem(form,4164))
            check(form,4162,False);check(form,4163,False)
            (out/'SM_Rock.txt').write_bytes(b'existing file stays intact')
            save(form);filled('SM_Rock')
            assert not u.IsWindowVisible(u.GetDlgItem(panel,912)) and not u.IsWindowVisible(u.GetDlgItem(panel,913))
            menu.select(panel,app.pid,['设为默认','设为全局默认'],anchor=792)
            stop();start();filled('SM_Rock')
            form=designer();assert '已关闭' in n.text(u.GetDlgItem(form,4242))
            button(form,4023);assert not u.SendDlgItemMessageW(form,4162,0xF0,0,0) and not u.SendDlgItemMessageW(form,4163,0xF0,0,0)
            assert '1' in n.text(u.GetDlgItem(form,4164))
            check(form,4162,True);save(form);filled('SM_Rock')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,912)));assert not u.IsWindowVisible(u.GetDlgItem(panel,913))
            form=designer();button(form,4023);check(form,4163,True);save(form);filled('SM_Rock')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,913)))
            form=designer();button(form,4342);check(form,4160,True);save(form);filled('SM_Rock_A')
            n.wait(lambda:not u.IsWindowVisible(u.GetDlgItem(panel,913)))
            assert (out/'SM_Rock.txt').read_bytes()==b'existing file stays intact' and not (out/'SM_Rock_A.txt').exists()
            checks.append('per-field and scheme switches persist after restart; disabled directory rules are retained and restore when enabled; directory recommendations and exact-name checks obey independent switches; field re-enabling restores its output without submitting Save')
            report={'passed':checks,'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2));return
        if assist_only or preview_only:
            before=cfg.read_bytes();form=designer()
            assert n.text(u.GetDlgItem(form,4150))=='预览多个文件名…'
            u.PostMessageW(form,0x111,4150,0);trial=modal('文件名效果预览')
            assert len(items(trial,4601))==3 and not u.IsWindowVisible(u.GetDlgItem(trial,4603))
            def preview_input(value):
                control=u.GetDlgItem(trial,4600);buffer=c.create_unicode_buffer(value)
                u.SendMessageW(control,0xB1,0,-1);u.SendMessageW(control,0xC2,1,c.addressof(buffer))
            preview_input('Rock.txt\r\nGuard.fbx\r\nbad/name.png')
            n.wait(lambda:len(items(trial,4601))==3 and 'Guard_v001.fbx' in items(trial,4601)[1] and '不要带文件夹路径' in items(trial,4601)[2])
            assert '2 个可生成' in n.text(u.GetDlgItem(trial,4602)) and '1 个需修改' in n.text(u.GetDlgItem(trial,4602))
            e.capture(trial,out/'filename-preview.png')
            listbox=u.GetDlgItem(trial,4601);u.SendMessageW(listbox,0x186,1,0);u.SendMessageW(trial,0x111,4601|(1<<16),listbox)
            button(trial,4604);assert u.IsWindowVisible(u.GetDlgItem(trial,4603))
            assert '新文件名：Guard_v001.fbx' in n.text(u.GetDlgItem(trial,4603)) and '来自左侧的原名' in n.text(u.GetDlgItem(trial,4603))
            e.capture(trial,out/'filename-preview-details.png')
            u.SendMessageW(listbox,0x186,2,0);u.SendMessageW(trial,0x111,4601|(1<<16),listbox)
            assert '直接修改左侧对应的那一行' in n.text(u.GetDlgItem(trial,4603))
            button(trial,4606);assert not items(trial,4601) and u.IsWindowVisible(u.GetDlgItem(trial,4608))
            assert u.IsWindowEnabled(u.GetDlgItem(trial,4604)),'expanded explanation must still collapse after Clear'
            button(trial,4604);assert not u.IsWindowVisible(u.GetDlgItem(trial,4603))
            button(trial,4605);assert len(items(trial,4601))==3
            preview_input('\r\n'.join(f'Sample{i}.fbx' for i in range(21)))
            n.wait(lambda:'共 21 个' in n.text(u.GetDlgItem(trial,4602)));assert len(items(trial,4601))==20
            button(trial,4606);u.PostMessageW(trial,0x111,2,0);n.wait(lambda:not u.IsWindow(trial))
            u.PostMessageW(form,0x111,4150,0);trial=modal('文件名效果预览')
            assert n.text(u.GetDlgItem(trial,4600))=='' and not items(trial,4601),'cleared input must stay empty on reopening'
            e.capture(trial,out/'filename-preview-empty.png')
            u.PostMessageW(trial,0x111,2,0);n.wait(lambda:not u.IsWindow(trial))
            assert n.text(u.GetDlgItem(form,4083))=='Rock_v001'
            if preview_only:
                u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
                report={'passed':['before/after filename cards update live; sample and clear actions, empty state, actionable errors, optional selected-name explanation, 20-name limit and reopen state; preview leaves the scheme, native filename and configuration unchanged'],'output':str(out)}
                (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2));return
            # New rules stay in the scheme draft and cancel atomically.
            target=out/'Models';target.mkdir()
            def configure_route(form):
                add_item(form,'目录推荐…');routes=modal('目录推荐')
                choose(routes,4701,2);text(routes,4702,'模型');text(routes,4703,str(target));button(routes,4704)
                assert u.SendDlgItemMessageW(routes,4700,0x18B,0,0)==1
                e.capture(routes,out/'directory-rules.png');save(routes)
            configure_route(form);u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
            form=designer();configure_route(form);text(form,4202,'模型')
            add_item(form,'文件名格式…');choose(form,4003,0);button(form,4100)
            (out/'Rock_模型.txt').write_bytes(b'existing content')
            (out/'Rock_模型_v001.txt').write_bytes(b'old version')
            save(form);filled('Rock_模型')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,912)) and u.IsWindowVisible(u.GetDlgItem(panel,913)),label='directory suggestion and same-name warning')
            assert '16 B' in n.text(u.GetDlgItem(panel,913))
            def footer_clear():
                preview=e.rect(u.GetDlgItem(panel,101));warning=e.rect(u.GetDlgItem(panel,913));footer=e.rect(u.GetDlgItem(panel,892))
                assert warning.top>=preview.bottom and footer.top>=warning.bottom,'result, warning and footer must not overlap'
            footer_clear()
            e.capture(panel,out/'suggestion-and-conflict.png')
            long_primary='很长的资源名称用于检查提示不会遮挡最终文件名'*2
            (out/(long_primary+'_模型.txt')).write_bytes(b'long name')
            command(831);text(panel,811,long_primary);filled(long_primary+'_模型')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,913)));footer_clear()
            e.capture(panel,out/'long-name-conflict.png')
            command(831);text(panel,811,'Rock');filled('Rock_模型')
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,913)));footer_clear()
            def panel_popup(ident,prefix):
                n.activate(panel);frame=e.rect(panel);u.SetCursorPos(frame.left+4,frame.top+4);u.PostMessageW(panel,0x111,ident,0)
                popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None));handle=u.SendMessageW(popup,0x1E1,0,0)
                owner,index=menu.find(handle,prefix);r=w.RECT();assert u.GetMenuItemRect(None,owner,index,c.byref(r))
                u.SetCursorPos((r.left+r.right)//2,(r.top+r.bottom)//2);time.sleep(.08);u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0)
                n.wait(lambda:not n.windows(app.pid,'#32768',True))
            panel_popup(912,'Models')
            n.wait(lambda:n.panel_ready(panel) and str(target).lower() in n.text(u.GetDlgItem(panel,113)).lower())
            filled('Rock_模型');n.wait(lambda:not u.IsWindowVisible(u.GetDlgItem(panel,913)) and not u.IsWindowVisible(u.GetDlgItem(panel,912)))
            assert not (target/'Rock_模型.txt').exists(),'a directory suggestion must not save a file'
            command(155);edit=u.GetDlgItem(panel,156);buffer=c.create_unicode_buffer(str(out));u.SendMessageW(edit,0xC,0,c.addressof(buffer));u.PostMessageW(edit,0x100,0x0D,1)
            filled('Rock_模型');n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,913)))
            panel_popup(913,'换用下一个编号');filled('Rock_模型_v002')
            n.wait(lambda:not u.IsWindowVisible(u.GetDlgItem(panel,913)))
            assert (out/'Rock_模型.txt').read_bytes()==b'existing content'
            assert not (out/'Rock_模型_v002.txt').exists(),'choosing the next number must not submit Save'
            # Active numbering follows the chosen directory's existing versions.
            for version in (1,2):(target/f'Rock_模型_v{version:03}.txt').write_bytes(b'target version')
            panel_popup(912,'Models');filled('Rock_模型_v003')
            assert not (target/'Rock_模型_v003.txt').exists()
            command(155);edit=u.GetDlgItem(panel,156);buffer=c.create_unicode_buffer(str(out));u.SendMessageW(edit,0xC,0,c.addressof(buffer));u.PostMessageW(edit,0x100,0x0D,1)
            filled('Rock_模型_v002')
            form=designer();add_item(form,'目录推荐…');routes=modal('目录推荐')
            assert u.SendDlgItemMessageW(routes,4700,0x18B,0,0)==1
            u.PostMessageW(routes,0x111,2,0);n.wait(lambda:not u.IsWindow(routes));u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            # Save the resulting file through the original dialog; the earlier target remains intact.
            n.activate(dialog);n.click(dialog,ident=1);host.wait(timeout=10)
            assert (out/'Rock_模型_v002.txt').is_file()
            report={'passed':['multi-name trial and per-field reasons use the shared engine without changing the draft; directory rules persist and cancel atomically; clicking a recommendation navigates without saving; exact-name metadata warns before Save and next-number action skips existing versions; original-dialog Save creates the new file without changing the old file'],'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2));return
        if help_only:
            before=cfg.read_bytes();form=designer()
            def hover_help(path,expected,capture=None,disabled=False,leave=False,activate=False):
                path=[path] if isinstance(path,str) else path
                n.activate(u.GetAncestor(form,2))
                previous=w.POINT();u.GetCursorPos(c.byref(previous));frame=e.rect(form)
                u.SetCursorPos(frame.left+4,frame.top+4)
                u.PostMessageW(form,0x111,4005,0)
                popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
                current=u.SendMessageW(popup,0x1E1,0,0)
                try:
                    for name in path[:-1]:
                        owner,index=menu.find(current,name);child=menu.select_item(popup,owner,index)
                        popup=n.wait(lambda:next((h for h in n.windows(app.pid,'#32768',True) if u.SendMessageW(h,0x1E1,0,0)==child),None))
                        current=child
                    owner,index=menu.find(current,path[-1])
                    assert bool(u.GetMenuState(owner,index,0x400)&3)==disabled
                    row=w.RECT();assert u.GetMenuItemRect(None,owner,index,c.byref(row))
                    started=time.monotonic();u.SetCursorPos((row.left+row.right)//2,(row.top+row.bottom)//2)
                    tip=n.wait(lambda:next((h for h in n.windows(app.pid,'tooltips_class32',True) if expected in n.text(h)),None),label='hover explanation '+path[-1])
                    assert time.monotonic()-started>=.25,'detailed help should wait for a short hover'
                    tip_rect=e.rect(tip)
                    for window in n.windows(app.pid,'#32768',True):
                        r=e.rect(window)
                        assert tip_rect.right<=r.left or tip_rect.left>=r.right or tip_rect.bottom<=r.top or tip_rect.top>=r.bottom,'help covers an open menu'
                    if capture:e.capture(tip,out/capture)
                    if activate:
                        # Click the same owned menu row while its help is visible.
                        u.mouse_event(2,0,0,0,0);u.mouse_event(4,0,0,0,0)
                        n.wait(lambda:not n.windows(app.pid,'#32768',True))
                    if leave:
                        # Separators have no help and clear the previous explanation.
                        separator=next(i for i,value in enumerate(menu.labels(current)) if not value)
                        assert u.GetMenuItemRect(None,current,separator,c.byref(row))
                        u.SetCursorPos((row.left+row.right)//2,(row.top+row.bottom)//2)
                        n.wait(lambda:not n.windows(app.pid,'tooltips_class32',True),label='leave menu item clears help')
                        time.sleep(.55)
                        assert not n.windows(app.pid,'tooltips_class32',True),'a separator must not reshow help'
                finally:
                    if n.windows(app.pid,'#32768',True):u.PostMessageW(form,0x1F,0,0)
                    n.wait(lambda:not n.windows(app.pid,'#32768',True))
                    n.wait(lambda:not n.windows(app.pid,'tooltips_class32',True),label='menu close clears help')
                    u.SetCursorPos(previous.x,previous.y)
            for label,expected in [('新增命名内容','最多 8 项'),('编辑已有内容','修改名称'),
                                   ('文件名格式…','按保存目录续号'),('关联软件…','新打开保存窗口'),('复制当前方案','软件关联不会复制')]:
                hover_help(label,expected)
            for label,expected in [('主要名称','自动带入原文件名'),('项目名称','自动使用当前项目名称'),
                                   ('日期','保存时手动填写或选择'),('文件类别','已有 9 个常用选项'),
                                   ('补充说明','重甲、夜景、侧面'),('当前状态','工作、审核、交付'),
                                   ('手动编号','1 → 001'),('固定文字…','每次都输出相同'),('自定义内容…','作者、LOD、镜头号')]:
                hover_help(['新增命名内容',label],expected,'manual-number-help.png' if label=='手动编号' else None,leave=label=='手动编号')
            hover_help(['编辑已有内容','2 · 主要名称'],'不会重复添加')
            hover_help(['编辑已有内容','调整排列顺序…'],'守卫_模型 → 模型_守卫')
            # Keyboard selection shares the same delayed help.
            n.activate(u.GetAncestor(form,2));u.PostMessageW(form,0x111,4005,0)
            popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None));current=u.SendMessageW(popup,0x1E1,0,0)
            owner,index=menu.find(current,'文件名格式…');menu.select_item(popup,owner,index,activate=False)
            n.wait(lambda:any('按保存目录续号' in n.text(h) for h in n.windows(app.pid,'tooltips_class32',True)))
            u.PostMessageW(form,0x1F,0,0);n.wait(lambda:not n.windows(app.pid,'#32768',True))
            for value in ['First','Second']:
                hover_help(['新增命名内容','固定文字…'],'每次都输出相同',activate=True)
                entry=modal('添加固定文字');text(entry,100,value);save(entry)
            hover_help(['编辑已有内容','7 · 固定文字'],'固定内容不能为空')
            hover_help(['新增命名内容','自定义内容…'],'当前已有 8 项',disabled=True)
            hover_help(['新增命名内容','主要名称'],'不会重复添加')
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
            report={'passed':['hover explanations cover every Add menu entry, show existing sources and full-capacity reasons, stay outside open menus, support keyboard selection and disappear on leave/close; Cancel preserves configuration'],'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        if labels_only:
            before=cfg.read_bytes();form=designer()
            migrated=['项目名称','主要名称','文件类别','补充说明','当前状态','记录日期']
            assert [n.text(u.GetDlgItem(form,4220+i)) for i in range(6)]==migrated
            button(form,4100);assert items(form,4004)==migrated
            assert n.text(u.GetDlgItem(form,4120))=='自动带入原文件名'
            assert n.text(u.GetDlgItem(form,4044))=='在主面板显示'
            bounds(form);e.capture(form,out/'field-labels.png')
            expected={
                '资产文件':['项目名称','主要名称','文件类别','补充说明','当前状态','日期'],
                '游戏引擎资产':['文件类别','主要名称','补充说明','贴图用途'],
                '贴图资源':['文件类别','主要名称','补充说明','贴图用途'],
                '项目分类与编号':['项目前缀','分类','主要名称','编号']}
            for name,labels in expected.items():
                choose(form,4001,items(form,4001,True).index(name));button(form,4020)
                assert items(form,4004)==labels,(name,items(form,4004))
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            assert cfg.read_bytes()==before, 'reading upgraded labels and cancelling must not write configuration'
            form=designer();save(form);filled('Rock_v001')
            assert all(label in recipe_structure(0) for label in migrated)
            stop();start();filled('Rock_v001')
            form=designer();assert [n.text(u.GetDlgItem(form,4220+i)) for i in range(6)]==migrated
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));filled('Rock_v001')
            e.capture(panel,out/'panel-labels.png')
            report={'passed':['all four built-ins use plain labels; an old saved copy upgrades across editor and restart, preserves custom labels and filename output, and Cancel leaves configuration unchanged'],'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        if rules_only:
            before=cfg.read_bytes();form=designer()
            assert not u.IsWindowVisible(u.GetDlgItem(form,4100))
            add_item(form,capture='unified-rules-menu.png')
            add_item(form,'日期')
            assert u.SendDlgItemMessageW(form,4004,0x18B,0,0)==6
            assert n.text(u.GetDlgItem(form,4040))=='日期' and u.SendDlgItemMessageW(form,4041,0x147,0,0)==0
            button(form,4100)
            edit_content(form,1)
            assert n.text(u.GetDlgItem(form,4020))=='基本设置'
            assert n.text(u.GetDlgItem(form,4022))=='候选与条件'
            assert n.text(u.GetDlgItem(form,4100))=='返回填写'
            assert u.IsWindowVisible(u.GetDlgItem(form,4023))
            assert not u.IsWindowVisible(u.GetDlgItem(form,4045)), 'global primary selector is replaced by the field checkbox'
            assert u.SendDlgItemMessageW(form,4120,0xF0,0,0)==1
            assert '主要名称' in n.text(u.GetDlgItem(form,4121))
            text(form,4040,'主体名称');assert '主体名称' in n.text(u.GetDlgItem(form,4121))
            text(form,4040,'主要名称')
            field(form,0);assert not u.IsWindowEnabled(u.GetDlgItem(form,4007))
            assert not u.IsWindowEnabled(u.GetDlgItem(form,4120))
            assert '未选择项目' in n.text(u.GetDlgItem(form,4125))
            field(form,5);assert not u.IsWindowEnabled(u.GetDlgItem(form,4008))
            field(form,2);text(form,4042,'模型')
            field(form,4);text(form,4042,'工作')
            field(form,1);bounds(form);e.capture(form,out/'rules-basic.png')
            assert '主要名称 ／ 文件类别 ／ 当前状态 ／ 自动版本' in n.text(u.GetDlgItem(form,4124))
            # Move original-name reception to an arbitrary field, then restore it.
            field(form,3);check(form,4120,True)
            assert not u.IsWindowVisible(u.GetDlgItem(form,4042))
            field(form,1);assert u.SendDlgItemMessageW(form,4120,0xF0,0,0)==0
            text(form,4042,'Base')
            assert n.text(u.GetDlgItem(form,4083))=='Base_模型_Rock_工作_v001'
            field(form,3);assert u.SendDlgItemMessageW(form,4120,0xF0,0,0)==1
            choose(form,4041,2);assert not u.IsWindowEnabled(u.GetDlgItem(form,4120))
            assert u.SendDlgItemMessageW(form,4045,0x147,0,0)==0, 'automatic source clears the original-name mapping'
            field(form,1);check(form,4120,True)
            add_item(form,'文件名格式…');assert not u.IsWindowVisible(u.GetDlgItem(form,4020))
            assert '整个文件名' in n.text(u.GetDlgItem(form,4121))
            choose(form,4003,2);choose(form,4090,2);check(form,4092,True)
            button(form,4023);assert u.IsWindowVisible(u.GetDlgItem(form,4120))
            button(form,4100)
            assert not u.IsWindowVisible(u.GetDlgItem(form,4100))
            add_item(form,'文件名格式…')
            assert u.SendDlgItemMessageW(form,4003,0x147,0,0)==2
            assert u.SendDlgItemMessageW(form,4092,0xF0,0,0)==1
            add_item(form,'复制当前方案')
            assert n.text(u.GetDlgItem(form,4000))=='资产文件 副本'
            assert u.IsWindowVisible(u.GetDlgItem(form,4001)) and not u.IsWindowVisible(u.GetDlgItem(form,4100))
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            assert cfg.read_bytes()==before;filled('Rock_v001')
            checks.append('field/global settings are separate; original-name mapping moves through per-field checkboxes; source changes, scope, order boundaries and collapsed drafts stay consistent; Cancel is atomic')

            form=designer();choose(form,4001,1);edit_content(form,3,2)
            assert '当前条件不满足' in n.text(u.GetDlgItem(form,4125))
            e.capture(form,out/'rules-condition-hidden.png')
            edit_content(form,0)
            field(form,0);text(form,4042,'');field(form,1)
            assert '补全其他字段' in n.text(u.GetDlgItem(form,4125))
            field(form,0);text(form,4042,'T');field(form,3)
            assert '贴图用途' in n.text(u.GetDlgItem(form,4124))
            assert '预览中已标出' in n.text(u.GetDlgItem(form,4125))
            button(form,4022);bounds(form);e.capture(form,out/'rules-conditions.png')
            field(form,2);button(form,4022)
            u.PostMessageW(form,0x111,4080,0);options=modal('补充说明 · 常用选项')
            for index in range(3):text(options,200+index,str(index+1))
            save(options)
            button(form,4020);text(form,4042,'1')
            edit_content(form,2,1);text(form,4062,'9');button(form,4023)
            assert u.IsWindowVisible(u.GetDlgItem(form,4062)) and '0–8' in n.text(u.GetDlgItem(form,4084))
            text(form,4062,'2');text(form,4065,'LOD');check(form,4063,True);text(form,4064,'-')
            assert n.text(u.GetDlgItem(form,4083))=='T_Rock-LOD01_BC', n.text(u.GetDlgItem(form,4084))
            bounds(form);e.capture(form,out/'rules-output.png')
            add_item(form,'文件名格式…');choose(form,4003,1);check(form,4092,True)
            assert n.text(u.GetDlgItem(form,4083))=='T_Rock-LOD01_BC_V001'
            bounds(form);e.capture(form,out/'rules-scheme.png')
            add_item(form,['编辑已有内容','调整排列顺序…'])
            button(form,4007);assert n.text(u.GetDlgItem(form,4083))=='T-LOD01_Rock_BC_V001'
            button(form,4008);assert n.text(u.GetDlgItem(form,4083))=='T_Rock-LOD01_BC_V001'
            button(form,4100)
            choose(form,4001,2);choose(form,4001,1);edit_content(form,2,1)
            assert u.IsWindowVisible(u.GetDlgItem(form,4062)), 'each example retains its prior field tab'
            text(form,4000,'规则交互验证');save(form);filled('T_Rock-LOD01_BC_V001')
            # A temporary scheme choice no longer changes the startup default.
            menu.select(panel,app.pid,['设为默认','设为全局默认'],anchor=792)
            stop();start();filled('T_Rock-LOD01_BC_V001')
            form=designer();edit_content(form,2,1)
            assert n.text(u.GetDlgItem(form,4062))=='2' and n.text(u.GetDlgItem(form,4065))=='LOD'
            button(form,4023);assert u.SendDlgItemMessageW(form,4092,0xF0,0,0)==1
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            checks.append('preview composition and conditional omission agree with the naming engine; formatting, reorder, global numbering, validation and persistence work through one draft')
            report={'passed':checks,'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        if catalog_only:
            names={'资产文件','游戏引擎资产','贴图资源','项目分类与编号'}
            before=cfg.read_bytes();manager=manage()
            assert set(items(manager,202))==names
            select_row(manager,'资产文件');u.PostMessageW(manager,0x111,207,0);form=modal('命名方案')
            assert n.text(u.GetDlgItem(form,4000))=='资产文件', 'editing must retain the scheme identity/name'
            templates=items(form,4001,True)
            assert templates[0]=='当前方案 · 资产文件（保留修改）'
            assert set(templates[1:])==names-{'资产文件'}
            assert [n.text(u.GetDlgItem(form,4220+i)) for i in range(6)]==['项目名称','主要名称','文件类别','补充说明','当前状态','日期']
            text(form,4204,'审核');u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            assert cfg.read_bytes()==before, 'viewing built-ins or cancelling must not persist them'
            u.PostMessageW(manager,0x10,0,0);n.wait(lambda:not u.IsWindow(manager))
            checks.append('all built-ins are visible and editable without side effects; asset edit preserves its name and original fields')

            form=designer();assert n.text(u.GetDlgItem(form,4000))=='资产文件'
            text(form,4204,'审核');save(form);filled('Rock_审核_v001')
            assert parser().getint('Jump','recipes')==1 and 'asset-v2' in recipe_structure(0)
            assert menu.select(panel,app.pid,None,anchor=792).count('资产文件')==1
            stop();start();filled('Rock_审核_v001')
            form=designer();assert n.text(u.GetDlgItem(form,4204))=='审核'
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            checks.append('direct edits persist under asset-v2, apply to the current native dialog and survive restart without duplicate menu entries')

            manager=manage();select_row(manager,'游戏引擎资产');u.PostMessageW(manager,0x111,207,0);form=modal('命名方案')
            templates=items(form,4001,True);assert '资产文件' in templates
            choose(form,4001,templates.index('资产文件'))
            assert n.text(u.GetDlgItem(form,4204))=='审核', 'templates must read the saved asset definition'
            u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
            select_row(manager,'资产文件');u.PostMessageW(manager,0x111,254,0);form=modal('命名方案')
            text(form,4000,'Asset Copy');text(form,4204,'交付');save(form)
            assert len(items(manager,202))==5 and items(manager,202).count('资产文件')==1
            assert parser().getint('Jump','recipes')==2
            select_row(manager,'Asset Copy');u.PostMessageW(manager,0x111,255,0)
            n.wait(lambda:not u.IsWindow(manager));filled('Rock_交付_v001')
            menu.select(panel,app.pid,('命名方案','资产文件'));filled('Rock_审核_v001')
            checks.append('editor examples use saved definitions; explicit copies remain independent and share the panel/manager catalog')

            manager=manage();select_row(manager,'资产文件');assert n.text(u.GetDlgItem(manager,208))=='还原初始规则'
            u.SendMessageW(manager,0x111,208,u.GetDlgItem(manager,208))
            select_row(manager,'资产文件');assert not u.IsWindowEnabled(u.GetDlgItem(manager,208))
            assert len(items(manager,202))==5
            e.capture(manager,out/'shared-scheme-catalog.png')
            u.PostMessageW(manager,0x10,0,0);n.wait(lambda:not u.IsWindow(manager))
            n.activate(dialog);filled('Rock_v001')
            stop();start();filled('Rock_v001')
            checks.append('restoring an edited built-in keeps it available, updates the current dialog and survives restart')
            report={'passed':checks,'output':str(out)}
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2));return
        before=cfg.read_bytes();form=designer()
        assert u.IsWindowVisible(u.GetDlgItem(form,4005))
        assert [n.text(u.GetDlgItem(form,4220+i)) for i in range(6)]==['项目名称','主要名称','文件类别','补充说明','当前状态','日期']
        text(form,4000,'Original Draft');text(form,4301,'原始资产');text(form,4203,'重甲');text(form,4204,'审核')
        original_preview=n.text(u.GetDlgItem(form,4083))
        choose(form,4001,1)
        assert n.text(u.GetDlgItem(form,4301))=='Asset'
        assert not u.IsWindowVisible(u.GetDlgItem(form,4224))
        text(form,4000,'Engine Draft');text(form,4202,'B')
        choose(form,4001,1)
        assert n.text(u.GetDlgItem(form,4083))=='SM_Asset_B'
        text(form,4202,'bad/value');choose(form,4001,0)
        assert u.SendDlgItemMessageW(form,4001,0x147,0,0)==1
        assert n.text(u.GetDlgItem(form,4202))=='bad/value' and n.text(u.GetDlgItem(form,4084))
        text(form,4202,'B');add_field(form,'LOD');text(form,4304,'0')
        assert n.text(u.GetDlgItem(form,4083))=='SM_Asset_B_0'
        e.capture(form,out/'designer-add-field.png')
        choose(form,4001,2);assert n.text(u.GetDlgItem(form,4083))=='T_Asset_A_BC'
        choose(form,4001,1)
        assert n.text(u.GetDlgItem(form,4000))=='Engine Draft'
        assert n.text(u.GetDlgItem(form,4224))=='LOD' and n.text(u.GetDlgItem(form,4304))=='0'
        choose(form,4001,0)
        assert n.text(u.GetDlgItem(form,4000))=='Original Draft'
        assert n.text(u.GetDlgItem(form,4083))==original_preview
        assert n.text(u.GetDlgItem(form,4301))=='原始资产'
        assert n.text(u.GetDlgItem(form,4203))=='重甲' and n.text(u.GetDlgItem(form,4204))=='审核'
        assert u.IsWindowVisible(u.GetDlgItem(form,4225)) and not u.IsWindowVisible(u.GetDlgItem(form,4226))
        add_item(form,'自定义内容…');entry=modal('添加命名内容')
        u.PostMessageW(entry,0x111,2,0);n.wait(lambda:not u.IsWindow(entry))
        assert not u.IsWindowVisible(u.GetDlgItem(form,4226))
        add_field(form,'批次');text(form,4306,'01');add_field(form,'输出');text(form,4307,'Main')
        assert u.IsWindowEnabled(u.GetDlgItem(form,4005))
        add_item(form,disabled=['自定义内容…','固定文字…'])
        choose(form,4001,1);assert n.text(u.GetDlgItem(form,4083))=='SM_Asset_B_0'
        choose(form,4001,0);choose(form,4001,0)
        assert n.text(u.GetDlgItem(form,4306))=='01' and n.text(u.GetDlgItem(form,4307))=='Main'
        assert n.text(u.GetDlgItem(form,4083))==original_preview.replace('_v001','_01_Main_v001')
        button(form,4327)
        assert u.IsWindowEnabled(u.GetDlgItem(form,4005)) and not u.IsWindowVisible(u.GetDlgItem(form,4327))
        assert n.text(u.GetDlgItem(form,4306))=='01'
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
        checks.append('example roundtrips retain original fields, unsaved names, defaults and Unicode preview input; each example keeps independent additions; invalid edits block switching without losing input')
        checks.append('direct Add field asks only for a name, focuses editable content, supports custom fields up to eight and cancels without writing configuration')

        form=designer();choose(form,4001,1);text(form,4202,'B')
        button(form,4320)
        assert [n.text(u.GetDlgItem(form,4220+i)) for i in range(3)]==['主要名称','补充说明','贴图用途']
        assert n.text(u.GetDlgItem(form,4083))=='Rock_B_BC'
        assert '仅当' not in n.text(u.GetDlgItem(form,4242))
        field(form,2);button(form,4022)
        assert u.SendDlgItemMessageW(form,4081,0x147,0,0)==0, 'deleted field cannot leave a dangling condition'
        button(form,4100);button(form,4320)
        assert n.text(u.GetDlgItem(form,4083))=='B_BC'
        button(form,4023)
        assert u.SendDlgItemMessageW(form,4045,0x147,0,0)==0, 'deleting the primary field clears original-name mapping'
        button(form,4100);button(form,4320)
        assert n.text(u.GetDlgItem(form,4083))=='BC'
        assert not u.IsWindowEnabled(u.GetDlgItem(form,4320))
        button(form,4320)
        assert n.text(u.GetDlgItem(form,4220))=='贴图用途' and not u.IsWindowVisible(u.GetDlgItem(form,4321))
        choose(form,4001,0);assert u.IsWindowVisible(u.GetDlgItem(form,4325))
        choose(form,4001,1);assert n.text(u.GetDlgItem(form,4083))=='BC'
        choose(form,4001,2);text(form,4300,'')
        assert '不能为空' in n.text(u.GetDlgItem(form,4084))
        button(form,4320);assert n.text(u.GetDlgItem(form,4083))=='Rock_A_BC'
        choose(form,4001,0);bounds(form);e.capture(form,out/'designer-delete-fields.png')
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
        checks.append('fill-in rows delete fields directly, preserve other edits, clear dependent conditions and original-name mappings, retain example drafts and protect the last field; invalid fixed values can be deleted and Cancel remains atomic')
        filled('Rock_v001')

        form=designer()
        assert u.IsWindowVisible(u.GetDlgItem(form,4301))
        assert not any(u.IsWindowVisible(u.GetDlgItem(form,i)) for i in [4004,4020,4021,4022,4023,4041,4090])
        assert not u.IsWindowVisible(u.GetDlgItem(form,4281))
        text(form,4301,'Rock_v007');assert n.text(u.GetDlgItem(form,4083))=='Rock_v008'
        text(form,4301,'Rock');bounds(form);e.capture(form,out/'designer-fill.png')
        button(form,4100)
        assert n.text(u.GetDlgItem(form,4040))=='主要名称', 'start with the primary field'
        assert n.text(u.GetDlgItem(form,4083))=='Rock_v001', 'preview includes default numbering'
        text(form,4093,'Rock_v007');assert n.text(u.GetDlgItem(form,4083))=='Rock_v008'
        text(form,4093,'Rock')
        assert not u.IsWindowVisible(u.GetDlgItem(form,4042))
        field(form,0);assert not u.IsWindowVisible(u.GetDlgItem(form,4042))
        assert n.text(u.GetDlgItem(form,4046))=='自动取当前项目'
        choose(form,4041,2);assert n.text(u.GetDlgItem(form,4046))=='自动取当天日期'
        button(form,4023);choose(form,4091,1)
        assert time.strftime('%Y-%m-%d')+'_Rock_v001'==n.text(u.GetDlgItem(form,4083))
        field(form,0);assert n.text(u.GetDlgItem(form,4047))==time.strftime('%Y-%m-%d')
        choose(form,4001,1);bounds(form);e.capture(form,out/'designer-basic.png')
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_A'
        choose(form,4001,0)
        assert n.text(u.GetDlgItem(form,4083))==time.strftime('%Y-%m-%d')+'_Rock_v001'
        choose(form,4001,1)
        assert not u.IsWindowVisible(u.GetDlgItem(form,4004))
        text(form,4202,'B');assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_B'
        text(form,4202,'A')
        text(form,4000,'My Engine');save(form);filled('SM_Rock_A')
        assert n.text(u.GetDlgItem(panel,790))=='命名设置 ▾'
        assert 'My Engine' in n.text(u.GetDlgItem(panel,792))
        assert all(u.GetDlgItem(panel,i) for i in [840,841,842,831,852])
        assert not any(u.GetDlgItem(panel,i) for i in [788,789,791])
        assert not u.GetDlgItem(panel,858), 'conditional usage must be absent for models'
        command(841);filled('SK_Rock_A');command(842);filled('M_Rock_A')
        # Custom input exposes other types without changing the scheme definition.
        command(830);text(panel,810,'T');filled('T_Rock_A_BC');command(888)
        n.wait(lambda:u.GetDlgItem(panel,858));command(859);filled('T_Rock_A_N')
        assert host.poll() is None
        e.capture(panel,out/'custom-quick.png')
        command(789);assert u.GetDlgItem(panel,783)
        command(788);filled('T_Rock_A_N')
        checks.append('saved custom schemes use generated quick/all-field views, aliases, optional variants and conditional texture channels')
        checks.append('the editor starts with direct fill-in rows and a live preview; rule tabs stay hidden, and trial original names never become defaults')

        form=designer();add_field(form,'LOD');text(form,4304,'0');save(form);filled('T_Rock_A_N_0')
        assert 'LOD' in recipe_structure(0)
        stop();start();filled('SM_Rock_A_0')
        form=designer();assert n.text(u.GetDlgItem(form,4224))=='LOD' and n.text(u.GetDlgItem(form,4304))=='0'
        button(form,4324);save(form);filled('SM_Rock_A')
        stop();start();filled('SM_Rock_A')
        form=designer();assert not u.IsWindowVisible(u.GetDlgItem(form,4324))
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
        command(830);text(panel,810,'T');filled('T_Rock_A_BC');command(888)
        command(859);filled('T_Rock_A_N')
        checks.append('adding and deleting fields directly both persist and compose in the real save dialog and survive restart')

        before=cfg.read_bytes();form=designer()
        u.PostMessageW(form,0x111,4260,0);options=modal('文件类别 · 常用选项')
        text(options,100,'Static mesh');text(options,200,'Static')
        u.PostMessageW(options,0x111,2,0);n.wait(lambda:not u.IsWindow(options))
        assert n.text(u.GetDlgItem(form,4200))=='SM'
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
        checks.append('choice editing is reachable directly from the fill-in page; cancelling the option form leaves the scheme and configuration untouched')

        form=designer();u.PostMessageW(form,0x111,4260,0);options=modal('文件类别 · 常用选项')
        text(options,200,'T');text(options,204,'SM');save(options)
        assert n.text(u.GetDlgItem(form,4083))=='T_Rock_A'
        assert 'SM' in n.text(u.GetDlgItem(form,4243))
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
        checks.append('swapping option outputs updates the original default and dependent conditions once; cancelling the outer draft remains atomic')

        before=cfg.read_bytes();form=designer()
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_A', 'preview uses reusable defaults, not T/N session values'
        text(form,4093,'Tree');button(form,4023);choose(form,4003,1);choose(form,4090,2);check(form,4092,True)
        assert n.text(u.GetDlgItem(form,4083))=='SM_Tree_A_V0001'
        e.capture(form,out/'designer-defaults.png')
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
        assert cfg.read_bytes()==before;filled('T_Rock_A_N')
        checks.append('source-specific fields hide irrelevant defaults; original-name mapping is unique; preview uses defaults, a disposable example name and full numbering format')

        form=designer();field(form,3);button(form,4022)
        e.capture(form,out/'designer-choices.png')
        u.PostMessageW(form,0x111,4080,0);options=modal('贴图用途 · 常用选项')
        button(options,12)
        for index,(label,value) in enumerate([('颜色','BC'),('法线','N'),('通道','ORM'),('细节','Detail')]):
            text(options,100+index,label);text(options,200+index,value)
        text(options,203,'N');button(options,1)
        assert u.IsWindow(options) and '重复' in n.text(u.GetDlgItem(options,300))
        text(options,203,'Detail')
        for index,value in [(4,'Mask'),(5,'Height'),(0,'AO')]:
            button(options,12);text(options,100+index,value)
        assert n.text(u.GetDlgItem(options,13))=='2 / 2'
        button(options,10);assert n.text(u.GetDlgItem(options,105))=='Height'
        bounds(options);e.capture(options,out/'designer-options.png');save(options)
        save(form);filled('T_Rock_A_N')
        assert all(value in recipe_structure(0) for value in ['Mask','Height','AO'])
        checks.append('two-column options need no expression syntax, infer an omitted output, retain edits across pages and reject duplicate outputs')
        command(794);filled('T_Rock_A_N_v001')
        command(142);text(panel,186,'P_');filled('P_T_Rock_A_N_v001');command(142)
        command(116);text(panel,172,'Rock');text(panel,173,'Cliff');filled('P_T_Cliff_A_N_v001');command(116)
        form=designer();button(form,4023)
        assert u.SendDlgItemMessageW(form,4003,0x147,0,0)==0, 'temporary numbering is not a scheme default'
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_A'
        save(form);filled('P_T_Cliff_A_N_v001')
        assert parser()['Recipe0'].getint('counter')==0
        command(779);filled('T_Rock_A_N_v001');command(793);filled('T_Rock_A_N')
        checks.append('editing rules preserves per-file picks, prefix, replacement and temporary numbering without baking them into reusable defaults')
        command(830);text(panel,810,'SM');filled('SM_Rock_A');command(888)
        assert not u.GetDlgItem(panel,858)
        stop();start();filled('SM_Rock_A')
        assert 'My Engine' in n.text(u.GetDlgItem(panel,792))
        checks.append('definitions persist across app restart while per-file values do not become defaults')

        original=original_definition()
        form=designer();button(form,4009);text(form,4000,'Independent Copy')
        field(form,0);text(form,4042,'SK');save(form);filled('SK_Rock_A')
        command(830);text(panel,810,'T');filled('T_Rock_A_BC');command(888)
        candidate=u.GetDlgItem(panel,842)
        assert n.text(candidate)=='T'
        u.PostMessageW(candidate,0x7B,candidate,-1)
        form=modal('重命名候选');text(form,100,'TX');save(form);filled('TX_Rock_A_BC')
        assert original_definition()==original, 'editing a copy must preserve its source definition'
        stop();start();filled('SK_Rock_A')
        command(830);text(panel,810,'TX');filled('TX_Rock_A_BC');command(888)
        checks.append('a direct scheme copy is independent; right-click renaming updates its conditional references and persists without changing the original')

        command(830);text(panel,810,'HistoryOnly');filled('HistoryOnly_Rock_A');command(888)
        n.activate(dialog);n.click(dialog,ident=1);host.wait(timeout=10)
        assert (out/'HistoryOnly_Rock_A.txt').is_file()
        stop();start();filled('SK_Rock_A')
        form=designer();save(form);filled('SK_Rock_A')
        assert 'HistoryOnly' not in recipe_structure(1)
        candidate=u.GetDlgItem(panel,841);u.PostMessageW(candidate,0x7B,candidate,-1)
        form=modal('重命名候选');text(form,100,'SK_Copy');save(form);filled('SK_Copy_Rock_A')
        assert 'HistoryOnly' not in recipe_structure(1)
        checks.append('opening the designer and renaming an existing candidate do not copy temporary saved-file history into the definition')

        form=designer();button(form,4009);text(form,4000,'Numbered Copy')
        choose(form,4001,3);text(form,4000,'Numbered Copy')
        text(form,4300,'DEMO');assert n.text(u.GetDlgItem(form,4083))=='DEMO_Prop_Rock_001'
        field(form,0);choose(form,4041,3)
        field(form,3);button(form,4021);text(form,4065,'LOD');text(form,4062,'2')
        # Separate joiner with a dash.
        u.SendDlgItemMessageW(form,4063,0xF1,1,0);button(form,4063);text(form,4064,'-')
        e.capture(form,out/'designer-output.png')
        assert n.text(u.GetDlgItem(form,4083))=='DEMO_Prop_Rock-LOD01'
        save(form);filled('DEMO_Prop_Rock-LOD01')
        command(833);text(panel,813,'2');filled('DEMO_Prop_Rock-LOD02')
        text(panel,813,'bad');time.sleep(.2)
        assert n.text(u.GetDlgItem(panel,101))=='DEMO_Prop_Rock-LOD02.txt'
        assert '数字' in n.text(u.GetDlgItem(panel,114))
        u.SendMessageW(u.GetDlgItem(panel,813),0x100,27,0);filled('DEMO_Prop_Rock-LOD01')
        checks.append('a copied alternative rule preserves the original and supports fixed text, per-field separators, padded numbers and invalid input recovery')

        form=designer();field(form,3);button(form,4007);save(form)
        filled('DEMO_Prop-LOD01_Rock')  # Moving the numeric field moves its custom joiner with it.
        # Restore the rule after verifying reorder. No numeric field is assumed to be last.
        form=designer();field(form,2);button(form,4008);save(form);filled('DEMO_Prop_Rock-LOD01')
        checks.append('field reordering changes the output without depending on hard-coded game field names')

        form=designer();button(form,4023);choose(form,4003,1);choose(form,4090,2);choose(form,4091,1);check(form,4092,True)
        assert n.text(u.GetDlgItem(form,4083))=='DEMO_Prop_Rock-LOD01_V0001'
        save(form);filled('DEMO_Prop_Rock-LOD01_V0001')
        stop();start();filled('DEMO_Prop_Rock-LOD01_V0001')
        form=designer();button(form,4023)
        assert u.SendDlgItemMessageW(form,4003,0x147,0,0)==1
        assert u.SendDlgItemMessageW(form,4090,0x147,0,0)==2
        assert u.SendDlgItemMessageW(form,4091,0x147,0,0)==1
        assert u.SendDlgItemMessageW(form,4092,0xF0,0,0)==1
        choose(form,4003,0);choose(form,4090,1);choose(form,4091,0);check(form,4092,False)
        save(form);filled('DEMO_Prop_Rock-LOD01')
        checks.append('scheme numbering, digits, version case and date format preview, apply and survive restart independently of per-file overrides')

        before=cfg.read_bytes();form=designer();text(form,4000,'Cancelled');field(form,0);add_field(form,'新字段')
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
        assert cfg.read_bytes()==before
        filled('DEMO_Prop_Rock-LOD01')
        form=designer();field(form,3);button(form,4021);text(form,4062,'9');button(form,1)
        assert u.IsWindow(form) and '0–8' in n.text(u.GetDlgItem(form,4084))
        u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form))
        checks.append('Cancel leaves the persisted configuration unchanged; malformed rules keep the draft open')

        stop();start();filled('DEMO_Prop_Rock-LOD01')
        menu.select(panel,app.pid,('命名方案','My Engine'));filled('SM_Rock_A')
        menu.select(panel,app.pid,('命名方案','Numbered Copy'));filled('DEMO_Prop_Rock-LOD01')
        checks.append('saved schemes switch through the same Naming settings menu without losing the primary asset name')
        # A literal native filename is respected even with required/default rules.
        native=next(h for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit' and n.text(h)=='DEMO_Prop_Rock-LOD01')
        buffer=c.create_unicode_buffer('Literal_Custom');u.SendMessageW(native,0xC,0,c.addressof(buffer))
        filled('Literal_Custom');assert u.GetDlgItem(panel,790)
        command(830);filled('Literal_Custom')
        assert not u.GetDlgItem(panel,810), 'fixed fields must remain fixed after a literal native edit'
        command(846);filled('DEMO_Prop_Literal_Custom-LOD01')
        checks.append('native manual edits remain literal and keep quick naming available; a later field action resumes the configured defaults')
        n.activate(dialog);n.click(dialog,ident=1);host.wait(timeout=10)
        assert (out/'DEMO_Prop_Literal_Custom-LOD01.txt').is_file()
        schemes=[s for s in parser().sections() if s.startswith('Recipe')]
        assert len(schemes)==3
        report={'passed':checks,'output':str(out)}
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        print('Failure output: '+str(out),flush=True)
        if (catalog_only or rules_only or labels_only or help_only or assist_only or preview_only or controls_only) and app:
            print('Panels:',[(h,bool(u.IsWindowVisible(h)),n.text(u.GetDlgItem(h,101))) for h in n.windows(app.pid,'Jump.Panel')],flush=True)
            print('Current panel:',panel,n.text(u.GetDlgItem(panel,101)),flush=True)
            print('Native edits:',[(h,n.text(h)) for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit'],flush=True)
        if panel and u.IsWindowVisible(panel):e.capture(panel,out/'failure.png')
        for h in n.dialogs(app.pid) if app else []:
            if n.text(h)=='命名方案':e.capture(h,out/'designer-failure.png')
        raise
    finally:stop()

if __name__=='__main__':
    args=argparse.ArgumentParser(description=__doc__)
    args.add_argument('--catalog-only',action='store_true',help='Shared built-in catalog, editing, copy, persistence and reset')
    args.add_argument('--rules-only',action='store_true',help='Advanced field/global settings, original-name mapping, preview and draft persistence')
    args.add_argument('--labels-only',action='store_true',help='Plain labels in built-ins, saved copies and editor; preserve custom labels and filename output')
    args.add_argument('--help-only',action='store_true',help='Add-menu hover descriptions, examples, existing items, full capacity and dismissal')
    args.add_argument('--assist-only',action='store_true',help='Multi-name trial, conditional directory recommendations and same-file preflight')
    args.add_argument('--preview-only',action='store_true',help='Beginner-friendly filename comparisons, examples, empty state and optional explanation')
    args.add_argument('--controls-only',action='store_true',help='Visible field settings, preview-to-rule links, independent scheme switches and persistence')
    opts=args.parse_args();run(opts.catalog_only,opts.rules_only,opts.labels_only,opts.help_only,opts.assist_only,opts.preview_only,opts.controls_only)
