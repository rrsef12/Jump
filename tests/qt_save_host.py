"""Owned QFileDialog fixture; optionally run with an application's bundled PySide6.

No application plugins are installed and no user documents are opened.
The result file is a test manifest, not an image exporter.
"""
import argparse
import json
import os
from pathlib import Path
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--output', required=True)
parser.add_argument('--qt-root')
parser.add_argument('--title', default='Save Image')
parser.add_argument('--open', action='store_true')
parser.add_argument('--delay-ms', type=int, default=0)
args = parser.parse_args()
dll_handle = os.add_dll_directory(args.qt_root) if args.qt_root else None
from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication, QDialog, QFileDialog, QWidget, QLineEdit, QDialogButtonBox

out = Path(args.output).resolve()
out.mkdir(parents=True, exist_ok=True)
app = QApplication(sys.argv[:1])
owner = QWidget()
owner.setWindowTitle('Jump owned Qt fixture')
owner.resize(720, 480)
owner.show()
dialog = QFileDialog(owner, args.title, str(out))
dialog.setOption(QFileDialog.DontUseNativeDialog, True)
dialog.setAcceptMode(QFileDialog.AcceptOpen if args.open else QFileDialog.AcceptSave)
dialog.setFileMode(QFileDialog.ExistingFile if args.open else QFileDialog.AnyFile)
dialog.setNameFilters(['Portable Network Graphics files (*.png)', 'JPEG images (*.jpg *.jpeg)'])
dialog.setDefaultSuffix('png')

def record_state():
    edit = dialog.findChild(QLineEdit, 'fileNameEdit')
    buttons = dialog.findChild(QDialogButtonBox, 'buttonBox')
    save = buttons.button(QDialogButtonBox.Save)
    state = {'name': edit.text(), 'directory': dialog.directory().absolutePath(),
             'filter': dialog.selectedNameFilter(), 'saveEnabled': bool(save and save.isEnabled())}
    pending = out/'state.tmp'
    pending.write_text(json.dumps(state, ensure_ascii=False), encoding='utf-8')
    pending.replace(out/'state.json')

recorder = QTimer()
recorder.timeout.connect(record_state)
recorder.start(150)

def finished(result):
    (out/'result.json').write_text(json.dumps({'accepted': result == QDialog.Accepted,
        'selected': dialog.selectedFiles(), 'filter': dialog.selectedNameFilter()}, ensure_ascii=False), encoding='utf-8')
    app.quit()

dialog.finished.connect(finished)
QTimer.singleShot(args.delay_ms, dialog.open)
sys.exit(app.exec())
