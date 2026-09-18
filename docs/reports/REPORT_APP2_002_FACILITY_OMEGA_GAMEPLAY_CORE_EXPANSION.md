# APP2_002 — FACILITY-Ω 玩法核心扩展

> 历史实现报告，评审实现为 `9c355d4efadcd08160262902bc7793933d863b32`。2026-09-17 的 REVIEW_APP2_002_V1 判定阶段 FAIL — NARROW CLOSURE，确认多级升级/RNG缺陷并要求视觉与报告收尾。以下101/101及58项技术记录是当时测试覆盖内的结果，不能覆盖新发现的缺陷。本轮整改以 `REPORT_APP2_002R_NARROW_CLOSURE.md` 为准，旧截图保留历史证据。

日期：2026-09-17。状态：TECHNICAL PASS / PENDING OWNER VISUAL REVIEW。完整 CTest **101/101 PASS，0 FAIL**。

- START_COMMIT：`57c5eb35d0c64aa7c3940f3d6f11c11f018f8376`。
- MAP R3V2 baseline：同上。环境文件、碰撞几何、地图资产与 GPU Golden 语义未修改。
- END_COMMIT：`9c355d4efadcd08160262902bc7793933d863b32`（本历史报告对应的实现与审查版本）。APP2_002R 的实现版本由新报告独立记录。
- 授权：`TASK_APP2_002_FACILITY_OMEGA_Gameplay_Core_Expansion.md`、`REVIEW_APP2_MAP_R3V2_VISUAL_FINAL.md`。前轮 M13 VISUAL PASS，map-first HOLD 已解除；本轮视觉不能继承前轮 PASS。

## 实现与边界

玩法从 app.cpp 拆至 gameplay.cpp，增加集中配置、确定性 Director、四武器状态、构筑选项与 CPU 空间索引。保持 Graphics API 单一路径；未改 ISA、算术、Golden、RTL、寄存器或地图碰撞规则。

Director 以 60Hz 游戏 tick 计时，不依赖主机墙钟。屏外 32–160px 采样，要求世界内、环境合法，失败跳过；绝不夹回屏幕内。

|阶段|起始 tick / 秒|生成间隔 tick|活动上限|每组|Drone/Crawler/Tank/Runner/Elite 权重|额外 Elite 周期|
|---|---|---|---|---|---|---|
|Early|0 / 0|24|48|1–2|65/35/0/0/0|无|
|Build|7200 / 120|15|160|2–4|30/25/15/30/0|无|
|Swarm|18000 / 300|10|360|3–6|25/25/15/30/5|300 tick|
|Late|28800 / 480|8|700|4–8|20/25/20/28/7|180 tick|

|敌人|HP|速度 px/tick|环境半径|接触伤害|XP|
|---|---|---|---|---|---|
|Drone|2|2|10|2|1|
|Crawler|3|1|10|2|2|
|Tank|10|1|18|6|3|
|Runner|3|4|10|3|2|
|Elite|40|2|18|10|12|

Elite 采用已有专属贴图、1.5 倍缩放及紫色 AddSat 光晕。五类均走原环境移动/绕行函数。维修独立于 XP，以 6% 击杀概率掉落、恢复 20 HP，夹至 max_hp。

|武器|L1–L5 参数与伤害规则|Graphics API 表现|
|---|---|---|
|Pulse|伤害1–5；冷却18–10；1/1/2/2/3弹；速度6–8；寻敌220px；寿命90tick|已有弹丸、命中特效；网格最近目标与命中候选|
|Orbit|伤害1–5；冷却13–9；1/1/2/2/3架；轨道48–72px；接触半径20|整数32项 LUT，128相位；贴图与 AddSat 光晕|
|Nova|伤害3–7；冷却220–140；半径100–180；36tick；触发中心固定、每槽每波一次伤害|真实 Scale+Bilinear+AddSat，global_alpha 随年龄减弱|
|Field|伤害1–5；冷却32–16；半径50–82；平方距离含边界|随身两层低透明度 StraightAlpha；位于角色下方|

