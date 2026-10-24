"""Validate the deliverable ZIP, first launch and packaged optional installers."""
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid
import zipfile
import native_integration as n


def run():
    out = n.ROOT / 'tests/output/portable-release' / uuid.uuid4().hex[:10]
    extraction = out / '中文 路径'
    extraction.mkdir(parents=True)
    archive = n.ROOT / 'dist/Jump-0.1.0-win-x64.zip'
    passed = []
    with zipfile.ZipFile(archive) as zipped:
        assert zipped.testzip() is None
        names = zipped.namelist()
        assert all(name.startswith('Jump/') and '..' not in Path(name).parts for name in names)
        assert not any(name.lower().endswith(('.ini', '.lnk', '.pdb', '.obj', '.pyc')) or
                       any(part.lower() in ('data', 'build', 'tests', '__pycache__', 'reports') for part in Path(name).parts) or
                       'testhost' in name.lower() for name in names)
        zipped.extractall(extraction)
    package = extraction / 'Jump'
    executable = package / 'Jump.exe'
    manifest = json.loads((package / 'release-manifest.json').read_text(encoding='utf-8-sig'))
    assert manifest['configuration'] == 'Release' and manifest['version'] == '0.1.0'
    expected = {row['path']: row['sha256'] for row in manifest['files']}
    actual = {file.relative_to(package).as_posix(): hashlib.sha256(file.read_bytes()).hexdigest()
              for file in package.rglob('*') if file.is_file() and file.name != 'release-manifest.json'}
    assert actual == expected
    assert hashlib.sha256(executable.read_bytes()).digest() == hashlib.sha256((n.ROOT / 'build/Jump.exe').read_bytes()).digest()
    assert (package / '开始使用.md').is_file()
    passed.append('clean ZIP extracts under a Chinese path with spaces; all manifest hashes match the verified Release binary and no runtime settings or test artifacts are included')
    app = None
    try:
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        app = subprocess.Popen([str(executable), '--test-host-only'], startupinfo=startup,
                               creationflags=subprocess.CREATE_NO_WINDOW)
        controller = n.wait(lambda: next(iter(n.windows(app.pid, 'Jump.Control')), None))
        cfg = package / 'data/settings.ini'
        n.wait(cfg.is_file)
        assert not n.windows(app.pid, 'Jump.Manager', True) and not n.windows(app.pid, 'Jump.Panel', True)
        second = subprocess.run([str(executable), '--test-host-only'], timeout=10,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        assert second.returncode == 0 and app.poll() is None and n.u.IsWindow(controller)
        passed.append('fresh portable launch creates its own data configuration, starts quietly with login-style hidden flags and rejects a duplicate instance')
    finally:
        if app and app.poll() is None:
            for controller in n.windows(app.pid, 'Jump.Control'):
                n.u.PostMessageW(controller, 0x10, 0, 0)
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.terminate()
                app.wait(timeout=5)
    # Windows PowerShell must resolve its own modules, not inherit PowerShell 7's module path.
    env = {key: value for key, value in os.environ.items() if key.lower() != 'psmodulepath'}
    env['APPDATA'] = str((out / 'appdata').resolve())
    subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    str(package / 'integrations/blender/install.ps1'), '-Version', '99.0'], env=env, check=True)
    blender = out / 'appdata/Blender Foundation/Blender/99.0/scripts/startup/jump_save_bridge.json'
    assert json.loads(blender.read_text(encoding='utf-8-sig'))['apps'] == [str(executable.resolve())]
    prefs = out / 'houdini-prefs'
    subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    str(package / 'integrations/houdini/install.ps1'), '-PreferencesDirectory', str(prefs.resolve())], env=env, check=True)
    houdini = prefs / 'jump_scene_save/python3.13libs'
    config = json.loads((houdini / 'jump_scene_save.json').read_text(encoding='utf-8-sig'))
    assert config['apps'] == [str(executable.resolve())] and (houdini / config['native_library']).is_file()
    subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    str(package / 'integrations/houdini/uninstall.ps1'), '-PreferencesDirectory', str(prefs.resolve())], env=env, check=True)
    assert json.loads((prefs / 'packages/jump_scene_save.json').read_text(encoding='utf-8-sig'))['enable'] is False
    passed.append('packaged Blender and Houdini installers resolve the portable executable and bundled adapter without a development tree; install/uninstall use isolated preferences')
    # Reuse this process's foreground-fixture lock instead of spawning a competing fixture.
    import startup_integration
    startup_integration.run(['--app', str(executable)])
    passed.append('the extracted executable passes real startup enable/disable, path repair, locked-link recovery and restart checks; the original user shortcut is restored')
    report = {'passed': passed, 'archive': str(archive), 'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
              'app_sha256': actual['Jump.exe'], 'output': str(out)}
    (out / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    run()
