# RISC-V–FPGA 通用 2D GPU
# GPU Command ISA Specification V0.1

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：GPU Command ISA / Command Ring / Draw Descriptor 规格  
> 版本：V0.1  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> 状态：ISA Baseline Freeze Candidate

---

# 0. 文档目的

本文档定义 CPU/RISC-V 与 FPGA GPU 之间的命令级软件/硬件接口，包括：

- 64 Byte Base Command 格式；
- Command Ring 语义；
- Opcode Namespace；
- 2D Draw Descriptor；
- FILL / BLIT / BLIT_EXT；
- Tile Frame；
- Present；
- Fence / Barrier；
- Performance / Trace；
- 扩展描述符；
- Capability / Fault 行为；
- Tile Work List；
- 未来 2.5D / Vector / 3D 命令空间。

本 ISA 的目标不是只满足当前 BitBlt，而是确保未来增加：

- Per-Pixel Alpha；
- Scaling；
- Bilinear；
- Palette；
- Texture Cache；
- Affine / Mode-7；
- Vector；
- Triangle；
- Z-Buffer；

时，不需要重新设计 CPU/GPU Command Ring 基础协议。

---

# 1. ISA 总体设计原则

## ISA-P-01：固定 Base Command

V0.1 所有 Command Ring Entry 固定：

> **64 Byte = 16 × 32-bit Word**

优点：

- 64B 对齐；
- Ring 地址计算简单；
- DMA Fetch 简单；
- Parser 简单；
- Future Extension 通过 Pointer，而不是不断扩大 Base Command。

---

## ISA-P-02：32-bit Physical Address

所有 V0.1 Command 中的 GPU 可见指针均采用：

> **32-bit Physical Address**

包括：

- Texture Base；
- Framebuffer Base；
- Extension Pointer；
- Draw Descriptor Base；
- Tile Header Base；
- Work List Base；
- Depth Base；
- Palette Base。

如未来平台需要 >4GB 地址空间：

- Base Command Header 不变；
- 通过新 Encoding Version / 64-bit Address Extension Descriptor 扩展。

---

## ISA-P-03：尽量 Stateless / Self-Describing

为了支持：

- Tile 重排；
- Golden Replay；
- Benchmark；
- Fault Recovery；
- 多应用；

V0.1 避免依赖大量隐藏的“当前 GPU 状态”。

每个 Draw Descriptor 尽量自带：

- Source；
- Destination；
- Format；
- Blend；
- Alpha；
- Filter；
- Palette；
- Feature Flags。

Clip / UV Transform 等较大状态通过 `ext_ptr` 指向扩展描述符。

因此 V0.1 **不把 `SET_CLIP` 作为必须的全局状态命令**。

---

## ISA-P-04：Tile Descriptor 与 Immediate Draw 共用编码

Tile 模式下的 Draw Descriptor 使用与 Ring 中 2D Draw Command 相同的 64B 编码。

优点：

- Golden Model 共用解析器；
- RTL 共用 Parser；
- Driver 共用 Encode 逻辑；
- Immediate / Tile 消融实验公平。

---

## ISA-P-05：版本化

每条 Command 自带：

- Class；
- Opcode；
- Encoding Version；
- Length；
- Header Flags。

未知版本/未知语义必须产生可诊断 Fault，而不是静默误执行。

---

## ISA-P-06：Reserved 必须清零

软件生成的所有 Reserved Field / Reserved Bit 必须写 0。

硬件在 Strict 模式下发现非 0 Reserved 字段应报告：

`FAULT_RESERVED_NONZERO`

---

# 2. 字节序、对齐和坐标约定

## 2.1 Endianness

V0.1 定义：

> **Little-Endian**

32-bit Word 在内存中的最低地址保存最低有效 Byte。

例如：

```text
uint32_t value = 0x11223344
```

DDR Byte：

```text
+0 : 0x44
+1 : 0x33
+2 : 0x22
+3 : 0x11
```

---

## 2.2 Command Alignment

Command Ring：

- Base：64B 对齐；
- Entry：64B；
- Entry Count：2^N。

Draw Descriptor Array：

- Base：64B 对齐；
- 每个 Descriptor：64B。

Extension Descriptor：

- 推荐并默认：64B 对齐；
- V0.1 Extension Block：64B。

---

## 2.3 坐标系统

屏幕坐标：

```text
(0,0) ─────────────► +X
  │
  │
  │
  ▼
 +Y
```

左上角为原点。

---

## 2.4 Rectangle 语义

所有 Rectangle 均采用：

> **Half-Open Interval**

即：

```text
[x, x + width)
[y, y + height)
```

因此：

- `width = 0` 或 `height = 0`：No-op；
- 最后一个覆盖像素为：
  - `x + width - 1`
  - `y + height - 1`

---

## 2.5 坐标位宽

### Source X/Y
`uint16_t`

### Destination X/Y
`signed int16_t`

允许 Sprite 部分位于屏幕外：

```text
dst_x < 0
dst_y < 0
```

但如果要获得定义明确的裁剪行为，应启用 Clip Extension。

---

## 2.6 Width / Height

`uint16_t`

范围：

```text
0 ... 65535
```

---

# 3. Base Command：64 Byte 格式

所有命令：

```text
Byte  0 ────────────────────────────── 63
      │ Header / Common │ Payload       │
      │   16 Bytes      │ 48 Bytes      │
```

共：

```text
W0 ... W15
```

每个 Word 为 32 bit。

---

# 4. Common Header

| Word | 名称 | 含义 |
|---:|---|---|
| W0 | `CMD_HEADER` | Class / Opcode / Version / Length / Header Flags |
| W1 | `SEQUENCE_ID` | 软件分配的命令序号 |
| W2 | `USER_TAG` | 软件自定义 Tag |
| W3 | `EXT_PTR` | 可选 64B Extension Descriptor 物理地址 |

---

# 5. W0：CMD_HEADER 位定义

```text
31          28 27                 20 19      16 15        8 7         0
┌─────────────┬─────────────────────┬──────────┬────────────┬───────────┐
│ CMD_CLASS   │ OPCODE              │ VERSION  │ LENGTH_DW  │ HDR_FLAGS │
└─────────────┴─────────────────────┴──────────┴────────────┴───────────┘
```

## 5.1 `CMD_CLASS[31:28]`

4 bit。

---

## 5.2 `OPCODE[27:20]`

8 bit。

一个 Class 最多 256 个 Opcode。

---

## 5.3 `VERSION[19:16]`

V0.1：

> **Encoding Version = 1**

即：

```text
VERSION = 0x1
```

---

## 5.4 `LENGTH_DW[15:8]`

Base Command 长度，单位为 32-bit Word。

V0.1：

```text
LENGTH_DW = 16
```

即 64 Byte。

V0.1 GPU 遇到其他值：

- Strict Mode：Fault；
- Future ISA 可以定义 Variable Length。

---

## 5.5 `HDR_FLAGS[7:0]`

| Bit | 名称 | 含义 |
|---:|---|---|
| 0 | `H_IRQ_ON_RETIRE` | Command retire 时请求 IRQ |
| 1 | `H_TRACE` | 将此 Command 写入 Trace 事件 |
| 2 | `H_EXT_VALID` | `EXT_PTR` 有效 |
| 3 | `H_STRICT` | 开启严格参数/Reserved 检查 |
| 7:4 | Reserved | 必须为0 |

