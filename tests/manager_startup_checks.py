"""Regression for the real hidden-startup flags used by dev.ps1 and the startup shortcut.

The application checks its own management window before any other top-level
ShowWindow call. No synthetic mouse/window messages or user's application are used.
"""
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run():
    results = []
    for hidden, entry in [(True, 'locations'), (True, 'names'), (True, 'prefixes'),
                          (True, 'suffixes'), (False, 'locations')]:
        out = ROOT / 'tests/output/manager-startup' / (('hidden-' if hidden else 'normal-') + entry)
        out.mkdir(parents=True, exist_ok=True)
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE if hidden else 1  # SW_SHOWNORMAL
        result = subprocess.run([str(ROOT / 'build/Jump.exe'), '--manager-show-check', str(out),
            '--manager-entry', entry], startupinfo=startup, timeout=10)
        report = json.loads((out / 'result.json').read_text(encoding='utf-8'))
        results.append({'hiddenStartup': hidden, 'entry': entry, **report})
        assert result.returncode == 0 and all(report.values()), results[-1]
    (ROOT / 'tests/output/manager-startup/report.json').write_text(
        json.dumps(results, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(results, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    run()
