# APP2_002 实施计划

日期 2026-09-17；START_COMMIT / MAP baseline：57c5eb35d0c64aa7c3940f3d6f11c11f018f8376。

授权依据为 TASK_APP2_002 与 REVIEW_APP2_MAP_R3V2_VISUAL_FINAL。M13 已通过，地图 HOLD 解除；不更改冻结地图或 GPU 语义。

顺序：关闭文档 → 集中 Director 配置 → 128px GameplayGridCell 与不少于100组暴力 oracle → 五敌人/合法屏外生成 → 四武器整数状态与升级 → XP/维修 → HUD/渲染 → 120帧一致性及60帧密度 → 正常600帧/完整回归 → V1–V7/展示/64行证据报告。

容量：1024 enemies / 2048 projectiles / 2048 pickups；预分配并复用；稳定槽位顺序，最近目标距离相同取最低槽号。GameplayGridCell 与 RenderTile、MapTile、MacroChunk 分开命名。

Director 使用60Hz game tick，阶段起点0/7200/18000/28800；参数集中可查询。出生策略允许整个4096世界，要求视口外32–160px、世界边界内且环境合法；找不到候选则跳过，不夹回屏内。

Nova 显式规则：触发时记录中心，逐帧增长的圆盘扫过敌人，每个敌人槽位每次波只受伤一次，结束后重置命中记录。Field 随玩家移动，平方距离判定，按固定周期伤害。Orbit 使用整数LUT、固定相位、周期接触伤害。Pulse 有寿命和范围，网格查最近目标与命中候选。

升级选项在升级事件生成并冻结，四武器L1–5，满级不再提供；不足3项使用不同的有效玩家属性升级。测试手动设置旧 level_up_pending 的兼容面板保留，但自然升级使用新候选生成。

每个stop condition按原task处理，不篡改oracle或Golden预期。正常进度、加速tick验证、人工展示分别标注。新视觉验收仍由用户/审查者授权；本轮不自动提交或推送。
