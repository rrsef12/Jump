"""Fast hidden-window layout, flyout and preset checks; never take foreground focus."""
import json
import subprocess
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def run():
    out = ROOT/'tests/output/layout'/uuid.uuid4().hex[:10]
    out.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(ROOT/'build/Jump.exe'), '--self-test', str(out)],
                            creationflags=subprocess.CREATE_NO_WINDOW, timeout=20)
    failure = (out/'failure.txt').read_text(encoding='utf-8') if (out/'failure.txt').exists() else ''
    if (out/'workflow-failure.txt').exists():failure+=(out/'workflow-failure.txt').read_text()
    assert result.returncode == 0, f'Hidden UI checks failed: {result.returncode} {failure}'
    previews=['reference-layout.bmp','manager-schemes.bmp','manager-locations.bmp',
              'manager-workflows.bmp','manager-options.bmp','manager-prefixes.bmp','manager-expressions.bmp',
              'manager-projects.bmp','manager-backups.bmp','manager-connections.bmp']
    previews += [f'structured-{dpi}-{side}{mode}.bmp' for dpi in [96,120,144]
                 for side in ['side','bottom'] for mode in ['', '-input','-edit']]
    previews += [f'recommended-{dpi}-{side}.bmp' for dpi in [96,120,144] for side in ['side','bottom']]
    previews += [f'recommended-four-{dpi}-{side}.bmp' for dpi in [96,120,144] for side in ['side','bottom']]
    previews += [f'locations-empty-{dpi}-{side}.bmp' for dpi in [96,120,144] for side in ['side','bottom']]
    assert all((out/name).is_file() for name in previews)
    report={'passed':['model roundtrip, legacy group migration and backup; no repeated migration',
                       'current naming panel: quick choices, field editor, 100/125/150 percent DPI and both placements',
                       'all nine settings sections fit; old group and stage/date controls absent',
                       'manager placement recovery without foreground activation',
                       'paint stability, input selection and topmost restoration',
                       'four location tabs; all opened folders merged/deduplicated in Recommended, source badge refresh, scrolling at three DPI levels and both placements',
                       'compact panel with balanced two-by-two layout for four folders at all three DPI levels and both placements',
                       'segmented location bar and empty state align with the content at all three DPI levels and both placements'],
            'previews':previews,'artifacts':str(out)}
    (out/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    run()
