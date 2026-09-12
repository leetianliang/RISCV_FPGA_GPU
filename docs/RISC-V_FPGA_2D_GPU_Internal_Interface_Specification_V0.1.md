# RISC-V–FPGA 通用 2D GPU
# Internal Interface Specification V0.1

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：FPGA 内部模块接口规格  
> 版本：V0.1  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`  
> 状态：Internal Interface Baseline Freeze Candidate

---

# 0. 文档目的

本文档定义 FPGA GPU 内部主要模块之间的接口协议、字段、握手、顺序、Backpressure、错误传播和 Clock Domain 规则。

目标是让以下模块可以在接口冻结后并行开发：

- Command DMA / Parser；
- Dispatcher；
- Tile Scheduler；
- 2D Front-End；
- Affine / Triangle Front-End；
- Texture Unit；
- Pixel Back-End；
- Immediate / Tile Render Target；
- Memory Service；
- Performance Monitor；
- Fault Controller；
- Display Engine。

本文档冻结的是：

> **模块之间“怎么说话”。**

本文档不冻结：

- 模块内部状态机；
- FIFO具体深度；
- Pipeline具体级数；
- BRAM Banking具体映射；
- Memory Arbiter算法；
- 最终 LANES；
- 最终 GPU Core Frequency。

---

# 1. 接口总览

```text
Command DMA
    │
    │ command_if
    ▼
Command Parser
    │
    ├──────── control command ───────► Control / Display / Sync
    │
    └──────── render_context_if
                     │
                     ▼
                Dispatcher
                     │
                  work_if
                     │
                     ▼
          ┌───────────────────────┐
          │ Render Front-End      │
          │ 2D / Affine / 3D     │
          └──────────┬────────────┘
                     │
                fragment_if
                     │
                     ▼
               Texture Unit
             │              │
             │              ├── texture_req_if
             │              │        ↓
             │              │   Cache / Fetch
             │              │        ↓
             │              └── texture_rsp_if
             │
             ▼
                pixel_if
                     │
                     ▼
              Pixel Back-End
                     │
             ┌───────┴────────┐
             │                │
      rt_read_req_if     rt_write_if
             │                │
             ▼                ▼
        Render Target Subsystem
       Immediate / Tile Adapter
             │
             └──────── memory_req/rsp_if
                              │
                              ▼
                         Memory Service
                              │
                              ▼
                       Platform Memory

所有模块：
    ├── event_if ─────► Performance Monitor
    └── fault_if ─────► Fault Controller
```

---

# 2. 全局接口设计原则

## IF-P-01：流式接口统一使用 Valid / Ready

除 `event_if` 和 `fault_if` 外，所有数据流接口统一采用：

```text
valid
ready
payload
```

传输事件定义为：

\[
transfer = valid \land ready
\]

---

## IF-P-02：Stall 时 Payload 必须稳定

若：

```text
valid = 1
ready = 0
```

则 Producer 必须保持：

- `valid = 1`
- `payload` 不变

直到发生 transfer。

---

## IF-P-03：Producer 不得组合依赖 Ready 生成 Valid

禁止：

```text
valid = ready & has_data
```

推荐：

```text
valid = has_data
```

这样避免组合环路。

---

## IF-P-04：避免长 Ready Combinational Path

跨多个模块的 ready 链不得无限组合传播。

必要位置插入：

- Register Slice；
- Skid Buffer；
- FIFO。

---

## IF-P-05：一旦 Transfer，Payload 所有权转移

Producer 在：

```text
valid && ready
```

后的下一周期可以修改/释放该 Payload。

Consumer 必须在 transfer 时锁存所需数据。

---

## IF-P-06：Reset 后 Valid 必须清零

所有流式 Producer：

```text
reset => valid = 0
```

不得在 Reset Release 同周期产生不确定 transaction。

---

# 3. Clock Domain 总体规则

V0.1 定义四类 Clock Domain。

| Domain | 名称 | 主要模块 |
|---|---|---|
| `CLK_GPU` | GPU Core Clock | Parser / Front-End / Texture / Pixel / Tile / Perf |
| `CLK_MEM` | Memory / Interconnect Clock | Platform Memory Adapter / DDR Interface |
| `CLK_DISP` | Display Pixel Clock | Timing / Compositor / Pixel Output |
| `CLK_CPU` | CPU/MMIO Clock | Control Bus / IRQ端 |

---

## 3.1 GPU内部主数据流

以下接口默认都属于：

> `CLK_GPU`

- `command_if`
- `render_context_if`
- `work_if`
- `fragment_if`
- `texture_req_if`
- `texture_rsp_if`
- `pixel_if`
- `rt_*`
- `event_if`
- `fault_if`

---

## 3.2 Memory CDC

`Memory Service Core` 可以工作在 `CLK_GPU`。

跨到 `CLK_MEM`：

> 必须通过 `Platform Memory Adapter + Async FIFO / CDC Bridge`

GPU上层模块不得自行处理 DDR Clock Domain。

---

## 3.3 Display CDC

推荐：

```text
Memory Fetch @ CLK_GPU/CLK_MEM
       ↓
Async Scanout FIFO
       ↓
Display Compositor @ CLK_DISP
```

禁止 Render Pipeline 直接跨到 `CLK_DISP`。

---

## 3.4 MMIO CDC

如果 MMIO 与 GPU Core 不同域：

- 配置寄存器：Shadow + CDC；
- 单bit控制：Synchronizer；
- Pulse：Toggle/Pulse Synchronizer；
- 多bit动态数据：Handshake/FIFO。

---

# 4. 全局参数建议

V0.1 推荐在公共 `gpu_pkg.sv` 中定义：

```systemverilog
parameter int GPU_ADDR_W       = 32;
parameter int GPU_COLOR_W      = 32;   // RGBA8888
parameter int GPU_COORD_W      = 16;
parameter int GPU_UV_W         = 32;   // Q16.16
parameter int GPU_CTX_ID_W     = 8;
parameter int GPU_WORK_ID_W    = 16;
parameter int GPU_MEM_TAG_W    = 12;
parameter int GPU_TEX_TAG_W    = 12;
parameter int GPU_EVENT_ID_W   = 12;
parameter int GPU_FAULT_CODE_W = 16;

parameter int GPU_LANES        = 1;
parameter int GPU_MEM_DATA_W   = 128;
```

说明：

- `GPU_LANES` 后续可选 1 / 2 / 4；
- `GPU_MEM_DATA_W=128` 是推荐内部 Memory Service 默认宽度；
- Platform Adapter 可桥接到实际 DDR/AXI 数据宽度。

---

# 5. 公共类型

## 5.1 Canonical Color

统一：

```text
RGBA8888 = 0xAARRGGBB
```

SystemVerilog：

```systemverilog
typedef logic [31:0] gpu_rgba_t;
```

---

## 5.2 Coordinate

```systemverilog
typedef logic signed [15:0] gpu_coord_t;
```

Render Front-End 可使用负坐标。

进入 Render Target 前必须完成 Surface/Clip 裁剪，保证实际写入坐标合法。

---

## 5.3 UV

```systemverilog
typedef logic signed [31:0] gpu_uv_t;
```

格式：

> signed Q16.16

---

## 5.4 IDs

```systemverilog
typedef logic [GPU_CTX_ID_W-1:0]  gpu_ctx_id_t;
typedef logic [GPU_WORK_ID_W-1:0] gpu_work_id_t;
```

---

# 6. Interface A：command_if

## 6.1 位置

```text
Command DMA
    ↓
