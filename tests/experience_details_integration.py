"""Focused checks for filename copy, subject removal, path display and local errors."""
import configparser
import ctypes as c
import json
import subprocess
import time
import uuid
from pathlib import Path
import win32clipboard as clipboard
import native_integration as n
import extended_integration as e
import naming_menu as menu

u=n.u

def run():
    out=n.ROOT/'tests/output/experience-details'/uuid.uuid4().hex[:10]
    folder=out/'很长的项目目录_用于确认路径显示'/'海港项目'/'角色资产'/'模型交付'
    folder.mkdir(parents=True)
    cfg=out/'settings.ini'
    enc=lambda text:text.encode('utf-16-be').hex().upper()
    cfg.write_text('[Jump]\nschema=1\nFavorite=0\nHistory=0\nrecipes=0\nrecentSubjects=3\n'+
        ''.join(f'\n[RecentSubject{i}]\nprojectId=\nvalue={enc(name)}\n'
                for i,name in enumerate(['保留主体','移除主体','另一个主体'])),encoding='utf-16')
    app=host=None;panel=None;checks=[];original_clipboard=[];copy_sequence=None
    def open_clipboard():
        def ready():
            try:clipboard.OpenClipboard();return True
            except Exception:return False
        n.wait(ready,label='clipboard available')
    open_clipboard()
    try:
        kind=clipboard.EnumClipboardFormats(0)
        while kind:
            try:
                value=clipboard.GetClipboardData(kind)
                if isinstance(value,(str,bytes)):original_clipboard.append((kind,value))
            except Exception:pass
            kind=clipboard.EnumClipboardFormats(kind)
    finally:clipboard.CloseClipboard()
    def start():
        nonlocal app,panel
        app=subprocess.Popen([str(n.ROOT/'build/Jump.exe'),'--config',str(cfg),'--test-ui','--test-host-only'],creationflags=subprocess.CREATE_NO_WINDOW)
        control=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Control')),None))
        n.activate(dialog);u.PostMessageW(control,0x312,3,0)
        panel=n.wait(lambda:next(iter(n.windows(app.pid,'Jump.Panel',True)),None))
        n.wait(lambda:u.GetDlgItem(panel,892) and n.panel_ready(panel))
    def stop():
        if app and app.poll() is None:
            for control in n.windows(app.pid,'Jump.Control'):u.PostMessageW(control,0x10,0,0)
            app.wait(timeout=5)
    def command(ident):
        n.activate(panel);u.SendMessageW(panel,0x111,ident,u.GetDlgItem(panel,ident) or 0)
    def text(ident,value):
        target=u.GetDlgItem(panel,ident);assert target
        n.activate(panel);buffer=c.create_unicode_buffer(value)
        u.SendMessageW(target,0xC,0,c.addressof(buffer));return target
    def visible(ident):
        target=u.GetDlgItem(panel,ident);return target and u.IsWindowVisible(target)
    def hint(ident):n.wait(lambda:visible(ident) and n.text(u.GetDlgItem(panel,ident)),label='local error '+str(ident))
    try:
        host=subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'),'--output',str(folder),'--name','Check.txt'],creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog=n.wait(lambda:next(iter(n.windows(host.pid,'#32770',True)),None))
        start()
        baseline=n.text(u.GetDlgItem(panel,101))
        assert n.text(u.GetDlgItem(panel,113))==str(folder)
        e.capture(panel,out/'tail-path.png')
        checks.append('long path retains its full value and tooltip; drawing prioritizes trailing directories')

        preview=u.GetDlgItem(panel,101)
        u.PostMessageW(preview,0x7B,preview,-1)
        popup=n.wait(lambda:next(iter(n.windows(app.pid,'#32768',True)),None),label='filename context menu')
        handle=u.SendMessageW(popup,0x1E1,0,0)
        assert menu.labels(handle)==['复制完整文件名']
        menu.select_item(popup,handle,0)
        n.wait(lambda:'已复制' in n.text(u.GetDlgItem(panel,895)))
        copy_sequence=clipboard.GetClipboardSequenceNumber()
        open_clipboard()
        try:assert clipboard.GetClipboardData(13)==baseline
        finally:clipboard.CloseClipboard()
        n.wait(lambda:n.text(u.GetDlgItem(panel,895))=='最终文件名',label='copy feedback clears')
        assert n.text(u.GetDlgItem(panel,101))==baseline and host.poll() is None
        checks.append('right-click copies the complete filename; feedback clears and no save is submitted')

        menu.select(panel,app.pid,('管理记录','删除：移除主体'),anchor=821)
        assert n.text(u.GetDlgItem(panel,101))==baseline
        root=menu.select(panel,app.pid,anchor=821)
        assert '移除主体' not in root and '保留主体' in root
        stop();start()
        root=menu.select(panel,app.pid,anchor=821)
        assert '移除主体' not in root and '保留主体' in root
        checks.append('single subject removal persists across restart without changing the current name')

        command(831);editor=u.GetDlgItem(panel,811);base=e.rect(u.GetDlgItem(panel,852)).top
        text(811,'bad/name');hint(901)
        assert not visible(114) and u.GetPropW(editor,'Jump.InputError')
        assert e.rect(u.GetDlgItem(panel,901)).top>=e.rect(editor).bottom
        assert e.rect(u.GetDlgItem(panel,852)).top>base
        e.capture(panel,out/'subject-error.png')
        text(811,'移除主体')
        n.wait(lambda:not visible(901) and n.panel_ready(panel))
        assert u.GetDlgItem(panel,811)==editor and e.rect(u.GetDlgItem(panel,852)).top==base
        command(888)
        root=menu.select(panel,app.pid,anchor=821)
        assert '移除主体' in root
        checks.append('subject error appears below its editor; correction preserves input and restores layout; reused names rejoin recents')

        command(789);command(781);editor=u.GetDlgItem(panel,760)
        text(760,'bad/name');hint(901)
        assert u.GetDlgItem(panel,760)==editor and not visible(114)
        u.SendMessageW(editor,0x100,27,0);n.wait(lambda:not u.GetDlgItem(panel,760));command(788)

        command(142);text(186,'bad/');hint(908);text(187,'bad/');hint(909)
        assert e.rect(u.GetDlgItem(panel,909)).top>e.rect(u.GetDlgItem(panel,908)).bottom
        e.capture(panel,out/'affix-errors.png')
        text(186,'');text(187,'');n.wait(lambda:not visible(908) and not visible(909) and n.panel_ready(panel));command(142)

        command(116);combo=u.GetDlgItem(panel,174)
        count=u.SendMessageW(combo,0x146,0,0)
        u.SendMessageW(combo,0x14E,count-1,0);u.SendMessageW(panel,0x111,174|(1<<16),combo)
        text(172,'[');hint(911)
        assert e.rect(u.GetDlgItem(panel,911)).top>=e.rect(u.GetDlgItem(panel,172)).bottom
        assert not visible(114)
        e.capture(panel,out/'replacement-error.png')
        command(178);n.wait(lambda:not visible(911) and n.panel_ready(panel));command(116)

        command(755);editor=text(896,'bad/name');hint(910)
        text(896,'完整名称');n.wait(lambda:not visible(910) and n.panel_ready(panel))
        assert u.GetDlgItem(panel,896)==editor
        command(755)
        checks.append('advanced fields, prefix, suffix, regex and full-name errors stay next to their inputs and clear on correction')
        e.capture(panel,out/'completed.png')
        result={'passed':checks,'output':str(out)}
        (out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(result,ensure_ascii=False,indent=2))
    except Exception:
        if panel and u.IsWindow(panel):e.capture(panel,out/'failure.png')
        print('Failure output: '+str(out),flush=True);raise
    finally:
        stop()
        if host and host.poll() is None:
            for owned in n.windows(host.pid,'#32770',True):u.PostMessageW(owned,0x111,2,0)
            host.wait(timeout=5)
        if copy_sequence is not None and clipboard.GetClipboardSequenceNumber()==copy_sequence:
            open_clipboard()
            try:
                clipboard.EmptyClipboard()
                for kind,value in original_clipboard:clipboard.SetClipboardData(kind,value)
            finally:clipboard.CloseClipboard()

if __name__=='__main__':run()
