# xlang 本地 TCC 修改的许可

本目录包含 Tiny C Compiler 的上游代码及 xlang 的本地修改。上游权利人的版权声明、逐文件许可及额外许可继续保留。

xlang 权利人于 2026-10-30 明确选择：对 TCC 主体的衍生修改继续沿用上游 LGPL，不通过 LGPL 第 3 节将修改版改为普通 GPL。主要源文件的原始通知指定 LGPL version 2 or later；[COPYING](COPYING) 保留上游提供的 LGPL 2.1 正文。具体文件以其原有声明为准，不能仅因处于本目录而统一替换许可。

特别保留 `lib/libtcc1.c` 的 GPL 与链接额外许可，以及 `include/stdatomic.h` 的 Apache-2.0 WITH LLVM-exception 等不同来源条件。根目录 MIT 许可证适用于 xlang 原创部分，不覆盖这些第三方及其衍生代码。

修改内容与日期可由仓库针对本目录的提交历史核对。当前文件是本地许可范围说明，不替代发行时逐文件修改标注、对应源码、许可证副本及静态重新链接材料的核验。

完整组件范围见[项目第三方许可说明](../../THIRD_PARTY_NOTICES.md)。
