# RISC-V + FPGA 2D GPU 应用层与最终演示规划 V1.0

> 项目：基于 RISC-V–FPGA 异构架构的 Tile-Based 命令驱动式可编程 2D GPU  
> 文档类型：应用层 / 最终展示规划  
> 版本：V1.0  
> 当前阶段：PC Golden Model 已完成核心功能建模，RTL 尚未进入大规模实现  
> 本文目的：定义“最终显示器上向评委展示什么”，并作为后续 PC Interactive Demo、游戏应用和 FPGA 板级演示开发的上层规划依据。

## 1. 文档目标

本项目最终不是只展示一个“能画图的 FPGA 模块”，而是展示一个完整的：

> **RISC-V + FPGA 通用 2D GPU 平台**

因此最终显示器上的内容必须同时证明三件事：

1. **它能运行真实应用，而不是只能跑单一测试图。**
2. **它具有明显的 2D GPU 技术特征，而不是 CPU 画图或写死的游戏硬件。**
3. **评委可以直观看到架构、功能、性能和视觉效果之间的关系。**

应用层应构造一个可扩展 Demo 平台：

```text
统一启动菜单
    │
    ├── Application 1：高负载 Survivor / Bullet-Hell 游戏
    ├── Application 2：待后续定义
    ├── Application 3：待后续定义
    ├── GPU Playground / Feature Demo
    ├── Architecture X-Ray
    └── Benchmark / Stress Test
```

第一阶段只详细设计 **Application 1**。其他应用保留接口和菜单入口，后续根据开发进度和比赛展示需要再分别设计。

## 2. 总体展示理念

最终现场展示应避免两个极端：

```text
只有游戏
→ 看起来像普通小游戏，看不出 FPGA/GPU 技术含量

只有波形/性能数字
→ 技术很强，但视觉冲击弱，评委理解成本高
```

本项目应采用：

> **“真实应用 + 可视化架构 + 可量化性能”三层演示**

### 2.1 Application Layer

评委第一眼看到的是：

- 游戏；
- GUI；
- 动态 Sprite；
- 粒子；
- Alpha；
- Glow；
- Scaling；
- UI；
- 高密度弹幕。

它回答：

> **“这块 GPU 能做什么？”**

### 2.2 Architecture Visualization Layer

通过按键切换 X-Ray / Heatmap / Tile Grid 等模式，让评委看到：

- Tile 网格；
- 每个 Tile 的 WorkRef 数量；
- Overdraw；
- Active Tile；
- Sprite/Command 数量；
- 当前 Render Mode；
- Texture / Blend 等状态。

它回答：

> **“这个画面为什么和普通 CPU 画图不一样？”**

### 2.3 Performance Layer

显示：

- Sprite 数；
- Particle 数；
- Command 数；
- WorkRef 数；
- Active Tile 数；
- Max Overdraw；
- Frame Time；
- FPS；
- DDR / Pixel / Command 相关硬件计数器（FPGA 阶段加入）。

它回答：

> **“这套架构到底性能怎么样？”**

## 3. 应用层总体软件结构

应用程序不得直接操作 GoldenGPU、Tile Header、WorkRef 或 FPGA 寄存器。

应用层只通过统一 Graphics API 提交绘制请求。

```text
Application
    │
    │ Game / GUI Logic
    ▼
Graphics API
    │
    │ generic 2D draw calls
    ▼
Renderer Front-End
    │
    ├── PC Golden Backend
    └── FPGA Backend
    ▼
GPU
```

建议统一 API：

```cpp
gpu_begin_frame();

gpu_fill_rect(...);

gpu_draw_sprite(...);

gpu_draw_sprite_key(...);

gpu_draw_sprite_alpha(...);

gpu_draw_sprite_scaled(...);

gpu_draw_sprite_ex(...);

gpu_set_clip(...);

gpu_present();
```

应用程序不得知道底层是：

```text
PC Golden
Immediate Mode
Tile Mode
FPGA RTL
```

这保证：

