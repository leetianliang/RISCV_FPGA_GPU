#!/usr/bin/env python3
"""FACILITY-Omega offline asset compiler.

R1: ARGB8888 emitted as BGRA (Golden little-endian 0xAARRGGBB).
R3-R5: one pose per runtime sprite; contact sheet for human review.
Deterministic; --update rewrites checked binaries.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
import zipfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "facility_omega" / "source" / "source_sheets"
OUT = ROOT / "assets" / "facility_omega" / "runtime"
PREV = ROOT / "assets" / "facility_omega" / "processed"
AUDIT = ROOT / "assets" / "facility_omega" / "processed" / "asset_audit.json"

CROPS = json.loads((Path(__file__).with_name("facility_omega_crops.json")).read_text(encoding="utf-8"))
PLAYER_BOXES = {n.removeprefix("engineer_"): s["box"] for n,s in CROPS.items() if n.startswith("engineer_")}
SHEET_FILES = {k: k + "_source.png" for k in ("player", "enemies", "fx", "environment", "ui")}
SHEET_FILES["weapons"] = "weapons_pickups_source.png"


def pack_rgb565(im: Image.Image) -> bytes:
    px = im.load()
    out = bytearray(im.width * im.height * 2)
    i = 0
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out[i] = v & 0xFF
            out[i + 1] = (v >> 8) & 0xFF
            i += 2
    return bytes(out)


def pack_argb8888_bgra(im: Image.Image) -> bytes:
    # Golden: little-endian 0xAARRGGBB → memory B,G,R,A
    return im.tobytes("raw", "BGRA")


def png_bytes(im):
    stream=io.BytesIO()
    im.save(stream,format='PNG',optimize=False)
    return stream.getvalue()


def process_one(spec, sheets):
    sh=sheets[spec['sheet']]
    x,y,w,h=spec['box']
    if min(x,y)<0 or x+w>sh.width or y+h>sh.height:
        raise ValueError('crop outside source')
    crop=sh.crop((x,y,x+w,y+h)).convert('RGBA')
    if spec.get('mirror_x'):
        crop=crop.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if spec.get('mirror_y'):
        crop=crop.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    tw,th=spec['tw'],spec['th']
    if spec['fmt']=='rgb565':
        crop=crop.resize((tw,th),Image.Resampling.LANCZOS)
        bg=Image.new('RGBA',(tw,th),(12,18,24,255))
        bg.alpha_composite(crop)
        material=ImageEnhance.Brightness(bg.convert('RGB')).enhance(spec.get('tone',1))
        return ImageEnhance.Contrast(material).enhance(spec.get('contrast',1)).convert('RGBA')
    bbox=crop.getchannel('A').getbbox()
    if not bbox:
        raise ValueError('empty crop')
    crop=crop.crop(bbox)
    maxh=spec['ay']-1 if spec.get('feet') else th-2
    factor=min((tw-2)/crop.width,maxh/crop.height)
    nw,nh=max(1,round(crop.width*factor)),max(1,round(crop.height*factor))
    crop=crop.resize((nw,nh),Image.Resampling.LANCZOS)
    dst=Image.new('RGBA',(tw,th))
    dst.alpha_composite(crop,((tw-nw)//2,spec['ay']-nh if spec.get('feet') else (th-nh)//2))
    return dst


def verify_source():
    prefix='FACILITY_OMEGA_First_Asset_Pack_V0.1/'
    with zipfile.ZipFile(ROOT/'docs/tasks/FACILITY_OMEGA_First_Asset_Pack_V0.1.zip') as z:
        manifest=json.loads(z.read(prefix+'MANIFEST.json'))
        for f in manifest['files']:
            data=z.read(prefix+f['path'])
            if hashlib.sha256(data).hexdigest()!=f['sha256'] or len(data)!=f['bytes']:
                raise ValueError('package integrity: '+f['path'])
            if (SRC.parent/f['path']).read_bytes()!=data:
                raise ValueError('source changed: '+f['path'])


def build_outputs():
    verify_source()
    sheets={k:Image.open(SRC/fn).convert('RGBA') for k,fn in SHEET_FILES.items()}
    outputs,items,images={},{},{}
    for name,spec in CROPS.items():
        im=process_one(spec,sheets)
        images[name]=im
        opaque=spec['fmt']=='rgb565'
        raw=pack_rgb565(im) if opaque else pack_argb8888_bgra(im)
        rel=spec['fmt']+'/'+name+('.565' if opaque else '.argb')
        outputs[OUT/rel]=raw
        outputs[PREV/'png'/(name+'.png')]=png_bytes(im)
        items[name]=dict(name=name,format=spec['fmt'],width=im.width,height=im.height,
            stride=im.width*(2 if opaque else 4),anchor_x=spec['ax'],anchor_y=spec['ay'],
            runtime_file=rel,sha256=hashlib.sha256(raw).hexdigest(),role=spec.get('role','sprite'),
            source_sheet=SHEET_FILES[spec['sheet']],source_box=list(spec['box']),
            source_size=list(sheets[spec['sheet']].size),source_has_alpha=True,
            preview_file='processed/png/'+name+'.png')
    # Two shelf-packed atlases keep runtime resource use below the existing
    # backend texture-slot limit. Individual raws remain audit/inspection outputs.
    atlas_records=[]
    for fmt in ('rgb565','argb8888'):
        selected=[n for n in items if items[n]['format']==fmt]
        positions={}
        x=y=row_h=0
        for name in selected:
            im=images[name]
            if x+im.width>512:
                x=0; y+=row_h; row_h=0
            positions[name]=(x,y)
            x+=im.width; row_h=max(row_h,im.height)
        height=y+row_h
        atlas=Image.new('RGBA',(512,height),(0,0,0,255) if fmt=='rgb565' else (0,0,0,0))
        for name in selected:
            atlas.paste(images[name],positions[name])
        rel='atlases/'+fmt+('.565' if fmt=='rgb565' else '.argb')
        outputs[OUT/rel]=pack_rgb565(atlas) if fmt=='rgb565' else pack_argb8888_bgra(atlas)
        atlas_records.append(dict(file=rel,format=fmt,width=512,height=height,
            stride=512*(2 if fmt=='rgb565' else 4),sha256=hashlib.sha256(outputs[OUT/rel]).hexdigest()))
        outputs[PREV/('atlas_'+fmt+'.png')]=png_bytes(atlas)
        for name in selected:
            # Atlas fields precede the longer audit-only source fields.
            original=items[name]
            leading={k:original[k] for k in ('name','format','width','height','stride','anchor_x','anchor_y','runtime_file')}
            leading.update(atlas_file=rel,atlas_width=512,atlas_height=height,
                           atlas_stride=512*(2 if fmt=='rgb565' else 4),
                           atlas_x=positions[name][0],atlas_y=positions[name][1])
            leading.update({k:v for k,v in original.items() if k not in leading})
            items[name]=leading
    cw,ch,cols=240,202,4
    contact=Image.new('RGBA',(cw*cols,ch*((len(items)+cols-1)//cols)),(18,26,36,255))
    draw=ImageDraw.Draw(contact)
    for i,(name,it) in enumerate(items.items()):
        x,y=i%cols*cw,i//cols*ch
        im=images[name]
        draw.rectangle((x+120,y+4,x+236,y+143),fill=(113,124,137))
        factor=min(3,110/im.width,136/im.height)
        enlarged=im.resize((max(1,round(im.width*factor)),max(1,round(im.height*factor))),Image.Resampling.NEAREST)
        native=im.copy()
        native.thumbnail((110,136),Image.Resampling.NEAREST)
        contact.alpha_composite(native,(x+4,y+8))
        contact.alpha_composite(enlarged,(x+122,y+6))
        draw.text((x+5,y+148),name,fill='white')
        draw.text((x+5,y+164),f'{im.width}x{im.height} '+it['format'],fill='#89b8cb')
        draw.text((x+5,y+180),f'pivot {it["anchor_x"]},{it["anchor_y"]} / {it["role"]}',fill='#89b8cb')
    outputs[PREV/'contact_sheet.png']=png_bytes(contact)
    frames=[]
    for tick in range(2):
        frame=Image.new('RGBA',(320,128),(25,36,47))
        d=ImageDraw.Draw(frame)
        for j,direction in enumerate('abcd'):
            frame.alpha_composite(images[f'engineer_{direction}{tick}'].resize((64,64),Image.Resampling.NEAREST),(j*80+8,20))
            d.line((j*80,76,j*80+79,76),fill='#557080')
            d.text((j*80+10,95),['UP','DOWN','LEFT','RIGHT'][j],fill='white')
        frames.append(frame.convert('RGB'))
    buf=io.BytesIO()
    frames[0].save(buf,format='GIF',save_all=True,append_images=frames[1:],duration=200,loop=0)
    outputs[PREV/'player_walk.gif']=buf.getvalue()
    manifest=dict(package='facility_omega_runtime',version='0.3',argb_byte_order='BGRA',
        sprite_count=len(items),sprites=list(items.values()),atlases=atlas_records,contact_sheet='processed/contact_sheet.png')
    outputs[OUT/'facility_omega_assets.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    audit=dict(candidates=list(items.values()),source_labels_are_runtime=False,
        open_candidates=['HP frame','XP frame','time panel','kills panel','level-up panel/card','main menu panel/button'],
        note='Baked UI sample values need separate cleanup; full Block A remains OPEN.')
    outputs[AUDIT]=(json.dumps(audit,indent=2)+'\n').encode()
    return outputs


def check_or_write(outputs, update=False):
    bad=[]
    for path,payload in outputs.items():
        if update:
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(payload)
        elif not path.is_file() or path.read_bytes()!=payload:
            bad.append(str(path))
    return bad


def main():
    ap=argparse.ArgumentParser(description='Default: read-only verify. --update: regenerate checked assets.')
    ap.add_argument('--update',action='store_true')
    args=ap.parse_args()
    try:
        outputs=build_outputs()
        bad=check_or_write(outputs,args.update)
    except (OSError,ValueError,zipfile.BadZipFile) as exc:
        print(str(exc),file=sys.stderr)
        return 1
    if bad:
        print('VERIFY FAILED (no writes):\n'+'\n'.join(bad),file=sys.stderr)
        return 1
    print(f'{"UPDATED" if args.update else "VERIFIED"}: {len(CROPS)} sprites, {len(outputs)} files; source integrity PASS')
    return 0


if __name__=='__main__':
    sys.exit(main())
