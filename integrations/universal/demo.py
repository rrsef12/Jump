"""Runnable canvas-only save destination example; never writes a document."""
import argparse
import json
from pathlib import Path
import tkinter as tk
from jump_save_bridge import Bridge, Snapshot

p = argparse.ArgumentParser()
p.add_argument('--jump', required=True, help='Trusted absolute Jump.exe path')
p.add_argument('--directory', required=True)
p.add_argument('--state', help='Optional test instrumentation JSON')
p.add_argument('--ignore-apply', action='store_true')
p.add_argument('--attach-delay', type=int, default=0, help='Demonstrate a save operation beginning in an already visible window (ms)')
args = p.parse_args()
root = tk.Tk()
root.title('Canvas output destination')  # Deliberately not a Save/Export title.
root.geometry('720x300+100+100')
canvas = tk.Canvas(root, bg='#25272d', highlightthickness=0)
canvas.pack(fill='both', expand=True)
current = Snapshot(str(Path(args.directory).resolve()), 'original.asset', '.asset', 'Asset')
applies = 0

def draw():
    canvas.delete('all')
    canvas.create_text(25, 25, anchor='nw', fill='white', font=('Segoe UI', 15), text='Jump API — pending save destination')
    canvas.create_text(25, 80, anchor='nw', fill='#b7d5ff', width=660, text=current.directory)
    canvas.create_text(25, 145, anchor='nw', fill='white', text=current.name)
    canvas.create_text(25, 220, anchor='nw', fill='#bbbbbb', text='Demo only. No document is saved. Close this window to detach.')
    if args.state:
        state = Path(args.state)
        temp = state.with_suffix('.tmp')
        temp.write_text(json.dumps({**current.__dict__, 'applies': applies, 'submissions': 0}, ensure_ascii=False), encoding='utf-8')
        temp.replace(state)

def apply(directory, name):
    global current, applies
    applies += 1
    if not args.ignore_apply:
        current = Snapshot(directory, name, current.extension, current.format)
    draw()

bridge = Bridge([args.jump])
root.update()
# Tk's public HWND is a child of its top-level wrapper on Windows.
import ctypes
from ctypes import wintypes as wt
ctypes.windll.user32.GetAncestor.argtypes = [wt.HWND, wt.UINT]
ctypes.windll.user32.GetAncestor.restype = wt.HWND
target_window = ctypes.windll.user32.GetAncestor(root.winfo_id(), 2)
if args.attach_delay:
    root.after(args.attach_delay, lambda: bridge.attach(target_window, lambda: current, apply))
else:
    bridge.attach(target_window, lambda: current, apply)
draw()

def tick():
    bridge.tick()
    root.after(15, tick)

def close():
    bridge.detach()
    root.destroy()

root.protocol('WM_DELETE_WINDOW', close)
root.after(15, tick)
root.mainloop()