### `H_EXT_VALID = 0`

要求：

```text
EXT_PTR = 0
```

Strict 模式下否则 Fault。

### `H_EXT_VALID = 1`

要求：

- `EXT_PTR != 0`
- 64B 对齐；
- Extension Header 有效。

---

# 6. W1：SEQUENCE_ID

32 bit，由 Driver 分配。

推荐：

> 单调递增，允许32-bit自然回绕。

用途：

- Fault 定位；
- Trace；
- Benchmark；
- Debug；
- Command/Frame 对应。

GPU 不依赖其数值决定执行顺序。

---

# 7. W2：USER_TAG

32-bit 软件自定义值。

可用于：

- Draw ID；
- Sprite ID；
- Frame-local Object ID；
- Benchmark Marker；
- Debug。

GPU 正常渲染不解释 USER_TAG。

Fault / Trace 中应尽量保留。

---

# 8. W3：EXT_PTR

32-bit Physical Address。

若：

```text
H_EXT_VALID = 1
```

则指向一个 64B Extension Descriptor。

若：

```text
H_EXT_VALID = 0
```

必须为 0。

---

# 9. Command Class Namespace

| Class | 名称 | 用途 |
|---:|---|---|
| `0x0` | CONTROL | NOP / Stats / Control |
| `0x1` | DRAW_2D | Fill / Blit / Tile Frame |
| `0x2` | SURFACE | Present / Surface |
| `0x3` | AFFINE_2D5 | Affine / Mode-7 |
| `0x4` | VECTOR | Line / Circle 等 |
| `0x5` | RASTER_3D | Triangle / Z |
| `0x6-0xD` | RESERVED | Future |
| `0xE` | DEBUG_PERF | Trace / Perf |
| `0xF` | SYNC_SYSTEM | Barrier / Fence |

---

# 10. Opcode 总表

## 10.1 CONTROL：Class `0x0`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `NOP` | V0.1 |
| `0x01` | `RESET_STATS` | Competition |
| `0x02` | `PERF_SNAPSHOT` | Competition |
| `0x03-0xFF` | Reserved | Future |

---

## 10.2 DRAW_2D：Class `0x1`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `FILL_RECT` | P0 |
| `0x01` | `BLIT` | P0 |
| `0x02` | `BLIT_EXT` | P1/P2 |
| `0x10` | `TILE_FRAME` | P1 |
| `0x11-0xFF` | Reserved | Future |

---

## 10.3 SURFACE：Class `0x2`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `PRESENT` | P0/P1 |
| `0x01-0xFF` | Reserved | Future |

---

## 10.4 AFFINE_2D5：Class `0x3`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `AFFINE_BLIT` | P3 |
| `0x01` | `MODE7_PLANE` | Reserved / P3 |
| `0x02-0xFF` | Reserved | Future |

---

## 10.5 VECTOR：Class `0x4`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `DRAW_LINE` | Reserved / P3 |
| `0x01` | `DRAW_RECT_OUTLINE` | Reserved / P3 |
| `0x02` | `DRAW_CIRCLE` | Reserved / P3 |
| `0x03-0xFF` | Reserved | Future |

---

## 10.6 RASTER_3D：Class `0x5`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `DRAW_TRIANGLE_FLAT` | Reserved / P3 |
| `0x01` | `DRAW_TRIANGLE_GOURAUD` | Reserved / P3 |
| `0x02` | `DRAW_TRIANGLE_TEX` | Reserved / P3 |
| `0x03-0xFF` | Reserved | Future |

---

## 10.7 DEBUG_PERF：Class `0xE`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `TRACE_MARKER` | P1/P2 |
| `0x01` | `PERF_MARKER` | P2 |
| `0x02-0xFF` | Reserved | Future |

---

## 10.8 SYNC_SYSTEM：Class `0xF`

| Opcode | 命令 | 状态 |
|---:|---|---|
| `0x00` | `MEMORY_BARRIER` | P1 |
| `0x01` | `FENCE_SIGNAL` | P1 |
| `0x02` | `WAIT_FENCE` | Reserved |
| `0x03-0xFF` | Reserved | Future |

---

# 11. Capability 与 Opcode

命令执行前硬件必须检查对应 Capability。

示例：

| 命令/功能 | Capability |
|---|---|
| `FILL_RECT` | `CAP_FILL` |
| `BLIT` | `CAP_BLIT` |
| Per-Pixel Alpha | `CAP_PIXEL_ALPHA` |
| Bilinear | `CAP_BILINEAR` |
| Tile Frame | `CAP_TILE` |
| Indexed8 | `CAP_INDEXED8` |
| Additive | `CAP_ADDITIVE` |
| Affine | `CAP_AFFINE` |
| Triangle | `CAP_TRIANGLE` |
| Z | `CAP_ZBUFFER` |

如果语义上必需的 Feature 不存在：

> 必须 Fault，不允许静默降级。

例如：

- 请求 Bilinear，但 `CAP_BILINEAR=0`
- 请求 Tile，但 `CAP_TILE=0`

均必须报告：

`FAULT_UNSUPPORTED_FEATURE`

---

# 12. 通用 2D Draw Payload

`FILL_RECT`、`BLIT`、`BLIT_EXT` 使用统一 2D Draw Base Payload。

| Word | 名称 |
|---:|---|
| W4 | `SRC_BASE` |
| W5 | `DST_BASE` |
| W6 | `SRC_STRIDE` |
| W7 | `DST_STRIDE` |
| W8 | `SRC_XY` |
| W9 | `DST_XY` |
| W10 | `SRC_WH` |
| W11 | `DST_WH` |
| W12 | `DRAW_STATE` |
| W13 | `PRIMARY_COLOR` |
| W14 | `ALPHA_KEY` |
| W15 | `PALETTE_ADDR` |

这种布局的目标：

> Parser 将多种2D命令统一转换为 Render Context。

---

# 13. 地址与 Stride

## 13.1 `SRC_BASE`

32-bit Physical Address。

FILL 时忽略并应写0。

---

## 13.2 `DST_BASE`

32-bit Physical Address。

Immediate Mode：

指向 Framebuffer / Render Surface。

Tile Mode 中：

Tile Frame 的 Render Target 为最终权威目标；
Descriptor 中该字段仍应填写一致值用于：

- Golden Replay；
- Validation；
- Immediate/Tile 统一编码。

Strict Tile Mode 下若不一致可 Fault。

---

## 13.3 `SRC_STRIDE`

单位：

> Byte / Row

FILL 时忽略。

---

## 13.4 `DST_STRIDE`

单位：

> Byte / Row

---

# 14. `SRC_XY`

```text
31                  16 15                   0
┌─────────────────────┬──────────────────────┐
│ SRC_Y (uint16)      │ SRC_X (uint16)       │
└─────────────────────┴──────────────────────┘
```

---

# 15. `DST_XY`

```text
31                  16 15                   0
┌─────────────────────┬──────────────────────┐
│ DST_Y (int16)       │ DST_X (int16)        │
└─────────────────────┴──────────────────────┘
```

支持负目标坐标。

---

# 16. `SRC_WH`