> **同一个应用程序未来可以从 PC Model 无缝迁移到 RISC-V + FPGA。**

## 4. PC Interactive Demo 总体形态

当前阶段先在 PC 上建立一个可实时操作的 Demo Host：

```text
Input
  │
  ▼
Application Logic
  │
  ▼
Graphics API
  │
  ▼
Command Generator
  │
  ▼
Software Tile Binner
  │
  ▼
Golden GPU
  │
  ▼
Framebuffer
  │
  ▼
PC Window Presenter
```

PC Window 层可使用 SDL2 或同等轻量方案，仅负责：

- 窗口；
- 键盘输入；
- 鼠标输入；
- 时间；
- framebuffer 上传显示。

Golden Core 本身继续保持：

- 无窗口依赖；
- 无 SDL/OpenGL 依赖；
- 可测试；
- bit-accurate；
- deterministic。

## 5. 最终启动界面规划

最终板级 Demo 建议开机进入统一 Launcher。

```text
┌──────────────────────────────────────────────────────────┐
│              RISC-V + FPGA 2D GPU DEMO                  │
│                                                          │
│   [1] NEON SURVIVOR          Flagship Game              │
│   [2] APPLICATION 2          Coming / TBD                │
│   [3] APPLICATION 3          Coming / TBD                │
│                                                          │
│   [4] GPU PLAYGROUND         Feature Demonstration       │
│   [5] ARCHITECTURE X-RAY     Tile / Overdraw View        │
│   [6] GPU BENCHMARK          Stress / Performance        │
│                                                          │
│   GPU: Tile-Based 2D Accelerator                         │
│   CPU: RISC-V                                            │
│   Renderer: TILE32                                       │
└──────────────────────────────────────────────────────────┘
```

第一阶段实际实现：

```text
[1] Flagship Game
[4] GPU Playground（可后续实现）
[5] Architecture X-Ray（至少集成在游戏中）
[6] Benchmark（由游戏 Stress Mode 先承担）
```

Application 2 / 3 暂时只保留架构位置，不在本阶段详细定义。

## 6. Application 1：旗舰游戏总体定义

### 6.1 游戏定位

第一款游戏采用：

> **Survivor + Bullet-Hell 高密度 2D 战斗游戏**

核心目标不是游戏玩法复杂度，而是最大程度展示 GPU 的：

- Sprite throughput；
- Alpha；
- Additive；
- Overdraw；
- Scaling；
- Bilinear；
- Palette；
- Clip；
- Tile-Based Rendering；
- Command-driven architecture。

暂定项目名：

> **NEON SURVIVOR**

最终名称可以后续重新设计。

## 7. 游戏核心玩法

玩家位于场景中央附近，可自由移动。

基础操作：

```text
W / A / S / D    移动
鼠标或方向键      可选瞄准
ESC              菜单
```

第一版推荐自动攻击，降低操作复杂度。

玩家具有：

- HP；
- 等级；
- 经验；
- 技能冷却；
- 无敌闪烁；
- Damage Flash。

## 8. 敌人设计

第一版只需要三类敌人。

### 8.1 Normal Enemy

- 数量多；
- 简单追踪玩家；
- 小 Sprite；
- 构成 Sprite Storm 主体。

### 8.2 Fast Enemy

- 速度快；
- 数量中等；
- 强化动态场面。

### 8.3 Heavy Enemy

- Sprite 较大；
- HP 高；
- 可以使用 Scaling；
- 死亡时产生大量粒子。

未来可加入 Boss。

## 9. 武器 / 弹幕系统

第一版建议至少支持四种视觉模式。

### 9.1 Straight Projectile

技术展示：

- BLIT；
- Color Key；
- Sprite throughput。

### 9.2 Radial Burst

技术展示：

- 高 Sprite 数；
- Command throughput。

### 9.3 Spiral Bullet

技术展示：

- Sprite density；
- Overdraw。

### 9.4 Wide Beam / Area Attack

可先使用：

- 拉伸矩形；
- Scale Sprite；
- Alpha overlay。

技术展示：

