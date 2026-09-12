# RISC-V–FPGA 通用 2D GPU
# System Architecture Specification V1.0

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：系统总体架构规格（System Architecture Specification）  
> 版本：V1.0  
> 日期：2026-09-12  
> 上游文档：`RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> 状态：Architecture Baseline Freeze Candidate

---

# 0. 文档目的

本文档在需求规格 V1.0 基础上，确定系统级架构、模块边界、核心接口、数据流、扩展路径和关键架构决策。

本架构遵循以下原则：

> **按“功能全集”设计架构，按项目阶段裁剪实现硬件。**

即：

- 当前没有实现的扩展功能，也必须在总体数据流、命令空间、模块边界和接口语义上有明确位置；
- 后续增加 Scaling、Palette、Texture Cache、Affine/Mode-7、Triangle Rasterizer、Z-Buffer 等功能时，应尽量通过新增/替换局部模块完成；
- 不允许因为增加一个规划内功能而推翻 Command、Pixel、Memory 或 Render Target 等基础接口；
- 未实现的功能通过 Capability Bits 明确暴露为“不支持”，而不是改变整个软件/硬件协议。

本架构是项目未来 RTL、软件、Golden Model、验证平台和性能模型的共同依据。

---

# 1. 架构范围与设计目标

## 1.1 系统最终形态

系统目标不是“固定游戏专用 FPGA 电路”，而是：

> **面向嵌入式游戏、GUI/HMI 和图形可视化场景的 RISC-V–FPGA 异构 2D GPU 平台。**

系统应支持：

- 主游戏；
- GUI/HMI；
- GPU Benchmark；
- Architecture X-Ray；
- 2.5D / 轻量 3D 扩展；

在同一套 FPGA Bitstream 与统一 Graphics API / Command ISA 下运行。

## 1.2 官方功能在架构中的位置

官方要求的核心功能均作为本架构的基础子集：

- Block Copy / BitBlt；
- Solid Fill；
- Burst Transfer；
- FIFO；
- Double Buffer；
- Color Key；
- Alpha Blending；
- CPU / FPGA FPS 对比；
- 高密度 Sprite 场景。

项目自主扩展：

- Command Ring；
- Fence / IRQ；
- Tile-Based Rendering；
- Unified Pixel Pipeline；
- Per-Pixel Alpha；
- Hardware Scaling；
- Bilinear；
- Palette / Indexed8；
- Additive Blend；
- Texture Cache；
- 2.5D Affine / Mode-7；
- Triangle Rasterizer；
- Z-Buffer；
- 多应用软件平台。

---

# 2. 架构设计原则

## ARCH-P-01：应用与 GPU 解耦

FPGA GPU 不理解 Player、Enemy、Bullet、Boss、Skill、GUI Button，只理解：

- Draw Command；
- Surface；
- Texture；
- Rectangle；
- Coordinate；
- Alpha；
- Blend；
- Tile；
- Fragment；
- Render Target。

## ARCH-P-02：控制平面与数据平面分离

CPU/GPU 通信分成：

### Control Plane
通过 MMIO：

- Reset；
- Status；
- Capability；
- Command Ring Pointer；
- Interrupt；
- Fault；
- Performance Counter。

### Data Plane
通过 DDR：

- Command Ring；
- Draw Descriptor Array；
- Tile Work List；
- Texture；
- Framebuffer；
- Optional Depth；
- Optional Extended Descriptor。

## ARCH-P-03：多前端，共享后端

不同图形前端：

- 2D Rectangle/Sprite；
- Affine / Mode-7；
- Vector Primitive；
- Triangle Rasterizer；

统一生成标准 Fragment Stream，并共享：

- Texture Unit；
- Pixel Back-End；
- Blend；
- Tile Buffer；
- Render Target；
- Memory Service。

## ARCH-P-04：所有规划功能必须有稳定扩展点

可选模块必须满足：

- 有固定插入位置；
- 有 Bypass / Null Adapter；
- Feature关闭时不改变上下游接口；
- 软件通过 `CAPS` 判断功能是否存在。

## ARCH-P-05：内部统一数据格式

外部纹理和 framebuffer 可以使用不同格式，但进入 Pixel Back-End 后统一使用：

> **Canonical RGBA8888**

## ARCH-P-06：内存访问集中管理

Command、Texture、Tile、Framebuffer、Display、Depth 等客户端统一进入：

> **GPU Memory Service**

由其负责 Arbitration、Burst、FIFO、Alignment、QoS、Backpressure、Statistics。

## ARCH-P-07：验证优先

所有模块接口必须能被 PC Golden Model / RTL Testbench 独立驱动与观测。

---

# 3. 系统总体架构

```text
┌─────────────────────────────────────────────────────────────┐
│                     RISC-V SOFTWARE                         │
│                                                             │
│ Game / GUI / Benchmark / X-Ray / 2.5D Demo                 │
│                           │                                 │
│                    2D Engine / SDK                          │
│                           │                                 │
│            Graphics API + Resource Manager                  │
│                           │                                 │
│             GPU Driver + Software Tile Binner               │
└───────────────────────────┬─────────────────────────────────┘
                            │
                MMIO        │       DDR Data Structures
                  │         │
                  │         ├── Command Ring
                  │         ├── Draw Descriptor Array
                  │         ├── Tile Work Lists
                  │         ├── Texture / Palette
                  │         └── Framebuffer
                  │
                  ▼
