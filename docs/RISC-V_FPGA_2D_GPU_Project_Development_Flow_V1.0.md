# RISC-V–FPGA 通用 2D GPU
# 项目开发流程与实施计划 V1.0

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：项目开发流程 / 实施计划 / 阶段门禁  
> 版本：V1.0  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`  
> 状态：Project Execution Baseline

---

# 0. 文档目的

本文档用于规定本项目从规格设计、PC Golden Model、FPGA RTL、板级系统、软件、应用 Demo 到最终竞赛交付的完整开发流程。

核心目标：

1. 避免“边写RTL边改架构”；
2. 避免软件、Golden、RTL 三套行为不一致；
3. 避免后期新增 Tile、Scaling、Palette、2.5D 等功能时大规模返工；
4. 保证每个新功能都经过规格、Golden、RTL、仿真、上板、性能测试的完整闭环；
5. 保证项目始终保留一个可运行、可回退、可演示的稳定版本；
6. 形成最终答辩所需的完整工程证据链和性能演进数据。

本项目采用：

> **规格先行 + Golden先行 + 模块化RTL + 阶段门禁 + 持续回归 + 数据驱动优化**

的开发方法。

---

# 1. 总体开发流程

```text
需求定义
   ↓
System Architecture
   ↓
Command ISA
   ↓
Internal Interface Spec
   ↓
Pixel / Arithmetic Spec
   ↓
────────────────────────────
        规格冻结 Gate A
────────────────────────────
   ↓
PC Golden GPU ───────┐
Architecture Model ──┤
Board Bring-up ──────┤  并行推进
Software Framework ──┤
Demo Prototype ──────┘
   ↓
Basic GPU RTL
   ↓
Official Baseline Complete
   ↓
Command Ring / Fence / IRQ
   ↓
Tile-Based Renderer
   ↓
Pixel Pipeline Enhancements
   ↓
Performance / X-Ray
   ↓
Main Game / GUI / Benchmark
   ↓
2.5D / 3D Stretch
   ↓
System Optimization
   ↓
Regression Freeze
   ↓
