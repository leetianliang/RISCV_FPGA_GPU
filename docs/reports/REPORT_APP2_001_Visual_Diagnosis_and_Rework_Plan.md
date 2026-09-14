# APP2_001 当前视觉问题诊断与返工准备

日期：2026-09-14。核对基线：`6c20f68`（已包含 R1–R8 修复提交）。

本次范围：分析现状、复核素材与代码、运行局部验证、准备下一轮修改；未修改游戏实现、源素材、冻结规格或测试期望。本报告不是 TASK_APP2_001 的最终交付报告，不代表 56 项验收完成。

## 1. 结论

当前画面与概念图的差距来自三个相互叠加的问题：**素材转换仍有正确性缺陷、没有把设施美术组织成场景、战斗运动与反馈尚不完整。** 继续往现有画面中堆敌人、武器和特效，无法有效缩小差距。

保留独立 Application、Common Graphics API、4096×4096 世界、32×32 MapTile、相机和现有战斗骨架。下一轮重点重做素材加工与 Power Test Area 场景呈现，同时修复敌人停滞；先验收可滚动的样板区，再继续 XP/升级。

概念图是包含角色设定、武器图鉴、区域示例的美术展示板。应参考其中**中央战斗场景的空间层次和主体比例**，不能把整张板的复杂度当成第一切片必须实现的功能。Boss、六武器和后期敌潮仍不属于当前任务范围。

## 2. 本次直接查看的证据

- [设计方案](../Application_2_Survivor_like_Game_Concept_and_Art_Direction_V0.1.md)：尤其第 7、51–55、59 节。
- [原任务](../tasks/TASK_APP2_001_FACILITY_OMEGA_First_Asset_Pack_and_Vertical_Slice.md)。
- [旧评审](../reviews/REVIEW_APP2_001_PARTIAL_V1_FACILITY_OMEGA.md)：只作为历史问题清单，不能代替当前检查。
- 概念图、环境/角色/武器源图、现有 processed/contact_sheet.png、已有 facility_v3_check.png。
- 本次新运行：640×360、Immediate、seed 1234、第 180 帧，无移动输入。

![本次运行的实际画面](../../results/stage0045/captures/facility_review_20260914_180.png)

本次画面可见：重复地板占满视口、黄色环线缺少空间用途、玩家周围出现矩形背景覆盖、道具零散、没有工业 HUD 和击杀特效。它是当前版本的运行证据，不是效果目标图。单张早期截图不用于证明所有后期状态。

## 3. 已经修过的项目，不应重复认定为现存故障

| 项目 | 当前证据 | 结论 |
|---|---|---|
| ARGB 字节序 | assetc 已输出 BGRA；独立 Python 字节序检查 PASS | 原红蓝交换在编码端已修；尚缺生产后端定向像素回归 |
| 上下方向 | down→engineer_b，up→engineer_a；当前测试 PASS | 映射已修，但角色裁剪仍有问题 |
| 两张敌人压成一张 | 已有 drone/crawler/runner/tank/elite 各两帧；联系表可见单个对象 | 旧问题已有改善；帧间轮廓与锚点仍需动画验收 |
| 素材完整性 | 包内 9 个清单文件校验通过，source 文件与 ZIP 字节相同 | 源图未损坏 |
| runtime 清单 | 45 项实际文件 SHA256 与清单全部一致 | 文件一致性通过，不等于素材语义正确 |

## 4. 当前问题与因果链

### P0-1：透明素材被当成不透明矩形复制

`software/graphics/include/gpu2d/types.hpp:82` 的 `SpriteParams.blend` 默认是 `Copy`。`model/pc_demo/golden_backend/golden_renderer.cpp:251` 仅在非 Copy 模式启用像素 alpha。

`software/applications/facility_omega/src/app.cpp` 的道具、敌人、玩家绘制没有设置 blend，只有 Pulse Shot 显式设置 AddSat。因此 PNG/ARGB 文件里保存了透明度，也不会自动得到透明混合；透明处存储的 RGB 会覆盖原地板。这解释了截图里精灵的矩形底色和生硬边缘。

修复：应用层对具有透明度的角色、敌人、道具明确使用已有 StraightAlpha；不透明地面保留 Copy，发光效果按实际需求使用现有 AddSat。不要修改 API 默认值或 Golden 像素语义。增加经过真实 Common API→Golden 路径的透明/半透明边缘测试，同时比较 Immediate 与 Tile32。

### P0-2：裁图仍没有按实际对象边界完成

`tools/facility_omega_assetc.py:94–121` 的环境框与实际源图格子不对齐。例如 floor_01 的 x=190、w=140 跨过第一、第二块地板；floor_04 的 x=360、w=140 横跨破损板和通风格；floor_07 的 x=530、w=140 横跨通风格与警戒条纹。联系表与场景中均能看到这类拼接碎片。

这不是源图细节不足，而是把展示图的一部分当成了独立可拼接 MapTile。将错误片段以 32 像素周期铺满地图，会把误裁放大为满屏杂乱接缝。

