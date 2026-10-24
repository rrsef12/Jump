"""Fast protocol boundary checks without opening or controlling any application."""
import contextlib
import importlib.util
import os
from pathlib import Path
import sys
from types import SimpleNamespace as Obj, ModuleType
import unittest

ROOT = Path(__file__).resolve().parents[1]
bpy = ModuleType('bpy')
app = ModuleType('bpy.app')
handlers = ModuleType('bpy.app.handlers')
handlers.persistent = lambda fn: fn
sys.modules.update({'bpy': bpy, 'bpy.app': app, 'bpy.app.handlers': handlers})
spec = importlib.util.spec_from_file_location('bridge_checks', ROOT / 'integrations/blender/jump_save_bridge.py')
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)


class BridgeChecks(unittest.TestCase):
    def setUp(self):
        self.directory = ROOT / 'tests/output/blender/checks'
        self.directory.mkdir(parents=True, exist_ok=True)
        self.operator = Obj(bl_idname='EXPORT_SCENE_OT_fbx', filename_ext='.fbx', keep_options={'selection': True})
        self.params = Obj(filename='原文件.fbx', directory=os.fsencode(str(self.directory)), filter_glob='*.fbx')
        self.space = Obj(active_operator=self.operator, params=self.params)
        self.area = Obj(tag_redraw=lambda: None, regions=[Obj(type='WINDOW')])
        self.exports = 0
        bpy.data = Obj(filepath='D:/Project/current.blend')
        bpy.context = Obj(temp_override=lambda **kw: contextlib.nullcontext())
        bpy.ops = Obj(file=Obj(execute=self.execute))
        bridge._browsers = lambda: [(Obj(), self.area, self.space)]
        bridge._native_browser = lambda: 10
        bridge._exe = lambda pid: 'jump' if pid == 55 else 'other'
        bridge._allowed = {'jump'}
        bridge._pid = lambda hwnd: {10: os.getpid(), 20: 55}.get(hwnd, 0)
        bridge._text = lambda hwnd, cls=False: 'Jump.Panel' if hwnd == 20 else ''
        bridge.user = Obj(GetForegroundWindow=lambda: 20, IsWindowEnabled=lambda h: True)

    def execute(self):
        self.exports += 1
        return {'FINISHED'}

    def request(self, verb='APPLY', name='角色_场景', extension='.fbx', directory='', submit='0', pid=55):
        return bridge._serve('\n'.join([bridge.PROTOCOL, verb, '10', directory, name, extension, submit, '20']), pid)

    def test_read_apply_and_original_export_options(self):
        self.assertIn('原文件.fbx', self.request('INSPECT'))
        self.request()
        self.assertEqual(self.params.filename, '角色_场景.fbx')
        self.assertEqual(self.operator.keep_options, {'selection': True})
        self.assertEqual(self.exports, 0)
        self.request(submit='1')
        self.assertEqual(self.exports, 1)

    def test_identity_focus_and_format_changes_reject_without_writes(self):
        for kwargs in [{'pid': 99}, {'extension': '.obj'}]:
            with self.assertRaises(ValueError): self.request(**kwargs)
        bridge.user.GetForegroundWindow = lambda: 999
        with self.assertRaises(ValueError): self.request()
        bridge.user.GetForegroundWindow = lambda: 20
        bridge._browsers = lambda: []
        with self.assertRaises(ValueError): self.request()
        self.assertEqual(self.params.filename, '原文件.fbx')
        self.assertEqual(self.exports, 0)

    def test_invalid_path_and_name_cannot_export(self):
        for kwargs in [{'name': '../bad'}, {'name': ''}, {'name': 'bad\nname'},
                       {'directory': str(self.directory / 'does-not-exist')}]:
            with self.assertRaises(ValueError): self.request(submit='1', **kwargs)
        self.assertEqual(self.params.filename, '原文件.fbx')
        self.assertEqual(self.exports, 0)

    def test_directory_navigation_and_read_only_open(self):
        child = self.directory / 'destination'
        child.mkdir(exist_ok=True)
        self.assertEqual(self.request(directory=str(child)).split('\n')[-1], '1')
        self.assertEqual(Path(os.fsdecode(self.params.directory)), child)
        self.operator.bl_idname = 'WM_OT_open_mainfile'
        self.params.filename = 'original.blend'
        with self.assertRaises(ValueError): self.request(extension='.blend', submit='1')
        self.request(extension='.blend')
        self.assertEqual(self.params.filename, 'original.blend')

    def test_gltf_glb_and_blend_formats(self):
        self.operator.bl_idname = 'EXPORT_SCENE_OT_gltf'
        self.operator.export_format = 'GLB'
        self.operator.filename_ext = '.gltf'
        self.params.filename = 'original.glb'
        self.params.filter_glob = '*.glb;*.gltf'
        self.assertEqual(bridge._extension(self.space), '.glb')
        self.operator.export_format = 'GLTF_SEPARATE'
        self.assertEqual(bridge._extension(self.space), '.gltf')
        self.operator.bl_idname = 'WM_OT_save_as_mainfile'
        self.assertEqual(bridge._extension(self.space), '.blend')


if __name__ == '__main__':
    unittest.main()
