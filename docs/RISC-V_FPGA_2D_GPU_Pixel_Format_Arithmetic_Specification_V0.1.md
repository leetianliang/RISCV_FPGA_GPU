# RISC-V–FPGA 通用 2D GPU
# Pixel Format & Arithmetic Specification V0.1

> 项目：易灵思 FPGA 创新设计赛道——RISC-V–FPGA 通用 2D GPU  
> 文档类型：像素格式 / 定点数 / 采样 / 混合位精确规格  
> 版本：V0.1  
> 日期：2026-09-12  
> 上游文档：  
> - `RISC-V_FPGA_2D_GPU_Requirements_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_System_Architecture_V1.0.md`  
> - `RISC-V_FPGA_2D_GPU_Command_ISA_V0.1.md`  
> - `RISC-V_FPGA_2D_GPU_Internal_Interface_Specification_V0.1.md`  
> 状态：Pixel Arithmetic Baseline Freeze Candidate

---

# 0. 文档目的

本文档定义 GPU 所有与像素值、坐标、颜色、Alpha、采样和格式转换有关的**位精确语义**，作为：

- PC Golden GPU；
- RTL Texture Unit；
- RTL Pixel Back-End；
- Immediate Render Target；
- Tile Render Target；
- Random Regression；
- Full-Frame Pixel Compare；

共同遵循的唯一数值规格。

本文档的核心原则：

> **算法语义先冻结，实现方式可以优化。**

例如：

- 规格定义“Round-to-Nearest”；
- RTL 可以用移位/加法恒等变换实现；
- 但最终输出必须与本文 Reference Arithmetic 一致。

---

# 1. 适用范围

本文冻结：

- RGB565；
- ARGB8888；
- XRGB8888；
- Indexed8 + Palette；
- Canonical RGBA8888；
- Little-Endian Memory Layout；
- 8-bit Channel Arithmetic；
- RGB565 Decode / Encode；
- Ordered Dither；
- Color Key；
- Color Modulate；
- Global Alpha；
- Per-Pixel Alpha；
- Coverage；
- Straight Alpha；
- Premultiplied Alpha；
- Additive Blend；
- Multiply Blend；
- XOR ROP；
- Q16.16；
- Pixel Center Convention；
- BLIT / Scaling UV；
- Nearest；
- Bilinear；
- Clamp / Repeat；
- Flip；
- Affine UV；
- Destination Quantization；
- Immediate / Tile 数值一致性；
- 初步 Depth Compare 语义。

本文暂不冻结：

- Gamma-correct blending；
- sRGB/Linear Color Space转换；
- HDR；
- >8-bit Channel；
- Perspective-Correct Texture；
- MSAA；
- 高级各向异性过滤；
- 浮点像素格式。

---

# 2. 总体数值约定

## 2.1 整数类型

除特别说明外：

- 无符号量：`uint`
- 有符号量：Two's Complement
- 右移无符号值：Logical Shift
- 右移有符号值：Arithmetic Shift

Golden Model 中涉及乘加的中间结果必须使用足够宽的整数，禁止依赖 C/C++ 未定义溢出。

推荐：

> **至少 64-bit signed/unsigned intermediate**

---

## 2.2 Saturation

定义：

```text
SAT_U8(x) =
    0,   x < 0
    x,   0 <= x <= 255
    255, x > 255
```

```text
SAT_U5(x) = clamp(x, 0, 31)
SAT_U6(x) = clamp(x, 0, 63)
```

---

## 2.3 Clamp

定义：

\[
CLAMP(x,l,h)=
\begin{cases}
l,&x<l\\
x,&l\le x\le h\\
h,&x>h
\end{cases}
\]

---

# 3. 精确除以255辅助函数

8-bit 色彩运算大量出现：

\[
\frac{x}{255}
\]

其中典型范围：

\[
0\le x\le65025
\]

V0.1 统一采用：

> **Round-to-Nearest**

定义：

\[
DIV255\_RN(x)=
\left\lfloor
\frac{x+127}{255}
\right\rfloor
\]

---

## 3.1 等价硬件实现

对于：

\[
0\le x\le65025
\]

允许使用：

```text
y = x + 128
result = (y + (y >> 8)) >> 8
```

该实现与：

```text
floor((x + 127) / 255)
```

在上述完整输入范围内位精确一致。

---

## 3.2 8-bit乘法

定义：

\[
MUL8\_RN(a,b)=DIV255\_RN(a\times b)
\]

其中：

```text
a,b ∈ [0,255]
```

输出：

```text
0...255
```

---

## 3.3 8-bit LERP

定义：

\[
LERP8(a,b,t)=
DIV255\_RN(
a(255-t)+bt
)
\]

其中：

```text
t = 0   => a
t = 255 => b
```

---

# 4. Canonical Internal Color

GPU Pixel Pipeline 的规范内部颜色为：

> **RGBA8888**

逻辑 Word：

```text
31       24 23       16 15        8 7         0
┌──────────┬───────────┬───────────┬───────────┐
│ A        │ R         │ G         │ B         │
└──────────┴───────────┴───────────┴───────────┘
```

记作：

```text
0xAARRGGBB
```

每通道：

```text
0 ... 255
```

---

# 5. Little-Endian Memory Layout

逻辑：

```text
0xAARRGGBB
```

在 Little-Endian DDR 中的 Byte 顺序：

```text
addr+0 = BB
addr+1 = GG
addr+2 = RR
addr+3 = AA
```

---

# 6. RGB565

逻辑16-bit：

```text
15      11 10          5 4         0
┌─────────┬─────────────┬───────────┐
│ R5      │ G6          │ B5        │
└─────────┴─────────────┴───────────┘
```

Memory Little-Endian：

```text
addr+0 = bits[7:0]
addr+1 = bits[15:8]
```

---

# 7. RGB565 → RGBA8888

V0.1 决定：

> **Bit Replication Expansion**

对于：

```text
R5 ∈ [0,31]
G6 ∈ [0,63]
B5 ∈ [0,31]
```

定义：

```text
R8 = (R5 << 3) | (R5 >> 2)
G8 = (G6 << 2) | (G6 >> 4)
B8 = (B5 << 3) | (B5 >> 2)
A8 = 255
```

---

## 7.1 性质

保证：

```text
0  -> 0
max -> 255
```

并且配合本文无Dither的RGB565编码规则：

