# Application 2 — Survivor-like Game Concept & Art Direction V0.1

> 项目：RISC-V + FPGA 2D GPU  
> 应用：Application 2 — Survivor-like 完整游戏  
> 临时代号：**FACILITY-Ω**  
> 文档类型：游戏概念设计 + 美术方向 + 素材规划  
> 版本：V0.1  
> 状态：Concept Freeze Candidate  
> 目标：在不新增 GPU 基础语义的前提下，设计一个比 NEON SURVIVOR 更完整、更像正式游戏的 Survivor-like 应用，作为后续比赛现场的旗舰可玩应用之一。

---

# 1. 文档定位

本文件不是实现 TASK，也不是 RTL / Golden 规格。

它用于回答：

> **Application 2 最终要做成什么游戏、画面是什么风格、角色和敌人长什么样、武器和技能怎么表现、第一批素材需要准备什么。**

后续开发顺序建议为：

```text
Concept & Art Direction
        ↓
Asset Plan / First Asset Pack
        ↓
Application 2 Gameplay TASK
        ↓
PC Golden Integration
        ↓
Visual Polish
        ↓
FPGA Demo Migration
```

Application 2 与现有 NEON SURVIVOR 并存，不替换旧游戏。

最终主菜单目标：

```text
RISC-V + FPGA 2D GPU
│
├── Game 1 — NEON SURVIVOR
│            技术原型 / Stress / Architecture Demo
│
├── Game 2 — FACILITY-Ω
│            完整 Survivor-like 旗舰游戏
│
├── GPU Playground
├── Architecture X-Ray
└── Benchmark
```

---

# 2. 游戏核心定位

## 2.1 游戏类型

FACILITY-Ω 定位为：

> **Top-down Survivor-like / Bullet-Heaven 2D 生存动作游戏**

核心玩法：

```text
玩家移动
↓
武器自动攻击
↓
敌人持续从四周涌入
↓
击杀敌人
↓
获得经验晶体
↓
升级三选一
↓
武器 / 技能强化
↓
敌人密度持续提高
↓
Elite / Boss
↓
后期满屏敌人、弹幕、粒子与范围技能
```

游戏操作保持简单，把主要复杂度留给：

```text
敌人密度
武器组合
技能成长
视觉特效
GPU workload
```

---

# 3. 项目展示定位

FACILITY-Ω 不是为了证明“我们会做游戏”。

它的项目价值是：

> **用一个完整、可玩的游戏，把 GPU 的通用 2D 渲染能力自然地展示出来。**

NEON SURVIVOR 更偏：

```text
Feature Demo
Stress Demo
Architecture Demo
```

FACILITY-Ω 更偏：

```text
Playable Game
Visual Showcase
System Integration
Long-running Real Workload
```

两者共同证明：

> **One FPGA Bitstream, Multiple Applications.**

---

# 4. 临时游戏名与世界观

## 4.1 临时名称

首选临时代号：

> # **FACILITY-Ω**

副标题可选：

> **Survive. Reboot. Uncover.**

其他备选：

- ZERO CORE SURVIVAL
- SYNTH SWARM
- CORE: LAST STAND
- MACHINE SIEGE
- NEON FRONTIER

V0.1 暂时使用 **FACILITY-Ω**。

---

# 5. 世界观设定

故事发生在一座废弃的高能实验设施。

过去这里负责：

```text
高密度能源研究
自主机器人维护
实验型防御系统
AI 自动化设施管理
```

一次事故后，设施主控 AI 失控。

自动维护机器人、无人机、防御机甲和实验单位全部进入异常状态。

玩家扮演：

> **一名被困在设施内部的维护工程师。**

工程师本身不是传统战士，而是利用：

```text
维修工具
实验型能量装置
无人机
过载设备
设施残留能源
```

在机械潮中生存。

这使游戏题材与 FPGA/GPU 项目的“工程技术感”保持一致。

---

# 6. 主场景设定

## 6.1 第一主场景