command_if
    ↓
Command Parser
```

---

## 6.2 目的

传输从 DDR Command Ring 读取出的完整：

> **64 Byte Raw Command**

Parser 直接按照 GPU Command ISA V0.1 解析。

---

## 6.3 Payload

```systemverilog
typedef struct packed {
    logic [511:0] raw_cmd;
    logic [31:0]  fetch_addr;
    logic [15:0]  ring_index;
} gpu_command_pkt_t;
```

---

## 6.4 信号

```text
cmd_valid
cmd_ready
cmd_payload
```

---

## 6.5 顺序规则

Command DMA 必须严格按 Ring：

```text
HEAD
HEAD+1
HEAD+2
...
```

发送。

Parser 不允许重新排序。

---

## 6.6 Ring Index

`ring_index`：

- 用于 Fault；
- Trace；
- Debug；

不属于 ISA 命令内容。

---

## 6.7 command_if 错误

DDR Fetch Error 不应向 Parser 发送伪造命令。

Command DMA 应：

1. 发 `fault_if`
2. 停止进一步 Fetch
3. 等待 Fault Controller / Reset

---

# 7. Interface B：render_context_if

## 7.1 位置

```text
Command Parser / Descriptor Parser
          ↓
 render_context_if
          ↓
 Dispatcher / Render Pipeline Setup
```

---

## 7.2 目的

将 ISA Command 转换成：

> **Normalized Render Context**

后续模块无需再次理解 Command Binary Layout。

---

## 7.3 Context 生命周期

V0.1 规则：

1. Producer 先提交 `render_context_if`
2. Context 被接受
3. Producer/Dispatcher 提交对应 `work_if`
4. Context 在 Work Completion 前保持逻辑不变
5. `work_complete_if` 后该 `ctx_id` 可复用

---

## 7.4 V0.1 Context并发策略

接口支持多个 `ctx_id`。

但第一版实现允许：

> `ACTIVE_CONTEXTS = 1`

即：

```text
ctx_id = 0
```

未来可增加到 2 / 4 / 8，而不改变接口。

---

# 8. Render Context Payload

建议：

```systemverilog
typedef struct packed {
    // Identity
    gpu_ctx_id_t       ctx_id;
    logic [31:0]       sequence_id;
    logic [31:0]       user_tag;

    // Command / Front-end
    logic [3:0]        frontend_kind;
    logic [31:0]       feature_flags;

    // Source surface
    logic [31:0]       src_base;
    logic [31:0]       src_stride;
    logic [15:0]       src_width;
    logic [15:0]       src_height;
    logic [3:0]        src_format;
    logic [1:0]        addr_mode_u;
    logic [1:0]        addr_mode_v;
    logic [31:0]       palette_addr;

    // Destination surface
    logic [31:0]       dst_base;
    logic [31:0]       dst_stride;
    logic [15:0]       dst_width;
    logic [15:0]       dst_height;
    logic [3:0]        dst_format;

    // Source rectangle
    logic [15:0]       src_x;
    logic [15:0]       src_y;
    logic [15:0]       src_rect_w;
    logic [15:0]       src_rect_h;

    // Destination rectangle
    logic signed[15:0] dst_x;
    logic signed[15:0] dst_y;
    logic [15:0]       dst_rect_w;
    logic [15:0]       dst_rect_h;

    // Pixel state
    logic [3:0]        blend_mode;
    logic [1:0]        filter_mode;
    logic [7:0]        global_alpha;
    logic [23:0]       color_key_rgb;
    logic [31:0]       primary_color;

    // Clip
    logic signed[15:0] clip_xmin;
    logic signed[15:0] clip_ymin;
    logic signed[15:0] clip_xmax;
    logic signed[15:0] clip_ymax;

    // UV transform, Q16.16
    logic signed[31:0] u0;
    logic signed[31:0] v0;
    logic signed[31:0] du_dx;
    logic signed[31:0] dv_dx;
    logic signed[31:0] du_dy;
    logic signed[31:0] dv_dy;

    // Optional depth
    logic [31:0]       depth_base;
    logic [31:0]       depth_stride;
    logic [2:0]        depth_func;

    // Reserved for internal compatibility
    logic [28:0]       reserved;
} gpu_render_ctx_t;
```

---

# 9. frontend_kind Enum

| Value | 名称 |
|---:|---|
| `0x0` | `FE_FILL` |
| `0x1` | `FE_BLIT` |
| `0x2` | `FE_BLIT_EXT` |
| `0x3` | `FE_AFFINE` |
| `0x4` | `FE_VECTOR` |
| `0x5` | `FE_TRIANGLE` |
| `0x6-0xF` | Reserved |

---

# 10. feature_flags

Normalized feature flags 建议：

| Bit | 名称 |
|---:|---|
| 0 | `TEX_ENABLE` |
| 1 | `COLOR_KEY_ENABLE` |
| 2 | `GLOBAL_ALPHA_ENABLE` |
| 3 | `PIXEL_ALPHA_ENABLE` |
| 4 | `PALETTE_ENABLE` |
| 5 | `COLOR_MOD_ENABLE` |
| 6 | `DITHER_ENABLE` |
| 7 | `DEPTH_TEST_ENABLE` |
| 8 | `FLIP_X` |
| 9 | `FLIP_Y` |
| 10 | `CLIP_ENABLE` |
| 11 | `PREMULT_SRC` |
| 12 | `BILINEAR_ENABLE` |
| 13 | `TILE_MODE` |
| 14 | `STRICT_MODE` |
| 31:15 | Reserved |

---

# 11. Context Normalization 规则

Parser 必须把不同 ISA 命令归一化。

例如：

## FILL_RECT

```text
TEX_ENABLE = 0
primary_color = Fill Color
u/v = 0
```

## BLIT

```text
TEX_ENABLE = 1
filter = NEAREST
du_dx = +1.0 Q16.16
dv_dy = +1.0 Q16.16
```

## BLIT_EXT

使用 Extension 中的 UV。

这样：

> Render Front-End 不再需要知道原始 Command Word 位置。

---

# 12. Interface C：work_if

## 12.1 位置

```text
Dispatcher / Tile Scheduler
          ↓
       work_if
          ↓
Selected Render Front-End
```

---

## 12.2 目的

`render_context_if` 表示：

> “这个 Draw 是什么。”

`work_if` 表示：

> “现在执行这个 Draw 的哪一个有效区域。”

这使同一个 Draw Descriptor 可以在 Tile Mode 下针对不同 Tile 执行。

---

# 13. Work Payload

```systemverilog
typedef struct packed {
    gpu_work_id_t      work_id;
    gpu_ctx_id_t       ctx_id;

    logic [3:0]        frontend_kind;
    logic              tile_mode;

    logic signed[15:0] raster_xmin;
    logic signed[15:0] raster_ymin;
    logic signed[15:0] raster_xmax;
    logic signed[15:0] raster_ymax;

    logic [15:0]       tile_id;
    logic [15:0]       flags;
} gpu_work_t;
```

---

# 14. Raster Bounds

使用 half-open：

```text
[raster_xmin, raster_xmax)
[raster_ymin, raster_ymax)
```

Immediate：

\[
RasterBounds =
DrawRect \cap ClipRect \cap SurfaceRect
\]

Tile：

\[
RasterBounds =
DrawRect \cap ClipRect \cap SurfaceRect \cap TileRect
\]

---

# 15. tile_id

Immediate：

```text
tile_id = 0xFFFF
```

Tile Mode：

线性 Tile Index：

\[
tile\_id = tile_y \times grid_w + tile_x
\]

主要用于：

- Event；
- Trace；
- Debug。

---

# 16. work_id

每个实际执行 Work 分配唯一：

> 16-bit work_id

Tile Mode 下：

同一个 Draw Descriptor 被多个 Tile 引用时：

- `sequence_id` 可以相同；
- `user_tag` 可以相同；
- `work_id` 必须不同。

---

# 17. Interface D：work_complete_if

## 17.1 位置

```text
Render Target / Completion Tracker
          ↓
   work_complete_if
          ↓
