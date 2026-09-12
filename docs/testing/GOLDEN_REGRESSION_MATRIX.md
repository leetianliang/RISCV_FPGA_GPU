# Golden Regression Matrix

Permanent behavioral coverage. Do **not** delete categories during refactors without an equivalent replacement.

| Category | Directed oracle | Random / differential | Fixture |
|---|---|---|---|
| Arithmetic | `golden_test_gpu_math` | — | — |
| Command Decode / IO | `golden_test_command_header`, `golden_test_cmd_io` | — | — |
| Memory | `golden_test_memory_image` | — | — |
| Formats / XRGB | `golden_test_surface`, `golden_test_mutation` | — | `argb8888_target` |
| FILL | `golden_test_fill_directed` (001–010) | `golden_test_random_diff` | `fill_basic` |
| BLIT COPY/Key/Alpha/Add | `golden_test_blit_directed` | `golden_test_diff_suites` (core), `golden_test_random_diff` | stage002 blit_* |
| Premult | `golden_test_premult_exact`, `golden_test_premult_matrix` | — | `premult_alpha` |
| Color Mod | `golden_test_premult_matrix` (mod RGB/α) | — | `color_mod` |
| BLIT_EXT axis | `golden_test_ext_faults`, `golden_test_sprite_ext` | — | `blit_ext_scale_*` |
| Nearest scale | `golden_test_oracle_exact` (bilinear fractions) | `golden_test_diff_suites` (scale) | `blit_ext_scale_nearest` |
| Bilinear | `golden_test_oracle_exact` | — | `blit_ext_scale_bilinear`, `indexed8_bilinear` |
| Clamp / Repeat | `golden_test_oracle_exact` | — | — |
| Clip | `golden_test_semantics_pairs` (CLIP_EN pair) | — | `blit_clip` |
| Flip | `golden_test_sprite_ext`, `golden_test_semantics_pairs` (EXT flip illegal) | — | `blit_flip_xy` |
| Palette / INDEX8 | `golden_test_oracle_exact` | — | `indexed8_*` |
| Dither | `golden_test_oracle_exact` (4×4 Bayer) | — | `rgb565_dither` |
| Sampler faults | `golden_test_sampler_errors` | — | — |
| Ext / fault priority | `golden_test_ext_faults`, `golden_test_semantics_pairs` | — | — |
| Mutation / ISA authority | `golden_test_mutation` | — | — |
| Test integrity | `scripts/check_test_integrity.py` | — | — |
| Fixture validation | `fixture_validate.py` | — | all `tests/frames/*` |
