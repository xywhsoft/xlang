# demo6/tools

This directory contains DEMO6 toolchain setup scripts and engineering guard checks.

## Toolchain

- `xl6_toolchain_env.bat`: centralizes TCC, optional Flex/Lemon regeneration tools, XRT, include, lib, and default link settings.
- `bootstrap_tcc.bat`: builds `build\tmp\tcc_bootstrap.exe` from `lib\tcc` with MinGW `gcc` when no `XL6_TCC` override exists.
- `build_xrt_obj.bat`: builds the default runtime object from `lib\xruntime\xruntime.h`; the product contains its exact XRT dependency closure.
- `sync_xrt_snapshot.py`: validates XRT/xruntime maturity, synchronizes both product headers, and regenerates deterministic product metadata, the complete TCC symbol table, and the language capability matrix.
- `gen_xrt_xlang_matrix.py`: validates all low-level runtime symbols, generates the module capability matrix, and maintains the exact xhttp/xws/xregex migration boundary.
- `generate_stdlib_index.py`: cross-platform standard-library hash-index generator used by the Linux build.
- `gen_tcc_vfs_resources.py`: cross-platform LZMA VFS resource generator; its output is kept byte-identical with the PowerShell generator.
- `tcc_vfs_manifest_linux_runtime.txt`: minimal Linux runtime VFS resource list for `libxl.so`.
- `tcc_vfs_manifest_linux_xlc.txt`: Linux compiler VFS resource list for `libxlc.so`.
- `clear_release_tmp_files.ps1`: removes top-level `build\test` regression artifacts with bounded retries so cleanup fails hard instead of continuing after file-lock errors.
- `run_xlir_guards.ps1`: discovers specialized XLIR semantic guards and supplements the explicit full-regression or RC0 entries without running those entries twice. Standalone guards run directly; workers with mandatory inputs are delegated to the reviewed scenario runner.
- `run_xlir_parameterized_guards.ps1`: builds one current-source public compiler, one runtime-only compiler and one shared contract-driver set, then executes every parameterized XLIR worker through dependency-ordered immutable batch reports.
- `rc0_guard.ps1`: lightweight release-candidate guard for required files, static boundaries, build, command smoke, diagnostics, docs, risk ledger, cgen size, and wiring.

## Guard scripts and inventory

The files matching `tools/check_*.ps1` are the canonical and exhaustive guard inventory. `check_tool_wiring.ps1` discovers that inventory directly and requires every script to be wired into the full regression and the RC0 guard (or an explicit full-regression-only exception). This README records the stable toolchain entry points and representative cross-cutting guards; it does not duplicate every specialized XLIR semantic gate name.

Representative and cross-cutting guards include:

- `check_async_task_soak.ps1`
- `check_batch_wiring.ps1`
- `check_c_fixture_wiring.ps1`
- `check_c_string_safety.ps1`
- `check_cgen_module_size.ps1`
- `check_cgen_statement_expr_allowlist.ps1`
- `check_cgenrule_docs.ps1`
- `check_cli_temp_cleanup.ps1`
- `check_debugger_not_released.ps1`
- `check_dev_projects.ps1`
- `check_dll_api_boundary.ps1`
- `check_docs_index.ps1`
- `check_example_wiring.ps1`
- `check_format_extlibs.ps1`
- `check_frontend_fuzz.ps1`
- `check_generated_c_warnings.ps1`
- `check_markdown_links.ps1`
- `check_negative_diagnostics.ps1`
- `check_package_fixtures.ps1`
- `check_package_xrt_symbols.ps1`
- `check_release_extlibs.ps1`
- `check_release_tmp_artifacts.ps1`
- `check_risk_ledger.ps1`
- `check_source_encoding.ps1`
- `check_source_wiring.ps1`
- `check_typed_hir_contract.ps1`
- `check_xlir_contract.ps1`
- `check_xlir_cgen_determinism.ps1`
- `check_xlir_cgen_differential.ps1`
- `check_xlir_intrinsic_calls.ps1`
- `check_xlir_collection_member_intrinsics.ps1`
- `check_xlir_dynamic_collection_methods.ps1`
- `check_xlir_migration_coverage.ps1`
- `check_xlir_default_backend_boundary.ps1`
- `check_standalone_compiler_vfs.ps1`
- `check_stdlib_coverage.ps1`
- `check_stdlib_dll_symbols.ps1`
- `check_stdlib_examples.ps1`
- `check_stdlib_test_wiring.ps1`
- `check_tcc_export_environment.ps1`
- `check_tcc_vfs_resources.ps1`
- `check_tcc_xrt_contract.ps1`
- `check_test_case_wiring.ps1`
- `check_test_data_wiring.ps1`
- `check_tool_wiring.ps1`
- `check_toolchain_paths.ps1`
- `check_xlang_output_markers.ps1`
- `check_xrt_snapshot.ps1`
- `check_xrt_product_trim.ps1`
- `check_xrt_header_abi.ps1`
- `check_xrt_ownership_contract.ps1`
- `check_xruntime_language_bridge.ps1`