Competition Release
```

---

# 2. 项目阶段划分

本项目分为 17 个阶段。

| 阶段 | 名称 | 核心目标 |
|---|---|---|
| P0 | Requirements Freeze | 明确最终实现目标 |
| P1 | System Architecture | 冻结总体模块与数据流 |
| P2 | Command ISA | 冻结CPU/GPU命令协议 |
| P3 | Internal Interface | 冻结RTL模块接口 |
| P4 | Pixel Arithmetic | 冻结位精确算法 |
| P5 | PC Golden GPU | 建立可执行参考模型 |
| P6 | Architecture Modeling | Tile/BW/Lane/Cache探索 |
| P7 | Board Bring-up | DDR/HDMI/RISC-V基础链路 |
| P8 | Basic GPU RTL | Fill/Blit/Immediate |
| P9 | Official Advanced | Key/Alpha/Double Buffer |
| P10 | Async GPU Front-End | Command Ring/Fence/IRQ |
| P11 | Tile Renderer | 核心存储架构创新 |
| P12 | Pixel Enhancements | Scale/Bilinear/Palette/Add |
| P13 | Perf / X-Ray | 性能计数与可视化 |
| P14 | Applications | 主游戏/GUI/Benchmark |
| P15 | 2.5D / 3D Stretch | Mode-7/Triangle |
| P16 | Final Optimization | 时序/资源/性能/稳定性 |
| P17 | Release & Defense | 比赛发布与答辩材料 |

---

# 3. Gate A：规格冻结

在进入大规模 RTL 开发前，必须完成以下文档。

## P0 — Requirements Freeze

状态：

> 已完成

输出：

- `Requirements V1.0`

冻结：

- 项目定位；
- 应用；
- GPU功能；
- P0/P1/P2/P3优先级；
- 720p60竞赛目标；
- 核心KPI；
- Command / Tile / Pixel三条创新主线。

---

## P1 — System Architecture

状态：

> 已完成

输出：

- `System Architecture V1.0`

冻结：

- 软件/硬件分层；
- Command Front-End；
- 多Render Front-End；
- Texture Unit；
- Shared Pixel Back-End；
- Immediate / Tile Render Target；
- Central Memory Service；
- Display Engine；
- Perf/Debug；
- 2.5D/3D扩展位置。

---

## P2 — Command ISA

状态：

> 已完成

输出：

- `GPU Command ISA V0.1`

冻结：

- 64B Command；
- Opcode；
- 2D Draw Descriptor；
- Tile Work List；
- Present；
- Fence；
- Extension；
- Q16.16 UV；
- 未来Affine/Triangle命令空间。

---

## P3 — Internal Interface Specification

状态：

> 下一阶段

必须定义：

- `command_if`
- `render_context_if`
- `work_if`
- `fragment_if`
- `texture_req_if`
- `texture_rsp_if`
- `pixel_if`
- `render_target_if`
- `memory_req_if`
- `memory_rsp_if`
- `event_if`
- `fault_if`

必须冻结：

- valid/ready语义；
- payload字段；
- backpressure；
- lane语义；
- error行为；
- clock domain归属。

---

## P4 — Pixel Format & Arithmetic Specification

必须定义：

- RGB565；
- ARGB8888；
- RGBA8888；
- Indexed8；
- RGB565↔RGBA8888转换；
- Alpha公式；
- Global Alpha；
- Per-Pixel Alpha；
- Straight/Premultiplied Alpha；
- Additive；
- Color Key；
- Q16.16；
- Nearest；
- Bilinear；
- Rounding；
- Saturation；
- Pixel Center规则。

目标：

> PC Golden 与 RTL 对同一输入得到逐bit一致的输出。

---

# 4. Gate A 通过条件

只有以下全部完成才允许进入“正式大规模GPU RTL开发”：

- [x] Requirements
- [x] System Architecture
- [x] Command ISA
- [ ] Internal Interface
- [ ] Pixel Arithmetic
- [ ] Register/Memory Map初版
- [ ] Golden Model框架设计
- [ ] Verification Plan初版

允许例外：

> DDR/HDMI/RISC-V板级Bring-up可以提前并行开始。

---

# 5. P5：PC Golden GPU

## 5.1 目标

开发一个：

> **Bit-Accurate Software GPU**

它直接解析最终 GPU Command ISA。

结构：

```text
Game / Benchmark
       ↓
Graphics API
       ↓
Command Builder
       ↓
64B GPU Command
       ↓
PC Golden GPU
       ↓
Software Framebuffer
       ↓
PC Window / PNG / RAW
```

---

## 5.2 Golden Model 第一阶段功能

按顺序：

1. FILL_RECT
2. BLIT
3. Color Key
4. Global Alpha
5. Per-Pixel Alpha
6. Clip
7. BLIT_EXT
8. Scaling
9. Indexed8 / Palette
10. Additive
11. Tile Renderer
12. Bilinear
13. Affine

---

## 5.3 Golden Model必须输出

- `commands.bin`
- `textures.bin`
- `initial_fb.bin`
- `golden_fb.bin`
- `golden_frame.png`
- Performance Statistics
- Debug Trace

---

## 5.4 Golden Model用途

同时承担：

- 图形功能参考；
- ISA解析参考；
- RTL验证；
- Demo原型；
- 随机测试；
- 架构建模输入；
- CPU/GPU公平对比基准。

---

# 6. P6：Architecture Modeling

在写复杂RTL前，用PC建模回答架构问题。

## 6.1 Tile Model

扫描：

- 16×16
- 32×32
- 64×64

统计：

- Tile数；
- WorkRef数；
- Sprites/Tile；
- Tile Load；
- Tile Store；
- DDR Bytes/Frame；
- CPU Binning时间；
- BRAM需求。

默认32×32，但允许根据数据调整。

---

## 6.2 Memory Model

建立：

\[
P_{actual}
=
\min(P_{compute},P_{memory})
\]

分析：

- RGB565；
- ARGB8888；
- Alpha RMW；
- Texture Read；
- Display Scanout；
- Tile Load/Store；
- 720p60。

输出：

- DDR Bandwidth Budget；
- Worst-case估计；
- Average workload估计。

---

## 6.3 Lane Model

比较：

- 1 pixel/clk；
- 2 pixels/clk；
- 4 pixels/clk。

只有在 DDR 有余量时才增加 Lane。

---

## 6.4 Texture Cache Model

如果考虑 Cache：

扫描：

- 1KB；
- 2KB；
- 4KB；
- 8KB；
- 16KB；

以及：

- Direct-Mapped；
- 2-Way。

统计：

- Hit Rate；
- DDR Reduction；
- BRAM Cost。

---

# 7. P7：Board Bring-up

与 Golden/Model 并行。

## 7.1 基础顺序

```text
Clock / Reset
   ↓
