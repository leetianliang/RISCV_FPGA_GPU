# RISC-V–FPGA 通用 2D GPU 项目需求规格书 V1.0

> 项目定位：面向嵌入式游戏、GUI/HMI 与图形可视化场景的 RISC-V–FPGA 异构 2D 图形加速平台  
> 目标平台：易灵思（Efinix）竞赛平台  
> 文档类型：实现目标规划 / 系统需求规格（Requirements Specification）  
> 版本：V1.0  
> 状态：Baseline Freeze Candidate  
> 日期：2026-09-12

---

## 0. 文档目的

本文档用于冻结项目“要实现什么”，作为后续系统架构、微架构、RTL、软件、验证、Demo 与性能评估的统一目标基线。

本阶段只冻结：

- 产品定位；
- 应用场景；
- 图形功能；
- CPU/FPGA 功能边界；
- 竞赛核心创新方向；
- 性能指标体系；
- 展示方式；
- 验证与交付要求；
- 功能优先级。

本阶段**不冻结**以下微架构参数：

- Tile Size；
- Cache Size / Associativity；
- Pixel Lane 数量；
- Command 固定长度；
- FIFO 深度；
- DMA Burst 长度；
- Alpha / Scaler 精确流水级数；
- 固定点具体位宽；
- 最终 DDR 调度策略。

上述参数须在 Golden Model、性能模型与 FPGA 原型数据基础上确定。

---

# 1. 项目总目标

设计并实现一款面向嵌入式二维图形场景的 **RISC-V–FPGA 异构 2D GPU**。

系统要求：

1. RISC-V 负责游戏/GUI 高层逻辑、场景管理、资源管理和图形命令生成；
2. FPGA 负责底层二维图形命令执行、像素流水处理、DDR 数据搬运与显示相关硬件加速；
3. FPGA 硬件不得绑定某一具体游戏对象或特效；
4. 不重新综合/配置 FPGA 即可运行多个不同应用；
5. 通过 Command Front-End、Tile-Based Memory Architecture、Unified Pixel Pipeline 三条主线提升 GPU 的体系结构竞争力；
6. 通过高密度 Sprite 游戏、GUI/HMI、Benchmark/X-Ray 模式证明通用性、性能和创新点；
7. 以稳定 60FPS 下最大 Sprite 数、CPU 卸载程度、DDR 流量降低、像素吞吐与 FPGA 资源效率为主要量化指标；
8. 预留向 2.5D / 轻量 3D 图形扩展的能力。

---

# 2. 项目定位与边界

## 2.1 产品定位

本项目不是：

- 针对某一游戏的专用 FPGA 游戏机；
- 仅支持 Block Copy 的 BitBlt 外设；
- 为展示某个固定视觉效果而硬编码的图像流水线；
- 通用 PC 级 3D GPU；
- 以软件游戏内容为主、FPGA 仅作为接口外设的项目。

本项目是：

> **可由 RISC-V 软件编程、面向嵌入式 2D 图形工作负载优化的领域专用 GPU / Graphics Accelerator。**

其“通用性”定义为：

> 在 2D 游戏、嵌入式 GUI/HMI、图形 Benchmark 等领域内，通过统一 Graphics API、Command ISA 和 FPGA Bitstream 支持不同应用。

---

## 2.2 FPGA 不得理解游戏语义

FPGA 只理解图形原语与图形状态，例如：

- Texture；
- Rectangle；
- Source / Destination；
- Coordinate；
- Width / Height；
- Alpha；
- Color Key；
- Scale；
- Blend Mode；
- Palette；
- Clip Region；
- Framebuffer；
- Fence。

FPGA 不应出现以下游戏专用逻辑：

- enemy_renderer；
- bullet_generator；
- boss_effect；
- player_renderer；
- 某个固定地图或固定 Logo 的专用状态机。

游戏对象语义全部由 RISC-V 软件解释，并转换为通用 GPU Command。

---

# 3. 需求优先级定义

| 优先级 | 定义 |
|---|---|
| **P0** | 官方基线与项目存活能力，必须完成 |
| **P1** | 冲奖核心功能/架构，必须完成 |
| **P2** | 提升系统完整度、视觉效果和通用性的增强功能 |
| **P3** | 冲刺/研究型扩展，不影响主体项目验收 |

---

# 4. 官方基线需求映射（P0）

> 以下为项目必须覆盖的官方题目核心方向，最终实现应至少达到并完整验证这些能力。

