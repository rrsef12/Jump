"""Owned custom picker with a transient address editor, independent of QFileDialog."""
import argparse
import json
import os
from pathlib import Path
import sys

p = argparse.ArgumentParser()
p.add_argument('--output', required=True)
p.add_argument('--qt-root', required=True)
p.add_argument('--ambiguous', action='store_true')
p.add_argument('--ignore-navigation', action='store_true')
args = p.parse_args()
dll = os.add_dll_directory(args.qt_root)
from PySide6.QtCore import QTimer, QEvent
from PySide6.QtWidgets import QApplication, QDialog, QGroupBox, QHBoxLayout, QVBoxLayout, QCheckBox, QPushButton, QLineEdit, QLabel

out = Path(args.output).resolve()
app = QApplication(sys.argv[:1])
dialog = QDialog()
dialog.setWindowTitle('Save custom document')
dialog.resize(720, 400)
layout = QVBoxLayout(dialog)
directory = ''
submissions = 0
edit_opens = 0

class Address(QGroupBox):
    def __init__(self):
        super().__init__()
        row = QHBoxLayout(self)
        self.bar = QGroupBox()
        self.bar_layout = QHBoxLayout(self.bar)
        self.crumbs = QGroupBox()
        self.crumb_layout = QHBoxLayout(self.crumbs)
        self.button = QPushButton('')
        self.button.setAutoDefault(False)
        self.button.setMinimumWidth(100)
        self.bar_layout.addWidget(self.crumbs)
        self.bar_layout.addWidget(self.button)
        self.editor = QLineEdit()
        self.closing_editor = False
        row.addWidget(self.bar)
        row.addWidget(self.editor)
        self.editor.hide()
        self.button.clicked.connect(self.open)
        self.editor.returnPressed.connect(self.navigate)
        self.editor.installEventFilter(self)
        self.rebuild()

    def rebuild(self):
        while self.crumb_layout.count():
            item = self.crumb_layout.takeAt(0)
            widget = item.widget()
            widget.hide()
            widget.setParent(None)
            widget.deleteLater()
        self.crumb_layout.addWidget(QCheckBox('This PC'))
        if directory:
            self.crumb_layout.addWidget(QCheckBox(''))
            self.crumb_layout.addWidget(QCheckBox(Path(directory).name))

    def open(self):
        global edit_opens
        edit_opens += 1
        self.bar.hide()
        self.editor.setText(directory or 'This PC')
        self.editor.show()
        self.editor.setFocus()
        self.editor.selectAll()

    def navigate(self):
        global directory
        if not args.ignore_navigation and Path(self.editor.text()).is_dir():
            directory = str(Path(self.editor.text()).resolve())
        self.close_editor()
        name.setFocus()

    def close_editor(self):
        if not self.editor.isVisible() or self.closing_editor:
            return
        self.closing_editor = True
        self.editor.hide()
        self.bar.show()
        self.rebuild()
        self.closing_editor = False

    def eventFilter(self, watched, event):
        if watched is self.editor and event.type() == QEvent.FocusOut:
            self.close_editor()
        return super().eventFilter(watched, event)

address = Address()
layout.addWidget(address)
if args.ambiguous:
    layout.addWidget(Address())
layout.addWidget(QLabel('Owned fixture; no document is written.'))
layout.addStretch()
name = QLineEdit('original.docx')
name.setAccessibleName('File name:')
layout.addWidget(name)
save = QPushButton('Save')
save.setAutoDefault(False)
def submitted():
    global submissions
    submissions += 1
save.clicked.connect(submitted)
layout.addWidget(save)

def record():
    data = {'directory': directory, 'name': name.text(), 'submissions': submissions, 'addressOpens': edit_opens,
            'editorVisible':address.editor.isVisible(), 'barVisible':address.bar.isVisible(),
            'crumbs':address.crumb_layout.count()}
    pending = out/'state.tmp'
    pending.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8')
    pending.replace(out/'state.json')
timer = QTimer()
timer.timeout.connect(record)
timer.start(100)
dialog.show()
name.setFocus()
sys.exit(app.exec())