Dispatcher / Tile Scheduler
```

---

## 17.2 目的

表示：

> 该 Work 的所有架构可见渲染效果已经达到本层定义的完成条件。

不是简单“Front-End已经产生完Fragment”。

---

## 17.3 Payload

```systemverilog
typedef struct packed {
    gpu_work_id_t work_id;
    gpu_ctx_id_t  ctx_id;
    logic [1:0]   status;
    logic [15:0]  reserved;
} gpu_work_complete_t;
```

---

## 17.4 status

| Value | 含义 |
|---:|---|
| `0` | OK |
| `1` | SKIPPED / NO-OP |
| `2` | FAULTED |
| `3` | Reserved |

---

# 18. Completion 语义

## Immediate

`work_complete` 只能在：

- 所有 Pixel 已通过 Pixel Back-End；
- 所有对应 Render Target Writes 已进入受控写队列；
- 不再存在该 Work 的未完成 Destination Read；

后产生。

Fence 会进一步保证 DDR 可见性。

---

## Tile

`work_complete` 表示：

- 当前 Work 对 Tile BRAM 的所有更新完成。

Tile Store 完成是：

> Tile Frame / Tile Scheduler 更高层完成条件

而不是每个 Work 的条件。

---

# 19. Front-End Local Done

允许 Front-End 内部存在：

```text
fe_generation_done
```

表示所有 Fragment 已生成。

但：

> 不作为 Command Retirement 依据。

---

# 20. Interface E：fragment_if

## 20.1 位置

```text
Render Front-End
      ↓
 fragment_if
      ↓
 Texture Unit
```

---

## 20.2 设计目标

统一支持：

- Fill；
- 1:1 Blit；
- Scaling；
- Affine；
- Triangle；
- Multi-Lane。

---

# 21. Fragment Vector Payload

每个 packet 包含 `GPU_LANES` 个 lane。

```systemverilog
typedef struct packed {
    gpu_ctx_id_t ctx_id;
    gpu_work_id_t work_id;

    logic [GPU_LANES-1:0] lane_mask;

    logic signed [GPU_LANES-1:0][15:0] x;
    logic signed [GPU_LANES-1:0][15:0] y;

    logic signed [GPU_LANES-1:0][31:0] u;
    logic signed [GPU_LANES-1:0][31:0] v;

    logic [GPU_LANES-1:0][31:0] vertex_color;
    logic [GPU_LANES-1:0][31:0] z;
    logic [GPU_LANES-1:0][7:0]  coverage;

    logic packet_first;
    logic packet_last;
} gpu_fragment_vec_t;
```

---

# 22. lane_mask 语义

`lane_mask[i] = 1`：

lane i 有效。

`lane_mask[i] = 0`：

该lane必须被所有下游模块忽略。

禁止：

```text
valid=1 && lane_mask=0
```

作为普通数据包发送。

例外：

> 不使用空packet表达End-of-Work。

End-of-Work 由 `packet_last` 附着在最后一个至少有一个有效lane的packet上。

对于零像素Work：

直接产生 `work_complete(status=SKIPPED)`，不发送 fragment。

---

# 23. packet_first / packet_last

用于：

- Debug；
- Pipeline Flush；
- Context Boundary检查。

要求：

- 每个非空 Work 第一包：`packet_first=1`
- 最后一包：`packet_last=1`

二者可以在单packet Work同时为1。

---

# 24. Fragment坐标约束

进入 `fragment_if` 的 X/Y：

- 必须已经满足 `work_if` Raster Bounds；
- Tile Mode 下必须落在当前 Tile Effective Bounds；
- 可以仍使用 signed16；
- Render Target前再次做防御性合法性检查。

---

# 25. Fill 的 Fragment

Fill：

```text
u = 0
v = 0
vertex_color = primary_color
```

Texture Unit根据：

```text
TEX_ENABLE=0
```

不产生 Texture Read。

---

# 26. Blit / Scale 的 Fragment

Blit：

```text
u/v = Q16.16
```

Texture Unit不关心其来自：

- 1:1；
- Scale；
- Flip；
- Affine。

---

# 27. Triangle Fragment

未来 Triangle：

```text
x/y
u/v
vertex_color
z
coverage
```

使用同一 `fragment_if`。

---

# 28. Interface F：texture_req_if

## 28.1 位置

```text
Texture Sampler / Address Generator
            ↓
      texture_req_if
            ↓
 Texture Cache / Raw Fetch Adapter
```

---

## 28.2 目的

此接口位于：

> Texture Sampling逻辑与底层纹理存储访问之间。

它描述：

> “请从某个物理地址取一个原始Texel元素。”

它不直接等同于 DDR Burst。

Texture Cache / Fetch Adapter 再将多个Texel请求转换成 Memory Service Burst。

---

# 29. Texture Request Payload

```systemverilog
typedef struct packed {
    logic [31:0]              addr;
    logic [1:0]               size;
    logic [3:0]               format;
    logic [GPU_TEX_TAG_W-1:0] tag;
} gpu_tex_req_t;
```

---

# 30. size

| Value | Byte数 |
|---:|---:|
| `0` | 1 Byte |
| `1` | 2 Byte |
| `2` | 4 Byte |
| `3` | Reserved |

典型：

- INDEX8 → 1；
- RGB565 → 2；
- ARGB8888 → 4。

---

# 31. Texture Tag

`tag` 是：

> Opaque Tag

Texture Fetch层不得解释。

只需在 Response 原样返回。

Sampler负责用 Tag 对应：

- Fragment；
- Lane；
- Bilinear Neighbor；
- Context。

---

# 32. Texture Ordering

Response：

> 允许 Out-of-Order。

因此所有 outstanding request 必须依赖 Tag 匹配。

第一版 Cache/Fetch 实现可以 In-Order，但接口不得依赖 In-Order。

---

# 33. Interface G：texture_rsp_if

Payload：

```systemverilog
typedef struct packed {
    logic [GPU_TEX_TAG_W-1:0] tag;
    logic [31:0]              data;
    logic [1:0]               err;
} gpu_tex_rsp_t;
```

---

## 33.1 data

Raw Texel：

- 1B / 2B / 4B 结果右对齐；
- 未使用高位清0。

---

## 33.2 err

| Value | 含义 |
|---:|---|
| `0` | OK |
| `1` | Memory Error |
| `2` | Unsupported |
| `3` | Reserved |

非0必须转为 `fault_if`。

---

# 34. Bilinear 对 Texture Interface 的要求

每目标像素最多需要：

> 4个 Raw Texel请求。

Sampler负责：

- 生成4个tex_req；
- 用tag关联；
- 等4个rsp；
- 完成插值。

Texture Cache无需理解 Bilinear 算法。

---

# 35. Palette 与 Texture Fetch

INDEX8：

```text
Texture Fetch
   ↓
8-bit Index
   ↓
Palette Unit
   ↓
