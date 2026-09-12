# RISC-V–FPGA 通用 2D GPU
# Golden GPU Software Architecture V0.1

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：PC Golden GPU / Architecture Modeling 软件架构规格  
> 版本：V0.1  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Verification_Plan_V0.1.md`  
> 状态：Golden Software Baseline Freeze Candidate

---

# 0. 文档目的

本文档定义 PC 端 Golden GPU Modeling Platform 的软件架构、模块职责、数据流、接口边界、测试文件格式和建模范围。

该平台不是一个“为了显示画面而写的软件渲染器”，而是本项目的：

> **Executable Specification + Functional Golden Model + Architecture Experiment Platform + Verification Vector Generator + Demo Prototype Backend**

核心目标：

1. 用软件完整表达最终 GPU 的图形功能和 Command ISA 语义；
2. 作为 FPGA RTL 的位精确正确性参考；
3. 在 RTL 前验证 Graphics API、Command ISA、Tile Renderer 等架构方案可行性；
4. 为尚未冻结的 Tile、Cache、Lane、Bandwidth 等参数提供实验依据；
5. 自动生成 RTL / FPGA 验证所需 Test Vector；
6. 支持游戏、GUI、Benchmark 和 X-Ray 在 PC 上提前开发；
7. 不在当前阶段构建完整 Cycle-Accurate GPU Simulator。

---

# 1. 顶层定位

PC 端统一称为：

> **PC GPU Modeling Platform**

其内部划分为五个子系统：

```text
PC GPU Modeling Platform
│
├── 1. Functional Golden GPU
│      └── Bit-Accurate / Pixel-Exact
│
├── 2. Workload Profiler
│      └── Draw / Pixel / Overdraw / Memory Traffic
│
├── 3. Architecture Explorer
│      └── Tile / Lane / Format / WorkList Sweep
│
├── 4. Cache Simulator
│      └── Texture Access Trace / Hit-Miss Analysis
│
└── 5. Performance Estimator
       └── Analytical / Transaction-Level Estimation
```

V0.1 明确：

> **不要求 Cycle-Accurate。**

后续若 RTL 已稳定且确有分析需要，可增加：

> Cycle-Approximate Model

但它不是 Golden GPU 正确性的组成部分。

---

# 2. Golden GPU 的权威地位

数值正确性遵循：

```text
Specification
     ↓
Golden GPU
     ↓
RTL
     ↓
FPGA Hardware
```

即：

> RTL 应实现 Golden 所定义的行为，而不是 Golden 去追随 RTL 的当前实现细节。

如果：

```text
Golden != RTL
```

在没有规格变更的情况下：

> RTL 判错。

如果 Golden 与文字规格冲突：

应先 Review：

- Command ISA；
- Pixel Arithmetic；
- Golden Implementation；

然后统一修正规格和模型版本。

---

# 3. Golden 不模拟的内容

Golden V0.1 不模拟：

- RTL流水级数；
- 精确Clock周期；
- DDR真实Bank Timing；
- FIFO逐周期Occupancy；
- Ready/Valid每周期握手；
- CDC；
- PLL；
- P&R；
- Routing Delay；
- 精确Fmax；
- 精确IRQ时延。

这些属于：

- RTL Simulation；
- STA；
- FPGA Hardware Measurement。

Golden只模拟：

> **Architectural / Functional Behavior**

---

# 4. 软件技术栈建议

## 4.1 核心语言

推荐：

> **C++17 或 C++20**

原因：

- 与RISC-V侧 C/C++ Driver 接近；
- 固定宽度整数；
- 性能高；
- 适合逐像素处理；
- 方便共享结构和常量；
- 可直接生成二进制 Test Vector。

---

## 4.2 Python

Python负责：

- Test orchestration；
- Parameter sweep；
- Frame compare；
- Plot；
- Result analysis；
- Benchmark report；
- Regression；
- Artifact generation。

原则：

> Python 不作为位精确像素计算的唯一实现。

最核心 Pixel Arithmetic 建议统一在：

> C++ Golden Core

避免Python与RTL出现不同整数语义。

---

## 4.3 显示库

可选择：

- SDL2；
- GLFW/OpenGL；
- Qt；
- 其他轻量窗口库。

要求：

> 显示库只能显示 Golden 自己生成的 framebuffer。

禁止：

> 用 OpenGL/SDL GPU renderer 代替 Golden 的图形算法。

---

# 5. 软件总体架构

```text
┌───────────────────────────────────────────────────────────┐
│                    Applications                           │
│ Game / GUI / Benchmark / X-Ray / Tests                   │
└─────────────────────────┬─────────────────────────────────┘
                          │ Graphics API
                          ▼
┌───────────────────────────────────────────────────────────┐
│                    Graphics Runtime                       │
│ Sprite / Resource / Command Builder / Tile Binner         │
└─────────────────────────┬─────────────────────────────────┘
                          │ 64B Command / Descriptor
                          ▼
┌───────────────────────────────────────────────────────────┐
│                 Golden Command Front-End                  │
│ ISA Decoder / Validator / Dispatcher                      │
└─────────────────────────┬─────────────────────────────────┘
                          │ RenderContext / TileFrame
                          ▼