DDR
   ↓
Framebuffer
   ↓
Display Timing
   ↓
HDMI
   ↓
RISC-V
   ↓
MMIO
```

---

## 7.2 M0：Video Baseline

必须做到：

- DDR稳定；
- HDMI稳定；
- 显示Color Bar；
- 棋盘格；
- 固定图片；
- 无明显闪烁/撕裂；
- 能连续运行至少30分钟。

---

## 7.3 M1：CPU → Framebuffer

RISC-V：

- 写 framebuffer；
- 修改像素/矩形；
- HDMI看到对应结果。

证明：

- CPU；
- DDR；
- Display；

完整链路可用。

---

# 8. P8：Basic GPU RTL

第一个正式 GPU。

## 8.1 实现顺序

### Step 1
`NOP`

验证 Command Parser。

### Step 2
`FILL_RECT`

验证：

- 2D Address；
- Write FIFO；
- Burst Write；
- Immediate RT。

### Step 3
`BLIT`

增加：

- Texture Read；
- Read FIFO；
- Format Decode；
- Copy Pipeline。

---

## 8.2 Basic GPU 验收

必须达到：

```text
Golden Model
    =
RTL Simulation
    =
FPGA Hardware
```

至少对：

- Fill；
- Blit；

逐像素一致。

---

# 9. P9：Official Advanced Features

优先完成官方高阶功能，形成安全可交付版本。

顺序：

1. Burst优化
2. FIFO优化
3. Color Key
4. Global Alpha
5. Per-Pixel Alpha
6. Double Buffer
7. VSYNC Page Flip
8. CPU vs GPU Benchmark

---

## 9.1 Gate D：Official Complete

通过条件：

- [ ] Fill
- [ ] Blit
- [ ] Burst/FIFO
- [ ] Color Key
- [ ] Alpha
- [ ] Double Buffer
- [ ] CPU/GPU FPS
- [ ] 简单Sprite Demo
- [ ] Golden Regression全部PASS

达到这里：

> 即使后续创新延期，项目也已有完整官方交付基础。

---

# 10. P10：Command-Driven Async GPU

这是核心创新1。

## 10.1 实现

```text
CPU
 ↓
DDR Command Ring
 ↓
Command DMA
 ↓
Parser
 ↓
Dispatcher
 ↓
GPU Pipeline
```

加入：

- HEAD/TAIL；
- Doorbell；
- Fence；
- IRQ；
- Fault；
- Sequence ID。

---

## 10.2 验收

比较：

### Direct MMIO
vs
### Command Ring

统计：

- CPU Submit Cycles；
- Commands/s；
- CPU/GPU overlap；
- FPS；
- CPU Load。

---

# 11. P11：Tile-Based Renderer

这是核心创新2。

## 11.1 软件侧

实现：

- Draw List；
- Bounding Box；
- Tile Binning；
- Tile Header；
- Work List；
- Draw Descriptor Array。

---

## 11.2 FPGA侧

实现：

```text
TILE_FRAME
   ↓
Tile Scheduler
   ↓
Tile Load
   ↓
WorkRef Fetch
   ↓
Draw Descriptor Fetch
   ↓
Render
   ↓