升级事件使用 PRNG 洗牌生成三个唯一有效选项，冻结至选择；未拥有武器解锁至 L1，已有武器升一级，L5 排除。四武器均满后用不同属性补足。补足属性为伤害 +1、最大HP +10（同时恢复10）、磁吸 +8；旧测试手动置暂停时保留原三属性选项，但自然升级必走新生成器。HUD 显示四槽图标、等级、升级/解锁文案。

GameplaySpatialGrid 与 GPU RenderTile、MapTile 独立：128px 单元，32×32，固定数组链表；每 tick 重建敌人与拾取物索引。矩形候选按稳定槽号排序；最近目标用单元距离下界剪枝，相同距离选最低槽号。细判用整数平方距离。容器预留并复用死槽，容量 enemy=1024、projectile=2048、pickup=2048、effect=1024。实体热循环不进行贴图名称查找、浮点 sin/cos 或弹丸×全部敌人扫描。

## 验证记录

最终完整日志：`results/facility_omega/app2_002/full_ctest.log`，101 条 Test Passed、0 条 Test Failed，包含完整结束标记。专项日志 `facility_tests.log`：13/13 PASS，99.69秒。旧的 LastTestsFailed.log 是此前非法起点测试的历史残留，不是本次结果。

|验证|精确结果|
|---|---|
|空间 oracle|128 fixtures / 4096 queries，边界、空单元、密集、2048容量、最近目标等距规则 PASS|
|正常600tick|seed1234；level3、kills25、spawned38、collected26、upgrades2、HP84、最高26敌人；hash `8e1ab342`|
|120帧 corpus|Immediate==Tile32；最少30敌人；最大121 sprites / 405 commands / 2238 WorkRefs / OD121；sim `07fe0d9e`，累计像素 `29ee0cb1`|
|60帧 density|每帧至少300敌人、727组合实体；最大1421 sprites / 2368 commands / 9438 WorkRefs / OD61；sim `680726bd`，累计像素 `aa558ff1`|
|容量|1024 enemy / 2048 projectile / 2048 pickup，60tick复用无扩容，hash `a52f4cc6`|
|阶段0/1/2/3|各800tick Director独立验证；活动数48/154/360/588；hash `05e8ca46` / `3ed5135e` / `f5a64121` / `8b32efc6`|
|原有Facility地图/视觉/稳定性|全部 PASS；冻结几何断言、连通性与原60帧像素测试保持有效|
|完整回归|Golden + gpu2d **101/101 PASS，0 FAIL**|

专项与完整回归的120/60帧 sim及累计像素hash一致；两组均逐帧验证独立状态重复模拟hash以及双后端全部像素，而非只比较最后一张图。统计最大值来自各帧，不要求发生在同一帧。正常600tick由真实移动输入追踪XP、自动选择有效解锁选项；没有注入敌人、经验或武器。

命令（仓库根目录）：

```powershell
cmake --build build/stage0045 -j 1
ctest --test-dir build/stage0045 -R '^gpu2d_test_facility' --output-on-failure
ctest --test-dir build/stage0045 --output-on-failure
python scripts/capture_app2_002.py
git diff --check
git diff -- software/applications/facility_omega/src/environment.cpp software/applications/facility_omega/include/facility/environment.hpp
```

沙箱外串行构建用于避免此前沙箱内内存分配失败；不据此推断产品性能。

过程问题：新增敌人测试最初把半径18的 Tank 放在 x=1580，距墙末端1568仅12px，属于非法初始测试位置；改为 x=1590 并新增起点合法断言，未改碰撞逻辑或放宽断言。新增渲染测试最初在逐字节哈希循环反复调用会复制整帧的 framebuffer()，导致异常耗时；用 `gdb -batch -p 1156 -ex "thread apply all bt" -ex detach` 定位后，终止该次测试，改为每帧缓存一次返回指针。该中断运行不计 PASS；未发现像素分歧，也未修改 Golden。

## 视觉证据

