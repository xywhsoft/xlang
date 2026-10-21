# DPI 感知的长行绘制缓存

2026-10-11。SDK65 已接入工作区 C 源码，84 模块 / 99 产物严格构建与包含渲染的标准完整回归退出 0。随后仅同步 SDK65 来源描述元数据并独立重建，产品源码、构建选项和 SDK 一致；最终便携包的三项实际验收通过，见 [当前候选说明](../CURRENT-CANDIDATE.md)。

## 实现

编辑器仍以逻辑字号排版，保留精确的光标坐标。DPI 代理为物理绘制字号单独保存长行的排版检查点；插入或撤销后，从未变前缀之后继续排版。字体销毁和缩放事务成功替换字体前先释放该检查点，缩放事务失败时保留原字体与检查点。

每个字体只保留当前长段落的副本，启用范围为 64 KiB 到 32 MiB。超过范围、分配失败或提供者拒绝时继续使用普通绘制路径。绘制索引使用已有的字形范围桶和视口重放，保留字体、emoji、最终基线和度量的独立引用。逻辑字体创建过缩放字体后采用保守标记，避免为不能直接用于绘制的逻辑字体收集范围。

应用构建显式启用 `XIDE_PAINT_CURSOR_ENABLED=1`；未指定此开关时不启用物理字号缓存。公开 XGE/XUI 接口保持一致，XRT 继续由 `xge.dll` 单独拥有。C / xlang 语法规则、工作空间—项目—文件模型和定版 Dock 布局沿用现有实现。

## 原生结果

SDK62 与 SDK65 在同一组应用对象上各运行 C / xlang × 8 KiB / 16 MiB 四种情形，每种 162 项检查；SDK65 仅替换经过严格编译的 DPI 代理对象。测试覆盖末尾编辑与撤销、首尾滚动、语法切换、Unicode 路径与正文、独立长精度坐标核验，以及 125% → 150% → 200% → 100% → 125% 字体替换期间的编辑、撤销、逻辑度量、选区、版本和撤销状态。

28 对完整 IDE 截图逐像素一致。SDK65 底层绘制通过 1,736 项断言，其中有 194 对完整扫描 / 视口绘制的 GPU 像素比较、86 次缓存重放和 60 次保留索引后重新定位。372 个公开类型布局、9,081 条公开声明、5,954 个导出名称及序号一致；109 个现有 PE 文件的 46,687 处具名 XGE 导入实际核对通过。

下表记录 125% 下各一次本机观测，时间为操作之后的绘制帧。测试期间有 SDK 编译负载，数值用于定位开销，不构成稳定基准；编辑和撤销操作本身的耗时另行记录，不能只用重绘时间描述完整输入延迟。

| 16 MiB 单行 | 首次帧 ms：SDK62 → SDK65 | Ctrl+End 后 ms | 末尾插入后 ms | 撤销后 ms | Ctrl+Home 后 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| C | 1352 → 1642 | 654 → 48 | 628 → 90 | 594 → 67 | 583 → 13 |
| xlang | 1357 → 1860 | 632 → 71 | 623 → 121 | 597 → 105 | 588 → 17 |

滚动和末尾编辑后的重绘改善，首次长行绘制仍有额外建索引成本。靠近行首的改动需要重新排版后续内容；当前方案不能保证所有长行编辑都只处理少量字符。物理混合 DPI、实体输入法、干净 Windows 设备与正式发行核对继续。

## 可追溯材料

- [DPI 代理 C 实现](../src/platform/dpi_proxy.c)
- [SDK65 准备工具](../tools/prepare_dpi_paint_sdk.py)
- [原生编辑与缩放检查](../tools/check_editor_performance.py)
- [SDK65 严格构建](../build/editor-sdk-dpi-paint-3b1f3d941ddd/sdk-build.json)
- [原 SDK62 的缩放检查](../build/editor-observation-5a48c7ecc562/evidence.json)
- [SDK65 的缩放检查](../build/editor-observation-3848b0b63071/evidence.json)
- [28 对像素比较](../build/dpi-paint-comparison-v1.json)
- [底层 GPU 绘制检查](../build/viewport-paint-oracle-11784683a58b/evidence.json)
- [公开接口与实际导入检查](../build/dpi-paint-public-compatibility-v2/result.json)
- [升级前保存材料](../build/dpi-paint-baseline-v1.json)
- [源码成套升级记录](../build/dpi-paint-source-promotion-v1.json)
- [含渲染的完整回归](../build/dpi-paint-full-v1-8615d1e6/evidence.json)
- [SDK65 预处理依赖观察](../build/sdk65-materials-closure-v1/result.json)
- [来源描述同步记录](../build/dpi-paint-materials-update-v1/update.json)
- [100% 启动时的实际源码检查](../build/editor-observation-d14105b1f02a/evidence.json)
- [来源描述同步后的构建核对](../build/dpi-paint-package-build-equivalence-v1.json)
- [最终便携包的三项实际验收](../build/dpi-paint-package-native-v1.json)
- [阶段证据与保存材料](../build/dpi-paint-stage-v1.json)

SDK63 / SDK64 的独立实验与失败记录见 [视口绘制索引](viewport-paint-index.md)。