> 对任意合法 RGB565 值，Decode → Encode 可恢复原始 RGB565。

---

# 8. RGBA8888 → RGB565：无 Dither

V0.1 默认使用：

> **Scale-to-Range + Round-to-Nearest**

定义：

\[
R5 =
\left\lfloor
\frac{R8\times31+127}{255}
\right\rfloor
\]

\[
G6 =
\left\lfloor
\frac{G8\times63+127}{255}
\right\rfloor
\]

\[
B5 =
\left\lfloor
\frac{B8\times31+127}{255}
\right\rfloor
\]

然后：

```text
RGB565 = (R5 << 11) | (G6 << 5) | B5
```

Alpha 被丢弃。

---

# 9. RGB565 Ordered Dither

当：

```text
DITHER_EN = 1
DST_FORMAT = RGB565
```

使用 4×4 Bayer Matrix：

```text
B4 =
 0   8   2  10
12   4  14   6
 3  11   1   9
15   7  13   5
```

索引：

```text
b = B4[y & 3][x & 3]
```

其中 x/y 为**全局 Render Target Pixel Coordinate**。

---

## 9.1 通用 Dither Quantizer

对 8-bit 通道 `C`，目标最大值 `M`：

- R/B：`M=31`
- G：`M=63`

计算：

```text
p    = C * M
base = p / 255
rem  = p % 255
```

阈值比较：

```text
if base < M and rem * 32 > (2*b + 1) * 255:
    q = base + 1
else:
    q = base
```

最终：

```text
q ∈ [0,M]
```

---

## 9.2 Dither适用限制

V0.1：

- 仅对 `RGB565` Render Target 定义；
- 对 ARGB8888/XRGB8888 开启 DITHER：
  - Strict Mode：`FAULT_BAD_FORMAT/STATE`
  - Non-Strict：忽略 DITHER。

---

# 10. ARGB8888

External ARGB8888 与 Canonical RGBA8888 使用同一逻辑 Word：

```text
0xAARRGGBB
```

因此：

> Decode / Encode 不改变位值。

---

# 11. XRGB8888

External：

```text
0xXXRRGGBB
```

读取：

```text
R = RR
G = GG
B = BB
A = 255
```

写入：

```text
X = 0xFF
```

V0.1 统一写成：

```text
0xFFRRGGBB
```

---

# 12. Indexed8

Texture Memory：

```text
1 Byte / Texel
```

值：

```text
0...255
```

作为 Palette Index。

Palette：

```text
256 entries × RGBA8888
```

Palette Entry：

```text
0xAARRGGBB
```

---

# 13. Indexed8 + Bilinear

V0.1 定义顺序：

```text
Fetch 4 Index8
    ↓
Palette Decode each neighbor
    ↓
4 × RGBA8888
    ↓
Bilinear Interpolate RGBA
```

禁止：

> 直接对8-bit索引做插值后再查Palette。

---

# 14. Source Format Decode

Texture Unit最终输出：

```text
src_rgba
```

规则：

| Source | Canonical输出 |
|---|---|
| RGB565 | Expand，A=255 |
| ARGB8888 | 原值 |
| XRGB8888 | A=255 |
| INDEX8 | Palette RGBA |

---

# 15. Color Key

Color Key 比较发生在：

> **Texture Format / Palette Decode + Sampling 之后，Color Modulate之前。**

比较：

```text
sampled_rgba.RGB == COLOR_KEY_RGB
```

忽略 Alpha。

如果相等：

> 该 Pixel / Lane Discard。

---

## 15.1 Color Key + Nearest

Nearest 模式下：

> 对真实采样Texel颜色进行精确RGB比较。

---

## 15.2 Color Key + Bilinear

Bilinear 模式下：

> 对 Bilinear 输出的最终 RGB 做比较。

因此 Bilinear 与 Color Key 组合在透明边缘可能出现非预期颜色。

推荐：

> Bilinear Sprite 使用 Per-Pixel Alpha，而不是 Color Key。

---

# 16. Color Modulate

当：

```text
COLOR_MOD_EN = 1
```

设：

```text
M = PRIMARY_COLOR = (Ma, Mr, Mg, Mb)
```

源RGB变换：

\[
S'_r=MUL8\_RN(S_r,M_r)
\]

\[
S'_g=MUL8\_RN(S_g,M_g)
\]

\[
S'_b=MUL8\_RN(S_b,M_b)
\]

---

## 16.1 Mod Alpha

`M.a` 不直接改写 Texture Alpha Stored Value，而作为：

> **额外Opacity因子**

参与 Effective Alpha 计算。

这保证：

- Color Tint；
- Fade；
- Global Alpha；

可以组合。

---

# 17. Alpha输入因子

定义四类 Alpha 因子。

---

## 17.1 Source / Per-Pixel Alpha

若：

```text
PIXEL_ALPHA_EN = 1
```

则：

\[
A_{src}=S_a
\]

否则：

\[
A_{src}=255
\]

---

## 17.2 Color-Mod Alpha

若：

```text
COLOR_MOD_EN = 1
```

则：

\[
A_{mod}=M_a
\]

否则：

\[
A_{mod}=255
\]

---

## 17.3 Global Alpha

若：

```text
GLOBAL_ALPHA_EN = 1
```

则：

\[
A_{global}=GLOBAL\_ALPHA
\]

否则：

\[
A_{global}=255
\]

---

## 17.4 Coverage

2D主体：

```text
coverage = 255
```

未来 Triangle / Anti-Alias：

```text
coverage ∈ [0,255]
```

若 Coverage Feature未启用：

\[
A_{coverage}=255
\]

---

# 18. Effective Alpha：精确计算顺序

V0.1 **冻结分阶段舍入顺序**：

```text
A0 = A_src
A1 = MUL8_RN(A0, A_mod)
A2 = MUL8_RN(A1, A_global)
A3 = MUL8_RN(A2, A_coverage)
Aeff = A3
```

不得替换成：

```text
round(Sa * Ma * Ga * Coverage / 255^3)
```

因为单次总乘法与分阶段Round可能产生不同结果。

Golden与RTL必须按上述顺序。

---

# 19. Straight Alpha Blend

适用于：

```text
BLEND_STRAIGHT_ALPHA
PREMULT_SRC = 0
```

输入：

```text
S' = Color-Modulated Source RGB
D  = Destination RGBA
A  = Aeff
```

