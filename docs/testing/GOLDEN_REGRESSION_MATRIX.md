# Golden Regression Matrix

Permanent behavioral coverage. Do **not** delete categories during refactors without an equivalent replacement.

| Category | Directed oracle | Random / differential | Fixture |
|---|---|---|---|
| Arithmetic | `golden_test_gpu_math` | — | — |
| Command Decode / IO | `golden_test_command_header`, `golden_test_cmd_io` | — | — |
| Memory | `golden_test_memory_image` | — | — |
| Formats / XRGB | `golden_test_surface`, `golden_test_mutation`, `golden_test_dither_dst` | — | `argb8888_target` |
| FILL | `golden_test_fill_directed` | `golden_test_random_diff` | `fill_basic` |
| BLIT COPY/Key/Alpha/Add | `golden_test_blit_directed` | `golden_test_core_alpha_diff`, `golden_test_random_diff` | stage002 blit_* |
| Premult | `golden_test_premult_exact`, `golden_test_premult_matrix` | — | `premult_alpha` |
| Straight vs Premult | `golden_test_oracle_v2` | — | — |
| Color Mod | `golden_test_premult_matrix` | — | `color_mod` |
| BLIT_EXT axis | `golden_test_ext_faults`, `golden_test_sprite_ext` | — | `blit_ext_scale_*` |
| Nearest scale | `golden_test_oracle_exact` | `golden_test_scale_param_diff`, `golden_test_diff_suites` | `blit_ext_scale_nearest` |
| Bilinear | `golden_test_oracle_v2` (fractions + 2D order), `golden_test_oracle_exact` | — | `blit_ext_scale_bilinear`, `indexed8_bilinear` |
| INDEX8 Bilinear | `golden_test_oracle_v2` | — | `indexed8_bilinear` |
| Clamp / Repeat | `golden_test_oracle_exact`, `golden_test_oracle_v2` (periods) | — | — |
| Clip | `golden_test_semantics_pairs` | `golden_test_clip_diff` | `blit_clip` |
| Flip | `golden_test_sprite_ext`, `golden_test_semantics_pairs` | — | `blit_flip_xy` |
| Palette / INDEX8 | `golden_test_oracle_exact`, `golden_test_oracle_v2` | — | `indexed8_*` |
| Dither | `golden_test_oracle_exact`, `golden_test_dither_dst` (xy/off/non-RGB) | — | `rgb565_dither` |
| Sampler faults | `golden_test_sampler_errors`, `golden_test_ext_mem_matrix` | — | — |
| Ext Strict / header | `golden_test_ext_faults`, `golden_test_semantics_pairs`, `golden_test_ext_mem_matrix` | — | — |
| Mutation / ISA authority | `golden_test_mutation` | — | — |
| Test integrity | `scripts/check_test_integrity.py` | — | — |
| Fixture validation | `fixture_validate.py` | — | all `tests/frames/*` |