┌───────────────────────────────────────────────────────────┐
│                    Golden Renderer                        │
│                                                           │
│ 2D Front-End / Affine [EXT] / Triangle [EXT]              │
│        ↓                                                  │
│ Texture Unit                                              │
│        ↓                                                  │
│ Pixel Pipeline                                            │
│        ↓                                                  │
│ Immediate RT / Tile RT                                    │
└─────────────────────────┬─────────────────────────────────┘
                          │
                          ▼
┌───────────────────────────────────────────────────────────┐
│                  Golden Memory Model                      │
│ Surface / Texture / Palette / Descriptor / WorkList       │
└─────────────────────────┬─────────────────────────────────┘
                          │
              ┌───────────┼────────────┐
              ▼           ▼            ▼
        Framebuffer    Profiler    Trace / Dump
```

---

# 6. Repository Structure

推荐：

```text
model/golden/
│
├── CMakeLists.txt
├── include/
│  ├── gpu_types.hpp
│  ├── gpu_isa.hpp
│  ├── gpu_formats.hpp
│  ├── gpu_math.hpp
│  ├── gpu_surface.hpp
│  ├── gpu_texture.hpp
│  └── gpu_golden.hpp
│
├── src/
│  ├── isa/
│  │  ├── command_decoder.cpp
│  │  ├── validator.cpp
│  │  └── render_context.cpp
│  │
│  ├── render/
│  │  ├── frontend_2d.cpp
│  │  ├── frontend_affine.cpp
│  │  ├── texture_unit.cpp
│  │  ├── sampler_nearest.cpp
│  │  ├── sampler_bilinear.cpp
│  │  ├── pixel_pipeline.cpp
│  │  ├── blend.cpp
│  │  ├── rt_immediate.cpp
│  │  └── rt_tile.cpp
│  │
│  ├── memory/
│  │  ├── memory_image.cpp
│  │  ├── surface.cpp
│  │  ├── texture.cpp
│  │  └── palette.cpp
│  │
│  ├── tile/
│  │  ├── tile_binner.cpp
│  │  ├── tile_worklist.cpp
│  │  └── tile_renderer.cpp
│  │
│  ├── profiling/
│  │  ├── workload_profiler.cpp
│  │  ├── overdraw.cpp
│  │  ├── memory_counter.cpp
│  │  └── trace.cpp
│  │
│  └── app/
│     ├── cli_main.cpp
│     └── viewer.cpp
│
├── tests/
│  ├── unit/
│  ├── directed/
│  ├── random/
│  └── frames/
│
└── tools/
   ├── dump_command.py
   ├── compare_frame.py
   └── run_regression.py
```

Architecture Model建议单独：

```text
model/architecture/
├── tile_model/
├── cache_model/
├── bandwidth_model/
├── lane_model/
└── reports/
```

但共享 Golden 的 Command / Workload。

---

# 7. 单一常量源

ISA / Register / Pixel Enum 应避免三份手写。

推荐：

```text
spec/
├── gpu_isa.yaml
├── gpu_regs.yaml
└── gpu_constants.yaml
```

自动生成：

```text
software/include/gpu_isa.h
rtl/include/gpu_isa_pkg.sv
model/golden/include/gpu_isa_generated.hpp
```

V0.1若暂不实现生成器：

> 至少写自动一致性检查。

---

# 8. Core Object Model

Golden核心对象：

```text
GoldenGPU
│
├── CommandProcessor
├── GPUState
├── MemoryImage
├── ResourceTable
├── Renderer
│  ├── Frontend2D
│  ├── TextureUnit
│  ├── PixelPipeline
│  ├── ImmediateRenderTarget
│  └── TileRenderTarget
├── Profiler
└── TraceRecorder
```

---

# 9. GoldenGPU

建议接口：

```cpp
class GoldenGPU {
public:
    void reset();

    ExecResult execute_command(const GpuCmd64& cmd);

    ExecResult execute_stream(
        std::span<const GpuCmd64> commands);

    void load_memory(...);
    void save_memory(...);

    const Profiler& profiler() const;
};
```

职责：

- Command级执行；
- Fault；
- Fence语义；
- Present语义；
- Render；
- Perf语义统计。

---

# 10. Golden Memory Model

V0.1采用：

> **Flat 32-bit Physical Address Space Model**

软件对象：

```cpp
class MemoryImage {
public:
    uint8_t  read8(uint32_t addr);
    uint16_t read16(uint32_t addr);
    uint32_t read32(uint32_t addr);

    void write8(...);
    void write16(...);
    void write32(...);

    void read_block(...);
    void write_block(...);
};
```

---

## 10.1 目的

Golden应使用：

> 与FPGA一致的物理地址语义。

而不是所有资源都只用C++对象指针。

这样可以发现：

- 地址计算错误；
- stride错误；
- alignment问题；
- Descriptor/Palette地址问题。

---

## 10.2 Memory Region

MemoryImage维护Region：

```text
Framebuffer
Texture
Command
Descriptor
WorkList
Palette
Depth
Debug
```

Debug模式可检查：

- 越界；
- 只读区域写；
- 未分配地址。

---

# 11. Surface Object

```cpp
struct SurfaceDesc {
    uint32_t base;
    uint32_t stride;
    uint16_t width;
    uint16_t height;
    PixelFormat format;
};
```

Surface读写必须走：

> Pixel Format Specification

统一函数：

```cpp
RGBA read_pixel(surface, x, y);
void write_pixel(surface, x, y, RGBA c);
```

---

# 12. Texture Object

逻辑：

```cpp
struct TextureDesc {
    uint32_t base;
    uint32_t stride;