┌─────────────────────────────────────────────────────────────┐
│                     GPU CONTROL PLANE                       │
│                                                             │
│  MMIO Register File                                         │
│    ├─ ID / VERSION / CAPS                                   │
│    ├─ CONTROL / STATUS                                      │
│    ├─ RING BASE / HEAD / TAIL                               │
│    ├─ IRQ / FENCE                                           │
│    ├─ DISPLAY                                                │
│    ├─ PERF                                                   │
│    └─ FAULT / TRACE                                          │
│                                                             │
│  Command DMA → Command Parser → Dispatcher                  │
└───────────────────────────┬─────────────────────────────────┘
                            │ Work / Draw Context
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                     RENDER FRONT-END                        │
│                                                             │
│  2D Rect/Sprite Front-End ───────────┐                      │
│  Affine/Mode-7 Front-End [EXT] ──────┤                      │
│  Vector Front-End [EXT] ─────────────┼─► Fragment Stream    │
│  Triangle Raster Front-End [EXT] ────┘                      │
└───────────────────────────┬─────────────────────────────────┘
                            │ Fragment + Render Context
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                       TEXTURE UNIT                          │
│                                                             │
│ Texture Address Generator                                   │
│      ↓                                                      │
│ Texture Cache [optional]                                    │
│      ↓                                                      │
│ Compression Adapter [optional]                              │
│      ↓                                                      │
│ Format Decode / Palette                                     │
│      ↓                                                      │
│ Nearest / Bilinear Sampler                                  │
└───────────────────────────┬─────────────────────────────────┘
                            │ Canonical RGBA8888
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                     PIXEL BACK-END                          │
│                                                             │
│ Color Key                                                   │
│    ↓                                                        │
│ Color / Alpha Modifier                                      │
│    ↓                                                        │
│ Depth Test [optional]                                       │
│    ↓                                                        │
│ Destination Fetch                                           │
│    ↓                                                        │
│ Alpha / Additive / ROP                                      │
│    ↓                                                        │
│ Render-Target Format Conversion                             │
└───────────────────────────┬─────────────────────────────────┘
                            │ Pixel Write
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                 RENDER TARGET SUBSYSTEM                     │
│                                                             │
│                  ┌─ Immediate Adapter ──► DDR                │
│ Pixel Back-End ──┤                                          │
│                  └─ Tile Adapter ───────► Tile Buffer       │
│                                              │              │
│                                     Color Plane RGBA8888    │
│                                     Depth Plane [optional]  │
│                                              │              │
│                                         Tile Store          │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      MEMORY SERVICE                         │
│                                                             │
│ CMD │ TEX │ RT/TILE │ DEPTH │ VERTEX │ DISPLAY             │
│                         │                                   │
│        Arbiter / Burst Engine / FIFO / QoS                  │
│                         │                                   │
│                 Platform Memory Adapter                     │
│                         │                                   │
│                        DDR                                  │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      DISPLAY ENGINE                         │
│                                                             │
│ Framebuffer Scanout ─────┐                                  │
│ OSD Plane ────────────────┼─► Display Compositor            │
│ Cursor Plane [optional] ──┘          │                       │
│                                     ▼                       │
│                              RGB / Timing                   │
│                                     │                       │
│                                    HDMI                     │
└─────────────────────────────────────────────────────────────┘
```

---

# 4. 软件架构

```text
Application
   │
   ├─ High-Density Game
   ├─ Embedded GUI/HMI
   ├─ Benchmark
   └─ 2.5D Tech Demo
   │
   ▼
2D Engine
   │
   ├─ Sprite
   ├─ Animation
   ├─ Particle
   ├─ Tile Map
   └─ Basic GUI
   │
   ▼
Graphics API
   │
   ▼
GPU Driver
   │
   ├─ Resource Manager
   ├─ Command Builder
   ├─ Tile Binner
   ├─ Ring Manager
   ├─ Fence/IRQ
   └─ Perf Reader
