"""Current scheme/workflow management, transfer and real Save using owned fixtures."""
import configparser
import ctypes as c
import json
import subprocess
import uuid
import native_integration as n
import extended_integration as e
import naming_fixture

u=n.u

def run():
    out=n.ROOT/'tests/output/workflow'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    target=out/'delivery';target.mkdir()
    cfg=out/'settings.ini';cfg.write_text('[Jump]\nschema=1\nrecipes=0\nFavorite=3\n',encoding='utf-16')
    naming_fixture.install(cfg)
    p=configparser.ConfigParser();p.read(cfg,encoding='utf-16')
    for i in range(3):p['Favorite'+str(i)]={'name':naming_fixture.encode('位置'+str(i)),'path':naming_fixture.encode(str(out))}
    with cfg.open('w',encoding='utf-16') as file:p.write(file)
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
    host=None;checks=[]
    def read():
        p=configparser.ConfigParser();p.read(cfg,encoding='utf-16');return p
    def decode(value):return bytes.fromhex(value).decode('utf-16-be')
    def recipes():
        p=read();return [(decode(p['Recipe'+str(i)]['name']),dict(p['Recipe'+str(i)])) for i in range(p.getint('Jump','recipes'))]
    def command(window,ident):u.SendMessageW(window,0x111,ident,u.GetDlgItem(window,ident) or 0)
    def modal(title):return n.wait(lambda:next((h for h in n.dialogs(app.pid,title)),None),label=title)
    def save(form):
        u.PostMessageW(form,0x111,1,0);n.wait(lambda:not u.IsWindow(form))
    def notice():
        form=modal('Jump');button=next(h for h in n.children(form) if n.text(h,u.GetClassNameW)=='Button')
        u.PostMessageW(form,0x111,u.GetDlgCtrlID(button),button);n.wait(lambda:not u.IsWindow(form))
    def filled(value):
        n.wait(lambda:n.text(u.GetDlgItem(panel,101))==value+'.txt' and n.panel_ready(panel) and
               any(n.text(h) in (value,value+'.txt') for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit'),label=value)
    def manage():
        n.activate(panel);command(panel,112)
        return n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None))
    def close(manager):
        u.PostMessageW(manager,0x10,0,0);n.wait(lambda:not u.IsWindow(manager));n.activate(dialog);n.wait(lambda:u.IsWindowVisible(panel))
    def tab(manager,index):
        n.manager_tab(manager,index)
    def select(manager,index):
        control=u.GetDlgItem(manager,202);u.SendMessageW(control,0x186,index,0)
        u.SendMessageW(manager,0x111,202|(1<<16),control)
    def edit(manager,ident,name,prefix):
        u.PostMessageW(manager,0x111,ident,0);form=modal('命名方案')
        assert n.text(u.GetDlgItem(form,1))=='保存方案'
        e.set_field(form,4000,name);command(form,4100);command(form,4021);e.set_field(form,4065,prefix);save(form)
    def file_choice(path):
        form=n.wait(lambda:next(iter(n.windows(app.pid,'#32770',True)),None))
        control=u.GetDlgItem(form,1152)
        if not control:
            combo=u.GetDlgItem(form,1148)
            control=next(h for h in n.children(combo or form) if n.text(h,u.GetClassNameW)=='Edit' and u.IsWindowVisible(h))
        value=c.create_unicode_buffer(str(path));u.SendMessageW(control,0xC,0,c.addressof(value));u.PostMessageW(form,0x111,1,0)
    try:
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(out),'--name','Rock.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host);dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None));n.activate(dialog)
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None));filled('Rock')
        command(panel,142);e.set_field(panel,186,'P_');filled('P_Rock');command(panel,142)
        u.PostMessageW(panel,0x111,707,0);form=modal('保存流程');e.set_field(form,100,'Flow');save(form);notice()
        original_flow=recipes()[1][1]
        manager=manage();assert u.SendMessageW(u.GetDlgItem(manager,200),0x1304,0,0)==9
        assert not any(u.GetDlgItem(manager,i) for i in [201,205,213,217])
        before=cfg.read_bytes();u.PostMessageW(manager,0x111,207,0);form=modal('命名方案')
        e.set_field(form,4000,'Discard');u.PostMessageW(form,0x111,2,0);n.wait(lambda:not u.IsWindow(form));assert cfg.read_bytes()==before
        edit(manager,207,'Main','X_');close(manager);filled('P_X_Rock')
        checks.append('scheme management uses the new designer; Cancel is atomic; editing the active definition updates its session while preserving affixes')
        manager=manage();edit(manager,254,'Alt','Y_');assert decode(read()['Jump']['activeNamingScheme'])=='adapter-fixture'
        select(manager,1);command(manager,209);assert [name for name,_ in recipes()]==['Alt','Flow','Main']
        assert recipes()[1][1]==original_flow
        schemes=[recipes()[0][1],recipes()[2][1]]
        tab(manager,2);assert not any(u.GetDlgItem(manager,i) for i in [210,225,226,227])
        before=cfg.read_bytes();e.set_field(manager,203,'Main');u.PostMessageW(manager,0x111,207,0);notice();assert cfg.read_bytes()==before
        e.set_field(manager,203,'Flow edited');e.set_field(manager,204,str(target));command(manager,207)
        assert [recipes()[0][1],recipes()[2][1]]==schemes
        edited_flow=recipes()[1][1];assert edited_flow['structure']==original_flow['structure']
        command(manager,209);assert recipes()[1][1]==edited_flow
        command(manager,228);assert len(recipes())==4
        command(manager,209);assert [recipes()[0][1],recipes()[2][1]]==schemes
        command(manager,208);assert [name for name,_ in recipes()]==['Alt','Main','Flow edited']
        tab(manager,0);select(manager,0);command(manager,208);assert [name for name,_ in recipes()]==['Main','Flow edited']
        tab(manager,1);select(manager,2);command(manager,209)
        assert [decode(read()['Favorite'+str(i)]['name']) for i in range(3)]==['位置0','位置2','位置1']
        checks.append('filtered scheme/workflow lists edit, copy, reorder and delete the intended records; favorites keep their own ordering')
        tab(manager,0);command(manager,255);n.wait(lambda:not u.IsWindow(manager));filled('X_Rock')
        checks.append('Use scheme returns to the save dialog and applies through the current naming pipeline')
        package=out/'schemes.ini';n.activate(panel);u.PostMessageW(panel,0x111,730,0);form=modal('选择迁移内容')
        assert not u.GetDlgItem(form,103);e.set_field(form,101,'1');e.set_field(form,102,'');save(form);file_choice(package);notice()
        exported=configparser.ConfigParser();exported.read(package,encoding='utf-16')
        assert exported.getint('Jump','recipes')==1 and not exported.has_option('Jump','groups')
        n.activate(panel);u.PostMessageW(panel,0x111,731,0);file_choice(package);form=modal('选择迁移内容');save(form)
        form=modal('确认合并结果');save(form);notice()
        assert len(recipes())==3 and recipes()[2][0]=='Main（导入 1）'
        assert decode(read()['Jump']['activeNamingScheme'])=='adapter-fixture'
        checks.append('selective transfer contains only schemes/workflows and projects; duplicate imports receive independent identities without replacing the active scheme')
        if u.IsWindow(manager):close(manager)
        else:
            manager=next(iter(n.windows(app.pid,'Jump.Manager',True)),None)
            if manager:close(manager)
        n.wait(lambda:n.panel_ready(panel))
        n.activate(panel);command(panel,4001);n.wait(lambda:n.folder_is(dialog,target));filled('P_Rock')
        n.activate(dialog);u.PostMessageW(dialog,0x111,1,0);host.wait(timeout=8)
        n.wait(lambda:(target/'P_Rock.txt').is_file())
        n.wait(lambda:read().getint('Jump','savedFiles',fallback=0)==1)
        assert cfg.with_name(cfg.name+'.bak1').is_file()
        checks.append('a saved workflow restores its captured naming and edited location; native Save writes and verifies the real file')
        report={'passed':checks,'output':str(out)};(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        print('OUTPUT',out,flush=True)
        for window in n.windows(app.pid,visible=True):
            if n.text(window,u.GetClassNameW) in ['Jump.Panel','Jump.Manager','#32770']:e.capture(window,out/('failure-'+str(window)+'.png'))
        raise
    finally:
        if host and host.poll() is None:host.terminate();host.wait(timeout=5)
        if app.poll() is None:
            for h in n.windows(app.pid,'Jump.Control'):u.PostMessageW(h,0x10,0,0)
            try:app.wait(timeout=4)
            except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=4)

if __name__=='__main__':run()
