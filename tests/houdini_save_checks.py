"""Native-chooser IPC boundaries without opening Houdini or saving a scene."""
import importlib.util
import ntpath
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests/output/houdini-native'
OUT.mkdir(parents=True, exist_ok=True)
widget = Mock()
widget.winId.return_value = 42
widget.isVisible.return_value = True
widget.objectName.return_value = 'RE_Window'
widget.windowTitle.return_value = 'Save As'
fake_hou = types.SimpleNamespace(hipFile=types.SimpleNamespace(path=lambda: str(OUT / 'source.hip')),
    expandString=lambda s: s.replace('$HIP', str(OUT)))
fake_qt = types.ModuleType('PySide6')
fake_qt.QtCore = types.SimpleNamespace(QObject=object)
fake_qt.QtWidgets = types.SimpleNamespace(QApplication=types.SimpleNamespace(topLevelWidgets=lambda: [widget]))
fake_pipe = types.SimpleNamespace(executable=Mock(return_value='trusted'), pid=Mock(return_value=9),
    window_class=Mock(return_value='Jump.Panel'), user=types.SimpleNamespace(
        GetForegroundWindow=Mock(return_value=42), IsWindowEnabled=Mock(return_value=True)))
spec = importlib.util.spec_from_file_location('native_houdini_test', ROOT / 'integrations/houdini/jump_scene_save.py')
adapter = importlib.util.module_from_spec(spec)
with patch.dict(sys.modules, {'hou': fake_hou, 'PySide6': fake_qt,
        'shiboken6': types.SimpleNamespace(getCppPointer=lambda w: (123,)), 'jump_pipe': fake_pipe}):
    spec.loader.exec_module(adapter)


class NativeBoundaryChecks(unittest.TestCase):
    def setUp(self):
        adapter._external = 0
        adapter._allowed = {'trusted'}
        adapter._native = types.SimpleNamespace(jump_houdini_inspect=Mock(return_value=
            b'OK\noriginal.hip\n$HIP/\n*.hip,*.hiplc,*.hipnc'), jump_houdini_apply=Mock(return_value=b'OK\nfilled.hip\n$HIP/\n*.hip'))
        fake_pipe.executable.return_value = 'trusted'
        fake_pipe.user.GetForegroundWindow.return_value = 42
        fake_pipe.user.IsWindowEnabled.return_value = True

    def request(self, name='角色设定_场景参考', directory='', extension='.hip', submit='0', hwnd='42'):
        return '\n'.join([adapter.PROTOCOL, 'APPLY', hwnd, directory, name, extension, submit, '77'])

    def test_unicode_fill_preserves_extension_without_submit(self):
        reply = adapter._serve(self.request(), 9).split('\n')
        self.assertEqual(reply[3], '角色设定_场景参考.hip')
        self.assertEqual(reply[8], '0')
        self.assertFalse(adapter._native.jump_houdini_apply.call_args.args[-1])

    def test_original_accept_is_explicit(self):
        self.assertEqual(adapter._serve(self.request(submit='1'), 9).split('\n')[8], '1')
        self.assertTrue(adapter._native.jump_houdini_apply.call_args.args[-1])

    def test_untrusted_client_and_stale_window(self):
        fake_pipe.executable.return_value = 'other'
        with self.assertRaises(ValueError): adapter._serve(self.request(), 9)
        fake_pipe.executable.return_value = 'trusted'
        with self.assertRaises(ValueError): adapter._serve(self.request(hwnd='999'), 9)
        adapter._native.jump_houdini_apply.assert_not_called()

    def test_focus_and_modal_guard(self):
        fake_pipe.user.GetForegroundWindow.return_value = 888
        with self.assertRaises(ValueError): adapter._serve(self.request(), 9)
        fake_pipe.user.GetForegroundWindow.return_value = 42
        fake_pipe.user.IsWindowEnabled.return_value = False
        with self.assertRaises(ValueError): adapter._serve(self.request(), 9)
        adapter._native.jump_houdini_apply.assert_not_called()

    def test_format_change_and_invalid_name_do_not_mutate(self):
        for request in [self.request(extension='.hipnc'), self.request(name='bad/name'),
                        self.request(name='bad\tname'), self.request(name=''), self.request(name='name.')]:
            with self.subTest(request=request), self.assertRaises(ValueError): adapter._serve(request, 9)
        adapter._native.jump_houdini_apply.assert_not_called()

    def test_bad_directory_and_native_readback_failure(self):
        with self.assertRaises(ValueError): adapter._serve(self.request(directory='Z:\\JumpMissing_987654321'), 9)
        adapter._native.jump_houdini_apply.assert_not_called()
        adapter._native.jump_houdini_apply.return_value = b'ERROR\nOriginal filename readback failed'
        with self.assertRaisesRegex(ValueError, 'readback failed'): adapter._serve(self.request(), 9)

    def test_variable_paths_and_license_extensions(self):
        for extension in ['.hip', '.hiplc', '.hipnc', '.hip.gz']:
            adapter._native.jump_houdini_inspect.return_value = ('OK\nsub/原工程' + extension + '\n$HIP/\n*.hip*').encode()
            name, directory, ext, look_in = adapter._read(widget)
            self.assertEqual(ext, extension)
            self.assertEqual(name, '原工程' + extension)
            self.assertEqual(ntpath.basename(directory), 'sub')

    def test_empty_original_filename_stays_attached(self):
        adapter._native.jump_houdini_inspect.return_value = b'OK\n\n$HIP/\n*.hip*'
        name, directory, extension, _ = adapter._read(widget)
        self.assertEqual(name, '')
        self.assertEqual(directory, ntpath.normpath(str(OUT)))
        self.assertEqual(extension, '.hip')

    def test_closed_chooser_keeps_lightweight_reopen_detection(self):
        adapter._timer, adapter._server = Mock(), Mock()
        adapter._last = (42, 'previous')
        with patch.object(adapter, '_candidates', return_value=[]):
            adapter._tick()
        adapter._timer.setInterval.assert_called_once_with(250)
        adapter._timer.stop.assert_not_called()
        adapter._server.close.assert_called_once()
        self.assertIsNone(adapter._last)

    def test_external_chooser_and_teardown_ignored(self):
        adapter._external = 1
        self.assertEqual(adapter._candidates(), [])
        with self.assertRaises(ValueError): adapter._serve(self.request(), 9)
        adapter._filter = None
        with patch.object(adapter, 'QtCore', None):
            self.assertFalse(adapter.NativeSaveFilter().eventFilter(None, None))


if __name__ == '__main__':
    unittest.main()
