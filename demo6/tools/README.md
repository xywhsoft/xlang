# demo6/tools

This directory contains DEMO6 toolchain setup scripts and engineering guard checks.

## Toolchain

- `xl6_toolchain_env.bat`: centralizes TCC, optional Flex/Lemon regeneration tools, XRT, include, lib, and default link settings.
- `bootstrap_tcc.bat`: builds `build\tmp\tcc_bootstrap.exe` from `lib\tcc` with MinGW `gcc` when no `XL6_TCC` override exists.
- `build_xrt_obj.bat`: builds `build\tmp\xrt.o` from `lib\xrt\xrt.h`, so normal builds do not depend on an external XRT repository checkout.
- `generate_stdlib_index.py`: cross-platform standard-library hash-index generator used by the Linux build.
- `gen_tcc_vfs_resources.py`: cross-platform LZMA VFS resource generator; its output is kept byte-identical with the PowerShell generator.
- `tcc_vfs_manifest_linux_runtime.txt`: minimal Linux runtime VFS resource list for `libxl.so`.
- `tcc_vfs_manifest_linux_xlc.txt`: Linux compiler VFS resource list for `libxlc.so`.
- `clear_release_tmp_files.ps1`: removes top-level `build\test` regression artifacts with bounded retries so cleanup fails hard instead of continuing after file-lock errors.
- `rc0_guard.ps1`: lightweight release-candidate guard for required files, static boundaries, build, command smoke, diagnostics, docs, risk ledger, cgen size, and wiring.

## Guard Scripts

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
- `check_standalone_compiler_vfs.ps1`
- `check_stdlib_coverage.ps1`
- `check_stdlib_dll_symbols.ps1`
- `check_stdlib_examples.ps1`
- `check_stdlib_test_wiring.ps1`
- `check_tcc_export_environment.ps1`
- `check_tcc_xrt_contract.ps1`
- `check_test_case_wiring.ps1`
- `check_test_data_wiring.ps1`
- `check_tool_wiring.ps1`
- `check_toolchain_paths.ps1`
- `check_xlang_output_markers.ps1`

`check_async_task_soak.ps1` runs a focused async/task/concurrency soak guard. The default mode covers direct `xl run` entries plus the `cgen_task_spawn.xl` build/run artifact path; use `-Repeat N -IncludeHeavy` before release cuts to include the larger `release\examples\task_api.xl` and `cgen_task_api.xl` paths.

`check_frontend_fuzz.ps1` builds and runs the deterministic lexer/parser mutation harness. CI enables `-Sanitize` on Linux so address and undefined-behavior failures stop the build; the normal Windows suite runs the same corpus without requiring an external sanitizer runtime.

`check_generated_c_warnings.ps1` compiles representative generated C translation units with GCC warning errors for missing returns and incompatible pointer types. It is part of both the full artifact-closure suite and the RC0 guard.

`check_debugger_not_released.ps1` verifies that the incomplete probe/DAP prototype is not advertised or accepted by the first-release CLI. A real debugger must return through a new reviewed design and cannot reuse synthetic stack or variable responses.

## Maintenance

- New `check_*.ps1` files must be wired into `test.bat` or one of the `test/suites/*.bat` full-regression suites.
- New `check_*.ps1` files must also be wired into `rc0_guard.ps1`, unless explicitly listed as a full-regression-only exception in `check_tool_wiring.ps1`.
- Keep batch scripts ASCII where practical and keep toolchain paths centralized through `xl6_toolchain_env.bat`.
- `build.bat` must run on a clean Windows machine with MinGW in `PATH`.
- `build.sh` dispatches to `build_linux.sh` on Linux and to `build.bat` through `cmd.exe` on Windows.
- Linux builds require GCC, GNU Make and Python 3, but do not require a preinstalled TCC.
- Flex/Lemon are only required when `XL6_REGEN_GRAMMAR=1`.
