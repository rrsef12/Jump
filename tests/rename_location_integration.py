"""Current inline naming rules and explicit location behavior on owned dialogs."""
import configparser
import json
import subprocess
import time
import uuid
import native_integration as n
import naming_fixture
import extended_integration as e


def run():
    out=n.ROOT/'tests/output/rename-locations'/uuid.uuid4().hex[:10];out.mkdir(parents=True)
    cfg=out/'settings.ini';n.write_config(cfg)
    naming_fixture.install(cfg)
    destination=out/'delivery';destination.mkdir()
    p=configparser.ConfigParser();p.read(cfg,encoding='utf-16')
    enc=lambda value:str(value).encode('utf-16-be').hex().upper()
    p['Jump']['Favorite']='1';p['Favorite0']={'name':enc('规则交付'),'path':enc(destination)}
    with cfg.open('w',encoding='utf-16') as f:p.write(f)
    app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
    host=None;u=n.u;checks=[]
    def command(ident):u.SendMessageW(panel,0x111,ident,u.GetDlgItem(panel,ident) or 0)
    def expected(value):
        n.activate(panel)
        preview=u.GetDlgItem(panel,101)
        u.SendMessageW(preview,0x201,1,8|(8<<16));u.SendMessageW(preview,0x202,0,8|(8<<16))
        n.wait(lambda:n.text(u.GetDlgItem(panel,101))==value+'.md' and n.panel_ready(panel) and
               any(n.text(h)==value for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit'),label=value)
    def rule(index):
        field=u.GetDlgItem(panel,174);u.SendMessageW(field,0x14E,index-1,0)
        u.SendMessageW(panel,0x111,174|(1<<16),field)
    def save_blocked():
        command(106);time.sleep(.2)
        assert host.poll() is None and u.IsWindowVisible(dialog), 'invalid naming must not submit the host'
    try:
        n.wait(lambda:n.windows(app.pid,'Jump.Control'))
        host,dialog,_=n.launch_host('modern','md')
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None))
        n.wait(lambda:n.text(u.GetDlgItem(panel,101))=='原文件.md')
        command(121);n.click(panel,caption='规则交付')
        n.wait(lambda:n.folder_is(dialog,destination) and n.panel_ready(panel))
        command(124);assert n.text(u.GetDlgItem(panel,124))=='＋收藏'
        command(124);assert n.text(u.GetDlgItem(panel,124))=='已收藏'
        checks.append('favorite star toggles independently; location navigation reaches the native folder')
        n.activate(panel);e.set_field(panel,101,'角色 012_v02');expected('角色 012_v02')
        command(142);e.set_field(panel,186,'项目_');e.set_field(panel,187,'_定稿')
        expected('项目_角色 012_v02_定稿')
        if not u.GetDlgItem(panel,186):command(142)
        e.set_field(panel,186,'bad/');save_blocked()
        command(142);assert u.GetDlgItem(panel,186), 'invalid affix cannot be hidden by closing its tool'
        u.SendMessageW(u.GetDlgItem(panel,186),0x100,27,0)
        n.wait(lambda:not u.GetDlgItem(panel,186));expected('项目_角色 012_v02_定稿')
        command(142);assert n.text(u.GetDlgItem(panel,186))=='项目_'
        checks.append('affix validation blocks Jump Save and hiding; Escape discards invalid text without changing the last valid output')
        command(142);command(116)
        # Remove numbers with the built-in rule. Inline rules apply to the composed body.
        rule(3);expected('项目_角色 _v_定稿')
        checks.append('inline rules update both preview and native name; affixes remain independent')
        command(116);expected('项目_角色 _v_定稿')
        command(116);command(177);expected('项目_角色 012_v02_定稿')
        command(177);expected('项目_角色 _v_定稿')
        checks.append('collapsing keeps the rule; disabling and re-enabling are reversible')
        command(700);expected('项目_角色 012_v02_定稿')
        command(701);expected('项目_角色 _v_定稿')
        # Custom regex is the last combo entry.
        if not u.GetDlgItem(panel,174):command(116)
        field=u.GetDlgItem(panel,174);count=u.SendMessageW(field,0x146,0,0);rule(count)
        e.set_field(panel,172,'[')
        save_blocked()
        assert '表达式' in n.text(u.GetDlgItem(panel,176))
        e.set_field(panel,172,'.+');e.set_field(panel,173,'')
        save_blocked()
        assert host.poll() is None
        e.set_field(panel,172,r'项目_(.+) ([0-9]+)_v([0-9]+)_定稿');e.set_field(panel,173,'$2_$1')
        expected('012_角色')
        checks.append('invalid regex and empty results block save; capture groups produce the intended name')
        e.capture(panel,out/'inline-regex.png')
        assert n.text(u.GetDlgItem(panel,101))=='012_角色.md' and not u.GetDlgItem(panel,115)
        command(106);host.wait(timeout=8)
        assert (destination/'012_角色.md').read_text(encoding='utf-8')=='Jump native integration test\n'
        checks.append('explicit save writes the transformed name using the host format')
        report={'passed':checks};(out.parent/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False,indent=2))
    except Exception:
        print('Failure output: '+str(out),flush=True)
        if 'panel' in locals():
            print(json.dumps({'panel':[(u.GetDlgCtrlID(h),n.text(h),bool(u.IsWindowEnabled(h))) for h in n.children(panel)],
                              'dialog':[(u.GetDlgCtrlID(h),n.text(h)) for h in n.children(dialog)],
                              'foregroundClass':n.text(u.GetForegroundWindow(),u.GetClassNameW)},ensure_ascii=False),flush=True)
            if u.IsWindowVisible(panel):e.capture(panel,out/'failure.png')
        raise
    finally:
        if host and host.poll() is None:
            for h in n.windows(host.pid,'#32770',True):u.PostMessageW(h,0x111,2,u.GetDlgItem(h,2))
            try:host.wait(timeout=3)
            except subprocess.TimeoutExpired:host.terminate()
        for h in n.windows(app.pid,'Jump.Control'):u.PostMessageW(h,0x10,0,0)
        try:app.wait(timeout=3)
        except subprocess.TimeoutExpired:app.terminate()

if __name__=='__main__':run()