- Scaling；
- Alpha；
- Clip。

## 10. 粒子系统

粒子系统是最终画面“看起来高级”的关键。

需要至少支持：

- Explosion；
- Spark；
- Trail；
- Smoke；
- Hit Flash；
- Shockwave。

粒子属性：

```text
position
velocity
lifetime
scale
alpha
color
blend mode
```

技术映射：

```text
粒子 Fade      → Global Alpha
粒子 Glow      → Additive Blend
粒子变大       → Scaling
粒子颜色变化    → Color Mod / Palette
大量粒子       → Overdraw / Pixel Throughput
```

## 11. 重点视觉效果

### 11.1 Additive Glow

用于：

- 玩家能量；
- 子弹；
- 激光；
- 爆炸；
- Boss；
- UI Highlight。

### 11.2 Shockwave

击杀或技能释放时产生扩大的半透明圆环，展示：

```text
Scaling
+
Alpha
+
Overdraw
```

### 11.3 Damage Flash

敌人受伤时短时间：

```text
sprite → Color Mod / Palette Variant
```

### 11.4 Trail

高速弹丸或玩家 Dash 后留下半透明残影，展示：

```text
Alpha
Additive
Scaling
```

### 11.5 Background Parallax

地图背景采用至少 2 层滚动：

```text
Far Background
Mid Background
Gameplay Layer
```

避免画面背景过于静态。

## 12. UI / HUD 规划

建议：

```text
┌──────────────────────────────────────────┐
│ HP ████████████░░░     LV 17            │
│ XP ███████░░░░░░                         │
│                                          │
│                         TIME 08:42        │
│                         KILLS 1267        │
│                                          │
│                     GPU MODE: TILE32      │
└──────────────────────────────────────────┘
```

调试/比赛模式下可显示：

```text
Sprite Count
Particle Count
Command Count
WorkRef Count
Active Tiles
Max Overdraw
```

普通游戏模式可隐藏。

## 13. Architecture X-Ray 模式

这是项目展示中必须重点设计的模式。

按键：

```text
F9 / F10
```

在 Normal / X-Ray 之间切换。

### 13.1 Tile Grid Overlay

显示 32×32 Tile 边界和每 Tile WorkRef Count。

### 13.2 Overdraw Heatmap

显示像素 Overdraw 热度。

### 13.3 Active Tile View

只高亮本帧真正有绘制工作的 Tile。

目标是让评委直观看到：

> Tile 架构如何组织高密度 2D 场景。

## 14. Runtime Render Mode 切换

PC 版建议支持：

```text
F6 = Immediate Mode
F7 = Tile Mode
```

画面应保持完全一致。

HUD 显示：

```text
RENDER MODE: IMMEDIATE
```

或：

```text
RENDER MODE: TILE 32x32
```

这可以证明：

> Tile-Based Rendering 改变的是执行与内存组织，而不是游戏逻辑和像素语义。

## 15. Runtime Feature Toggle

建议 PC Demo 中增加开发/演示快捷键：

```text
1  Normal Blend
2  Additive
3  Nearest
4  Bilinear
5  Palette Mode
6  Dither ON/OFF
7  Tile Grid
8  Overdraw Heatmap
```

最终比赛版可隐藏部分开发开关。

## 16. Stress / Benchmark 场景

旗舰游戏内部需要内置压力测试模式。

### 16.1 Sprite Storm

目标：

```text
500
1000
2000
甚至更多 Sprite
```

主要展示：

```text
Command throughput
Sprite throughput
Texture sampling
WorkRef pressure
```

### 16.2 Alpha Storm

大量半透明粒子叠加，主要展示：

```text
Dst Read
Alpha Blend
Overdraw
Pixel throughput
```

### 16.3 Bullet Hell

数千弹幕，主要展示：

```text
高密度 Sprite
Command rate
Texture fetch
```

### 16.4 Scale Storm

大量不同大小 Sprite，主要展示：

```text
Nearest
Bilinear
Scaling pipeline
```

### 16.5 Overdraw Storm

