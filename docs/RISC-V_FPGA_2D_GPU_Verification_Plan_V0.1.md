# RISC-V–FPGA 通用 2D GPU
# Verification Plan V0.1

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：功能验证 / 数值验证 / 集成验证 / 板级验证 / 性能验证计划  
> 版本：V0.1  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Pixel_Format_Arithmetic_Specification_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Register_Map_Memory_Map_V0.1.md`  
> 状态：Verification Baseline

---

# 0. 文档目的

本文档定义本项目的验证目标、验证层级、测试环境、Golden Model关系、Directed Test、Random Regression、Assertion、Coverage、Full-System Simulation、板级验证、性能验证和阶段退出条件。

最终目标：

> **证明GPU“功能正确、数值正确、协议正确、扩展后不回归、上板稳定、性能数据可信”。**

验证不是项目最后一步，而是贯穿：

```text
Specification
→ Golden
→ RTL
→ Integration
→ Board
→ Optimization
→ Release
```

全过程。

---

# 1. Verification 总原则

## VER-P-01：Specification is the Contract

所有测试预期结果必须来源于：

- Command ISA；
- Internal Interface；
- Pixel Arithmetic；
- Register Map；

不能以“当前RTL行为”反向定义正确结果。

---

## VER-P-02：Golden is Executable Specification

像素结果：

> PC Golden GPU 为主要 Reference。

RTL与FPGA必须匹配Golden。

---

## VER-P-03：Bit-Exact before Performance

先证明：

```text
Correct
```

再讨论：

```text
Fast
```

禁止用性能优化掩盖数值偏差。

---

## VER-P-04：Unit before System

模块先独立验证。

不能第一次测试Alpha Unit就在完整游戏上板。

---

## VER-P-05：Every Bug Gets a Regression

发现Bug并修复后：

> 必须把最小复现加入自动回归。

防止以后再次出现。

---

# 2. 验证层级

```text
L0  Arithmetic Primitive
L1  RTL Unit
L2  RTL Subsystem
L3  Command / ISA
L4  Render Pipeline
L5  Tile / Memory System
L6  Full GPU Simulation
L7  FPGA Board Functional
L8  Performance / Stress
L9  Application / Competition Release
```

---

# 3. L0：Arithmetic Primitive Verification

验证：

- RGB565 Decode；
- RGB565 Encode；
- DIV255_RN；
- MUL8_RN；
- Alpha；
- Additive；
- Bilinear；
- Q16.16；
- Scale Coefficient；
- Dither。

---

## 3.1 Exhaustive Test

必须穷举：

### RGB565 Decode/Encode round-trip

```text
65536 values
```

要求：

```text
encode(decode(x)) == x
```

### MUL8_RN

```text
256 × 256 = 65536 cases
```

### DIV255_RN

完整合法输入范围：

```text
0 ... 65025
```

---

## 3.2 Directed Arithmetic

Alpha：

```text
A = 0,1,127,128,254,255
```

颜色：

```text
0,1,127,128,254,255
```

Bilinear：

```text
fx/fy = 0,1,0x7FFF,0x8000,0xFFFF
```

---

# 4. L1：RTL Unit Verification

每个模块必须有独立 Testbench。

---

## 4.1 Command Parser

输入：

`command_if`

验证：

- Header；
- Opcode；
- Version；
- Length；
- Reserved；
- Strict；
- Extension；
- Capability；
- Fault。

输出：

- Render Context；
- Control Command。

---

## 4.2 2D Front-End

验证：

- Fill bounds；
- Blit；
- Scale；
- Flip；
- Clip；
- packet_first/last；
- lane_mask；
- Q16.16 UV。

---

## 4.3 Texture Unit

验证：

- RGB565；
- ARGB8888；
- XRGB8888；
- Indexed8；
- Palette；
- Nearest；
- Bilinear；
- Clamp；
- Repeat；
- Out-of-order texture response。

---

## 4.4 Pixel Back-End

验证：

- Color Key；
- Color Mod；
- Effective Alpha；
- Straight Alpha；
- Premultiplied Alpha；
- Additive；
- Multiply；
- Discard；
- RT Read/Write sequencing。

---

## 4.5 Immediate RT Adapter

验证：

- RGB565 Read/Write；
- ARGB/XRGB；
- Partial edge；
- Burst coalescing；
- Alpha RMW；
- write completion。

---

## 4.6 Tile Buffer / Tile Adapter

验证：

- Load；
- Clear；
- Work update；
- Store；
- Partial tile；
- Compatibility quantization；
- Dual-bank若实现。

---

## 4.7 Memory Service

验证：

- Read；
- Write；
- Arbitration；
- Alignment；
- 4KB split；
- backpressure；
- client tags；
- read out-of-order；
- write response；
- Display QoS。

---

## 4.8 Display Engine

验证：

- Timing；
- Front buffer scanout；
- VSYNC；
- Present queue；
- Flip；
- Underflow；
- OSD。

---

# 5. Interface Protocol Verification

所有Valid/Ready接口加入Assertion。

---

## 5.1 Stable under Stall

```systemverilog
valid && !ready |=> $stable(payload)
```

---

## 5.2 Valid Payload No-X

```systemverilog
valid |-> !$isunknown(payload)
```

---

## 5.3 Lane Mask

```systemverilog
valid |-> lane_mask != 0
```

适用于：

- fragment；
- pixel；
- RT write。

---

## 5.4 Tag Safety

Response Tag必须对应Outstanding Request。

否则：

`FAULT_TAG_MISMATCH`

---

## 5.5 Context Lifecycle

```text
ctx_alloc
→ work
→ complete
→ ctx_free
```

禁止：

- reuse before completion；
- packet after free；
- mismatched ctx。

---

# 6. Backpressure Verification

对每个流接口随机施加：

```text
ready = random
```

包括长时间：

```text
ready=0
```

验证：

- payload稳定；
- 无丢数据；
- 无重复；
- 无死锁；
- packet_first/last正确。

---

# 7. Reset Verification

必须覆盖：

### Power-on Reset
所有state回到定义值。

### Soft Reset
Command/Context/Fault恢复。

### Reset during Idle

### Reset during Command Fetch

### Reset during Render

### Reset during Memory Stall

比赛Release可以规定：

> 运行时Soft Reset不保证保留未完成Frame，但必须能恢复到可重新初始化状态。

---

# 8. L2：Subsystem Verification

推荐子系统。

---

## S1：Command Subsystem

```text
Ring Model
→ Command DMA
→ Parser
→ Dispatcher
```

测试：

- wrap；
- full；
- empty；
- doorbell；
- invalid command；
- fence；
- IRQ。

---

## S2：Texture Pipeline

```text
Fragment
→ Sampler
→ Cache/Fetch
→ Format
→ Palette
→ Pixel
```

---

## S3：Pixel + RT

```text
Pixel
→ Backend
→ RT Read
→ Blend
→ RT Write
```

---

## S4：Tile Subsystem

```text
Tile Header
→ WorkList
→ Descriptor
→ Render
→ Tile Buffer
→ Store
```

---

## S5：Display Subsystem

```text
Framebuffer
→ Memory
→ FIFO
→ Timing
→ Present
```

---

# 9. L3：ISA Verification

目标：

> 二进制Command在Golden与RTL中解释一致。

---

## 9.1 ISA Binary Vector

每种Command保存：

```text
command.bin
decoded.json
expected_action.json
```

---

## 9.2 必测Command

- NOP
- FILL_RECT
- BLIT
- BLIT_EXT
- TILE_FRAME
- PRESENT
- MEMORY_BARRIER
- FENCE_SIGNAL
- RESET_STATS
- TRACE_MARKER
- AFFINE_BLIT（扩展阶段）

---

## 9.3 Header Fault

覆盖：

- Bad Class；
- Bad Opcode；
- Bad Version；
- Bad Length；
- Reserved nonzero；
- Bad Ext Ptr；
- Unsupported Feature。

---

# 10. Register Map Verification

MMIO Testbench必须验证：

- GPU_ID；
- VERSION；
- CAPS；
- Scratch；
- Control；
- Status；
- Ring Base/Size；
- Head/Tail；
- IRQ Mask/Clear；
- Fault Capture/Clear；
- Perf 64-bit read；
- Display status；
- Trace window。

---

# 11. L4：Render Pipeline Verification

使用：

> Same Command Binary

分别驱动：

```text
Golden GPU
RTL GPU
```

最终比较：

```text
Framebuffer
```

---

# 12. Frame Compare

要求：

> Pixel Exact

对于RGB565：

直接比较16-bit value。

对于ARGB8888：

比较32-bit。

Mismatch输出：

```text
frame
x
y
golden
rtl
sequence_id
user_tag
nearest draw/work
```

---

# 13. Directed Full-Frame Tests

至少建立：

## F001 — Solid Fill
多个矩形、重叠、边缘。

## F002 — Basic Blit
RGB565 sprite。

## F003 — Negative Coordinate
Sprite部分出屏。

## F004 — Color Key

## F005 — Global Alpha

## F006 — Per-Pixel Alpha

## F007 — Alpha Overdraw
100层以上透明叠加。

## F008 — Scaling Up

## F009 — Scaling Down

## F010 — Bilinear

## F011 — Indexed8 Palette

## F012 — Additive Particles

## F013 — Dither

## F014 — Clip

## F015 — Flip X/Y

---

# 14. Edge Case Tests

必须覆盖：

- width=0；
- height=0；
- 1×1；
- 最大合法尺寸；
- dst_x=-1；
- dst_y=-1；
- 正好屏幕右边界；
- 正好屏幕下边界；
- stride > tight stride；
- source rect边缘；
- bilinear边缘；
- alpha=0/255；
- palette index 0/255。

---

# 15. L5：Tile Verification

Tile是项目核心创新，必须单独强化验证。

---

## 15.1 Immediate vs Tile Exact

同一：

- Draw List；
- Draw Order；
- Resource；
- Framebuffer Format；

分别执行：

```text
Immediate
Tile
```

要求：

> Compatibility Mode 逐像素完全一致。

---

## 15.2 Tile Size Sweep

对：

- 16×16；
- 32×32；
- 64×64；

Golden/Model验证：

- 结果一致；
- Work List正确；
- Partial Tile正确。

---

## 15.3 Tile Work Order

构造顺序敏感Alpha场景。

交换两个Draw会明显改变结果。

验证硬件严格按：

> Work List顺序

执行。

---

## 15.4 Tile Partial Edge

720p + 32×32：

底部最后Tile高度仅16。

必须验证：

- 不越界；
- Load/Store byte正确；
- 不污染Guard。

---

## 15.5 Tile Clear

测试：

- Load Color；
- Dont Load；
- Clear Color；
- Partial Tile Clear。

---

# 16. Random Command Regression

建立随机Command生成器。

---

## 16.1 Random维度

- Opcode；
- Position；
- Size；
- Source Rect；
- Stride；
- Format；
- Alpha；
- Color Key；
- Blend；
- Scale Ratio；
- Clip；
- Palette；
- Filter；
- Flip；
- Draw Order；
- Tile/Immediate。

---

## 16.2 Constraint Random

生成两类：

### Legal Random
只生成合法Command。

目标：

验证图像结果。

### Illegal Random
故意生成非法组合。

目标：

验证Fault。

---

# 17. Random Seed

每次Regression记录：

```text
seed
test_count
git_commit
ISA_VERSION
PIXEL_ARITH_VERSION
RTL_CONFIG
```

失败必须能够：

```text
--seed N
```

一键复现。

---

# 18. Regression规模

## Quick

每次开发提交：

- arithmetic unit；
- 100~1000 random commands；
- 典型Frame。

目标：

数分钟。

---

## Nightly

- 10k~100k random commands；
- multiple frames；
- Tile/Immediate；
- backpressure random；
- memory latency random。

---

## Release

- 全Directed；
- Nightly；
- Board regression；
- Performance sanity；
- 长时间stress。

---

# 19. Memory Model

Simulation中的DDR Model必须支持：

- 可变读延迟；
- 可变写响应；
- Random backpressure；
- Burst；
- error injection；
- address bounds；
- optional out-of-order reads。

---

# 20. Memory Error Injection

注入：

- read error；
- write error；
- timeout；
- malformed response（仅protocol test）。

验证：

- fault capture；
- GPU halt；
- sequence/tag记录；
- reset恢复。

---

# 21. Display Stress

模拟：

- GPU高负载；
- Texture大量访问；
- Display持续scanout。

验证：

- QoS；
- FIFO；
- 无underflow。

另外故意减慢Memory：

应触发：

`DISPLAY_UNDERFLOW`

并被Counter/IRQ记录。

---

# 22. Fence Verification

Fence前：

- 多个Draw；
- Pending writes。

Fence完成时：

必须满足：

- prior architectural completion；
- required memory visibility；
- `FENCE_COMPLETED` 更新；
- writeback正确；
- IRQ正确。

---

# 23. Present Verification

必须区分：

### Command Retire
PRESENT已经进入Display Pending。

### Flip Done
VSYNC真正切换。

测试：

- Next VSYNC；
- Pending；
- Replace Pending；
- Token；
- IRQ；
- Front/Back安全复用。

---

# 24. CPU/GPU Coherency Test

若CPU Cache存在：

验证：

- Command Flush；
- Texture Upload Flush；
- Palette Update Flush；
- GPU Writeback Invalidate。

必须建立最小板级测试：

> 不做Cache维护时故意观察错误，做维护后稳定正确。

确保Driver规则真实有效。

---

# 25. Functional Coverage

即使不使用SystemVerilog Covergroup，也要维护覆盖矩阵。

---

## 25.1 Command Coverage

每个Opcode至少：

- valid；
- invalid；
- edge。

---

## 25.2 Format Coverage

Source：

- RGB565
- ARGB8888
- XRGB8888
- INDEX8

Destination：

- RGB565
- ARGB/XRGB（实现后）

---

## 25.3 Blend Coverage

- COPY
- STRAIGHT
- PREMULT
- ADD
- MULTIPLY
- XOR

按Profile实现程度。

---

## 25.4 Cross Coverage

关键交叉：

```text
Format × Blend
Format × Filter
Alpha × Scale
Tile × Blend
Tile × Dither
Indexed8 × Bilinear
NegativeCoord × Clip
```

---

# 26. Code Coverage

如果仿真工具支持：

目标：

- Statement > 90%
- Branch > 85%
- FSM State 100%
- Toggle作为参考

不以Code Coverage替代Functional Coverage。

---

# 27. Assertion Coverage

关键Assertion必须实际触发覆盖：

- stall；
- transfer；
- packet_first；
- packet_last；
- tag outstanding；
- reset；
- fault。

---

# 28. Formal / Property Checking

如工具可用，可对小模块采用Formal。

优先：

- FIFO；
- Ring pointer；
- valid/ready；
- Arbiter；
- Counter；
- Tag allocator；
- Tile address bounds。

不是必须，但高价值。

---

# 29. L6：Full GPU Simulation

结构：

```text
CPU/Driver Model
    ↓