## REQ-OFF-001：RISC-V + FPGA 异构分工

- RISC-V 承担游戏或 GUI 高层逻辑；
- FPGA 承担底层 2D 图形加速；
- CPU 通过系统总线/寄存器/命令机制控制 GPU。

**验收：**
- 上层应用由 RISC-V 运行；
- FPGA 完成实际 framebuffer 图形操作；
- CPU 与 GPU 职责清晰可解释。

---

## REQ-OFF-002：Block Copy / BitBlt

FPGA 应支持二维矩形区域从源图像/纹理到目标 framebuffer 的复制。

至少支持：

- Source Base Address；
- Destination Base Address；
- Width / Height；
- Source / Destination Stride；
- 2D Address Generation。

**验收：**
- 与软件 Golden Model 逐像素一致。

---

## REQ-OFF-003：Solid Fill

FPGA 应支持指定矩形区域的纯色填充。

**验收：**
- 支持任意合法矩形坐标和尺寸；
- 支持边界裁剪或明确的输入约束。

---

## REQ-OFF-004：Burst + FIFO

GPU 与 DDR 之间的图像数据搬运必须使用适合连续访问的 Burst 和 FIFO 解耦机制，不得采用低效率的逐像素同步事务作为最终实现。

**验收：**
- 可测量有效 DDR 吞吐；
- 对比非优化方式说明 Burst/FIFO 收益。

---

## REQ-OFF-005：Double Buffer / Page Flip

至少支持双 framebuffer：

- Front Buffer：Display Scanout；
- Back Buffer：GPU Rendering。

VSYNC 时完成 Page Flip，避免撕裂。

---

## REQ-OFF-006：Color Key

支持基于指定颜色值的透明键控。

---

## REQ-OFF-007：Alpha Blending

至少支持 Global Alpha。

P1 进一步要求支持 Per-Pixel Alpha。

---

## REQ-OFF-008：CPU vs FPGA 性能对比

必须建立 CPU 软件渲染与 FPGA GPU 渲染的公平对比。

至少统计：

- FPS；
- CPU Cycles/Frame；
- 最大稳定 Sprite 数；
- Frame Time。

---

# 5. 应用层需求

## 5.1 APP-01：原创高负载主游戏（P1）

最终必须实现一个原创高密度 2D 游戏作为主视觉 Demo。

推荐形态：

> **俯视“幸存者 + 弹幕”混合型游戏**

主游戏应自然覆盖：

- 大量 Sprite；
- 多敌人；
- 大量弹幕；
- Particle；
- Alpha；
- Additive Blend；
- Scale；
- Animation；
- HUD；
- 多层背景；
- 高 Overdraw 场景。

游戏设计目标：

- 展示 GPU，而不是展示复杂剧情/AI；
- 游戏逻辑简洁可维护；
- 场景负载可程序化调节；
- 可以作为标准 Benchmark Workload。

---

## 5.2 APP-02：GPU Stress Control（P1）

主游戏/Benchmark 必须支持动态调节负载，例如：

- Sprite 数量；
- Enemy 数量；
- Bullet 数量；
- Particle 数量；
- Overdraw 强度；
- Alpha 使用比例；
- Scaling 使用比例。

应支持自动递增测试：

`低负载 → 中负载 → 高负载 → 极限负载`

最终自动得到：

> **Max Active Sprites @ Stable 60 FPS**

---

## 5.3 APP-03：第二应用——Embedded GUI / HMI（P2）

必须尽量实现一个明显不同于主游戏的第二应用，用于证明 GPU 通用性。

建议包含：

- 图标；
- 窗口；
- 半透明菜单；
- 仪表盘；
- 进度条；
- 动态曲线；
- 字体；
- 多图层 UI。

关键要求：

> **无需重新配置 FPGA Bitstream。**

只更换 RISC-V 应用/资源即可运行。

---

## 5.4 APP-04：GPU Benchmark Suite（P1）

独立 Benchmark 至少包括：

### B1 — Sprite Storm
大量普通 Sprite，测试：
- Command Throughput；
- Texture Bandwidth；
- Pixel Throughput。

### B2 — Alpha Storm
大量半透明 Sprite，测试：
- Alpha Pipeline；
- Read-Modify-Write；
- DDR Bandwidth。

### B3 — Overdraw Storm
大量 Sprite 集中重叠，测试：
- Tile-Based Rendering；
- Framebuffer Traffic Reduction。

