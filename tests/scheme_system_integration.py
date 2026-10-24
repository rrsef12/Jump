"""Shared naming behavior across built-ins and fields with arbitrary identities."""
import configparser
import argparse
import ctypes as c
import json
import subprocess
import time
import uuid
from datetime import date
import native_integration as n
import extended_integration as e
import naming_menu as menu
from naming_fixture import pack, encode

u = n.u


def run(toggles_only=False):
    out = n.ROOT/'tests/output/scheme-system'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True)
    cfg = out/'settings.ini'

    def field(ident, label, default='', required=False, source=0, quick=True,
              choices=(), validation=0, width=0, condition=('', '')):
        meta = pack([default, '', '', '', *condition, str(int(quick)), '0', '0',
                     str(validation), str(width), str(source)])
        return pack([ident, label, default, str(int(required)), str(source), meta]+
                    [pack([value, value, '']) for value in choices])

    fields = [field('constant', '固定', 'FX', True, 3),
              field('kind', '类型', 'One', True, choices=('One', 'Two')),
              field('asset_name', '对象名称', required=True, quick=False),
              field('subject', '自定义项', 'Extra'),
              field('serial', '序号', '1', validation=2, width=3),
              field('timestamp', '动态日期', source=2, quick=False),
              field('department', '项目名称', source=1, quick=False)]
    definition = pack(['JNS2', 'shared-fixture', '自定义共用系统', '_', '1', '0', '0',
                       '0', '3', '0', '0', pack(['asset_name', '0', '']), *fields])
    parser = configparser.ConfigParser()
    parser['Jump'] = {'schema': '1', 'groups': '0', 'Favorite': '0', 'History': '0',
                      'recipes': '1', 'recentSubjects': '1', 'projects': '1'}
    (out/'work').mkdir(); (out/'delivery').mkdir()
    parser['Project0'] = {key: encode(value) for key, value in
                          {'id': 'fixture-project', 'name': 'Team', 'root': str(out),
                           'work': 'work', 'delivery': 'delivery'}.items()}
    parser['RecentSubject0'] = {'projectId': '', 'value': encode('中文旧名称')}
    parser['Recipe0'] = {'name': encode('自定义共用系统'), 'value': encode('Asset'),
                         'namingOnly': '1', 'complete': '1', 'structure': encode(definition)}
    with cfg.open('w', encoding='utf-16') as file:
        parser.write(file)
    app = host = panel = dialog = None
    checks = []

    def command(ident):
        n.wait(lambda: n.panel_ready(panel))
        n.activate(panel)
        u.SendMessageW(panel, 0x111, ident, u.GetDlgItem(panel, ident) or 0)

    def text(ident, value):
        target = n.wait(lambda: u.GetDlgItem(panel, ident))
        n.activate(panel)
        buffer = c.create_unicode_buffer(value)
        assert u.SendMessageW(target, 0xC, 0, c.addressof(buffer))
        return target

    def filled(value):
        n.wait(lambda: n.text(u.GetDlgItem(panel, 101)) == value+'.txt' and
               n.panel_ready(panel) and any(n.text(h, u.GetClassNameW) == 'Edit' and
               n.text(h) in (value, value+'.txt') for h in n.children(dialog)), label='shared output '+value)

    def pick_scheme(name):
        menu.select(panel, app.pid, ('命名方案', name))
        n.wait(lambda: n.text(u.GetDlgItem(panel, 792)) == '方案 · '+name and
               u.GetDlgItem(panel, 892) and n.panel_ready(panel), label='scheme '+name)

    def edit(index, value, close=True):
        command(830+index)
        editor = text(810+index, value)
        if close:
            u.PostMessageW(editor, 0x100, 13, 0)
            n.wait(lambda: not u.GetDlgItem(panel, 810+index))
        return editor

    def stop():
        if host and host.poll() is None:
            host.terminate(); host.wait(timeout=5)
        if app and app.poll() is None:
            for control in n.windows(app.pid, 'Jump.Control'):
                u.PostMessageW(control, 0x10, 0, 0)
            app.wait(timeout=5)

    try:
        app = subprocess.Popen([str(n.ROOT/'build/Jump.exe'), '--config', str(cfg),
                                '--test-ui', '--test-host-only'], creationflags=subprocess.CREATE_NO_WINDOW)
        n.wait(lambda: n.windows(app.pid, 'Jump.Control'))
        host = subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(out),
                                 '--name', 'Rock.txt'], creationflags=subprocess.CREATE_NO_WINDOW)
        n._fixture_hosts.append(host)
        dialog = n.wait(lambda: next(iter(n.windows(host.pid, '#32770', True)), None))
        n.activate(dialog)
        panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
        n.wait(lambda: u.GetDlgItem(panel, 892) and n.panel_ready(panel))

        # Every candidate entry point shares selection/omission, including required
        # secondary fields. Keep this regression runnable without unrelated checks.
        for name, index, before, removed in [
                ('资产文件', 2, 'Rock_模型', 'Rock'),
                ('游戏引擎资产', 0, 'SM_Rock_A', 'Rock_A'),
                ('贴图资源', 3, 'T_Rock_A_BC', 'T_Rock_A'),
                ('项目分类与编号', 1, 'PRJ_Prop_Rock_001', 'PRJ_Rock_001'),
                ('自定义共用系统', 1, 'FX_One_Rock_Extra_001_'+date.today().strftime('%Y%m%d'),
                 'FX_Rock_Extra_001_'+date.today().strftime('%Y%m%d'))]:
            pick_scheme(name); command(793)
            chip = 840+index*6
            if name == '资产文件': command(chip)
            filled(before)
            command(chip); filled(removed)
            assert not any(u.IsWindowVisible(u.GetDlgItem(panel, 900+i)) for i in range(8))
            assert '待修正' not in n.text(u.GetDlgItem(panel, 895))
            command(700); filled(before); command(701); filled(removed)
            command(chip); filled(before)
            command(789); command(780+index)
            command(800); filled(removed); command(800); filled(before)
            command(763); command(788)
            menu.select(panel, app.pid, '不添加', anchor=820+index); filled(removed)
            command(chip); filled(before)
            checks.append(name+': repeat click cancels/restores in both views, dropdown clear, undo/redo and native filename agree')

        pick_scheme('游戏引擎资产'); filled('SM_Rock_A')
        menu.select(panel, app.pid, '贴图 → T', anchor=820); filled('T_Rock_A_BC')
        command(858); filled('T_Rock_A'); command(858); filled('T_Rock_A_BC')
        menu.select(panel, app.pid, '贴图 → T', anchor=820); filled('Rock_A')
        n.wait(lambda: not u.GetDlgItem(panel, 858))
        menu.select(panel, app.pid, '贴图 → T', anchor=820); filled('T_Rock_A_BC')
        n.wait(lambda: u.GetDlgItem(panel, 858))
        checks.append('repeating a selected dropdown choice cancels it; conditional fields hide and return with their values')
        pick_scheme('贴图资源'); filled('T_Rock_A_BC')
        command(830); assert not u.GetDlgItem(panel, 810), 'fixed text remains read-only'
        editor = edit(1, '', close=False)
        n.wait(lambda: u.IsWindowVisible(u.GetDlgItem(panel, 901)))
        u.PostMessageW(editor, 0x100, 27, 0)
        n.wait(lambda: not u.GetDlgItem(panel, 811)); filled('T_Rock_A_BC')
        checks.append('fixed text and the required primary field retain their protection')
        if toggles_only:
            # Save and reopen the owned fixture: excluded required fields must
            # persist for the file without changing the reusable scheme defaults.
            pick_scheme('自定义共用系统'); command(846)
            saved_name='FX_Rock_Extra_001_'+date.today().strftime('%Y%m%d')
            filled(saved_name)
            n.activate(dialog); u.PostMessageW(dialog, 0x111, 1, 0)
            host.wait(timeout=10)
            assert (out/(saved_name+'.txt')).is_file()
            def recorded():
                try:
                    p=configparser.ConfigParser(); p.read(cfg, encoding='utf-16')
                    return p.getint('Jump', 'savedFiles', fallback=0)>0
                except (OSError, UnicodeError, ValueError, configparser.Error): return False
            n.wait(recorded, label='saved field omission')
            host = subprocess.Popen([str(n.ROOT/'build/Jump.TestHost.exe'), '--output', str(out),
                                     '--source', str(out/(saved_name+'.txt')), '--name', saved_name+'.txt'], creationflags=subprocess.CREATE_NO_WINDOW)
            n._fixture_hosts.append(host)
            dialog = n.wait(lambda: next(iter(n.windows(host.pid, '#32770', True)), None))
            n.activate(dialog)
            panel = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Panel', True)), None))
            filled(saved_name)
            command(846); filled('FX_One_Rock_Extra_001_'+date.today().strftime('%Y%m%d'))
            checks.append('saved filenames retain omitted fields on reopen; selecting restores the original candidate')
            report = {'passed': checks, 'output': str(out)}
            (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
            print(json.dumps(report, ensure_ascii=False, indent=2))
            return
        pick_scheme('资产文件'); command(793)

        for name, primary, compose in [
                ('资产文件', 1, lambda v: v),
                ('游戏引擎资产', 1, lambda v: 'SM_'+v+'_A'),
                ('贴图资源', 1, lambda v: 'T_'+v+'_A_BC'),
                ('项目分类与编号', 2, lambda v: 'PRJ_Prop_'+v+'_001'),
                ('自定义共用系统', 2, lambda v: 'FX_One_'+v+'_Extra_001_'+date.today().strftime('%Y%m%d'))]:
            pick_scheme(name); command(793); filled(compose('Rock'))
            assert all(u.GetDlgItem(panel, i) for i in [700, 701, 889, 894, 895])
            assert n.text(u.GetDlgItem(panel, 101), u.GetClassNameW) == 'Static'
            assert not u.GetDlgItem(panel, 115)
            editor = edit(primary, 'Tree', close=False); filled(compose('Tree'))
            u.PostMessageW(editor, 0x100, 27, 0)
            n.wait(lambda: not u.GetDlgItem(panel, 810+primary)); filled(compose('Rock'))
            edit(primary, 'Tree'); filled(compose('Tree'))
            edit(primary, 'Grove'); filled(compose('Grove'))
            menu.select(panel, app.pid, 'Tree', anchor=820+primary); filled(compose('Tree'))
            command(700); filled(compose('Grove')); command(701); filled(compose('Tree'))
            # The same history is reachable from the complete field editor.
            command(789); n.wait(lambda: u.GetDlgItem(panel, 780+primary))
            command(780+primary); n.wait(lambda: u.GetDlgItem(panel, 760))
            editor = u.GetDlgItem(panel, 760)
            menu.select(panel, app.pid, None, anchor=820+primary)
            assert u.GetDlgItem(panel, 760) == editor, 'cancelling recent names keeps the all-field editor open'
            menu.select(panel, app.pid, 'Grove', anchor=820+primary); filled(compose('Grove'))
            command(763); command(788)
            edit(primary, 'Rock'); filled(compose('Rock'))
            # Prefix, replacement and numbering all flow through the same output.
            command(142); text(186, 'P_'); filled('P_'+compose('Rock')); command(142)
            command(116); text(172, 'Rock'); text(173, 'Cliff')
            filled('P_'+compose('Cliff')); command(116)
            command(779); filled(compose('Rock'))
            command(794); filled(compose('Rock')+'_v001')
            command(793); filled(compose('Rock'))
            e.capture(panel, out/(name+'.png'))
            checks.append(name+': shared input, cancel, recent names in both views, undo/redo, rules and numbering')

        # A primary field may have any ID/position and may not be marked quick.
        assert e.rect(u.GetDlgItem(panel, 832)).top < e.rect(u.GetDlgItem(panel, 846)).top
        command(830); assert not u.GetDlgItem(panel, 810), 'fixed text cannot become an editor'
        command(831); text(811, 'Third'); filled('FX_Third_Rock_Extra_001_'+date.today().strftime('%Y%m%d'))
        command(888)
        assert u.IsWindowVisible(u.GetDlgItem(panel, 848)) and n.text(u.GetDlgItem(panel, 848)) == 'Third'
        command(833); text(813, 'Value'); command(888)
        assert not u.GetDlgItem(panel, 858), 'free-text fields must not acquire phantom chips'
        command(834); editor = text(814, 'bad')
        n.wait(lambda: u.IsWindowVisible(u.GetDlgItem(panel, 904)))
        assert '数字' in n.text(u.GetDlgItem(panel, 904))
        u.PostMessageW(editor, 0x100, 27, 0)
        n.wait(lambda: not u.GetDlgItem(panel, 814))
        command(789); n.wait(lambda: u.GetDlgItem(panel, 785)); command(785)
        text(760, 'ManualDate'); command(771)
        n.wait(lambda: n.text(u.GetDlgItem(panel, 760)) == date.today().strftime('%Y%m%d'))
        command(763); command(788)
        checks.append('arbitrary primary identity, fixed sources, growing choice lists, plain fields, numeric validation and dynamic date restoration')

        pick_scheme('游戏引擎资产'); filled('SM_Rock_A')
        menu.select(panel, app.pid, '贴图 → T', anchor=820); filled('T_Rock_A_BC')
        n.wait(lambda: u.GetDlgItem(panel, 858)); command(859); filled('T_Rock_A_N')
        menu.select(panel, app.pid, '静态模型 → SM', anchor=820); filled('SM_Rock_A')
        n.wait(lambda: not u.GetDlgItem(panel, 858))
        menu.select(panel, app.pid, None, anchor=821, exclude=['中文旧名称'])
        checks.append('conditions update both the output and controls; recent names obey the active field validation')

        pick_scheme('资产文件'); command(793); edit(1, '中文主体'); filled('中文主体')
        pick_scheme('游戏引擎资产')
        n.wait(lambda: u.IsWindowVisible(u.GetDlgItem(panel, 901)))
        assert '英文字母' in n.text(u.GetDlgItem(panel, 901))
        assert '待修正' in n.text(u.GetDlgItem(panel, 895))
        edit(1, 'Rock'); filled('SM_Rock_A')
        assert not u.IsWindowVisible(u.GetDlgItem(panel, 901))
        checks.append('incompatible inherited names remain editable, show a local explanation and never silently replace the native filename')
        native = next(h for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit' and n.text(h) == 'SM_Rock_A')
        n.activate(dialog); value = c.create_unicode_buffer('Manual_Name')
        u.SendMessageW(native, 0xC, 0, c.addressof(value)); filled('Manual_Name')
        editor = edit(1, 'Temporary', close=False); filled('SM_Temporary_A')
        u.PostMessageW(editor, 0x100, 27, 0)
        n.wait(lambda: not u.GetDlgItem(panel, 811)); filled('Manual_Name')
        command(700); filled('Manual_Name')
        checks.append('cancelling an edit after a native manual rename restores its literal baseline without reapplying template defaults or leaving an undo step')

        pick_scheme('自定义共用系统'); edit(2, 'Rock')
        command(5000)  # Select the owned fixture project.
        n.wait(lambda: u.IsWindowVisible(u.GetDlgItem(panel, 902)))
        assert n.text(u.GetDlgItem(panel, 832)) == '点击填写', 'changing project clears an arbitrary primary field'
        edit(2, 'Rock')
        project_name = 'FX_One_Rock_Extra_001_'+date.today().strftime('%Y%m%d')+'_Team'
        filled(project_name)
        command(789); n.wait(lambda: u.GetDlgItem(panel, 786)); command(786)
        text(760, 'Override'); command(770)
        n.wait(lambda: n.text(u.GetDlgItem(panel, 760)) == 'Team')
        filled(project_name); command(763); command(788)
        checks.append('project changes reset session values through scheme defaults; arbitrary project-source fields restore the current project')

        def delivery():
            n.activate(panel); u.PostMessageW(panel, 0x111, 753, 0)
            confirm = n.wait(lambda: next(iter(n.dialogs(app.pid, '应用交付方案')), None))
            u.PostMessageW(confirm, 0x111, 1, 0)
            n.wait(lambda: not u.IsWindow(confirm))
            n.wait(lambda: n.text(u.GetDlgItem(panel, 113)).lower() == str(out/'delivery').lower() and n.panel_ready(panel))

        delivery(); filled(project_name)
        command(789); n.wait(lambda: u.GetDlgItem(panel, 786))
        assert not u.GetDlgItem(panel, 787), 'delivery cannot append fields to the configured schema'
        command(788)
        pick_scheme('资产文件'); command(793); filled('Team_Rock')
        delivery(); filled('Team_Rock_交付')
        native = next(h for h in n.children(dialog) if n.text(h, u.GetClassNameW) == 'Edit' and n.text(h) == 'Team_Rock_交付')
        n.activate(dialog); value = c.create_unicode_buffer('Manual_Delivery')
        u.SendMessageW(native, 0xC, 0, c.addressof(value)); filled('Manual_Delivery')
        delivery(); filled('Team_Manual_Delivery_交付')
        assert '关闭' in n.text(u.GetDlgItem(panel, 889)), 'delivery cannot force numbering on'
        assert host.poll() is None
        checks.append('delivery uses only configured delivery choices, preserves numbering, supports schemes without stage fields and never submits Save')
        report = {'passed': checks, 'output': str(out)}
        (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(report, ensure_ascii=False, indent=2))
    except Exception:
        print('Failure output: '+str(out), flush=True)
        print('Panel:',panel,bool(u.IsWindowVisible(panel)),n.text(u.GetDlgItem(panel,101)),n.text(u.GetDlgItem(panel,114)),flush=True)
        print('Native edits:',[(h,n.text(h)) for h in n.children(dialog) if n.text(h,u.GetClassNameW)=='Edit'],flush=True)
        if panel and u.IsWindowVisible(panel): e.capture(panel, out/'failure.png')
        raise
    finally:
        stop()


if __name__ == '__main__':
    args=argparse.ArgumentParser(description=__doc__)
    args.add_argument('--toggles-only', action='store_true', help='Only candidate selection, clearing, undo and persistence')
    run(args.parse_args().toggles_only)
