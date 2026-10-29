# argon2 文本输入的 NUL 边界

2026-10-25：当前 Windows 发布版的完整 `extlib_argon2.xl` 已在 JIT
及独立 EXE 中通过。用例确认含 NUL 的密码、盐、secret、ad 参与
完整哈希和验证，PHC 文本内嵌 NUL 被拒绝；Linux C ASan/UBSan
专项门禁也通过。本页以下“XLang 待验”只描述当时的旧发布物。

2026-10-14：公开 C ABI 已统一采用指针 + 显式字节长度；密码、手动盐、
secret/ad 含 NUL 正反例、PHC 文本含 NUL 拒绝、严格 GCC 和 Linux
ASan/UBSan 均通过。以下为修复前的历史复现；XLang 导入仍被
`argon2_byteat_sub.md` 的主线 XLIR 错误阻断，尚不能签收语言侧往返。

修复前 `release/extlibs/argon2/argon2_native.c` 对 `password`、`salt`、
`secret`、`ad` 使用 `strlen`，`xargon2_hash`、`xargon2_hash_raw`、
`xargon2_verify` 的公开声明也没有对应长度参数。XLang 字符串允许
内嵌 NUL，因此该输入会被当成 NUL 前缀，且没有报错。

## 最小复现

以固定盐、相同成本调用 C `xargon2_hash_raw`，分别传入字节数组
`{'p', 'w', '\0', 'x', '\0'}` 与字符串 `"pw"`，复制并比较返回的
hex。修复前两者相同；期望它们产生不同哈希，或明确拒绝第一个输入，
不能静默将两个密码视为同一密码。`secret`、`ad` 和手动盐同理。

## 修复要求

以一个明确的长度感知 C ABI 统一 `password`、`salt`、`secret`、`ad`；
XLang 入口传入 `string.byteLen`。PHC 编码串本身为 ASCII 文本，
应拒绝内嵌 NUL。增加固定盐的 C/XLang 对照、含 NUL 的密码与上下文
正反例及 `verify` 往返测试，不保留会静默截断的旧 ABI 路径。

当时 `import argon2` 被 `argon2_byteat_sub.md` 中的主线 XLIR 错误阻断；
本任务没有在编译器或包内绕过。当前 Windows JIT/AOT 的语言侧含 NUL
往返已完成，不能再将这一旧阻断写作现行状态。
