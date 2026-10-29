# 共享主线阻断：Buffer 索引读取后参与异或赋值时 XLIR 失败

2026-10-26 当前 Windows/Linux 发布版分别运行本文件最小复现均退出 0；
`gzip` 完整双平台门禁也通过，包含 Linux 核心、文件/错误边界及双语
范例独立 EXE。下文报错与 blocked-upstream 状态仅是当时构建的历史，
此问题不再阻断当前发布版；保留最小用例供未来编译器回归验证。

- 发现日期：2026-10-12；使用当前 `release/xl.exe`。
- 在 `release` 执行 `xl run ../dev/extlib_blockers/gzip_buffer_index_xor.xl`。
- 期望：`data[0]` 从 1 翻转为 0，程序退出码为 0。
- 实际：`[ir] typed XLIR operator operand has no canonical physical-to-execution path`。
- 最小表达式：`data[data.len() - 1] = data[data.len() - 1] ^ 1`，不依赖 gzip 包。
- 影响：`tools/check_extlib_gzip.ps1` 的 C 边界、增量流、分配故障注入、Python/zlib 互通均通过；首个 XLang JIT 测试在 `test/cases/extlib_gzip.xl:49` 因同一表达式失败。完整 AOT 与双语范例尚未在当前发布物复验。
- 建议：检查 Buffer `[]` 读取的物理类型、按位运算两操作数的规范化以及索引写回的 lowering；对临时索引和单独变量索引分别补 JIT/AOT 测试。不要仅改 gzip 用例绕过这一共享错误。
- 状态：blocked-upstream；本扩展库任务不修改 xlang 主线，转开发下一个库。

## 2026-10-25 复验

Windows 当前 `release/xl.exe` 从 `release` 工作目录运行
`test/cases/extlib_gzip.xl` 已退出 0；上面的 Windows 失败记录只对应旧发布物。
现有 Linux `release/xl` 运行同一用例仍在第 49 行报相同 XLIR 错误。
更新后的 `tools/check_extlib_gzip.sh` 已将该用例纳入最后一关：此前的 C
ASan/UBSan、139 项独立互通、文件事务、异常分派、进度与双语基础范例
均通过，随后门禁明确非零退出。当前优先动作是修复 Linux 发布构建清单、
重建 `release/xl` 并重新运行整套门禁；仅凭旧 Linux 二进制不能断言
当前编译器源码仍有同一缺陷，也不能据 Windows 通过签收 Linux。
