"""Owned, factory-startup Blender UI host; never opens a user project."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import shutil

import bpy

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/output/blender'
OUT.mkdir(parents=True, exist_ok=True)
settings = OUT / 'host-settings.json'
settings.write_text(json.dumps({'pid': os.getpid(), 'executable': bpy.app.binary_path}), encoding='utf-8')
bridge_folder = OUT / 'bridge'
bridge_folder.mkdir(exist_ok=True)
shutil.copyfile(ROOT / 'integrations/blender/jump_save_bridge.py', bridge_folder / 'jump_save_bridge.py')
(bridge_folder / 'jump_save_bridge.json').write_text(json.dumps({'apps': [str(ROOT / 'build/Jump.exe')]}), encoding='utf-8')
spec = importlib.util.spec_from_file_location('jump_save_bridge_test', bridge_folder / 'jump_save_bridge.py')
bridge = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = bridge
spec.loader.exec_module(bridge)
bridge.register()

state = {'case': '', 'operator': '', 'filename': '', 'directory': '', 'error': '', 'pid': os.getpid()}
last = None


def watch():
    global last
    browsers = bridge._browsers()
    state['browser_count'] = len(browsers)
    state['bridge_timer'] = bpy.app.timers.is_registered(bridge._tick)
    state['windows'] = [(w[0], w[2], w[3]) for w in bridge._windows() if w[1] == os.getpid()]
    if len(browsers) == 1:
        space = browsers[0][2]
        state.update(operator=space.active_operator.bl_idname, filename=space.params.filename,
                     directory=os.fsdecode(space.params.directory), extension=bridge._extension(space))
    if state != last:
        temp = OUT / 'state.tmp'
        temp.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
        temp.replace(OUT / 'state.json')
        last = state.copy()
    command_file = OUT / 'command.json'
    if command_file.exists():
        command = json.loads(command_file.read_text(encoding='utf-8'))
        command_file.unlink()
        try:
            if command['action'] == 'quit':
                bpy.ops.wm.quit_blender()
                return None
            if command['action'] == 'cancel':
                window, area, space = bridge._browsers()[0]
                with bpy.context.temp_override(window=window, area=area):
                    bpy.ops.file.cancel()
            elif command['action'] == 'open':
                if browsers:
                    raise ValueError('Test browser is already open')
                kind = command['kind']
                state['case'] = kind
                destination = OUT / ('original.' + {'blend': 'blend', 'fbx': 'fbx', 'gltf': 'glb'}[kind])
                if kind == 'blend':
                    bpy.ops.wm.save_as_mainfile('INVOKE_DEFAULT', filepath=str(destination), check_existing=True)
                elif kind == 'fbx':
                    bpy.ops.export_scene.fbx('INVOKE_DEFAULT', filepath=str(destination))
                elif kind == 'gltf':
                    bpy.ops.export_scene.gltf('INVOKE_DEFAULT', filepath=str(destination), export_format='GLB')
        except Exception as exc:
            state['error'] = str(exc)
    return 0.15


bpy.app.timers.register(watch, first_interval=0.5)
