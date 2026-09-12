# Golden Regression Matrix

Permanent behavioral coverage. Do **not** delete categories during refactors without an equivalent replacement.

| Category | Primary tests |
|---|---|
| Arithmetic | `golden_test_gpu_math` (exhaustive RGB565/DIV255/MUL8, Q16/LERP/round_div) |
| Command Decode | `golden_test_command_header`, `golden_test_cmd_io` |
| Memory | `golden_test_memory_image` (LE, UNMAPPED/OOR, region end) |
| Formats | `golden_test_surface`, `golden_test_pixel_pipeline`, XRGB write in mutation tests |
| FILL | `golden_test_fill_directed` (FILL-001..010 + alpha boundaries) |
| BLIT | `golden_test_blit_directed` |
| BLIT_EXT | `golden_test_sprite_ext` (scale, eq, flip, clip) |
| Key | BLIT directed color key |
| Alpha | FILL/BLIT alpha + premult fixture |
| Additive | BLIT directed + `blit_additive` fixture |
| Premult | `premult_alpha` fixture |
| Scaling | `blit_ext_scale_nearest/bilinear` fixtures + sprite_ext |
| Nearest | sprite_ext + fixtures |
| Bilinear | sprite_ext + bilinear fixture |
| Clip | sprite_ext + `blit_clip` fixture |
| Flip | sprite_ext + `blit_flip_xy` fixture |
| Palette | `indexed8_palette`, `indexed8_bilinear` fixtures |
| Dither | `rgb565_dither` fixture |
| Mutation / ISA authority | `golden_test_mutation` |
| Fixtures | all `tests/frames/*` + `fixture_validate.py` |
| Random Differential | `golden_test_random_diff` (independent oracle, fixed seeds) |

Stage-001/002 regressions are mapped in REPORT_003 §14.