```

## 4.1 Application Layer

职责：

- 游戏逻辑；
- GUI逻辑；
- 动画状态；
- Enemy / Bullet / Particle 更新；
- 场景构建。

不得直接写 GPU 寄存器，也不得知道 Tile Buffer / DMA / RTL 流水级。

## 4.2 2D Engine

提供：

- Sprite；
- Animation；
- Layer；
- Particle；
- Tile Map；
- Text/UI Primitive。

## 4.3 Graphics API

统一接口：

```c
gpu_begin_frame();
gpu_fill_rect();
gpu_draw_sprite();
gpu_draw_sprite_ex();
gpu_set_clip();
gpu_set_palette();
gpu_present();
gpu_wait_fence();
gpu_get_stats();
```

`gpu_draw_sprite_ex()` 支持：

- Color Key；
- Global Alpha；
- Per-Pixel Alpha；
- Scale；
- Flip；
- Filter；
- Blend；
- Palette。

## 4.4 GPU Driver

负责：

- Command Encoding；
- Ring Buffer；
- MMIO；
- Fence；
- Interrupt；
- Page Flip；
- Fault Recovery；
- Performance Counter。

## 4.5 Software Tile Binner

Tile Rendering 模式下：

1. 按 Draw Order 接收所有 Draw Command；
2. 计算 Draw Bounding Box；
3. 求与 Tile Grid 的交集；
4. 将 Draw Index 按原始顺序追加到对应 Tile Work List；
5. 提交 `TILE_FRAME` Command。

保证每个 Tile 内 Draw 顺序与全局提交顺序一致。

---

# 5. Hardware Profile 与 Capability

## 5.1 PROFILE_BASE

- Fill；
- Blit；
- Burst/FIFO；
- Color Key；
- Global Alpha；
- Double Buffer；
- Performance Basic Counter。

## 5.2 PROFILE_COMPETITION

最终竞赛版本：

- Command Ring；
- Fence / IRQ；
- Per-Pixel Alpha；
- Tile-Based Rendering；
- Unified Pixel Pipeline；
- Scaling；
- Additive Blend；
- Indexed8 / Palette；
- X-Ray Counters；
- 720p60。

## 5.3 PROFILE_EXTENDED

- Texture Cache；
- RLE/Compression Adapter；
- Affine / Mode-7；
- Triangle Rasterizer；
- Z-Buffer；
- Advanced ROP；
- 1080p60 Exploration。

## 5.4 Capability Bits

```text
CAP_FILL
CAP_BLIT
CAP_COLOR_KEY
CAP_GLOBAL_ALPHA
CAP_PIXEL_ALPHA
CAP_TILE
CAP_SCALE
CAP_BILINEAR
CAP_INDEXED8
CAP_ADDITIVE
CAP_TEX_CACHE
CAP_RLE
CAP_AFFINE
CAP_TRIANGLE
CAP_ZBUFFER
CAP_MULTIPLANE
```

---

# 6. CPU / GPU 通信架构

## 6.1 Control Plane

采用 MMIO。

外部优先适配：

> **AXI4-Lite / AXI-Lite 风格 Control Bus**

如果实际平台使用 Wishbone，则仅替换 Control Bus Adapter。

## 6.2 Data Plane

大量数据均放在 DDR：

- Commands；
- Draw Descriptors；
- Work Lists；
- Texture；
- Framebuffer；
- Palette；
- Optional Depth；
- Optional Vertex Data。

---

# 7. Command Architecture

## 7.1 Base Command

V1.0 决定：

> **Base Command 固定 64 Byte。**

理由：

- Command 带宽远小于像素/纹理带宽；
- 64B 对齐友好；
- Command Fetch、Parser、Ring Wrap 简单；
- 预留空间充足。

## 7.2 Command Address Width

V1.0 决定：

> **32-bit Physical Address**

满足当前平台 DDR 地址空间，同时节省 Command 字段。

## 7.3 Command Ring

- Entry = 64B；
- Entry Count = 2^N；
- Ring Base 64B 对齐；
- CPU = Producer；
- GPU = Consumer。

寄存器：

```text
CMD_RING_BASE
CMD_RING_SIZE
CMD_HEAD
CMD_TAIL
```

提交流程：

```text
CPU写Command
   ↓
Memory Barrier
   ↓
更新TAIL
   ↓
GPU Command DMA读取
   ↓
执行
   ↓
更新HEAD
```

## 7.4 Command Header

精确位域在 Command ISA 文档冻结，但必须包含：

- Opcode Class；
- Opcode；
- Version；
- Flags；
- Length / Type；
- Sequence / Fence Tag；
- Reserved。

## 7.5 Opcode Namespace

| 范围 | 用途 |
|---|---|
| `0x0x` | Control / Sync |
| `0x1x` | 2D Draw |
| `0x2x` | Surface / State |
| `0x3x` | Affine / 2.5D |
| `0x4x` | Vector |
| `0x5x` | Experimental 3D |
| `0xEx` | Performance / Debug |
| `0xFx` | Fence / System |

## 7.6 Immediate Draw Mode

Ring 直接包含：

```text
FILL_RECT
BLIT
BLIT_EXT
```

用于 Bring-up、Baseline、Debug、Golden Validation。

## 7.7 Tile Draw Mode

使用：

### Draw Descriptor Array
所有 Draw Command 以 64B Descriptor 形式顺序存储。

### Tile Work List
每个 Tile 保存：

```text
DrawIndex0
DrawIndex1
DrawIndex2
...
```

WorkRef：

> **32-bit Draw Descriptor Index**

### Tile Header

逻辑字段：

```text
work_offset
work_count
tile_flags
reserved
```

### Tile Frame Command

```text
TILE_FRAME
draw_desc_base
tile_header_base
work_list_base
tile_grid_w
tile_grid_h
render_target
```

---

# 8. Draw / Render Context

Draw Descriptor 被解析为统一 Render Context：

```text
Source Base
Source Width/Height/Stride
Source Format

