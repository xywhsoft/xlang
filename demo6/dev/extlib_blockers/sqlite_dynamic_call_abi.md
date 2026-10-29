# sqlite 主线阻断：动态方法调用的 XLIR C emitter ABI

- 验证时间：2026-10-12；当前 `demo6/release/xl.exe` 与同目录 DLL。
- 已知最小可靠入口：在 `demo6/release` 执行 `xl run examples/extlibs/sqlite_basic.xl`。该范例本身约 1.1 KB，导入 `sqlite` 包。
- 期望：SQLite 包的 `Database.query()` 将行对象追加到结果数组，基础范例可运行。
- 实际：`extlibs/sqlite/src/sqlite.xl:550:28: [checked] XLIR C emitter runtime operation ABI is unresolved`，失败语句为 `rows.append(stmt.row())`。
- 现有证据：`xl ir examples/extlibs/sqlite_basic.xl --lowered --emit <file>` 成功；相应指令的 runtime op 是 `xvalue-dynamic-method-call`，携带 `dynamicCallSite=dc0`、一个位置参数、`fallback=push`。`src/cgen/xl6_ir_cgen_runtime_async_instruction.c` 有该 op 的发射分支，`src/cgen/xl6_ir_cgen_verify_instructions.c` 也有白名单分支；应检查真实校验条件与动态调用 site 的 ABI 元数据是否一致。
- 反证边界：脱离包的简单 `var rows=[]; rows.append(row())`，即使放入类方法、循环和 `try/finally` 仍可运行，所以不能把问题概括为所有 `var.append` 失败。它与包导入或具体动态调用元数据有关，尚需主线内更精细的最小化。
- 修复建议：在主线补一条涵盖包导入、动态集合方法 fallback、返回 `var` 的类方法及 `try/finally` 的回归；修正 XLIR verifier 与 C emitter 对同一 `dynamicCallSite` 的 ABI 接受条件，并覆盖 JIT/AOT。不要在 SQLite 包内将 `query()` 改写成特例绕行。
- 2026-10-15 复验：当前 `demo6/release/xl.exe run examples/extlibs/sqlite_basic.xl`
  仍在同一 XLIR C emitter ABI 位置失败；未在包内绕过或修改 `src/` 主线。
- 状态：blocked-upstream；主线修复后重跑上述入口与包级门禁。
- 2026-10-17 复验：从 `demo6` 运行 `release/extlibs/sqlite/test/boundaries.xl`，仍在 `sqlite.xl:550:28` 报同一 XLIR ABI 错误，包级边界测试未进入执行。
- 2026-10-18 Linux 复验：在 WSL 的 `demo6` 根目录运行 `./release/xl run release/examples/extlibs/sqlite_basic.xl`，仍在第 550 行报同一 `[checked] XLIR C emitter runtime operation ABI is unresolved`；并非仅 Windows 构建的诊断。未修改包源码或主线。
- 同日包内静态审计发现待验证的字符串截断风险：`sqlite3_open`、`sqlite3_open_v2`、`sqlite3_exec`、`sqlite3_prepare_v2`、`sqlite3_bind_text`、`sqlite3_bind_blob_text` 将 XLang 字符串直接传给 NUL 结尾的 C 参数；原生端对部分参数使用 `strlen` 或 SQLite 的 `-1` 长度。若字符串含内嵌 NUL，文件名/SQL/绑定值可能仅使用前缀。主线恢复后先用 `":memory:\0suffix"`、`"select 1;\0invalid"`、`"A\0B"` 建红色用例，再在包的公开字符串入口统一拒绝内嵌 NUL；显式长度的 Buffer 绑定接口应继续允许二进制 NUL。当前仅为静态风险，未将它误报成已跑通的动态复现，也不在阻断状态下签收修复。
- 2026-10-20 复验：从 `demo6` 根目录运行 `release/xl.exe run release/extlibs/sqlite/test/file.xl`，在执行文件测试前仍于 `sqlite.xl:550:28` 报同一 XLIR ABI 错误。`test/file.xl` 已将固定的 `build/test/sqlite/中文数据库.db` 改为独占临时目录并在 `finally` 清理；该改动尚未完成运行验证，主线修复后须重跑文件/锁边界和完整包门禁。
