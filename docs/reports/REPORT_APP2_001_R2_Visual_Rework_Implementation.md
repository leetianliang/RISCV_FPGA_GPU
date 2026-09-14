# APP2_001 R2 视觉返工实施报告

日期：2026-09-14。基线：`6c20f68`。

本轮已完成素材重裁、运行时图集、透明绘制、慢速敌人运动、Power Test Area 样板区、基础 HUD 和战斗反馈。没有修改 GPU 冻结语义、世界尺寸、MapTile 尺寸或 source/源图。

## 文件与产物

- `tools/facility_omega_assetc.py`：逐对象裁剪、等比缩放、脚底锚点、BGRA、图集、默认只读验证。
- `tools/facility_omega_crops.json`：独立裁剪配置。
- `software/applications/facility_omega/`：透明混合、图集切片、确定性敌人位移、样板区、FX/HUD。
- `model/pc_demo/app/main.cpp`：FACILITY 图集上传、基础 HUD、`--facility-route`。
- `model/pc_demo/tests/test_facility.cpp`、`test_facility_visual.cpp`：运动、像素、透明和双后端回归。
- `scripts/check_app2_asset_pipeline.py`、`capture_app2_rework.py`：只读/可复现验证与截图。
- `results/facility_omega/rework_r2/`：raw、ppm、png、captures.json、ctest.log。

## 验证

```powershell
cmake --build build/stage0045 -j 4
ctest --test-dir build/stage0045 --output-on-failure --output-log results/facility_omega/rework_r2/ctest.log
python -B scripts/capture_app2_rework.py --update
```

结果：`100% tests passed, 0 tests failed out of 90`。新增定向测试包括 source/runtime 可复现性、默认验证不写文件、ARGB BGRA、透明/半透明像素、Crawler/Tank 斜向累计位移、60 帧 Immediate==Tile32。截图产物：`quiet_001`、`normal_180`、`normal_600`、`scroll_600`、`combat_fixture`。

## 限制与下一步

原 TASK_APP2_001 的 XP/Level-Up 完整玩法、全量 UI 候选、最终 56 项证据矩阵尚未完成；本报告不宣告原任务整体 PASS。当前 UI 使用已有 bitmap-font 加基础面板，source UI 中带示例数值的完整面板仍标记 OPEN。样板区道具仍是视觉地标，不引入碰撞墙体。