    uint16_t src_x;
    uint16_t src_y;
    uint16_t src_w;
    uint16_t src_h;

    PixelFormat format;
    uint32_t palette_addr;
};
```

Golden Texture不使用“隐藏PNG对象”。

PNG/JPG导入后必须先转换成：

> 实际GPU存储格式。

---

# 13. Palette

统一：

```text
256 × RGBA8888
```

Golden必须从：

```text
PALETTE_ADDR
```

对应的 MemoryImage读取。

允许优化：

> 内部cache decoded palette

但结果必须仍服从地址内容。

---

# 14. Command Front-End

结构：

```text
64B Binary Command
      ↓
Header Decode
      ↓
Validation
      ↓
Extension Fetch
      ↓
Normalized RenderContext
      ↓
Dispatch
```

必须直接读取：

> `GpuCmd64`

禁止应用绕过 ISA 直接调用内部 Renderer 作为正式验证路径。

---

# 15. 双API模式

可以保留两类接口。

## 15.1 Internal Unit API

用于单元测试：

```cpp
blend(...)
sample(...)
render_context(...)
```

## 15.2 Architectural API

正式场景：

```text
Command Binary
→ Golden GPU
```

最终Reference Frame必须通过：

> Architectural API

生成。

---

# 16. ISA Validator

必须复现硬件 Strict Rules：

- Class；
- Opcode；
- Version；
- Length；
- Capability；
- Format；
- Blend；
- Filter；
- Alignment；
- Ext Type；
- Tile Config。

Golden非法Command不得“自动纠正”。

---

# 17. Fault Model

Golden定义：

```cpp
struct GpuFault {
    FaultSeverity severity;
    FaultCode code;
    uint32_t address;
    uint32_t sequence_id;
    uint32_t user_tag;
    uint32_t info;
};
```

Test中可直接比较：

```text
Golden Fault
vs
RTL Fault
```

---

# 18. Normalized RenderContext

Golden RenderContext字段应与：

> Internal Interface Specification

语义一致。

建议直接建立共享结构名：

```cpp
struct RenderContext {
    ...
};
```

字段包括：

- Source；
- Destination；
- Rect；
- Blend；
- Filter；
- Alpha；
- Key；
- Palette；
- Clip；
- UV；
- Flags。

---

# 19. 2D Front-End

职责：

> 将 RenderContext + Raster Bounds 转换为 Fragment。

Golden无需模拟packet/lane时序。

逻辑循环：

```cpp
for y in raster_y:
    for x in raster_x:
        Fragment f = generate_fragment(ctx, x, y);
        ...
```

---

# 20. Golden Fragment

```cpp
struct Fragment {
    int16_t x;
    int16_t y;

    int32_t u_q16_16;
    int32_t v_q16_16;

    uint32_t vertex_color;
    uint32_t z;
    uint8_t coverage;
};
```

与RTL `fragment_if` 语义一致。

---

# 21. Texture Unit

职责：

1. 判断 `TEX_ENABLE`
2. Address Mode
3. Nearest/Bilinear
4. Raw Texture Fetch
5. Pixel Format Decode
6. Palette
7. 输出Canonical RGBA8888

---

# 22. Texture Access Trace

Texture Unit每次Raw Texel Read应可选记录：

```text
frame
command_seq
draw_id
x/y
u/v
texture_addr
bytes
```

用于：

> Cache Simulator / Bandwidth Model

因此 Functional Golden 与 Architecture Model之间共享：

> **真实Texture Address Stream**

---

# 23. Pixel Pipeline

建议纯函数化：

```cpp
RGBA process_pixel(
    const RenderContext& ctx,
    const Fragment& frag,
    RGBA src,
    RGBA dst);
```

内部严格按：

```text
Color Key
→ Color Mod
→ Effective Alpha
→ Optional Depth
→ Blend
→ Logical RT Quantization
```

---

# 24. Pixel Arithmetic复用

所有算法来自单独模块：

```text
gpu_math.cpp
```

必须包含：

- div255_rn
- mul8_rn
- lerp16
- rgb565 encode/decode
- alpha
- additive
- bilinear
- q16 helpers

禁止：

> Viewer / Benchmark / Tile Renderer各写一套Blend。

---

# 25. Immediate Render Target

V0.1：

```text
Pixel
 ↓
Surface Read
 ↓
Blend
 ↓
DST_FORMAT Quantization
 ↓
MemoryImage Write
```

同时统计：

- logical FB read bytes；
- logical FB write bytes；
- render pixels。

---

# 26. Tile Render Target

采用：

```text
Tile Load
 ↓
Tile Local Render
 ↓