### B4 — Scaling Storm
大量动态缩放 Sprite，测试：
- Hardware Scaler；
- Memory Access Efficiency。

### B5 — Particle Storm
大量 Additive Particle，测试：
- Pixel Pipeline；
- Blend Throughput。

---

## 5.5 APP-05：X-Ray Architecture View（P1）

必须设计 GPU 可视化调试模式，至少可展示：

- Tile Grid；
- 当前/高负载 Tile；
- Sprite Bounding Box；
- Command Count；
- GPU Utilization；
- DDR Read/Write Bandwidth；
- Tile Load/Store；
- Tile Reuse；
- Cache Hit/Miss（若实现 Cache）；
- Pixel Throughput。

目的：

> 让评委直接“看到 GPU 架构正在工作”。

---

## 5.6 APP-06：CPU / GPU 实时对比（P1）

支持在相同场景、相同资源、相同分辨率条件下切换：

- CPU Software Renderer；
- FPGA GPU Renderer。

画面应实时展示：

- FPS；
- Frame Time；
- CPU Cycles/Frame；
- Sprite Count。

---

## 5.7 APP-07：Architecture Ablation Demo（P1）

至少支持一个关键创新的运行时/可配置对比：

- Immediate Rendering vs Tile Rendering；

推荐进一步支持：

- Cache OFF/ON；
- Nearest/Bilinear；
- CPU Direct Submit / Command Ring（若可运行时切换）。

用于现场展示架构优化前后的性能差异。

---

# 6. 软件层需求

## 6.1 SW-01：Graphics API（P1）

应用层不得直接操作 FPGA 硬件寄存器。

至少提供：

```c
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

推荐：

```c
gpu_wait_fence(...);
gpu_get_stats(...);
gpu_set_palette(...);
```

---

## 6.2 SW-02：GPU Driver（P1）

驱动层负责：

- MMIO；
- Command Buffer；
- Command Ring；
- Fence；
- IRQ；
- Page Flip；
- Buffer Ownership；
- Performance Counter 读取。

---

## 6.3 SW-03：2D Engine Layer（P2）

Graphics API 之上提供轻量 2D Engine：

- Sprite；
- Animation；
- Tile Map；
- Particle；
- Layer；
- Basic Text/UI；
- Resource Handle。

---

## 6.4 SW-04：Resource Manager（P2）

支持：

- Texture Loading；
- Palette Loading；
- Texture Handle；
- DDR Address 管理；
- Texture Format 管理。

素材不得硬编码进 RTL。

---

## 6.5 SW-05：CPU Tile Binning（P1）

在 Tile-Based 模式下，RISC-V 负责将图形命令按 Tile 归类。

CPU 更适合承担：

- Bounding Box；
- Tile Intersection；
- Z/Draw Order；
- Command List Construction。

FPGA 负责 Tile 内规则像素渲染。

---

# 7. GPU Command / ISA 需求

## 7.1 CMD-01：统一命令模型（P1）

GPU 需定义稳定、可扩展的 Command ISA。

基础命令建议包括：

- `NOP`
- `FILL_RECT`
- `BLIT`
- `BLIT_EXT`
- `SET_CLIP`
- `SET_PALETTE`
- `PRESENT`
- `FENCE`

---

## 7.2 CMD-02：可组合 Flags（P1）

`BLIT_EXT` 等命令通过 Flags 动态启用：

- Color Key；
- Global Alpha；
- Per-Pixel Alpha；
- Scale；
- Flip X；
- Flip Y；
- Bilinear；
- Additive Blend；
- Palette；
- Premultiplied Alpha（如实现）。

---

## 7.3 CMD-03：Command Ring（P1）

最终系统必须支持：

> **CPU 与 GPU 异步执行。**

RISC-V 在 DDR 构建 Command Ring：

- Write Pointer：CPU 更新；
- Read Pointer：GPU 更新；
- GPU DMA Fetch Command；
- CPU 不需要每个 Sprite 同步等待。

---

## 7.4 CMD-04：Fence / IRQ（P1）

GPU 应支持：

- Fence ID；
- Command Batch Completion；
- Interrupt；
- 非 Busy-Wait 同步。

---

## 7.5 CMD-05：ISA 扩展空间（P2）

Opcode 编码需保留扩展空间，为：

- Vector Primitive；
- 2.5D；
- Experimental Triangle Rasterizer；

预留兼容空间。

---

# 8. GPU Memory System 需求

## 8.1 MEM-01：2D Strided DMA（P1）

GPU DMA 必须原生支持：

- Source Base；
- Destination Base；
- Width；
- Height；
- Source Stride；
- Destination Stride。

CPU 不应逐行提交二维搬运。

---

## 8.2 MEM-02：Burst Transfer（P0）

对连续像素访问进行 Burst。

要求：

- Read FIFO；
- Write FIFO；
- Burst Length 管理；
- DDR Stall 统计。

---

## 8.3 MEM-03：Tile-Based Rendering（P1 / 核心创新）

项目正式目标包含：

> **CPU Software Binning + FPGA Tile Renderer / Tile Buffer**

目的：

- 减少高 Overdraw 场景下 framebuffer 重复 DDR Read-Modify-Write；
- 将多个 Sprite 对同一局部区域的合成尽量留在片上完成；
- 提升 DDR 有效利用率。

必须支持：

- Tile Load；
- Tile Local Rendering；
- Tile Store；
- Tile Statistics。

具体 Tile Size 在架构模型后确定。

---

## 8.4 MEM-04：Tile Mode 与 Immediate Mode 对比（P1）

系统须能够比较：

- Immediate Rendering；
- Tile-Based Rendering。

至少统计：

- DDR Read Bytes/Frame；
- DDR Write Bytes/Frame；
- Frame Time；
- FPS；
- Tile Reuse。

---

## 8.5 MEM-05：Texture Bandwidth Optimization（P2）

至少实现以下一种或多种：

- Indexed8 Texture；
- Palette；
- Texture Cache；
- Sprite Cache；
- RLE Texture；
- Burst Coalescing。

其中 Texture Cache 是否实现，根据 PC Architecture Model 的收益评估决定。

---

# 9. Rendering Back-End 需求

## 9.1 PIX-01：Unified Pixel Pipeline（P1 / 核心创新）

所有主要像素操作应尽量融合到统一 Pixel Pipeline 中，而不是多次经过 framebuffer。

建议逻辑顺序：

```text
Texture Fetch
    ↓