RGBA8888
```

Palette不通过 `texture_req_if` 逐像素访问 DDR。

推荐：

> Palette预加载到小型RAM/Cache。

如未命中：

Palette subsystem自己通过 Memory Service refill。

---

# 36. Interface H：pixel_if

## 36.1 位置

```text
Texture Unit
    ↓
 pixel_if
    ↓
Pixel Back-End
```

---

## 36.2 目的

表示：

> 已完成 Texture Sampling / Palette / Source Format Decode 的 Canonical Source Pixel。

---

# 37. Pixel Vector Payload

```systemverilog
typedef struct packed {
    gpu_ctx_id_t  ctx_id;
    gpu_work_id_t work_id;

    logic [GPU_LANES-1:0] lane_mask;

    logic signed [GPU_LANES-1:0][15:0] x;
    logic signed [GPU_LANES-1:0][15:0] y;

    logic [GPU_LANES-1:0][31:0] src_rgba;
    logic [GPU_LANES-1:0][31:0] vertex_color;
    logic [GPU_LANES-1:0][31:0] z;
    logic [GPU_LANES-1:0][7:0]  coverage;

    logic packet_first;
    logic packet_last;
} gpu_pixel_vec_t;
```

---

# 38. Texture Unit 输出规则

## Texture Enable

```text
src_rgba = sampled texture
```

## Texture Disable

```text
src_rgba = vertex_color
```

因此 Fill 也走同一 `pixel_if`。

---

# 39. Pixel Back-End 处理顺序

`pixel_if` 后：

```text
Color Key
   ↓
Color Mod
   ↓
Alpha Combine
   ↓
Depth [optional]
   ↓
Destination Read
   ↓
Blend
   ↓
RT Write
```

---

# 40. Discard 规则

Color Key / Depth Test 失败：

对应 lane：

```text
lane_mask -> 0
```

后续不得发 Render Target Write。

如果整packet被Discard：

可以内部消除，不需要向 RT 发空packet。

但 Work Completion Tracker 仍必须正确处理 packet_last / outstanding计数。

---

# 41. Interface I：rt_read_req_if

## 41.1 位置

```text
Pixel Back-End
      ↓
 rt_read_req_if
      ↓
Render Target Adapter
```

用于：

- Straight Alpha；
- Additive；
- Multiply；
- ROP；
- 未来Depth关联逻辑。

Copy/Fully Opaque可绕过 Destination Read。

---

# 42. RT Read Request Payload

```systemverilog
typedef struct packed {
    gpu_ctx_id_t  ctx_id;
    gpu_work_id_t work_id;

    logic [GPU_LANES-1:0] lane_mask;

    logic [GPU_LANES-1:0][15:0] x;
    logic [GPU_LANES-1:0][15:0] y;

    logic [15:0] tag;
} gpu_rt_read_req_t;
```

---

# 43. RT Read坐标

在该接口处：

> 必须已经合法且非负。

即：

```text
0 <= x < dst_width
0 <= y < dst_height
```

违反视为内部错误：

`FAULT_INTERNAL_RT_COORD`

---

# 44. RT Read Tag

Pixel Back-End 分配：

> 16-bit opaque tag

Render Target Adapter在 Response原样返回。

允许多个 outstanding RT Read。

---

# 45. Interface J：rt_read_rsp_if

Payload：

```systemverilog
typedef struct packed {
    gpu_ctx_id_t  ctx_id;
    gpu_work_id_t work_id;

    logic [GPU_LANES-1:0] lane_mask;
    logic [GPU_LANES-1:0][31:0] dst_rgba;

    logic [15:0] tag;
    logic [1:0]  err;
} gpu_rt_read_rsp_t;
```

---

# 46. Render Target Read统一颜色格式

无论真实 Target 是：

- RGB565 DDR；
- ARGB8888 DDR；
- RGBA8888 Tile BRAM；

Response一律：

> **RGBA8888**

Format Conversion由 Render Target Adapter完成。

---

# 47. Interface K：rt_write_if

## 47.1 位置

```text
Pixel Back-End
      ↓
  rt_write_if
      ↓
Render Target Adapter
```

---

## 47.2 Payload

```systemverilog
typedef struct packed {
    gpu_ctx_id_t  ctx_id;
    gpu_work_id_t work_id;

    logic [GPU_LANES-1:0] lane_mask;

    logic [GPU_LANES-1:0][15:0] x;
    logic [GPU_LANES-1:0][15:0] y;
    logic [GPU_LANES-1:0][31:0] rgba;

    logic packet_last_for_work;
} gpu_rt_write_t;
```

---

# 48. RT Write语义

`rt_write_if` 被接受：

表示：

> Render Target Subsystem 已取得该写事务所有权。

不一定表示：

> DDR已经完成写回。

Immediate模式：

- 可能进入Write FIFO；
- Fence负责最终Drain。

Tile模式：

- 通常已写入Tile BRAM；
- Tile Store在Tile级别后续完成。

---

# 49. packet_last_for_work

用于 Completion Tracker 判断：

> 此 Work 已无新的 Pixel Write。

但不能单独作为完成条件。

必须同时满足：

- RT Read outstanding = 0；
- Blend pipeline为空；
- RT Write accepted；
- 必要内部写队列完成到定义边界。

---

# 50. Render Target Adapter抽象

Pixel Back-End完全不区分：

### Immediate Adapter
后端是 DDR。

### Tile Adapter
后端是 BRAM Tile。

二者对上均实现：

- `rt_read_req_if`
- `rt_read_rsp_if`
- `rt_write_if`
- Completion接口。

---

# 51. Tile Adapter局部地址

Tile Adapter收到全局 X/Y 后：

\[
local_x = x - tile_x0
\]

\[
local_y = y - tile_y0
\]

上游不需要知道Tile BRAM物理组织。

---

# 52. Interface L：memory_req_if

## 52.1 位置

所有高带宽 Client：

```text
Command
Texture
Immediate RT
Tile Load/Store
Display
Depth
Vertex
Debug
    ↓
memory_req_if
    ↓