Destination Surface
Destination Rect

Texture Rect

Color Key
Global Alpha
Blend Mode

Filter Mode
Flip
Clip Rect

Palette Pointer

UV Transform / Extension Pointer
Feature Flags
```

Render Context 在一个 Draw 执行期间保持稳定，Fragment 不重复携带完整状态。

---

# 9. Render Front-End

## 9.1 2D Rect / Sprite Front-End

### Fill
产生 Screen X/Y + Constant RGBA。

### Blit

\[
u=x_{src}+x
\]

\[
v=y_{src}+y
\]

### Scale

\[
u=u_0+x\Delta u
\]

\[
v=v_0+y\Delta v
\]

### Flip

通过负方向步进实现。

## 9.2 Affine / Mode-7 Front-End

扩展模块正式保留：

\[
u=ax+by+c
\]

\[
v=dx+ey+f
\]

输出标准 Fragment Interface。

## 9.3 Vector Front-End

P3预留。

## 9.4 Triangle Raster Front-End

P3预留，输出：

- X/Y；
- U/V；
- Color；
- Z；
- Coverage。

---

# 10. Fragment Interface

## 10.1 单 Lane 逻辑字段

```text
valid

x : signed 16-bit
y : signed 16-bit

u : signed Q16.16
v : signed Q16.16

vertex_color : RGBA8888

z : 32-bit        [optional]
coverage : 8-bit  [optional]

lane_mask
```

## 10.2 UV 格式

V1.0 决定：

> **U/V = signed Q16.16**

## 10.3 Lane 扩展

接口定义：

> **FragmentVec<LANES>**

支持编译时：

- 1；
- 2；
- 4 lane。

Bring-up 默认：

> `LANES = 1`

最终 Competition 值由 DDR、Fmax 与 BRAM Banking 模型决定。

---

# 11. Texture Subsystem

## 11.1 独立 Texture Unit

所有图形前端统一访问 Texture Unit。

## 11.2 纹理格式

- RGB565；
- ARGB8888；
- Indexed8；
- Reserved。

## 11.3 Canonical Internal Color

V1.0 决定：

> **RGBA8888**

逻辑布局：

```text
A[31:24]
R[23:16]
G[15:8]
B[7:0]
```

Little-Endian 存储。

RGB565：

```text
R[15:11]
G[10:5]
B[4:0]
```

转换到 RGBA8888 时 A=255。

## 11.4 Palette

Indexed8：

```text
Index8 → Palette[256] → RGBA8888
```

Palette Entry = RGBA8888。

## 11.5 Sampler

架构支持：

- Nearest；
- Bilinear。

第一阶段可只实现 Nearest。

## 11.6 Address Mode

预留：

- Clamp；
- Repeat。

主体优先 Clamp。

## 11.7 Texture Cache

固定插入点：

```text
Texture Request
      ↓
Texture Cache
      ↓ miss
Memory Service
```

未实现时使用 Null Cache Adapter。

## 11.8 Compression Adapter

压缩属于 Storage Adapter，不属于 Sampler。

```text
Compressed Texture
        ↓
Storage Adapter
        ↓
Logical Texel Space
        ↓
Sampler
```

随机访问能力由具体压缩格式 Capability 限定。

---

# 12. Pixel Back-End

## 12.1 固定逻辑顺序

```text
Fragment
   ↓
Texture / Constant Color
   ↓
Palette / Format Decode
   ↓
Color Key
   ↓
Color / Alpha Modifier
   ↓
Depth Test [optional]
   ↓
Destination Fetch
   ↓
Blend / ROP
   ↓
Render Target Format Convert
   ↓