正式推荐：

> **Abandoned Energy Test Facility — 废弃能源试验场**

这是一个大型地下工业设施中的开放测试区域。

场景特征：

```text
蓝灰金属地板
大型钢板拼接
工业管线
电缆
通风格栅
能量罐
设备残骸
黄色/黑色警示条
红色警报灯
青蓝色能源照明
局部蒸汽
电弧
损坏的实验装置
```

---

# 7. 主场景视觉层次

推荐至少使用以下视觉层级。

## Layer 0 — Base Floor

基础地板：

- 深蓝灰金属板
- 拼接线
- 螺栓
- 网格
- 排水/通风格

要求：

> 低对比度，不抢角色和弹幕。

---

## Layer 1 — Facility Markings

地面标识：

```text
A-3
POWER TEST AREA
DANGER
HIGH VOLTAGE
NO RETURN
WARNING
```

可以通过预制贴图或 tile 组合。

价值：

> 提升环境叙事，不需要复杂关卡美术。

---

## Layer 2 — Props

场景道具：

- 能量桶
- 工业箱
- 设备柜
- 电池罐
- 护栏
- 管线
- 控制终端
- 小型发电设备

---

## Layer 3 — Environmental FX

动态环境特效：

- 蒸汽
- 电火花
- 闪烁灯
- 电弧
- 小烟雾
- 能量脉冲

建议尽量由：

```text
小 Sprite
+
Alpha
+
Additive
+
Scaling
```

组合，而不是做大量预渲染动画。

---

# 8. 场景阶段变化

即使第一版只有一张地图，也应随时间推进改变氛围。

## Phase 1 — 0~2 min

```text
环境正常偏暗
警报灯较少
敌人密度低
技能效果较少
```

目标：

> 让玩家和评委能清楚看懂基础画面。

---

## Phase 2 — 2~5 min

```text
红色警报增强
敌人密度上升
设备开始周期闪烁
Elite 出现
粒子密度提高
```

---

## Phase 3 — 5~8 min

```text
机械潮
更多 Overdraw
更多范围技能
Boss 预警
背景出现高能电弧
```

---

## Phase 4 — Late Game

```text
大量敌人
大量弹丸
经验晶体
范围技能
Additive Glow
爆炸
Boss
```

这时画面目标是：

> **“几乎整个屏幕都在动，但玩家仍然清楚可读。”**

---

# 9. 玩家角色设计

## 9.1 身份

玩家角色：

> **Maintenance Engineer / 维护工程师**

---

## 9.2 外形特征

建议视觉元素：

```text
橙色安全头盔
蓝灰工作服 / 轻型外骨骼
青色发光能源背包
小型工程工具枪
护目镜 / 面罩
腰部工具包
```

必须满足：

> 缩小到 24×24 / 32×32 后仍一眼能识别“工程师”。

---

# 10. 玩家 Sprite 规格

建议第一版：

```text
基础尺寸：32×32
```

第一批帧：

```text
Down  × 2
Up    × 2
Left  × 2
Right × 2
```

共：

```text
8 frames
```

后续可扩展：

- Idle
- Hurt
- Dash
- Death

---

# 11. 玩家颜色规范

主色：

```text
Helmet        Orange / Yellow
Suit          Dark Blue / Blue Gray
Energy Pack   Cyan
Highlight     White
Damage State  Red / Pink tint
```

玩家不能与敌人红色发光混淆。

---

# 12. 玩家视觉反馈

## Normal

清晰主体。

## Moving

轻微 2 帧移动动画即可。

## Damage Flash

使用：

```text
Color Mod
```

而不是单独制作大量受击图。

## Invulnerable

可使用：

```text
Alpha blink
Color Mod
```

## Upgrade / Power-up

可使用：

```text
Additive ring
Particles
Scale pulse
```

---

# 13. 敌人总体设计语言

所有机械敌人共用：

```text
黑 / 深灰机体
红色或橙红发光传感器
少量危险黄
```