```text
31                  16 15                   0
┌─────────────────────┬──────────────────────┐
│ SRC_H (uint16)      │ SRC_W (uint16)       │
└─────────────────────┴──────────────────────┘
```

---

# 17. `DST_WH`

```text
31                  16 15                   0
┌─────────────────────┬──────────────────────┐
│ DST_H (uint16)      │ DST_W (uint16)       │
└─────────────────────┴──────────────────────┘
```

---

# 18. `DRAW_STATE`

```text
31                                           0
┌─────────────────────────────────────────────┐
│        Pixel / Texture / Blend State        │
└─────────────────────────────────────────────┘
```

位定义：

| Bit | 名称 | 含义 |
|---:|---|---|
| 3:0 | `SRC_FORMAT` | Source Texture Format |
| 7:4 | `DST_FORMAT` | Render Target Format |
| 11:8 | `BLEND_MODE` | Blend / ROP |
| 13:12 | `FILTER_MODE` | Sampling Filter |
| 15:14 | `ADDR_MODE_U` | U Address Mode |
| 17:16 | `ADDR_MODE_V` | V Address Mode |
| 18 | `COLOR_KEY_EN` | Color Key |
| 19 | `GLOBAL_ALPHA_EN` | Global Alpha |
| 20 | `PIXEL_ALPHA_EN` | Per-Pixel Alpha |
| 21 | `FLIP_X` | Horizontal Flip |
| 22 | `FLIP_Y` | Vertical Flip |
| 23 | `PALETTE_EN` | Palette Decode |
| 24 | `PREMULT_SRC` | Source 已预乘 Alpha |
| 25 | `CLIP_EN` | Clip Extension有效 |
| 26 | `COLOR_MOD_EN` | PRIMARY_COLOR 作为Color Mod |
| 27 | `DITHER_EN` | Format Convert Dither |
| 28 | `DEPTH_TEST_EN` | 预留3D |
| 31:29 | Reserved | 必须0 |

---

# 19. Pixel Format Enum

## `SRC_FORMAT` / `DST_FORMAT`

| Value | Format | Bytes/Pixel | Alpha |
|---:|---|---:|---|
| `0x0` | `RGB565` | 2 | No |
| `0x1` | `ARGB8888` | 4 | Yes |
| `0x2` | `XRGB8888` | 4 | No |
| `0x3` | `INDEX8` | 1 | Palette决定 |
| `0x4-0xF` | Reserved | - | - |

`INDEX8` 作为 Destination 在 V0.1 不要求支持。

---

# 20. Color Packing

## 20.1 Canonical RGBA8888 Word

逻辑32-bit值：

```text
31       24 23       16 15        8 7         0
┌──────────┬───────────┬───────────┬───────────┐
│ A        │ R         │ G         │ B         │
└──────────┴───────────┴───────────┴───────────┘
```

即：

```text
0xAARRGGBB
```

---

## 20.2 RGB565

```text
15      11 10          5 4         0
┌─────────┬─────────────┬───────────┐
│ R[4:0]  │ G[5:0]      │ B[4:0]    │
└─────────┴─────────────┴───────────┘
```

---

# 21. Blend Mode Enum

| Value | 名称 | 说明 |
|---:|---|---|
| `0x0` | `BLEND_COPY` | 直接覆盖 |
| `0x1` | `BLEND_STRAIGHT_ALPHA` | Straight Alpha |
| `0x2` | `BLEND_PREMULT_ALPHA` | Premultiplied Alpha |
| `0x3` | `BLEND_ADD_SAT` | 饱和加法 |
| `0x4` | `BLEND_MULTIPLY` | Multiply / Modulate |
| `0x5` | `ROP_XOR` | XOR，扩展 |
| `0x6-0xF` | Reserved | Future |

Competition Profile 至少：

- COPY；
- STRAIGHT_ALPHA；
- ADD_SAT。

---

# 22. Filter Mode

| Value | 名称 |
|---:|---|
| `0x0` | `FILTER_NEAREST` |
| `0x1` | `FILTER_BILINEAR` |
| `0x2-0x3` | Reserved |

---

# 23. Texture Address Mode

## U/V 各2 bit

| Value | 名称 |
|---:|---|
| `0x0` | `ADDR_CLAMP` |
| `0x1` | `ADDR_REPEAT` |
| `0x2` | `ADDR_MIRROR` Reserved |
| `0x3` | Reserved |

Competition 主体至少实现：

`CLAMP`

---

# 24. `PRIMARY_COLOR`

32-bit：

> RGBA8888

## 对 `FILL_RECT`

表示：

> Fill Color

## 对 `BLIT*`

如果：

```text
COLOR_MOD_EN = 1
```

则作为 Texture Color Modifier。

建议语义：

\[
C' = C \times M
\]

精确舍入由 Pixel Arithmetic Specification 定义。

若：

```text
COLOR_MOD_EN = 0
```

该字段应写：

```text
0xFFFFFFFF
```

---

# 25. `ALPHA_KEY`

```text
31       24 23                              0
┌──────────┬─────────────────────────────────┐
│ G_ALPHA  │ COLOR_KEY_RGB = 0xRRGGBB       │
└──────────┴─────────────────────────────────┘
```

## Global Alpha

```text
0 ... 255
```

## Color Key

按 Canonical Texture Decode 后的：

```text
RGB[23:0]
```

比较。

比较发生顺序：

```text
Texture Decode / Palette
        ↓
Color Key Compare
        ↓
Color Mod / Alpha
```

即：

> Color Key 在 Color Mod 之前比较。

---

# 26. Effective Alpha 语义

若：

```text
PIXEL_ALPHA_EN = 0
```

则：

\[
A_{pixel}=255
\]

若：

```text
PIXEL_ALPHA_EN = 1
```

则：

\[
A_{pixel}=Texture.A
\]

若：

```text
GLOBAL_ALPHA_EN = 0
```

则：

\[
A_{global}=255
\]

最终：

\[
A_{effective}
=
\frac{A_{pixel}\times A_{global}}{255}
\]

精确整数舍入由 Pixel Arithmetic Specification 定义。

---

# 27. `PALETTE_ADDR`

32-bit Physical Address。

当：

```text
SRC_FORMAT = INDEX8
```

时：

- 指向 256 × RGBA8888 Palette；
- 至少4B对齐；
- 推荐64B对齐。

当非 Indexed8：

建议写0。

---

# 28. FILL_RECT

## Encoding

```text
CLASS  = 0x1
OPCODE = 0x00
```

## Payload

使用通用 2D Draw Payload。

### 必须有效

- `DST_BASE`
- `DST_STRIDE`
- `DST_XY`
- `DST_WH`
- `DST_FORMAT`
- `PRIMARY_COLOR`

### 必须为0/忽略

- `SRC_BASE`
- `SRC_STRIDE`
- `SRC_XY`
- `SRC_WH`
- `PALETTE_ADDR`

---

## 28.1 支持的 Blend

FILL 可以支持：

- COPY；
- Straight Alpha；
- Additive；

因此可用于：

- 矩形 UI；
- 半透明面板；
- Overlay。

---

## 28.2 Clip

若：

```text
CLIP_EN = 1
```

则要求：

```text
H_EXT_VALID = 1
```