Tile Store
```

---

## 11.3 第一版

先：

> Single-Bank Tile Buffer

验证正确性。

---

## 11.4 第二版

再考虑：

> Dual-Bank Load/Render/Store overlap

不要一开始就优化。

---

## 11.5 Tile 验收

必须证明：

### 图像一致

\[
ImmediateFrame = TileFrame
\]

对同一 Command List：

逐像素一致。

### 性能收益

统计：

- DDR Read Bytes；
- DDR Write Bytes；
- Frame Time；
- FPS；
- Tile Reuse；
- Max Sprites@60FPS。

---

# 12. P12：Unified Pixel Pipeline Enhancements

核心创新3在这一阶段完成。

推荐顺序：

1. Clip
2. Scaling Nearest
3. Additive Blend
4. Indexed8
5. Palette
6. Bilinear
7. Flip
8. Color Modulate
9. Optional ROP

---

## 12.1 每个功能都遵循同一流程

```text
Spec
 ↓
Golden
 ↓
Directed Test
 ↓
Random Test
 ↓
RTL
 ↓
Unit Simulation
 ↓
Full Frame
 ↓
Synthesis
 ↓
Board
 ↓
Benchmark
```

---

# 13. P13：Performance / X-Ray

## 13.1 Performance Counter

逐步完善：

```text
TOTAL_CYCLES
BUSY_CYCLES

COMMAND_COUNT
COMMAND_STALL

PIXEL_COUNT
BLEND_PIXEL_COUNT
KEY_DISCARD_COUNT
SCALE_PIXEL_COUNT

DDR_READ_BYTES
DDR_WRITE_BYTES
DDR_STALL_CYCLES

TILE_LOAD_COUNT
TILE_STORE_COUNT
TILE_WORK_COUNT

