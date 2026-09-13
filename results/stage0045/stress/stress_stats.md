# Stage 004.5 Stress Statistics

> **Functional PC Golden workload statistics — NOT FPGA performance evidence**

Source: `gpu2d_demo --headless --backend tile` on Agent-local Windows.

| Scene | Frames | Seed | Cmds | Sprites | WorkRefs | Tiles Active | Max OD |
|---|---:|---:|---:|---:|---:|---:|---:|
| Sprite Storm | 90 | 9 | 557 | 520 | 990 | 240 | 8 |
| Alpha Storm | 150 | 9 | 230 | 183 | 1696 | 920 | 4 |
| Bullet Hell | 90 | 9 | 415 | 378 | 968 | 240 | 6 |
| Scale Storm | 90 | 9 | 256 | 219 | 706 | 240 | 5 |
| Overdraw Storm | 120 | 9 | 285 | 238 | 1940 | 920 | 44 |

Capture SHA256 (RGB565 1280×720):

```text
73cde40f0e9557e90f9926aeb49481a06d8c5c9f6cc91e3c0e682649496001c0  v1_game_showcase.raw
9cdcd3c05f6b91433caca511687371512eece8597b217906c31357d6782d6480  v2_effects.raw
0a2db70763aa200d253b4c549b766e86e7a6d69449f8c7b8483dcc776fa54c5a  v3_xray.raw
d2c00c2a2f8feb2eabdf0962621d91566c493409df888e0c505b228201efb0b7  v4_overdraw_stress.raw
```

PC Golden Host FPS is host timing only.