Extension 中必须提供 Clip Rect。

---

# 29. BLIT

## Encoding

```text
CLASS  = 0x1
OPCODE = 0x01
```

用于：

> **快速、轴对齐、1:1 Texture → Render Target**

---

## 29.1 限制

V0.1 BLIT：

```text
SRC_W == DST_W
SRC_H == DST_H
FILTER = NEAREST
```

不使用 UV Matrix。

允许：

- Color Key；
- Global Alpha；
- Per-Pixel Alpha；
- Blend；
- Palette；
- Flip（可由硬件地址步进实现）。

---

## 29.2 BLIT Fast Path

硬件可以为 BLIT 提供简化 Address Generator。

但：

> 最终 Pixel Back-End 与 BLIT_EXT 共用。

---

# 30. BLIT_EXT

## Encoding

```text
CLASS  = 0x1
OPCODE = 0x02
```

用于：

- Scaling；
- Bilinear；
- Explicit Clip；
- Advanced UV；
- Color Mod；
- 复杂 Sprite。

---

## 30.1 Extension 要求

以下任一功能开启时，建议/要求：

```text
H_EXT_VALID = 1
```

尤其：

- Scale；
- Explicit Clip；
- 自定义 UV Step。

---

# 31. Draw2D Extension Descriptor V1

固定：

> 64 Byte / 16 Word

`EXT_PTR` 指向。

---

## 31.1 Extension Header

### W0

```text
31                   20 19       12 11      8 7        0
┌──────────────────────┬───────────┬─────────┬──────────┐
│ EXT_FLAGS            │ LENGTH_DW │ VERSION │ EXT_TYPE │
└──────────────────────┴───────────┴─────────┴──────────┘
```

V1：

```text
EXT_TYPE  = 0x01   // DRAW2D_EXT
VERSION   = 0x1
LENGTH_DW = 16
```

---

## 31.2 Draw2D Extension Layout

| Word | 名称 |
|---:|---|
| W0 | `EXT_HEADER` |
| W1 | `CLIP_MIN_XY` |
| W2 | `CLIP_MAX_XY` |
| W3 | `U0` |
| W4 | `V0` |
| W5 | `DU_DX` |
| W6 | `DV_DX` |
| W7 | `DU_DY` |
| W8 | `DV_DY` |
| W9 | `BORDER_COLOR` |
| W10 | `ROP_PARAM` |
| W11 | `AUX0` |
| W12 | Reserved |
| W13 | Reserved |
| W14 | Reserved |
| W15 | Reserved |

---

# 32. Clip Rect

## W1：CLIP_MIN_XY

```text
YMIN:int16 | XMIN:int16
```

## W2：CLIP_MAX_XY

```text
YMAX:int16 | XMAX:int16
```

Clip 同样使用 half-open：

```text
[XMIN, XMAX)
[YMIN, YMAX)
```

---

# 33. UV 参数

全部：

> signed Q16.16

定义目标左上像素对应：

```text
u = U0
v = V0
```

每向 +X 走一个目标像素：

\[
u \leftarrow u + DU\_DX
\]

\[
v \leftarrow v + DV\_DX
\]

每向 +Y 走一行：

\[
u_{row+1}=u_{row}+DU\_DY
\]

\[
v_{row+1}=v_{row}+DV\_DY
\]

---

## 33.1 轴对齐 Scaling

典型：

```text
DV_DX = 0
DU_DY = 0
```

---

## 33.2 Horizontal Flip

可通过：

```text
DU_DX < 0
```

实现。

---

## 33.3 Vertical Flip

可通过：

```text
DV_DY < 0
```

实现。

---

# 34. BLIT_EXT 与 AFFINE_BLIT 的关系

`BLIT_EXT`：

要求：

```text
DV_DX = 0
DU_DY = 0
```

即：

> 轴对齐 Scale / Flip。

`AFFINE_BLIT`：

允许：

```text
DV_DX != 0
DU_DY != 0
```

因此同一 `Draw2DExtV1` 可以被复用。

这样：

- 2D Scaling；
- Rotation；
- Shear；
- Affine；

不需要更换 Texture / Pixel / Tile ISA。

---

# 35. AFFINE_BLIT

## Encoding

```text
CLASS  = 0x3
OPCODE = 0x00
```

状态：

> P3 / 2.5D Extension

---

## 35.1 Payload

复用通用 2D Draw Payload。

要求：

```text
H_EXT_VALID = 1
EXT_TYPE = DRAW2D_EXT
```

并允许完整二维 UV Matrix：

\[
u(x,y)=U0+x\cdot DU\_DX+y\cdot DU\_DY
\]

\[
v(x,y)=V0+x\cdot DV\_DX+y\cdot DV\_DY
\]

---

# 36. MODE7_PLANE

Opcode 已预留：

```text
CLASS  = 0x3
OPCODE = 0x01
```

V0.1 不冻结其完整 Payload。

原则：

- 必须继续通过标准 Fragment / Texture / Pixel Back-End；
- Base Command 保持64B；
- 复杂参数通过 `EXT_PTR` / Descriptor；
- 不得修改 Command Ring 基础协议。

---

# 37. Tile Frame Architecture

Tile 模式不是把每个 Draw Command 复制到每个 Tile。

采用：

```text
Draw Descriptor Array
        +
Tile Header Array
        +
Tile Work List
```

---

# 38. Draw Descriptor Array

每条 Draw Descriptor：

> 64B

编码直接复用：

- `FILL_RECT`
- `BLIT`
- `BLIT_EXT`
- `AFFINE_BLIT`
- Future Draw

---

## 38.1 Tile Descriptor 限制

Tile Work List 中只能引用：

> Draw 类 Command / Descriptor

不得引用：

- PRESENT；
- FENCE；
- RESET_STATS；
- MEMORY_BARRIER；
- TILE_FRAME 自身。

---

## 38.2 Descriptor Header Flags

Tile Descriptor 中：

- `H_IRQ_ON_RETIRE` 应为0；
- `H_TRACE` 可选；
- `H_EXT_VALID` 正常有效；
- `SEQUENCE_ID` 作为 Draw ID；
- `USER_TAG` 保留。

---

# 39. Tile WorkRef

V0.1：

> **32-bit Draw Descriptor Index**

若：

```text
draw_desc_base = B
work_ref = i
```

对应 Descriptor 地址：

\[
B + i\times64
\]

---

# 40. Tile Header V1

每个 Tile：

> **16 Byte**

| Word | 名称 |
|---:|---|
| W0 | `WORK_OFFSET` |
| W1 | `WORK_COUNT` |
| W2 | `TILE_FLAGS` |
| W3 | Reserved |

---

## 40.1 WORK_OFFSET

单位：

> WorkRef Entry

不是 Byte。

WorkList地址：

\[
work\_list\_base + WORK\_OFFSET\times4
\]

---

## 40.2 WORK_COUNT

该 Tile 的 WorkRef 数量。

---

## 40.3 TILE_FLAGS

| Bit | 名称 |
|---:|---|
| 0 | `TILE_DONT_LOAD_COLOR` |
| 1 | `TILE_CLEAR_COLOR` |
| 2 | `TILE_LOAD_DEPTH` |
| 3 | `TILE_CLEAR_DEPTH` |
| 31:4 | Reserved |