Tile Store
```

Competition Compatibility模式：

> 每次逻辑RT Write执行 DST_FORMAT Quantization。

Tile内部物理模拟可使用：

```text
RGBA8888 array
```

但其值必须是：

> 量化后再Expand的Canonical Color。

---

# 27. Immediate / Tile强一致性

同一：

- Command；
- Draw Order；
- Resource；
- Format；

必须：

```text
Immediate framebuffer
==
Tile framebuffer
```

V0.1要求：

> Pixel Exact。

这是Golden自身的重要单元/集成测试。

---

# 28. Tile Binner

软件Tile Binner既是：

- PC模型组件；
- 未来RISC-V软件算法原型。

建议单独模块：

```cpp
TileBins build_tile_bins(
    const std::vector<DrawDescriptor>& draws,
    SurfaceDesc target,
    TileConfig cfg);
```

---

# 29. Tile Binner输入

- Draw Descriptor Array；
- Surface；
- Tile W/H；
- Clip；
- Draw Order。

输出：

```text
TileHeader[]
WorkRef[]
```

要求：

> 可以直接序列化为 FPGA 使用的二进制格式。

---

# 30. Tile Binner顺序

WorkRef必须保持：

> 原Draw Submission Order。

不得为了模型方便进行透明Draw重排。

---

# 31. Tile Renderer

Golden Tile Renderer必须从真实：

- Tile Header；
- WorkList；
- Draw Descriptor；

执行。

不要直接使用 Binner 内部C++容器绕过序列化格式作为最终验证路径。

推荐：

```text
Binner
 ↓ serialize
tile_headers.bin/worklist.bin
 ↓
Golden Tile Renderer重新解析
```

这样能验证：

> 二进制Tile数据结构本身。

---

# 32. Present Model

PC Golden无需等待真实VSYNC。

定义：

```text
PRESENT
→ pending_surface = surface
```

如果运行：

> Functional Mode

可直接在“virtual vsync”点完成Flip。

Viewer可在每Frame结尾触发：

```text
golden_vsync()
```

---

# 33. Fence Model

Golden没有真实流水线和DDR outstanding。

因此：

`FENCE_SIGNAL`

只需在所有先前同步执行命令完成后：

- 更新 fence；
- optional writeback；
- 记录event。

这是 Architectural Semantics，不模拟实际等待周期。

---

# 34. Workload Profiler

Golden每Frame统计：

```text
FrameStats
│
├─ command_count
├─ draw_count
├─ pixel_generated
├─ pixel_discarded
├─ pixel_blended
├─ pixel_scaled
├─ texture_read_bytes
├─ framebuffer_read_bytes
├─ framebuffer_write_bytes
├─ palette_read_count
├─ tile_load_count
├─ tile_store_count
├─ tile_work_count
└─ overdraw metrics
```

---

# 35. Logical Traffic vs Physical Traffic

V0.1区分：

## Logical Traffic

由图形算法必然产生：

- texel读取；
- FB read；
- FB write；
- Tile load/store。

## Estimated Physical Traffic

考虑：

- Burst；
- Cache；
- Reuse；
- Alignment；

由 Architecture Model估计。

这样避免Golden把某种硬件实现方式写死。

---

# 36. Overdraw Profiler

维护：

```text
overdraw_count[x,y]
```

统计：

- Average Overdraw；
- Max Overdraw；
- P95 Overdraw；
- Overdraw Heatmap。

这是 Tile 架构评估的重要输入。

---

# 37. Tile Profiler

每Tile统计：

```text
work_count
pixel_count
blend_count
load_bytes
store_bytes
```

输出：

- active tile count；
- avg work/tile；
- max work/tile；
- tile reuse；
- heatmap。

---

# 38. Command Profiler

统计：

- Command count；
- Draw type；
- Feature usage；
- Alpha ratio；
- Bilinear ratio；
- Indexed8 ratio；
- Scale ratio。

用于回答：

> 最终游戏究竟在大量使用什么功能。

---

# 39. Architecture Explorer

Architecture Explorer 不修改 Golden的数值行为。

输入：

> Golden产生的 workload trace / command stream。

---

# 40. Tile Size Sweep

自动跑：

```text
16×16
32×32
64×64
```

可选：

8×8。

输出：

- Tile Count；
- WorkRef Count；
- Header Bytes；
- CPU Binning Time；
- Estimated FB Traffic；
- Active Tile Ratio；
- Average Work/Tile。

---

# 41. Lane Explorer

Lane Model不需要逐Cycle。

输入：

```text
pixel workload
feature mix
estimated pipeline throughput
target Fmax
```

比较：

- 1 lane；
- 2 lane；
- 4 lane。

输出：

```text
peak pixel throughput
required memory bandwidth
estimated utilization range
resource multiplier estimate
```

最终LANES必须结合：

> RTL综合/板级数据

再冻结。

---

# 42. Bandwidth Model

输入：

- Display Scanout；
- Command；
- Texture；
- RT；
- Tile；
- Depth。

输出：

```text
bytes/frame
bytes/s @ 60FPS
bandwidth share
```

---

# 43. Roofline式性能估计

采用：

\[
P_{actual}
\approx
\min(P_{compute},P_{memory})
\]

其中：

\[
P_{compute} = F_{clk}\times LANES\times utilization
\]

\[
P_{memory} =
\frac{BW_{effective}}
{BytesPerPixelEffective}
\]

V0.1不追求：

> 单一精确FPS小数值。

推荐输出：

- Upper Bound；
- Lower/Conservative Bound；
- Bottleneck；
- Required BW。

---

# 44. Cache Simulator

独立于 Functional Golden。

输入：

> Texture Address Trace

配置：

```text
cache size
line size
associativity
replacement
```

输出：

- Hit；
- Miss；
- Hit Rate；
- Read Bytes Saved；
- Working Set；
- Conflict Miss。

---

# 45. Cache初始Sweep

建议：

```text
1KB
2KB
4KB
8KB
16KB
```

Associativity：

```text
Direct-Mapped
2-Way
```

Line Size后续结合 DDR Burst选择。

---

# 46. Cache Simulator不影响Golden结果

无论Cache配置：

> Golden Framebuffer必须相同。

Cache只影响：

- estimated traffic；
- hit/miss；
- performance estimate。

---

# 47. Performance Model边界

V0.1明确不模拟：

```text
Cycle N:
FIFO full
Cycle N+1:
DDR ready...
```

而采用：

> Analytical / Transaction-Level

---

# 48. Cycle Model准入条件

只有满足以下情况才考虑增加 Cycle-Approximate Model：

1. Basic GPU + Command Ring + Tile 已稳定；
2. 硬件实测性能与Analytical Model偏差明显；
3. 需要定位Queue/Stall；
4. 添加该模型不会影响主项目进度。

---

# 49. Cycle Model如果实现

只需要近似模拟：

- Queue Depth；
- Service Rate；
- Memory Stall；
- Tile Load/Store overlap；
- Texture miss；
- Pixel throughput。

不要求：

> 复制RTL每一级寄存器。

---

# 50. 应用层开发

PC Golden必须支持主应用直接运行：

```text
Game
GUI
Benchmark
X-Ray
```

---

# 51. Graphics API Backend

推荐：

```cpp
class IGpuBackend {
public:
    virtual void begin_frame() = 0;
    virtual void submit(...) = 0;
    virtual void present() = 0;
};
```

实现：

```text
GoldenBackend
HardwareBackend
```

---

# 52. 应用透明切换

应用代码：

```cpp
renderer.draw_sprite(...)
```

PC：

```text
GoldenBackend
```

RISC-V：

```text
HardwareBackend
```

尽量保持上层接口一致。

---

# 53. Command Builder

Graphics API不直接调用Golden内部函数。

而是：

```text
Graphics API
   ↓