Command Ring DDR
    ↓
GPU RTL Top
    ↓
DDR Behavioral Model
    ↓
Framebuffer Dump
    ↓
Compare with Golden
```

---

# 30. Full-System输入

统一由PC Tool生成：

```text
commands.bin
draw_desc.bin
extensions.bin
tile_headers.bin
worklist.bin
textures.bin
palettes.bin
initial_fb.bin
```

---

# 31. Full-System输出

RTL：

```text
rtl_fb.bin
rtl_perf.json
rtl_trace.bin
rtl_fault.json
```

Golden：

```text
golden_fb.bin
golden_perf_semantic.json
```

Compare Tool：

```text
PASS / FAIL
```

---

# 32. Mismatch最小化

Random失败后自动尝试：

- 减少Frame中的Draw数；
- 二分Command；
- 保留最小复现。

目标：

```text
1000-command failure
→ 3-command minimal reproducer
```

非常推荐。

---

# 33. L7：FPGA Board Functional Verification

板级验证顺序严格分阶段。

---

## B0 — Clock / Reset

- PLL lock；
- reset sequence；
- LED/UART状态。

---

## B1 — DDR

- walking bit；
- address pattern；
- pseudo-random；
- long burst；
- stress。

---

## B2 — HDMI / Display

- Color Bar；
- Checkerboard；
- Gradient；
- Static Image；
- 连续30分钟。

---

## B3 — CPU → Framebuffer

RISC-V软件写：

- pixel；
- rect；
- bitmap。

---

## B4 — MMIO

- ID；
- Scratch；
- Reset；
- IRQ；
- Status。

---

## B5 — GPU Fill

Command：

`FILL_RECT`

Golden截图一致。

---

## B6 — GPU Blit

Texture → Framebuffer。

---

## B7 — Alpha / Key

---

## B8 — Command Ring / Fence

---

## B9 — Tile

---

## B10 — Full Demo

---

# 34. Board Smoke Test

每次新Bitstream至少运行：

1. ID/Version；
2. DDR quick test；
3. Fill；
4. Blit；
5. Alpha；
6. Present；
7. Perf Counter；
8. Fault register empty。

---

# 35. Board Golden Compare

板上Frame完成后：

将Framebuffer导出：

```text
board_fb.bin
```

与PC Golden：

```text
cmp board_fb.bin golden_fb.bin
```

做到真正：

> FPGA Board Pixel Exact

而不只肉眼看。

---

# 36. 长时间稳定性

Competition Candidate至少：

### 1小时
高负载游戏循环。

### 2~4小时
Benchmark Stress循环。

检查：

- crash；
- freeze；
- DDR error；
- display underflow；
- fault；
- counter异常；
- memory leak。

---

# 37. L8：性能验证

性能结果必须在：

> 功能回归PASS

的版本上测量。

---

# 38. 性能基准

至少：

- Sprite Storm；
- Alpha Storm；
- Overdraw Storm；
- Scaling Storm；
- Particle Storm。

---

# 39. 固定Workload

每个Benchmark必须固定：

- Resolution；
- Texture Size；
- Format；
- Sprite Size；
- Draw Count；
- Blend比例；
- Alpha比例；
- Scale比例；
- Random Seed。

---

# 40. 主KPI

> **Max Active Sprites @ Stable 60FPS**

必须定义“Stable”。

---

# 41. Stable 60FPS

记录：

- Mean FPS；
- Mean Frame Time；
- P95 Frame Time；
- Max Frame Time。

目标：

```text
P95 <= 16.67ms
```

更严格时也报告Max。

---

# 42. CPU Offload

对比：

### CPU Software Renderer

### FPGA GPU

统计：

- CPU cycles/frame；
- CPU utilization；
- FPS；
- max sprites。

---

# 43. Command Front-End Ablation

比较：

- Direct/MMIO submit；
- Command Ring。

统计：

- submit cycles；
- commands/s；
- CPU/GPU overlap。

---

# 44. Tile Ablation

同一场景：

- Immediate；
- Tile。

统计：

- DDR Read；
- DDR Write；
- total bytes/frame；
- frame time；
- FPS；
- tile reuse。

---

# 45. Cache Ablation

若实现：

- Cache OFF；
- Cache ON。

统计：

- hit；
- miss；
- DDR BW；
- FPS。

---

# 46. Lane Scaling

若支持多配置：

- 1 lane；
- 2 lane；
- 4 lane。

统计：

- resource；
- Fmax；
- pixel rate；
- FPS；
- DDR saturation。

---

# 47. Performance Sanity

功能正确但性能异常时检查：

- GPU Util过低；
- DDR Stall过高；
- Command Stall；
- Tile Reuse低；
- Pixel Backend Stall；
- Display抢占。

---

# 48. Resource Regression

每个里程碑保存：

```text
LUT
FF
BRAM
DSP
Fmax
```

如果新提交导致：

- LUT +10%以上；
- Fmax明显下降；
- BRAM异常上升；

必须Review。

---

# 49. Timing Verification

每个主要版本：

- Synthesis；
- Place & Route；
- STA。

检查：

- Setup；
- Hold；
- Clock；
- false path；
- CDC。

---

# 50. CDC Verification

必须检查：

- MMIO↔GPU；
- GPU↔DDR；
- GPU↔Display。

禁止：

- unsynchronized control；
- multi-bit raw crossing；
- combinational CDC。

---

# 51. Reset Domain Verification

检查：

- reset assertion；
- deassert同步；
- PLL unlock；
- DDR calibration；
- display reset。

---

# 52. L9：Application Verification

主游戏必须验证：

- 正常启动；
- 长时间运行；
- scene切换；
- boss；
-极限负载；
- pause/resume；
- CPU/GPU切换；
- X-Ray；
- benchmark自动测试。

---

# 53. GUI/HMI Verification

验证：

> 同一Bitstream、不改RTL

运行不同应用。

这是通用性的直接证据。

---

# 54. X-Ray正确性

X-Ray Overlay中的：

- FPS；
- GPU Util；
- DDR BW；
- Sprite Count；
- Tile Reuse；

必须来自真实Counter/软件统计。

禁止写死演示数字。

---

# 55. Fault Injection Board Test

至少人工触发：

- Bad Opcode；
- Bad Alignment；
- Unsupported Feature；
- WorkList越界；
- Display Underflow（若可安全注入）。

验证：

- Fault Code；
- Sequence；
- IRQ；
- recovery。

---

# 56. Verification Artifact

每次Release保存：

```text
verification/
│
├─ test_report.md
├─ directed/
├─ random/
├─ coverage/
├─ golden/
├─ rtl_results/
├─ board_results/
├─ perf/
├─ timing/
└─ known_issues.md
```

---

# 57. Test Case命名

统一：

```text
UT_<module>_<id>
ST_<subsystem>_<id>
FT_<feature>_<id>
RT_<random>_<seed>
BT_<board>_<id>
PT_<performance>_<id>
```

---

# 58. Test Result格式

每个Test：

```text
name
git_commit
rtl_config
seed
start_time
duration
result
first_failure
artifacts
```

---

# 59. CI建议

如果条件允许：

每次Push：

- lint；
- compile；
- arithmetic unit；
- parser；
- quick regression。

Nightly：

- random；
- full frame；
- synthesis sanity。

---

# 60. Lint

RTL进入Review前至少：

- no latch；
- no unintended width truncation；
- signed/unsigned明确；
- no multiple driver；
- no implicit net；
- reset明确。

---

# 61. 数值Lint重点

特别检查：

- 8×8乘法位宽；
- 17-bit blend sum；
- signed Q16.16；
- negative shift；
- coordinate sign extension；
- address multiplication；
- truncation；
- saturation。

---

# 62. Bug分类

### Functional
画面错误。

### Protocol
valid/ready/tag/order。

### Memory
address/burst/overrun。

### Timing
Fmax/CDC。

### Performance
正确但慢。

### Software
driver/binner/resource。

### Board
DDR/HDMI/clock。

分类有助于统计项目风险。

---

# 63. Critical Bug标准

P0：

- crash；
- data corruption；
- wrong frame；
- deadlock；
- DDR error；
- display持续underflow；
- Pixel mismatch。

P1：

- 某扩展Feature错误；
- 性能严重退化。

Release不允许存在已知P0。

---

# 64. Gate C：Basic GPU退出条件

- Fill directed PASS；
- Blit directed PASS；
- Pixel Exact；
- FPGA board PASS；
- DDR/HDMI稳定；
- Quick Regression PASS。

---

# 65. Gate D：Official Complete退出条件

- Color Key PASS；
- Global Alpha PASS；
- Per-Pixel Alpha PASS；
- Double Buffer PASS；
- CPU/GPU Benchmark可重复；
- 30分钟stress无Fatal。

---

# 66. Gate E：Async GPU退出条件

- Ring wrap PASS；
- Fence PASS；
- IRQ PASS；
- Fault PASS；
- 10k+ command random PASS；
- submit性能数据存在。

---

# 67. Gate F：Tile退出条件

- Immediate = Tile Pixel Exact；
- Partial Tile PASS；
- Alpha order PASS；
- WorkList Random PASS；
- DDR Traffic reduction数据；
- Tile stress 1h稳定。

---

# 68. Gate G：Competition Complete退出条件

- 主游戏；
- Benchmark；
- X-Ray；
- 720p60；
- Target Pixel Features；
- Full Regression PASS；
- Timing Clean；
- Resource余量可接受；
- 性能数据冻结；
- Board长时间稳定。

---

# 69. Gate H：Stretch准入条件

只有Gate G通过后，才开始：

- Mode-7；
- Triangle；
- Z；
- 1080p60探索。

---

# 70. Release Candidate Regression

RC必须跑：

### Simulation
全Directed + Random + Full Frame。

### Board
Smoke + Stress + Game + Benchmark。

### Timing
最终P&R报告。

### Data
性能表重新确认。

---

# 71. 比赛前冻结规则

正式比赛Bitstream：

> 至少提前若干天冻结。

冻结后仅允许：

- P0 Critical Fix；

且修复后必须：

> Full Regression重新跑。

禁止临场加新Feature。

---

# 72. 最终答辩可展示的验证证据

推荐准备：

1. Golden vs RTL Pixel Exact截图；
2. Immediate vs Tile diff = 0；
3. Random Regression数量；
4. Resource/Fmax演进；
5. DDR Traffic before/after；
6. CPU cycles before/after；
7. Max Sprite@60FPS；
8. 长时间stress记录；
9. X-Ray真实Counter；
10. Fault/Recovery机制。

---

# 73. V0.1 已冻结验证策略

- Golden作为Pixel Reference；
- Arithmetic穷举；
- Module Unit Test；
- valid/ready Assertions；
- Random Backpressure；
- Directed Full Frames；
- Random Command Regression；
- Immediate/Tile Exact；
- Full GPU DDR Model；
- Board Frame Dump Compare；
- Performance在功能PASS版本上测；
- 每个Bug加入Regression；
- Gate式退出标准；
- Release前全量回归。

---

# 74. 当前立即要实现的Verification基础设施

在写大量RTL前完成：

1. `frame_compare.py`
2. `command_dump.py`
3. `command_generator`
4. `golden_fb.raw`格式
5. `rtl_fb.raw`格式
6. Test Manifest
7. Seed管理
8. Unit Test目录结构
9. Assertion模板
10. Mock Memory

---

# 75. V0.1 一句话定义

> **Verification Plan V0.1 采用“规格为契约、Golden为可执行参考、模块先验证、随机持续回归、Immediate/Tile逐像素等价、仿真与上板双重比对、性能仅在功能正确版本上测量”的验证体系，确保这颗RISC-V–FPGA 2D GPU从位精确算术到完整游戏展示都具备可复现、可量化、可回归的工程证据。**