---

# 41. TILE_FRAME

## Encoding

```text
CLASS  = 0x1
OPCODE = 0x10
```

---

## 41.1 Payload

| Word | 名称 |
|---:|---|
| W4 | `DRAW_DESC_BASE` |
| W5 | `TILE_HEADER_BASE` |
| W6 | `WORK_LIST_BASE` |
| W7 | `DST_BASE` |
| W8 | `DST_STRIDE` |
| W9 | `SURFACE_WH` |
| W10 | `GRID_WH` |
| W11 | `TILE_WH` |
| W12 | `RT_STATE` |
| W13 | `CLEAR_COLOR` |
| W14 | `DEPTH_BASE` |
| W15 | `DEPTH_STRIDE` |

---

# 42. `SURFACE_WH`

```text
31                  16 15                   0
┌─────────────────────┬──────────────────────┐
│ SURFACE_H           │ SURFACE_W            │
└─────────────────────┴──────────────────────┘
```

---

# 43. `GRID_WH`

```text
GRID_H | GRID_W
```

单位：

> Tile 数量。

---

# 44. `TILE_WH`

```text
TILE_H | TILE_W
```

V1.0 默认：

```text
32 × 32
```

---

# 45. `RT_STATE`

建议位定义：

| Bit | 名称 |
|---:|---|
| 3:0 | `DST_FORMAT` |
| 4 | `LOAD_COLOR_DEFAULT` |
| 5 | `STORE_COLOR` |
| 6 | `DEPTH_ENABLE` |
| 7 | `STORE_DEPTH` |
| 8 | `STRICT_TARGET_MATCH` |
| 31:9 | Reserved |

---

# 46. Tile 执行顺序

GPU 按 Tile Scheduler 决定的 Tile 顺序执行。

但：

> 一个 Tile 内的 WorkRef 必须严格按 Work List 顺序执行。

CPU Tile Binner 必须保持：

> 全局 Draw Submission Order

以保证 Alpha / Additive 等顺序依赖操作结果一致。

---

# 47. Tile Target Override

在 Tile 模式：

`TILE_FRAME.DST_BASE / DST_STRIDE / DST_FORMAT`

是 Render Target 的权威定义。

每个 Draw Descriptor 中的：

- DST_BASE；
- DST_STRIDE；
- DST_FORMAT；

用于：

- Replay；
- Debug；
- Immediate兼容。

当：

```text
STRICT_TARGET_MATCH = 1
```

若不一致：

`FAULT_TILE_TARGET_MISMATCH`

---

# 48. Tile Frame Completion

`TILE_FRAME` 只有在：

- 所有 Tile Work 执行完成；
- 所有要求 Store 的 Color/Depth 已完成写回；

后才能 retire。

因此紧随其后的：

`FENCE_SIGNAL`

可以作为完整 Frame Render Fence。

---

# 49. PRESENT

## Encoding

```text
CLASS  = 0x2
OPCODE = 0x00
```

---

## 49.1 Payload

| Word | 名称 |
|---:|---|
| W4 | `SURFACE_BASE` |
| W5 | `SURFACE_STRIDE` |
| W6 | `SURFACE_WH` |
| W7 | `SURFACE_FORMAT` |
| W8 | `FRAME_ID` |
| W9 | `PRESENT_FLAGS` |
| W10 | `PRESENT_TOKEN` |
| W11-W15 | Reserved |

---

# 50. PRESENT_FLAGS

| Bit | 名称 | 含义 |
|---:|---|---|
| 1:0 | `PRESENT_MODE` | 0=Next VSYNC, 1=Immediate, others reserved |
| 2 | `IRQ_ON_FLIP` | 实际发生Flip时产生Display IRQ |
| 3 | `REPLACE_PENDING` | 允许替换尚未Flip的Pending Buffer |
| 31:4 | Reserved | 0 |

Competition 默认：

```text
PRESENT_MODE = NEXT_VSYNC
```

---

# 51. PRESENT 语义

`PRESENT` 被 Command Processor 接受后：

- 将 Surface 放入 Display Pending Queue；
- Command 本身可以 retire；
- 实际 Page Flip 在 VSYNC 时发生。

实际 Flip 后更新：

```text
PRESENT_DONE_ID
PRESENT_DONE_TOKEN
```

因此：

> Command Retire 与 Display Flip Completion 是两个事件。

这样 GPU 不会因为等待 VSYNC 阻塞下一批命令。

---

# 52. Double Buffer Driver 规则

典型：

```text
Render Back Buffer
   ↓
FENCE_SIGNAL(render_done)
   ↓
PRESENT(back)
   ↓
等待 PRESENT_DONE_TOKEN
   ↓
旧 Front 才可作为新的 Back 重用
```

---

# 53. FENCE_SIGNAL

## Encoding

```text
CLASS  = 0xF
OPCODE = 0x01
```

---

## 53.1 Payload

| Word | 名称 |
|---:|---|
| W4 | `FENCE_VALUE` |
| W5 | `FENCE_FLAGS` |
| W6 | `WRITEBACK_ADDR` |
| W7-W15 | Reserved |

---

# 54. Fence 语义

GPU 到达 Fence 后：

1. 等待所有先前命令 architectural completion；
2. 等待必须可见的 Render Target 写入完成；
3. 更新：
   - `FENCE_COMPLETED = FENCE_VALUE`
4. 如果设置 Writeback：
   - 向 `WRITEBACK_ADDR` 写 Fence Value；
5. 如请求 IRQ：
   - 产生 IRQ；
6. Fence retire。

---

# 55. FENCE_FLAGS

| Bit | 名称 |
|---:|---|
| 0 | `FENCE_WRITEBACK` |
| 1 | `FENCE_FLUSH_WRITES` |
| 2 | `FENCE_INVALIDATE_TEX` |
| 3 | `FENCE_INVALIDATE_PALETTE` |
| 31:4 | Reserved |

默认 Driver 应设置：

```text
FENCE_FLUSH_WRITES = 1
```

---

# 56. MEMORY_BARRIER

## Encoding

```text
CLASS  = 0xF
OPCODE = 0x00
```

---

## 56.1 Payload

| Word | 名称 |
|---:|---|
| W4 | `BARRIER_FLAGS` |
| W5-W15 | Reserved |

---

## 56.2 BARRIER_FLAGS

| Bit | 名称 |
|---:|---|
| 0 | `DRAIN_GPU_WRITES` |
| 1 | `INVALIDATE_TEXTURE_CACHE` |
| 2 | `INVALIDATE_PALETTE_CACHE` |
| 3 | `INVALIDATE_DESCRIPTOR_CACHE` |
| 31:4 | Reserved |

主要用途：

> CPU 修改 DDR 中 Texture / Palette / Descriptor 后，要求 GPU 清理内部缓存视图。

---

# 57. NOP

## Encoding

```text
CLASS  = 0x0
OPCODE = 0x00
```

Payload：

全部0。

用途：

- Ring Debug；
- Alignment Test；
- Trace；
- 占位。

---

# 58. RESET_STATS

## Encoding

```text
CLASS  = 0x0
OPCODE = 0x01
```

---

## Payload

### W4：Counter Group Mask

示例：

