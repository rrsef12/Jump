"""Run checks serially; foreground checks are an explicit, separate mode."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
HIDDEN=['layout_smoke.py']
FOREGROUND=['settings_center_integration.py','unified_scheme_integration.py','scheme_tasks_layout_integration.py','scheme_usage_scenarios_integration.py','default_schemes_integration.py','naming_designer_integration.py','scheme_system_integration.py','extended_integration.py','rename_location_integration.py',
            'affix_manager_integration.py','recent_saves_integration.py',
            'workflow_integration.py','quick_picks_integration.py','candidate_rename_integration.py','fill_paint_integration.py','material_integration.py','convenience_integration.py','generic_integration.py','universal_save_integration.py','universal_protocol_checks.py','manager_startup_checks.py','manager_click_smoke.py','startup_integration.py']

def run():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--foreground',action='store_true',help='Use only owned native fixtures, serially; stop the development app first.')
    parser.add_argument('--only',nargs='+',help='Specific check filenames from the selected mode')
    args=parser.parse_args();available=FOREGROUND if args.foreground else HIDDEN
    selected=args.only or available
    if any(name not in available for name in selected):parser.error('Check does not belong to this mode')
    out=ROOT/'tests/output/check-runs';out.mkdir(parents=True,exist_ok=True)
    report={}
    for name in selected:
        print('Running '+name,flush=True)
        try:
            result=subprocess.run([sys.executable,'-X','utf8',str(ROOT/'tests'/name)],cwd=ROOT,
                                  capture_output=True,text=True,encoding='utf-8',timeout=180)
            (out/(name+'.log')).write_text(result.stdout+result.stderr,encoding='utf-8')
            report[name]={'passed':result.returncode==0,'exitCode':result.returncode}
        except subprocess.TimeoutExpired:
            report[name]={'passed':False,'error':'timed out; inspect owned fixture processes before continuing'}
            break
        print('PASS' if report[name]['passed'] else 'FAIL: '+str(out/(name+'.log')),flush=True)
        # Do not launch another foreground fixture after a failure with unknown cleanup.
        if not report[name]['passed']:break
    (out/('foreground.json' if args.foreground else 'hidden.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
    return 0 if all(item['passed'] for item in report.values()) else 1

if __name__=='__main__':raise SystemExit(run())
