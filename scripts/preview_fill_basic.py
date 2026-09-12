from pathlib import Path

from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
out_dir = root / "results" / "stage001_preview"
out_dir.mkdir(parents=True, exist_ok=True)


def load_raw(path: Path, w: int, h: int, stride: int) -> Image.Image:
    raw = path.read_bytes()
    img = Image.new("RGBA", (w, h), (0, 0, 0, 255))
    pixels = []
    for y in range(h):
        row = raw[y * stride : (y + 1) * stride]
        for x in range(w):
            i = x * 2
            v = row[i] | (row[i + 1] << 8)
            r5 = (v >> 11) & 0x1F
            g6 = (v >> 5) & 0x3F
            b5 = v & 0x1F
            r = (r5 << 3) | (r5 >> 2)
            g = (g6 << 2) | (g6 >> 4)
            b = (b5 << 3) | (b5 >> 2)
            pixels.append((r, g, b, 255))
    img.putdata(pixels)
    return img


def main() -> None:
    w, h, stride, scale = 16, 16, 32, 24
    fx = root / "model" / "golden" / "tests" / "frames" / "fill_basic"
    init = load_raw(fx / "initial_fb.raw", w, h, stride)
    golden = load_raw(fx / "golden_fb.raw", w, h, stride)

    init_big = init.resize((w * scale, h * scale), Image.NEAREST)
    gold_big = golden.resize((w * scale, h * scale), Image.NEAREST)

    draw = ImageDraw.Draw(gold_big)
    x0, y0 = 3 * scale, 4 * scale
    x1, y1 = (3 + 7) * scale, (4 + 6) * scale
    draw.rectangle(
        [x0 - 1, y0 - 1, x1, y1],
        outline=(255, 255, 0, 255),
        width=2,
    )

    pad = 24
    label_h = 36
    panel_w = w * scale
    panel_h = h * scale
    canvas = Image.new(
        "RGB",
        (pad * 3 + panel_w * 2, pad * 2 + label_h + panel_h),
        (24, 26, 30),
    )
    d = ImageDraw.Draw(canvas)
    d.text(
        (pad, 8),
        "Stage 001 Golden FILL_RECT — initial_fb vs golden_fb "
        "(16x16 RGB565, zoom x24)",
        fill=(230, 230, 230),
    )
    d.text((pad, pad + label_h - 8), "initial", fill=(180, 180, 180))
    d.text(
        (pad * 2 + panel_w, pad + label_h - 8),
        "golden (FILL 3,4 7x6, color 0xFF2A55C8)",
        fill=(180, 180, 180),
    )
    canvas.paste(init_big, (pad, pad + label_h))
    canvas.paste(gold_big, (pad * 2 + panel_w, pad + label_h))

    out1 = out_dir / "fill_basic_preview.png"
    canvas.save(out1)
    out2 = out_dir / "golden_fb_16x16.png"
    golden.save(out2)
    print(out1)
    print(out2)


if __name__ == "__main__":
    main()