角色还有另一种问题：先固定裁框，再普遍删除底部 22%。例如 engineer_a0 从 y=40、高 200 裁出后只保留 156 像素，底边约在原图 y=196；源图靴子仍在更下方。此操作会截掉身体下部，随后强制拉伸到 32×32。侧面、正面不同宽高比也分别被拉成同样方形，身份和动画比例受到损伤。

修复：逐对象确认 source_box，去标签使用具体边界而非统一百分比；角色等比缩小后放入统一透明画布，以脚底/身体中心注册动画锚点。地面与角色使用不同处理规则。地面应保留明确的完整边界或经过设计的无缝连接关系。

### P1-1：场景只有贴图填充，没有设施空间组织

`sim_reset()` 虽然改成“88% base”，但该分支仍近似均匀选择 0–3 四块素材，其中含大裂纹、机械结构、污损或跨块片段。“base”命名没有使其变成低对比背景。

中央只有 8×8 图块边框，四边使用同一 floor_07，没有连续的方向、转角或入口。全世界仅硬编码 12 个道具位置，其中 grate/hazard 属于地面或标识，真正立体道具类型只有 barrel/crate/console/canister 四种。场景缺少设备群、管线连接、区域地标与明确的可通行空间。

方案第 7 节要求的基础地板、设施标识、道具、环境 FX 四层，目前远未充分落地。问题不能靠把背景整体调暗解决。

修复：先组织一段可滚动的 Power Test Area：中央留出安静战斗空间，边侧布置设备和管线，标识只出现在有用途的位置，路口和设备区形成连续关系。继续使用 4096×4096 世界和可见区剔除，不硬编码为单屏竞技场。优先利用现有箱体、终端、管段、栏杆、设备柜等源图。

### P1-2：展示分辨率扩大了视野，却没有相应的美术尺度策略

`model/pc_demo/app/main.cpp:99–106` 的 interactive 为 640×360，showcase 为 1280×720；FACILITY 将这个尺寸直接作为世界视口，精灵仍按原始尺寸绘制。

同一个 32 像素角色，占 360 高视口约 8.9%，占 720 高视口仅约 4.4%。showcase 同时展示约四倍世界面积，角色、子弹、道具和文字都显得更小，有限敌人数也更稀疏。它不是同构图的高清截图。

当前窗口层已按 framebuffer 的两倍尺寸显示，因此 640×360 interactive 本来就能以 1280×720 窗口呈现。下一轮先用已有 interactive 作为美术验收基准，原生截图和展示尺寸分开标注。暂不为改善截图而扩大原生视野，也不修改世界尺寸。原生 720p 的美术尺度与预算作为后续独立决策。

### P0-3：慢速敌人在斜向位置会停滞

`app.cpp:235–250` 为 Crawler/Tank 选择速度 1，再执行整数计算 `(dx*sp)/len`、`(dy*sp)/len`。例如 dx=300、dy=200、整数长度 360，两轴结果都是 0；玩家不动时，该位置会持续不动。

这不仅是旧评审提到的逐单位 sqrt 性能问题，还会直接破坏敌人追击、进场与战斗密度。不能通过提高生成数量掩盖。

修复：采用应用层确定性的子像素累计或等价的整数余量方法，避免每帧丢弃小数位移；距离计算使用有界迭代。加入斜向、轴向、近距离、不同方向和长期累计位移回归。此处不涉及 GPU Q16.16 定义变更。

### P1-3：战斗反馈与工业 HUD 尚未接入

目前击杀只更新 alive/kills，渲染函数没有使用已经加工的 explosion/spark；敌人受击通过色彩乘法表现，尚无完整击杀反馈。Pulse Shot 8×8 画布中的亮核很小，在复杂背景上不明显。

HUD 仍是两行小字，包含 EN/BL/CAM 等调试字段。UI source 被加载到离线脚本，但 CROPS 中没有 UI 项。XP、升级暂停选择、完整时间/武器 HUD 均不能按完成项报告。

下一轮先补现有 Pulse Shot 的发射—命中—死亡反馈和基本 HP/时间/击杀/武器展示；XP 与升级展示真实状态，按后续 Block I 完成，不在截图里填写虚假等级或战果。暂不扩展六武器和 Boss。

### P1-4：当前验证机制会给人错误的完成感

- 联系表注释声称有标签，但实际使用空占位，未绘制名称；thumbnail 也不会将小图片放大，8 像素素材仍难审核。
- assetc 无参数模式仍写 preview、contact、manifest、audit；已有 raw 没有更新时，清单 SHA 却来自新生成字节，未来可能造成清单与旧文件不一致。本次现有 45 项哈希一致，不能因此忽略设计缺陷。
- assetc 默认运行被登记为 CTest，普通测试可能修改检查输出。本次刻意未运行该测试。
- 字节序测试仅检查 Python 输出并模拟解码，没有调用生产后端。
- facility 测试检查相机、数量、名称和部分击杀，但缺少慢速敌人斜向追击、alpha 覆盖和裁图语义；找不到 runtime 时只打印 WARN 也可 PASS。

