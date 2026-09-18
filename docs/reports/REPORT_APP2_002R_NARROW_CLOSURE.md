# APP2_002R — 窄范围整改与审查交付

实现与测试：2026-09-17；归档：2026-09-18。授权：REVIEW_APP2_002_V1_FACILITY_OMEGA + 用户“开始”。

状态：技术整改 PASS；完整回归104/104 PASS、0 FAIL；阶段视觉 PENDING OWNER REVIEW。

- START_COMMIT：`9c355d4efadcd08160262902bc7793933d863b32`。
- MAP R3V2 baseline：`57c5eb35d0c64aa7c3940f3d6f11c11f018f8376`。
- END_COMMIT：`c1778bffbd0a7dd4f4a2dbe03df21034d366d48a`。这是完成最终104项验证的实现、测试与视觉证据版本。本报告随后单独提交，报告提交不改变实现代码；审查代码应以此完整SHA为准。
- 审查边界：不新增武器、地图系统、GPU语义或RTL；未做A2/A3性能重构。

## 审查项处理

|项|处理|证据/状态|
|---|---|---|
|B1 多级升级丢选项|每次越过经验阈值增加 pending_levelups；一次选择扣一个，有剩余就根据新构筑生成下一组并继续暂停|multi_level 定向测试 PASS|
|B2 signed RNG|64位有符号差值/偏移；next()%span 后加回 lo，保证 [lo,hi)，无符号回绕已消除|signed_rng + XP散布 PASS|
|B3 VIS审批|提交全部实际新截图；此前地图M13不扩展为本轮视觉批准|PENDING OWNER REVIEW|
|B4 图片别名|V3开放中央范围效果、V4单个Elite威胁、showcase非对称24敌人构图|三组原始帧 SHA256 不同，独立证据已生成|
|B5 精确END_COMMIT|实现提交与报告提交分离，正文记录完整实现SHA|已记录 `c1778bffbd0a7dd4f4a2dbe03df21034d366d48a`，PASS|
|A1 P/R|Facility自身暂停门控；重置队列、暂停、模拟、视口和相机|input 回归 PASS|
|用户反馈 1/2/3 无响应|清理edge前保存本帧选择；增加NumLock小键盘支持；忽略Win32按键自动重复，避免一次长按消耗多次升级|input 回归 PASS；实体键盘人工复测未执行|
|A2/A3 与 Nova槽位规则|保留现有实现与已披露限制|本轮不实施非阻塞优化|

## 升级与随机数契约

自然升级路径必须使每个 level gain 对应一个 pending_levelups。`apply_upgrade` 只消耗一个，扣至零才能恢复；下组选项在应用当前构筑变化之后生成。队列数量、手动暂停状态均进入 hash_sim。旧测试/人工fixture只设 level_up_pending 的单次入口仍兼容，不影响自然计数路径。

定向案例：Level1、XP9，同tick收集12+2+1，越过10和14两个阈值，变为Level3、XP0、pending=2。第一选择后pending=1且tick/hash保持冻结；第二选择后pending=0恢复；非法选择不扣队列。输入回归另外验证同一edge不会连续消耗两项。

`irange(lo,hi)` 使用64位中间量覆盖完整i32边界，lo>=hi返回lo且不推进PRNG（兼容原退化区间约定）。4096次跨零样本覆盖负/零/正、范围合法、同seed重放；另测全宽、纯负、纯正和空/逆序区间。128次实际 spawn_xp 验证两轴偏移范围以及不恒定为(-6,-6)。

修复会让XP散布消耗真实PRNG步数，从而改变后续刷怪/掉落序列；新hash与旧报告不同是预期变化，未修改Golden像素算术或期望值。

## 验证与可复现命令

工作目录为仓库根。完整日志和截图元数据在 `results/facility_omega/app2_002r/`。

```powershell
cmake --build build/stage0045 -j 1
ctest --test-dir build/stage0045 -R '^gpu2d_test_facility_(signed_rng|multi_level|input|upgrades|determinism)$' --output-on-failure
ctest --test-dir build/stage0045 -R '^gpu2d_test_facility' --output-on-failure
ctest --test-dir build/stage0045 --output-on-failure
python scripts/capture_app2_002.py --output-dir results/facility_omega/app2_002r
git diff --check
git diff -- software/applications/facility_omega/src/environment.cpp software/applications/facility_omega/include/facility/environment.hpp model/golden software/graphics assets
```