Central Memory Service
```

---

# 53. Memory Service 设计原则

内部 Memory接口：

> 不直接复制 AXI。

目的：

- 与平台总线解耦；
- 更容易仿真；
- 更容易替换 Efinix 平台 Memory Adapter。

---

# 54. Memory Request Header

```systemverilog
typedef struct packed {
    logic                     write;
    logic [3:0]               client_id;
    logic [31:0]              addr;
    logic [15:0]              byte_count;
    logic [GPU_MEM_TAG_W-1:0] tag;
    logic [7:0]               flags;
} gpu_mem_req_t;
```

---

# 55. Memory Request语义

一个 Header 描述一个逻辑连续 Byte Range：

```text
[addr, addr + byte_count)
```

Memory Service负责：

- Alignment；
- Platform Burst；
- 4KB Boundary Split；
- Beat packing；
- Backpressure。

上层Client不需要关心AXI burst length。

---

# 56. byte_count

16-bit：

```text
1 ... 65535
```

实际实现可限制最大：

例如：

```text
4096 Bytes
```

限制值通过：

> Internal Capability Parameter

定义。

---

# 57. client_id

V0.1 建议：

| ID | Client |
|---:|---|
| `0x0` | CMD |
| `0x1` | TEXTURE |
| `0x2` | RT_IMMEDIATE |
| `0x3` | TILE |
| `0x4` | DEPTH |
| `0x5` | VERTEX |
| `0x6` | DISPLAY |
| `0x7` | DEBUG |
| `0x8-0xF` | Reserved |

---

# 58. Memory Request flags

建议：

| Bit | 名称 |
|---:|---|
| 0 | `PREFETCH` |
| 1 | `NO_CACHE` |
| 2 | `HIGH_PRIORITY` |
| 3 | `STREAMING` |
| 7:4 | Reserved |

Memory Service可以将flags作为QoS Hint。

---

# 59. Memory Write Data Channel

写事务使用独立：

> `mem_wdata_if`

```systemverilog
typedef struct packed {
    logic [GPU_MEM_DATA_W-1:0]   data;
    logic [GPU_MEM_DATA_W/8-1:0] strb;
    logic                        last;
} gpu_mem_wdata_t;
```

---

# 60. Write顺序

V0.1规定：

> 全局内部接口不支持不同Write Transaction的WData交错。

即：

1. 某Write Header被grant；
2. 该Transaction的所有WData连续传输；
3. `last=1`
4. 才切换到另一个Write Transaction。

这样显著简化 Arbiter。

---

# 61. Memory Read Response

`memory_rsp_if` 的 Read Data：

```systemverilog
typedef struct packed {
    logic [GPU_MEM_DATA_W-1:0] data;
    logic [GPU_MEM_TAG_W-1:0]  tag;
    logic [3:0]                client_id;
    logic                      last;
    logic [1:0]                err;
} gpu_mem_rdata_t;
```

---

# 62. Memory Write Completion

单独：

```systemverilog
typedef struct packed {
    logic [GPU_MEM_TAG_W-1:0] tag;
    logic [3:0]               client_id;
    logic [1:0]               err;
} gpu_mem_wresp_t;
```

---

# 63. Read Ordering

不同 Tag：

> 允许Out-of-Order Response。

同一Tag内部：

> Beat顺序必须保持。

V0.1 Platform Adapter可以先实现全局In-Order，但Client不得依赖跨Tag顺序。

---

# 64. Memory Tag

由Client分配。

Memory Service/Adapter：

> 原样返回。

Client负责保证其 outstanding 范围内 Tag 不冲突。

---

# 65. Memory Data Width

V0.1 推荐内部：

> **128 bit**

即：

```text
16 Byte / Beat
```

原因：

- 适合Burst；
- 对RGB565/RGBA8888打包友好；
- 不强制平台DDR物理总线同宽。

如果实际平台更适合64/256bit：

只修改参数和Adapter。

---

# 66. Partial Write

通过：

```text
strb
```

支持。

Immediate RGB565边界写等可使用 Byte Enable。

但性能路径应尽量做：

> Burst Coalescing

减少大量Partial Write。

---

# 67. Interface M：event_if

## 67.1 位置

所有模块：

```text
Module
  ↓
event_if
  ↓
Central Performance Monitor
```

---

## 67.2 特性

Performance Event：

> **不得反压GPU主数据流。**

因此：

`event_if` 不使用 ready。

---

# 68. Event Payload

```systemverilog
typedef struct packed {
    logic                    valid;
    logic [5:0]              source_id;
    logic [GPU_EVENT_ID_W-1:0] event_id;
    logic [31:0]             value;
    logic [31:0]             aux;
} gpu_event_t;
```

---

# 69. Event语义

当：

```text
valid = 1
```

Performance Monitor在该周期采样。

`value`：

- 默认增量；
- 常用为1；
- DDR Bytes可以一次上报16/32/64等。

`aux`：

- 可携带 tile_id；
- work_id；
- stall_reason。

---

# 70. 多Event同周期

如果同一模块一个周期可能产生多个独立Event：

必须：

- 实例化多个 `event_if`；
- 或模块内部先聚合成Counter增量。

不允许因 Event Channel 忙而 Stall 主数据流。

---

# 71. Event Source ID

建议：

| ID | Module |
|---:|---|
| 0 | CMD |
| 1 | FRONTEND |
| 2 | TEXTURE |
| 3 | PIXEL |
| 4 | RT |
| 5 | TILE |
| 6 | MEMORY |
| 7 | DISPLAY |
| 8 | CACHE |
| 9 | DEBUG |
| others | Reserved |

---

# 72. Event ID建议

示例：

```text
EV_CMD_ACCEPT
EV_CMD_STALL
EV_PIXEL_GENERATED
EV_PIXEL_BLEND
EV_PIXEL_DISCARD_KEY
EV_PIXEL_SCALE
EV_DDR_READ_BYTES
EV_DDR_WRITE_BYTES
EV_DDR_STALL
EV_TILE_LOAD
EV_TILE_STORE
EV_TILE_WORK
EV_CACHE_HIT
EV_CACHE_MISS
EV_DISPLAY_UNDERFLOW
```

精确数值后续在 Performance Event Map 冻结。

---

# 73. Interface N：fault_if

## 73.1 特性

Fault同样：

> 不允许等待 ready 导致 GPU 死锁。

采用：

> pulse + sticky capture

---

# 74. Fault Payload

```systemverilog
typedef struct packed {
    logic                       valid;
    logic [1:0]                 severity;
    logic [GPU_FAULT_CODE_W-1:0] code;

    logic [31:0]                address;
    logic [31:0]                sequence_id;
    logic [31:0]                user_tag;
    logic [31:0]                info;
} gpu_fault_t;
```

---

# 75. severity

| Value | 名称 |
|---:|---|
| 0 | INFO |
| 1 | WARNING |
| 2 | RECOVERABLE |
| 3 | FATAL |

---

# 76. Fault Controller行为

V0.1 推荐：

- 第一个 `FATAL` Fault：锁存完整信息；
- GPU进入 Halt；
- 后续Fault可累计Lost Fault Counter；
- `WARNING` 可只计数；
- Display尽量继续工作。

---

# 77. 同周期多个Fault

每个模块可各有一个 `fault_if`。

Fault Controller使用优先级：

```text
FATAL > RECOVERABLE > WARNING > INFO
```

若同严重度：

预定义Module Priority。

同时增加：

```text
FAULT_DROPPED_COUNT
```

用于调试。

---

# 78. Interface O：control_command_if

非Draw命令不经过 Render Front-End。

Parser/Dispatcher可使用内部：

```text
control_command_if
```

目标：

- PRESENT；
- FENCE；
- BARRIER；
- RESET_STATS；
- PERF_SNAPSHOT；
- TRACE_MARKER。

---

# 79. Control Command Payload

建议：

```systemverilog
typedef struct packed {
    logic [3:0]   cmd_class;
    logic [7:0]   opcode;

    logic [31:0]  sequence_id;
    logic [31:0]  user_tag;

    logic [383:0] payload; // Base Command W4-W15
} gpu_control_cmd_t;
```

---

# 80. 为什么保留 control_command_if

这样：

Render Pipeline只处理Draw。

Display / Sync / Perf：

直接接收控制命令。

避免把：

`PRESENT`

伪装成 Render Context。

---

# 81. Interface P：tile_work_if

Tile Scheduler内部可选专用接口。

位置：

```text
Tile Header / WorkList Engine
          ↓
      tile_work_if
          ↓