Write
```

## 12.2 Color Key

比较 Canonical RGB/RGBA，命中则 Discard。

## 12.3 Color / Alpha Modifier

支持：

- Global Alpha；
- Constant/Vertex Color Modulate；
- Future Tint。

## 12.4 Depth Test

扩展点：

- LESS；
- LEQUAL；
- ALWAYS。

未启用时 Bypass。

## 12.5 Destination Fetch

通过 Render Target Read 抽象访问现有目标像素。

## 12.6 Blend Unit

架构全集：

- Copy；
- Straight Alpha；
- Premultiplied Alpha；
- Additive；
- Multiply / Modulate；
- Basic ROP。

竞赛目标至少：

- Copy；
- Straight Alpha；
- Additive。

## 12.7 Alpha

内部 8-bit：

\[
0\ldots255
\]

舍入规则后续由 Pixel Arithmetic Specification 冻结。

---

# 13. Render Target Subsystem

## 13.1 两种 Adapter

```text
                 ┌─ Immediate Adapter ─► DDR
Pixel Back-End ──┤
                 └─ Tile Adapter ─────► Tile Buffer
```

## 13.2 Immediate Mode

直接对 Framebuffer Read/Modify/Write。

用途：

- Bring-up；
- Baseline；
- Debug；
- 消融实验。

## 13.3 Tile Mode

```text
Tile Select
   ↓
Tile Load
   ↓
Render Work List
   ↓
Tile Store
```

---

# 14. Tile Architecture

## 14.1 默认 Tile Size

V1.0 决定默认：

> **32 × 32 pixels**

同时参数化支持：

- 16×16；
- 32×32；
- 64×64。

如果后续 PC Architecture Model 显示明显更优，可在 V1.1 修改默认值。

## 14.2 Tile Color Format

V1.0 决定：

> **RGBA8888**

即使 framebuffer 为 RGB565，也在 Tile 内保持 RGBA8888，最终 Store 时一次量化。

## 14.3 Tile Buffer Banking

架构按：

> **Dual-Bank Tile Buffer**

设计接口。

目标：

- Bank A Rendering；
- Bank B Load/Store。

Bring-up 可先只启用一 Bank。

## 14.4 Plane

```text
Tile Storage
│
├─ Color Plane : RGBA8888
├─ Depth Plane : Z32 [optional]
└─ Aux Plane   : reserved
```

## 14.5 Depth

V1.0 预留：

> **32-bit Depth Storage**

实际有效位宽后续可选择 Z24 + Reserved。

## 14.6 Fast Clear

架构预留：

- CLEAR_TILE；
- DONT_LOAD。

---

# 15. Framebuffer 与 Surface

## 15.1 主 Framebuffer Format

V1.0 决定：

> **RGB565**

原因：

- DDR 带宽低；
- 720p60 更稳；
- 2D游戏/GUI足够；
- Tile内部仍保持 RGBA8888。

## 15.2 Texture Format

- RGB565；
- ARGB8888；
- Indexed8。

## 15.3 Surface Layout

第一版：

> **Linear Scanline**

Descriptor：

```text
base
width
height
stride
format
```

未来新增 Layout Type 时不改变上层接口。

---

# 16. GPU Memory Service

## 16.1 Logical Clients

```text
MEM_CLIENT_CMD
MEM_CLIENT_TEXTURE
MEM_CLIENT_RENDER_TARGET
MEM_CLIENT_TILE
MEM_CLIENT_DEPTH
MEM_CLIENT_VERTEX
MEM_CLIENT_DISPLAY
MEM_CLIENT_DEBUG
```

## 16.2 Internal Memory Interface

统一 valid/ready，支持：

- Read Burst；
- Write Burst；
- Client ID；
- Address；
- Length；
- Byte Enable；
- Tag；
- Response。

## 16.3 Platform Adapter

V1.0 推荐：

> **AXI4 Full**

作为高带宽 DDR 适配协议。

如果平台实际使用其他接口，只替换 Platform Memory Adapter。

## 16.4 QoS 原则

### Highest
Display Scanout

### High
Tile Store / Critical Render Target

### Medium
Texture Read

### Medium-Low
Command Read

### Low
Debug / Trace

具体算法后续决定。

## 16.5 Burst Engine

统一处理：

- Alignment；
- Split；
- Boundary；
- Coalescing；
- FIFO；
- Backpressure。

---

# 17. Display Architecture

## 17.1 Render 与 Display 解耦

```text
GPU → DDR Framebuffer → Display Scanout → HDMI
```

## 17.2 Double Buffer

至少 A/B 两个 framebuffer。

## 17.3 Present

`PRESENT`：

- 标记 Back Ready；
- Fence；
- VSYNC 时 Page Flip。

## 17.4 Display Compositor

```text
Base Framebuffer
      │
OSD Plane
      │
Cursor Plane [optional]
      │
      ▼
Display Compositor
```

## 17.5 OSD

独立于游戏主 framebuffer。

实现方式暂不冻结：

- BRAM Bitmap；
- Character Generator；
- DDR Overlay。

---

# 18. Performance / Debug Architecture

## 18.1 Central Event Bus

模块统一向 Central Performance Monitor 输出事件。

## 18.2 必须统计

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
```

Cache 实现后：

```text
CACHE_HIT
CACHE_MISS
```