| Bit | Group |
|---:|---|
| 0 | Core |
| 1 | Command |
| 2 | Pixel |
| 3 | DDR |
| 4 | Tile |
| 5 | Cache |
| 31:6 | Reserved |

便于 Benchmark 在 Workload 前清零指定计数器。

---

# 59. PERF_SNAPSHOT

## Encoding

```text
CLASS  = 0x0
OPCODE = 0x02
```

Payload：

| Word | 名称 |
|---:|---|
| W4 | `DST_ADDR` |
| W5 | `COUNTER_MASK` |
| W6 | `SNAPSHOT_ID` |
| W7-W15 | Reserved |

目标：

> 将一组 Performance Counter 快照写入 DDR。

如果 Competition Profile 最终不实现：

- MMIO读取 Counter 仍可满足基础需求；
- `CAP_PERF_SNAPSHOT=0`。

---

# 60. TRACE_MARKER

## Encoding

```text
CLASS  = 0xE
OPCODE = 0x00
```

Payload：

| Word | 名称 |
|---:|---|
| W4 | `MARKER_ID` |
| W5 | `DATA0` |
| W6 | `DATA1` |
| W7-W15 | Reserved |

用于：

- Frame Begin；
- Boss Phase；
- Benchmark Begin/End；
- Debug。

---

# 61. Command Ring Architecture

## 61.1 Ring Registers

```text
CMD_RING_BASE
CMD_RING_SIZE
CMD_HEAD
CMD_TAIL
```

---

## 61.2 Ring Size

`CMD_RING_SIZE`：

> Entry Count

必须：

```text
2^N
```

建议初始：

```text
256 / 512 / 1024 Entries
```

具体值后续决定。

---

## 61.3 HEAD / TAIL

### HEAD
GPU Consumer Index。

### TAIL
CPU Producer Index。

范围：

```text
0 ... RING_SIZE-1
```

---

# 62. Ring Full / Empty

Empty：

```text
HEAD == TAIL
```

Full 判定由 Driver 保留一个空 Entry：

```text
next(TAIL) == HEAD
```

CPU 不得覆盖未消费 Command。

---

# 63. CPU 提交流程

```text
1. 读取/维护HEAD shadow
2. 确认Ring空间
3. 写64B Command到DDR
4. CPU Memory Barrier
5. 更新CMD_TAIL
6. 可选 Doorbell
```

---

# 64. GPU Fetch 流程

```text
HEAD != TAIL
   ↓
Command DMA读64B
   ↓
Header Validate
   ↓
Capability Validate
   ↓
Parse
   ↓
Dispatch
   ↓
Retire
   ↓
HEAD++
```

---

# 65. Doorbell

V0.1 推荐提供：

```text
CMD_DOORBELL
```

CPU 更新 TAIL 后写 Doorbell 可立即唤醒 GPU Command Fetch。

如果实现持续轮询 Head/Tail，也可以不用。

Driver API 保留 Doorbell 抽象。

---

# 66. Architectural Ordering

V0.1 定义：

> **Command Fetch / Architectural Retire 按 Ring 顺序。**

内部模块可以并行流水：

- Texture Read；
- Pixel Processing；
- Memory Burst；

但从软件可见语义：

> Command N 不得越过 Command N-1 产生违反 ISA 顺序的可见结果。

---

# 67. Draw Command Completion

Immediate Draw retire：

> 该 Draw 的所有目标写已提交并满足定义的可见性要求。

Tile Frame retire：

> 所有 Tile Store 完成。

Fence：

> 在此基础上进一步保证 prior writes flush / visibility。

---

# 68. Present 与 Rendering 的特殊异步性

PRESENT：

- 按 Ring 顺序被提交到 Display Engine；
- Ring Command 可在 VSYNC 前 retire；
- 实际显示完成通过 PRESENT_DONE 观察。

---

# 69. Command Fault 行为

如果 Command Validation 失败：

GPU：

1. 记录 Fault；
2. 记录：
   - Sequence ID；
   - User Tag；
   - Ring Index；
   - Fault Address；
3. 按 Fault Policy：
   - Skip；
   - Halt；
4. 可产生 IRQ。

---

# 70. Fault Policy

V0.1 推荐：

### Recoverable / Skip

- Zero-size Draw；
- Unsupported Advisory Debug Flag。

### Fatal / Halt

- Unknown Opcode；
- Invalid Version；
- Invalid Length；
- Bad Alignment；
- Unsupported Required Feature；
- Invalid Extension；
- Ring corruption；
- Memory Error。

软件可以：

```text
GPU_RESET
```

恢复。

---

# 71. Strict Mode

`H_STRICT=1` 时额外检查：

- Reserved = 0；
- Base/Stride Alignment；
- BLIT 1:1 限制；
- Tile Target一致；
- Extension Type；
- Palette Required；
- Capability；
- Format合法组合。

比赛 Benchmark 与 Golden Regression 推荐始终：

```text
H_STRICT = 1
```

---

# 72. Format Alignment

## RGB565

- Base：至少2B对齐；
- Stride：2B倍数。

## ARGB/XRGB8888

- Base：至少4B对齐；
- Stride：4B倍数。

## INDEX8

- Base：Byte对齐；
- 推荐64B。

---

# 73. BLIT 参数合法性

`BLIT`：

```text
SRC_W == DST_W
SRC_H == DST_H
FILTER = NEAREST
H_EXT_VALID = 0 或仅Clip Ext
```

如果要求 Scale：

使用：

`BLIT_EXT`

---

# 74. BLIT_EXT 参数合法性

如果使用 Q16.16 UV：

要求：

```text
EXT_TYPE = DRAW2D_EXT
```

Driver 负责提前计算：

- U0 / V0；
- DU_DX / DV_DX；
- DU_DY / DV_DY。

GPU 不要求做除法计算 Scale Ratio。

这是重要架构决定：

> **Scale Ratio / Affine Coefficient 由 CPU 预计算，FPGA 只做定点增量。**

这样降低：

- Divider；
- Latency；
- DSP资源；
- 控制复杂度。

---

# 75. 2D Scale 推荐映射

精确 Pixel Center 规则将在 Pixel Arithmetic Specification 冻结。

V0.1 ISA 只规定：

- UV 为 Q16.16；
- `U0/V0` 对应第一个目标像素的采样坐标；
- 后续按增量推进。

因此：

> 不修改 ISA 即可更换 Nearest/Bilinear 的精确采样约定。

---

# 76. Tile 与 Clip 的交互

Tile Renderer 在执行 Descriptor 时有效 Clip：

\[
EffectiveClip =
DrawClip \cap TileRect \cap SurfaceRect
\]

因此：

- Draw 不需为每个 Tile 修改自身坐标；
- 同一 Descriptor 可被多个 Tile Work List 引用。

---

# 77. Tile 与 Alpha 顺序

因为 Alpha 合成通常非交换：

\[
A\ over\ B \ne B\ over\ A
\]

Tile Binner 必须保持原始 Draw Order。

Hardware Tile Scheduler：

> 禁止在单 Tile 内任意重排 Draw。

未来如要做不透明对象排序优化：

必须是独立 ISA/优化版本，并保证语义。

---

# 78. Tile Work List Overflow

软件 Binner 在生成列表前必须有容量规划。

如果硬件发现：

