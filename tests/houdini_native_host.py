"""Owned Houdini native-chooser test host. Use isolated preferences and normal UI exit."""
import json
import os
from pathlib import Path
import traceback
import hou
from PySide6 import QtCore, QtWidgets
import jump_scene_save as bridge

OUT = Path(os.environ['JUMP_NATIVE_OUT'])
APP = QtWidgets.QApplication.instance()
state = {'pid': os.getpid(), 'registered_at_startup': bridge._filter is not None, 'error': ''}
last = None
hou.node('/obj').createNode('geo', 'jump_owned_native_test')


def observe():
    global last
    try:
        state['scene'] = hou.hipFile.path()
        state['pipe_timer'] = bridge._timer.isActive() if bridge._timer else False
        state['windows'] = [{'title': w.windowTitle(), 'id': int(w.winId()), 'name': w.objectName(),
                             'class': w.metaObject().className()} for w in APP.topLevelWidgets() if w.isVisible()]
        state['choosers'] = []
        for widget in bridge._candidates():
            try:
                state['choosers'].append({'id': int(widget.winId()), 'snapshot': bridge._snapshot(widget)})
            except ValueError:
                pass
        if state != last:
            temporary = OUT / 'state.tmp'
            temporary.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
            temporary.replace(OUT / 'state.json')
            last = state.copy()
        command = OUT / 'command.json'
        if command.exists():
            action = json.loads(command.read_text(encoding='utf-8-sig'))
            command.unlink()
            if action['action'] == 'prepare-exit':
                timer.stop()
                hou.hipFile.clear(suppress_save_prompt=True)
            elif action['action'] == 'external':
                QtCore.QTimer.singleShot(10, lambda: hou.ui.selectFile(title='Save As',
                    file_type=hou.fileType.Hip, chooser_mode=hou.fileChooserMode.Write))
    except Exception:
        state['error'] = traceback.format_exc()


timer = QtCore.QTimer(APP)
timer.timeout.connect(observe)
timer.start(200)
APP.setProperty('jump_native_test', timer)