Command Builder
   ↓
64B Command
```

然后：

GoldenBackend再执行Command。

这确保：

> PC应用实际使用的是未来硬件接口。

---

# 54. Dual Backend Debug

PC可额外提供：

### Direct Reference Backend
内部直接调用render函数。

### ISA Golden Backend
通过64B Command解析。

二者结果必须一致。

用途：

> 检查Command Encoder / Decoder本身。

最终正式Reference：

> ISA Golden Backend。

---

# 55. Benchmark Workload

必须内置固定Scene：

- Sprite Storm；
- Alpha Storm；
- Overdraw Storm；
- Scaling Storm；
- Particle Storm；
- GUI Composition；
- Tile Stress。

---

# 56. Fixed Seed

所有Procedural Scene：

```text
seed
```

固定。

Reference版本不允许用当前时间作为随机种子。

---

# 57. Scene Definition

建议定义：

```text
scene.json
```

包含：

```text
resolution
seed
assets
sprite count
blend distribution
scale distribution
position distribution
overdraw pattern
```

Golden读取后生成：

> 确定性 Command Stream。

---

# 58. Test Vector Generator

Golden平台必须支持：

```text
--export-test-vector <dir>
```

生成：

```text
manifest.json
commands.bin
draw_desc.bin
extensions.bin
tile_headers.bin
worklist.bin
textures.bin
palettes.bin
initial_fb.bin
golden_fb.bin
golden_frame.png
stats.json
trace.json
```

---

# 59. Manifest

至少包含：

```json
{
  "isa_version": 1,
  "pixel_arith_version": 1,
  "width": 1280,
  "height": 720,
  "framebuffer_format": "RGB565",
  "seed": 12345,
  "tile_mode": true,
  "tile_width": 32,
  "tile_height": 32
}
```

---

# 60. Memory Image导出

理想情况下RTL Full-System Test可以直接加载：

> 与Golden相同的Memory Image。

例如：

```text
memory_init.bin
```

配套：

```text
memory_map.json
```

这样：

- Command；
- Texture；
- Palette；
- FB；

地址都完全一致。

---

# 61. Framebuffer Raw Format

必须固定。

## RGB565

```text
row-major
little-endian
stride-aware
```

建议Golden导出两种：

### `fb_tight.raw`
只包含有效width。

### `fb_memory.raw`
包含真实stride padding。

RTL Board对比优先：

> `fb_memory.raw`

---

# 62. PNG Preview

PNG仅用于：

- 人工观察；
- 文档；
- Demo。

不能作为位精确Compare的主格式。

主Compare：

> RAW。

---

# 63. Frame Compare Tool

输入：

```text
golden_fb.raw
rtl_fb.raw
```

输出：

```text
mismatch_count
first_mismatch
max_channel_diff
diff_image
```

Pixel-Exact模式：

```text
mismatch_count == 0
```

才PASS。

---

# 64. Debug Trace

Golden可选记录：

```text
CommandSeq
DrawID
WorkID
Pixel X/Y
UV
Sampled Color
Dst Before
Blend Mode
Result
```

不能默认全开：

> 数据量巨大。

建议按：

- Command；
- Pixel Region；
- Sequence ID；

过滤。

---

# 65. Pixel Query Debug

推荐命令：

```text
golden_gpu --trace-pixel 321,207
```

输出该Pixel经历的：

```text
Draw 17
Draw 81
Draw 93
...
```

以及每次Blend结果。

对定位RTL mismatch非常有价值。

---

# 66. Command Dump Tool

提供：

```text
command_dump commands.bin
```

输出：

```text
#17 BLIT_EXT
src=...
dst=...
rect=...
alpha=...
scale=...
```

这样RTL失败时无需手工看Hex。

---

# 67. Tile Dump Tool

输入：

```text
tile_headers.bin
worklist.bin
draw_desc.bin
```

输出：

```text
Tile(12,7):
  work_count = 14
  draw = 17
  draw = 21
  ...
