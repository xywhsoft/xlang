# 主线阻断：os.args 缺少 xl6_xrt_os_args 实现或运行时注册

- 验证环境：2026-10-12，`release/xl.exe`（2026-10-07 构建）。
- 最小复现：在 `release` 运行
  `./xl run ../dev/extlib_blockers/os_args_direct.xl`；仅导入 `cli` 的
  `./xl run ../dev/extlib_blockers/cli_import_os_args.xl` 也会失败。
- 期望：`os.args()` 返回当前进程参数；仅导入 `cli` 不需要实际调用
  `parseOs` 也应能够编译和运行。
- 实际：两条命令均报 `tcc: error: unresolved reference to 'xl6_xrt_os_args'`。
  现有 `../test/cases/extlib_cli.xl` 因此不能进入包级测试。
- 线索：`src/stdlib/xl6_stdlib_data.c` 注册了
  `__xl_std_runtime_os_args -> xl6_xrt_os_args`，但在 `src`
  未找到该符号的定义。现有 `src/runtime/xl6_array_os_argv.c`
  提供 `xl6ArrayOsArgvCreateV1`，它需要调用方提供 `argc/argv`
  或 Windows 原生命令行，不能直接视为同签名替代。
- 建议：在主线明确 `os.args()` 对运行上下文参数的语义，提供真实的
  `xl6_xrt_os_args` 实现，并使 JIT 符号注入与 AOT 编入单元一致。
  以直接 `os.args()`、仅 `import cli`、`cli.parseOs()` 的运行/AOT
  正反例验证；不要在 cli 包里通过移除 `parseOs` 绕开链接缺口。
- 状态：blocked-upstream。本扩展库任务不修改主线；C 侧解析器、范例和
  边界测试可以继续，XLang 运行、双语对照与 AOT 须待单独线程修复后验收。
- 2026-10-14 复验：当前 `release/xl.exe` 仍是 2026-10-07 构建；上述
  直接调用与仅导入两条最小命令均以相同未解析符号失败，阻断未解除。
- 2026-10-16 复验：`xl check release/extlibs/cli/examples/basic.xl` 通过；
  `xl run test/cases/extlib_cli.xl` 仍报同一缺符号。包内 C 实现与
  Windows/Linux C 门禁已推进，未在 `cli` 中移除 `parseOs` 或添加回退。
- 2026-10-17 复验：在 `demo6` 运行当前
  `./release/xl.exe run test/cases/extlib_cli.xl` 仍报
  `unresolved reference to 'xl6_xrt_os_args'`；阻断未解除。本任务转查其他包，不修改主线。
- 2026-10-18 复验：同一命令仍报 `unresolved reference to 'xl6_xrt_os_args'`。
  本轮只修了包内 C 解析器的未知 spec 字段校验；不添加语言侧符号回退。
