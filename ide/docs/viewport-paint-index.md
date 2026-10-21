# 极长行的视口绘制索引

2026-10-11。本阶段完成时，可运行候选采用 SDK62。本阶段实现和验证了两种独立的 C / XGE 绘制实验，当时的候选、工作区产品源码、工具链和历史材料保持原样。后续 SDK65 已处理物理字号缓存，并接入产品源码，见 [DPI 绘制缓存](dpi-paint-cache.md)与 [当前候选](../CURRENT-CANDIDATE.md)。C / xlang 双语法及定版 Dock 布局继续可用。

## 当前结果

SDK63 为长文本建立每 4 KiB 一个桶的实际字形范围，保留完整行的最终基线、字体与 emoji 引用以及排版检查点。视口改变后，只重新排版与视口相交的桶；同一视口继续使用最多 4,096 条可见绘制记录。范围包含 SVG 溢出与可能的回退字形，不依赖画笔位置单调递增；未知范围、记录过多或分配失败时使用完整绘制路径。

当前 84 个应用对象分别加载原 SDK62 与实验 SDK63，实际打开 UTF-8 中文路径下的 C / xlang 文件。每版分别测量 8 KiB、16 MiB 单行文件，共八个原生窗口、840 项检查，12 对完整 IDE 截图逐像素相同。切换语法保留文档身份、版本、正文长度、清洁状态、选区、滚动及撤销 / 重做状态；独立的长精度累加与末尾字符点击检查继续通过。

以下为本机各一次观测，未隔离后台负载。列中的时间仅为操作之后的 `xideAppFrame`；Ctrl+End 本身仍需约 172–175 ms，插入 / 撤销本身仍需约 400–466 ms，因此这些数值不代表完整输入响应时间。

| 16 MiB 单行 | 首次帧 ms：SDK62 → SDK63 | Ctrl+End 后重绘 ms | Ctrl+Home 后重绘 ms | 插入后重绘 ms | 撤销后重绘 ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| C | 1293 → 1438 | 629 → 42 | 589 → 11 | 618 → 751 | 596 → 700 |
| xlang | 1325 → 1451 | 616 → 44 | 581 → 14 | 610 → 743 | 588 → 723 |

滚动后的重绘明显改善，首次绘制和编辑后首次绘制仍有回退。SDK63 保持实验状态，尚未进入当前便携候选，也未通过整体性能验收。

## 首次绘制复用实验

SDK64 在几何排版时收集字形范围，并为绘制创建独立的检查点、最终度量与范围副本。复用前核对文本、字体、排版参数、emoji 包与版本，副本独立保持字体及包的引用；跨文本修改复用时保留未变前缀的保守范围，重放加入新的后缀范围。

实际验证揭示了 DPI 限制：当前 DPI 代理以 `base` 逻辑字体排版，以 `raw` 缩放字体绘制。125% 下两者的字体身份不同，复用被正确拒绝，但提前收集的范围增加了成本。C 首次帧增加到 1739 ms，插入后帧到 853 ms；xlang 分别为 1584、825 ms。该尝试未改善默认缩放下的首次绘制，保留为独立实验。

在 100% 的 C / 16 MiB 配对观测中，SDK63 → SDK64 的首次帧为 1361 → 789 ms，插入后帧为 713 → 43 ms，撤销后帧为 695 → 20 ms。每次各通过 105 项检查。此结果仅对应逻辑缩放覆盖；下一步需要使首次绘制复用感知物理字体，或避免为不匹配的绘制字体提前建立范围。直接把逻辑坐标乘以缩放比例不足以证明极长行的逐字形精度。

## 正确性与材料

- SDK63 与 SDK64 分别完整 O2 / Werror 编译退出 0，耗时 284.437、286.437 秒；所有 SDK 源材料均在独立目录复制和校验。
- SDK63 的底层完整扫描 / 视口重放通过 194 对 GPU 像素比较、86 对绘制缓存重放及字体 / 文本 / 默认与显式 emoji 包变化检查，共 1,711 项断言。
- SDK64 的底层检查额外从几何排版创建独立绘制副本，并释放原几何缓存后继续绘制；包括 16 MiB 非整数步进末尾、SVG 溢出 / 加载失败、回退大字体、多行 / 嵌入 NUL / 无效 UTF-8、过量可见记录与参数拒绝。实际计数见其原生报告。
- 两版各自与 SDK62 比较的 12 对 IDE 截图逐像素相同；5,954 个 PE 导出名称和序号、25 份消费者及供应商头文件保持一致。
- 36,433 份受保护文件、2,309 组保存副本的核验无差异。所有 GUI 程序在各自的私有 Windows 桌面与 Job 中运行，没有切换用户桌面。

原生消息脉冲延迟同时包含夹具的全行精度核验与截图读取，不能作为发行级输入响应指标。本阶段的单次时间观测、逻辑 DPI 覆盖和局部像素检查不代替标准完整回归、便携验收、物理混合 DPI 或实体输入法验证。整体性能仍待验收，长期任务保持 active。

实现入口：

- [视口重放 C 实现](../third_party/sdk_patches/viewport-paint-v1/xge_text_viewport_paint_internal.inc)
- [几何范围与独立绘制副本](../third_party/sdk_patches/geometry-paint-v1/cursor_ink.inc)
- [当前应用的原生性能观察](../tools/check_editor_performance.py)
- [独立引擎像素与引用验证](../tools/check_viewport_paint.py)
- [SDK63 准备和严格构建](../tools/prepare_viewport_paint_sdk.py)
- [SDK64 准备和严格构建](../tools/prepare_geometry_paint_sdk.py)

实际证据：

- [SDK62 当前应用观察](../build/editor-observation-d2fa879811d3/evidence.json)
- [SDK63 当前应用观察](../build/editor-observation-79c2456ef17c/evidence.json)
- [SDK64 当前应用观察](../build/editor-observation-7a81e95c1d74/evidence.json)
- [SDK63 像素 / 导出比较](../build/viewport-paint-comparison-v1.json)
- [SDK64 像素 / 导出比较](../build/geometry-paint-comparison-v1.json)
- [SDK63 底层原生检查](../build/viewport-paint-oracle-d19313c2fc4e/evidence.json)
- [SDK64 几何复用原生检查](../build/viewport-paint-oracle-b4a1a61df8bd/evidence.json)
- [SDK63 / 100% 观察](../build/editor-observation-e5ffba424b00/evidence.json)
- [SDK64 / 100% 观察](../build/editor-observation-0cc9ba26413f/evidence.json)
- [保护材料核验](../build/viewport-paint-protected-v1.json)
