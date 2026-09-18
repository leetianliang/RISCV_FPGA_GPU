# APP2_002 升级数字键无响应修复

> 历史单独修复记录。后续 APP2_002R 已合并此修复并扩展队列/P/R测试；当前版本和完整回归以 REPORT_APP2_002R_NARROW_CLOSURE.md 为准。

日期：2026-09-17；基线：`9c355d4`。用户反馈升级面板按 1/2/3 无反应。

## 原因与修复

交互主循环先执行 `input.clear_edges()`，之后升级逻辑才读取 `edge_1/2/3`，导致三个选项全部失效。此前 101/101 回归验证了模拟与渲染，但 headless 自动选项直接调用 apply_upgrade，未覆盖此交互输入顺序；此前通过结果不代表该路径正常。

新增 `facility_input::finish_input_frame`，在清空本帧输入前保存有效升级选择。主循环使用保存的选择应用升级。没有升级面板时不缓存选择；菜单仍独立消费输入；下一帧不会重复使用数字键事件。Win32 数字键映射增加 VK_NUMPAD1/2/3，支持开启 NumLock 的小键盘。

## 修改文件

- `model/pc_demo/app/facility_input.hpp`：本帧选择解析与输入清理。
- `model/pc_demo/app/main.cpp`：保存并使用升级选择。
- `model/pc_demo/host/presenter.cpp`：小键盘数字键支持。
- `model/pc_demo/tests/test_facility_input.cpp`：三个真实构筑选项、暂停/恢复、无重复输入、面板外按键不排队、多键优先级与持续移动键保留。
- `model/pc_demo/CMakeLists.txt`：注册输入回归。

## 验证

```powershell
cmake --build build/stage0045 --target gpu2d_test_facility_input gpu2d_test_presenter -j 1
cmake --build build/stage0045 -j 1
ctest --test-dir build/stage0045 -R 'gpu2d_test_(presenter|facility_input|facility_upgrades|facility_determinism)$' --output-on-failure
git diff --check
```

- 最终完整构建：PASS，已更新 `build/stage0045/model/pc_demo/gpu2d_demo.exe`。
- 定向回归：**4/4 PASS，0 FAIL，0.43秒**。
- 用户关闭旧进程后完成可执行文件替换。
- 编写新头文件时曾遗漏 namespace 结束括号，首次编译失败；补齐后构建通过。
- 本次完整102项回归：NOT RUN。未改变模拟、渲染或GPU语义；原101项结果是前次提交证据。
- 实际物理键盘/窗口焦点操作：尚未人工复测。自动回归覆盖主循环所用选择解析函数及真实 apply_upgrade，不声称已通过实体键盘端到端验证。

## 使用与后续

从仓库根目录重新启动上述 exe；升级面板按主键盘 1/2/3，或开启 NumLock 后使用小键盘 1/2/3。窗口应获得键盘焦点。

BLOCKER：无。DESIGN_QUESTION：无。修复尚未提交/推送；下一步由用户复测交互路径。
