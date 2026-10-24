"""Owned 1/4/8-field UX scenarios; configuration and production naming stay isolated."""
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
from naming_fixture import pack, encode

u=n.u


def field(ident,label,default='',required=False,source=0,choices=(),condition=('', ''),width=0,joiner=None,prefix=''):
    meta=pack([default,joiner or '',prefix,'',*condition,'1','1' if joiner is not None else '0','0','2' if width else '0',str(width),str(source)])
    return pack([ident,label,default,'1' if required else '0',str(source),meta]+[pack([name,value,'']) for name,value in choices])


def scheme(ident,name,fields,counter=0,locations=(),recommend=True):
    routes=pack([pack(list(rule)) for rule in locations])
    meta=pack(['subject','0','',routes,pack(['1' if recommend else '0','1'])])
    return pack(['JNS2',ident,name,'_','1','0','0',str(counter),'3','0','0',meta,*fields])


def run():
    out=n.ROOT/'tests/output/scheme-usage-scenarios'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    dev=n.ROOT/'build/dev/settings.ini';dev_hash=hashlib.sha256(dev.read_bytes()).hexdigest()
    types=[('静态模型','SM'),('贴图','T'),('材质','M')]
    stages=[('制作中','WIP'),('审核中','REVIEW'),('已完成','FINAL')]
    subject=field('subject','主要名称',required=True,choices=[(name,name) for name in ['Rock','Cliff','Guard','Tree','Wall','Bridge','House','Door']])
    simple=scheme('usage-simple','简单 · 原文件名',[subject])
    typical=scheme('usage-typical','典型 · 4 个命名部分',[
        field('type','文件类别','SM',True,choices=types),subject,
        field('stage','当前状态','WIP',choices=stages),field('detail','补充说明')],1)
    long_label='用于多团队资产交付和自动导出的自定义精度与层级名称'
    complex_fields=[field('type','文件类别','SM',True,choices=types),subject,
        field('stage','当前状态','WIP',choices=stages),field('author','作者缩写','LJ'),
        field('lod',long_label,'1',width=2,joiner='-',prefix='LOD'),
        field('usage','贴图用途','BC',choices=[('颜色','BC'),('法线','N')],condition=('type','T')),
        field('revision','手动修订号','2',width=3,joiner='~',prefix='r'),field('fixed','固定文字','FX',source=3)]
    routes=[('type',value,str(out/folder)) for value,folder in [('SM','Models'),('T','Textures'),('M','Materials')]]
    complex_schema=scheme('usage-complex','复杂 · 8 个命名部分',complex_fields,1,routes)
    cfg=out/'settings.ini';seed=configparser.ConfigParser()
    seed['Jump']={'schema':'1','recipes':'3','Favorite':'0','History':'0','defaultNamingScheme':encode('usage-typical'),'applicationNamingSchemes':'1'}
    seed['ApplicationNamingScheme0']={'executable':encode('jump.testhost.exe'),'scheme':encode('usage-typical')}
    for i,(ident,name,definition) in enumerate([('usage-simple','简单 · 原文件名',simple),('usage-typical','典型 · 4 个命名部分',typical),('usage-complex','复杂 · 8 个命名部分',complex_schema)]):
        seed[f'Recipe{i}']={'name':encode(name),'value':encode('Rock'),'namingOnly':'1','complete':'1','structure':encode(definition)}
    with cfg.open('w',encoding='utf-16') as file:seed.write(file)
    baseline=cfg.read_bytes();checks=[];metrics={}
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--manage','--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
    def cmd(form,ident):u.SendMessageW(form,0x111,ident,u.GetDlgItem(form,ident) or 0)
    def choose(form,ident,index):
        u.SendMessageW(u.GetDlgItem(form,ident),0x14e,index,0);u.SendMessageW(form,0x111,ident|(1<<16),u.GetDlgItem(form,ident))
    def original(form,value):e.set_field(form,4093,value)
    def edit_combo(form,ident,value):
        e.set_field(form,ident,value);u.SendMessageW(form,0x111,ident|(5<<16),u.GetDlgItem(form,ident))
    def editor(index):
        u.SendMessageW(u.GetDlgItem(manager,202),0x186,index,0);cmd(manager,202|(1<<16))
        u.PostMessageW(manager,0x111,207,u.GetDlgItem(manager,207))
        return n.wait(lambda:next(iter(n.dialogs(app.pid,'命名方案')),None))
    def close(form):cmd(form,2);n.wait(lambda:not u.IsWindow(form))
    def footer(form):
        r=e.rect(u.GetDlgItem(form,1));return r.left,r.top,r.right,r.bottom
    def check_rows(form,count):
        dpi=u.GetDpiForWindow(manager);sizes=[]
        for i in range(count):
            name=e.rect(u.GetDlgItem(form,4220+i));source=e.rect(u.GetDlgItem(form,4850+i))
            assert name.bottom<=source.top
            assert all(u.IsWindowVisible(u.GetDlgItem(form,base+i)) for base in [4260,4340,4545,4555,4320])
            choices=e.rect(u.GetDlgItem(form,4260+i));rules=e.rect(u.GetDlgItem(form,4340+i))
            assert choices.top==rules.top and choices.right<rules.left
            if i:sizes.append(round((name.top-previous)*96/dpi,1))
            previous=name.top
        metrics[str(count)+'_fields']={'row_intervals_dip':sizes,'direct_row_actions':5,'expand_to_edit_default':False}
    def copy_menu(form,select):
        n.activate(manager);u.PostMessageW(form,0x111,4870,u.GetDlgItem(form,4870))
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None));handle=u.SendMessageW(popup,0x1e1,0,0)
        assert menu.labels(handle)==['复制为新方案']
        if select:menu.select_item(popup,handle,0)
        else:u.PostMessageW(form,0x1f,0,0)
        n.wait(lambda:not n.windows(app.pid,'#32768',True))
    try:
        manager=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Manager',True)),None));n.activate(manager)
        form=editor(0);original(form,'Rock');check_rows(form,1)
        assert n.text(u.GetDlgItem(form,4083))=='Rock'
        assert n.text(u.GetDlgItem(form,4280))=='使用保存窗口的原文件名'
        assert '手填或选项替换本次' in n.text(u.GetDlgItem(form,4240))
        assert all(not u.IsWindowEnabled(u.GetDlgItem(form,ident)) for ident in [4545,4555,4320])
        assert n.text(u.GetDlgItem(form,4862))=='目录推荐：尚未配置'
        e.capture(manager,out/'simple-one-part.png')
        original(form,'');assert '模拟未完成' in n.text(u.GetDlgItem(form,4083))
        assert 'Asset' not in n.text(u.GetDlgItem(form,4083)), 'an empty original never silently becomes Asset'
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        e.capture(manager,out/'simple-empty-original.png');original(form,'Rock')
        cmd(form,4260);assert n.text(u.GetDlgItem(form,4500))=='Rock'
        cmd(form,4170);cmd(form,4530);original(form,'Cliff')
        assert n.text(u.GetDlgItem(form,4083))=='Cliff' and n.text(u.GetDlgItem(form,4868))=='尚无修改'
        cmd(form,4170);cmd(form,4862);assert u.IsWindowVisible(u.GetDlgItem(form,4700))
        cmd(form,4170);copy_menu(form,False);assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        close(form);assert cfg.read_bytes()==baseline
        checks.append('one part: Rock and empty original follow the actual source without Asset fallback; choices remain one action away; impossible reorder/delete are disabled; preview, cancelled menu and Cancel leave configuration untouched')

        form=editor(1);original(form,'Rock');check_rows(form,4)
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_WIP_v001'
        assert n.text(u.GetDlgItem(form,4200))=='SM' and n.text(u.GetDlgItem(form,4202))=='WIP'
        assert '3 个选项' in n.text(u.GetDlgItem(form,4260))
        assert u.IsWindowVisible(u.GetDlgItem(form,4303)) and n.text(u.GetDlgItem(form,4303))==''
        assert not u.IsWindowVisible(u.GetDlgItem(form,4009))
        e.capture(manager,out/'typical-four-parts.png')
        # High-frequency edits stay direct: one default change, one option page,
        # and one numbering selection. All update the live result immediately.
        edit_combo(form,4202,'REVIEW');assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_REVIEW_v001'
        edit_combo(form,4202,'WIP');assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        cmd(form,4260);assert n.text(u.GetDlgItem(form,4500))=='静态模型' and n.text(u.GetDlgItem(form,4510))=='SM'
        cmd(form,4170);choose(form,4192,0);assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_WIP'
        choose(form,4192,1);assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        cmd(form,4530);edit_combo(form,4800,'T');assert n.text(u.GetDlgItem(form,4083))=='T_Rock_WIP_v001'
        cmd(form,4170);assert n.text(u.GetDlgItem(form,4200))=='SM'
        cmd(form,4530);cmd(form,4533);original(form,'Rock');cmd(form,4170)
        copy_menu(form,True);n.wait(lambda:n.text(u.GetDlgItem(form,4000)).endswith('副本'))
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_WIP_v001'
        cmd(form,4172);assert u.SendMessageW(u.GetDlgItem(form,4173),0xf0,0,0)==0
        assert n.text(u.GetDlgItem(form,4141))=='未关联软件'
        close(form);assert cfg.read_bytes()==baseline
        checks.append('four configured fields: SM_Rock_WIP_v001 with an empty optional detail; default/option mapping, live numbering, simulation isolation and revert all work; the scheme menu copies the full draft without software/global defaults and Cancel discards it')

        form=editor(2);original(form,'Rock');check_rows(form,8)
        assert n.text(u.GetDlgItem(form,4083))=='SM_Rock_WIP_LJ-LOD01~r002_FX_v001'
        assert n.text(u.GetDlgItem(form,4862))=='目录推荐：已启用 · 3 条规则'
        dpi=u.GetDpiForWindow(manager);r=e.rect(manager)
        u.SetWindowPos.argtypes=[w.HWND,w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.UINT]
        u.SetWindowPos(manager,None,r.left,r.top,r.right-r.left,round(860*dpi/96),0x14)
        e.capture(manager,out/'complex-eight-top.png');pinned=footer(form)
        u.SendMessageW(form,0x115,7,0);assert footer(form)==pinned
        e.capture(manager,out/'complex-eight-bottom.png')
        # Local formatting errors retain the invalid input and correction path.
        cmd(form,4344);cmd(form,4021);e.set_field(form,4062,'9')
        assert u.IsWindowVisible(u.GetDlgItem(form,4541)) and '补零' in n.text(u.GetDlgItem(form,4084))
        e.capture(manager,out/'complex-invalid-rule.png');cmd(form,4541)
        assert n.text(u.GetDlgItem(form,4062))=='9';e.set_field(form,4062,'2');cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        cmd(form,4530);edit_combo(form,4800,'T')
        assert n.text(u.GetDlgItem(form,4083))=='T_Rock_WIP_LJ-LOD01_BC~r002_FX_v001'
        assert 'BC' in n.text(u.GetDlgItem(form,4534));cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4200))=='SM' and n.text(u.GetDlgItem(form,4868))=='尚无修改'
        cmd(form,4862);assert u.IsWindowVisible(u.GetDlgItem(form,4700))
        u.SendMessageW(u.GetDlgItem(form,4162),0xf1,0,0);cmd(form,4162);cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4862))=='目录推荐：已关闭 · 保留 3 条规则'
        u.SendMessageW(form,0x115,7,0);e.capture(manager,out/'complex-routes-disabled.png')
        cmd(form,4171);u.SendMessageW(u.GetDlgItem(form,4162),0xf1,1,0);cmd(form,4162);cmd(form,4170)
        assert n.text(u.GetDlgItem(form,4862))=='目录推荐：已启用 · 3 条规则'
        assert n.text(u.GetDlgItem(form,4868))=='尚无修改'
        close(form);assert cfg.read_bytes()==baseline
        checks.append('eight fields: long custom names, conditional omission/inclusion, local LOD/revision formatting, direct row actions and fixed footer remain readable; invalid rules can be repaired without losing input; route states reflect effective configuration and toggles retain all three rules')
        assert hashlib.sha256(dev.read_bytes()).hexdigest()==dev_hash
        report={'passed':checks,'metrics':metrics,'output':str(out),'development_config_sha256':dev_hash,
                'limitations':'Automated owned fixtures and screenshot inspection; not a novice user study. Existing layout integration covers DPI transitions.'}
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
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
