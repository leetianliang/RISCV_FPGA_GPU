"""Read-only production output verification and isolated mutation regressions."""
import json
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import facility_omega_assetc as assetc


def main():
    outputs = assetc.build_outputs()
    before = {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in outputs}
    assert not assetc.check_or_write(outputs)
    assert all((p.read_bytes(), p.stat().st_mtime_ns) == old for p, old in before.items())
    assert outputs == assetc.build_outputs(), "non-deterministic asset generation"
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        existing, missing = root / "existing.raw", root / "missing.raw"
        existing.write_bytes(b"old")
        old_time = existing.stat().st_mtime_ns
        expected = {existing: b"new", missing: b"missing"}
        assert len(assetc.check_or_write(expected)) == 2
        assert existing.read_bytes() == b"old" and existing.stat().st_mtime_ns == old_time
        assert not missing.exists()
        assert not assetc.check_or_write(expected, update=True)
        assert not assetc.check_or_write(expected)
    # Geometry checks do not replace human review of the source object semantics.
    from PIL import Image
    for name in assetc.PLAYER_BOXES:
        im = Image.open(assetc.PREV / "png" / ("engineer_" + name + ".png"))
        bbox = im.getchannel("A").getbbox()
        assert bbox and bbox[3] == 28 and bbox[1] > 0, (name, bbox)
    manifest = json.loads(outputs[assetc.OUT / "facility_omega_assets.json"])
    assert len(manifest["sprites"]) == len(assetc.CROPS)
    assert len(manifest['atlases']) == 2, 'runtime texture-slot regression'
    for sprite in manifest['sprites']:
        bpp = 2 if sprite['format'] == 'rgb565' else 4
        atlas = outputs[assetc.OUT / sprite['atlas_file']]
        extracted = bytearray()
        for y in range(sprite['height']):
            start=(sprite['atlas_y']+y)*sprite['atlas_stride']+sprite['atlas_x']*bpp
            extracted.extend(atlas[start:start+sprite['width']*bpp])
        assert bytes(extracted) == outputs[assetc.OUT / sprite['runtime_file']], sprite['name']
    print("APP2 source integrity / reproducibility / verify no-write / mutation / feet: PASS")


if __name__ == "__main__":
    main()
