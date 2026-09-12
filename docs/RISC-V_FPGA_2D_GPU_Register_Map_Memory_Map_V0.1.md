# RISC-V–FPGA 通用 2D GPU
# GPU Register Map & Memory Map Specification V0.1

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：MMIO Register Map / DDR Logical Memory Map  
> 版本：V0.1  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`  
> 状态：Register/Memory Baseline Freeze Candidate

---

# 0. 文档目的

本文档定义：

1. GPU MMIO 寄存器空间；
2. 各寄存器 Offset、访问属性、Reset Value 和位定义；
3. Command Ring / IRQ / Fence / Present / Perf / Fault / Trace 控制；
4. Capability 暴露方式；
5. DDR 中各类 GPU 数据结构的逻辑内存组织；
6. Framebuffer、Command Ring、Draw Descriptor、Tile Header、Work List、Texture、Palette、Depth、Debug Buffer 的对齐和生命周期规则。

本文不固定 GPU 在 SoC 地址空间中的绝对 Base Address。

定义：

```text
GPU_BASE
```

由平台集成阶段决定。

所有寄存器地址：

\[
ADDR = GPU\_BASE + OFFSET
\]

---

# 1. 全局访问约定

## 1.1 Register Width

V0.1 所有 MMIO Register：

> **32-bit**

CPU应以32-bit自然对齐访问。

---

## 1.2 Endianness

> **Little-Endian**

与 GPU Command ISA 一致。

---

## 1.3 Access Type

| 缩写 | 含义 |
|---|---|
| `RO` | Read Only |
| `RW` | Read / Write |
| `WO` | Write Only |
| `W1C` | Write 1 to Clear |
| `W1S` | Write 1 to Set |
| `RC` | Read and Clear |
| `RSVD` | Reserved |

Reserved：

- 软件写0；
- 软件不依赖读值；
- Strict Debug Build 可对非法写上报 Fault/Warning。

---

# 2. MMIO 顶层空间

| Offset Range | 区域 |
|---:|---|
| `0x0000-0x00FF` | ID / Version / Capability |
| `0x0100-0x01FF` | Core Control / Status |
| `0x0200-0x02FF` | Command Ring |
| `0x0300-0x03FF` | Fence / IRQ |
| `0x0400-0x04FF` | Display / Present |
| `0x0500-0x05FF` | Performance Counters |
| `0x0600-0x06FF` | Fault / Debug |
| `0x0700-0x07FF` | Trace |
| `0x0800-0x08FF` | Palette / Auxiliary |
| `0x0900-0x0FFF` | Reserved for future |
| `0x1000+` | Optional implementation-specific aperture |

---

# 3. ID / Version / Capability 区域

## 3.1 `GPU_ID` — `0x0000`

Access：

`RO`

Reset：

Implementation-defined constant

推荐编码：

```text
31          16 15            0
┌─────────────┬───────────────┐
│ VENDOR_ID   │ DEVICE_ID     │
└─────────────┴───────────────┘
```

建议：

- `VENDOR_ID`：项目自定义；
- `DEVICE_ID`：2D GPU Core ID。

---

## 3.2 `GPU_VERSION` — `0x0004`

Access：

`RO`

编码：

```text
31      24 23      16 15       8 7        0
┌─────────┬──────────┬──────────┬──────────┐
│ MAJOR   │ MINOR    │ PATCH    │ BUILD    │
└─────────┴──────────┴──────────┴──────────┘
```

V0.1 建议：

```text
MAJOR = 0
MINOR = 1
```

---

## 3.3 `ISA_VERSION` — `0x0008`

Access：

`RO`

位定义：

| Bits | 名称 |
|---:|---|
| `7:0` | Command ISA Version |
| `15:8` | Pixel Arithmetic Version |
| `23:16` | Internal Interface Version |
| `31:24` | Reserved |

V0.1：

```text
Command ISA        = 0x01
Pixel Arithmetic   = 0x01
Internal Interface = 0x01
```

---

## 3.4 `CAPS0` — `0x000C`

Access：

`RO`

| Bit | Capability |
|---:|---|
| 0 | `CAP_FILL` |
| 1 | `CAP_BLIT` |
| 2 | `CAP_COLOR_KEY` |
| 3 | `CAP_GLOBAL_ALPHA` |
| 4 | `CAP_PIXEL_ALPHA` |
| 5 | `CAP_COMMAND_RING` |
| 6 | `CAP_FENCE` |
| 7 | `CAP_IRQ` |
| 8 | `CAP_TILE` |
| 9 | `CAP_SCALE` |
| 10 | `CAP_BILINEAR` |
| 11 | `CAP_INDEXED8` |
| 12 | `CAP_ADDITIVE` |
| 13 | `CAP_MULTIPLY` |
| 14 | `CAP_DITHER` |
| 15 | `CAP_CLIP` |
| 16 | `CAP_FLIP` |
| 17 | `CAP_COLOR_MOD` |
| 18 | `CAP_TEX_CACHE` |
| 19 | `CAP_RLE` |
| 20 | `CAP_AFFINE` |
| 21 | `CAP_VECTOR` |
| 22 | `CAP_TRIANGLE` |
| 23 | `CAP_ZBUFFER` |
| 24 | `CAP_TRACE` |
| 25 | `CAP_PERF_SNAPSHOT` |
| 26 | `CAP_MULTIPLANE` |
| 27 | `CAP_TILE_HIGH_PRECISION` |
| 31:28 | Reserved |

---

## 3.5 `CAPS1` — `0x0010`

Access：

`RO`

V0.1 用于实现参数。

| Bits | 名称 |
|---:|---|
| `3:0` | `MAX_LANES_LOG2_OR_CODE` |
| `7:4` | `ACTIVE_CONTEXTS_CODE` |
| `15:8` | `MAX_OUTSTANDING_MEM` |
| `23:16` | `MAX_OUTSTANDING_TEX` |
| `31:24` | Reserved |

推荐定义：

```text
LANES code:
0 -> 1 lane
1 -> 2 lanes
2 -> 4 lanes
```

---

## 3.6 `MAX_SURFACE_WH` — `0x0014`

```text
31                 16 15                  0
┌────────────────────┬─────────────────────┐
│ MAX_H              │ MAX_W               │
└────────────────────┴─────────────────────┘
```

---

## 3.7 `MAX_TEXTURE_WH` — `0x0018`

同上。

---

## 3.8 `DEFAULT_TILE_WH` — `0x001C`

```text
31                 16 15                  0
┌────────────────────┬─────────────────────┐
│ TILE_H             │ TILE_W              │
└────────────────────┴─────────────────────┘
```

V0.1 推荐：

```text
32 × 32
```

---

## 3.9 `MEM_DATA_INFO` — `0x0020`

| Bits | 名称 |
|---:|---|
| `7:0` | Internal Memory Data Width / 8 |
| `15:8` | Max Logical Request KB |
| `23:16` | Recommended Alignment |
| `31:24` | Reserved |

---

# 4. Core Control / Status

## 4.1 `GPU_CONTROL` — `0x0100`

Access：

`RW/W1S mix`

Reset：

`0x00000000`

| Bit | 名称 | 属性 | 含义 |
|---:|---|---|---|
| 0 | `ENABLE` | RW | GPU Core Enable |
| 1 | `SOFT_RESET` | W1S | 软复位请求 |
| 2 | `HALT_REQ` | RW | 请求停止新Command |
| 3 | `RESUME` | W1S | 从HALT恢复 |
| 4 | `STRICT_DEFAULT` | RW | Driver默认Strict模式 |
| 5 | `DOORBELL_ENABLE` | RW | Command Doorbell使能 |
| 6 | `PERF_ENABLE` | RW | Performance Counter总使能 |
| 7 | `TRACE_ENABLE` | RW | Trace总使能 |
| 31:8 | Reserved | - | 0 |

---

## 4.2 `GPU_STATUS` — `0x0104`

Access：

`RO`

| Bit | 名称 |
|---:|---|
| 0 | `READY` |
| 1 | `ENABLED` |
| 2 | `BUSY` |
| 3 | `HALTED` |
| 4 | `FAULTED` |
| 5 | `CMD_IDLE` |
| 6 | `RENDER_IDLE` |
| 7 | `MEM_IDLE` |
| 8 | `TILE_IDLE` |
| 9 | `DISPLAY_READY` |
| 10 | `DDR_READY` |
| 11 | `RESET_IN_PROGRESS` |
| 31:12 | Reserved |

---

## 4.3 `GPU_CONFIG0` — `0x0108`

Access：

`RW`

用于当前 Bitstream 可配置项。

建议：

| Bits | 名称 |
|---:|---|
| `1:0` | `RUNTIME_LANES_MODE`，如实现 |
| 2 | `IMMEDIATE_ENABLE` |
| 3 | `TILE_ENABLE` |
| 4 | `BILINEAR_ENABLE` |
| 5 | `DITHER_ENABLE_DEFAULT` |
| 6 | `TRACE_ON_FAULT` |
| 31:7 | Reserved |

不支持运行时配置的 Feature：

- 对应位可RO为固定值；
- 或写入无效并产生Warning。

---

## 4.4 `GPU_SCRATCH0` — `0x0110`

Access：

`RW`

Reset：

0

无硬件副作用。

用于：

- Driver/MMIO通路测试；
- Bring-up。

---

## 4.5 `GPU_SCRATCH1` — `0x0114`

同上。

---

# 5. Command Ring 寄存器

## 5.1 `CMD_RING_BASE` — `0x0200`

Access：

`RW`

32-bit Physical Address。

要求：

> 64B 对齐。

---

## 5.2 `CMD_RING_SIZE` — `0x0204`

Access：

`RW`

单位：

> Entry Count

要求：

- Power of 2；
- 每 Entry 64B；
- 最小推荐 64；
- 推荐 256/512/1024。

非法值：

`FAULT_BAD_RING_CONFIG`

---

## 5.3 `CMD_HEAD` — `0x0208`

Access：

`RO`

GPU Consumer Index。

---

## 5.4 `CMD_TAIL` — `0x020C`

Access：

`RW`

CPU Producer Index。

CPU仅在：

> Command Data已写入DDR并完成必要CPU Memory Barrier

之后更新。

---

## 5.5 `CMD_DOORBELL` — `0x0210`

Access：

`WO`

写任意值：

> 唤醒/提示 Command DMA 检查 TAIL。

若 Doorbell未实现：

- 可忽略；
- `CAP_COMMAND_RING` 仍可为1。

---

## 5.6 `CMD_STATUS` — `0x0214`

Access：

`RO`

| Bits | 名称 |
|---:|---|
| 0 | `RING_EMPTY` |
| 1 | `RING_FULL_OBSERVED` |
| 2 | `FETCH_BUSY` |
| 3 | `PARSER_BUSY` |
| 4 | `DISPATCH_BUSY` |
| `15:8` | Prefetch Queue Occupancy |
| `31:16` | Reserved |

---

## 5.7 `CMD_LAST_SEQ` — `0x0218`

Access：

`RO`

最后 Retired Command 的 `SEQUENCE_ID`。

---

## 5.8 `CMD_LAST_TAG` — `0x021C`

Access：

`RO`

最后 Retired Command 的 `USER_TAG`。

---

# 6. Fence / IRQ 区域

## 6.1 `FENCE_COMPLETED` — `0x0300`

Access：

`RO`

最后完成的 Fence Value。

---

## 6.2 `FENCE_LAST_SEQ` — `0x0304`

Access：

`RO`

最近 Fence Command 的 Sequence ID。

---

## 6.3 `IRQ_STATUS` — `0x0310`

Access：

`RO`

Raw Pending IRQ。

| Bit | IRQ Source |
|---:|---|
| 0 | `IRQ_CMD_RETIRE` |
| 1 | `IRQ_FENCE_DONE` |
| 2 | `IRQ_PRESENT_DONE` |
| 3 | `IRQ_FAULT` |
| 4 | `IRQ_DISPLAY_UNDERFLOW` |
| 5 | `IRQ_PERF` |
| 6 | `IRQ_TRACE_FULL` |
| 31:7 | Reserved |

---

## 6.4 `IRQ_MASK` — `0x0314`

Access：

`RW`

1：

> 允许对应IRQ输出到CPU。

---

## 6.5 `IRQ_PENDING` — `0x0318`

Access：

`RO`

定义：

```text
IRQ_PENDING = IRQ_STATUS & IRQ_MASK
```

---

## 6.6 `IRQ_CLEAR` — `0x031C`

Access：

`W1C`

写1：

清除对应IRQ Pending。

注意：

如果底层条件仍存在，例如 Fault Sticky 未清除，IRQ可再次置位。

---

## 6.7 `IRQ_FORCE` — `0x0320`

Access：

`W1S`

Debug用途。

软件可强制触发选定 IRQ Source。

Competition Release可通过参数关闭。

---

# 7. Display / Present 寄存器

## 7.1 `DISPLAY_CONTROL` — `0x0400`

Access：

`RW`

| Bit | 名称 |
|---:|---|
| 0 | `DISPLAY_ENABLE` |
| 1 | `VSYNC_IRQ_ENABLE` |
| 2 | `OSD_ENABLE` |
| 3 | `CURSOR_ENABLE` |
| 4 | `UNDERFLOW_HALT` |
| 31:5 | Reserved |

---

## 7.2 `FRONT_BASE` — `0x0404`

Access：

`RO`

当前正在 Scanout 的 Front Buffer Base。

---

## 7.3 `FRONT_STRIDE` — `0x0408`

Access：

`RO`

---

## 7.4 `FRONT_WH` — `0x040C`

Access：

`RO`

---

## 7.5 `FRONT_FORMAT` — `0x0410`

Access：

`RO`

---

## 7.6 `PENDING_BASE` — `0x0414`

Access：

`RO`

已经接受但尚未VSYNC Flip的 Pending Surface。

无Pending时：

0。

---

## 7.7 `PRESENT_DONE_ID` — `0x0418`

Access：

`RO`

最近实际完成 Page Flip 的 `FRAME_ID`。

---

## 7.8 `PRESENT_DONE_TOKEN` — `0x041C`

Access：

`RO`

最近完成 Flip 的 `PRESENT_TOKEN`。

---

## 7.9 `DISPLAY_STATUS` — `0x0420`

Access：

`RO`

| Bit | 名称 |
|---:|---|
| 0 | `ACTIVE` |
| 1 | `VSYNC` |
| 2 | `PENDING_VALID` |
| 3 | `UNDERFLOW_STICKY` |
| 4 | `OSD_ACTIVE` |
| 31:5 | Reserved |

---

## 7.10 `DISPLAY_UNDERFLOW_COUNT` — `0x0424`

Access：

`RO`

显示FIFO Underflow累计次数。

---

# 8. Performance Counter 区域

## 8.1 计数器宽度

V0.1 推荐：

> **64-bit Counter**

通过两个32-bit Register读取。

读取规则：

推荐：

> 读 LOW 时硬件Latch对应HIGH，随后读HIGH。

避免跨32bit读时撕裂。

---

## 8.2 Counter Layout

每个 Counter 占8Byte：

```text
LOW  @ base + 0
HIGH @ base + 4
```

---

## 8.3 `PERF_CONTROL` — `0x0500`

Access：

`RW`

| Bit | 名称 |
|---:|---|
| 0 | `ENABLE` |
| 1 | `FREEZE` |
| 2 | `CLEAR_ALL` W1S |
| 3 | `CLEAR_ON_SNAPSHOT` |
| 31:4 | Reserved |

---

## 8.4 `TOTAL_CYCLES` — `0x0510/0x0514`

GPU Perf Clock开启期间总周期。

---

## 8.5 `BUSY_CYCLES` — `0x0518/0x051C`

GPU Core Busy 周期。

---

## 8.6 `COMMAND_COUNT` — `0x0520/0x0524`

Retired Command数。

---

## 8.7 `COMMAND_STALL_CYCLES` — `0x0528/0x052C`

Command Front-End因下游反压/等待资源导致Stall周期。

---

## 8.8 `PIXEL_COUNT` — `0x0530/0x0534`

产生并进入Pixel Pipeline的有效Pixel/Lane数量。

---

## 8.9 `BLEND_PIXEL_COUNT` — `0x0538/0x053C`

执行Destination Blend的Pixel数。

---

## 8.10 `KEY_DISCARD_COUNT` — `0x0540/0x0544`

Color Key丢弃Pixel数。

---

## 8.11 `SCALE_PIXEL_COUNT` — `0x0548/0x054C`

通过非1:1采样路径的Pixel数。

---

## 8.12 `DDR_READ_BYTES` — `0x0550/0x0554`

Memory Service实际向外部Memory发起并成功完成的Read Byte数。

---

## 8.13 `DDR_WRITE_BYTES` — `0x0558/0x055C`

Write Byte数。

---

## 8.14 `DDR_STALL_CYCLES` — `0x0560/0x0564`

Memory Service或Platform Adapter由于外部Memory不可接受事务造成的Stall周期。

---

## 8.15 `TILE_LOAD_COUNT` — `0x0568/0x056C`

---

## 8.16 `TILE_STORE_COUNT` — `0x0570/0x0574`

---

## 8.17 `TILE_WORK_COUNT` — `0x0578/0x057C`

执行的Tile WorkRef数量。

---

## 8.18 `CACHE_HIT` — `0x0580/0x0584`

若无Cache：

读0。

---

## 8.19 `CACHE_MISS` — `0x0588/0x058C`

若无Cache：

读0。

---

## 8.20 `PRESENT_COUNT` — `0x0590/0x0594`

实际完成Page Flip次数。

---

# 9. Performance Counter 读一致性

推荐 Driver：

1. 写 `PERF_CONTROL.FREEZE=1`
2. 读取所有Counter
3. 写 `FREEZE=0`

用于严格Benchmark。

对于OSD实时显示：

可直接非冻结读取，允许极小时间偏差。

---

# 10. Fault / Debug 区域

## 10.1 `FAULT_STATUS` — `0x0600`

Access：

`RO/W1C mix`

| Bit | 名称 |
|---:|---|
| 0 | `VALID` |
| 1 | `FATAL` |
| 2 | `RECOVERABLE` |
| 3 | `WARNING` |
| 4 | `HALTED_BY_FAULT` |
| 31:5 | Reserved |

清 Fault：

通过 `FAULT_CLEAR`。

---

## 10.2 `FAULT_CODE` — `0x0604`

Access：

`RO`

V0.1 建议编码：

| Code | Fault |
|---:|---|
| `0x0000` | NONE |
| `0x0001` | BAD_CMD_CLASS |
| `0x0002` | BAD_OPCODE |
| `0x0003` | BAD_VERSION |
| `0x0004` | BAD_LENGTH |
| `0x0005` | RESERVED_NONZERO |
| `0x0006` | UNSUPPORTED_FEATURE |
| `0x0007` | BAD_ALIGNMENT |
| `0x0008` | BAD_EXT_PTR |
| `0x0009` | BAD_EXT_TYPE |
| `0x000A` | BAD_FORMAT |
| `0x000B` | BAD_BLEND |
| `0x000C` | BAD_FILTER |
| `0x000D` | BAD_RECT |
| `0x000E` | BAD_RING_CONFIG |
| `0x000F` | BAD_TILE_CONFIG |
| `0x0010` | TILE_TARGET_MISMATCH |
| `0x0011` | WORKLIST_BOUNDS |
| `0x0012` | DESCRIPTOR_BOUNDS |
| `0x0013` | MEMORY_ERROR |
| `0x0014` | DISPLAY_UNDERFLOW |
| `0x0015` | INTERNAL_TIMEOUT |
| `0x0016` | INTERNAL_RT_COORD |
| `0x0017` | CONTEXT_MISMATCH |
| `0x0018` | TAG_MISMATCH |
| `0x0019` | RING_OVERFLOW |
| `0x001A` | BAD_BLEND_STATE |
| `0x001B` | BAD_ADDRESS |
| `0x001C-FFFF` | Reserved |

---

## 10.3 `FAULT_ADDRESS` — `0x0608`

Fault相关地址。

可能是：

- Command Fetch Address；
- Texture Address；
- Descriptor Address；
- DDR地址。

---

## 10.4 `FAULT_SEQUENCE` — `0x060C`

Fault Command Sequence ID。

---

## 10.5 `FAULT_USER_TAG` — `0x0610`

---

## 10.6 `FAULT_RING_INDEX` — `0x0614`

低16bit有效。

---

## 10.7 `FAULT_INFO` — `0x0618`

模块自定义补充信息。

必须在 Verification 文档中对关键Fault形成可重复测试。

---

## 10.8 `FAULT_DROPPED_COUNT` — `0x061C`

多Fault同周期未能完整记录的次数。

---

## 10.9 `FAULT_CLEAR` — `0x0620`

Access：

`W1S`

写 bit0=1：

- 清Sticky Fault；
- 不自动Reset GPU；
- 如果GPU因FATAL Halt，需要后续 `RESUME` 或 `SOFT_RESET`。

---

# 11. Trace 区域

## 11.1 `TRACE_CONTROL` — `0x0700`

Access：

`RW`

| Bit | 名称 |
|---:|---|
| 0 | `ENABLE` |
| 1 | `FREEZE` |
| 2 | `CLEAR` W1S |
| 3 | `STOP_ON_FULL` |
| 4 | `STOP_ON_FAULT` |
| 31:5 | Reserved |

---

## 11.2 `TRACE_STATUS` — `0x0704`

| Bit | 名称 |
|---:|---|
| 0 | `EMPTY` |
| 1 | `FULL` |
| 2 | `OVERFLOW` |
| `15:8` | Occupancy Low Bits |
| `31:16` | Reserved |

---

## 11.3 `TRACE_RD_INDEX` — `0x0708`

Access：

`RW`

软件选择要读取的 Trace Entry。

---

## 11.4 Trace Entry

V0.1 推荐：

> 16 Byte / Entry

四个Read Window Register：

```text
TRACE_DATA0 @ 0x0710
TRACE_DATA1 @ 0x0714
TRACE_DATA2 @ 0x0718
TRACE_DATA3 @ 0x071C
```

逻辑：

```text
W0 timestamp
W1 source_id | event_id
W2 data0
W3 data1
```

---

# 12. Palette / Auxiliary 区域

## 12.1 `PALETTE_CONTROL` — `0x0800`

主要用于 Debug / Optional preload。

正常 Draw 仍使用 Command 中 `PALETTE_ADDR`。

---

## 12.2 `PALETTE_STATUS` — `0x0804`

可报告：

- Loaded Palette Base；
- Valid；
- Cache Hit状态。

具体实现可选。

---

## 12.3 `AUX_CONFIG0` — `0x0810`

保留未来：

- High Precision Tile Mode；
- Cache策略；
- Debug Override。

---

# 13. MMIO Reset 行为

`SOFT_RESET` 后必须：

- Command Engine停止；
- HEAD回到软件重新配置值；
- 清内部Context/Outstanding；
- 清Fatal Halt；
- 保持 Capability/ID；
- Display是否保持旧Front由Implementation决定，但推荐继续显示；
- Performance Counter是否清零由 `PERF_CONTROL` 决定，推荐不自动清。

Driver Reset流程：

```text
HALT_REQ
 ↓