|检查|结果|
|---|---|
|串行构建|PASS；原测试文件两处signed/unsigned比较警告保持原状|
|缺陷定向测试|5/5 PASS，0 FAIL，0.30秒|
|Facility专项|16/16 PASS，0 FAIL，93.36秒；facility_tests.log|
|完整Golden/gpu2d|最终版本104/104 PASS、0 FAIL；full_ctest.log，2026-09-17结束|
|空间oracle|128 fixtures / 4096 queries PASS|
|正常600tick无注入|seed1234；level2 / kills8 / spawned43 / collected8 / upgrades1 / HP50 / max_enemies35；sim `b269a3be`|
|120连续渲染帧|双后端逐像素相等；min_enemies12 / min_objects13；max sprites85 / commands324 / WorkRefs1809 / OD58；sim `6c6ba020` / 累计pixels `a875690a`|
|60密度渲染帧|双后端逐像素相等；每帧>=300敌人、>=727组合实体；max sprites1514 / commands2666 / WorkRefs9659 / OD524；sim `aefbf178` / 累计pixels `998722c1`|
|容量|1024 enemy / 2048 projectile / 2048 pickup，复用不扩容；sim `18d0a2f4`|
|地图/API边界|环境、资产、GPU/Golden无diff；原地图/连通性回归PASS|

密度fixture明确将xp_need设为100000，避免意外升级把压力场景冻结；新增逐帧tick=18000+frame+1且无升级暂停断言，确保60帧均推进模拟。高HP、700固定弹丸及人工经验阈值都是压力测试设定，不代表正常游戏平衡。最大统计来自不同帧。已比对专项与最终完整运行，120/60帧sim及累计像素hash逐项相同。

所有结果为 **Agent-local verified**；没有独立CI或FPGA性能结论。前次101/101缺少B1/B2与真实输入顺序覆盖，本轮新增3项注册测试（input、signed_rng、multi_level），没有删除失败测试、弱化断言或改Golden期望。

## 独立视觉证据与门禁

审查入口：[完整八图审查包](../../results/facility_omega/app2_002r/README.md)。包含V1早期、V2中期、V3特效、V4 Elite、V5升级、V6密度、V7技术HUD、独立showcase。

|构图|原始帧 SHA256|
|---|---|
|V3_fx|e6d7696c939a0ed79962d1ce5a81337d46a189efee2fbd3f5d512a356b1d35fd|
|V4_elite|b768a4c492ed26ba2346474db053c9af86d2e87d9124c2f95062e1b57e562c94|
|showcase_app2_002|e53c88eb8234af6af0654854b4fdece3eaef05d80f2c25cdb7ea6565b9982caf|

截图脚本断言这三个原始帧hash不同。图像没有拼接/外部合成；仍使用生产Graphics API和原资产。V1是正常Director180帧空闲输入；其余均为人工场景，V2也排除尚未进入阶段的Elite。旧app2_002图像不覆盖，新证据独立归档。现场检查确认三组构图不同，不能替代owner美术审批。

新随机序列使V5出现Energy Field选项，检查发现完整文案挤入相邻卡片；仅在卡片内缩写为“ENERGY FLD”，武器名称/玩法参数不变。重新编译、生成全部截图并在最终版本重跑完整回归。用户已明确选择“交给后续正式审查”，因此不改变VIS待审状态。

## 64项验收证据索引

G为test_facility_gameplay的命名模式，S为空间oracle，R为app2_visual，V为本轮app2_002r截图。UPG同时受本报告B1队列定向测试约束。当前用户选择交给后续正式视觉审查，六项VIS均不得标PASS。

|ID|实现/行为证据|本轮验证与结果|
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
|SYS-08|全部 Golden/gpu2d|完整 CTest 104/104 PASS|
|VIS-01|V1_early.png|PENDING OWNER REVIEW|
|VIS-02|V2_mid.png|PENDING OWNER REVIEW|
|VIS-03|V3_fx.png|PENDING OWNER REVIEW|
|VIS-04|V4_elite.png|PENDING OWNER REVIEW|
|VIS-05|V6_density.png|PENDING OWNER REVIEW|
|VIS-06|showcase_app2_002.png + V7_technical.png|PENDING OWNER REVIEW|

## 文件变更

- app.hpp/gameplay.cpp：signed RNG、升级计数队列、暂停、hash。
- facility_input.hpp/main.cpp/presenter.cpp：1/2/3生命周期与小键盘、P/R路由、暂停提示。
- test_facility_input.cpp/test_facility_gameplay.cpp/test_facility_app2_visual.cpp、CMake：输入/队列/RNG定向回归、密度持续推进断言。
- facility_app2_fixtures.hpp/capture_app2_002.py：独立构图、输出目录参数、非别名断言。
- 新计划、本报告、下发审查原文、截图/日志/README；旧报告增加明确历史版本和复审FAIL提示。
- 之前的用户临时截图全部保留；冻结规格、地图几何、纹理源、GPU实现不变。

## 限制与下一步

阶段不能自授PASS：B3六项视觉审批仍待审查者决定。本地实现提交仅用于B5版本固定，不等于视觉验收；本轮不推送远端。没有新架构BLOCKER/DESIGN_QUESTION。A2/A3线性空闲槽和全单元最近查询优化继续延期，Nova槽位复用语义仍按前轮披露。

下一步是审阅八张实际截图，逐项判定VIS；若需要调整，仅修改指定构图/表现并重验相关路径。禁止趁本轮扩展Boss/Chain/Meteor/地图/GPU/RTL。
