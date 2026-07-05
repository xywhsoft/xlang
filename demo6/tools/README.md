# demo6/tools

This directory contains DEMO6 toolchain setup scripts and engineering guard checks.

## Toolchain

- `xl6_toolchain_env.bat`: centralizes TCC, optional Flex/Lemon regeneration tools, XRT, include, lib, and default link settings.
- `bootstrap_tcc.bat`: builds `build\tmp\tcc_bootstrap.exe` from `lib\tcc` with MinGW `gcc` when no `XL6_TCC` override exists.
- `build_xrt_obj.bat`: builds `build\tmp\xrt.o` from `lib\xrt\xrt.h`, so normal builds do not depend on an external XRT repository checkout.
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
- `check_dap_smoke.ps1`
- `check_dll_api_boundary.ps1`
- `check_docs_index.ps1`
- `check_example_wiring.ps1`
- `check_markdown_links.ps1`
- `check_negative_diagnostics.ps1`
- `check_package_fixtures.ps1`
- `check_package_xrt_symbols.ps1`
- `check_release_extlibs.ps1`
- `check_release_tmp_artifacts.ps1`
- `check_risk_ledger.ps1`
- `check_source_encoding.ps1`
- `check_source_wiring.ps1`
- `check_stdlib_coverage.ps1`
- `check_stdlib_dll_symbols.ps1`
- `check_stdlib_examples.ps1`
- `check_stdlib_test_wiring.ps1`
- `check_tcc_export_environment.ps1`
- `check_test_case_wiring.ps1`
- `check_test_data_wiring.ps1`
- `check_tool_wiring.ps1`
- `check_toolchain_paths.ps1`
- `check_xlang_output_markers.ps1`

`check_async_task_soak.ps1` runs a focused async/task/concurrency soak guard. The default mode covers direct `xl run` entries plus the `cgen_task_spawn.xl` build/run artifact path; use `-Repeat N -IncludeHeavy` before release cuts to include the larger `release\examples\task_api.xl` and `cgen_task_api.xl` paths.

`check_dap_smoke.ps1` runs the `xl dap` protocol smoke test and verifies DAP response/event framing for initialize, breakpoints, stack/scopes/variables, unsupported requests, continue, and disconnect.

## Maintenance

- New `check_*.ps1` files must be wired into `test.bat` or one of the `test/suites/*.bat` full-regression suites.
- New `check_*.ps1` files must also be wired into `rc0_guard.ps1`, unless explicitly listed as a full-regression-only exception in `check_tool_wiring.ps1`.
- Keep batch scripts ASCII where practical and keep toolchain paths centralized through `xl6_toolchain_env.bat`.
- `build.bat` must be able to run on a clean Windows machine with MinGW in `PATH`; `build.sh` is a Git Bash/MSYS wrapper over the same build path. Flex/Lemon are only required when `XL6_REGEN_GRAMMAR=1`.