Descriptor Fetch / Parser
```

---

# 82. Tile Work Payload

```systemverilog
typedef struct packed {
    logic [31:0] draw_desc_base;
    logic [31:0] draw_index;

    logic [15:0] tile_id;

    logic signed[15:0] tile_xmin;
    logic signed[15:0] tile_ymin;
    logic signed[15:0] tile_xmax;
    logic signed[15:0] tile_ymax;
} gpu_tile_work_t;
```

---

# 83. Tile Work执行

Descriptor Parser：

1. 地址：

\[
draw\_desc\_base + draw\_index \times 64
\]

2. Fetch 64B；
3. Parse；
4. 生成 Render Context；
5. 与Tile Bounds求交；
6. 生成 `work_if`。

---

# 84. Tile Target Context

`TILE_FRAME` 建立：

> Tile Frame Context

包含：

- DST Base；
- DST Stride；
- Surface W/H；
- DST Format；
- Tile W/H；
- Grid W/H；
- Clear Color；
- Optional Depth。

Tile Frame Context在整个 TILE_FRAME 执行期间稳定。

---

# 85. Interface Q：tile_control_if

建议 Tile Frame Dispatcher → Tile Scheduler：

```systemverilog
typedef struct packed {
    logic [31:0] draw_desc_base;
    logic [31:0] tile_header_base;
    logic [31:0] work_list_base;

    logic [31:0] dst_base;
    logic [31:0] dst_stride;
    logic [15:0] surface_w;
    logic [15:0] surface_h;
    logic [3:0]  dst_format;

    logic [15:0] grid_w;
    logic [15:0] grid_h;
    logic [15:0] tile_w;
    logic [15:0] tile_h;

    logic [31:0] clear_color;

    logic [31:0] depth_base;
    logic [31:0] depth_stride;

    logic [31:0] sequence_id;
} gpu_tile_frame_ctx_t;
```

---

# 86. Tile Frame Completion

Tile Scheduler只有在：

- 所有 Tile完成；
- 所有要求的 Tile Store完成；
- Depth Store完成；
- 无 outstanding Tile Memory Transaction；

后才能向 Command Retirement产生：

> TILE_FRAME complete

---

# 87. Interface R：display_present_if

位置：

```text
Command Dispatcher
      ↓
display_present_if
      ↓
Display Engine
```

---

# 88. Present Payload

```systemverilog
typedef struct packed {
    logic [31:0] surface_base;
    logic [31:0] surface_stride;
    logic [15:0] surface_w;
    logic [15:0] surface_h;
    logic [3:0]  surface_format;

    logic [31:0] frame_id;
    logic [31:0] present_token;
    logic [3:0]  present_mode;
    logic        irq_on_flip;
    logic        replace_pending;
} gpu_present_t;
```

---

# 89. display_present_if Completion

Display Engine提供独立：

```text
present_done_if
```

Payload：

```systemverilog
typedef struct packed {
    logic [31:0] frame_id;
    logic [31:0] present_token;
    logic [1:0]  status;
} gpu_present_done_t;
```

它表示：

> 真正VSYNC Flip完成。

不是Command Processor的普通 retire。

---

# 90. Interface S：fence_if

Fence/Barrier由 Sync Unit处理。

推荐内部请求：

```systemverilog
typedef struct packed {
    logic [31:0] fence_value;
    logic [31:0] writeback_addr;
    logic [7:0]  flags;
    logic [31:0] sequence_id;
} gpu_fence_req_t;
```

---

# 91. Fence依赖的Drain信号

Sync Unit需要从子系统获得：

```text
cmd_idle
render_idle
rt_write_idle
memory_write_idle
tile_idle
```

具体不建议使用散乱组合线。

推荐：

> `activity_status_if`

---

# 92. activity_status_if

每个主要Subsystem输出：

```systemverilog
typedef struct packed {
    logic busy;
    logic [15:0] outstanding;
} gpu_activity_status_t;
```

Sync Unit根据：

- required subsystem；
- Fence Flags；

判断是否可完成Fence。

---

# 93. Interface T：irq_event_if

内部模块不直接驱动 CPU IRQ。

统一向 IRQ Controller提交：

```systemverilog
typedef struct packed {
    logic        valid;
    logic [7:0]  irq_source;
    logic [31:0] data;
} gpu_irq_event_t;
```

IRQ Controller：

- Pending；
- Mask；
- Clear；
- CPU IRQ输出。

---

# 94. IRQ Source建议

```text
IRQ_CMD_RETIRE
IRQ_FENCE_DONE
IRQ_PRESENT_DONE
IRQ_FAULT
IRQ_DISPLAY_UNDERFLOW
IRQ_PERF
```

---

# 95. Context ID 管理

V0.1定义：

- `ctx_id` 为内部ID；
- 不等于 ISA `sequence_id`；
- 不暴露给软件；
- 生命周期仅在内部。

---

# 96. 默认单Context实现

初始 RTL：

```text
ACTIVE_CONTEXTS = 1
ctx_id = 0
```

Dispatcher只有在：

`work_complete`

后才接受下一 Render Context。

---

# 97. 后续多Context扩展

未来可以：

- Context RAM；
- 2/4/8 Context Slot；
- 多 outstanding Texture；
- Pipeline overlap。

因为所有像素/RT接口已经携带：

`ctx_id`

无需改协议。

---

# 98. Context访问方式

V0.1推荐：

> 各主要模块在 `render_context_if` transfer 时锁存本模块所需字段。

例如：

### Texture Unit锁存
- src_base；
- stride；
- format；
- palette；
- filter；
- address mode。

### Pixel Backend锁存
- blend；
- alpha；
- key；
- dst format。

### RT Adapter锁存
- dst surface。

未来多Context时：

改为 Context Table indexed by `ctx_id`。

接口不变。

---

# 99. Packet与Context一致性检查

Strict Debug Build下：

如果模块收到：

```text
packet.ctx_id != active_ctx_id
```

必须：

`FAULT_CONTEXT_MISMATCH`

Release Build可选择关闭部分断言逻辑。

---

# 100. Backpressure路径设计

推荐在以下边界必须有FIFO/Skid：

```text
Command DMA → Parser
Front-End → Texture
Texture → Pixel
Pixel → RT
Memory Client → Memory Service
Memory Service → Platform Adapter
```

目标：

> 不让DDR Backpressure通过组合路径一路传到 Front-End。

---

# 101. 推荐FIFO深度原则

本规格不冻结具体值，但建议初始：

| 边界 | 初始建议 |
|---|---:|
| Command DMA → Parser | 2–4 commands |
| Fragment FIFO | 4–16 packets |
| Texture Request Queue | 16+ |
| Texture Response Queue | 16+ |
| Pixel FIFO | 4–16 packets |
| RT Write FIFO | 16+ |
| Memory Client Queue | 4–16 requests |

实际通过仿真/综合调整。

---

# 102. 组合逻辑规则

禁止把：

```text
DDR ready
```

经过：

```text
Memory
→ RT
→ Pixel
→ Texture
→ FrontEnd
```

形成一个长组合链。

至少在两个以上子系统边界注册。

---

# 103. Lane语义

V0.1支持：

```text
GPU_LANES = 1 / 2 / 4
```

所有 Vector Interface：

- lane顺序按 +X方向；
- `lane0` 对应最小X；
- lane i 默认：

\[
x_i = x_0 + i
\]

特殊 Front-End可以生成不连续Lane，但 Competition 2D路径应保持连续。

---

# 104. 行尾处理

若剩余像素不足LANES：

例如：

```text
LANES=4
remaining=2
```

则：

```text
lane_mask = 4'b0011
```

无效lane payload可Don't Care，但仿真推荐清0。

---

# 105. Lane与Bilinear

每个Lane独立采样。

因此 `LANES=4` + Bilinear：

理论最多需要：

```text
16 Raw Texel Requests / packet
```

Texture Unit必须通过：

- request scheduling；
- cache；
- buffering；

处理。

这也是最终是否采用4 Lane的重要建模依据。

---

# 106. Lane与Tile BRAM Banking

Render Target接口按Vector定义。

Tile Buffer内部必须在多Lane版本处理：

- Bank Conflict；
- Multi-port；
- Replication / Banking。

该问题属于微架构，不修改 `rt_*` 接口。

---

# 107. 错误传播原则

错误分三类。

## A. Protocol Error

例如：

- invalid lane；
- ctx mismatch；
- illegal tag response。

产生：

> FATAL / INTERNAL fault

---

## B. Data/Memory Error

例如：

- DDR response error。

产生：

> FATAL，停止当前GPU Work。

---

## C. Unsupported Feature

Command Parser阶段产生：

> ISA Fault

不应进入Render Pipeline。

---

# 108. Timeout

建议 Debug Build为：

- Memory Transaction；
- Work Completion；
- Tile Load/Store；

提供可配置 watchdog。

超时：

`FAULT_INTERNAL_TIMEOUT`

Competition Release可保留较大阈值。

---

# 109. Interface断言

每个 valid/ready interface推荐加入 SystemVerilog Assertions。

---

## 109.1 Stable Under Stall

```systemverilog
assert property (
    valid && !ready |=> $stable(payload)
);
```

---

## 109.2 No X on Valid

```systemverilog
assert property (
    valid |-> !$isunknown(payload)
);
```

---

## 109.3 Lane Mask

```systemverilog
assert property (
    valid |-> (lane_mask != '0)
);
```

适用于 fragment/pixel/write packet。

---

# 110. Tag一致性断言

Response Tag：

必须匹配某个 outstanding request。

否则：

`FAULT_TAG_MISMATCH`

---

# 111. Work生命周期断言

对于同一个 `work_id`：

```text
work_accept
   ↓
0 or more fragments
   ↓
work_complete
```

禁止：

- duplicate complete；
- complete before accept；
- fragment after complete。

---

# 112. Context生命周期断言

对于同一个 `ctx_id`：

```text
context_accept
    ↓
work_accept
    ↓
work_complete
    ↓
ctx_id reusable
```

---

# 113. Tile Draw Order

Tile Scheduler必须保持：

```text
WorkRef[0]
WorkRef[1]
WorkRef[2]
...
```

顺序。

不得在同Tile内按Texture/Blend重排。

---

# 114. Memory Barrier内部要求

执行 GPU `MEMORY_BARRIER` 时：

根据 flags：

- drain selected writes；
- invalidate Texture Cache；
- invalidate Palette Cache；
- invalidate Descriptor Cache。

Cache模块应暴露内部：

```text
invalidate_req
invalidate_done
```

这种管理接口可以是点对点控制信号，不要求走主stream。

---

# 115. Cache Maintenance 接口

建议统一：

```systemverilog
typedef struct packed {
    logic        invalidate_all;
    logic        flush_all;
    logic [31:0] base;
    logic [31:0] length;
} gpu_cache_maint_req_t;
```

V0.1可以只实现：

`invalidate_all`

---

# 116. Tile Load/Store Memory访问

Tile Adapter：

### Load
使用 `MEM_CLIENT_TILE`

### Store
使用 `MEM_CLIENT_TILE`

必须：

- 生成Burst；
- 处理Surface边缘Partial Tile；
- RGB565 ↔ RGBA8888转换。

---

# 117. Immediate RT Memory访问

Immediate Adapter：

- Copy opaque write可直接write；
- Alpha/Additive需要read-modify-write；
- 应利用Write FIFO和Burst Coalescing。

对 Pixel Back-End仍保持统一 `rt_*` 接口。

---

# 118. Display Memory Client

Display Scanout：

`MEM_CLIENT_DISPLAY`

Memory Service QoS：

> 最高实时优先级。

Display client必须有：

- Scanout FIFO；
- Low Watermark；
- Underflow Event/Fault。

---

# 119. Display与GPU写同一Surface

Driver必须遵守：

- Front Buffer：Display读；
- Back Buffer：GPU写。

正常Double Buffer不允许同时GPU写Front。

若违反：

硬件不保证无撕裂。

---

# 120. OSD接口

OSD不进入主Pixel Backend。

推荐 Display Compositor内部接口：

```text
base_pixel
osd_pixel
cursor_pixel
      ↓
compositor
```

OSD实现细节不属于本GPU Render内部接口。

---

# 121. Performance Event不得改变功能结果

即使：

- Perf Monitor关闭；
- Event被丢弃；
- Trace Buffer满；

GPU图像结果必须保持一致。

---

# 122. Trace接口

Trace属于Debug增强。

推荐各模块通过：

`event_if`

上报Trace Marker。

Central Trace模块根据：

- source；
- event_id；
- trace enable；

决定是否记录。

避免每个模块各自实现Trace RAM。

---

# 123. SystemVerilog Interface建议

如果工具链对SV Interface支持良好，可以定义：

```systemverilog
interface gpu_stream_if #(type T = logic [31:0]) (
    input logic clk,
    input logic rst_n
);
    logic valid;
    logic ready;
    T payload;

    modport producer (
        output valid,
        output payload,
        input  ready
    );

    modport consumer (
        input  valid,
        input  payload,
        output ready
    );