每个 RGB Channel：

\[
O_c =
DIV255\_RN(
S'_c\times A +
D_c\times(255-A)
)
\]

---

## 19.1 Output Alpha

Porter-Duff Source-Over：

\[
O_a =
A +
MUL8\_RN(D_a,255-A)
\]

理论范围不超过255。

---

## 19.2 边界条件

### A=0

```text
O = D
```

### A=255

```text
O.rgb = S'.rgb
O.a   = 255
```

---

# 20. Premultiplied Alpha Blend

适用于：

```text
BLEND_PREMULT_ALPHA
PREMULT_SRC = 1
PIXEL_ALPHA_EN = 1
```

Strict模式下若不满足：

> `FAULT_BAD_BLEND_STATE`

---

## 20.1 Premultiplied Source定义

Texture中的：

```text
S.rgb
```

已经包含：

\[
S_{rgb}=C_{rgb}\times S_a
\]

的预乘结果。

不能再次乘 Source Alpha。

---

## 20.2 Extra Alpha

定义：

```text
E0 = 255
E1 = MUL8_RN(E0, A_mod)
E2 = MUL8_RN(E1, A_global)
E3 = MUL8_RN(E2, A_coverage)

Aextra = E3
```

Effective Alpha：

\[
Aeff=MUL8\_RN(S_a,Aextra)
\]

---

## 20.3 Premultiplied RGB Contribution

先Color Mod RGB：

\[
S'_c=MUL8\_RN(S_c,M_c)
\]

若Color Mod关闭：

```text
S' = S
```

然后：