让它们和玩家的：

```text
青蓝色
```

形成稳定敌我识别。

---

# 14. Enemy 1 — Flying Drone

定位：

```text
Small / Common / Ranged or Contact
```

外形：

- 小型四翼 / 两翼无人机
- 中央红色眼睛
- 高对比发光点

推荐尺寸：

```text
16×16
或
20×20
```

用途：

> Sprite Storm 主力单位。

---

# 15. Enemy 2 — Crawler

定位：

```text
Small / Melee / Swarm
```

外形：

- 4~6 足机械爬虫
- 红色头部传感器
- 低矮宽体

推荐：

```text
20×20 / 24×24
```

视觉作用：

> 和 Flying Drone 在轮廓上明显不同。

---

# 16. Enemy 3 — Runner

定位：

```text
Fast / Fragile / Rush
```

外形：

- 细长机械腿
- 轻型装甲
- 红紫发光

推荐：

```text
20×20
```

玩法：

> 高速度从外围切入。

---

# 17. Enemy 4 — Tank Unit

定位：

```text
Heavy / High HP
```

外形：

- 大型多足机甲
- 厚重装甲
- 炮口
- 红橙发光核心

推荐资源：

```text
32×32 / 48×48
```

展示功能：

```text
Scaling
Bilinear
Damage Flash
Large Sprite
```

---

# 18. Enemy 5 — Elite Unit

定位：

```text
Enhanced / Elite
```

外形：

- 与普通敌人不同的紫色能量场
- 额外 Glow
- 更亮核心

技术展示：

```text
Palette
Color Mod
Additive Aura
```

---

# 19. Boss — Overseer Core

第一 Boss：

> **OVERSEER CORE**

设定：

> 设施中央 AI 防御核心的移动战斗单元。

视觉设计：

```text
中央红色 / 橙色能量核心
环形装甲
多个机械臂 / 炮口
大型结构
```

推荐：

> 不做一个巨大单 Sprite。

而是：

```text
Core Sprite
+
Armor Ring Sprite
+
Weapon Pod Sprites
+
Glow Layer
```

这样既可复用素材，也更适合展示多 Sprite 合成。

---

# 20. Boss 技能

第一版 Boss 可只做 3 个技能。

## Skill A — Radial Pulse

环形弹幕。

## Skill B — Spiral Burst

螺旋弹幕。

## Skill C — Facility Strike

地图局部标记后爆炸。

对应：

```text
Sprite
Scaling
Alpha
Additive
Particles
Overdraw
```

---

# 21. 武器系统总体原则

武器必须满足两个目标：

1. **玩法上易理解**
2. **视觉上能展示 GPU**

不优先追求复杂算法。

---

# 22. Weapon 1 — Pulse Shot

默认武器。

表现：

```text
Engineer
   →
cyan projectile
   →
enemy
```

技术：

```text
BLIT
Color Key
Additive optional
```

素材：

```text
6×6
8×8
```

---

# 23. Weapon 2 — Orbit Drones

数个小型无人机绕玩家旋转。

视觉：

```text
       ○

   ○   P   ○

       ○
```

每个无人机：

```text
8×8 / 12×12
```

技术展示：

```text
Sprite count
Overdraw
Position update
Additive trail
```

不要求 GPU 任意旋转。

---

# 24. Weapon 3 — Plasma Nova

重要展示技能。

效果：

```text
中心发光
↓
圆环快速扩大
↓
Alpha降低
↓
消失
```

建议使用：

```text
1 张 ring texture
+
Scaling
+
Bilinear
+
Alpha
+
Additive
```

不要做 20 张动画。

---

# 25. Weapon 4 — Energy Field

持续存在的范围技能。

效果：

```text
玩家周围
半透明圆形区域
周期脉冲
```

技术：

```text
Alpha
Overdraw
Scaling
```

---

# 26. Weapon 5 — Chain Arc

连锁电弧。

不需要 Vector primitive。