- Header Work Offset越界；
- Work Count越界；
- Descriptor越界；

产生：

`FAULT_WORKLIST_BOUNDS`

---

# 79. Texture / Palette 更新一致性

CPU 修改 Texture / Palette DDR 内容后：

必须执行：

1. CPU侧 Memory Barrier / Cache Flush（平台相关）；
2. 必要时 GPU：
   - `MEMORY_BARRIER(INVALIDATE_TEXTURE_CACHE)`
   - `MEMORY_BARRIER(INVALIDATE_PALETTE_CACHE)`

然后才提交使用新资源的 Draw。

---

# 80. Extension Descriptor 通用原则

所有 Extension：

- 64B对齐；
- Header含：
  - Type；
  - Version；
  - Length；
- Reserved清零；
- 通过 `EXT_PTR` 引用。

未来增加：

- 64-bit Address；
- Perspective；
- Material；
- Triangle；

优先通过新 `EXT_TYPE` 扩展，而不是修改 Base Ring Entry。

---

# 81. Extension Type Namespace

| Type | 名称 |
|---:|---|
| `0x01` | `DRAW2D_EXT_V1` |
| `0x02` | `AFFINE_EXT_V1` Reserved |
| `0x10` | `TRIANGLE_EXT_V1` Reserved |
| `0x11` | `MATERIAL_EXT_V1` Reserved |
| `0x20` | `ADDRESS64_EXT` Reserved |
| others | Reserved |

目前 `AFFINE_BLIT` 可复用 `DRAW2D_EXT_V1`。

---

# 82. Vector Command 预留策略

Vector Class 已冻结 Opcode Namespace，但 V0.1 不冻结详细 Payload。

原则：

- Base Command仍为64B；
- 大参数通过 Ext；
- 最终输出标准 Fragment；
- 共用 Pixel Backend / Tile。

这样未来增加 Line/Circle 不影响现有命令。

---

# 83. Triangle Command 预留策略

`RASTER_3D` Class 已冻结。

未来 Triangle Command 推荐采用：

```text
Base Command
   │
   ├─ Triangle/Vertex Descriptor Pointer
   ├─ Render Target
   ├─ Depth Target
   └─ Material / Texture
```

复杂顶点数据不塞进 Base 64B。

---

# 84. 未来 Triangle 数据路径约束

Triangle Command 必须最终生成：

```text
x, y
u, v
color
z
coverage
```

进入标准 Fragment Interface。

因此：

> Triangle ISA 扩展不允许绕开共享 Pixel Back-End。

---

# 85. 未来 Z 语义

V0.1 只预留：

```text
DEPTH_TEST_EN
DEPTH_BASE
DEPTH_STRIDE
```

实际：

- Z24；
- Z32；
- Clear Depth；
- Compare Function；

在 Triangle ISA 版本冻结时定义。

Base 2D ISA 不需要变化。

---

# 86. Command ISA 与 Performance Counter 的对应

每个 Command retire 应至少能够产生：

```text
COMMAND_COUNT++
```

2D Draw：

```text
PIXEL_COUNT
BLEND_PIXEL_COUNT
KEY_DISCARD_COUNT
SCALE_PIXEL_COUNT
```

Tile：

```text
TILE_LOAD_COUNT
TILE_STORE_COUNT
TILE_WORK_COUNT
```

这样：

> ISA Workload 与硬件测量可以直接关联。

---

# 87. Benchmark 推荐命令序列

## 87.1 Immediate Benchmark

```text
RESET_STATS
FILL_RECT
BLIT...
BLIT...
BLIT...
FENCE_SIGNAL
PERF_SNAPSHOT
PRESENT
```

---

## 87.2 Tile Benchmark

```text
RESET_STATS
TILE_FRAME
FENCE_SIGNAL
PERF_SNAPSHOT
PRESENT
```

---

## 87.3 CPU vs GPU

CPU Software Renderer 使用与 GPU 相同：

- Texture；
- Draw List；
- Draw Order；
- Pixel Format；
- Alpha规则。

保证对比公平。

---

# 88. C 参考结构：Base Command

```c
#include <stdint.h>

typedef struct __attribute__((packed, aligned(64))) {
    uint32_t header;       // W0
    uint32_t sequence_id;  // W1
    uint32_t user_tag;     // W2
    uint32_t ext_ptr;      // W3

    uint32_t w4;
    uint32_t w5;
    uint32_t w6;
    uint32_t w7;
    uint32_t w8;
    uint32_t w9;
    uint32_t w10;
    uint32_t w11;
    uint32_t w12;
    uint32_t w13;
    uint32_t w14;
    uint32_t w15;
} gpu_cmd64_t;

_Static_assert(sizeof(gpu_cmd64_t) == 64, "gpu_cmd64_t must be 64 bytes");
```

---

# 89. C 参考结构：2D Draw Overlay

```c
typedef struct __attribute__((packed, aligned(64))) {
    uint32_t header;
    uint32_t sequence_id;
    uint32_t user_tag;
    uint32_t ext_ptr;

    uint32_t src_base;
    uint32_t dst_base;
    uint32_t src_stride;
    uint32_t dst_stride;

    uint32_t src_xy;
    uint32_t dst_xy;
    uint32_t src_wh;
    uint32_t dst_wh;

    uint32_t draw_state;
    uint32_t primary_color;
    uint32_t alpha_key;
    uint32_t palette_addr;
} gpu_draw2d_cmd_t;
```

---

# 90. C 参考结构：Draw2D Extension

```c
typedef struct __attribute__((packed, aligned(64))) {
    uint32_t ext_header;
    uint32_t clip_min_xy;
    uint32_t clip_max_xy;

    int32_t u0_q16_16;
    int32_t v0_q16_16;

    int32_t du_dx_q16_16;
    int32_t dv_dx_q16_16;

    int32_t du_dy_q16_16;
    int32_t dv_dy_q16_16;

    uint32_t border_color;
    uint32_t rop_param;
    uint32_t aux0;

    uint32_t reserved[4];
} gpu_draw2d_ext_v1_t;

_Static_assert(sizeof(gpu_draw2d_ext_v1_t) == 64,
               "gpu_draw2d_ext_v1_t must be 64 bytes");
```

---

# 91. C 参考 Packing Helper

```c
static inline uint32_t gpu_pack_xy_u16(uint16_t x, uint16_t y)
{
    return ((uint32_t)y << 16) | x;
}

static inline uint32_t gpu_pack_xy_s16(int16_t x, int16_t y)
{
    return ((uint32_t)(uint16_t)y << 16) |
           (uint16_t)x;
}

static inline uint32_t gpu_pack_wh(uint16_t w, uint16_t h)
{
    return ((uint32_t)h << 16) | w;
}
```

---

# 92. Header Packing 建议

```c
#define GPU_CMD_HEADER(cls, op, ver, len_dw, flags) \
    ((((uint32_t)(cls)   & 0xF)  << 28) |          \
     (((uint32_t)(op)    & 0xFF) << 20) |          \
     (((uint32_t)(ver)   & 0xF)  << 16) |          \
     (((uint32_t)(len_dw)& 0xFF) << 8)  |          \
     ((uint32_t)(flags)  & 0xFF))
```

V0.1：