wait HALTED or timeout
 ↓
SOFT_RESET
 ↓
reprogram Ring
 ↓
clear Fault/IRQ
 ↓
ENABLE
```

---

# 14. DDR Logical Memory Map

本项目不固定 DDR 绝对物理地址。

采用：

> 软件启动时统一 Memory Planner / Allocator 分配。

逻辑区域：

```text
DDR
│
├─ Framebuffer A
├─ Framebuffer B
│
├─ Command Ring
│
├─ Draw Descriptor Array
├─ Draw Extension Pool
├─ Tile Header Array
├─ Tile Work List
│
├─ Texture Pool
├─ Palette Pool
├─ Font / GUI Resource
│
├─ Optional Depth Buffer
├─ Optional Vertex / Triangle Descriptor
│
├─ Perf Snapshot Buffer
├─ Frame Dump / Debug Buffer
└─ Reserved
```

---

# 15. 基础对齐要求

| 对象 | 最小对齐 | 推荐对齐 |
|---|---:|---:|
| Framebuffer Base | 64B | 64B/4KB |
| Framebuffer Stride | Pixel对齐 | 64B |
| Command Ring Base | 64B | 64B |
| Command Entry | 64B | 64B |
| Draw Descriptor Base | 64B | 64B |
| Draw Descriptor Entry | 64B | 64B |
| Extension Descriptor | 64B | 64B |
| Tile Header | 16B | 64B Array Base |
| WorkRef | 4B | 64B Array Base |
| Texture Base | Pixel对齐 | 64B |
| Palette Base | 4B | 64B |
| Depth Buffer | 64B | 64B/4KB |
| Perf Snapshot | 64B | 64B |
| Debug Dump | 64B | 4KB |

---

# 16. Framebuffer Size

公式：

\[
size = stride \times height
\]

推荐RGB565 720p：

```text
width  = 1280
height = 720
bpp    = 2
```

若紧密Stride：

```text
stride = 2560 B
size   = 1,843,200 B ≈ 1.758 MiB
```

双缓冲约：

```text
3.516 MiB
```

---

# 17. Framebuffer Stride

虽然 1280×RGB565 的 2560B 已天然64B对齐：

```text
2560 / 64 = 40
```

一般规则：

> `stride = align_up(width * bytes_per_pixel, 64)`

便于Burst。

---

# 18. Command Ring 内存

公式：

\[
size = entry\_count \times 64
\]

例如：

```text
1024 entries
```

大小：

```text
64 KiB
```

---

# 19. Draw Descriptor Array

每Frame：

```text
N_draw × 64B
```

例如：

```text
3000 draws
```

约：

```text
192 KB
```

---

# 20. Draw Extension Pool

最坏：

每Draw一个64B Extension：

```text
N_draw × 64B
```

实际可只给 BLIT_EXT/Affine 分配。

建议：

> Frame-local linear allocator

每帧重置。

---

# 21. Tile Grid

720p + 32×32 Tile：

```text
grid_w = ceil(1280/32) = 40
grid_h = ceil(720/32)  = 23
tile_count = 920
```

最后一行是Partial Tile：

```text
height = 16
```

---

# 22. Tile Header Array

每Tile：

```text
16B
```

720p/32：

```text
920 × 16 = 14,720B
```

约14.4KiB。

---

# 23. Tile Work List

每WorkRef：

```text
4B
```

大小：

\[
4 \times \sum work\_count(tile)
\]

其大小与：

- Draw大小；
- Overdraw；
- Tile Size；

高度相关。

必须由 Architecture Model 统计。

---

# 24. Tile Work List Capacity

Driver必须为每Frame分配：

```text
WORKLIST_CAPACITY
```

Binner若超过：

> 不得静默截断。

软件应：

- 报错；
- 切换Immediate；
- 或重新分配更大Buffer。

硬件若收到越界Header：

`FAULT_WORKLIST_BOUNDS`

---

# 25. Texture Pool

推荐：

> 长生命周期资源池

不同于：

- Frame-local Descriptor；
- WorkList。

Texture应通过：

```text
texture_handle -> physical address
```

由 Resource Manager 管理。

---

# 26. Texture Allocation

推荐：

- Base 64B对齐；
- Row Stride尽量64B对齐；
- Sprite Atlas可减少资源碎片；
- Indexed8适合带宽敏感资源。

---

# 27. Palette Pool

每Palette：

```text
256 × 4 = 1024B
```

推荐：

> 1KB大小，64B对齐。

多Palette可以连续存储。

---

# 28. Font / GUI Resource

可与 Texture Pool共用Allocator。

如果使用字符点阵：

可单独划分只读Resource区。

---

# 29. Depth Buffer

未来3D：

推荐：

```text
32-bit / pixel
```

720p：

```text
1280 × 720 × 4
≈ 3.516 MiB
```

如使用Tile Depth且不保持Full Frame Depth，可另行优化，但V0.1保留完整Depth Buffer逻辑地址。

---

# 30. Frame-local Memory Arena

强烈推荐每个Back Buffer Frame配套一个：

> Frame Arena

结构：

```text
Frame Arena
│
├─ Draw Descriptor Array
├─ Extension Pool
├─ Tile Header
├─ Work List
├─ Optional Dynamic Texture Upload
└─ Debug Metadata
```

Frame完成并确认资源不再被GPU使用后整体Reset Pointer。

这样避免复杂Free。

---

# 31. Triple Arena建议

即使Framebuffer只Double Buffer，Command/Descriptor可以使用：

> 2或3个 Frame Arena

避免CPU构建下一帧时覆盖GPU正在读取的上一帧数据。

推荐：

```text
Arena 0 : GPU consuming
Arena 1 : CPU building
Arena 2 : free / queued
```

是否最终采用3个由DDR容量决定。

---

# 32. Command Ring与Frame Arena关系

Command Ring本身：

> 长生命周期循环Buffer

Draw Descriptor / WorkList：

> Frame Arena临时Buffer

Ring 中 `TILE_FRAME` 只引用当前Frame Arena地址。

---

# 33. CPU Cache Coherency

若RISC-V CPU有Cache：

在更新以下GPU读取内存后：

- Command；
- Descriptor；
- WorkList；
- Texture；
- Palette；

CPU必须执行：

> 平台相关 Cache Clean / Flush + Memory Barrier

之后再更新 `CMD_TAIL`。

---

# 34. GPU写回与CPU读取

CPU读取：

- Perf Snapshot；
- Frame Dump；
- Fence Writeback；

前应：

> 平台相关 Cache Invalidate

除非该区配置为Uncached/Device memory。

---

# 35. 推荐内存属性

如果系统支持MMU/MPU：

### MMIO
Device / Strongly Ordered

### Command Ring / Shared Descriptor
Non-cacheable 或 Write-through / 显式Flush

### Texture
CPU写完后长期只读，可Cacheable + Flush一次

### Framebuffer
取决于CPU软件Renderer需求

---

# 36. 典型720p内存预算示例

以下只是规划示例，不绑定绝对地址。

| 区域 | 典型大小 |
|---|---:|
| FB A RGB565 | 1.76 MiB |
| FB B RGB565 | 1.76 MiB |
| Command Ring 1024 | 64 KiB |
| Draw Desc 4096 | 256 KiB |
| Ext Pool 2048 | 128 KiB |
| Tile Header | ~16 KiB |
| Work List | 0.5–4 MiB，Workload相关 |
| Palette Pool 16组 | 16 KiB |
| Texture Pool | 按素材，数MiB~数十MiB |
| Depth 720p | 3.52 MiB，可选 |
| Debug/Perf | 1–4 MiB |

---

# 37. Memory Planner 输出

软件启动时建议打印/记录：

```text
GPU Memory Layout:
FB0            0x........ size ...
FB1            0x........ size ...
CMD_RING       0x........ size ...
FRAME_ARENA0   0x........ size ...
FRAME_ARENA1   0x........ size ...
TEXTURE_POOL   0x........ size ...
PALETTE_POOL   0x........ size ...
DEBUG_POOL     0x........ size ...
```

用于比赛调试。

---

# 38. Memory Guard Region

Debug Build推荐各大Buffer之间插入：

> 64B / 4KB Guard Pattern

用于检测：

- 越界写；
- WorkList overflow；
- Framebuffer overrun。

Golden/Board Test可定期检查Guard未被修改。

---

# 39. Memory Dump

Debug Region应支持保存：

- Command Ring Snapshot；
- Draw Descriptor；
- Tile Header；
- WorkList；
- Framebuffer；
- Trace；
- Perf Counter。

便于从板上复现失败场景到PC Golden。

---

# 40. Register Driver Initialization Sequence

建议：

```text
1. Read GPU_ID / VERSION / CAPS
2. Verify ISA_VERSION
3. SOFT_RESET
4. Wait READY
5. Allocate DDR buffers
6. Program CMD_RING_BASE/SIZE
7. Clear FAULT
8. Clear IRQ
9. Configure PERF
10. Configure DISPLAY
11. Set ENABLE
12. Submit NOP
13. Submit FILL smoke test
```

---

# 41. Driver Shutdown Sequence

```text
1. HALT_REQ
2. Wait HALTED / CMD_IDLE
3. Drain/complete current fence
4. Disable Display if needed
5. ENABLE=0
```

---

# 42. Register Access Verification

必须建立MMIO test：

- ID固定；
- Scratch读写；
- Reserved保持；
- W1C正确；
- SOFT_RESET行为；
- IRQ Mask/Clear；
- Perf Latch；
- Fault Capture；
- Ring Head/Tail。

---

# 43. V0.1 已冻结内容

- MMIO 32-bit；
- 顶层Offset分区；
- Capability位定义；
- Core Control/Status基本语义；
- Command Ring Register；
- Fence/IRQ Register；
- Display/Present Register；
- 64-bit Perf Counter分区；
- Fault Code基线；
- Trace窗口；
- DDR逻辑区域；
- 主要对象对齐；
- Frame-local Arena思想；
- CPU/GPU Cache Coherency职责；
- 720p内存预算方法。

---

# 44. 尚未冻结内容

- GPU_BASE绝对地址；
- Vendor/Device ID具体值；
- Trace Entry最终深度；
- Perf Counter是否全部物理实现；
- WorkList默认容量；
- Texture Pool大小；
- Frame Arena数量；
- DDR总容量下的最终地址；
- MMU/Cache具体配置；
- 实际Platform Adapter地址限制。

---

# 45. 与后续实现的关系

本文完成后可直接开始：

- MMIO Register File RTL；
- Driver Register Header；
- Command Ring Driver；
- Capability Detection；
- Perf/Fault Driver；
- Board DDR Memory Planner。

建议由单一源生成：

```text
gpu_regs.yaml
   ├─ gpu_regs.h
   ├─ gpu_regs_pkg.sv
   └─ docs table
```

避免C/SV寄存器地址漂移。

---

# 46. V0.1 一句话定义

> **GPU Register Map & Memory Map V0.1 通过固定的MMIO分区、可发现的Capability、标准化Command/Fence/IRQ/Display/Perf/Fault寄存器以及Frame Arena式DDR逻辑内存规划，将RISC-V驱动、FPGA控制面和GPU数据结构统一到可扩展且可验证的软件/硬件地址规范中。**