`check_async_task_soak.ps1` runs a focused async/task/concurrency soak guard. The default mode covers direct `xl run` entries plus the `cgen_task_spawn.xl` build/run artifact path; use `-Repeat N -IncludeHeavy` before release cuts to include the larger `release\examples\task_api.xl` and `cgen_task_api.xl` paths.

`check_frontend_fuzz.ps1` builds and runs the deterministic lexer/parser mutation harness. CI enables `-Sanitize` on Linux so address and undefined-behavior failures stop the build; the normal Windows suite runs the same corpus without requiring an external sanitizer runtime.

`check_generated_c_warnings.ps1` compiles representative generated C translation units with GCC warning errors for missing returns and incompatible pointer types. It is part of both the full artifact-closure suite and the RC0 guard.

`check_debugger_not_released.ps1` verifies that the incomplete probe/DAP prototype is not advertised or accepted by the first-release CLI. A real debugger must return through a new reviewed design and cannot reuse synthetic stack or variable responses.

`check_xrt_ownership_contract.ps1` prevents the removed XRT object-mode compatibility layer from returning. xlang uses retain, ownership transfer and explicit clone/copy; synchronization primitives provide cross-thread publication and visibility.

`check_xrt_header_abi.ps1` compiles the XRT declaration SDK and xlang private runtime header in all supported include orders. It rejects duplicate declarations, type-layout drift and the historical `str` signedness mismatch.

`check_xrt_product_trim.ps1` builds minimal and typed representative programs, verifies that both use one xruntime implementation product, and rejects duplicate core implementation or unnecessary typed module retention.

`check_xruntime_language_bridge.ps1` fixes the native-container bridge contract: collection algebra and dictionary merge call xruntime directly, storage controls stay available to xlang, and signed bounds are validated before conversion.

## Maintenance

`check_xlir_pending_exception_route.ps1` now runs the full pending-error matrix by default (`-FullOutcome` remains a compatible alias). It covers post-try routing, managed/nested finally, nullable diagnostics and real OOM, return/break/continue overrides, internal catches, and actual await/yield suspension. Explicit pending-error owners, cleanup-preservation causes, cancellation and async/generator error-frame lifetimes are mutation-tested before C/header output; both fixtures have strict-C allocation-balance tests, deterministic C/header and C-host JIT/AOT runs. Cancellation/closed-Future terminal cleanup and other untested continuation combinations are not implied by this matrix.

`check_xlir_return_projection_owner.ps1` checks physical owner reads separately from flow-narrowed observations. Its runtime matrix covers single/product returns, repeated aliases, class widening, boxed scalars, nullable strings, partial evaluation, finally, mutable capture cells and both branches of conditional/coalesce results, with nullable diagnostics, real OOM and cleanup rollback. SSA producer ownership is shared by lowering and continuation analysis; product construction rejects untransferred borrows and duplicate acquired owners. A unique temporary initializer must dominate its read and preserve its acquisition identity; multiple branch initializers require an exact CFG activation and independently owned input on every branch. STORE/MOVE cannot manufacture ownership. The gate includes producer-ABI table checks, mutation rejection before C/header output, strict allocation balance, deterministic C/header, C-host JIT/AOT and the original cgen_var_return_exprs C ABI host. This does not claim a complete global affine-use proof across all local stores and managed SSA consumers.

- New `check_*.ps1` files must be wired into `test.bat` or one of the `test/suites/*.bat` full-regression suites.
- New `check_*.ps1` files must also be wired into `rc0_guard.ps1`, unless explicitly listed as a full-regression-only exception in `check_tool_wiring.ps1`.
- Do not hand-maintain a second exhaustive filename list in documentation; `check_tool_wiring.ps1` enumerates `tools/check_*.ps1` and validates each discovered script.
- Keep batch scripts ASCII where practical and keep toolchain paths centralized through `xl6_toolchain_env.bat`.
- Synchronize XRT/xruntime with `python tools/sync_xrt_snapshot.py --sync --xrt-root <path>`; never edit either vendored product header or its metadata by hand.
- `build.bat` must run on a clean Windows machine with MinGW in `PATH`.
- `build.sh` dispatches to `build_linux.sh` on Linux and to `build.bat` through `cmd.exe` on Windows.
- Linux builds require GCC, GNU Make and Python 3, but do not require a preinstalled TCC.
- Flex/Lemon are only required when `XL6_REGEN_GRAMMAR=1`.