## 18.3 GPU Utilization

\[
GPU\ Util=
rac{BUSY\_CYCLES}{TOTAL\_CYCLES}
\]

## 18.4 Trace

预留 Small Trace Buffer：

- 256；
- 512；
- 1024 events。

记录：

- Command Fetch；
- Dispatch；
- Tile Start/End；
- Memory Stall；
- Fault；
- Fence。

---

# 19. Fault Architecture

寄存器：

```text
FAULT_STATUS
FAULT_CODE
FAULT_ADDRESS
FAULT_COMMAND_SEQ
FAULT_INFO
```

Fault：

```text
UNSUPPORTED_OPCODE
UNSUPPORTED_FEATURE
BAD_ALIGNMENT
BAD_SURFACE
BAD_TEXTURE_FORMAT
RING_OVERFLOW
WORKLIST_OVERFLOW
MEMORY_ERROR
DISPLAY_UNDERFLOW
INTERNAL_TIMEOUT
```

严重 Fault 时 GPU Halt，但 Display 尽量继续显示旧 Front Buffer。

---

# 20. MMIO Register Space

GPU Base Address 由平台集成决定。

| Offset | 区域 |
|---:|---|
| `0x0000-0x00FF` | ID / Version / Capability |
| `0x0100-0x01FF` | Control / Status |
| `0x0200-0x02FF` | Command Ring |
| `0x0300-0x03FF` | Fence / IRQ |
| `0x0400-0x04FF` | Display / Page Flip |
| `0x0500-0x05FF` | Performance |
| `0x0600-0x06FF` | Fault / Debug |
| `0x0700-0x07FF` | Trace |
| `0x0800-0x08FF` | Palette / Auxiliary |
| `0x0900+` | Reserved |

精确寄存器后续进入 `GPU_Register_Map_V0.1`。

---

# 21. DDR Logical Layout

```text
DDR
│
├─ Framebuffer A
├─ Framebuffer B
├─ Command Ring
├─ Draw Descriptor Array
├─ Tile Header Array
├─ Tile Work List
├─ Texture Pool
├─ Palette Pool
├─ GUI / Font Resources
├─ Optional Depth Buffer
├─ Optional Vertex / 3D Descriptor
└─ Debug / Dump Region
```

推荐 Alignment：

- Command：64B；
- Draw Descriptor：64B；
- Tile Header：16B；
- WorkRef：4B；
- Texture：至少16B，推荐64B；
- Framebuffer：64B。

---

# 22. Clock / Reset Architecture

## 22.1 Clock Domains

### GPU Core Clock
Command、Front-End、Texture、Pixel、Tile、Memory Logic。

### DDR / Interconnect Clock
平台 DDR Controller 决定。

### Display Pixel Clock
720p60 标准目标：

> 74.25 MHz

### HDMI PHY / Serialization Clock
显示模块决定。

## 22.2 CDC

统一使用：

- Async FIFO；
- Pulse Synchronizer；
- Gray Pointer FIFO。

## 22.3 Reset

```text
Power On
  ↓
PLL Lock
  ↓
DDR Calibration Done
  ↓
GPU Core Reset Release
  ↓
Display Reset Release
  ↓
CPU Driver Init
  ↓
GPU Ready
```

---

# 23. Backpressure 与流接口

模块间统一：

> **Valid / Ready**

规则：

- `valid && ready` 才发生传输；
- `valid && !ready` 时 payload 保持稳定；
- 避免长组合 ready path；
- 必要时插入 FIFO / Skid Buffer。

---

# 24. 2D Rendering Data Flow

## Immediate BLIT

```text
Command DMA
   ↓
Parser
   ↓
2D Front-End
   ↓
Fragment
   ↓
Texture Unit
   ↓
RGBA8888
   ↓
ColorKey / Alpha / Blend
   ↓
Immediate RT Adapter
   ↓
DDR Framebuffer
```

## Tile BLIT

```text
TILE_FRAME
   ↓
Tile Scheduler
   ↓
Load Tile
   ↓
Work List
   ↓
Draw Descriptor
   ↓
2D Front-End
   ↓
Texture
   ↓
Pixel Backend
   ↓
Tile Buffer
   ↓
Tile Store
   ↓
RGB565 Framebuffer
```

---

# 25. Scaling Data Flow

```text
2D Front-End
   ↓
Q16.16 UV
   ↓
Sampler
   ├─ Nearest
   └─ Bilinear
   ↓
RGBA8888
   ↓
Blend
```

---

# 26. Indexed8 / Palette Data Flow

```text
DDR Indexed8
   ↓
Texture Fetch
   ↓
Palette Lookup
   ↓
RGBA8888
   ↓
Pixel Back-End
```

---

# 27. Affine / Mode-7 扩展路径