采用：

```text
多个发光节点 Sprite
+
短粒子段
```

拼出电链。

技术：

```text
Many sprites
Additive
Color Mod
```

---

# 27. Weapon 6 — Meteor Strike

高冲击范围攻击。

视觉：

```text
落点预警
↓
高亮闪光
↓
爆炸
↓
火花/碎片
```

技术：

```text
Scaling
Alpha
Additive
Particles
High Overdraw
```

---

# 28. 第一阶段武器范围

V0.1 / MVP 只要求：

```text
Pulse Shot
Orbit Drones
Plasma Nova
Energy Field
```

V0.2 再加入：

```text
Chain Arc
Meteor Strike
```

---

# 29. 升级系统

游戏核心成长循环：

```text
Kill
↓
Drop XP
↓
Collect XP
↓
Level Up
↓
3 Choices
```

---

# 30. 经验晶体

视觉：

```text
Bright Green / Cyan diamond
small glow
```

尺寸：

```text
8×8
```

可用：

```text
Palette Variant
Color Mod
Additive
```

实现不同价值等级。

---

# 31. Level-Up UI

升级时游戏暂停。

中央弹窗：

```text
┌────────────────────────────┐
│          LEVEL UP          │
│                            │
│  [1] PULSE SHOT +1         │
│  [2] ORBIT DRONE           │
│  [3] PLASMA NOVA           │
│                            │
└────────────────────────────┘
```

技术展示：

```text
Alpha overlay
Clip
Bitmap Font
UI Sprite
```

---

# 32. HUD 设计

正常 HUD 建议：

```text
LV. 12
HP ███████████░░
XP ███████░░░░░

TIME 07:32
KILLS 438
```

技能图标：

```text
Pulse
Orbit
Nova
Field
```

技术 HUD 可通过按键打开：

```text
SPRITES
PARTICLES
COMMANDS
WORKREFS
ACTIVE TILES
MAX OVERDRAW
MODE TILE32
```

---

# 33. UI 美术方向

关键词：

> **Industrial Engineering HUD**

表现：

```text
深蓝黑背景
细青色边框
工业字体
小型警告标识
低透明面板
红/黄危险提示
```

避免做成过度科幻透明玻璃 UI。

目标：

> 易读、工程感强、适合比赛演示。

---

# 34. 色彩规范

建议建立统一 Palette Guide。

## Background

```text
#080C14
#101824
#182638
```

## Floor / Steel

```text
Blue Gray
Dark Slate
```

## Player

```text
Cyan
Blue
White
Orange Helmet
```

## Enemy

```text
Dark Gray
Red
Orange
```

## Elite

```text
Purple
Magenta
```

## XP / Positive

```text
Green / Cyan
```

## Damage / Warning

```text
Orange
Red
Yellow
```

---

# 35. 光效原则

画面不能“所有东西都发光”。

发光对象只集中在：

```text
Player energy
Enemy sensors
Weapons
Impact
Explosion
Elite
Boss core
Facility energy devices
```

普通地面和大部分场景道具保持暗色。

这样 Additive Glow 才有视觉价值。

---

# 36. Sprite 风格

推荐：

> **Top-down Stylized Pixel / Low-resolution Sprite + High-energy FX**

主体：

```text
清晰轮廓
少细节
高识别度
```

特效：

```text
更柔和
半透明
发光
可缩放
```

主体 Sprite 不需要做成照片级。

---

# 37. 推荐素材尺寸

| 类型 | 建议尺寸 |
|---|---:|
| Player | 32×32 |
| Common enemy | 16×16 / 24×24 |
| Tank enemy | 32×32 / 48×48 |
| Elite | 32×32 |
| Boss components | 32×32 / 64×64 |
| Bullet | 6×6 / 8×8 |
| XP crystal | 8×8 |
| Particle | 4×4 / 8×8 |
| Glow | 16×16 / 32×32 |
| Nova Ring | 32×32 / 64×64 |
| Floor tile | 32×32 |
| Large prop | 32×32 / 64×64 |
| UI icon | 16×16 / 24×24 |
| Font glyph | 6×8 / 8×8 |

