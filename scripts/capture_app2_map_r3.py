"""Capture R3 renderer and gameplay; overview joins four unmodified framebuffer crops."""
import hashlib
import argparse
import json
import subprocess
from pathlib import Path
from PIL import Image
from capture_app2_rework import raw_png

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', default='results/facility_omega/map_r3')
parser.add_argument('--showcase', action='store_true', help='Also capture the explicitly staged R3V2 snapshot')
args = parser.parse_args()
out = ROOT / args.output
out.mkdir(parents=True, exist_ok=True)
commands = [[str(ROOT / 'build/stage0045/model/pc_demo/gpu2d_test_facility_map.exe'), str(out)],
            [str(ROOT / 'build/stage0045/model/pc_demo/gpu2d_demo.exe'), '--app', 'facility',
             '--headless', '--frames', '180', '--seed', '1234', '--backend', 'immediate',
             '--profile', 'interactive', '--capture', str(out / 'gameplay_180.raw')]]
if args.showcase:
    for backend in ('immediate', 'tile'):
        commands.append([str(ROOT / 'build/stage0045/model/pc_demo/gpu2d_demo.exe'),
                         '--app', 'facility', '--headless', '--frames', '1', '--seed', '1234',
                         '--backend', backend, '--profile', 'interactive', '--facility-showcase',
                         '--capture', str(out / f'showcase_{backend}.raw')])
records = []
for command in commands:
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    print(result.stdout, flush=True)
    if result.returncode:
        print(result.stderr, flush=True)
        result.check_returncode()
    records.append(dict(command=command, stdout=result.stdout, stderr=result.stderr))
if args.showcase:
    if (out / 'showcase_immediate.raw').read_bytes() != (out / 'showcase_tile.raw').read_bytes():
        raise RuntimeError('Staged production capture Immediate/Tile mismatch')
    print('Staged production capture with HUD: Immediate == Tile PASS', flush=True)
for raw in out.glob('*.raw'):
    if 'overview' in raw.stem:
        continue  # Ignore obsolete failed captures from a larger framebuffer.
    size = (512, 512) if '_part_' in raw.stem else (640, 360)
    raw_png(raw, *size)
for name in ('hero', 'collision'):
    overview = Image.new('RGB', (1024, 1024))
    for part in range(4):
        overview.paste(Image.open(out / f'{name}_part_{part}.png'), ((part % 2)*512, (part // 2)*512))
    overview.save(out / f'{name}_overview.png')
frames = [Image.open(out / f'scroll_{i}.png').convert('RGB') for i in range(48)]
frames[0].save(out / 'scroll.gif', save_all=True, append_images=frames[1:], duration=80, loop=0)
data = dict(commands=records, note='Map views hide entities; gameplay_180 uses normal idle input. Showcase is an authored snapshot, not simulated gameplay: 20 enemies, 9 bullets, 7 XP, alpha glow and additive effects. Overview joins four unmodified 512px framebuffer crops at identical zoom.',
            sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*.raw'))})
(out / 'captures.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