Palette Lookup
    ↓
Color Key
    ↓
Scale / Sampling
    ↓
Alpha
    ↓
Blend / ROP
    ↓
Format Conversion
    ↓
Write Back
```

未启用 Stage 支持 Bypass。

---

## 9.2 PIX-02：Color Key（P0）

支持可配置透明键值。

---

## 9.3 PIX-03：Global Alpha（P0）

支持整 Sprite 统一 Alpha。

---

## 9.4 PIX-04：Per-Pixel Alpha（P1）

支持带 Alpha Channel 的 Sprite。

用于：

- 抗锯齿边缘；
- Smoke；
- Glow；
- Transparent UI；
- Particle。

---

## 9.5 PIX-05：Blend Mode（P2）

至少支持：

1. Normal Alpha；
2. Additive Blend。

推荐扩展：

- Premultiplied Alpha；
- Multiply / Modulate；
- Saturating Add。

---

## 9.6 PIX-06：Hardware Scaling（P2）

至少支持：

- Nearest Neighbor。

目标支持：

- Bilinear。

Scaler 应支持独立 X/Y Scale。

---

## 9.7 PIX-07：Flip（P2）

支持：

- Flip X；
- Flip Y。

---

## 9.8 PIX-08：Clipping / Scissor（P2）

GPU 应自动处理：

- Sprite 部分出屏；
- Scissor Rect；
- 非法区域丢弃。

CPU 不应为每个超界对象重新生成纹理。

---

## 9.9 PIX-09：Palette / Indexed8（P2）

支持：

- 8-bit Texture Index；
- Palette RAM；
- 软件动态 Palette Update。

目标价值：

- 降低纹理 DDR 带宽；
- 支持低成本换肤/色调变化。

---

## 9.10 PIX-10：Pixel ALU / ROP（P3）

研究型扩展，可支持：

- COPY；
- XOR；
- ADD；
- SUB；
- MIN；
- MAX。

非主体必做。

---

# 10. 显示系统需求

## 10.1 DISP-01：开发分辨率（P0）

开发初期：

> **640×480 @ 60Hz**

用于快速 Bring-up 与功能验证。

---

## 10.2 DISP-02：竞赛目标分辨率（P1）

主 Demo 目标：

> **1280×720 @ 60Hz**

前提：

- DDR 带宽模型支持；
- HDMI/显示链路支持；
- GPU 在目标 workload 下稳定。

---

## 10.3 DISP-03：Stretch 分辨率（P3）

如资源、DDR 与时序均有余量：

> **1920×1080 @ 60Hz**

该目标不得影响 720p60 主体的完成质量。

---

## 10.4 DISP-04：Double Buffer（P0）

支持 Front / Back Buffer。

---

## 10.5 DISP-05：VSYNC Page Flip（P0）

Page Flip 必须与 VSYNC 同步，避免 tearing。

---

## 10.6 DISP-06：OSD / Performance Overlay（P1）

实时显示：

- FPS；
- Frame Time；
- Sprite Count；
- GPU Utilization；
- Pixel Rate；
- DDR BW；
- Command Count；
- Tile Reuse；
- Cache Hit（如有）。

---

# 11. 性能与 KPI 需求

## 11.1 PERF-01：主 KPI

最终最重要指标：

> **Max Active Sprites @ Stable 60 FPS**

必须定义标准测试场景与 Sprite 尺寸/格式/效果比例，避免无意义的“Sprite 数”比较。

---

## 11.2 PERF-02：稳定帧率

主 Demo 目标：

> **Stable 60 FPS**

不只统计平均值。

至少记录：

- Average Frame Time；
- P95 Frame Time；
- Max Frame Time。

目标参考：

\[
T_{frame} \le 16.67ms
\]

---

## 11.3 PERF-03：GPU Front-End

统计：

- Commands/s；
- Commands/Frame；
- Command Fetch Stall；
- CPU Submit Cycles；
- Fence Latency。

---

## 11.4 PERF-04：Pixel Back-End

统计：

- Pixels Rendered；
- Pixels/Clock；
- MPixel/s；
- Blend Pixels；
- Key Discard Pixels。

---

## 11.5 PERF-05：Memory

统计：

- DDR Read Bytes；
- DDR Write Bytes；
- Effective BW；
- Stall Cycles；
- Bytes/Frame；
- Bytes/Rendered Pixel。

---

## 11.6 PERF-06：Tile Architecture

统计：

- Tile Load；
- Tile Store；
- Average Sprites/Tile；
- Tile Reuse；
- DDR Traffic Reduction；
- Frame Time Reduction。

---

## 11.7 PERF-07：CPU Offload

至少比较：

- Software Renderer CPU Cycles/Frame；
- Hardware Renderer CPU Cycles/Frame；
- CPU 卸载比例。

---

## 11.8 PERF-08：FPGA Cost

每个主要版本记录：

- LUT；
- FF；
- BRAM；
- DSP；
- Fmax。

---

## 11.9 PERF-09：Power / Energy（P2）

如工具链允许可靠测量/估计：

- Power；
- Energy/Frame；
- FPS/W。

非项目存活条件，但可作为加分数据。

---

# 12. Hardware Performance Counter 需求

## PERFCTR-01（P1）

GPU 至少应提供：

- `TOTAL_CYCLES`
- `BUSY_CYCLES`
- `COMMAND_COUNT`
- `PIXEL_COUNT`
- `DDR_READ_BYTES`
- `DDR_WRITE_BYTES`
- `DDR_STALL_CYCLES`
- `TILE_LOAD_COUNT`
- `TILE_STORE_COUNT`

如实现 Cache，再增加：

- `CACHE_HIT`
- `CACHE_MISS`

推荐：

- `BLEND_PIXEL_COUNT`
- `KEY_DISCARD_COUNT`
- `SCALE_PIXEL_COUNT`

Performance Counter 必须可由 RISC-V 读取，并可在 OSD/X-Ray 中显示。

---

# 13. 视觉效果需求

主 Demo 至少应体现以下视觉能力：

## VIS-01（P1）
高密度 Sprite 场景。

## VIS-02（P1）
Per-Pixel Alpha 半透明效果。

## VIS-03（P2）
Additive 发光粒子、弹幕、爆炸。

## VIS-04（P2）
动态 Scaling：
- Boss；
- UI；
- Skill；
- Particle。

## VIS-05（P2）
残影/拖尾：
- 多次 Sprite + 不同 Alpha。

## VIS-06（P2）
Parallax 多层背景。

## VIS-07（P2）
Palette Swap：
- Rage；
- Poison；
- Ice；
- Day/Night。

## VIS-08（P1）
X-Ray Tile / Architecture View。

要求：

> 视觉效果必须对应真实 GPU 功能，不以播放预制视频代替实时渲染。

---

# 14. 通用性需求

## GEN-01（P1）
同一 FPGA Bitstream 支持多个应用。

## GEN-02（P1）
应用不直接依赖 GPU RTL 内部实现。

## GEN-03（P1）
素材存于软件可管理资源区，不硬编码在 RTL。

## GEN-04（P2）
不同应用共用：
- Graphics API；
- Command ISA；
- Driver；
- GPU Hardware。

## GEN-05（P2）
至少通过：
- 主游戏；
- Benchmark；
- GUI/HMI；

中的两个以上应用证明通用性，推荐全部完成。

---

# 15. Golden Model 与验证需求

## VER-01：PC Visual Prototype（P0）

在 FPGA 高阶功能实现前，应在 PC 上完成可运行的 2D GPU 软件原型。

用途：

- Demo 验证；
- 视觉设计；
- Graphics API 验证；
- Command ISA 验证。

---

## VER-02：Bit-Accurate Golden Model（P1）

Golden Model 必须逐步做到：

> **Pixel-Exact / Bit-Accurate**

至少覆盖：

- Fill；
- Blit；
- Color Key；
- Alpha；
- Clip；
- Scale；
- Palette；
- Tile Rendering。

---

## VER-03：Architecture Model（P1）

建立架构性能模型评估：

- Pixel Lane；
- DDR BW；
- Tile Size；
- Cache；
- Overdraw；
- Bytes/Pixel。

---

## VER-04：RTL 自动对比（P1）

RTL Testbench 应能够：

输入：
- Framebuffer；
- Texture；
- Command Stream。

输出：
- RTL Framebuffer。

然后与 Golden 输出逐像素比较。

---

## VER-05：Random Command Regression（P1）

自动生成随机：

- Coordinates；
- Size；
- Alpha；
- Clip；
- Stride；
- Scale；
- Edge Cases。

用于验证：

- Address Generator；
- Clipping；
- Pixel Pipeline；
- Command Decoder。

---

## VER-06：Reference Workload（P1）

必须固定若干标准场景和 Random Seed，用作：

- 回归测试；
- 性能比较；
- 论文数据；
- 版本对比。

---

# 16. 2.5D / 3D 扩展需求

## EXT-2D5-01：Affine / Mode-7（P3，优先冲刺）

研究目标：

- Affine Texture Mapping；
- 旋转地面；
- 倾斜纹理；
- Mode-7 风格伪 3D 场景。

要求尽量复用：

- Texture Fetch；
- Scaler / Sampler；
- Pixel Pipeline；
- Command Front-End。

---

## EXT-3D-01：Minimal Triangle Rasterizer（P3）

如主体按计划完成，可探索：

- Flat Color Triangle；
- Edge Function；
- Basic Rasterization。

进一步可选：

- Gouraud Shading；
- Z Buffer；
- Affine Texture Mapping。

不得影响 2D 主体完成度。

---

## EXT-3D-02：架构复用

3D 扩展原则：

> 不另起一套完全独立 GPU。

应尽量形成：

```text
2D Fragment Front-End ─┐
                       ├── Shared Pixel / Blend / Tile Back-End
