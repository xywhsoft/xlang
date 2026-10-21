# 连续字符串字素遍历实验

SDK29 私有候选，基于 SDK27，只修改 `src/xui_unicode.c`：连续字符串字素步进固定字节读取函数，通用回调路径、分类数据和原规则保持一致，没有堆分配或新增持久状态。当前应用与便携候选未替换，性能未验收。

整套严格冷构建、36,483,444 项比较断言、3 项 SDK 与 6 项相关应用测试、16 组 C／xlang 实际窗口用例和独立重放通过，8 对截图逐像素一致。小函数遍历改善没有转化为一致的整体交互改善，超长行仍有约两秒停顿，不选入当前应用。

`patch-manifest.json` 保存累计 8 个源材料、8 份审核 diff 和 21 个测试材料的哈希；全部准备来源材料为 237 项。`tests/old_pointer_unicode.c` 是 SDK27 的完整 Unicode 源，`tests/pointer_next_test.c` 是实际对照程序，覆盖全部码点值、部分字节、随机输入、回调失败及保护页边界。

使用[准备工具](../../../tools/prepare_pointer_grapheme_sdk.py)输出新源目录，再用[字素对照工具](../../../tools/test_pointer_grapheme_sdk.py)核对已有私有构建报告并在新目录重放；该工具不修改 SDK 材料或引擎副本，没有另一份 XRT 实现。

机制、测量边界和待验收项目见[实验记录](../../../docs/large-source-pointer-grapheme.md)。独立重放、封存和见证结果在 `build/editor-pointer-durable-v1/result.json`、`build/editor-pointer-evidence-cohort-v1.json` 及 `build/editor-pointer-final-v1-witness.json`，不在补丁清单中产生自引用哈希。
