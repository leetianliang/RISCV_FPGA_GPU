# APP2 R3V2 — 视觉构图完善

日期：2026-09-16；基线：`3e97372`。依据：`REVIEW_APP2_MAP_R3_M13_VISUAL_V1.md`、`TASK_APP2_R3V2_Visual_Composition.md`。

## 状态与实现

**M13：HOLD / PENDING OWNER REVIEW。** 本轮只完善视觉并提供受控展示，不更改碰撞、战斗或进度系统。自动回归不能授予视觉批准。

- 地面保留原有基础材质并增加两个同源镜像变体，共三种。初稿明暗差异在 RGB565 量化后形成明显色块，截图检查后统一低对比色调，通过离线镜像改变细节方向。源图和 RGB565 算术未修改。
- 删除贯穿全房间的规则长接缝，改为错位、终止于局部设施的短段与 T 形关系。
- 新增非碰撞检修盖板、排水沟、线缆槽、方向箭头、检修数字、磨损簇和装卸地面框。它们由既有 Fill/Sprite API 绘制，不从装饰像素推导碰撞。
- 设备台座改为低对比底板、局部角标；连接至墙面或地面管线。墙体增加内侧沟槽、支撑扣件、转角/入口端部装饰及指示灯，保持原 AABB。
- 维修区用双管线、黄色检修标识；仓储区用暖色装卸框和磨损；动力区用青色线路、技术盖板与局部光；中央大厅保留开阔地面，以测试标识、稀疏检修口与排水沟形成尺度。
- 新增仅供 headless 捕获的 `--facility-showcase`。受控快照含 20 个敌人（2 个 Tank）、9 发子弹、7 个 XP、透明光晕、爆炸和火花。画面标注 STAGED SHOWCASE，不是自然游玩或 180 帧模拟结果；普通 `gameplay_180` 仍按原始空闲输入运行。

## 验证与证据

最终单进程构建：**PASS**。完整 CTest：**92/92 PASS，0 失败，260.40 秒**。日志：`results/facility_omega/map_r3v2/ctest_final.log`。覆盖资源确定性、应用边界、地图碰撞/一致性、战斗、稳定性以及既有 GPU 基准和压力测试。

地图回归增加独立冻结坐标断言，锁定 R3 的全部 12 个碰撞矩形，原有效测试保持。受控快照检查敌人出生位置合法、数量符合审查范围、实际命令包含 StraightAlpha 与 AddSat；调用生产 render_scene，并逐像素比较两后端。

最终捕获已通过三屏和 48 帧滚动一致性、带 HUD 的真实 CLI 展示图 Immediate/Tile 一致性。玩家通行采样为 3530/4096（86.2%），Tank 为 3216/4096（78.5%），全部连通，视口最多四组碰撞体，与 R3 相同。比率是 16 像素间距的采样估计。

本地逐一查看三屏、普通实战、受控展示与总览，修正了首稿材质色块。正式展示快照：394 条基础命令、164 sprites、1535 Tile work references、最大局部 overdraw 11；普通 180 帧截图：256 条命令、85 sprites。仅记录该帧的命令统计，不作实时帧率或上板性能承诺。

捕获清单 63 个 raw 文件 SHA256 全部核对一致。CLI 范围检查：缺少 headless、缺少 facility app、与 route 同用三种错误调用均返回 2，并给出明确限制说明。

输出目录 `results/facility_omega/map_r3v2/`，保留 R3 原证据目录：

- `hero_overview.png` / `collision_overview.png`：四个真实 512×512 帧缓冲分区原尺寸拼接；后者显示碰撞轮廓。
- `map_view_0.png`、`map_view_1.png`、`map_view_2.png`：640×360 邻接地图视图，隐藏角色。
- `scroll.gif`：48 帧地图摄像机滚动；不是玩家操作录像。
- `gameplay_180.png`：生产应用，seed 1234，180 帧空闲输入。
- `showcase_immediate.png` / `showcase_tile.png`：带完整 HUD 的受控展示，两后端原始 RGB565 数据应一致。
- `showcase_fixture.png`：地图测试中的同一快照，不含 PC presenter 的文字 HUD。
- `captures.json`：准确命令、标准输出与 raw SHA256。原始 raw 和逐帧 PNG 可供复查。

## 命令与结果

仓库根目录执行：

```powershell
python -B tools/facility_omega_assetc.py --update
cmake --build build/stage0045 -j 1
python -B scripts/capture_app2_map_r3.py --output results/facility_omega/map_r3v2 --showcase
ctest --test-dir build/stage0045 --output-on-failure -j 1 --output-log results/facility_omega/map_r3v2/ctest_final.log
git diff --check
```

CLI 范围检查命令：

```powershell
python -B -c "import subprocess; e='build/stage0045/model/pc_demo/gpu2d_demo.exe'; cases=[['--facility-showcase'],['--headless','--facility-showcase'],['--headless','--app','facility','--facility-showcase','--facility-route']]; rows=[subprocess.run([e,*a],capture_output=True) for a in cases]; assert all(r.returncode!=0 and b'requires --headless --app facility and no route' in r.stderr for r in rows); print('CLI scope guards PASS:',[r.returncode for r in rows])"
```

资源编译：71 sprites、150 个产物，源完整性 PASS。初次沙箱内地图捕获遇到 std::bad_alloc，沙箱外串行捕获成功。最终微调构建也曾遇到 cc1plus 内存不足，串行重试通过；保留失败事实，不降低编译优化或放宽测试预期。git diff --check 通过，仅提示已有 CRLF/LF 规范化。

## 修改文件

- `software/applications/facility_omega/src/environment.cpp`：仅视觉绘制。
- `tools/facility_omega_assetc.py`、`tools/facility_omega_crops.json` 与生成的 RGB565 atlas、manifest、audit、contact sheet；离线镜像采样，新增 cool/worn 材质 PNG 和 RGB565（名称沿用初稿）。
- `model/pc_demo/app/facility_showcase.hpp`：隔离的捕获快照；`app/main.cpp`：显式 headless 展示入口及水印文字，普通运行流程保持。
- `model/pc_demo/tests/test_facility_map.cpp`：冻结碰撞、受控快照合法性和 GPU 一致性检查。
- `scripts/capture_app2_map_r3.py`：可指定独立输出目录，附加展示捕获与原始双后端比较。
- 本轮任务文档、报告和 map_r3v2 证据。

## 警告、限制与下一步

无新增依赖，无 GPU/ISA/算术/Tile/RTL 更改。碰撞解算、角色半径、世界尺寸、武器、Boss 和升级逻辑未改。编译/捕获期间出现内存资源不足；无证据证明该异常来自游戏逻辑。

视觉仍是既有资产加原生 GPU 装饰的样板，地面盖板和墙体细节尚非独立精修美术包；静态灯光无真实照明模型。三种地面材质是同源低对比变体。展示图是标注的人工布置，不能作为自然刷怪、掉落频率或性能达标证据。局部绕障和样板区之外的世界限制沿用 R3。

规格冲突/设计问题：无。下一步由用户审阅实际总览、三屏、普通实战与受控展示，决定 M13。通过前不扩展玩法。本轮未提交或推送 Git。
