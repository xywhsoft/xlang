# 当前发布构建的 Buffer AOT 缺少 native handle trace 符号

- 环境：2026-10-15，`demo6/release/xl.exe` 与 `xlc.dll` 均为 2026-10-07 的发布构建。
- 最小复现：在 `demo6` 执行
  `release/xl.exe run dev/extlib_blockers/native_handle_trace_aot.xl`，退出码为 0；执行
  `release/xl.exe build dev/extlib_blockers/native_handle_trace_aot.xl -o build/test/native_handle_trace_aot.exe`，报
  `tcc: error: unresolved reference to 'xl6NativeHandleTraceLeaf'`。
- 扩展库对照：`release/xl.exe build dev/extlib_blockers/stb_image_write_import_aot.xl -o build/test/stb_image_write_import_aot.exe` 同样失败；仅导入并调用 `available()`，没有图片数据、文件输出或包内 C 回调。
- 期望：普通 `Buffer` 和仅导入 `stb_image_write` 的无输出程序可构建并运行。
- 边界：`src/runtime/xl6_native_handle.c` 已定义该函数；`src/compile/xl6_artifact_runtime_symbols.inc` 与 `src/compile/xl6_compile_unit.c` 也已有 `NATIVE_HANDLE` 组件声明。当前证据只证明旧发布构建的 AOT 链接闭包缺符号，不能据此断定当前主线源码仍缺实现。
- 建议：由主线线程在无本任务包侧绕过的前提下重建正式产品，先验证 `Buffer` 最小程序的 JIT/AOT，再验证 `stb_image_write` 仅导入用例、包级核心与带输出范例；如仍失败，检查运行时组件选择与产物符号表是否实际包含 `xl6_native_handle.c`。
- 状态：blocked-upstream；本任务不修改编译器或发布运行时。

2026-10-18 `blake2`、`blake3` Windows 包级门禁复核：严格 GCC、新增关闭失败
C 负例和首组 XLang JIT 均通过；各自首个 AOT 构建均报
`unresolved reference to 'xl6NativeHandleTraceLeaf'`。Linux 两包 C
ASan/UBSan、XLang JIT 与双语范例对照另行通过。此记录不代替上面的
独立 Buffer 最小复现，也不授权包侧绕过；发布物重建后须复跑完整门禁。

2026-10-17 `siphash128` 复核：在 `demo6` 运行
`powershell -NoProfile -File tools/check_extlib_siphash128.ps1`，语言 check/JIT
通过，AOT 构建同样缺 `xl6NativeHandleTraceLeaf`。严格 GCC 原生桥语法、
C 核心测试及 XLang 公开范例另行通过。此为上述共享 AOT 阻断，
不归咎于 SipHash128 算法；主线重建后须重跑整份门禁。

2026-10-17 当前 `release/xl.exe` 复核：在 `demo6` 执行
`release/xl.exe build dev/extlib_blockers/native_handle_trace_aot.xl -o build/test/native_handle_trace_probe.exe`
仍报 `unresolved reference to 'xl6NativeHandleTraceLeaf'`，退出码 1。
该用例不依赖任何扩展包，先由主线线程修复并重建发布物，再恢复包级 AOT 验收。
- 2026-10-15 复核：当前 `release/xl.exe build dev/extlib_blockers/native_handle_trace_aot.xl -o build/test/native_handle_trace_aot.exe` 仍报同一缺符号；`xxhash` 包的 `extlib_xxhash.xl` 完整 AOT 门禁亦同错。独立 C/XL JIT 已通过，不能据此签收 AOT。
- 2026-10-15 `cbor/test/check.ps1` 在 C/JIT 全绿后，首个 `test/aot_core.xl` 构建也报同一符号；上述独立 `Buffer` 最小复现再次确认。CBOR 定长边界修复的 AOT 不作包侧绕过，待主线产物重建后从核心、边界到双语范例顺序复验。
- 2026-10-16 Linux `release/xl` 复现：在 `demo6` 运行 `bash release/extlibs/toml/test/check_xlang.sh`，边界 JIT 与双语范例先通过，随后精简 AOT 构建报相同符号。单独运行 `release/xl build release/extlibs/toml/test/aot_core.xl -o /tmp/xlang_toml_aot_core` 同错；`release/xl run dev/extlib_blockers/native_handle_trace_aot.xl` 正常。说明当前 Linux 发布路径也有 AOT 符号闭包缺口，不是 TOML 解析或其新增负例所致。主线应在重建后核对 Linux AOT 组件和导出表；本任务不改编译器。
- 2026-10-16 Windows 当前发布物复核：`release/extlibs/zip/test/check.ps1` 的 C/XL 边界与范例通过，随后纯内存 `test/aot_core.xl` 构建报 `xl6NativeHandleTraceLeaf`；不依赖 ZIP 的 `release/xl.exe build dev/extlib_blockers/native_handle_trace_aot.xl -o <临时输出>` 同错。旧 ZIP 核心 AOT 通过记录是历史构建基线，当前构建不能签收；主线修复后先跑独立 Buffer 复现，再跑 ZIP 核心与完整范例。
- 2026-10-16 Linux CSV 包级复核：`bash release/extlibs/csv/test/check_xlang.sh` 的中央、包内边界、文件与范例 JIT 均通过，随后无输出 `test/aot_core.xl` 构建报 `xl6NativeHandleTraceLeaf`。独立运行 `release/xl build dev/extlib_blockers/native_handle_trace_aot.xl -o /tmp/xlang_csv_native_handle_repro` 同错。`--skip-aot` 仅供开发阶段单独检查 JIT，默认门禁仍保留失败；主线重建后需从独立 Buffer、CSV 核心到完整双语范例逐层复跑。
- 2026-10-18 Windows LZMA 包级复核：新增 `release/extlibs/lzma/test/check.ps1` 默认门禁在 `test/aot_core.xl` 报同一 `xl6NativeHandleTraceLeaf`；独立 `dev/extlib_blockers/native_handle_trace_aot.xl` 用当前 `release/xl.exe build` 也同错。`-SkipAot` 的严格 C、C/XLang 边界、文件/进度、原生可见性与双语对照通过，Linux ASan/UBSan 和 Python 标准 `.xz` 互通亦通过。旧 LZMA 核心 AOT 成功记录不代表当前发布物；主线修复后先跑独立 Buffer，再跑默认 LZMA 门禁及完整边界 AOT。
- 2026-10-21 Windows BLAKE2/BLAKE3 默认门禁复核：两包首个 `extlib_blake*-build` 均报 `tcc: error: unresolved reference to 'xl6NativeHandleTraceLeaf'`。各自 `-SkipAot` 的严格 C、XLang JIT、向量和双语对照及 Linux C ASan/UBSan、XLang 门禁通过；完整教学范例 JIT 也通过。AOT 仍由主线处理，不用包侧绕过替代验收。
- 2026-10-21 Windows SHA3 默认门禁复核：C 核心与原生层、`extlib_sha3` JIT 通过，首次 `extlib_sha3-build` 报相同缺符号。`-SkipAot` 的严格 C、完整 XLang JIT、生成 ABI、C JIT/AOT 和双语范例，以及 Linux C ASan/UBSan、XLang JIT 通过。AOT 仍需主线重建并复验，不签收 SHA3 整包发布级。