3D Raster Front-End ───┘
```

---

# 17. 非目标 / 明确不做

V1.0 阶段明确不将以下内容列为主体目标：

- 通用 OpenGL；
- Vertex Shader；
- Fragment Shader；
- Programmable Shader ISA；
- 完整 3D Pipeline；
- Perspective-Correct Texturing 作为必须项；
- 复杂物理引擎；
- SLAM；
- AI；
- 网络多人游戏；
- 任意角度旋转作为 P1；
- 复杂 RPG/剧情系统；
- 为单个特效硬编码 FPGA 逻辑。

---

# 18. 推荐功能优先级总表

| 功能 | 优先级 |
|---|---:|
| Fill | P0 |
| Block Copy | P0 |
| Burst / FIFO | P0 |
| 2D Strided DMA | P1 |
| Double Buffer | P0 |
| VSYNC Page Flip | P0 |
| Color Key | P0 |
| Global Alpha | P0 |
| Per-Pixel Alpha | P1 |
| Command ISA | P1 |
| Command Ring | P1 |
| Fence / IRQ | P1 |
| Unified Pixel Pipeline | P1 |
| Tile-Based Rendering | P1 |
| Hardware Performance Counter | P1 |
| CPU/GPU Realtime Compare | P1 |
| Benchmark Suite | P1 |
| X-Ray View | P1 |
| Hardware Scaling | P2 |
| Bilinear | P2 |
| Additive Blend | P2 |
| Flip | P2 |
| Clipping / Scissor | P2 |
| Indexed8 / Palette | P2 |
| GUI/HMI Demo | P2 |
| Texture Cache | P3* |
| RLE Texture | P3 |
| Pixel ROP | P3 |
| Mode-7 / Affine | P3 |
| Triangle Rasterizer | P3 |

\* Texture Cache 是否升级到 P2，待 PC Cache Simulation 后决定。

---

# 19. 竞赛核心创新定义

最终答辩核心创新控制在三条主线，不以“功能数量”作为主要创新依据。

## Innovation 1：Command-Driven Asynchronous Front-End

解决：

> 千级 Sprite 场景下 CPU 提交开销。

核心：

- Command ISA；
- Command Ring；
- DMA Fetch；
- Fence；
- IRQ。

证明指标：

- Commands/s；
- CPU Cycles/Frame；
- Submit Overhead；
- GPU/CPU Overlap。

---

## Innovation 2：Tile-Based Memory Architecture

解决：

> 高 Overdraw 场景下 framebuffer DDR Read-Modify-Write 流量过高。

核心：

- CPU Binning；
- Tile Buffer；
- Tile Local Rendering；
- Tile Writeback。

证明指标：

- DDR Bytes/Frame；
- Tile Reuse；
- Frame Time；
- FPS；
- Max Sprites @ 60FPS。

---

## Innovation 3：Unified Programmable Pixel Pipeline

解决：

> Color Key、Scale、Alpha、Blend 等像素操作串行多次读写 DDR 的问题。

核心：

- Stage Fusion；
- Configurable Flags；
- Pipeline；
- Multi-Operation Single Pass。

证明指标：

- Pixels/Clock；
- MPixel/s；
- DDR Traffic；
- Resource Efficiency。

---

# 20. 性能目标分层

## Baseline

- 640×480@60 显示链路稳定；
- Fill/Blit/ColorKey/Alpha 正确；
- CPU/GPU 可对比；
- Double Buffer 正常。

## Competition Target

- 1280×720@60 主显示；
- 高密度 Sprite 主游戏稳定；
- Command Ring；
- Tile Renderer；
- Unified Pixel Pipeline；
- Performance Counter；
- Game / Benchmark / X-Ray 完整；
- CPU/GPU 性能差异明确；
- Tile 架构能够以数据证明 DDR 流量下降。

## Stretch

- 1080p60；
- Texture Cache；
- RLE；
- Mode-7；
- Triangle Rasterizer；
- 更高 Pixel Lane；
- Power/Energy 优化。

---

# 21. “Stable 60 FPS”定义

不得只报告平均 FPS。

主 Benchmark 必须至少记录：

- Mean FPS；
- Mean Frame Time；
- P95 Frame Time；
- Worst Frame Time。

并明确 workload：

- 分辨率；
- Sprite 尺寸；
- Sprite 格式；
- Alpha 比例；
- Overdraw；
- Blend；
- Scale。

这样“最大 Sprite 数”才具有可复现性。

---

# 22. 现场展示需求

最终演示建议包含三种核心模式：

## 22.1 GAME MODE

目标：

> “看起来炫。”

展示：

- 主游戏；
- 弹幕；
- 粒子；
- Alpha；
- Scaling；
- Parallax；
- Boss 高负载场景。

---

## 22.2 BENCHMARK MODE

目标：

> “证明真的快。”

展示：

- Sprite 自动递增；
- CPU vs GPU；
- Immediate vs Tile；
- Frame Time；
- DDR BW；
- Max Sprite @ 60FPS。

---

## 22.3 X-RAY MODE

目标：

> “解释为什么快。”

展示：

- Tile；
- Command；
- DDR；
- GPU Util；
- Tile Reuse；
- Cache（如有）；
- Pixel Rate。

---

# 23. 项目最终启动菜单建议

```text
RISC-V × FPGA 2D GPU PLATFORM
--------------------------------
1. High-Density Game
2. Embedded GUI / HMI
3. GPU Benchmark
4. Architecture X-Ray
5. 2.5D Tech Demo        [Stretch]
```

要求：

> 1～4 使用同一 FPGA Bitstream。

---

# 24. 版本演进与数据留存

每个主要 GPU 版本均应记录：

| Version | Features | Fmax | LUT | FF | BRAM | DSP | FPS | DDR BW | Sprite@60 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| V0.x | Basic Fill | | | | | | | | |
| V0.x | Blit | | | | | | | | |
| V0.x | Alpha | | | | | | | | |
| V1.x | Command Ring | | | | | | | | |
| V1.x | Tile Renderer | | | | | | | | |
| V1.x | Scaling/Blend | | | | | | | | |

项目必须保留：

- RTL 版本；
- Fmax；
- Resource；
- Benchmark；
- Golden 结果；
- 波形；
- 屏幕录像；
- 性能数据。

用于最终论文/答辩构建完整演进证据链。

---

# 25. 当前尚未冻结的关键设计决策

以下事项进入 Architecture Exploration，而不是 Requirements Freeze：

## OPEN-01：Framebuffer Format

候选：

- RGB565；
- ARGB8888。

初步倾向：

- Framebuffer：RGB565；
- Texture：RGB565 / ARGB8888 / Indexed8。

最终通过：

- DDR BW；
- 视觉质量；
- Alpha 精度；
- FPGA资源；

综合决定。

---

## OPEN-02：Tile Size

候选：

- 8×8；
- 16×16；
- 32×32；
- 64×64。

必须通过 PC Architecture Model 扫描：

- BRAM；
- DDR Traffic；
- Binning Overhead；
- Overdraw；
- FPS。

之后冻结。

---

## OPEN-03：Pixel Lane

候选：

- 1 pixel/clk；
- 2 pixels/clk；
- 4 pixels/clk。

需结合：

- Fmax；
- DDR BW；
- Pixel Pipeline；
- BRAM带宽；

决定。

---

## OPEN-04：Command Size

候选：

- 32 Byte；
- 64 Byte；
- Variable Length。

由：

- ISA；
- DMA；
- Alignment；
- Future Extension；

共同决定。

---

## OPEN-05：Texture Cache

是否加入正式实现，取决于：

- Sprite 重用；
- Hit Rate；
- BRAM开销；
- DDR收益。

先建模，后决策。

---

# 26. V1.0 验收结论

如果最终项目能够完成以下集合，即视为达到本需求文档定义的“完整冲奖版 2D GPU”：

- [ ] RISC-V 上运行主游戏/GUI逻辑；
- [ ] FPGA 运行通用 2D GPU；
- [ ] Fill / Blit / Color Key / Alpha；
- [ ] Burst / FIFO / 2D DMA；
- [ ] Double Buffer / VSYNC；
- [ ] Graphics API；
- [ ] GPU Command ISA；
- [ ] Command Ring；
- [ ] Fence / IRQ；
- [ ] Per-Pixel Alpha；
- [ ] Unified Pixel Pipeline；
- [ ] CPU Tile Binning；
- [ ] FPGA Tile Renderer；
- [ ] Hardware Performance Counter；
- [ ] 高密度原创游戏；
- [ ] Benchmark Suite；
- [ ] CPU / GPU 实时对比；
- [ ] Immediate / Tile 实时或可复现实验对比；
- [ ] X-Ray Architecture View；
- [ ] PC Golden Model；
- [ ] RTL Pixel-Exact Verification；
- [ ] 完整资源/Fmax/DDR/FPS/CPU性能数据；
- [ ] 同一 FPGA Bitstream 运行至少两个不同应用/模式；
- [ ] 720p60 作为主竞赛显示目标（经带宽/时序验证后正式锁定）。

P2/P3 功能根据时间和资源逐步叠加。

---

# 27. 最终项目一句话定义

> **本项目设计一款面向嵌入式游戏与 GUI 的 RISC-V–FPGA 异构 2D GPU，通过异步命令前端、面向高 Overdraw 的 Tile-Based 存储体系和融合式像素流水线，将 CPU 从大规模像素搬运和混合计算中释放出来，并以高密度 Sprite 实时游戏、GUI/HMI、Benchmark 与 X-Ray 架构可视化验证其通用性、性能与可扩展性。**

---

# 28. 下一阶段输入

在 V1.0 需求冻结后，下一阶段应输出：

1. **System Architecture V1.0**
2. **GPU Command ISA V0.1**
3. **Memory Map V0.1**
4. **Pixel Format Specification V0.1**
5. **Golden Model Specification V0.1**
6. **Benchmark Specification V0.1**
7. **PC Architecture Exploration Plan**
8. **FPGA Bring-up Plan**

下一阶段原则：

> **先用建模数据决定微架构，再进入大规模 RTL 开发。**