```

---

# 68. X-Ray Visualization

PC阶段先实现最终X-Ray UI。

可显示：

- Tile Grid；
- Overdraw Heatmap；
- Work/Tile；
- Texture Access Heatmap；
- Blend Heatmap；
- Command Statistics；
- Estimated BW。

后续 FPGA上：

把数据源替换成：

- Hardware Counter；
- Driver Stats。

---

# 69. Golden Profiler与最终硬件Counter对齐

尽量使用同名字段：

```text
COMMAND_COUNT
PIXEL_COUNT
BLEND_PIXEL_COUNT
KEY_DISCARD_COUNT
TILE_LOAD_COUNT
TILE_STORE_COUNT
TILE_WORK_COUNT
```

Golden：

> Semantic Count

Hardware：

> Measured Count

最终可直接对表。

---

# 70. Memory Traffic Counter分类

Golden建议分开：

```text
CMD_READ_BYTES
DESC_READ_BYTES
WORKLIST_READ_BYTES
TEXTURE_READ_BYTES
FB_READ_BYTES
FB_WRITE_BYTES
TILE_LOAD_BYTES
TILE_STORE_BYTES
PALETTE_BYTES
DEPTH_READ_BYTES
DEPTH_WRITE_BYTES
```

不要只记录一个总DDR Bytes。

---

# 71. Reference Workload版本化

每个Benchmark：

```text
benchmark_name
version
seed
asset_hash
command_hash
```

任何改变：

- Sprite数量；
- Texture；
- Alpha比例；

必须升Workload Version。

---

# 72. Golden Regression

每次修改：

- Pixel Math；
- ISA；
- Tile；
- Resource；

运行：

```text
Unit Tests
Directed Frames
Random Commands
Immediate vs Tile
Reference Scenes
```

---

# 73. Golden Unit Tests

至少：

```text
test_rgb565
test_div255
test_alpha
test_additive
test_bilinear
test_q16
test_scale_uv
test_clip
test_address_mode
test_palette
test_command_decode
test_tile_binner
```

---

# 74. Golden Random Test

即使Golden自身是Reference，也要做：

> Internal Cross-Check

例如：

### Immediate Renderer
vs
### Tile Renderer

### Direct Reference Backend
vs
### ISA Backend

### Scalar Implementation
vs
### Optimized Implementation

防止Golden单点Bug。

---

# 75. Golden不宜过早优化

第一版优先：

- 清晰；
- 直接；
- 与规格一一对应；
- 便于调试。

不要一开始：

- SIMD；
- 多线程；
- GPU加速；
- complicated cache。

否则Golden自身难以审计。

---

# 76. 后续优化原则

如果Golden运行太慢，可增加：

- 多线程按Tile；
- SIMD；
- predecoded textures；

但：

> 必须保留Reference Scalar Path。

优化路径要与Reference做Cross-Check。

---

# 77. Determinism

Golden所有正式输出必须：

> Deterministic。

相同：

- binary；
- seed；
- assets；
- version；

输出必须完全相同。

---

# 78. 浮点限制

位精确核心禁止使用浮点决定：

- Alpha；
- RGB；
- Bilinear；
- UV step结果。

允许浮点用于：

- GUI统计；
- 绘图；
- 性能估算；
- 报表。

Scale/Affine最终写入Command的值必须通过：

> 规定的Integer Q16.16 helper。

---

# 79. Asset Pipeline

PNG等原始素材：

```text
PNG
 ↓
Asset Converter
 ↓
RGB565 / ARGB8888 / Indexed8
 ↓
texture.bin
```

Golden和FPGA使用：

> 同一转换后资产。

禁止：

Golden直接采样PNG真彩，而FPGA采样RGB565。

---

# 80. Texture Converter

功能：

- PNG → RGB565；
- PNG → ARGB8888；
- PNG → Indexed8 + Palette；
- stride；
- alignment；
- optional premultiply alpha。

---

# 81. Premultiply Asset

如果：

```text
PREMULT_SRC
```

素材转换工具必须明确：

```text
RGB = MUL8_RN(RGB, A)
```

并记录：

```text
premultiplied = true
```

不能在运行时含糊处理。

---

# 82. Architecture Experiment工作流

典型：

```text
Run Scene
   ↓
Golden generates command/workload
   ↓
Profiler collects trace
   ↓
Architecture Explorer sweeps params
   ↓
CSV/JSON
   ↓