---

# 38. Texture Format Direction

素材原始制作建议：

```text
PNG
```

项目内部转换为 GPU 支持格式。

---

## RGB565

适合：

```text
Floor
Background
Opaque props
Opaque sprites
```

优势：

- 与最终 framebuffer 风格统一
- 带宽较低

---

## ARGB8888

适合：

```text
FX
Glow
Semi-transparent sprites
```

---

## Indexed8 + Palette

适合：

```text
Font
UI icons
Enemy recolor variants
XP variants
```

这是项目中值得主动展示的格式。

---

# 39. Sprite Sheet / Atlas 原则

禁止长期使用大量零散图片作为正式资源结构。

推荐：

```text
player_sheet.png
enemy_sheet.png
weapons_sheet.png
fx_atlas.png
tiles_atlas.png
ui_atlas.png
```

并配：

```text
metadata.json
```

描述：

```text
name
x
y
w
h
anchor
frame
format
palette
```

---

# 40. 推荐资源目录

```text
assets/
└── facility_omega/
    ├── concept/
    │   └── facility_omega_concept_v0_1.png
    │
    ├── player/
    │   ├── engineer_sheet.png
    │   └── engineer_sheet.json
    │
    ├── enemies/
    │   ├── enemies_sheet.png
    │   └── enemies.json
    │
    ├── boss/
    │   ├── overseer_core.png
    │   └── overseer_core.json
    │
    ├── weapons/
    │   ├── weapons_sheet.png
    │   └── weapons.json
    │
    ├── fx/
    │   ├── fx_atlas.png
    │   └── fx.json
    │
    ├── tiles/
    │   ├── facility_tiles.png
    │   └── facility_tiles.json
    │
    ├── ui/
    │   ├── ui_atlas.png
    │   ├── icons.png
    │   └── font_index8.png
    │
    └── palette/
        ├── common.pal
        └── enemy_variants.pal
```

---

# 41. 第一批最小可用素材包

在游戏正式开发前，优先准备以下素材。

## Player

```text
Engineer 4-direction × 2 frames
```

## Enemies

```text
Flying Drone
Crawler
Tank Unit
```

Runner / Elite 可后补。

## Weapons

```text
Pulse Shot
Orbit Drone
Nova Ring
Energy Field
```

## FX

```text
spark
small glow
large glow
explosion
shockwave
trail particle
```

## Map

```text
4~8 floor tiles
warning stripe
grate
energy barrel
crate
console
energy canister
```

## UI

```text
HP frame
XP frame
upgrade card
weapon icon frame
bitmap font
```

这套就足以开始第一版游戏。

---

# 42. FX Atlas 规划

建议优先做一张高复用 FX atlas。

至少包含：

```text
small dot
spark
cross flash
round glow
ring
smoke puff
fragment
small explosion
energy orb
trail
```

不要把所有技能都画成独立动画。

应该尽量依靠：

```text
scale
alpha
blend
color mod
```

复用这批素材。

---

# 43. 动画策略

为了控制开发成本：

## Player

2 帧移动即可。

## Common Enemy

1~2 帧即可。

## Heavy Enemy

可以几乎静态，靠移动和 Glow 增加生命感。

## FX

优先使用：

```text
单纹理
+
Scale
+
Alpha
+
Color Mod
```

而不是帧动画。

---

# 44. GPU 功能映射

| 游戏视觉 | GPU 功能 |
|---|---|
| 工程师 Sprite | BLIT |
| 机械敌群 | BLIT / Color Key |
| Enemy hit flash | Color Mod |
| Elite variant | Palette / Color Mod |
| Pulse Shot | Sprite throughput |
| Orbit Drone | 多 Sprite |
| Nova Ring | Scaling + Bilinear |
| Energy Field | Alpha |
| Glow | Additive |
| Explosion | Additive + Alpha |
| XP crystal variants | Palette |
| HUD | Indexed8 + Palette |
| Upgrade UI | Clip + Alpha |
| Late-game swarm | Tile / WorkRef / Overdraw |
| RGB565 display | Format Convert / Dither |