CACHE_HIT
CACHE_MISS
```

---

## 13.2 X-Ray

实现：

- Tile Grid；
- Active Tile；
- Sprite Bounding Box；
- Sprites/Tile；
- GPU Util；
- DDR BW；
- Command Queue；
- Tile Reuse；
- Cache Hit（若有）。

---

## 13.3 Architecture Ablation

必须至少完成：

### Immediate OFF/ON Tile

最好再支持：

- Cache OFF/ON；
- Bilinear OFF/ON；
- Command Ring vs Direct Submit。

---

# 14. P14：Application Integration

## 14.1 主游戏

实现：

> 原创高负载“幸存者 + 弹幕”型 Demo

包含：

- 敌人；
- 弹幕；
- Particle；
- Boss；
- Alpha；
- Additive；
- Scale；
- Parallax；
- HUD。

---

## 14.2 Benchmark

独立场景：

- Sprite Storm；
- Alpha Storm；
- Overdraw Storm；
- Scaling Storm；
- Particle Storm。

---

## 14.3 GUI/HMI

使用同一 FPGA Bitstream。

展示：

- Window；
- Icon；
- Gauge；
- Graph；
- Menu；
- Transparency。

用于证明通用性。

---

# 15. P15：2.5D / 3D Stretch

只有 Gate G 之后才允许正式投入主要开发时间。

---

## 15.1 第一优先：Affine / Mode-7

实现：

\[
u=ax+by+c
\]

\[
v=dx+ey+f
\]

复用：

- Texture；
- Sampler；
- Pixel Backend；
- Tile；
- Display。

---

## 15.2 第二优先：Triangle

如果时间允许：

- Flat Triangle；
- Edge Function；
- Optional Gouraud；
- Optional Z。

CPU负责：

- Vertex Transform；
- Triangle Setup；
- Binning。

FPGA负责：

- Raster；
- Depth；
- Pixel。

---

# 16. P16：Final Optimization

最终优化必须基于数据。

---

## 16.1 Timing Optimization

检查：

- Critical Path；
- Fanout；
- BRAM Output；
- DSP Pipeline；
- Ready Path；
- CDC；
- Reset Fanout。

---

## 16.2 Resource Optimization

维护：

- LUT；
- FF；
- BRAM；
- DSP。

不能以牺牲核心功能换低资源数据，但要避免无效冗余。

---

## 16.3 Memory Optimization

基于 Counter 判断：

- Burst；
- Tile；
- Cache；
- Lane；
- Texture Format。

---

## 16.4 Optimization Rule

如果：

```text
Compute < Memory
```

优化：

- Lane；
- Pipeline。

如果：

```text
Memory < Compute
```

优化：

- Tile；
- Burst；
- Cache；
- Indexed8。

---

# 17. P17：Release & Competition

## 17.1 Final Release Freeze

冻结：

- FPGA Bitstream；
- RISC-V Software；
- Assets；
- Benchmark；
- Golden；
- Driver；
- Demo；
- Documentation。

禁止比赛前最后几天加入未经完整回归的新功能。

---

## 17.2 最终发布内容

```text
release/
│
├─ bitstream/
├─ software/
├─ driver/
├─ benchmark/
├─ games/
├─ gui/
├─ golden/
├─ docs/
├─ results/
└─ demo_assets/
```

---

# 18. 四条并行开发线

从 Gate A 后项目应并行开发。

---

## Track A：Architecture / RTL

负责：

- Command；
- Front-End；
- Texture；
- Pixel；
- Tile；
- Memory；
- Perf。

---

## Track B：Board / System

负责：

- RISC-V；
- DDR；
- HDMI；
- Clock；
- Reset；
- Bus；
- IRQ；
- CDC。

---

## Track C：Software / Verification

负责：

- Driver；
- Graphics API；
- Golden；
- Tile Binner；
- Test Generator；
- Regression；
- Benchmark Scripts。

---

## Track D：Application / Demo

负责：

- Game；
- GUI；
- Benchmark Scene；
- X-Ray；
- Assets；
- Demo流程。

---

# 19. 推荐多人协作分工

如果3人：

### Member A — GPU Architecture / RTL
重点：

- Command；
- Pixel；
- Tile；
- Memory。

### Member B — Platform / System
重点：

- DDR；
- HDMI；
- RISC-V；
- Driver底层；
- IRQ；
- Timing。

### Member C — Golden / Software / Demo
重点：

- Golden；
- API；
- Tile Binner；
- Benchmark；
- Game；
- GUI。

共同负责：

- Specification；
- Interface Review；
- Integration；
- Performance Analysis。

---

# 20. 每个新功能的标准开发闭环

任何新 Feature 禁止直接从“想法”进入 RTL。

必须走：

```text
① Requirement
   ↓
② Specification
   ↓
③ Golden Model
   ↓
④ Test Vector
   ↓
⑤ RTL
   ↓
⑥ Unit Simulation
   ↓
⑦ Random Regression
   ↓
⑧ Full Frame Comparison
   ↓
⑨ Synthesis / Timing
   ↓
⑩ FPGA Board Test
   ↓
⑪ Performance Measurement
   ↓
⑫ Documentation
```

---

# 21. RTL 模块级开发规则

每个 RTL 模块至少包含：

```text
rtl/<module>/
├─ <module>.sv
├─ <module>_pkg.sv
├─ README.md
└─ optional_submodules/
```

验证：

```text
sim/<module>/
├─ tb_<module>.sv
├─ vectors/
├─ expected/
└─ run/
```

---

# 22. Unit Test 原则

一个模块不允许第一次测试就在系统 Top。

例如：

### Alpha Blend

必须先：

```text
alpha_blend_unit
    ↓
unit test
```

再接：

```text
Pixel Backend
```

---

# 23. Subsystem Test

子系统建议：

### S1
Command Parser + 2D Front-End

### S2
Texture + Sampler

### S3
Pixel Backend

### S4
Immediate RT + Memory Service

### S5
Tile Scheduler + Tile Buffer

### S6
Display Engine

---

# 24. Full-System Simulation

最终：

```text
Command Binary
    ↓
GPU Top RTL
    ↓
DDR Model
    ↓
Framebuffer Dump
    ↓
Compare
    ↓
