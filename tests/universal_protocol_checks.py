"""Protocol failure checks on an owned, visible host window; no user apps."""
import os
from pathlib import Path
import sys
import tempfile
import tkinter as tk
import ctypes
from ctypes import wintypes as wt
import native_integration as n

sys.path.insert(0, str(n.ROOT/'integrations/universal'))
from jump_save_bridge import Bridge, Snapshot, PROTOCOL


def run():
    root = tk.Tk(); root.title('Jump owned bridge protocol checks'); root.update()
    n.u.GetAncestor.argtypes = [wt.HWND, wt.UINT]; n.u.GetAncestor.restype = wt.HWND
    window = n.u.GetAncestor(root.winfo_id(), 2)
    n.activate(window)
    bridge = Bridge([str(n.ROOT/'build/Jump.exe')])
    calls = []
    checks = []
    with tempfile.TemporaryDirectory(prefix='jump-bridge-checks-') as directory:
        state = Snapshot(directory, 'original.asset', '.asset', 'Asset')
        def apply(path, name):
            nonlocal state
            calls.append((path, name))
            state = Snapshot(path, name, '.asset', 'Asset')
        def packet(verb='INSPECT', extra=None):
            fields = [PROTOCOL, verb, str(window), '', '', '', '0', '0']
            if extra is not None: fields += extra
            return fields
        def send(fields): return bridge._serve('\n'.join(fields), os.getpid()).split('\n')
        def refused(fields, message):
            count = len(calls)
            try: send(fields)
            except (ValueError, OSError): pass
            else: raise AssertionError(message)
            assert len(calls) == count, message
            checks.append(message)
        try:
            bridge.attach(window, lambda: state, apply)
            refused(packet(), 'untrusted client executable rejected')
            bridge.allowed = {os.path.normcase(str(Path(sys.executable).resolve()))}
            first = send(packet())
            assert len(first) == 12 and first[1] == 'OK' and first[11] == '0'
            expectation = [state.name, state.directory, state.extension, first[10]]
            request = packet('APPLY', expectation)
            request[3:5] = [directory, '新名称.asset']
            wrong = request.copy(); wrong[6] = '1'; refused(wrong, 'Save submission forbidden')
            wrong = request.copy(); wrong[2] = '0'; refused(wrong, 'wrong target HWND rejected')
            wrong = request.copy(); wrong[4] = '../escape.asset'; refused(wrong, 'path traversal in filename rejected')
            wrong = request.copy(); wrong[4] = 'CON.asset'; refused(wrong, 'Windows reserved filename rejected')
            wrong = request.copy(); wrong[3] = str(Path(directory)/'missing'); refused(wrong, 'nonexistent destination rejected')
            wrong = request.copy(); wrong[-1] = 'outdated'; refused(wrong, 'outdated revision rejected before callback')
            state = Snapshot(directory, 'user-edited.asset', '.asset', 'Asset')
            refused(request, 'user edit between inspection and apply is preserved')
            state = Snapshot(directory, 'original.asset', '.asset', 'Asset')
            bridge.attach(window, lambda: state, apply)
            refused(request, 'previous session revision rejected after reattachment')
            latest = send(packet()); request[-1] = latest[10]
            reply = send(request)
            assert reply[3] == '新名称.asset' and reply[8] == '0' and len(calls) == 1
            checks.append('valid transaction updates destination with explicit no-save reply')
            submitted = []
            bridge.submit = lambda: submitted.append(True)
            current = send(packet()); request[8:] = [state.name, state.directory, state.extension, current[10]]
            request[6] = '1'; request[4] = '提交名称.asset'
            reply = send(request)
            assert reply[8] == '1' and reply[11] == '1' and submitted == [True]
            checks.append('optional submit callback invokes the original host action only when explicitly requested')
            current = send(packet()); request[8:] = [state.name,state.directory,state.extension,current[10]]
            request[4] = 'ignored.asset'
            bridge.apply = lambda *_: None
            refused(request, 'ignored host callback rejected by fresh state readback')
            bridge.read = lambda: None
            refused(packet(), 'no active save operation rejected')
        finally:
            bridge.detach(); root.destroy()
    print('\n'.join('PASS '+check for check in checks))


if __name__ == '__main__': run()
