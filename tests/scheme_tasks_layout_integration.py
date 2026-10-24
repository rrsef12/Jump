"""Eight-part editing, keyboard focus, viewport resize and DPI changes on an owned fixture."""
import configparser
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import subprocess
import uuid
import native_integration as n
import extended_integration as e
import naming_menu as menu

u=n.u

def run():
    out=n.ROOT/'tests/output/scheme-task-layout'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    cfg=out/'settings.ini';cfg.write_text('[Jump]\nschema=1\nrecipes=0\nFavorite=0\n',encoding='utf-16')
    dev=n.ROOT/'build/dev/settings.ini';baseline=hashlib.sha256(dev.read_bytes()).digest()
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--manage','--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
    checks=[]
    class GuiThread(c.Structure):
        _fields_=[('size',w.DWORD),('flags',w.DWORD)]+[(name,w.HWND) for name in ('active','focus','capture','menu','move','caret')]+[('rect',w.RECT)]
    u.GetGUIThreadInfo.argtypes=[w.DWORD,c.POINTER(GuiThread)]
    def focused_control(form):
        info=GuiThread();info.size=c.sizeof(info)
        assert u.GetGUIThreadInfo(u.GetWindowThreadProcessId(form,None),c.byref(info))
        return info.focus
    def cmd(form,ident):u.SendMessageW(form,0x111,ident,u.GetDlgItem(form,ident) or 0)
    def choose(form,ident,index):
        u.SendDlgItemMessageW(form,ident,0x14e,index,0);u.SendMessageW(form,0x111,ident|(1<<16),u.GetDlgItem(form,ident))
    def add(form,label):
        n.activate(manager);anchor=e.rect(manager);u.SetCursorPos(anchor.left+4,anchor.top+4)
        u.PostMessageW(form,0x111,4005,u.GetDlgItem(form,4005))
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None))
        handle=u.SendMessageW(popup,0x1e1,0,0);owner,index=menu.find(handle,label)
        menu.select_item(popup,owner,index);n.wait(lambda:not n.windows(app.pid,'#32768',True))
    def parser():
        result=configparser.ConfigParser();result.read(cfg,encoding='utf-16');return result
    def bounds(form):
        shell=e.rect(manager);r=e.rect(form)
        assert r.left>=shell.left and r.right<=shell.right and r.top>=shell.top and r.bottom<=shell.bottom
    def composition_bounds(form,count):
        # Interactive targets must not overlap, including when rows wrap at high DPI.
        shell=e.rect(form);previous=None
        for i in range(count):
            name=e.rect(u.GetDlgItem(form,4220+i));source=e.rect(u.GetDlgItem(form,4850+i));hint=e.rect(u.GetDlgItem(form,4240+i))
            assert name.bottom<=source.top
            assert hint.right<=source.left or source.right<=hint.left or source.bottom<=hint.top or hint.bottom<=source.top, 'source and status must occupy separate table cells'
            if previous:assert previous.bottom<name.top, 'wrapped status must not collide with the following part'
            previous=hint
            targets=[u.GetDlgItem(form,base+i) for base in [4200,4300,4260,4340,4545,4555,4320,4890]]
            targets=[(u.GetDlgCtrlID(h),e.rect(h)) for h in targets if h and u.IsWindowVisible(h)]
            for j,(ident,a) in enumerate(targets):
                assert a.left>=shell.left and a.right<=shell.right, 'row actions must fit the viewport width'
                for other,b in targets[j+1:]:
                    assert a.right<=b.left or b.right<=a.left or a.bottom<=b.top or b.bottom<=a.top, f'row {i} actions {ident} {a.left,a.top,a.right,a.bottom} / {other} {b.left,b.top,b.right,b.bottom} overlap'
        assert previous.bottom<e.rect(u.GetDlgItem(form,4191)).top, 'file format controls must follow all table rows'
        assert e.rect(u.GetDlgItem(form,4005)).bottom<e.rect(u.GetDlgItem(form,4220)).top, 'Add belongs beside the structure heading'
        batch=e.rect(u.GetDlgItem(form,4150));simulate=e.rect(u.GetDlgItem(form,4530));copy=e.rect(u.GetDlgItem(form,4870))
        assert batch.top==simulate.top and simulate.right<batch.left, 'batch preview belongs beside the simulation action'
        assert copy.bottom<batch.top and e.rect(u.GetDlgItem(form,4000)).top>=copy.top, 'scheme operations belong beside the scheme name'
        assert not u.IsWindowVisible(u.GetDlgItem(form,4009)), 'copy is a secondary scheme menu action'
        assert n.text(u.GetDlgItem(form,4861))=='保存位置'
        assert not u.IsWindowVisible(u.GetDlgItem(form,4091)), 'dates are configured from their own rows'
    def fixed_footer(form):
        shell=e.rect(form);save=e.rect(u.GetDlgItem(form,1));cancel=e.rect(u.GetDlgItem(form,2));status=e.rect(u.GetDlgItem(form,4868))
        assert shell.top<=save.top<save.bottom<=shell.bottom and save.top==cancel.top
        assert status.top==save.top and status.right<cancel.left
        surface=e.rect(u.GetDlgItem(form,4869));assert surface.bottom==shell.bottom and surface.top<save.top
        u.ChildWindowFromPointEx.argtypes=[w.HWND,w.POINT,w.UINT];u.ChildWindowFromPointEx.restype=w.HWND
        assert u.ChildWindowFromPointEx(form,w.POINT(2,surface.top-shell.top+2),3)==u.GetDlgItem(form,4869), 'the footer surface must cover scrolling content'
        batch=u.GetDlgItem(form,4150);b=e.rect(batch)
        if u.IsWindowVisible(batch) and shell.top<=b.top and b.bottom<=surface.top:
            point=w.POINT((b.left+b.right)//2-shell.left,(b.top+b.bottom)//2-shell.top)
            assert u.ChildWindowFromPointEx(form,point,3)==batch, 'empty feedback must not cover the tools row'
        u.GetNextDlgTabItem.argtypes=[w.HWND,w.HWND,w.BOOL];u.GetNextDlgTabItem.restype=w.HWND
        assert u.GetNextDlgTabItem(form,u.GetDlgItem(form,2),False)==u.GetDlgItem(form,1)
        assert u.GetNextDlgTabItem(form,u.GetDlgItem(form,1),False)==u.GetDlgItem(form,4000), 'Save cycles back to the first input'
        return (save.left,save.top,save.right,save.bottom)
    try:
        manager=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None));n.activate(manager)
        # Open the built-in default without modifying its configuration.
        listing=u.GetDlgItem(manager,202)
        for index in range(u.SendMessageW(listing,0x18b,0,0)):
            label=c.create_unicode_buffer(256);u.SendMessageW(listing,0x189,index,c.addressof(label))
            if label.value=='资产文件':break
        else:raise AssertionError('built-in default scheme missing')
        u.SendMessageW(listing,0x186,index,0);cmd(manager,202|(1<<16))
        u.PostMessageW(manager,0x111,207,u.GetDlgItem(manager,207))
        form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        assert n.text(u.GetDlgItem(form,4000))=='资产文件'
        e.capture(manager,out/'default-scheme.png');cmd(form,2);n.wait(lambda:not u.IsWindow(form))
        u.PostMessageW(manager,0x111,210,u.GetDlgItem(manager,210))
        form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        clean=n.text(u.GetDlgItem(form,4868));original=n.text(u.GetDlgItem(form,4000))
        e.set_field(form,4000,original+'修改');assert n.text(u.GetDlgItem(form,4868))=='有未保存的修改'
        e.set_field(form,4000,original);assert n.text(u.GetDlgItem(form,4868))==clean, 'reverting a rule edit must restore the clean state'
        e.set_field(form,4093,'PreviewOnly');assert n.text(u.GetDlgItem(form,4868))==clean, 'simulation is not a rule modification'
        e.set_field(form,4093,'Asset');choose(form,4001,1);cmd(form,4536)
        e.set_field(form,4000,'美术命名');cmd(form,4340);e.set_field(form,4040,'主要名称');cmd(form,4170);cmd(form,4260)
        for index,label in enumerate(['角色设定','角色参考','场景设定','场景参考','道具设定','服装设定','色彩方案','构图草图']):
            if index:cmd(form,4182)
            e.set_field(form,4500+index%3,label);e.set_field(form,4510+index%3,label)
        cmd(form,4170);composition_bounds(form,1);e.capture(manager,out/'single-part-eight-options.png')
        assert '8 个选项' in n.text(u.GetDlgItem(form,4260))
        assert n.text(u.GetDlgItem(form,4191))=='_'
        for separator in ['', '-', ' · ']:
            e.set_field(form,4191,separator);cmd(form,4530);cmd(form,4170)
            assert n.text(u.GetDlgItem(form,4191))==separator, 'editable separators must survive navigation, including empty and custom text'
        e.set_field(form,4191,'_')
        u.PostMessageW(form,0x111,4150,u.GetDlgItem(form,4150))
        trial=n.wait(lambda:next(iter(n.dialogs(app.pid,'文件名效果预览')),None))
        e.set_field(trial,4600,'Rock.fbx\r\nGuard.fbx')
        # WM_SETTEXT on a multiline EDIT does not send EN_CHANGE itself.
        u.SendMessageW(trial,0x111,4600|(0x300<<16),u.GetDlgItem(trial,4600))
        n.wait(lambda:'2 个名字 · 2 个可生成' in n.text(u.GetDlgItem(trial,4602)))
        results=u.GetDlgItem(trial,4601)
        for index,expected in enumerate(['Rock.fbx','Guard.fbx']):
            buffer=c.create_unicode_buffer(1024);u.SendMessageW(results,0x189,index,c.addressof(buffer))
            assert '新名：'+expected in buffer.value
        e.capture(manager,out/'batch-preview.png');cmd(trial,2);n.wait(lambda:not u.IsWindow(trial))
        assert n.text(u.GetDlgItem(form,4093))=='Asset'
        checks.append('single-part overview exposes eight options and batch preview; empty/custom separators survive navigation; batch results preserve extensions and leave simulation input intact')
        e.set_field(form,4000,'简单命名')
        for label in ['项目名称','日期']:add(form,label)
        e.set_field(form,4093,'Rock')
        assert n.text(u.GetDlgItem(form,4083)).startswith('Rock'), 'the compact original-name input updates the preview directly'
        composition_bounds(form,3);e.capture(manager,out/'three-parts.png')
        assert n.text(u.GetDlgItem(form,4850))=='原名带入 · 可改'
        assert '自动 · 当前项目' in n.text(u.GetDlgItem(form,4851))
        assert '自动 · 当天日期' in n.text(u.GetDlgItem(form,4852))
        assert n.text(u.GetDlgItem(form,4280))=='使用保存窗口的原文件名', 'the primary part must describe its source instead of presenting a sample as a default'
        # Date formatting follows the date part and stays independent of numbering.
        cmd(form,4892);assert focused_control(form)==u.GetDlgItem(form,4091)
        assert u.IsWindowVisible(u.GetDlgItem(form,4091)) and not u.IsWindowVisible(u.GetDlgItem(form,4090))
        original_label=n.text(u.GetDlgItem(form,4040));e.set_field(form,4040,'')
        assert e.rect(u.GetDlgItem(form,4091)).top+int(28*u.GetDpiForWindow(form)/96)<e.rect(u.GetDlgItem(form,4084)).top, 'date formatting and validation feedback must not overlap on first entry'
        e.set_field(form,4040,original_label)
        choose(form,4091,1);cmd(form,4170)
        assert '-' in n.text(u.GetDlgItem(form,4282))
        cmd(form,4892);choose(form,4091,0);cmd(form,4170)
        for label in ['文件类别','补充说明','当前状态','手动编号','固定文字…']:add(form,label)
        n.wait(lambda:u.IsWindowVisible(u.GetDlgItem(form,4227)),label='all eight parts created')
        e.set_field(form,4307,'FX')
        assert '每次输出' in n.text(u.GetDlgItem(form,4247)) and '可留空' not in n.text(u.GetDlgItem(form,4247))
        cmd(form,4343);long_name=('用于游戏资产导出和团队共享的非常长的文件类别名称以及附加说明'+'（导出规则校验）')[:40]
        e.set_field(form,4040,long_name);cmd(form,4170)
        cmd(form,4344);cmd(form,4540);choose(form,4081,4)
        e.set_field(form,4082,'SM_导出时仅用于高精度资产和外包交付的细分模型类别');u.SendMessageW(form,0x111,4082|(5<<16),u.GetDlgItem(form,4082));cmd(form,4170)
        summary=n.text(u.GetDlgItem(form,4244));assert '条件：' in summary and len(summary)<55 and '…' in summary, 'the overview must abbreviate long conditions'
        cmd(form,4344);cmd(form,4540)
        assert n.text(u.GetDlgItem(form,4082))=='SM_导出时仅用于高精度资产和外包交付的细分模型类别', 'the full condition remains editable'
        cmd(form,4170)
        composition_bounds(form,8);e.capture(manager,out/'eight-parts-top.png')
        pinned=fixed_footer(form);u.SendMessageW(form,0x115,7,0);assert fixed_footer(form)==pinned
        e.capture(manager,out/'eight-parts-bottom.png');u.SendMessageW(form,0x115,6,0);assert fixed_footer(form)==pinned
        checks.append('compact preview edits original-name simulation on the rules page; three/eight-part layouts keep direct actions aligned and separate from wrapped names and condition summaries')
        cmd(form,4530)
        e.set_field(form,4093,'Rock');e.set_field(form,4803,'SM');u.SendMessageW(form,0x111,4803|(5<<16),u.GetDlgItem(form,4803))
        assert 'SM' in n.text(u.GetDlgItem(form,4083)) and '001' in n.text(u.GetDlgItem(form,4083))
        u.GetNextDlgTabItem.argtypes=[w.HWND,w.HWND,w.BOOL];u.GetNextDlgTabItem.restype=w.HWND
        assert u.GetNextDlgTabItem(form,u.GetDlgItem(form,4093),False)==u.GetDlgItem(form,4803), 'Tab from original name must reach the first editable simulation input before Save'
        assert '自动来源不可用' in n.text(u.GetDlgItem(form,4534))
        bounds(form)
        checks.append('eight content types coexist, including automatic project/date, editable categories, fixed text and independent manual numbering')

        # Resizing only changes the viewport; composition and temporary values stay intact.
        u.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
        r=e.rect(manager);u.SetWindowPos(manager,None,r.left,r.top,r.right-r.left,510,0x14)
        bounds(form);fixed_footer(form);assert e.rect(form).bottom-e.rect(form).top<500
        before=n.text(u.GetDlgItem(form,4083));e.capture(manager,out/'small-viewport.png')
        original_dpi=u.GetDpiForWindow(manager)
        for dpi in [96,144,original_dpi]:
            r=e.rect(manager);suggested=w.RECT(r.left,r.top,r.left+int(810*dpi/96),r.top+int(650*dpi/96))
            u.SendMessageW(manager,0x2e0,dpi|(dpi<<16),c.addressof(suggested))
            assert n.text(u.GetDlgItem(form,4083))==before
            assert 'SM'==n.text(u.GetDlgItem(form,4803))
            bounds(form);fixed_footer(form);e.capture(manager,out/f'editor-dpi-{dpi}.png')
        cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4203))=='', 'DPI changes and simulation must not rewrite future defaults'
        assert n.text(u.GetDlgItem(form,4307))=='FX'
        for dpi in [96,144,192,original_dpi]:
            r=e.rect(manager);suggested=w.RECT(r.left,r.top,r.left+int(810*dpi/96),r.top+int(850*dpi/96))
            u.SendMessageW(manager,0x2e0,dpi|(dpi<<16),c.addressof(suggested));composition_bounds(form,8)
            assert n.text(u.GetDlgItem(form,4203))=='' and n.text(u.GetDlgItem(form,4307))=='FX'
            e.capture(manager,out/f'eight-parts-dpi-{dpi}-top.png')
            pinned=fixed_footer(form);u.SendMessageW(form,0x115,7,0);assert fixed_footer(form)==pinned
            e.capture(manager,out/f'eight-parts-dpi-{dpi}-bottom.png');u.SendMessageW(form,0x115,6,0);assert fixed_footer(form)==pinned
        checks.append('a reduced resizable viewport and 100/150/native-percent DPI change messages preserve both the eight-part draft and isolated simulation input')
        checks.append('long names and abbreviated conditions stay separate at 100/150/200/native-percent DPI; Save/Cancel remain fixed while scrolling, resizing and correcting errors; reverting edits clears dirty state and simulation never marks rules dirty')

        # Invalid input stays intact when following its correction action.
        cmd(form,4346);cmd(form,4021);e.set_field(form,4062,'9')
        assert u.IsWindowVisible(u.GetDlgItem(form,4541))
        r=e.rect(manager);u.SetWindowPos(manager,None,r.left,r.top,r.right-r.left,510,0x14)
        cmd(form,4541);assert n.text(u.GetDlgItem(form,4062))=='9'
        shell=e.rect(form);focused=e.rect(u.GetDlgItem(form,4062))
        assert focused.top>=shell.top and focused.bottom<e.rect(u.GetDlgItem(form,1)).top-8, 'correction must reveal input above the fixed footer'
        e.set_field(form,4062,'3');cmd(form,4170)
        cmd(form,4346);cmd(form,4539)
        assert u.IsWindowVisible(u.GetDlgItem(form,4160)) and not u.IsWindowVisible(u.GetDlgItem(form,4040))
        assert u.IsWindowVisible(u.GetDlgItem(form,4043)) and u.IsWindowVisible(u.GetDlgItem(form,4044))
        cmd(form,2);n.wait(lambda:not u.IsWindow(form))
        assert parser().getint('Jump','recipes')==0
        checks.append('correction preserves invalid text and reveals the focused input after shrinking; behavior is distinct from source editing; cancelling all eight parts writes no recipe')
        # Save and reopen settings reached through their new task-specific entrances.
        r=e.rect(manager);u.SetWindowPos(manager,None,r.left,r.top,r.right-r.left,int(850*original_dpi/96),0x14)
        u.PostMessageW(manager,0x111,210,u.GetDlgItem(manager,210));form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        choose(form,4001,1);cmd(form,4536);e.set_field(form,4000,'日期与编号回归')
        add(form,'日期');cmd(form,4891);choose(form,4091,1)
        choose(form,4041,0);assert not u.IsWindowVisible(u.GetDlgItem(form,4091))
        e.set_field(form,4042,'Manual');choose(form,4041,2);assert u.IsWindowVisible(u.GetDlgItem(form,4091))
        cmd(form,4170);assert '-' in n.text(u.GetDlgItem(form,4281))
        add(form,'自定义内容…');cmd(form,4342);choose(form,4041,2)
        assert u.SendDlgItemMessageW(form,4091,0x147,0,0)==1, 'all date parts share the existing scheme-level date format'
        choose(form,4091,2);e.capture(manager,out/'date-part-settings.png');cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4281))==n.text(u.GetDlgItem(form,4282)) and '_' in n.text(u.GetDlgItem(form,4281))
        cmd(form,4023)
        assert not u.IsWindowVisible(u.GetDlgItem(form,4091)) and not u.IsWindowVisible(u.GetDlgItem(form,4191))
        choose(form,4003,1);choose(form,4090,2);u.SendDlgItemMessageW(form,4092,0xf1,1,0);cmd(form,4092)
        e.capture(manager,out/'numbering-settings.png')
        choose(form,4003,0);assert not u.IsWindowEnabled(u.GetDlgItem(form,4090))
        cmd(form,4170)
        buffer=c.create_unicode_buffer(128);u.SendMessageW(u.GetDlgItem(form,4192),0x148,1,c.addressof(buffer))
        assert buffer.value=='版本 V0001', 'the quick selector must reflect the configured digits and version case'
        choose(form,4192,1);assert n.text(u.GetDlgItem(form,4083)).endswith('_V0001')
        cmd(form,4171);u.SendDlgItemMessageW(form,4163,0xf1,0,0);cmd(form,4163)
        u.SendDlgItemMessageW(form,4162,0xf1,0,0);cmd(form,4162);cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4862))=='目录推荐：尚未配置'
        # Invalid separators are corrected directly in the overview.
        e.set_field(form,4191,'/');u.SendMessageW(form,0x111,4191|(5<<16),u.GetDlgItem(form,4191));cmd(form,4541)
        assert focused_control(form)==u.GetDlgItem(form,4191) or u.GetParent(focused_control(form))==u.GetDlgItem(form,4191)
        e.set_field(form,4191,'_');cmd(form,1);n.wait(lambda:not u.IsWindow(form))
        n.wait(lambda:parser().getint('Jump','recipes')==1,label='new scheme persisted')
        u.PostMessageW(manager,0x111,207,u.GetDlgItem(manager,207));form=n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
        assert n.text(u.GetDlgItem(form,4000))=='日期与编号回归'
        cmd(form,4891);assert u.SendDlgItemMessageW(form,4091,0x147,0,0)==2
        cmd(form,4170);cmd(form,4023)
        assert u.SendDlgItemMessageW(form,4090,0x147,0,0)==2 and u.SendDlgItemMessageW(form,4092,0xf0,0,0)==1
        cmd(form,4171);assert not u.SendDlgItemMessageW(form,4163,0xf0,0,0) and not u.SendDlgItemMessageW(form,4162,0xf0,0,0)
        cmd(form,2);n.wait(lambda:not u.IsWindow(form))
        checks.append('date formats are edited beside date parts, remain shared across automatic date sources and survive source changes; numbering and same-name checks keep their settings through Save and reopen; invalid separators locate the visible overview control')
        assert hashlib.sha256(dev.read_bytes()).digest()==baseline
        report={'passed':checks,'output':str(out)};(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        print('OUTPUT',out,flush=True)
        for window in n.windows(app.pid,'Jump.Manager',True):e.capture(window,out/'failure.png')
        raise
    finally:
        for window in n.windows(app.pid,'Jump.Control'):u.PostMessageW(window,0x10,0,0)
        try:app.wait(timeout=5)
        except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=5)

if __name__=='__main__':run()