\[
S^{contrib}_c =
MUL8\_RN(S'_c,Aextra)
\]

输出：

\[
O_c =
SAT\_U8(
S^{contrib}_c +
MUL8\_RN(D_c,255-Aeff)
)
\]

输出Alpha：

\[
O_a =
Aeff+
MUL8\_RN(D_a,255-Aeff)
\]

---

# 21. BLEND_COPY

定义：

```text
O.rgb = Color-Modulated Source RGB
```

Output Alpha：

```text
if COLOR_MOD_EN:
    O.a = MUL8_RN(S.a, M.a)
else:
    O.a = S.a
```

`GLOBAL_ALPHA_EN` / `PIXEL_ALPHA_EN`：

> 不影响 `BLEND_COPY` 的覆盖行为。

如果需要透明覆盖：

> 使用 Straight/Premultiplied Alpha，而不是 COPY。

---

# 22. Additive Blend

Competition Profile定义：

> **Straight-source Saturating Add**

Strict模式要求：

```text
PREMULT_SRC = 0
```

源贡献：

\[
C^{src}_c =
MUL8\_RN(S'_c,Aeff)
\]

输出：

\[
O_c =
SAT\_U8(D_c+C^{src}_c)
\]

Alpha：

\[
O_a =
SAT\_U8(D_a+Aeff)
\]

---

## 22.1 用途

- Glow；
- Laser；
- Fire；
- Energy Particle；
- Bullet Trail。

---

# 23. Multiply / Modulate Blend

P2/P3语义冻结为：

先计算：

\[
M_c=MUL8\_RN(D_c,S'_c)
\]

再根据Aeff插值：

\[
O_c=LERP8(D_c,M_c,Aeff)
\]

Alpha：

\[
O_a =
Aeff+
MUL8\_RN(D_a,255-Aeff)
\]

---

# 24. XOR ROP

定义：

```text
O.R = D.R XOR S'.R
O.G = D.G XOR S'.G
O.B = D.B XOR S'.B
O.A = D.A
```

Strict模式：

- Alpha Flags必须关闭；
- PREMULT_SRC必须0。

---

# 25. Blend处理顺序

完整顺序：

```text
Sample / Format Decode
        ↓
Color Key
        ↓
RGB Color Modulate
        ↓
Alpha Factor Construction
        ↓
Optional Depth Test
        ↓
Destination Read
        ↓
Blend / ROP
        ↓
Logical Render-Target Quantization
        ↓
Render-Target Storage
```

---

# 26. 一个重要的 V0.1 数值一致性决定

System Architecture V1.0 中 Tile Buffer 物理存储使用：

> RGBA8888

但为了满足：

> **Immediate Mode 与 Tile Mode 在相同 Draw Order 下逐像素一致**

V0.1 冻结：

> **每一次 Render Target Write 都必须首先按照 `DST_FORMAT` 做逻辑量化，然后再形成下一次 Destination Read 所见的值。**

---

## 26.1 RGB565目标

每个 Pixel Write：

```text
Post-Blend RGBA8888
        ↓
RGB565 Encode
        ↓
RGB565 Decode
        ↓
Canonical RGBA8888
```

Tile BRAM 中实际物理可以仍保存32-bit RGBA8888，但必须保存：

> **RGB565量化后重新Expand的RGBA8888值**

A固定：

```text
255
```

---

## 26.2 目的

这样：

### Immediate

```text
Blend
 ↓
RGB565写DDR
 ↓
下一Draw读RGB565
```

### Tile

```text
Blend
 ↓
逻辑RGB565量化
 ↓
RGBA8888 BRAM存量化结果
 ↓
下一Draw直接读BRAM
```

数值语义一致。

---

## 26.3 对上游架构文档的澄清

因此 V0.1 的 Compatibility / Benchmark 语义：

> Tile Buffer 使用 RGBA8888 作为**物理内部存储宽度**，但不是“跨多个Draw无限保留未量化高精度”。

若未来希望增加：

> **High-Precision Tile Mode**

可新增 Capability / State：

- Tile内保留全RGBA8888；
- 最终Store时才量化；

但该模式：

> 不保证与Immediate RGB565逐像素一致。

该扩展不属于V0.1 Competition Compatibility模式。

---

# 27. ARGB8888目标量化

对：

```text
DST_FORMAT = ARGB8888
```

逻辑量化：

> Identity

即：

```text
RGBA8888 -> RGBA8888
```

---

# 28. XRGB8888目标量化

写入：

```text
A = 255
RGB保持
```

Destination Read也返回：

```text
A = 255
```

---

# 29. INDEX8作为Render Target

V0.1：

> 不支持 INDEX8 Destination。

Strict：

`FAULT_BAD_FORMAT`

---

# 30. Dither与Tile/Immediate一致性

Dither使用：

> **全局目标像素坐标 x/y**

因此：

Immediate和Tile必须对同一个Pixel使用同一个Bayer元素。

在RGB565 Compatibility模式中：

> 每次逻辑RT Write都执行相同Dither/Quantization。

---

# 31. Q16.16 定义

UV使用：

> **signed Q16.16**

32-bit two's complement。

实数含义：

\[
real(q)=\frac{q}{65536}
\]

范围：

约：

```text
-32768.0 ... +32767.9999847412
```

---

# 32. Q16.16基本常量

```text
0.0 = 0x00000000
0.5 = 0x00008000
1.0 = 0x00010000
2.0 = 0x00020000
-1.0 = 0xFFFF0000
```

---

# 33. Q16.16 Floor

对于 signed Q16.16 `q`：

\[
floor(q)=q >>> 16
\]

要求：

> Arithmetic Right Shift

例如：

```text
q = -0.25
floor(q) = -1
frac = 0.75
```

---

# 34. Fraction

定义：

```text
frac16 = q[15:0]
```

其对应：

\[
f=\frac{frac16}{65536}
\]

即使q为负，配合Arithmetic Floor仍成立。

---

# 35. UV有效范围

V0.1要求 Driver / Front-End 确保：

- 评估后的 U/V 不发生 signed32 Q16.16 overflow；
- 合法源采样区域能在 Q16.16 范围表达。

超大纹理 >32768 Texel 不属于V0.1目标。

---

# 36. Fragment UV语义

`fragment_if.u/v` 表示：

> **Texture Texel-Center Coordinate**

定义：

```text
Texel 0 center -> 0.0
Texel 1 center -> 1.0
Texel N center -> N.0
```

这是整个Scaler/Sampler最关键的坐标约定。

---

# 37. Pixel Center Convention

目标Pixel同样以中心采样。

目标局部像素：

```text
i = 0...DST_W-1
j = 0...DST_H-1
```

其中心映射使用：

\[
i+0.5
\]

\[
j+0.5
\]

---

# 38. 1:1 BLIT UV

对于：

```text
SRC_X
SRC_Y
```

定义：

\[
U0=SRC_X
\]

\[
V0=SRC_Y
\]

Q16.16：

```text
U0 = SRC_X << 16
V0 = SRC_Y << 16
```

步进：

```text
DU_DX = +65536
DV_DX = 0

DU_DY = 0
DV_DY = +65536
```

因此：

> 1:1 BLIT 精确采样整数Texel Center。

---

# 39. Generic Affine UV

目标局部坐标：

```text
i = x - dst_x
j = y - dst_y
```

定义：

\[
u(i,j)=U0+i\cdot DU\_DX+j\cdot DU\_DY
\]

\[
v(i,j)=V0+i\cdot DV\_DX+j\cdot DV\_DY
\]

所有值均为：

> Q16.16 Integer Arithmetic

---

# 40. Affine语义计算方式

Golden Reference：

使用至少64-bit intermediate：

```text
u64 = U0 + i*DU_DX + j*DU_DY
v64 = V0 + i*DV_DX + j*DV_DY
```

然后要求：

```text
INT32_MIN <= result <= INT32_MAX
```

合法Command不得溢出。

RTL可以用增量累加器，但最终必须等价于上述已量化系数的整数线性表达式。

---

# 41. Scale Pixel Mapping

对于源矩形：

```text
SRC_X, SRC_Y, SRC_W, SRC_H
```

目标：

```text
DST_W, DST_H
```

无Flip时采用：

> **Pixel-center / align_corners=false 风格**

水平：

\[
u(i)=
SRC_X - \frac12
+
\left(i+\frac12\right)
\frac{SRC_W}{DST_W}
\]

垂直：

\[
v(j)=
SRC_Y - \frac12
+
\left(j+\frac12\right)
\frac{SRC_H}{DST_H}
\]

---

## 41.1 性质

当：

```text
SRC_W = DST_W
SRC_H = DST_H
```

得到：

```text
u(i)=SRC_X+i
v(j)=SRC_Y+j
```

即与1:1 BLIT完全一致。

---

# 42. Signed Round Division Helper

Driver/Golden计算Q16.16系数时统一使用：

> **Round-to-Nearest, Tie Away From Zero**

定义：

对于：

```text
den > 0
```

```text
ROUND_DIV_SIGNED(num, den):

if num >= 0:
    return (num + den/2) / den
else:
    return - ((-num + den/2) / den)
```

其中 `/` 为整数向下截断正数除法。

---

# 43. Scaling DU / DV

水平：

\[
DU\_DX =
ROUND\_DIV\_SIGNED(
SRC_W\times65536,
DST_W
)
\]

垂直：

\[
DV\_DY =
ROUND\_DIV\_SIGNED(
SRC_H\times65536,
DST_H
)
\]

其余：

```text
DV_DX = 0
DU_DY = 0
```

---

# 44. Scaling U0 / V0

使用精确Pixel-center公式后分别量化：

\[
U0=
(SRC_X<<16)
+
ROUND\_DIV\_SIGNED(
(SRC_W-DST_W)\times32768,
DST_W
)
\]

\[
V0=
(SRC_Y<<16)
+
ROUND\_DIV\_SIGNED(
(SRC_H-DST_H)\times32768,
DST_H
)
\]

注意：

> U0/V0 不通过“先量化DU再除2”计算。

这样Driver、Golden、RTL不会因二次舍入产生差异。

---

# 45. Horizontal Flip Scale Mapping

Flip X 时：

\[
u(i)=
SRC_X+SRC_W-\frac12
-
\left(i+\frac12\right)
\frac{SRC_W}{DST_W}
\]

因此：

\[
DU\_DX =
-
ROUND\_DIV\_SIGNED(
SRC_W\times65536,
DST_W
)
\]

\[
U0=
((SRC_X+SRC_W)<<16)
-32768
-
ROUND\_DIV\_SIGNED(
SRC_W\times32768,
DST_W
)
\]

---

# 46. Vertical Flip

同理：

\[
v(j)=
SRC_Y+SRC_H-\frac12
-
\left(j+\frac12\right)
\frac{SRC_H}{DST_H}
\]

---

# 47. Nearest Sampling

给定：

```text
u_q16
v_q16
```

Nearest Texel Index：

\[
x_n=floor(u+0.5)
\]

\[
y_n=floor(v+0.5)
\]

Q16.16等价：

```text
x_n = arithmetic_floor((u_q16 + 0x00008000) / 65536)
y_n = arithmetic_floor((v_q16 + 0x00008000) / 65536)
```

---

## 47.1 Tie规则

恰好：

```text
frac = 0.5
```

选择：

> **较大的整数坐标**

即：

```text
N + 0.5 -> N + 1
```

---

# 48. Bilinear Sampling

给定U/V。

定义：

```text
x0 = floor(u)
y0 = floor(v)

fx = frac16(u)
fy = frac16(v)

x1 = x0 + 1
y1 = y0 + 1
```

采样：

```text
P00 = tex(x0,y0)
P10 = tex(x1,y0)
P01 = tex(x0,y1)
P11 = tex(x1,y1)
```

---

# 49. LERP16

对8-bit通道：

```text
a,b ∈ [0,255]
f ∈ [0,65535]
```

定义：

\[
LERP16(a,b,f)=
\left\lfloor
\frac{
a(65536-f)+bf+32768
}{
65536
}
\right\rfloor
\]

等价：

```text
num = a*(65536-f) + b*f + 32768
out = num >> 16
```

---

# 50. Bilinear两级插值

对每个 RGBA Channel：

```text
H0 = LERP16(P00, P10, fx)
H1 = LERP16(P01, P11, fx)

OUT = LERP16(H0, H1, fy)
```

V0.1 **冻结水平先、垂直后，并在每一级Round**。

不得改成一次四项大乘加后只Round一次。

---

# 51. Bilinear Alpha

Alpha通道：

> 与RGB完全相同进行Bilinear。

因此带Per-Pixel Alpha Texture在缩放时：

- 边缘Alpha平滑；
- 推荐配合Straight或Premultiplied Alpha。

---

# 52. Address Domain

V0.1 Texture Sample Domain定义为：

> **Source Rectangle**

即：

```text
X domain:
[SRC_X, SRC_X + SRC_W - 1]

Y domain:
[SRC_Y, SRC_Y + SRC_H - 1]
```

而不是整个潜在Sprite Sheet。

这使 ISA 不需要额外传完整 Texture Height。

---

# 53. Texture Memory Address

对于最终合法整数Texel坐标：

```text
tx, ty
```

地址：

\[
addr=
SRC\_BASE
+
ty\times SRC\_STRIDE
+
tx\times BPP
\]

INDEX8：

```text
BPP=1
```

RGB565：

```text
BPP=2
```

ARGB/XRGB8888：

```text
BPP=4
```

---

# 54. ADDR_CLAMP

对于Source Rectangle：

```text
xmin = SRC_X
xmax = SRC_X + SRC_W - 1

ymin = SRC_Y
ymax = SRC_Y + SRC_H - 1
```

定义：

```text
tx = CLAMP(tx_raw, xmin, xmax)
ty = CLAMP(ty_raw, ymin, ymax)
```

---

# 55. ADDR_REPEAT

Repeat相对于Source Rectangle起点。

定义数学Modulo：

```text
mod_pos(a,n) = ((a % n) + n) % n
```

然后：

```text
tx = SRC_X + mod_pos(tx_raw - SRC_X, SRC_W)
ty = SRC_Y + mod_pos(ty_raw - SRC_Y, SRC_H)
```

要求：

```text
SRC_W > 0
SRC_H > 0
```

---

# 56. ADDR_MIRROR

V0.1：

> Opcode/Enum保留，数值语义暂不进入Competition Freeze。

请求：

```text
ADDR_MIRROR
```

若 `CAP_ADDR_MIRROR=0`：

> Fault。

---

# 57. Bilinear Edge规则

Address Mode分别作用于：

```text
x0
x1
y0
y1
```

例如Clamp：

边缘处：

```text
x0 = xmax
x1 = xmax
```

因此自然退化为边缘复制。

---

# 58. Source Rectangle合法性

若：

```text
SRC_W == 0
or
SRC_H == 0
```

Draw定义：

> No-op

不进行Texture访问。

---

# 59. Destination Zero Size

若：

```text
DST_W == 0
or
DST_H == 0
```

定义：

> No-op

---

# 60. Clip Arithmetic

所有Clip使用Half-Open：

```text
[xmin,xmax)
[ymin,ymax)
```

有效 Raster Bounds：

\[
R=
DrawRect
\cap ClipRect
\cap SurfaceRect
\]

Tile：

再交：

\[
TileRect
\]

---

# 61. Clip不改变UV映射原点

这是关键规则。

如果一个Sprite：

```text
dst_x = -10
```

左侧被Clip掉10像素，

剩余屏幕第一个像素：

> 必须仍然采样原Sprite的第10个目标位置对应UV。

因此：

UV永远根据原始：

```text
i = x - dst_x
j = y - dst_y
```

计算。

不能：

> Clip以后把U0重新当成0。

---

# 62. Alpha + RGB565 Destination

Destination RGB565 Decode：

```text
D.a = 255
```

因此Straight Alpha输出：

```text
O.a = 255
```

最终写回RGB565时丢弃Alpha。

---

# 63. Alpha + Tile RGB565 Compatibility

Tile BRAM中保存：

> RGB565量化后Expand的RGBA8888

因此每次Destination Read：

```text
D.a = 255
```

与Immediate一致。

---

# 64. Per-Pixel Alpha + RGB565 Texture

RGB565 Source：

```text
S.a = 255
```

因此：

```text
PIXEL_ALPHA_EN=1
```

不会改变结果。

---

# 65. Per-Pixel Alpha + XRGB8888

同理：

```text
S.a = 255
```

---

# 66. Per-Pixel Alpha + Indexed8

Alpha来自：

> Palette Entry.A

因此Palette可实现透明/半透明Sprite。

---

# 67. Color Key + Indexed8

顺序：

```text
Index
 ↓
Palette RGBA
 ↓
Filter
 ↓
Color Key RGB compare
```

不是比较 Index 值。

未来若需要：

> Index Key

应定义独立Feature，不复用Color Key语义。

---

# 68. Straight Alpha与Bilinear透明边缘

建议美术资源：

> 边缘透明Texel的RGB应做合理填充/扩展

避免Straight Alpha Bilinear时出现暗边/彩边。

更高质量方案：

> Premultiplied Alpha Texture

已在ISA/Pixel Pipeline预留。

---

# 69. Premultiplied Texture与Bilinear

Premultiplied RGBA：

> 四通道可直接Bilinear。

由于RGB已经乘Alpha，通常能改善透明边缘插值。

随后使用：

```text
BLEND_PREMULT_ALPHA
PREMULT_SRC=1
```

---

# 70. Additive + Bilinear

Bilinear先得到：

```text
S_rgba
```

再计算：

- Color Mod；
- Aeff；
- Additive。

---

# 71. Color Mod + Bilinear

顺序固定：

```text
Neighbor Decode
 ↓
Bilinear
 ↓
Color Key
 ↓
Color Mod
```

Color Mod不参与4个邻居采样前的计算。

---

# 72. Coverage

2D主体：

```text
coverage = 255
```

未来Triangle：

Coverage作为最后一个Opacity因子：

```text
A0 = source
A1 = * mod
A2 = * global
A3 = * coverage
```

---

# 73. Depth初步数值语义

P3扩展预留。

V0.1预定义：

> 32-bit Unsigned Normalized-like Integer Depth

数值：

```text
0x00000000 = Near
0xFFFFFFFF = Far
```

---

# 74. Depth Compare Enum

初步冻结：

| Value | Compare |
|---:|---|
| `0` | ALWAYS |
| `1` | LESS |
| `2` | LEQUAL |
| `3` | GREATER |
| `4` | GEQUAL |
| others | Reserved |

Competition 3D探索至少：

- ALWAYS；
- LESS；
- LEQUAL。

---

# 75. Depth Test顺序

```text
Color Key
 ↓
Alpha Factors
 ↓
Depth Test
 ↓ pass
Destination Color Read
 ↓
Blend
```

若Depth Fail：

> Color与Depth均不更新。

Depth Write Enable等更详细语义将在Triangle/Depth扩展规格中冻结。

---

# 76. Alpha Test

V0.1：

> 不定义独立Alpha Test。

透明像素可通过：

- Color Key；
- Aeff=0导致Blend无变化；

处理。

如未来需要 Alpha Test Threshold：

定义新Feature。

---

# 77. NaN / Float

GPU V0.1 Pixel Pipeline：

> 不使用浮点。

所有：

- UV；
- Alpha；
- Bilinear；
- Blend；

均为整数/定点。

因此无：

- NaN；
- Inf；
- Denormal。

---

# 78. 颜色空间

V0.1所有RGB算术直接作用于：

> **8-bit Stored RGB Values**

不进行：

- sRGB→Linear；
- Linear→sRGB。

即：

> 非Gamma-correct Blend。

这是嵌入式2D GPU Competition Profile的明确设计选择。

---

# 79. Golden Model Reference Helper

推荐 C++：

```cpp
static inline uint8_t sat_u8(int x)
{
    if (x < 0) return 0;
    if (x > 255) return 255;
    return static_cast<uint8_t>(x);
}

static inline uint8_t div255_rn(uint32_t x)
{
    return static_cast<uint8_t>((x + 127u) / 255u);
}

static inline uint8_t mul8_rn(uint8_t a, uint8_t b)
{
    return div255_rn(static_cast<uint32_t>(a) * b);
}

static inline uint8_t lerp8(
    uint8_t a,
    uint8_t b,
    uint8_t t)
{
    uint32_t x =
        static_cast<uint32_t>(a) * (255u - t) +
        static_cast<uint32_t>(b) * t;

    return div255_rn(x);
}
```

---

# 80. Hardware DIV255 Reference

允许RTL：

```systemverilog
tmp = x + 16'd128;
out = (tmp + (tmp >> 8)) >> 8;
```

前提：

- x合法范围；
- 位宽不截断。

---

# 81. RGB565 Decode Reference

```cpp
static inline uint8_t expand5(uint8_t v)
{
    return static_cast<uint8_t>((v << 3) | (v >> 2));
}

static inline uint8_t expand6(uint8_t v)
{
    return static_cast<uint8_t>((v << 2) | (v >> 4));
}
```

---

# 82. RGB565 Encode Reference

```cpp
static inline uint16_t pack_rgb565(
    uint8_t r,
    uint8_t g,
    uint8_t b)
{
    uint32_t r5 = (uint32_t(r) * 31u + 127u) / 255u;
    uint32_t g6 = (uint32_t(g) * 63u + 127u) / 255u;
    uint32_t b5 = (uint32_t(b) * 31u + 127u) / 255u;

    return uint16_t(
        (r5 << 11) |
        (g6 << 5) |
        b5);
}
```

---

# 83. Straight Blend Reference

```cpp
static inline uint8_t blend_straight_chan(
    uint8_t s,
    uint8_t d,
    uint8_t a)
{
    uint32_t num =
        uint32_t(s) * a +
        uint32_t(d) * (255u - a);

    return div255_rn(num);
}
```

---

# 84. Output Alpha Reference

```cpp
static inline uint8_t source_over_alpha(
    uint8_t a_src,
    uint8_t a_dst)
{
    return uint8_t(
        a_src +
        mul8_rn(a_dst, uint8_t(255u - a_src))
    );
}
```

---

# 85. Effective Alpha Reference

```cpp
static inline uint8_t effective_alpha(
    uint8_t src_a,
    bool pixel_alpha_en,
    uint8_t mod_a,
    bool mod_en,
    uint8_t global_a,
    bool global_en,
    uint8_t coverage)
{
    uint8_t a =
        pixel_alpha_en ? src_a : 255;

    if (mod_en)
        a = mul8_rn(a, mod_a);

    if (global_en)
        a = mul8_rn(a, global_a);

    a = mul8_rn(a, coverage);

    return a;
}
```

2D：

```text
coverage=255
```

---

# 86. LERP16 Reference

```cpp
static inline uint8_t lerp16(
    uint8_t a,
    uint8_t b,
    uint16_t f)
{
    uint32_t num =
        uint32_t(a) * (65536u - uint32_t(f)) +
        uint32_t(b) * uint32_t(f) +
        32768u;

    return uint8_t(num >> 16);
}
```

---

# 87. Bilinear Reference

```cpp
static inline uint8_t bilerp_chan(
    uint8_t p00,
    uint8_t p10,
    uint8_t p01,
    uint8_t p11,
    uint16_t fx,
    uint16_t fy)
{
    uint8_t h0 = lerp16(p00, p10, fx);
    uint8_t h1 = lerp16(p01, p11, fx);
    return lerp16(h0, h1, fy);
}
```

RGBA四通道分别执行。

---

# 88. Q16.16 Floor Reference

Golden Model禁止依赖 C++ 对负数右移的非严格可移植历史语义。

推荐显式函数：

```cpp
static inline int32_t floor_q16_16(int32_t q)
{
    int64_t x = q;

    if (x >= 0)
        return int32_t(x / 65536);

    return -int32_t(((-x) + 65535) / 65536);
}
```

Fraction：

```cpp
static inline uint16_t frac_q16_16(int32_t q)
{
    return uint16_t(uint32_t(q) & 0xFFFFu);
}
```

RTL使用Arithmetic Shift可得到相同Floor语义。

---

# 89. Nearest Reference

```cpp
static inline int32_t nearest_index_q16(int32_t q)
{
    int64_t t = int64_t(q) + 32768;
    return floor_q16_16(int32_t(t));
}
```

实现时必须避免 `q + 32768` 的32-bit overflow；Golden使用64-bit检查。

---

# 90. Signed Round Division Reference

```cpp
static inline int64_t round_div_signed(
    int64_t num,
    int64_t den)
{
    // den > 0
    if (num >= 0)
        return (num + den / 2) / den;
    else
        return - ((-num + den / 2) / den);
}
```

---

# 91. Scale Coefficient Reference

```cpp
du_dx =
    round_div_signed(
        int64_t(src_w) * 65536,
        dst_w);

u0 =
    (int64_t(src_x) << 16) +
    round_div_signed(
        int64_t(src_w - dst_w) * 32768,
        dst_w);
```

Y同理。

---

# 92. Flip X Reference

```cpp
du_dx =
    -round_div_signed(
        int64_t(src_w) * 65536,
        dst_w);

u0 =
    (int64_t(src_x + src_w) << 16)
    - 32768
    - round_div_signed(
        int64_t(src_w) * 32768,
        dst_w);
```

---

# 93. Arithmetic Intermediate Bit Width建议

规格定义结果，不强制RTL内部完全相同位宽，但建议：

| 运算 | 最小安全位宽 |
|---|---:|
| 8×8 | 16 bit |
| 两个Blend Product求和 | 17 bit |
| DIV255 helper临时 | 17 bit |
| Bilinear 8×16 | 24 bit |
| 两项Bilinear求和 | 25 bit |
| Q16.16 + i×step | 64 bit语义参考 |
| Address y×stride | 至少48/64 bit内部检查 |
| RGB565 quant product | 14 bit足够，建议16+ |

---

# 94. RTL可优化但必须等价

允许：

- DSP；
- LUT；
- Shift/Add；
- Pipelining；
- Parallel Lane；
- Resource Sharing；

但禁止通过：

- 降精度；
- 改Round；
- 改插值顺序；

换取性能而不升级规格版本。

---

# 95. Pixel Arithmetic Unit Test集合

必须建立。

---

## 95.1 RGB565

穷举：

> 全部65536个RGB565值

测试：

```text
decode
encode(decode(x)) == x
```

---

## 95.2 MUL8

穷举：

```text
256 × 256 = 65536 cases
```

Golden / RTL全部比对。

---

## 95.3 Straight Alpha

至少：

- S/D 黑白极值；
- A=0；
- A=1；
- A=127；
- A=128；
- A=254；
- A=255；
- Random。

---

## 95.4 Effective Alpha

组合：

- Pixel Alpha OFF/ON；
- Mod OFF/ON；
- Global OFF/ON；
- Coverage边界。

---

## 95.5 Bilinear

测试：

- fx/fy=0；
- 0x8000；
- 0xFFFF；
- 单色；
- 四角极值；
- RGBA；
- Random。

---

# 96. Exhaustive建议

以下模块输入空间较小，建议直接穷举：

- RGB565 Decode；
- RGB565 Encode部分映射；
- MUL8_RN；
- DIV255_RN；
- Alpha Channel Blend；
- LERP16选定关键f或分层遍历。

---

# 97. Full-Frame Arithmetic Regression

固定场景至少包括：

### Frame A
RGB565 opaque sprite。

### Frame B
ARGB8888 alpha edges。

### Frame C
100层半透明overdraw。

### Frame D
Bilinear upscale/downscale。

### Frame E
Indexed8 palette。

### Frame F
Additive particles。

### Frame G
Clip + negative coordinates。

### Frame H
Immediate vs Tile Compatibility。

---

# 98. Immediate vs Tile Exact Test

对于 Compatibility Mode：

要求：

```text
Immediate RGB565 Frame
==
Tile RGB565 Frame
```

逐Pixel：

> 完全一致。

测试必须覆盖：

- Alpha；
- Additive；
- Color Key；
- Scaling；
- Bilinear；
- Dither。

---

# 99. 高精度Tile模式

V0.1仅预留概念：

```text
CAP_TILE_HIGH_PRECISION
```

不进入Competition Baseline。

如果未来实现：

- Tile内部不做每Draw RGB565量化；
- 最终Store才量化；

则必须：

- 作为不同Render Mode明确报告；
- 不拿它与Immediate做“像素精确等价”测试；
- 单独评估图像质量和性能。

---

# 100. Golden Model错误处理

Golden Model若发现：

- Q16.16 overflow；
- 地址越界；
- 非法Format；
- 非法Blend组合；
- Bilinear + invalid source size；

必须：

> 抛出与RTL Fault语义对应的错误。

不能自动“帮硬件修正参数”。

---

# 101. Strict Mode数值检查

推荐 Strict 时检查：

### BLEND_STRAIGHT_ALPHA
```text
PREMULT_SRC == 0
```

### BLEND_PREMULT_ALPHA
```text
PREMULT_SRC == 1
PIXEL_ALPHA_EN == 1
```

### BLEND_ADD_SAT
```text
PREMULT_SRC == 0
```

### DITHER
```text
DST_FORMAT == RGB565
```

### INDEX8
```text
PALETTE_EN == 1
```

### Bilinear
```text
CAP_BILINEAR == 1
```

---

# 102. Pixel Format状态合法组合

## RGB565 Source

允许：

- Nearest；
- Bilinear；
- Key；
- Global Alpha；
- Color Mod；
- Additive。

Per-Pixel Alpha：

> 恒为255。

---

## ARGB8888 Source

完整支持。

---

## XRGB8888 Source

与ARGB类似，但Alpha恒255。

---

## INDEX8

要求：

```text
PALETTE_EN = 1
```

Palette提供RGBA。

---

# 103. Render Target合法格式

Competition V0.1：

- RGB565：必须；
- ARGB8888：架构支持，可分阶段实现；
- XRGB8888：架构支持；
- INDEX8：不支持Destination。

---

# 104. Dither与Alpha

顺序：

```text
Blend RGBA8888
 ↓
Dither / Quantize RGB565
 ↓
Store
```

Dither不能：

> 作用于Source Alpha或Blend前Source Color。

---

# 105. Dither与重复Blend

Compatibility模式：

每次写RGB565逻辑Target：

> 都执行Dither。

因为Immediate真正每次都会写RGB565。

因此同一Pixel在多层Blend中可能多次经过Dither/Quantize。

Tile必须复现。

---

# 106. Format Conversion与Perf Counter

建议统计：

```text
RGB565_ENCODE_COUNT
RGB565_DECODE_COUNT
BILINEAR_PIXEL_COUNT
ALPHA_PIXEL_COUNT
ADD_PIXEL_COUNT
```

不是必须全部暴露给软件，但便于调试性能。

---

# 107. 性能实现注意事项

本文不规定时序，但建议：

### RGB565 Decode
纯组合/1级。

### MUL8_RN
DSP或乘法+DIV255快速式。

### Straight Alpha
可2～4级Pipeline。

### Bilinear
水平与垂直分级Pipeline。

### Format Encode
写回前独立Stage。

Pipeline Latency：

> 可以改变，但结果不得改变。

---

# 108. Multi-Lane一致性

无论：

```text
GPU_LANES=1
2
4
```

对同一Command：

> 输出Framebuffer必须位精确一致。

Lane只是并行度变化。

---

# 109. 多Lane Pixel顺序

同packet：

```text
lane0 -> smallest X
lane1 -> X+1
...
```

对于同一Pixel，不存在跨Lane数值依赖。

---

# 110. Additive/Alpha与Draw Order

Draw顺序保持：

> ISA / Tile Work List顺序。

Pixel Arithmetic不允许通过重排透明Draw提高性能。

---

# 111. Math Version

本文定义：

```text
PIXEL_ARITH_VERSION = 0x01
```

建议硬件：

- Capability / ID Register；
- Golden Log；

记录此版本。

这样未来若改变Round规则，可明确区分结果。

---

# 112. Reference Frame Metadata

Golden输出每个Reference Frame时，建议保存：

```text
ISA_VERSION
PIXEL_ARITH_VERSION
FRAMEBUFFER_FORMAT
WIDTH
HEIGHT
DITHER
TILE_MODE
RANDOM_SEED
COMMAND_SHA256
```

确保几年后仍可复现。

---

# 113. 规格优先级冲突处理

如果：

- Command ISA；
- Internal Interface；
- Pixel Arithmetic；

在数值行为上存在冲突：

> **Pixel Arithmetic Specification V0.1 是像素数值行为的最终权威。**

如果冲突涉及：

- Command字段；
- Opcode；
- 内存布局；

则 Command ISA 为权威。

---

# 114. V0.1 已冻结数值决策

以下进入Freeze：

| 项目 | V0.1 决策 |
|---|---|
| Internal Color | RGBA8888 |
| Logical Packing | 0xAARRGGBB |
| Endian | Little-Endian |
| RGB565 Decode | Bit Replication |
| RGB565 Encode | Scale-to-range + RN |
| Dither | 4×4 Bayer Ordered |
| DIV255 | Round-to-Nearest |
| MUL8 | 分阶段DIV255_RN |
| Effective Alpha | Source→Mod→Global→Coverage顺序 |
| Straight Alpha | Source-Over |
| Premult Alpha | Source-Over，禁止重复乘Source Alpha |
| Additive | Alpha-scaled Saturating Add |
| Multiply | D与S乘积后按Aeff插值 |
| Color Key | Filter后、Color Mod前，比较RGB |
| UV Format | signed Q16.16 |
| Texel Center | Integer Coordinate |
| Scale Mapping | Pixel-center / align_corners=false |
| Nearest | floor(u+0.5)，半值向较大整数 |
| Bilinear | Horizontal RN → Vertical RN |
| Bilinear Fraction | 16-bit |
| Address Domain | Source Rectangle |
| Clamp | Clamp到Source Rect |
| Repeat | 相对Source Rect正模 |
| Clip | Half-open，不重置UV原点 |
| Tile Compatibility | 每次RT写按DST_FORMAT逻辑量化 |
| RGB565 Tile Store | BRAM保存量化后Expand RGBA |
| 2D Coverage | 255 |
| Color Space | Stored RGB直接算术，非Gamma-correct |
| Depth预留 | 32-bit，0近/FFFFFFFF远 |

---

# 115. 需要回写上游架构文档的澄清项

本文发现并正式澄清一个重要交叉规格问题：

System Architecture V1.0 中“Tile内部RGBA8888、最后Store时一次量化”的描述，与：

> Immediate / Tile Pixel-Exact Compatibility

不能同时严格成立。

因此 V0.1 竞赛兼容模式正式采用：

> **Tile物理存储RGBA8888，但每次逻辑Render Target Write按DST_FORMAT量化后再Expand存入Tile BRAM。**

后续建议发布：

> `System Architecture V1.1`

将这一点同步回写。

High-Precision Tile 可作为未来独立模式。

---

# 116. 下一阶段输出

完成本文后，Gate A 的位精确基础基本具备。

下一阶段建议生成：

1. `GPU Register Map & Memory Map V0.1`
2. `Verification Plan V0.1`
3. `Golden GPU Software Architecture V0.1`
4. `Architecture Performance Model Plan V0.1`

之后正式开始：

- PC Golden GPU；
- Board Bring-up；
- Basic RTL `FILL_RECT`。

---

# 117. V0.1 一句话定义

> **Pixel Format & Arithmetic V0.1 采用RGBA8888统一内部颜色、signed Q16.16纹理坐标、Pixel-Center缩放映射、位精确Round-to-Nearest Alpha/Bilinear算法和明确的RGB565量化规则，并通过“每次Render Target写按目标格式逻辑量化”的兼容语义保证Immediate与Tile路径在相同Draw Order下能够进行逐像素一致性验证。**