改进：默认 verify 只读比较并在差异时非零退出，只有显式 update 才写输出；联系表附名称、尺寸、格式、锚点，提供原尺寸与放大查看。功能回归与视觉验收分别通过，不能互相代替。

## 5. 下一轮工作安排

详细执行草案见 [视觉返工计划](../tasks/TASK_APP2_001_R2_Visual_Rework_Plan.md)。推荐顺序：

1. 修复透明混合、敌人停滞和只读验证机制，建立可信基础。
2. 重新裁剪角色、三类敌人、弹丸与环境关键素材；完成有标签的联系表。
3. 在现有世界中制作可滚动 Power Test Area 样板区，先验收安静地面、设备区、标识和主体尺度。
4. 接入现有战斗反馈与基础 HUD，固定种子和输入路径生成真实战斗截图。
5. 样板区视觉复核通过后继续 XP/三级选项等原任务剩余项，最后执行完整验收。

这轮不要求新增美术生成。现有源图足够先修复严重问题并搭建样板区；若完整墙体、设备组合或循环动画确实无法从源图可靠制作，再列出精确的缺件尺寸、用途和参考图，进入独立补图决策。不能沿用旧报告“绝对不需要新素材”的泛化结论。

## 6. 本次运行与结果

工作目录为仓库根目录。以下为本次核心构建、测试和捕获的实际命令：

```powershell
cmake --build build/stage0045 --target gpu2d_demo gpu2d_test_facility -j 4
.\build\stage0045\model\pc_demo\gpu2d_test_facility.exe
python -B scripts/check_app2_argb_order.py
.\build\stage0045\model\pc_demo\gpu2d_demo.exe --app facility --headless --frames 180 --seed 1234 --backend immediate --profile interactive --capture results/stage0045/captures/facility_review_20260914_180.raw
python -B -c 'from PIL import Image; p="results/stage0045/captures/facility_review_20260914_180"; Image.open(p+".ppm").save(p+".png")'
```

结果：构建提示 `ninja: no work to do.`；facility 测试加载 45 个素材并 PASS；BGRA 检查 PASS；180 帧 headless 正常退出并生成 raw/PPM。末帧报告 base_cmds=403、base_sprites=402。PNG 仅为 PPM 无损格式转换以供查看。

素材核验与整数算式复核的实际 Python 命令：

```powershell
python -B -c 'from PIL import Image; from pathlib import Path; import json,hashlib,zipfile; root=Path.cwd(); base=root/"assets/facility_omega"; m=json.loads((base/"runtime/facility_omega_assets.json").read_text()); bad=[s["name"] for s in m["sprites"] if hashlib.sha256((base/"runtime"/s["runtime_file"]).read_bytes()).hexdigest()!=s["sha256"]]; print("runtime count",len(m["sprites"]),"hash mismatches",bad); print("UI candidates",[s["name"] for s in m["sprites"] if s["source_sheet"]=="ui_source.png"]); print("source dimensions",[(p.name,Image.open(p).size) for p in (base/"source/source_sheets").glob("*.png")]); z=zipfile.ZipFile(root/"docs/tasks/FACILITY_OMEGA_First_Asset_Pack_V0.1.zip"); prefix="FACILITY_OMEGA_First_Asset_Pack_V0.1/"; man=json.loads(z.read(prefix+"MANIFEST.json")); checks=[(f["path"],hashlib.sha256(z.read(prefix+f["path"])).hexdigest()==f["sha256"],(base/"source"/f["path"]).read_bytes()==z.read(prefix+f["path"])) for f in man["files"]]; print("source integrity",checks); print("speed 1 dx=300 dy=200 len=360 step",int(300/360),int(200/360))'
```

素材核验结果：9/9 源文件通过、45/45 runtime 哈希通过、UI 候选为空、速度示例 `(0,0)`。

## 7. 文件、限制与待决事项

本次新增：本报告；`docs/tasks/TASK_APP2_001_R2_Visual_Rework_Plan.md`；`results/stage0045/captures/facility_review_20260914_180.{raw,ppm,png}`。原有未跟踪的旧评审和截图保留。未提交或推送 Git。

警告：本次只跑了上述局部验证；没有跑完整 Golden/NEON 回归、600 帧切片验收、60 帧双后端相等测试，不能据此宣告整个 APP2 PASS。敌人停滞由当前 C++ 算法和同值整数算式确认，尚未新增执行该分支的 C++ 专项回归。现有动画帧需连续播放审核。

检查工具限制：首次尝试的 `model/pc_demo/src/main.cpp` 和 `software/platform` 路径不存在，随后按实际路径读取；PowerShell 裸 `*.hpp` 路径搜索失败后改用 `rg -g '*.hpp'`。图像查看器不支持 PPM，改为无损 PNG 后完成视觉检查。

BLOCKER：本次分析与返工准备无阻塞项。当前视觉不满足验收，不应关闭原任务。

DESIGN_QUESTION：原生 1280×720 最终要扩大视野还是保持相同构图，需要在后续原生 720p 方案中明确；本轮使用既有 640×360 interactive，不依赖该决策。若新增墙体带有阻挡碰撞或需补充源图，应另行明确范围，不把视觉道具自动升级为新玩法。