```text
Affine Descriptor
      ↓
Affine Front-End
      ↓
u=ax+by+c
v=dx+ey+f
      ↓
Standard Fragment Interface
      ↓
Texture Unit
      ↓
Pixel Back-End
      ↓
Tile / FB
```

无需修改 Command Ring、Texture Decode、Blend、Tile、Display 基础接口。

---

# 28. Triangle Rasterizer 扩展路径

```text
Triangle Descriptor
      ↓
Triangle Raster Front-End
      ↓
Fragment:
x,y,u,v,color,z,coverage
      ↓
Texture Unit
      ↓
Depth Test
      ↓
Blend
      ↓
Tile Color + Depth
```

新增：

- Triangle Opcode；
- Triangle Front-End；
- Depth Test；
- Depth Plane。

复用：

- Command；
- Memory；
- Texture；
- Blend；
- Tile；
- Display；
- Perf。

---

# 29. 实验性3D CPU / GPU 分工

### CPU

- Model/View/Projection；
- Vertex Transform；
- Triangle Setup；
- Bounding Box；
- Tile Binning。

### FPGA

- Triangle Raster；
- Interpolation；
- Depth；
- Texture；
- Blend。

---

# 30. Architecture Ablation

V1.0 必须支持公平比较：

### Immediate Mode
vs
### Tile Mode

二者共享：

- Command；
- Texture；
- Pixel Backend；
- Display。

这样性能差异主要来自 Memory Architecture。

---

# 31. 性能目标对架构的约束

竞赛主目标：

> **1280×720 @ Stable 60 FPS**

显示像素率约：

\[
1280	imes720	imes60=55.3	ext{ MPixel/s}
\]

实际 GPU Pixel Throughput 必须显著高于该数值，因为存在：

- Overdraw；
- Alpha；
- Particle；
- Scaling；
- 多层合成。

因此架构必须允许：

- 高 Fmax；
- Multi-Lane；
- Tile Reuse；
- Burst；
- Texture 优化。

---

# 32. Feature Gate

RTL 参数：

```text
ENABLE_TILE
ENABLE_BILINEAR
ENABLE_INDEXED8
ENABLE_TEX_CACHE
ENABLE_AFFINE
ENABLE_TRIANGLE
ENABLE_ZBUFFER
ENABLE_TRACE
```

关闭功能时：

- 使用 Bypass；
- Capability=0；
- 综合删除无用逻辑；
- 顶层和基础接口语义保持不变。

---

# 33. RTL 模块划分建议

```text
gpu_top
│
├─ gpu_ctrl
│  ├─ mmio_regs
│  ├─ capability_regs
│  ├─ irq_ctrl
│  ├─ fault_ctrl
│  └─ perf_monitor
│
├─ cmd_frontend
│  ├─ cmd_dma
│  ├─ cmd_ring_ctrl
│  ├─ cmd_parser
│  └─ dispatcher
│
├─ render_frontend
│  ├─ fe_2d
│  ├─ fe_affine          [EXT]
│  ├─ fe_vector          [EXT]
│  └─ fe_triangle        [EXT]
│
├─ texture_subsystem
│  ├─ tex_addr_gen
│  ├─ tex_cache          [optional]
│  ├─ tex_decompress     [optional]
│  ├─ tex_format
│  ├─ palette
│  └─ sampler
│
├─ pixel_backend
│  ├─ color_key
│  ├─ color_mod
│  ├─ depth_test         [optional]
│  ├─ blend
│  ├─ rop
│  └─ format_convert
│
├─ render_target
│  ├─ rt_immediate
│  ├─ rt_tile
│  ├─ tile_scheduler
│  ├─ tile_buffer
│  └─ depth_tile         [optional]
│
├─ memory_service
│  ├─ mem_arbiter
│  ├─ burst_engine
│  ├─ read_fifo
│  ├─ write_fifo
│  ├─ qos
│  └─ platform_mem_adapter
│
└─ display_engine
   ├─ scanout
   ├─ osd
   ├─ compositor
   ├─ timing
   └─ hdmi_adapter
```

---

# 34. 第一阶段也必须遵守最终架构

第一阶段即使只实现：

```text
FILL + BLIT
```

也使用：

```text
Command Parser
   ↓
2D Front-End
   ↓
Texture / Constant
   ↓
Pixel Back-End
   ↓
Immediate RT Adapter
   ↓
Memory Service
```

只是：

```text
ENABLE_TILE=0
ENABLE_AFFINE=0
ENABLE_TRIANGLE=0
```

不能写一个以后整体丢弃的临时 BitBlt Top。

---

# 35. Architecture V1.0 冻结决策