目录 `results/facility_omega/app2_002/`，PNG 均由生产程序 RGB565 framebuffer 转换，未合成画面。`captures.json` 保存准确命令、输出、分辨率、seed、帧数与 SHA256。

|证据|内容|性质|
|---|---|---|
|V1_early.png|180帧早期画面|正常 Director、空闲输入，自动选择有效升级；无实体/构筑注入|
|V2_mid.png|110敌人、两武器|人工中期构筑|
|V3_fx.png|Nova/Field/Orbit/Pulse|人工四武器场景|
|V4_elite.png|Elite 混合敌群|人工遭遇场景|
|V5_level.png|三个升级选项|人工触发 Level-Up 面板|
|V6_density.png|300敌人、700弹丸、XP/维修/特效|人工压力场景|
|V7_technical.png|实体、命令、混合、缩放、双线性与 Tile 统计|人工压力场景，Tile32；PREV 指上一帧后端统计|
|showcase_app2_002.png|四武器展示|STAGED SHOWCASE，明确标注 STAGED APP2_002|

人工场景提高 HP，密度场景700个固定位置敌方标记弹丸用于稳定绘制负载；它们不代表正常关卡平衡。连续密度测试使用真实 sim_step 推进，不能用静态截图替代。PC Golden 耗时不等于 FPGA FPS。V3/V4/showcase 目前使用同一混合敌群，以同一画面呈现 FX、Elite 与构筑。

## 64 项验收矩阵

缩写：G=`test_facility_gameplay.cpp` 的命名模式；S=`test_facility_spatial.cpp`；R=`test_facility_app2_visual.cpp`；V=`results/facility_omega/app2_002`。PASS 对应本节已归档的具体测试证据；VIS 全部保留审查者权限。

