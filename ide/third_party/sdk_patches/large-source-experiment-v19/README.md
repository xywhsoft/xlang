# 单次排版内的原始字形度量复用

SDK48 私有实验，累计保留 SDK47 的有界 Emoji 匹配、SDK46 的水平候选收集和此前流式布局与 C/xlang 控件接入。64 槽的调用内表仅保存 face 与 glyph 对应的整数 advance、右边界和是否有轮廓；字体 face 在调用期间保持稳定。每次先检查实际字形图集，已有图集时读取其当前值；未进入图集时在原位置应用实时字体倍率。失败不缓存，短文本保留原度量调用。无跨调用状态或新堆分配，没有扩大公开 API。

实际完整严格构建、完整引擎冷编译及原生比较绑定在 patch-manifest.json。专项逐位比较共享 face 的字号、正负与邻近倍率、空轮廓、槽冲突、图集建立和图集数值变化、无效输入及未初始化字体；原 Emoji、回调字号变化、几何、检查点、绘制和 GPU 像素比较继续执行。私有观察宏默认为空。物理混合 DPI、实体 IME、相对固定 SDK 的完整 ABI、全 SDK 许可及正式发布仍需验收；本包未安装到固定 SDK 或便携候选。

准备工具为 tools/prepare_glyph_metrics_sdk.py 和 tools/prepare_glyph_metrics_app.py。tools/test_glyph_metrics_sdk.py 默认严格冷编译；显式 --reuse-compile-report 只有在全部 241 份引擎、14 份夹具、DPI adapter、参数、产物和日志匹配时才能复用。窗口与本次性能观测见 docs/large-source-glyph-metrics.md。
