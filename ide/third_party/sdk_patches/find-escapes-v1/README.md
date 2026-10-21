# 查找转义解码修正

SDK66 修改 SDK65 的 `src/xui_find.c` 和 `src/xui_code_edit.c`，公开头及导出接口不变。完整修改文件由 `tools/prepare_find_escape_sdk.py` 从已验收 SDK65 源清单生成，并与私有源构建绑定。

转义的十六进制读取先检查剩余长度；所有 NUL 编码均拒绝；`\u` 的 UTF-16 代理对合并为单个 Unicode 标量后输出 UTF-8，孤立或错配代理项拒绝。保留中文、原始 UTF-8 字节转义、换行、制表符、反斜线及原有普通查找/正则行为。

正则替换仍保留 `$1`、`\1` 及 `\0` 整体匹配引用；在匹配前校验转义，即使没有匹配也拒绝非法替换。公开查找 API 保留解码错误；编辑器“全部查找”及替换按钮保留无效表达式提示。

`tests/find_escape_test.c` 调用实际 DLL 的公开 API，并以保护页验证截断输入。`tests/find_options_view_test.c` 验证 C/xlang 原生编辑窗口中的实际选项与文档、撤销、磁盘边界。