大量 Sprite 和特效集中在中心区域，主要展示：

```text
Tile workload
Overdraw
Blend pressure
```

## 17. 自动压力升级模式

建议最终 Benchmark 支持：

```text
每 N 秒提高 Sprite 数
```

直到：

```text
FPS < 60
```

最终 FPGA 上显示：

```text
MAX SPRITES @ STABLE 60 FPS
```

PC Model 阶段只用于：

- 视觉密度检查；
- Command 数量检查；
- 场景构造。

PC FPS 不代表 FPGA FPS。

## 18. 第一款游戏的画面目标

建议最终目标分辨率：

```text
PC Preview:
1280 × 720

FPGA Bring-Up:
640 × 480

Competition:
1280 × 720 @ 60 FPS

Stretch:
1920 × 1080
```

旗舰场景视觉密度目标：

```text
Enemy     500 ~ 1500
Bullet    500 ~ 3000
Particle  500 ~ 2000
```

这些是画面设计目标，不是当前 FPGA 性能承诺。

## 19. 游戏画面图层设计

推荐从后向前：

```text
Layer 0   Background
Layer 1   Background decoration
Layer 2   Ground effects
Layer 3   Enemy
Layer 4   Player
Layer 5   Bullet / Projectile
Layer 6   Particle
Layer 7   Glow / Shockwave
Layer 8   HUD
Layer 9   Debug / X-Ray Overlay
```

## 20. 美术风格建议

推荐：

> **Dark + Neon Sci-Fi / Cyber Arena**

特点：

- 深色背景；
- 蓝/紫/青/橙发光；
- 高对比度；
- 粒子效果明显；
- Additive Blend 效果突出；
- UI 容易设计成工程仪表风格。

比赛展示优先级：

```text
清晰
动态
高密度
技术感
性能感
```

不以复杂剧情和精细人物立绘为主要目标。

## 21. 游戏与 GPU 功能映射

| 画面/玩法元素 | GPU 技术 |
|---|---|
| 地图背景 | BLIT |
| 玩家/敌人 | BLIT / Color Key |
| Damage Flash | Color Mod / Palette |
| 半透明效果 | Alpha |
| Glow | Additive |
| 粒子 Fade | Global Alpha |
| 爆炸扩大 | Scaling |
| 高质量缩放 | Bilinear |
| Sprite 边缘处理 | Color Key / Alpha |
| UI Clip | Clip Rect |
| Tile Heatmap | Tile Profiler |
| RGB565 输出 | Format Convert / Dither |
| 大规模敌人 | Command + Sprite throughput |
| 大规模弹幕 | Texture/Sprite throughput |
| 爆炸中心 | Overdraw |
| Tile X-Ray | Tile-Based Architecture |

## 22. PC Demo 与最终 FPGA Demo 的一致性

PC 阶段：

```text
Application
   ↓
Graphics API
   ↓
Golden Backend
   ↓
Framebuffer
   ↓
PC Window
```

FPGA 阶段：

```text
Application
   ↓
Graphics API
   ↓
RISC-V Driver
   ↓
Command Ring
   ↓
FPGA GPU
   ↓
DDR Framebuffer
   ↓
Display
```

要求：

> Application 层代码原则上不因 Backend 改变而重写。

游戏逻辑中禁止出现：

```text
Tile Header
AXI
DDR
FPGA Register
GoldenGPU
```

## 23. 后续其他应用的预留位置

当前不对 Application 2 / 3 做正式详细设计，但预留以下候选类型。

### Candidate A — Embedded GUI / HMI

可展示：

- Window；
- Icon；
- Dashboard；
- Animation；
- Alpha；
- Clip；
- Font；
- Chart。

价值：

> 证明 GPU 不只是游戏加速器。

### Candidate B — GPU Playground

用于单独演示：

- Scaling；
- Bilinear；
- Blend；
- Palette；
- Dither；
- Clip；
- Tile；
- Overdraw。

价值：

> 适合答辩时逐项解释技术。