endinterface
```

---

# 124. 工具链兼容策略

如果 Efinix 工具对复杂 `interface/type parameter` 支持不理想：

> 保留本文协议语义，但展开为普通端口。

例如：

```text
frag_valid
frag_ready
frag_ctx_id
frag_x0
...
```

不得因工具限制改变协议行为。

---

# 125. 推荐 gpu_pkg.sv 内容

```text
gpu_pkg.sv
│
├─ parameters
├─ enums
├─ typedef rgba
├─ typedef command packet
├─ typedef render context
├─ typedef work
├─ typedef fragment vector
├─ typedef pixel vector
├─ typedef tex req/rsp
├─ typedef rt req/rsp
├─ typedef mem req/rsp
├─ typedef event
└─ typedef fault
```

---

# 126. 推荐 enum

```systemverilog
typedef enum logic [3:0] {
    FE_FILL      = 4'h0,
    FE_BLIT      = 4'h1,
    FE_BLIT_EXT  = 4'h2,
    FE_AFFINE    = 4'h3,
    FE_VECTOR    = 4'h4,
    FE_TRIANGLE  = 4'h5
} gpu_frontend_kind_e;
```

以及：

- pixel_format_e
- blend_mode_e
- filter_mode_e
- addr_mode_e
- depth_func_e
- mem_client_e
- fault_severity_e

具体枚举值必须与 Command ISA 保持一致时，应从同一个 package/header 生成。

---

# 127. 软件/RTL共享常量

为了避免：

C Header与SystemVerilog常量漂移，

推荐单一源生成：

```text
spec/constants.yaml
        │
        ├── gpu_isa.h
        ├── gpu_isa_pkg.sv
        └── golden_constants.hpp