---

# 45. 游戏 progression 与 GPU progression

游戏设计应有意让 GPU workload 随时间上升。

```text
Early
BLIT / Color Key

Mid
Alpha / Scale / Palette

Late
Additive / Bilinear / High Overdraw / Massive Sprite
```

因此：

> 游戏的成长曲线同时也是 GPU Feature / Workload 曲线。

这非常适合比赛演示。

---

# 46. Architecture X-Ray

FACILITY-Ω 也应支持技术模式。

正常游戏：

```text
Normal
```

技术展示：

```text
Tile Grid
WorkRef Heatmap
Overdraw
Active Tile
```

但不要默认打开。

正常玩家应该感觉它是一款完整游戏。

评委需要时再：

```text
F10 → X-Ray
```

---

# 47. 与 NEON SURVIVOR 的区别

## NEON SURVIVOR

定位：

```text
Technical Demo
Stress Playground
Feature Toggle
```

重点：

```text
GPU功能和架构可视化
```

---

## FACILITY-Ω

定位：

```text
Playable Flagship Game
```

重点：

```text
完整玩法
升级系统
角色/敌人设计
Boss
视觉表现
```

不能只是 NEON SURVIVOR 换皮。

---

# 48. 第一阶段玩法范围

Application 2 V0.1 建议实现：

```text
1 张主地图
1 名工程师
3 类敌人
4 种武器
XP
升级三选一
HP / Death
计时
Kill
简单结算
1 个 Elite
```

Boss 可以进入 V0.2。

---

# 49. 第二阶段玩法范围

V0.2：

```text
5 类敌人
6 种武器
Elite
Overseer Core Boss
Boss 技能
更多升级
更完整 UI
```

---

# 50. 第一版不做

明确不做：

```text
复杂剧情
任务系统
对话
存档
多人
联网
复杂装备栏
开放世界
大地图探索
高复杂物理
复杂 AI
角色骨骼动画
3D
```

核心目标：

> **一个完成度高的 Survivor-like Demo。**

---

# 51. 美术验收原则

素材好不好，不以“细节数量”判断。

优先级：

```text
1. 识别度
2. 风格统一
3. 动态画面可读性
4. 特效层次
5. 高密度场景不混乱
6. 技术展示价值
7. 细节丰富程度
```

---

# 52. 画面可读性规则

## Rule 1

玩家必须始终最容易识别。

## Rule 2

敌人主体不要和子弹颜色相同。

## Rule 3

背景亮度显著低于战斗主体。

## Rule 4

大型 Glow 不能长期遮住角色。

## Rule 5

后期高密度画面仍应看得见：

```text
player
danger
XP
major skills
```

---

# 53. 概念图作为 Art Direction Reference

当前 Concept Art 建议作为：

> **V0.1 美术统一参考**

主要参考：

```text
场景色调
工程师造型
机械敌人轮廓
工业地图
武器发光风格
HUD结构
Boss设计语言
```

不要求最终逐像素复刻。

正式 Sprite 应根据：

```text
实际渲染分辨率
Sprite 尺寸
RGB565
FPGA带宽
```

做简化和量化。

---

# 54. 概念图中值得保留的视觉元素

建议冻结：

```text
Orange Engineer Helmet
Cyan Player Energy
Red Enemy Sensors
Blue-gray Facility
Yellow-black Hazard Stripe
Purple Elite / Nova
Orange Explosion
Engineering HUD
Overseer Core
```

这几项组合起来已经形成较明显的独立视觉身份。

---

# 55. 素材生产流程

推荐：

