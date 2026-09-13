# Stage 004.5 Stress Statistics

> **Functional PC Golden workload statistics — NOT FPGA performance evidence**

Source: `gpu2d_test_system` frozen thresholds + `gpu2d_demo --headless` (tile backend).

| Scene | Frames | Seed | Sprites | Alpha | Scaled | Bilinear | Live Bullets | Max OD | Threshold |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Sprite Storm | 80 | 9 | 872 | — | — | — | — | 14 | ≥500 sprites PASS |
| Alpha Storm | 80 | 9 | 502 | 432 | — | — | — | 6 | ≥300 alpha PASS |
| Bullet Hell | 80 | 9 | 1083 | — | — | — | 1016 | 29 | ≥1000 bullets PASS |
| Scale Storm | 80 | 9 | 548 | — | 480 | 480 | — | 17 | ≥200 scaled PASS |
| Overdraw Storm | 80 | 9 | 358 | — | 288 | 288 | — | 284 | max_od≥8 PASS |

Immediate==Tile byte equality holds for every stress mode (H-T4).

Capture SHA256 (RGB565 1280×720, headless, no live FPS baked in):

```text
(see gpu2d_test_capture_verify / results/stage0045/captures/*.raw)
```