Golden Frame
```

输出：

```text
PASS
```

或者：

```text
Mismatch:
Frame = ...
X = ...
Y = ...
Golden = ...
RTL = ...
Command Seq = ...
User Tag = ...
```

---

# 25. Directed Test

每项功能建立明确测试。

例如：

### Fill

- 1×1；
- 整屏；
- 边缘；
- 负坐标+clip；
- RGB565。

### Alpha

- alpha=0；
- alpha=255；
- alpha=1；
- alpha=128；
- 黑/白；
- 极值。

### Scaling

- 1:1；
- 0.5x；
- 2x；
- 非整数比；
- 极小尺寸。

---

# 26. Random Regression

自动随机：

- Position；
- Size；
- Format；
- Alpha；
- Key；
- Scale；
- Clip；
- Draw Order。

推荐长期目标：

> 每次重要提交运行数千至数万条随机Command。

---

# 27. Regression 分级

## Quick Regression

每次开发：

- Unit；
- 小随机；
- 典型Frame。

目标：

几分钟。

## Nightly / Full Regression

重大版本：

- 全功能；
- 大随机；
- 多Frame；
- Tile/Immediate；
- Performance sanity。

目标：

几十分钟至数小时。

---

# 28. 综合与时序流程

禁止：

> RTL全写完后才第一次综合。

每完成一个主要阶段立即：

```text
RTL
 ↓
Synthesis
 ↓
Resource
 ↓
Place & Route
 ↓
STA
```

至少在以下节点检查：

- Basic GPU；
- Alpha；
- Command Ring；
- Tile；
- Bilinear；
- Multi-Lane。

---

# 29. 三类预算

项目全程维护：

## 29.1 Resource Budget

```text
LUT
FF
BRAM
DSP
```

---

## 29.2 Bandwidth Budget

```text
Display
Command
Texture
Framebuffer
Tile
Depth
```

---

## 29.3 Timing Budget

```text
GPU Core
DDR Interface
Display Pixel
HDMI
CDC
```

---

# 30. 性能数据版本化

每个主要版本维护：

| Version | Feature | Fmax | LUT | FF | BRAM | DSP | FPS | DDR BW | Sprite@60 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| v0.x | Fill | | | | | | | | |
| v0.x | Blit | | | | | | | | |
| v0.x | Alpha | | | | | | | | |
| v1.x | Ring | | | | | | | | |
| v1.x | Tile | | | | | | | | |
| v1.x | Scale | | | | | | | | |

---

# 31. Git / Version Control 建议

主分支：

```text
main
```

只存：

> 已通过核心回归的版本。

开发：

```text
dev
```

功能分支：

```text
feature/fill
feature/blit
feature/alpha
feature/cmd-ring
feature/tile
feature/scaler
feature/palette
feature/mode7
```

修复：

```text
fix/...
```

---

# 32. Merge 规则

功能分支合入 `dev` 前至少：

- Unit PASS；
- Golden PASS；
- Quick Regression PASS；
- Synthesis通过。

`dev` 合入 `main` 前：

- Full Regression PASS；
- Board Smoke Test PASS；
- 关键Benchmark无明显退化。

---

# 33. 项目目录建议

```text
fpga-2d-gpu/
│
├─ docs/
│  ├─ requirements/
│  ├─ architecture/
│  ├─ isa/
│  ├─ interface/
│  ├─ verification/
│  └─ benchmark/
│
├─ rtl/
│  ├─ ctrl/
│  ├─ command/
│  ├─ frontend/
│  ├─ texture/
│  ├─ pixel/
│  ├─ tile/
│  ├─ memory/
│  └─ display/
│
├─ sim/
│  ├─ unit/
│  ├─ subsystem/
│  ├─ random/
│  └─ fullsystem/
│
├─ model/
│  ├─ golden/
│  ├─ tile_model/
│  ├─ cache_model/
│  └─ perf_model/
│
├─ software/
│  ├─ driver/
│  ├─ graphics/
│  ├─ engine/
│  ├─ benchmark/
│  └─ applications/
│
├─ tools/
│  ├─ texture_converter/
│  ├─ command_generator/
│  ├─ frame_compare/
│  └─ result_parser/
│
├─ assets/
├─ board/
├─ results/
└─ release/
```

---

# 34. 开发阶段门禁

## Gate A — Specification Freeze

必须：

- Requirements
- Architecture
- ISA
- Internal Interface
- Pixel Arithmetic

---

## Gate B — Platform Ready

必须：

- Clock
- DDR
- HDMI
- CPU
- Framebuffer

---

## Gate C — Basic GPU

必须：

- Fill
- Blit
- Golden Exact Match

---

## Gate D — Official Complete

必须：

- Key
- Alpha
- Double Buffer
- CPU/GPU Benchmark

---

## Gate E — Async GPU

必须：

- Command Ring
- Fence
- IRQ
- Fault

---

## Gate F — Core Architecture

必须：

- Tile Binning
- Tile Renderer
- Immediate vs Tile数据对比

---

## Gate G — Competition Complete

必须：

- Main Game
- Benchmark
- X-Ray
- Scaling
- Palette/Additive等目标功能
- 720p60主目标
- 完整性能数据

---

## Gate H — Stretch

才允许：

- Mode-7
- Triangle
- Z
- 1080p60探索

---

# 35. 风险控制原则

## 风险1：DDR/HDMI链路不稳定

措施：

> 尽早Bring-up，不等待GPU完成。

---

## 风险2：Golden与RTL行为不一致

措施：

> Pixel Arithmetic先冻结；所有RTL对Golden。

---

## 风险3：Tile太复杂

措施：

> 先Immediate完整，再做Single-Bank Tile，再优化Dual-Bank。

---

## 风险4：功能太多导致主体未完成

措施：

> P0/P1优先，P3严格在Gate G之后。

---

## 风险5：资源超标

措施：

> 每阶段综合，维护Resource Budget。

---

## 风险6：性能没有预期高

措施：

> Performance Counter + Architecture Model先定位瓶颈，再优化。

---

## 风险7：游戏开发占用GPU时间

措施：

> 游戏只用Graphics API，软件/应用与RTL并行开发。

---

# 36. 当前项目状态

截至本版本：

```text
Requirements V1.0          ✅
System Architecture V1.0   ✅
GPU Command ISA V0.1       ✅