| 项目 | V1.0 决策 |
|---|---|
| 产品定位 | 通用嵌入式2D GPU |
| CPU/GPU控制 | MMIO Control Plane |
| GPU工作提交 | DDR Command Ring |
| Base Command | 固定64B |
| Command Address | 32-bit Physical |
| Command Ring | 2^N Entries，64B对齐 |
| Tile模式 | CPU Binning + FPGA Tile Renderer |
| Tile WorkRef | 32-bit Draw Descriptor Index |
| 默认Tile | 32×32，可参数化 |
| Tile Color | RGBA8888 |
| Tile Banking | Dual-Bank架构 |
| 主Framebuffer | RGB565 |
| Texture | RGB565 / ARGB8888 / Indexed8 |
| 内部颜色 | RGBA8888 |
| UV | signed Q16.16 |
| Fragment | Vectorized，可参数化LANES |
| Render Front-End | 2D + 可插拔Affine/Vector/Triangle |
| Pixel Back-End | 所有前端共享 |
| Immediate/Tile | 共用Pixel Backend，通过RT Adapter切换 |
| Texture Unit | 独立共享 |
| Texture Cache | 固定扩展点，初期Bypass |
| Depth | 32-bit接口/存储预留 |
| Memory | Central Memory Service |
| 外部高带宽总线 | 优先AXI4 Full，经Adapter封装 |
| Display | 与Render Core解耦 |
| Buffering | Double Buffer + VSYNC Present |
| OSD | 独立Plane架构 |
| Perf | Central Event/Counter |
| Fault | 统一Fault机制 |
| 3D扩展 | 新增Front-End，复用后端 |
| Feature管理 | Compile-time Gate + Runtime CAPS |

---

# 36. 尚未冻结的微架构参数

进入后续 Architecture Exploration：

- GPU Core Fmax；
- LANES = 1/2/4；
- DDR Burst Length；
- Read/Write FIFO Depth；
- Texture Cache Size；
- Texture Cache Associativity；
- Palette RAM 实现；
- Competition Profile 是否启用完整 Dual-Bank；
- Memory Arbitration 权重；
- Bilinear流水结构；
- Blend流水级；
- Alpha Rounding；
- Q16.16内部乘加位宽；
- OSD Character/Bitmap；
- Triangle/Z具体阶段。

这些不得破坏本文已经冻结的系统接口。

---

# 37. 下一阶段接口规格

## 1. GPU Command ISA V0.1
冻结：

- 64B Command字段；
- Opcode；
- Flags；
- Fence；
- Tile Frame；
- Extension Pointer。

## 2. Internal Interface Specification V0.1
冻结：

- `command_if`
- `work_if`
- `fragment_if`
- `texture_if`
- `render_target_if`
- `memory_if`
- `event_if`

## 3. Pixel Format & Arithmetic Specification V0.1
冻结：

- RGB565；
- ARGB8888；
- RGBA8888；
- Alpha；
- Rounding；
- Saturation；
- Bilinear。

## 4. Memory Map & Register Map V0.1

## 5. Tile Architecture Model

---

# 38. 架构扩展性验收标准

如果后续增加一个已规划功能时必须：

- 修改 Command Ring 基本协议；
- 修改所有 Front-End/Back-End 接口；
- 重写 Display；
- 让应用直接识别新硬件细节；

则视为 System Architecture V1.0 扩展性设计失败。

理想扩展：

### Bilinear
只改 Sampler。

### Indexed8
只改 Texture Decode / Palette。

### Additive
只改 Blend Operator。

### Texture Cache
插入 Cache。

### Mode-7
增加 Affine Front-End + Opcode。

### Triangle
增加 Triangle Front-End + Depth Plane/Test。

---

# 39. 最终架构总结

```text
Software Scene
      ↓
Graphics API
      ↓
Command / Tile Work
      ↓
Pluggable Render Front-End
      ↓
Generic Fragment Stream
      ↓
Shared Texture Unit
      ↓
Shared Pixel Back-End
      ↓
Immediate / Tile Render Target
      ↓
Central Memory Service
      ↓
Framebuffer
      ↓
Independent Display Engine
```

体系结构三条主创新：

1. **Command-Driven Asynchronous Front-End**  
   解决 CPU Draw Submission 开销。

2. **Tile-Based Render Target / Memory Architecture**  
   解决高 Overdraw 下 DDR Read-Modify-Write 压力。

3. **Unified Configurable Pixel Back-End**  
   解决多种像素操作重复访存和扩展困难。

同时通过：

> **Pluggable Front-End + Stable Fragment Interface**

为：

- Affine / Mode-7；
- Vector；
- Triangle / Z；

提供自然扩展路径。

---

# 40. V1.0 一句话定义

> **采用“命令驱动前端 + 可插拔图形前端 + 统一 Fragment/Texture/Pixel 后端 + Tile-Based Render Target + 集中式 Memory Service + 独立 Display Engine”的模块化架构，使同一套 RISC-V–FPGA GPU 从基础 BitBlt 能够逐步扩展到高密度 Sprite、透明混合、缩放、Palette、2.5D Affine 以及实验性三角形光栅化，而无需推翻既有模块和接口。**