Python plots
   ↓
Architecture decision
   ↓
Spec freeze/update
```

---

# 83. Tile Experiment示例

同一 Scene：

```text
tile=16
tile=32
tile=64
```

生成：

```text
tile_size
tile_count
workrefs
avg_work_per_tile
fb_bytes
binner_time
```

然后选：

> 综合收益最佳。

---

# 84. Framebuffer Format Experiment

同一Scene：

```text
RGB565
ARGB8888
```

比较：

- final image quality；
- memory bytes；
- alpha behavior；
- framebuffer traffic。

用于验证：

> RGB565作为主Framebuffer是否合理。

---

# 85. Indexed8 Experiment

统计：

- texture bytes；
- palette overhead；
- image quality；
- cache hit；
- bandwidth reduction。

决定哪些资源：

> 值得Indexed8。

---

# 86. Cache Experiment

Golden本身产生：

> exact texel address trace。

Cache Simulator针对：

```text
scene × cache config
```

批量输出结果。

---

# 87. Performance Estimate Output

建议：

```json
{
  "compute_peak_mpix_s": 150.0,
  "required_texture_mb_s": 420.0,
  "required_fb_mb_s": 350.0,
  "required_total_mb_s": 850.0,
  "assumed_effective_ddr_mb_s": 1200.0,
  "bottleneck": "compute",
  "fps_upper_bound": 91.4
}
```

强调：

> Estimate，不是Hardware Measurement。

---

# 88. Model与Hardware数据对比

后期：

```text
Golden/Model             FPGA Counter
------------------------------------------------
pixel_count              PIXEL_COUNT
blend_count              BLEND_PIXEL_COUNT
fb bytes estimate        DDR_*_BYTES
tile load/store          TILE_*_COUNT
cache prediction         CACHE_HIT/MISS
```

用于验证模型可信度。

---

# 89. 误差分析

如果模型与Hardware差异大：

调查：

- Burst inefficiency；
- alignment；
- command overhead；
- cache refill；
- arbitration；
- display contention；
- FIFO bubbles；
- actual Fmax。

而不是修改Golden像素算法。

---

# 90. Golden GUI / Viewer

建议Viewer支持：

### Normal
显示Framebuffer。

### Split
Immediate / Tile。

### Diff
显示Pixel Difference。

### X-Ray
显示Tile/Overdraw。

### Stats
显示Profiler。

---

# 91. Viewer不参与正确性

Viewer完全是Presentation层。

任何：

- Scaling窗口；
- 色彩转换；
- UI；

不得改变Golden framebuffer数据。

---

# 92. CLI 建议

```text
golden_gpu run scene.json
golden_gpu run commands.bin
golden_gpu export scene.json out/
golden_gpu benchmark alpha_storm
golden_gpu compare immediate tile
golden_gpu dump-command commands.bin
golden_gpu trace-pixel x y
```

---

# 93. Return Code

自动化环境：

```text
0 = PASS
非0 = FAIL
```

便于CI。

---

# 94. Logging

分级：

```text
ERROR
WARN
INFO
DEBUG
TRACE
```

默认：

> INFO。

正式Regression：

> WARN/ERROR + result file。

---

# 95. 文件版本

所有Golden二进制输出建议在Manifest记录：

- format version；
- ISA version；
- arithmetic version。

不要靠文件名猜版本。

---

# 96. Reproducibility

Reference Frame必须能够通过：

```text
git commit
scene
seed
assets hash
config
```

重新生成。

---

# 97. Golden Release

每个重要项目版本保存：

```text
golden-release/
├── executable
├── configs
├── scenes
├── assets-hash
├── reference-frames
└── version.json
```

---

# 98. 开发阶段划分

## G0 — Skeleton

实现：

- project；
- types；
- MemoryImage；
- Surface；
- Command struct；
- CLI。

---

## G1 — Fill

实现：

- FILL_RECT；
- RGB565；
- Frame export。

---

## G2 — Blit

实现：

- Texture；
- RGB565 decode；
- 1:1 BLIT。

---

## G3 — Official Pixel

- Color Key；
- Global Alpha；
- Per-Pixel Alpha；
- Double Buffer semantics。

---

## G4 — Extended Pixel

- BLIT_EXT；
- Scale；
- Clip；
- Flip；
- Additive。

---

## G5 — Palette/Bilinear

- Indexed8；
- Palette；
- Bilinear。

---

## G6 — Tile

- Binner；
- WorkList；
- Tile Renderer；
- Immediate Exact Compare。

---

## G7 — Profiling

- Overdraw；
- memory traffic；
- tile stats；
- trace。

---

## G8 — Architecture Explorer

- Tile sweep；
- cache；
- bandwidth；
- lanes。

---

## G9 — Application

- Benchmark；
- Game；
- GUI；
- X-Ray。

---

## G10 — Stretch

- Affine；
- Mode-7；
- Triangle semantic prototype。

---

# 99. Golden与RTL开发关系

推荐：

```text
Golden Feature PASS
      ↓
Generate Test Vector
      ↓
RTL Feature Implement
      ↓
Unit Sim
      ↓
Full Frame Compare
      ↓