|ID|实现/行为证据|验证与结果|
|---|---|---|
|PRE-01|本报告 START_COMMIT|git HEAD，已记录 PASS|
|PRE-02|environment.cpp/hpp、地图资产未改|原地图测试 PASS|
|PRE-03|render_scene → Graphics API|boundary + full regression PASS|
|PRE-04|sim_step 固定tick|G determinism PASS|
|DIR-01|kDirectorPhases/phase_at|G director 阶段边界 PASS|
|DIR-02|gameplay.hpp 集中参数|G director 读取配置 PASS|
|DIR-03|同种子/同输入 PRNG|G director 双状态 PASS|
|DIR-04|frame测试入口、headless start-seconds|G director 快速切阶段 PASS|
|DIR-05|spawn_enemy 屏外候选|G director 视口/世界边缘 PASS|
|DIR-06|position_clear|G director 五类型生成 PASS|
|DIR-07|阶段权重|G director Runner/Elite 阶段分布 PASS|
|DIR-08|phase.max_active_enemies|G director 活动上限 PASS|
|ENM-01|Drone 配置/追踪|G enemies PASS|
|ENM-02|Crawler 配置/追踪|G enemies PASS|
|ENM-03|Tank 配置/追踪|G enemies PASS|
|ENM-04|Runner 高速低HP|G enemies + director PASS|
|ENM-05|Elite 高HP/伤害/XP|G enemies + director PASS|
|ENM-06|Elite 缩放与 AddSat 光晕|R corpus + V4 PASS|
|ENM-07|原环境移动函数|G enemies、R density PASS|
|ENM-08|1024敌人容量|G capacity PASS|
|WPN-01|Pulse nearest|G weapons 等距槽序 PASS|
|WPN-02|Pulse L1–5 tuning|G weapons + upgrades PASS|
|WPN-03|多弹扇形|G weapons 三弹断言 PASS|
|WPN-04|最近/碰撞候选网格|G weapons 跨单元命中 PASS|
|WPN-05|Orbit 状态与位置|G weapons PASS|
|WPN-06|整数 LUT/相位|G weapons 相位推进 PASS|
|WPN-07|轨道周期伤害|G weapons HP减少 PASS|
|WPN-08|数量/半径/速度等级|G weapons tuning PASS|
|WPN-09|Nova 生命周期|G weapons 一波只命中一次 PASS|
|WPN-10|Scale+Bilinear|R corpus 命令语义 PASS|
|WPN-11|AddSat+衰减alpha|R corpus 命令语义 PASS|
|WPN-12|Nova半径/寿命/命中位图|G weapons + R 双状态 PASS|
|WPN-13|Field 周期伤害|G weapons PASS|
|WPN-14|两层 StraightAlpha|R corpus 命令/OD PASS|
|WPN-15|平方距离含半径边界|G weapons 50/51px PASS|
|WPN-16|四武器独立状态|G weapons + R PASS|
|UPG-01|有效三选一|G upgrades 40事件 PASS|
|UPG-02|候选唯一|G upgrades 两两比较 PASS|
|UPG-03|L5排除|G upgrades PASS|
|UPG-04|L0解锁/已有升阶|G upgrades PASS|
|UPG-05|四类可解锁|G upgrades 全部最终L5 PASS|
|UPG-06|L1–5|G weapons + upgrades PASS|
|UPG-07|固定种子选项一致|G upgrades 双状态 PASS|
|UPG-08|main HUD 四图标等级|V2/V3/V5 实际图标/等级核对 PASS；最终视觉见 VIS|
|SPC-01|固定数组空间网格|S PASS|
|SPC-02|nearest 下界剪枝|S 暴力最近目标 PASS|
|SPC-03|弹丸候选后细判|G weapons 跨边界 PASS|
|SPC-04|拾取邻域后磁吸|G enemies 跨边界维修 PASS|
|SPC-05|oracle128组/4096查询|S PASS|
|SPC-06|1024/2048/2048|G capacity 容量不增长 PASS|
|SYS-01|120连续渲染帧|R corpus PASS|
|SYS-02|60密度渲染帧|R density PASS|
|SYS-03|完整玩法状态hash|G determinism + R双状态 PASS|
|SYS-04|逐帧>=300敌人|R density PASS|
|SYS-05|逐帧>=700组合实体|R density PASS|
|SYS-06|正常600tick无注入|G determinism PASS|
|SYS-07|冻结地图回归|facility_map + 原地图检查 PASS|
|SYS-08|全部 Golden/gpu2d|完整 CTest PASS|
|VIS-01|V1_early.png|PENDING OWNER REVIEW|
|VIS-02|V2_mid.png|PENDING OWNER REVIEW|
|VIS-03|V3_fx.png|PENDING OWNER REVIEW|
|VIS-04|V4_elite.png|PENDING OWNER REVIEW|
|VIS-05|V6_density.png|PENDING OWNER REVIEW|
|VIS-06|showcase_app2_002.png + V7_technical.png|PENDING OWNER REVIEW|

## 变更文件

- 新增 `gameplay.hpp`、`spatial.hpp`、`gameplay.cpp`；调整 `app.hpp/app.cpp` 接入状态与渲染。
- `model/pc_demo/app/main.cpp`、`facility_app2_fixtures.hpp`：HUD、验证入口与明确标注的展示场景。
- 新增 spatial/gameplay/app2_visual 三个测试文件；CMake 注册测试。
- `scripts/capture_app2_002.py`、本轮截图与元数据。
- `PLAN_APP2_002_Implementation.md`、本报告；APP2_001 总报告、验收JSON及两个生成器同步前轮地图闭环。
- 下发的新 review/task 原文未修改；已有 stage0045 未跟踪截图保留。

## 限制、问题与后续

当前无确认的架构 BLOCKER / DESIGN_QUESTION。完整回归已通过；视觉 gate 需 owner/reviewer 审阅实际截图，本报告不自授视觉 PASS。

高密度 fixture 用于确定性/正确性压力，不能证明长期生存平衡或板端吞吐。Nova 按稳定槽位记录一次命中，同波期间复用该槽位的新敌人也不会再次受伤，此规则已在计划明确。本轮不包含 Boss、Chain、Meteor、音频或硬件性能结论。

下一步：正式视觉审查。用户已授权本轮提交并推送；不得用前轮 M13 PASS 代替本轮四武器/高密度视觉批准。