```

如果暂时不做生成器：

必须至少维护自动一致性测试。

---

# 128. Internal Interface与Golden Model的关系

Golden Model不需要模拟valid/ready时序。

但必须共享：

- Render Context字段；
- Work Bounds；
- Fragment语义；
- Pixel Format；
- UV；
- Blend；
- Tile顺序。

Cycle Model以后可以额外模拟：

- Queue；
- Stall；
- DDR；
- Lane。

---

# 129. RTL Unit Test边界

接口冻结后，模块可独立验证。

---

## Command Parser

输入：

`command_if`

输出：

- `render_context_if`
- `control_command_if`

---

## 2D Front-End

输入：

- `render_context_if`
- `work_if`

输出：

`fragment_if`

---

## Texture Unit

输入：

- Context；
- `fragment_if`

输出：

`pixel_if`

Mock：

- `texture_req/rsp`

---

## Pixel Backend

输入：

- Context；
- `pixel_if`
- `rt_read_rsp_if`

输出：

- `rt_read_req_if`
- `rt_write_if`

---

## Tile Adapter

输入：

- RT接口；
- Tile Control

输出：

- Memory接口；
- Work Completion。

---

# 130. 推荐Mock模块

验证时准备：

```text
mock_memory
mock_texture_fetch
mock_render_target
mock_event_sink
mock_fault_sink
```

这样一个模块不需要等整个GPU完成。

---

# 131. Interface版本策略

本文接口定义：

> **Internal Interface Version = 0.1**

内部接口不需要软件兼容，但项目开发中仍遵守：

- 小版本增加字段优先使用 Reserved；
- 已冻结字段尽量不改语义；
- 大改必须升级文档版本并写 Migration Note。

---

# 132. V0.1 已冻结接口

以下进入V0.1 Baseline：

- Valid/Ready语义；
- Stall稳定规则；
- `command_if`；
- `render_context_if`；
- `work_if`；
- `work_complete_if`；
- Vector `fragment_if`；
- Raw Texel `texture_req_if`；
- `texture_rsp_if`允许Out-of-Order；
- Canonical `pixel_if`；
- `rt_read_req/rsp`；
- `rt_write_if`；
- Central `memory_req` + `mem_wdata` + `mem_rdata` + `mem_wresp`；
- `event_if`无Backpressure；
- `fault_if`无Backpressure；
- `control_command_if`；
- Tile Work / Tile Control接口；
- Present/Present Done；
- Context ID / Work ID；
- Clock Domain边界；
- 默认内部Memory Data Width 128bit；
- 参数化GPU_LANES；
- 默认ACTIVE_CONTEXTS=1但接口支持扩展。

---

# 133. V0.1 尚未冻结的微架构项

以下可在不破坏接口的前提下后续调整：

- FIFO深度；
- GPU_LANES最终值；
- ACTIVE_CONTEXTS最终值；
- Texture Outstanding数量；
- RT Read Outstanding数量；
- Memory Outstanding数量；
- MEM_DATA_W是否改64/256；
- Context RAM实现；
- Cache实现；
- Tile BRAM Banking；
- Arbiter算法；
- Pipeline级数；
- Trace深度。

---

# 134. 第一版 RTL 推荐配置

为了降低Bring-up风险：

```text
GPU_LANES          = 1
ACTIVE_CONTEXTS    = 1
ENABLE_TILE        = 0
ENABLE_BILINEAR    = 0
ENABLE_TEX_CACHE   = 0
ENABLE_AFFINE      = 0
ENABLE_TRIANGLE    = 0
```

仍然使用本文全部接口。

---

# 135. Competition配置目标

后续：

```text
GPU_LANES          = 1 or 2 or 4   // 建模决定
ACTIVE_CONTEXTS    >= 1
ENABLE_TILE        = 1
ENABLE_BILINEAR    = 1
ENABLE_INDEXED8    = 1
ENABLE_AFFINE      = optional
```

接口无需重写。

---

# 136. 典型 FILL 数据流

```text
Command DMA
  │ command_if
  ▼
Parser
  │ render_context_if
  ▼
Dispatcher
  │ work_if
  ▼
2D Front-End
  │ fragment_if
  ▼
Texture Unit
  │ TEX_ENABLE=0
  │ pixel_if
  ▼
Pixel Backend
  │ rt_write_if
  ▼
Immediate RT
  │ memory_req/wdata
  ▼
Memory Service
```

---

# 137. 典型 Alpha BLIT 数据流

```text
Front-End
  │ fragment_if
  ▼
Texture Unit
  │ texture_req/rsp
  ▼
pixel_if(src_rgba)
  ▼
Pixel Backend
  │
  ├── rt_read_req
  │       ↓
  │    RT Adapter
  │       ↓
  │   rt_read_rsp(dst_rgba)
  │
  ├── Blend
  │
  └── rt_write_if
          ↓
       RT Adapter
```

---

# 138. 典型 Tile 数据流

```text
TILE_FRAME
   ↓
tile_control_if
   ↓
Tile Scheduler
   ↓
Tile Load
   ↓
tile_work_if
   ↓
Descriptor Parser
   ↓
render_context_if
   ↓
work_if(tile bounds)
   ↓
Frontend → Texture → Pixel
   ↓
rt_write_if
   ↓
Tile Adapter / BRAM
   ↓
work_complete
   ↓
Next Work
   ↓
Tile Store
```

---

# 139. 典型 Bilinear 数据流

```text
fragment_if(u,v)
     ↓
Sampler
     ├── tex_req P00
     ├── tex_req P10
     ├── tex_req P01
     └── tex_req P11
             ↓
        Cache/Fetch
             ↓
        tex_rsp(tag)
             ↓
      Sampler Scoreboard
             ↓
      Bilinear Interpolate
             ↓
         pixel_if
```

---

# 140. 典型 Affine扩展

只新增：

```text
Affine Front-End
```

它仍输出同一个：

`fragment_if`

因此：

- Texture；
- Pixel；
- RT；
- Memory；

接口完全不变。

---

# 141. 典型 Triangle扩展

Triangle Front-End 输出：

```text
fragment_if:
x/y
u/v
color
z
coverage
```

启用：

```text
DEPTH_TEST_ENABLE
```

Pixel/RT Tile Depth模块扩展，但主stream协议不变。

---

# 142. 开发者接口检查表

新增RTL模块前必须回答：

- [ ] 模块属于哪个Clock Domain？
- [ ] 输入是否Valid/Ready？
- [ ] Stall时Payload是否稳定？
- [ ] 是否可能产生Backpressure？
- [ ] 是否需要FIFO？
- [ ] 是否携带ctx_id？
- [ ] 是否携带work_id？
- [ ] 是否需要Tag关联？
- [ ] 是否会Out-of-Order？
- [ ] 如何报告Perf Event？
- [ ] 如何报告Fault？
- [ ] Reset后输出是什么？
- [ ] Feature关闭时如何Bypass？
- [ ] 是否能被独立Mock/Test？

---

# 143. Interface Review Gate

进入模块RTL开发前，针对每个接口做一次 Review。

至少检查：

### Function
字段是否够。

### Expansion
未来Scaling/Tile/3D是否兼容。

### Timing
是否形成长组合路径。

### Verification
是否容易写Mock和Assertion。

### Ordering
是否定义清楚。

### Error
是否定义Fault路径。

---

# 144. 本文与后续文档关系

下一份：

> `Pixel_Format_Arithmetic_Specification_V0.1`

将进一步冻结：

- RGBA/RGB565转换；
- Alpha；
- Color Mod；
- Additive；
- Q16.16；
- Nearest；
- Bilinear；
- Rounding；
- Saturation。

其结果直接决定：

- Texture Unit；
- Pixel Backend；
- Golden Model。

---

# 145. V0.1 一句话定义

> **Internal Interface V0.1 使用统一 Valid/Ready 流协议，将 Command、Normalized Render Context、Work、Vector Fragment、Texture Fetch、Canonical Pixel、Render Target、Central Memory、Performance Event 和 Fault 拆分为稳定模块边界，并通过 ctx_id / work_id / tag 支持未来多上下文、Multi-Lane、Tile、Affine 与 Triangle 扩展，使首版单Lane基础GPU和后续竞赛完整GPU能够共享同一套内部通信协议。**