Board
```

除极简单基础Infrastructure外：

> 不建议RTL Feature领先Golden很久。

---

# 100. 第一阶段应立即实现的最小Golden

MVP：

```text
MemoryImage
Surface RGB565
Command Decoder
FILL_RECT
BLIT
Frame Export
Frame Compare
```

然后用它直接支撑：

> 第一版RTL FILL / BLIT。

---

# 101. Golden完成标准

一个Feature只有满足：

- 文档规格已定义；
- Golden实现；
- Directed Test；
- Random Test；
- Test Vector export；
- Reference Frame；

才算：

> Golden Ready for RTL。

---

# 102. Performance Modeling完成标准

一个架构参数只有经过：

- 固定Workload；
- 参数Sweep；
- 结果保存；
- 结论说明；

才允许从：

> Open Decision

升级到：

> Frozen Architecture Parameter。

---

# 103. PC Model阶段不做的事情

明确排除：

- 完整Cycle Accurate Simulator；
- 精确模拟每级FIFO；
- 精确AXI周期；
- DDR PHY模型；
- RTL等价状态机；
- 为预测一个精确FPS而过度建模。

---

# 104. 后期可选Cycle-Approximate Model

如果需要：

新增：

```text
model/perf_cycle/
```

不能污染：

```text
model/golden/
```

Golden继续保持：

> timing-independent。

---

# 105. 与Verification Plan的连接

Golden需为Verification Plan提供：

- Arithmetic Reference；
- Command Binary；
- Frame Reference；
- Fault Reference；
- Random Seed；
- Trace；
- Tile exact result。

---

# 106. 与System Architecture的连接

Golden完整实现的是：

> Architecture Semantics

不是：

> Microarchitecture Timing。

例如：

实现：

- Tile semantics；
- Texture sampling；
- Blend；

不模拟：

- Tile Buffer BRAM端口；
- DSP Pipeline Stage；
- Memory Arbiter每周期状态。

---

# 107. 与Command ISA的连接

Command ISA二进制：

> Golden必须直接支持。

任何正式Reference Frame不得使用“绕过ISA”的隐藏配置。

---

# 108. 与Pixel Arithmetic的连接

Pixel Arithmetic是：

> Golden数值实现最高权威。

建议 `gpu_math.cpp` 写成：

> 接近规格公式的直接代码。

并通过Exhaustive Unit Test。

---

# 109. 与RISC-V软件的连接

Golden Graphics API / Command Builder尽量共享：

- C structs；
- enums；
- opcode；
- packing helpers；

后续可以复用到RISC-V Driver。

---

# 110. 与最终Demo的连接

主游戏应最早运行在：

> GoldenBackend。

这样可提前完成：

- gameplay；
- visual effects；
- workload stress；
- X-Ray设计。

FPGA完成后只切换：

> HardwareBackend。

---

# 111. 关键设计决定总结

V0.1 决定：

| 项目 | 决策 |
|---|---|
| Golden Core | C++17/20 |
| Analysis/Plot | Python |
| Golden类型 | Bit-Accurate Functional |
| Cycle Accurate | 当前不做 |
| Performance Model | Analytical / Transaction-Level |
| Memory | Flat 32-bit physical memory image |
| Official path | Binary Command → Golden |
| Texture | 读取真实转换后GPU资源 |
| Pixel Math | 完全遵循Arithmetic Spec |
| Immediate/Tile | 两条独立路径，结果Pixel Exact |
| Tile Binner | 与未来RISC-V算法原型共用语义 |
| Test Vector | Golden自动导出 |
| Architecture Experiment | 基于真实Workload Trace |
| Cache | 独立Simulator，不改变图像 |
| Viewer | 只做显示，不参与渲染正确性 |
| Random | 固定Seed，可复现 |
| Timing | Golden与硬件时序解耦 |

---

# 112. V0.1 尚未冻结内容

- SDL / GLFW / Qt具体选择；
- CMake还是其他构建系统；
- JSON/YAML Scene具体Schema；
- Cache Line Size；
- Performance Estimator具体经验系数；
- 是否使用SQLite保存Benchmark；
- Golden内部是否多线程；
- Viewer UI框架；
- Cycle-Approximate模型是否最终需要。

这些不影响Golden核心语义。

---

# 113. 下一步实施顺序

建议立即：

### Step 1
建立：

```text
model/golden/
```

工程骨架。

### Step 2
实现：

```text
gpu_types
gpu_isa
gpu_math
MemoryImage
Surface
```

### Step 3
完成：

```text
FILL_RECT
```

并导出：

```text
golden_fill.raw
```

### Step 4
完成：

```text
BLIT
```

### Step 5
建立：

```text
frame_compare.py
```

### Step 6
建立第一个：

```text
commands.bin
textures.bin
initial_fb.bin
golden_fb.bin
```

验证向量。

这时就可以同步启动：

> RTL FILL_RECT。

---

# 114. V0.1 一句话定义

> **Golden GPU Software Architecture V0.1 将PC端建模系统划分为位精确Functional Golden、Workload Profiler、Architecture Explorer、Cache Simulator和Analytical Performance Estimator五部分；其中Golden直接执行与FPGA相同的64B Command ISA并作为RTL像素正确性的可执行规格，而性能建模仅统计Workload、Tile、Cache和Memory Traffic并进行架构级估算，不在当前阶段引入高成本的完整Cycle-Accurate模拟。**
