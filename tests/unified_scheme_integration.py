"""Unified scheme drafts: inline choices, dependent rules, defaults and persistence."""
import configparser
import ctypes as c
import hashlib
import json
import subprocess
import uuid
import native_integration as n
import extended_integration as e
import naming_menu as menu
from naming_fixture import pack, encode

u=n.u

def unpack(text):
    result=[]
    while text:
        size,tail=text.split(':',1);size=int(size);result.append(tail[:size]);text=tail[size:]
    return result

def run():
    out=n.ROOT/'tests/output/unified-scheme'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    cfg=out/'settings.ini';dev=n.ROOT/'build/dev/settings.ini';dev_hash=hashlib.sha256(dev.read_bytes()).digest()
    def field(ident,label,default='',choices=(),condition=('', '')):
        meta=pack([default,'','','',*condition,'1','0','0','0','0','0'])
        return pack([ident,label,default,'0','0',meta]+[pack([label,value,'keep-key']) for label,value in choices])
    definition=pack(['JNS2','unified-fixture','Unified fixture','_','1','0','0','0','3','0','0',pack(['subject','0','']),
        field('kind','类别','A',[('Alpha','A'),('Beta','B'),('Gamma','C'),('Delta','D')]),
        field('subject','主要名称'),field('tail','条件项','Extra',condition=('kind','A'))])
    seed=configparser.ConfigParser();seed['Jump']={'schema':'1','groups':'0','Favorite':'0','History':'0','recipes':'1',
        'defaultNamingScheme':encode('asset-v2'),'applicationNamingSchemes':'1'}
    seed['Recipe0']={'name':encode('Unified fixture'),'value':encode('Asset'),'namingOnly':'1','complete':'1','structure':encode(definition)}
    seed['ApplicationNamingScheme0']={'executable':encode('jump.testhost.exe'),'scheme':encode('unified-fixture')}
    with cfg.open('w',encoding='utf-16') as file:seed.write(file)
    app=manager=host=None;checks=[];current_field=0
    def config():
        p=configparser.ConfigParser();p.read(cfg,encoding='utf-16');return p
    def decode(p,section,key):return bytes.fromhex(p.get(section,key,fallback='')).decode('utf-16-be')
    def cmd(window,ident):u.SendMessageW(window,0x111,ident,u.GetDlgItem(window,ident) or 0)
    def post(window,ident):u.PostMessageW(window,0x111,ident,u.GetDlgItem(window,ident) or 0)
    def input_combo(form,ident,value):
        e.set_field(form,ident,value)
        u.SendMessageW(form,0x111,ident|(5<<16),u.GetDlgItem(form,ident))
    def select(window,ident,index,combo=True):
        nonlocal current_field
        if ident==4004:
            current_field=index;overview(window);cmd(window,4340+index)
            n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(window,4040)));return
        control=u.GetDlgItem(window,ident);u.SendMessageW(control,0x14e if combo else 0x186,index,0)
        u.SendMessageW(window,0x111,ident|(1<<16),control)
    def start():
        nonlocal app,manager
        app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--manage','--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
        manager=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None));n.activate(manager)
    def stop():
        if app and app.poll() is None:
            for control in n.windows(app.pid,'Jump.Control'):u.PostMessageW(control,0x10,0,0)
            try:app.wait(timeout=5)
            except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)
    def editor(button=207):
        listing=u.GetDlgItem(manager,202);count=u.SendMessageW(listing,0x18b,0,0)
        names=[]
        for index in range(count):
            text=c.create_unicode_buffer(256);u.SendMessageW(listing,0x189,index,c.addressof(text));names.append(text.value)
        index=next(i for i,text in enumerate(names) if 'Unified fixture' in text)
        select(manager,202,index,False);post(manager,button)
        return n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
    def overview(form):
        if not u.IsWindowVisible(u.GetDlgItem(form,4340)):
            cmd(form,4170);n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4340)))
    def popup(form,ident,label):
        n.activate(manager);r=e.rect(manager);u.SetCursorPos(r.left+4,r.top+4);post(form,ident)
        popup_window=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
        handle=u.SendMessageW(popup_window,0x1e1,0,0);owner,index=menu.find(handle,label)
        menu.select_item(popup_window,owner,index)
        n.wait(lambda:not n.windows(app.pid,'#32768',True))
    def section(form,ident,visible):
        if ident==4170:overview(form);visible=4340
        elif ident in (4020,4021,4022):
            overview(form);cmd(form,(4260 if ident==4022 else 4340)+current_field)
            if ident==4021:cmd(form,4021)
        else:cmd(form,ident)
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,visible)))
        assert u.IsWindowVisible(form) and u.IsChild(manager,form)
        assert not n.windows(app.pid,'#32770',True)
        for item in [4170,4530,4172,1,2,4083]:assert u.IsWindowVisible(u.GetDlgItem(form,item)),item
        assert not u.IsWindowVisible(u.GetDlgItem(form,4190)), 'advanced functions must have task-specific entrances'
        assert not u.IsWindowVisible(u.GetDlgItem(form,4004)), 'old navigation stays hidden'
    def close(form,save=False):post(form,1 if save else 2);n.wait(lambda:not u.IsWindow(form))
    def add(form,label):
        n.activate(manager);post(form,4005)
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
        handle=u.SendMessageW(popup,0x1e1,0,0);owner,index=menu.find(handle,label)
        assert not any(value in menu.labels(handle) for value in ['文件名格式…','目录推荐…','关联软件…','编辑已有内容'])
        menu.select_item(popup,owner,index)
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4040)) or u.IsWindowVisible(u.GetDlgItem(form,4343)))
        assert not n.windows(app.pid,'#32770',True)
    def edits(form):
        # Rename a candidate whose value is used by a default, condition and location rule.
        select(form,4004,0,False);section(form,4022,4500)
        e.set_field(form,4500,'Renamed Alpha');e.set_field(form,4510,'AX')
        cmd(form,4181);assert n.text(u.GetDlgItem(form,4500))=='Delta'
        cmd(form,4180);assert n.text(u.GetDlgItem(form,4510))=='AX'
        section(form,4020,4040);assert n.text(u.GetDlgItem(form,4042))=='AX'
        select(form,4004,2,False);cmd(form,4540)
        assert u.IsWindowVisible(u.GetDlgItem(form,4082))
        assert not u.IsWindowVisible(u.GetDlgItem(form,4500))
        assert n.text(u.GetDlgItem(form,4082))=='AX'
        section(form,4171,4700)
        select(form,4701,0);e.set_field(form,4702,'AX');e.set_field(form,4703,'Models')
        section(form,4172,4141)
        # One checkbox and the association list share the same final Save/Cancel.
        u.SendMessageW(u.GetDlgItem(form,4173),0xf1,1,0);cmd(form,4173)
        cmd(form,4144)
        overview(form);e.set_field(form,4191,'-')
        section(form,4170,4004);select(form,4004,1,False)
        assert 'AX-Asset-Extra'==n.text(u.GetDlgItem(form,4083))
    try:
        start();baseline=config();form=editor()
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        original=n.text(u.GetDlgItem(form,4000));e.set_field(form,4000,original+' edit')
        assert n.text(u.GetDlgItem(form,4868))=='有未保存的修改'
        e.set_field(form,4000,original);assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        cmd(form,4530);e.set_field(form,4093,'PreviewOnly');input_combo(form,4800,'B')
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改', 'simulation values must not mark the reusable draft dirty'
        cmd(form,4533);e.set_field(form,4093,'Asset');cmd(form,4172)
        for value,state in [(1,'有未保存的修改'),(0,'尚无修改')]:
            u.SendMessageW(u.GetDlgItem(form,4173),0xf1,value,0);cmd(form,4173)
            assert n.text(u.GetDlgItem(form,4868))==state, 'usage scope changes participate in dirty comparison and undo'
        cmd(form,4170)
        checks.append('the fixed footer tracks actual rule and scope edits, clears when edits are reverted, and excludes all simulation input')
        assert u.IsWindowVisible(u.GetDlgItem(form,4191)) and u.IsWindowVisible(u.GetDlgItem(form,4192))
        e.capture(manager,out/'simple-overview.png')
        # A first-time user can edit common values and see the result without a rule page.
        e.set_field(form,4200,'B');e.set_field(form,4191,'-');select(form,4192,1)
        assert n.text(u.GetDlgItem(form,4083)).startswith('B-Asset') and n.text(u.GetDlgItem(form,4083)).endswith('v001')
        cmd(form,4531);template=u.GetDlgItem(form,4001);template_names=[]
        for i in range(u.SendMessageW(template,0x146,0,0)):
            value=c.create_unicode_buffer(256);u.SendMessageW(template,0x148,i,c.addressof(value));template_names.append(value.value)
        game=next(i for i,name in enumerate(template_names) if name.startswith('游戏引擎资产 ·'));select(form,4001,game)
        assert not u.IsWindowEnabled(u.GetDlgItem(form,1)), 'previewing a template must not allow an implicit save'
        cmd(form,4536)
        assert n.text(u.GetDlgItem(form,4083))=='SM_Asset_A'
        e.set_field(form,4202,'Z');cmd(form,4531);select(form,4001,0);cmd(form,4536)
        assert n.text(u.GetDlgItem(form,4200))=='B' and n.text(u.GetDlgItem(form,4191))=='-'
        cmd(form,4531);select(form,4001,game);cmd(form,4536);assert n.text(u.GetDlgItem(form,4202))=='Z'
        cmd(form,4530);input_combo(form,4800,'T')
        assert n.text(u.GetDlgItem(form,4083))=='T_Asset_Z_BC'
        input_combo(form,4800,'SM');assert n.text(u.GetDlgItem(form,4083))=='SM_Asset_Z'
        cmd(form,4531);cmd(form,4537);close(form)
        assert config()['Recipe0']==baseline['Recipe0']
        checks.append('the compact overview edits common values, separators and numbering directly; examples retain separate drafts; cancelling preserves the original scheme')
        form=editor();cmd(form,4531)
        template=u.GetDlgItem(form,4001);select(form,4001,2)
        e.capture(manager,out/'template-preview.png')
        assert 'SM_Asset_v001'==n.text(u.GetDlgItem(form,4083))
        assert not u.IsWindowEnabled(u.GetDlgItem(form,1));cmd(form,4536)
        cmd(form,4260);assert n.text(u.GetDlgItem(form,4500))=='静态模型' and n.text(u.GetDlgItem(form,4510))=='SM'
        cmd(form,4170);cmd(form,4530);e.set_field(form,4093,'Rock')
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_v001'
        assert '示例' in n.text(u.GetDlgItem(form,4125))
        e.set_field(form,4093,'Rock.fbx');assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_v001.fbx'
        e.set_field(form,4093,'Rock')
        e.capture(manager,out/'starter-simulation.png');cmd(form,4170);e.capture(manager,out/'default-page.png')
        cmd(form,4340);e.capture(manager,out/'part-details.png');cmd(form,4172);e.capture(manager,out/'usage-scope.png')
        assert '优先' in n.text(u.GetDlgItem(form,4196))
        assert '手动使用' in n.text(u.GetDlgItem(form,4535))
        close(form);assert config()['Recipe0']==baseline['Recipe0']
        checks.append('the explicit starter creates category_original_v001; display names map to SM; simulation numbering is labeled; usage scope states precedence and manual entry')
        form=editor();select(form,4004,0,False);section(form,4021,4060)
        select(form,4060,2);select(form,4061,1)
        e.set_field(form,4065,'pre');e.set_field(form,4066,'post')
        u.SendMessageW(u.GetDlgItem(form,4063),0xf1,1,0);cmd(form,4063);e.set_field(form,4064,'-')
        overview(form);assert '文字处理' in n.text(u.GetDlgItem(form,4240))
        section(form,4021,4060)
        assert n.text(u.GetDlgItem(form,4065))=='pre' and n.text(u.GetDlgItem(form,4066))=='post'
        assert n.text(u.GetDlgItem(form,4064))=='-' and u.SendMessageW(u.GetDlgItem(form,4060),0x147,0,0)==2
        e.capture(manager,out/'optional-format.png');close(form)
        checks.append('existing output formatting remains editable on demand and survives returning to the overview without rewriting values')
        form=editor()
        for label,caption,value in [('自定义内容…','LOD','7'),('固定文字…','固定文字','FX')]:
            add(form,label);select(form,4004,3,False);e.set_field(form,4040,caption);e.set_field(form,4042,value)
            assert n.text(u.GetDlgItem(form,4083))=='A_Asset_Extra_'+value
            overview(form);cmd(form,4323);n.wait(lambda:not u.IsWindowVisible(u.GetDlgItem(form,4343)))
        checks.append('Add contains only fields; custom and fixed content edit in place without another form; removal restores the original composition')
        close(form)
        # Three independent states: definition defaults, simulation overrides, real save inputs.
        form=editor();cmd(form,4530)
        assert u.IsWindowVisible(u.GetDlgItem(form,4800)) and not u.IsWindowVisible(u.GetDlgItem(form,4191))
        e.set_field(form,4093,'Rock');input_combo(form,4800,'T')
        assert n.text(u.GetDlgItem(form,4083))=='T_Rock'
        assert '不符合添加条件' in n.text(u.GetDlgItem(form,4534))
        assert '本次模拟输入' in n.text(u.GetDlgItem(form,4534))
        e.capture(manager,out/'simulate-save.png')
        overview(form);assert n.text(u.GetDlgItem(form,4200))=='A'
        assert n.text(u.GetDlgItem(form,4083))=='T_Rock', 'simulation survives switching tasks without rewriting defaults'
        close(form,True)
        n.wait(lambda:'directory' in config()['Recipe0'],label='explicit scheme save flushed before the cancel baseline')
        assert decode(config(),'Recipe0','structure')==decode(baseline,'Recipe0','structure'), 'simulation must not alter the serialized definition'
        baseline=config()
        form=editor();assert n.text(u.GetDlgItem(form,4200))=='A'
        cmd(form,4530);assert n.text(u.GetDlgItem(form,4800))=='Alpha → A'
        cmd(form,4533);assert 'Extra' in n.text(u.GetDlgItem(form,4083))
        close(form)
        checks.append('simulation uses the production composer, explains conditional omissions, survives task switches, and never persists as scheme defaults')

        form=editor();cmd(form,4340);cmd(form,4021);e.set_field(form,4062,'9')
        assert '补零位数' in n.text(u.GetDlgItem(form,4084)) and u.IsWindowVisible(u.GetDlgItem(form,4541))
        e.capture(manager,out/'rule-error.png');cmd(form,4541)
        assert u.IsWindowVisible(u.GetDlgItem(form,4062));e.set_field(form,4062,'0')
        assert '规则需要修正' != n.text(u.GetDlgItem(form,4083))
        cmd(form,4100);cmd(form,4260)
        assert not u.IsWindowVisible(u.GetDlgItem(form,4081)), 'choices and conditions are separate tasks'
        close(form)
        checks.append('invalid advanced rules name the affected part and provide direct correction; choices and conditions have independent task pages')

        form=editor();cmd(form,4340);cmd(form,4540);select(form,4081,2);input_combo(form,4082,'Extra')
        assert '循环' in n.text(u.GetDlgItem(form,4084)) and '类别' in n.text(u.GetDlgItem(form,4084))
        cmd(form,1);assert u.IsWindow(form);cmd(form,4541)
        assert u.IsWindowVisible(u.GetDlgItem(form,4081))
        select(form,4081,0);assert n.text(u.GetDlgItem(form,4083))=='A_Asset_Extra'
        close(form)
        checks.append('category T enables texture usage while SM omits it; circular conditions block saving and can be located and repaired without closing the draft')

        form=editor();overview(form);cmd(form,4555)
        assert '主要名称' in n.text(u.GetDlgItem(form,4220))
        cmd(form,4546);assert '类别' in n.text(u.GetDlgItem(form,4220))
        close(form)
        checks.append('visible row actions reorder content without a generic menu and cancel restores the original order')

        form=editor();edits(form);e.capture(manager,out/'unified-fields.png');close(form)
        after=config();assert after['Recipe0']==baseline['Recipe0'];assert after['Jump']['defaultnamingscheme']==baseline['Jump']['defaultnamingscheme']
        assert after['ApplicationNamingScheme0']==baseline['ApplicationNamingScheme0']
        checks.append('inline options, paging, dependent defaults/conditions, directory rules and software/default changes cancel together without persistence')
        form=editor();edits(form)
        section(form,4171,4700);select(form,4700,0,False)
        assert n.text(u.GetDlgItem(form,4702))=='AX' and n.text(u.GetDlgItem(form,4703))=='Models'
        e.capture(manager,out/'unified-locations.png');close(form,True)
        n.wait(lambda:decode(config(),'Jump','defaultNamingScheme')=='unified-fixture')
        assert config().getint('Jump','applicationNamingSchemes')==0
        stop();start();form=editor(256)
        assert u.IsWindowVisible(u.GetDlgItem(form,4173)) and u.SendMessageW(u.GetDlgItem(form,4173),0xf0,0,0)==1
        e.capture(manager,out/'unified-applications.png')
        section(form,4170,4004);select(form,4004,0,False);section(form,4022,4500)
        assert n.text(u.GetDlgItem(form,4500))=='Renamed Alpha' and n.text(u.GetDlgItem(form,4510))=='AX'
        e.capture(manager,out/'unified-choices.png')
        # Rename again after restart; directory and field dependencies must follow together.
        e.set_field(form,4510,'AZ');section(form,4171,4700);select(form,4700,0,False)
        assert n.text(u.GetDlgItem(form,4702))=='AZ'
        close(form,True)
        checks.append('one Save persists all sections; restart restores defaults and software removals; later candidate rename also updates directory conditions')
        form=editor();section(form,4171,4700)
        cmd(form,4707);e.set_field(form,4702,'unfinished');cmd(form,1)
        n.wait(lambda:'请填完' in n.text(u.GetDlgItem(form,4084)))
        assert u.IsWindow(form);cmd(form,4705)
        section(form,4170,4004);select(form,4004,0,False);section(form,4022,4500)
        e.set_field(form,4510,'B');cmd(form,1)
        n.wait(lambda:'重复' in n.text(u.GetDlgItem(form,4084)))
        e.set_field(form,4510,'AZ');section(form,4172,4141);overview(form);cmd(form,4009)
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4340)))
        assert u.SendMessageW(u.GetDlgItem(form,4173),0xf0,0,0)==0
        assert n.text(u.GetDlgItem(form,4141))=='未关联软件';close(form)
        checks.append('incomplete directory rules and duplicate choices block saving; copies do not inherit global/software defaults')
        host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(out),'--name','Rock.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None));n.activate(dialog)
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None))
        n.wait(lambda:n.panel_ready(panel) and n.text(u.GetDlgItem(panel,101))=='AZ-Rock-Extra.txt')
        def panel_command(ident):
            n.wait(lambda:n.panel_ready(panel));n.activate(panel);u.SendMessageW(panel,0x111,ident,u.GetDlgItem(panel,ident) or 0)
        def filled(value):n.wait(lambda:n.panel_ready(panel) and n.text(u.GetDlgItem(panel,101))==value+'.txt',label='real filename '+value)
        def subject(value):
            panel_command(831);n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(panel,811)))
            e.set_field(panel,811,value);u.PostMessageW(u.GetDlgItem(panel,811),0x100,13,0)
            n.wait(lambda:not u.GetDlgItem(panel,811))
        subject('Cliff');filled('AZ-Cliff-Extra')
        panel_command(142);e.set_field(panel,186,'P_');filled('P_AZ-Cliff-Extra');panel_command(142)
        panel_command(116);e.set_field(panel,172,'Cliff');e.set_field(panel,173,'Stone');filled('P_AZ-Stone-Extra');panel_command(116)
        panel_command(794);filled('P_AZ-Stone-Extra_v001')
        menu.select(panel,app.pid,'方案设置…');form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        cmd(form,4530);e.set_field(form,4093,'SimulationOnly');input_combo(form,4800,'T')
        close(form,True);filled('P_AZ-Stone-Extra_v001')
        panel_command(779);filled('AZ-Cliff-Extra_v001')
        panel_command(793);filled('AZ-Cliff-Extra');subject('Rock');filled('AZ-Rock-Extra')
        checks.append('editing and simulating a scheme preserves the real per-file subject, prefix, replacement and temporary numbering; none becomes a reusable default')
        menu.select(panel,app.pid,'编辑常用选项…',anchor=820)
        form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        assert u.IsWindowVisible(u.GetDlgItem(form,4500)) and n.text(u.GetDlgItem(form,4510))=='AZ'
        close(form);n.activate(dialog)
        menu.select(panel,app.pid,'方案设置…');form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        close(form,True);n.wait(lambda:n.panel_ready(panel))
        n.activate(dialog);u.PostMessageW(dialog,0x111,1,0);host.wait(timeout=10)
        assert (out/'AZ-Rock-Extra.txt').is_file()
        checks.append('both panel settings and candidate editing open the same editor; saved global default composes and writes the expected filename through the owned native save dialog')
        # Actually create, persist and use the novice starter after a full restart.
        stop();start();post(manager,210)
        form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        assert u.IsWindowVisible(u.GetDlgItem(form,4001))
        select(form,4001,2);cmd(form,4536);e.set_field(form,4000,'Novice starter')
        cmd(form,4172);u.SendMessageW(u.GetDlgItem(form,4173),0xf1,1,0);cmd(form,4173)
        close(form,True)
        n.wait(lambda:config().getint('Jump','recipes')==2 and decode(config(),'Jump','defaultNamingScheme')!='unified-fixture')
        starter_id=decode(config(),'Jump','defaultNamingScheme');stop();start()
        target=out/'starter-save';target.mkdir()
        host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(target),'--name','Rock.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host);dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None));n.activate(dialog)
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None))
        n.wait(lambda:n.panel_ready(panel) and n.text(u.GetDlgItem(panel,101))=='SM_Rock_v001.txt')
        assert decode(config(),'Jump','defaultNamingScheme')==starter_id
        # The main panel still needs only the original application's Save action.
        n.activate(dialog);u.PostMessageW(dialog,0x111,1,0);host.wait(timeout=10)
        assert (target/'SM_Rock_v001.txt').is_file()
        checks.append('a newly adopted starter persists as a global default, survives restart, and saves SM_Rock_v001.txt without any additional main-panel steps')
        assert hashlib.sha256(dev.read_bytes()).digest()==dev_hash
        report={'passed':checks,'output':str(out)};(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        print('OUTPUT',out,flush=True)
        for window in n.windows(app.pid,'Jump.Manager',True) if app else []:e.capture(window,out/'failure.png')
        for window in n.windows(app.pid,'Jump.Panel',True) if app else []:
            print('PANEL NAME',n.text(u.GetDlgItem(window,101)),flush=True);e.capture(window,out/'failure-panel.png')
        raise
    finally:
        stop()
        if host and host.poll() is None:host.terminate();host.wait(timeout=5)

if __name__=='__main__':run()
