"""Capture APP2_002 through production Graphics API; fixtures are explicitly labelled."""
import hashlib
import json
import subprocess
from pathlib import Path
from capture_app2_rework import raw_png

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'results/facility_omega/app2_002'

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    demo = ROOT / 'build/stage0045/model/pc_demo/gpu2d_demo.exe'
    records = []
    for name, scene in [('V1_early', ''), ('V2_mid', 'mid'), ('V3_fx', 'fx'),
                        ('V4_elite', 'elite'), ('V5_level', 'level'),
                        ('V6_density', 'density'), ('V7_technical', 'technical'),
                        ('showcase_app2_002', 'showcase')]:
        raw = OUT / (name + '.raw')
        frames = 180 if not scene else 2 if scene == 'technical' else 1
        cmd = [str(demo), '--app', 'facility', '--headless', '--frames', str(frames),
               '--seed', '1234', '--backend', 'tile' if scene == 'technical' else 'immediate',
               '--profile', 'interactive', '--capture', str(raw)]
        if scene:
            cmd += ['--facility-capture-scene', scene]
        result = subprocess.run(cmd, cwd=ROOT, check=True, capture_output=True, text=True)
        raw_png(raw)
        record = dict(name=name, kind='authored fixture; boosted HP; not normal progression' if scene else 'normal director, idle input, automatic valid upgrade selection',
                      command=cmd, stdout=result.stdout, seed=1234, rendered_frames=frames,
                      width=640, height=360, sha256=hashlib.sha256(raw.read_bytes()).hexdigest())
        records.append(record)
        print(name, result.stdout.strip(), flush=True)
    (OUT / 'captures.json').write_text(json.dumps(records, indent=2)+'\n', encoding='utf-8')

if __name__ == '__main__':
    main()
