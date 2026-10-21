# 可见字形绘制缓存实验

SDK27 私有候选，基于 SDK23，补齐可见范围与默认包失效条件。只保留最多 4,096 项绘制记录及原路径的精确浮点笔位置，释放完整字形数组，并用饱和变更代数排除默认 Emoji 包在整形中切出再切回的情况。当前应用未替换，性能未验收。

整套严格冷构建、605 组成功像素对照、96 组相同错误返回、216 次重复绘制复用、3 项 SDK 与 6 项相关应用测试、16 组 C／xlang 实际窗口测试通过，8 对截图逐像素一致。16 MiB 单行输入仍约 1.9 秒，不选入当前应用。

`patch-manifest.json` 保存累计 8 个源材料和 19 个测试材料的哈希，完整来源材料为 237 项。SDK25 的首次构建参数遗漏及两版夹具整数矩形预期错误均保留实际失败记录。原源码树、当前 SDK 和便携包均保持原内容。

使用[准备工具](../../../tools/prepare_paint_cache_sdk.py)输出新源目录，再用[绘制对照工具](../../../tools/test_paint_cache_sdk.py)核对已有私有构建报告并在新目录重放。工具只在诊断引擎副本加入三个缓存分配失败钩子，没有修改 SDK 原材料。

完整机制、测量限制、失败原因和后续验收见[实验记录](../../../docs/large-source-viewport-paint.md)。独立重放、封存和见证结果在 `build/editor-paint-durable-v1/result.json`、`build/editor-paint-evidence-cohort-v1.json` 及 `build/editor-paint-final-v1-witness.json`，不在补丁清单中制造自引用哈希。