```c
#define GPU_CMD_VER      1
#define GPU_CMD_LEN_DW   16
```

---

# 93. Driver API 与 ISA 的关系

应用调用：

```c
gpu_draw_sprite_ex(...)
```

Driver：

1. 解析 API 参数；
2. 选择：
   - BLIT；
   - BLIT_EXT；
   - AFFINE_BLIT；
3. 填写 64B Command / Descriptor；
4. 必要时生成 Extension；
5. Immediate：
   - 写 Ring；
6. Tile：
   - 加入 Draw Descriptor Array；
   - CPU Binner生成 Work List；
   - 最后提交 TILE_FRAME。

应用不应知道具体 Opcode。

---

# 94. Golden Model 要求

Golden Model 必须直接解析：

> 本文档定义的二进制 Command / Descriptor。

不要为 Golden Model 使用另一套“近似参数结构”。

目标：

```text
Same Command Binary
       │
       ├── PC Golden Renderer
       └── RTL / FPGA GPU
```

这样才能真正实现：

> ISA-level Co-verification。

---

# 95. RTL Parser 要求

Parser 输出统一：

```text
RenderContext
CommandMeta
```

而不是：

- FILL一套完全独立控制；
- BLIT一套；
- ALPHA一套。

建议：

```text
Command Parser
    ↓
Normalize
    ↓
Render Context
    ↓
Dispatcher
```

---

# 96. Command Validation Pipeline

推荐 Parser 顺序：

```text
Fetch 64B
   ↓
Header Check
   ↓
Class / Opcode Check
   ↓
Version / Length Check
   ↓
Capability Check
   ↓
Reserved Check
   ↓
Alignment Check
   ↓
Extension Fetch / Validate
   ↓
Normalize Render Context
   ↓
Dispatch
```

---

# 97. ISA-level Fault Codes 建议

```text
FAULT_BAD_CMD_CLASS
FAULT_BAD_OPCODE
FAULT_BAD_VERSION
FAULT_BAD_LENGTH
FAULT_RESERVED_NONZERO
FAULT_UNSUPPORTED_FEATURE
FAULT_BAD_ALIGNMENT
FAULT_BAD_EXT_PTR
FAULT_BAD_EXT_TYPE
FAULT_BAD_FORMAT
FAULT_BAD_BLEND
FAULT_BAD_FILTER
FAULT_BAD_RECT
FAULT_BAD_TILE_CONFIG
FAULT_TILE_TARGET_MISMATCH
FAULT_WORKLIST_BOUNDS
FAULT_DESCRIPTOR_BOUNDS
FAULT_MEMORY
```

精确数值在 Register/Fault Map 文档定义。

---

# 98. Zero-size Draw

如果：

```text
DST_W == 0
or
DST_H == 0
```

定义为：

> 合法 No-op，不 Fault。

---

# 99. Source Zero-size

对于 BLIT：

如果：

```text
SRC_W == 0
or
SRC_H == 0
```

定义：

> No-op。

Strict 模式可计入 Warning Counter，但不需要 Halt。

---

# 100. Unsupported Advisory Feature 与 Semantic Feature

### Semantic Feature
会改变图像结果：

- Alpha；
- Bilinear；
- Palette；
- Tile；
- Depth。

不支持时：

> 必须 Fault。

### Advisory Feature
只影响：

- Trace；
- Perf；
- Hint。

可在 Capability 明确后允许忽略。

---

# 101. ISA 兼容性规则

未来 V0.2 / V1.0 必须遵守：

1. 不改变已有 Class/Opcode 的既有语义；
2. Reserved 位未来启用时必须配合 Version / Capability；
3. 新功能优先：
   - 新 Opcode；
   - 新 EXT_TYPE；
4. Base Ring Entry 继续保持64B，除非引入明确的新 Ring Mode；
5. 未识别 Command 必须 Fault，不允许误解释。

---

# 102. V0.1 已冻结内容

以下进入 V0.1 Freeze：

- Little-Endian；
- 64B Base Command；
- 16 × 32-bit Word；
- 32-bit Physical Address；
- Common Header W0-W3；
- Header Bit Layout；
- Encoding Version=1；
- LENGTH_DW=16；
- Class Namespace；
- 2D Draw Payload W4-W15；
- Draw State Bit Layout；
- Pixel Format Enum；
- Blend / Filter / Address Mode Enum；
- RGBA8888 Canonical Packing；
- `FILL_RECT`；
- `BLIT`；
- `BLIT_EXT`；
- `TILE_FRAME`；
- `PRESENT`；
- `FENCE_SIGNAL`；
- `MEMORY_BARRIER`；
- Draw2D Extension；
- Q16.16 UV；
- 32-bit Tile WorkRef；
- 16B Tile Header；
- Draw Descriptor与Immediate Command同编码；
- Present异步Flip语义；
- Tile内严格保持Draw顺序；
- Golden Model直接解析ISA二进制。

---

# 103. V0.1 尚未完全冻结内容

以下可以在 V0.2 前调整，但不得破坏上述基础编码：

- Fault Code 具体数值；
- Capability Register 具体bit编号；
- Perf Snapshot数据块布局；
- Trace Buffer格式；
- Palette Cache实现；
- `MODE7_PLANE` Payload；
- Vector Payload；
- Triangle Payload；
- Depth Compare精确枚举；
- Advanced ROP参数；
- ADDRESS64 Extension；
- Variable Length Command模式。

---

# 104. 建议实现顺序

## Stage 0
Parser支持：

- NOP；
- FILL_RECT。

## Stage 1
加入：

- BLIT；
- PRESENT；
- FENCE。

## Stage 2
加入：

- Color Key；
- Global Alpha；
- Per-Pixel Alpha。

## Stage 3
加入：

- BLIT_EXT；
- Draw2D Extension；
- Scale；
- Clip。

## Stage 4
加入：

- Command Ring完整DMA；
- Reset Stats；
- Perf / Trace。

## Stage 5
加入：

- Tile Descriptor；
- Tile Header；
- Work List；
- TILE_FRAME。

## Stage 6
加入：

- Indexed8；
- Bilinear；
- Additive。

## Stage 7
扩展：

- Affine；
- Mode-7。

## Stage 8
研究：

- Triangle / Z。

---

# 105. 最终 ISA 数据流

```text
RISC-V Application
        ↓
Graphics API
        ↓
GPU Driver
        ↓
┌────────────────────┐
│ Command / Descriptor│
│    64 Byte Entry    │
└──────────┬─────────┘
           │
           ▼
      DDR Command Ring
           │
           ▼
      Command DMA
           │
           ▼
      ISA Parser
           │
           ▼
    Render Context
           │
           ├── 2D Front-End
           ├── Affine Front-End [EXT]
           └── Triangle Front-End [EXT]
                   │
                   ▼
             Shared GPU Back-End
```

---

# 106. V0.1 一句话定义

> **GPU Command ISA V0.1 采用64B固定命令、32-bit物理地址和版本化Header，将Immediate Draw、Tile Descriptor、Present、Fence及未来2.5D/3D扩展统一在同一Command体系内；复杂图形状态通过64B Extension Descriptor扩展，使CPU软件、PC Golden Model和FPGA Command Processor共享同一种二进制协议，并为后续图形能力扩展保留稳定接口。**
