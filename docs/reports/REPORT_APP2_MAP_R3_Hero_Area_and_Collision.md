# APP2 MAP R3：Hero Area 与碰撞返工

日期：2026-09-15。基线：5091521。依据：CURRENT_V3_MAP_FIRST_GATE 审查和 TASK_APP2_MAP_R3_Hero_Area_and_Collision。

## 结果

**阶段 HOLD：等待 M13 用户视觉审阅。** 本轮实现地图样板与碰撞，冻结新玩法。旧 56 项实现声明不能替代视觉批准。

1024×1024 Hero Area 位于世界 [1536,2560)²：中央大厅、维修区、仓储区、动力实验区。移除周期地面结构和重复设备岛，四组设备按用途集中，边界留宽入口。世界仍为 4096×4096，MapTile 仍为 32×32；地面按 256 像素块绘制、剔除。样板区外只有基础地面。

环境分离区域、地面、结构、贴花、道具、局部光和碰撞层。结构视觉实体与碰撞体取自同一矩形描述；地面标识、接缝、警戒线和光不碰撞。不可变源图经资源编译器提取低对比度材料，保留两张运行时 atlas 和现有 GPU API。

玩家脚底中心采用半宽 8 的方形碰撞体，逐像素分步、先 X 后 Y 解算，实现滑动和防穿透。敌人共用碰撞层，普通/快速敌人半宽 10，Tank 半宽 18；遇阻沿矩形切线绕行，出生点必须合法。子弹不参与地图碰撞。

## 验证

最终完整回归：**PASS，92/92，0 失败，266.46 秒**，日志为 results/facility_omega/map_r3/ctest_final.log。此前三项内存异常测试均在最终串行回归中通过。

验收表结构检查 PASS（56 IDs），明确输出阶段 HOLD、用户视觉批准 PENDING，不授予视觉验收。

新增 gpu2d_test_facility_map 覆盖明确停止位置、滑动、高速防穿透、对角移动、生产 player_move 输入路径、500 帧敌人绕障、100 次出生合法性、贴花可通行性，以及三视图和 48 帧滚动的 Immediate/Tile32 像素一致性。

Hero Area 每 16 像素采样，按真实碰撞体检查：玩家可通行 3530/4096（86.2%），Tank 可通行 3216/4096（78.5%），各自全部可通行点四邻域连通。640×360 视口按 32 像素步进扫查，最多 4 组碰撞体。比例是采样估计，不是连续面积解析值；连通性不代表敌人具有全局寻路能力。

执行命令（仓库根目录）：

```powershell
python -B tools/facility_omega_assetc.py --update
cmake --build build/stage0045 -j 4
cmake --build build/stage0045 -j 1
build/stage0045/model/pc_demo/gpu2d_test_facility_map.exe
python -B scripts/capture_app2_map_r3.py
python -B scripts/check_app2_001_acceptance.py
ctest --test-dir build/stage0045 --output-on-failure -j 1 --output-log results/facility_omega/map_r3/ctest.log
ctest --test-dir build/stage0045 --output-on-failure -j 1 --output-log results/facility_omega/map_r3/ctest_final.log
git diff --check
```

资源编译：69 sprites、146 个产物，源完整性 PASS。单进程构建 PASS。首次并行构建因内存不足失败。地图测试发现角落孤立可站立点，已延长边界墙修复，保留断言。总览最初采用 1024 帧缓冲，与既有后端纹理地址重叠，已改为四个 512×512 分区，不修改 GPU 地址布局。

首轮完整回归 88/92 通过：当时地图测试仍使用旧边界二进制；capture_verify、stress_json、demo_headless_tile 报内存分配异常。原始日志 ctest.log 保留，这些失败没有计作 PASS。

## 视觉证据

目录 results/facility_omega/map_r3/：map_view_0/1/2.png 是正常倍率、隐藏角色的邻接视图；scroll.gif 是 48 帧实际地图滚动、80ms/帧；hero_overview.png 是四个原尺寸 512×512 帧缓冲分区直接拼接，未重绘或缩放；collision_overview.png 的橙线表示碰撞矩形。gameplay_180.png 是正式应用 seed 1234、180 帧空闲输入实渲染。captures.json 记录命令、输出和原始帧 SHA256。

本地已查看地图总览和实战视图，降低地面斑点对比度并移除被台座遮挡的不完整装饰。滚动证据是地图摄像机序列，不是玩家操作录像。视觉品质仍需用户审阅。

## 修改文件

- 新增 environment.hpp、environment.cpp；修改 facility/app.hpp、app.cpp：环境数据、地图渲染、玩家/敌人碰撞和合法出生点。
- 新增 model/pc_demo/tests/test_facility_map.cpp；修改 model/pc_demo/CMakeLists.txt 注册模块与测试。
- 修改 tools/facility_omega_assetc.py、facility_omega_crops.json：材料裁剪、对比度和大图接触表缩略；更新 RGB565 atlas、manifest、audit、contact sheet，新增 floor_material.png/.565。
- 新增 scripts/capture_app2_map_r3.py、R3 任务文档、本报告与捕获产物。
- 修改旧阶段报告、验收 JSON、两个生成器和检查器：整体 HOLD/PENDING，历史 PASS 行不构成视觉批准，检查器仅报告证据结构检查结果。

## 警告、限制与下一步

既有 facility/visual 测试保留两处 signed/unsigned 比较警告。C 盘在验证期间剩余约 250 MB，初次运行发生内存分配异常。未修改已有有效测试预期、GPU、RTL、ISA、像素算术或第三方依赖。

只实现样板区。材质仍有较弱的 256 像素周期，墙体为简单矩形，尚非最终美术品质。敌人采用局部绕障，不保证复杂凹形障碍全局可达。无复杂前后遮挡、墙体透明化、蒸汽粒子；局部光晕静态。旧验收矩阵没有逐项重新人工认证，CTest 通过也不能替代 M13。

功能实现无未解决规格冲突；视觉关口待审。先审阅三屏、总览和滚动，通过 M13 后再讨论后续玩法。本轮未提交 Git、未推送远端；开始时已有 V3 审查文件和旧截图均保留。