### Candidate C — Architecture Benchmark

用于：

- Sprite Sweep；
- Alpha Sweep；
- Overdraw Sweep；
- Scale Sweep；
- Tile16/32/64 comparison。

价值：

> 量化架构性能。

### Candidate D — 2.5D / Affine Tech Demo

未来如果进度允许：

- Mode-7；
- Affine Ground；
- pseudo-3D；
- scaling road / floor。

价值：

> 展示 GPU 可扩展性。

这些应用不属于第一阶段开发范围，只保留方向。

## 24. 最终比赛演示建议流程

### Step 1 — 直接进入旗舰游戏

先让评委看到：

- 高密度 Sprite；
- 粒子；
- Glow；
- 弹幕；
- 流畅画面。

### Step 2 — 打开 GPU HUD

显示：

```text
Sprites
Particles
Commands
WorkRefs
Active Tiles
Overdraw
FPS
```

强调每帧由 RISC-V 生成命令、FPGA GPU 实时渲染。

### Step 3 — 切换 Architecture X-Ray

显示：

```text
Tile Grid
WorkRef Heatmap
Overdraw Heatmap
```

解释 Tile-Based 架构。

### Step 4 — Immediate / Tile 切换

强调：

> 同一 Graphics API、同一游戏、同一 Pixel Backend，调度与内存架构发生变化。

### Step 5 — Stress Mode

逐渐提高 Sprite 数。

最终得到：

```text
MAX SPRITES @ STABLE 60 FPS
```

### Step 6 — 切换其他应用

后续如果 Application 2/3 完成，再证明：

> **One FPGA Bitstream, Multiple Applications.**

## 25. 第一阶段不做的事情

为了避免应用开发拖慢项目，本阶段明确不做：

- 复杂剧情；
- 完整 RPG 系统；
- 存档；
- 联网；
- 多人；
- 大规模地图编辑器；
- 高级音频系统；
- 大量角色养成；
- 高复杂度 AI；
- 复杂物理；
- 3D 游戏；
- 大型引擎框架。

游戏只是：

> **GPU 技术的高价值可视化载体。**

## 26. 第一阶段完成判据

Application 1 第一阶段完成时，应至少满足：

```text
可执行 PC 程序
可打开实时窗口
键盘可控制玩家
敌人持续生成
玩家/敌人/子弹使用 Graphics API
有 Alpha 粒子
有 Additive Glow
有 Scaling
有 UI
可切 Immediate / Tile
可显示 Tile Grid
可显示 Overdraw
有至少一个 Stress Mode
Game 不直接调用 Golden 内部对象
```

此时已经可以第一次完整预览：

> **最终 FPGA HDMI Demo 的视觉形态。**

## 27. 应用层长期目标

最终项目希望形成：

```text
                 One GPU Bitstream
                        │
       ┌────────────────┼─────────────────┐
       │                │                 │
       ▼                ▼                 ▼
  Survivor Game      GUI / HMI        Benchmark
       │                │                 │
       └────────────────┼─────────────────┘
                        │
                 Common Graphics API
                        │
                 RISC-V + FPGA GPU
```

最终展示重点不是：

> “我们做了一个游戏。”

而应该是：

> **“我们设计了一套面向嵌入式游戏与 GUI 的通用 2D GPU，而游戏只是其中最能展示其吞吐、混合和 Tile 架构能力的应用之一。”**

## 28. 下一步

在本文档确认后，下一步再生成具体实施任务：

> **TASK_0045 — PC Golden Interactive Application Framework & Flagship Game Prototype**

该任务建议采用分 Block + Gate 的形式实施：

```text
Block A  PC Window / Presenter
Block B  Common Graphics API
Block C  Golden Backend
Block D  Sprite Playground
Block E  Survivor Game Core
Block F  Effects / Particle / HUD
Block G  Architecture X-Ray
Block H  Stress / Benchmark Modes
Block I  Acceptance / Demo Capture
```

本文档仅冻结“最终要展示什么”和应用层总体方向，不替代后续实现 TASK。
