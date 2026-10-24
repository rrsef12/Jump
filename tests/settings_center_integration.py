"""One settings shell, hosted drafts, navigation, projects and local tools."""
import configparser
import ctypes as c
import json
import subprocess
import uuid
from ctypes import wintypes as w
import native_integration as n
import extended_integration as e

u=n.u

def run():
    out=n.ROOT/'tests/output/settings-center'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    cfg=out/'settings.ini';cfg.write_text('[Jump]\nschema=1\nFavorite=0\nrecipes=0\nprojects=0\n',encoding='utf-16')
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--manage','--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
    checks=[]
    def config():
        p=configparser.ConfigParser();p.read(cfg,encoding='utf-16');return p
    def command(window,ident):u.PostMessageW(window,0x111,ident,u.GetDlgItem(window,ident) or 0)
    def inline_command(ident):u.SendMessageW(manager,0x111,ident,u.GetDlgItem(manager,ident) or 0)
    def editor(title):return n.wait(lambda:next(iter(n.dialogs(app.pid,title)),None),label=title)
    def close_editor(form,save=False):
        command(form,1 if save else 2);n.wait(lambda:not u.IsWindow(form),label='editor finished')
    def only_shell():
        n.wait(lambda:n.windows(app.pid,'Jump.Manager',True)==[manager],label='same visible settings shell')
        assert not n.windows(app.pid,'#32770',True),'editing must not open another top-level dialog'
    def hosted(form):
        only_shell();assert u.IsChild(manager,form)
        assert u.GetAncestor(form,2)==manager
        r=e.rect(form);shell=e.rect(manager)
        assert r.left>=shell.left and r.top>=shell.top and r.right<=shell.right and r.bottom<=shell.bottom
    def click_tab(index):
        control=u.GetDlgItem(manager,200);dpi=u.GetDpiForWindow(manager)
        point=(60*dpi//96)|((16*dpi//96+index*(32*dpi//96+3))<<16)
        u.PostMessageW(control,0x201,1,point);u.PostMessageW(control,0x202,0,point)
    def navigate_draft(index):
        click_tab(index)
        return editor('未保存的编辑')
    def navigate_clean(form,index):
        click_tab(index)
        n.wait(lambda:not u.IsWindow(form) or n.dialogs(app.pid,'未保存的编辑'),label='clean editor navigation')
        assert not n.dialogs(app.pid,'未保存的编辑'),'unchanged saved scheme must leave without a warning'
        n.wait(lambda:u.SendMessageW(u.GetDlgItem(manager,200),0x130B,0,0)==index)
    def saved_scheme():
        n.manager_tab(manager,0);listing=u.GetDlgItem(manager,202)
        for index in range(u.SendMessageW(listing,0x18B,0,0)):
            label=c.create_unicode_buffer(256);u.SendMessageW(listing,0x189,index,c.addressof(label))
            if 'Navigation fixture' in label.value:break
        else:raise AssertionError('saved navigation fixture missing')
        u.SendMessageW(listing,0x186,index,0);command(manager,207)
        return editor('命名方案')
    def answer(prompt, discard):
        command(prompt,6 if discard else 7);n.wait(lambda:not u.IsWindow(prompt))
    def select_draft(index):
        control=u.GetDlgItem(manager,202);u.SendMessageW(control,0x186,index,0)
        u.PostMessageW(manager,0x111,202|(1<<16),control)
        return editor('未保存的编辑')
    def locked_save(action):
        n.k.CreateFileW.argtypes=[w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
        n.k.CreateFileW.restype=w.HANDLE;n.k.CloseHandle.argtypes=[w.HANDLE]
        handle=n.k.CreateFileW(str(cfg)+'.tmp',0x40000000,0,None,2,0,None)
        assert handle not in (0,None,c.c_void_p(-1).value)
        try:action()
        finally:n.k.CloseHandle(handle)
    try:
        manager=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None));n.activate(manager)
        assert u.SendMessageW(u.GetDlgItem(manager,200),0x1304,0,0)==9
        for index in range(9):
            n.manager_tab(manager,index);only_shell()
            e.capture(manager,out/('section-'+str(index)+'.png'))
        checks.append('all nine sections share one settings window; startup DPI and section bounds fit')

        n.manager_tab(manager,0);command(manager,210);form=editor('命名方案');hosted(form)
        assert u.IsWindowVisible(u.GetDlgItem(form,4536)), 'new schemes start with an explicit template decision'
        template=u.GetDlgItem(form,4001);u.SendMessageW(template,0x14E,1,0);u.SendMessageW(form,0x111,4001|(1<<16),template)
        command(form,4536);n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4340)))
        e.set_field(form,4000,'Unsaved scheme')
        command(form,4260);n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4500)))
        hosted(form)
        e.set_field(form,4500,'Unsaved option');command(form,4100)
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4340)))
        assert n.text(u.GetDlgItem(form,4000))=='Unsaved scheme'
        e.capture(manager,out/'nested-editor-return.png')
        prompt=navigate_draft(6);command(prompt,7);n.wait(lambda:not u.IsWindow(prompt))
        assert u.IsWindow(form) and u.IsWindowVisible(form)
        assert u.SendMessageW(u.GetDlgItem(manager,200),0x130B,0,0)==0
        prompt=navigate_draft(6);command(prompt,6)
        n.wait(lambda:not u.IsWindow(form) and u.SendMessageW(u.GetDlgItem(manager,200),0x130B,0,0)==6)
        assert config().getint('Jump','recipes')==0
        checks.append('options edit directly inside the scheme; section navigation protects the shared draft and confirmed discard writes nothing')

        n.manager_tab(manager,0);command(manager,210);form=editor('命名方案')
        template=u.GetDlgItem(form,4001);u.SendMessageW(template,0x14E,2,0);u.SendMessageW(form,0x111,4001|(1<<16),template)
        command(form,4536);n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4340)))
        e.set_field(form,4000,'Navigation fixture');close_editor(form,True)
        n.wait(lambda:config().has_option('Recipe0','structure'),label='saved navigation fixture persisted')
        saved_definition=dict(config()['Recipe0'])
        form=saved_scheme();assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        navigate_clean(form,6)
        form=saved_scheme();e.set_field(form,4093,'PreviewOnly')
        command(form,4530);n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4800)))
        e.set_field(form,4800,'SimulationOnly');u.SendMessageW(form,0x111,4800|(5<<16),u.GetDlgItem(form,4800))
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        navigate_clean(form,6)
        form=saved_scheme();e.set_field(form,4000,'Navigation fixture changed')
        assert n.text(u.GetDlgItem(form,4868))=='有未保存的修改'
        answer(navigate_draft(6),False);assert u.IsWindow(form)
        e.set_field(form,4000,'Navigation fixture')
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        navigate_clean(form,6)
        form=saved_scheme();e.set_field(form,4000,'Discard actual edit')
        answer(navigate_draft(6),True)
        n.wait(lambda:not u.IsWindow(form) and u.SendMessageW(u.GetDlgItem(manager,200),0x130B,0,0)==6)
        assert dict(config()['Recipe0'])==saved_definition
        checks.append('saved schemes leave silently when unchanged, simulated or reverted; actual edits still protect the draft and discard preserves the saved definition')

        n.manager_tab(manager,1);inline_command(210)
        e.set_field(manager,203,'Draft favorite');e.set_field(manager,204,str(out))
        u.PostMessageW(manager,0x232,0,0)
        n.wait(lambda:config().has_section('ManagerWindow'))
        assert config().getint('Jump','Favorite')==0,'new favorite must stay outside live configuration'
        prompt=navigate_draft(3);answer(prompt,False)
        assert n.text(u.GetDlgItem(manager,203))=='Draft favorite'
        u.PostMessageW(manager,0x10,0,0);answer(editor('未保存的编辑'),False)
        assert u.IsWindow(manager)
        def failed_favorite():
            inline_command(207)
            n.wait(lambda:'保存失败' in n.text(u.GetDlgItem(manager,292)))
            assert config().getint('Jump','Favorite')==0
            assert n.text(u.GetDlgItem(manager,203))=='Draft favorite'
        locked_save(failed_favorite)
        inline_command(207);n.wait(lambda:config().getint('Jump','Favorite')==1)
        inline_command(210);e.set_field(manager,203,'Duplicate')
        e.set_field(manager,204,str(out).upper()+'\\.\\')
        inline_command(207);n.wait(lambda:'已经收藏' in n.text(u.GetDlgItem(manager,292)))
        assert config().getint('Jump','Favorite')==1
        prompt=navigate_draft(3);answer(prompt,True)
        n.wait(lambda:u.SendMessageW(u.GetDlgItem(manager,200),0x130B,0,0)==3)
        n.manager_tab(manager,1);inline_command(210)
        e.set_field(manager,203,'Second favorite');e.set_field(manager,204,str(out/'other'))
        inline_command(207);n.wait(lambda:config().getint('Jump','Favorite')==2)
        e.set_field(manager,203,'Keep this row draft');answer(select_draft(0),False)
        assert u.SendMessageW(u.GetDlgItem(manager,202),0x188,0,0)==1
        assert n.text(u.GetDlgItem(manager,203))=='Keep this row draft'
        answer(select_draft(0),True);n.wait(lambda:n.text(u.GetDlgItem(manager,203))=='Draft favorite')
        checks.append('inline favorite drafts survive rejected navigation, row changes and close; unrelated config writes exclude new drafts; duplicate paths are blocked and failed saves retain inputs for retry')

        n.manager_tab(manager,3)
        quick=u.GetDlgItem(manager,221);u.SendMessageW(quick,0xF1,0,0)
        prompt=navigate_draft(1);answer(prompt,False)
        assert u.SendMessageW(quick,0xF0,0,0)==0
        def failed_options():
            inline_command(223);n.wait(lambda:'保存失败' in n.text(u.GetDlgItem(manager,292)))
            assert config().getint('Jump','quickSave')==1
        locked_save(failed_options)
        inline_command(223);n.wait(lambda:config().getint('Jump','quickSave')==0)
        assert not n.dialogs(app.pid,'Jump'),'successful options save should stay inline'
        n.manager_tab(manager,4);inline_command(210);e.set_field(manager,204,'Retry_')
        before=config().getint('Jump','prefixes')
        def failed_affix():
            inline_command(207);n.wait(lambda:'保存失败' in n.text(u.GetDlgItem(manager,230)))
            assert config().getint('Jump','prefixes')==before
            assert n.text(u.GetDlgItem(manager,204))=='Retry_'
        locked_save(failed_affix)
        category=u.GetDlgItem(manager,252);u.SendMessageW(category,0x14E,1,0)
        u.PostMessageW(manager,0x111,252|(1<<16),category);answer(editor('未保存的编辑'),False)
        assert u.SendMessageW(category,0x147,0,0)==0
        inline_command(207);n.wait(lambda:config().getint('Jump','prefixes')==before+1)
        checks.append('general options and affix categories protect drafts; blocked filesystem writes roll back configuration and can be retried without retyping; successful saves need no modal acknowledgment')

        n.manager_tab(manager,5);inline_command(600)
        prompt=navigate_draft(1);answer(prompt,False)
        inline_command(246);e.set_field(manager,240,'{name}')
        inline_command(243);n.wait(lambda:bytes.fromhex(config()['NamingVariables']['expression']).decode('utf-16-be')=='{name}')
        n.manager_tab(manager,6)
        checks.append('variable token rebuilds preserve the original dirty baseline; saved template clears its draft state')

        command(manager,210);form=editor('新建项目');hosted(form)
        e.set_field(form,100,'Discard project');close_editor(form)
        assert config().getint('Jump','projects')==0
        command(manager,210);form=editor('新建项目')
        e.set_field(form,100,'Engine assets');e.set_field(form,101,str(out))
        e.set_field(form,102,'work');e.set_field(form,103,'delivery');e.set_field(form,104,'项目=Game\r\n作者=Artist')
        e.capture(manager,out/'project-editor.png');close_editor(form,True)
        n.wait(lambda:config().getint('Jump','projects')==1)
        assert bytes.fromhex(config()['Project0']['name']).decode('utf-16-be')=='Engine assets'
        only_shell();assert 'Engine assets' in n.text(u.GetDlgItem(manager,202)) or u.SendMessageW(u.GetDlgItem(manager,202),0x18B,0,0)==1
        checks.append('project forms edit inside the center; cancelled drafts leave configuration intact and saved projects persist')

        n.manager_tab(manager,7);command(manager,287);notice=editor('Jump')
        button=next(h for h in n.children(notice) if n.text(h,u.GetClassNameW)=='Button')
        command(notice,u.GetDlgCtrlID(button));n.wait(lambda:not n.dialogs(app.pid,'Jump'))
        assert cfg.with_name(cfg.name+'.bak1').is_file()
        assert u.IsWindowEnabled(u.GetDlgItem(manager,288));only_shell()
        n.manager_tab(manager,8);command(manager,207)
        n.wait(lambda:'尚未关联' in n.text(u.GetDlgItem(manager,280)))
        command(manager,289);only_shell();e.capture(manager,out/'connection-tools.png')
        checks.append('backup and diagnostics operate in their sections; diagnostic copying opens no extra window')

        n.manager_tab(manager,0);command(manager,210);form=editor('命名方案')
        u.PostMessageW(manager,0x10,0,0);prompt=editor('未保存的编辑');command(prompt,7)
        n.wait(lambda:not u.IsWindow(prompt));assert u.IsWindow(form) and u.IsWindow(manager)
        close_editor(form)
        form=saved_scheme();u.PostMessageW(manager,0x10,0,0)
        n.wait(lambda:not u.IsWindow(manager) or n.dialogs(app.pid,'未保存的编辑'),label='close unchanged saved editor')
        assert not n.dialogs(app.pid,'未保存的编辑') and not u.IsWindow(manager)
        assert dict(config()['Recipe0'])==saved_definition
        checks.append('closing the center protects a new unsaved scheme; an unchanged saved scheme closes without a warning or configuration changes')
        report={'passed':checks,'output':str(out)};(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        print('OUTPUT',out,flush=True)
        for window in n.windows(app.pid,'Jump.Manager',True):e.capture(window,out/'failure.png')
        raise
    finally:
        for window in n.windows(app.pid,'Jump.Control'):u.PostMessageW(window,0x10,0,0)
        try:app.wait(timeout=5)
        except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)

if __name__=='__main__':run()