```text
Concept
↓
Sprite Sketch
↓
Small-size readability test
↓
PNG master
↓
Atlas pack
↓
RGB565 / ARGB8888 / Indexed8 conversion
↓
PC Golden preview
↓
in-game visual review
↓
polish
```

---

# 56. 资源制作阶段划分

## Asset Phase A — MVP

只求：

```text
统一
能用
清晰
```

## Asset Phase B — Visual Upgrade

增加：

```text
animation
more props
better explosion
elite glow
upgrade UI
```

## Asset Phase C — Competition Polish

集中打磨：

```text
Boss
main menu
level-up popup
late-game scene
high-density effects
UI
```

---

# 57. 技术限制下的美术原则

我们最终是 FPGA GPU，不应设计大量必须依赖 PC GPU Shader 的画面。

避免把核心视觉依赖于：

```text
复杂 Shader
实时模糊
动态光照
屏幕空间后处理
复杂旋转
实时阴影
```

优先采用：

```text
Sprite
Alpha
Additive
Scaling
Bilinear
Palette
Color Mod
Layering
Particles
```

保证：

> 概念图可以被我们的 GPU 能力真实落地。

---

# 58. 第一版画面密度目标

正常中期：

```text
Enemies   150~400
Bullets   100~500
Particles 100~500
```

后期展示目标：

```text
Enemies   500~1500
Bullets   500~3000
Particles 500~2000
```

这些是：

> 视觉设计目标，而不是当前 FPGA 性能承诺。

---

# 59. 分辨率目标

```text
PC Prototype:
640×360

Showcase Capture:
1280×720

FPGA Bring-up:
640×480 / 640×360 equivalent

Competition Target:
1280×720 @ 60 FPS
```

---

# 60. 最终比赛演示场景建议

一个理想现场流程：

```text
Main Menu
↓
选择 FACILITY-Ω
↓
开始游戏
↓
工程师移动
↓
敌人逐渐增多
↓
Level Up
↓
获得 Plasma Nova
↓
范围技能开始出现
↓
Elite
↓
后期机械潮
↓
打开 Tech HUD
↓
打开 X-Ray
↓
展示 Tile / WorkRef / Overdraw
```

这样技术展示是从真实游戏自然引出的。

---

# 61. V0.1 美术冻结项

建议从本版本开始暂时冻结：

```text
Theme:
Abandoned Energy Test Facility

Player:
Maintenance Engineer

Player color:
Orange + Blue + Cyan

Enemy:
Dark Machine + Red/Orange sensor

Elite:
Purple

World:
Blue-gray industrial facility

FX:
Cyan / Purple / Orange neon energy

Boss:
Overseer Core

UI:
Industrial engineering HUD
```

---

# 62. V0.1 暂不冻结项

后续可以变化：

```text
最终游戏名
剧情
精确角色造型
敌人数量
武器数值
Boss阶段
UI具体布局
Sprite帧数
最终地图尺寸
```

---

# 63. 下一步建议

在开始写 Application 2 Gameplay TASK 前，建议先完成：

> **Application 2 — First Asset Pack Plan / Asset Production Task**

第一批只需要制作：

```text
Engineer
Drone
Crawler
Tank
Pulse Shot
Orbit Drone
Nova Ring
Energy Field
FX Atlas
Floor Tiles
Props
UI Basic
Bitmap Font
```

当这批素材有了以后，再正式进入：

> **TASK — Application 2 Survivor-like Gameplay Prototype**

---

# 64. 结论

FACILITY-Ω 的核心目标不是复制某个现有游戏。

它借鉴 Survivor-like 的优秀玩法结构：

```text
移动
自动攻击
敌潮
经验
升级
Build成长
Boss
```

但在世界观和视觉上形成独立身份：

> **工程师 + 废弃能源设施 + 失控机械潮 + 工业霓虹 + 高密度 GPU 特效**

最终希望形成的印象是：

> **“这是一个真正可玩的 FPGA 2D GPU 游戏，而不是一个为了跑 benchmark 拼出来的测试场景。”**
