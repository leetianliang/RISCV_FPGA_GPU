"""Explicitly generate R2 review evidence from production rendering, no compositing."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def raw_png(path, width=640, height=360):
    data=path.read_bytes()
    assert len(data)==width*height*2
    rgb=bytearray()
    for offset in range(0,len(data),2):
        pixel=data[offset] | data[offset+1]<<8
        r,g,b=(pixel>>11)&31,(pixel>>5)&63,pixel&31
        rgb.extend(((r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)))
    im=Image.frombytes('RGB',(width,height),bytes(rgb))
    im.save(path.with_suffix('.png'))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--update',action='store_true',required=True)
    args=ap.parse_args()
    out=ROOT/'results/facility_omega/rework_r2'
    out.mkdir(parents=True,exist_ok=True)
    demo=ROOT/'build/stage0045/model/pc_demo/gpu2d_demo.exe'
    records=[]
    for name,frames,route in [('quiet_001',1,False),('normal_180',180,False),
                              ('normal_600',600,False),('scroll_600',600,True)]:
        raw=out/(name+'.raw')
        cmd=[str(demo),'--app','facility','--headless','--frames',str(frames),
             '--seed','1234','--backend','immediate','--profile','interactive','--capture',str(raw)]
        if route: cmd.append('--facility-route')
        result=subprocess.run(cmd,cwd=ROOT,check=True,capture_output=True,text=True)
        print(name, result.stdout.strip(),flush=True)
        raw_png(raw)
        records.append(dict(name=name,kind='normal gameplay with scripted input' if route else 'normal gameplay, idle input',
                            command=cmd,stdout=result.stdout,width=640,height=360,
                            frame=frames,seed=1234,sha256=hashlib.sha256(raw.read_bytes()).hexdigest()))
    test=ROOT/'build/stage0045/model/pc_demo/gpu2d_test_facility_visual.exe'
    cmd=[str(test),str(out/'combat_fixture')]
    result=subprocess.run(cmd,cwd=ROOT,check=True,capture_output=True,text=True)
    print(result.stdout,flush=True)
    raw_png(out/'combat_fixture.raw')
    records.append(dict(name='combat_fixture',kind='synthetic regression fixture; four prescribed enemies, actual simulation and rendering',
                        command=cmd,stdout=result.stdout,width=640,height=360,seed=1234,
                        sha256=hashlib.sha256((out/'combat_fixture.raw').read_bytes()).hexdigest()))
    (out/'captures.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    main()