Internal Interface V0.1    ← 下一步
Pixel Arithmetic V0.1
Register Map V0.1
Verification Plan V0.1
Golden GPU V0.1
Board Bring-up
```

---

# 37. 当前推荐立即执行顺序

## Task 1
完成：

> `Internal Interface Specification V0.1`

---

## Task 2
完成：

> `Pixel Format & Arithmetic Specification V0.1`

---

## Task 3
并行启动：

> PC Golden GPU Framework

---

## Task 4
并行启动：

> FPGA Board Bring-up：DDR + HDMI + RISC-V

---

## Task 5
完成：

> Verification Plan V0.1

---

## Task 6
开始：

> Basic GPU：FILL_RECT

---

# 38. 最终开发原则

项目全过程坚持以下规则：

### 规则1
**先规格，后RTL。**

### 规则2
**先Golden，后复杂硬件。**

### 规则3
**先正确，再优化。**

### 规则4
**先官方完整，再核心创新。**

### 规则5
**先Immediate，再Tile。**

### 规则6
**先单Lane，再多Lane。**

### 规则7
**先数据证明瓶颈，再优化架构。**

### 规则8
**每加一个功能，都必须完整回归。**

### 规则9
**任何P3扩展都不能破坏P1主体。**

### 规则10
**最终比赛版本必须是一个可重复构建、可重复验证、可重复Benchmark的Release，而不是“某一次刚好能跑”的工程。**

---

# 39. 项目流程一句话总结

> **本项目按照“需求 → 架构 → ISA → 接口 → 位精确规格 → Golden → 架构建模 → 基础RTL → 官方功能 → Command前端 → Tile架构 → Pixel增强 → 性能/X-Ray → 应用 → 2.5D/3D → 全量回归 → 比赛Release”的顺序实施，并通过多条并行开发线和阶段门禁保证功能正确性、扩展性、性能和项目进度。**
