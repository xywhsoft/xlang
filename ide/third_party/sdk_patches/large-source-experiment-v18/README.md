# 单次排版内的有界 Emoji 匹配复用

SDK47 私有实验，累计保留 SDK46 的完整度量与水平候选收集、SDK43 的流式绘制及 SDK41 的检查点与 C/xlang 控件接入。每次排版固定 presentation，文本先通过原完整 UTF-8 校验；64 个槽保存最多 16 字节前缀、包身份与修订号、匹配值及实际读取范围和结尾依赖。读取范围超出前缀、结尾不能确定、修订号饱和、禁用 Emoji 或短文本时沿用原路径；没有跨调用匹配缓存。

最长匹配可能读取已匹配簇以外的字节，检查点继续记录同样的读取依赖。命中时复用匹配结果与读取范围，在原位置执行字体解析、布局缩放与浮点累计。SVG 绘制和资源范围判断沿用 SDK46。公开 xgeEmojiPackMatch 本身没有改变，GUI 仍仅为不可注册的内建 Emoji 包保留跨帧绘制缓存。

实际完整严格构建、完整引擎冷编译与原生比较结果绑定在 patch-manifest.json；专项用例包含前瞻、EOF、变体选择符、肤色归一化、修订、身份、槽位冲突及检查点恢复，原几何与 GPU 像素比较继续执行。物理混合 DPI、实体 IME、相对固定 SDK 的完整 ABI、全 SDK 许可和正式发布仍需验收。实际 IDE 窗口与本次性能观测见 docs/large-source-emoji-match.md；这个包未安装到固定 SDK 或便携候选。

准备工具为 tools/prepare_emoji_match_sdk.py 和 tools/prepare_emoji_match_app.py。tools/test_emoji_match_sdk.py 默认严格冷编译；显式 --reuse-compile-report 只有在全部 241 份引擎、13 份夹具、DPI adapter、参数、产物和日志匹配时才能复用。字体及资源在调用期间保持稳定。
