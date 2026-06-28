# demo6/tools

This directory contains DEMO6 toolchain setup scripts and engineering guard checks.

## Toolchain

- `xl6_toolchain_env.bat`: centralizes TCC, Flex, Lemon, XRT, include, lib, and default link settings.
- `rc0_guard.ps1`: lightweight release-candidate guard for required files, static boundaries, build, command smoke, diagnostics, docs, risk ledger, cgen size, and wiring.

## Guard Scripts

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
- `check_release_tmp_artifacts.ps1`
- `check_risk_ledger.ps1`
- `check_source_encoding.ps1`
- `check_source_wiring.ps1`
- `check_stdlib_coverage.ps1`
- `check_stdlib_dll_symbols.ps1`
- `check_stdlib_examples.ps1`
- `check_stdlib_test_wiring.ps1`
- `check_test_case_wiring.ps1`
- `check_test_data_wiring.ps1`
- `check_tool_wiring.ps1`
- `check_toolchain_paths.ps1`

`check_dap_smoke.ps1` runs the `xl dap` protocol smoke test and verifies DAP response/event framing for initialize, breakpoints, stack/scopes/variables, unsupported requests, continue, and disconnect.

## Maintenance

- New `check_*.ps1` files must be wired into `test.bat`.
- New `check_*.ps1` files must also be wired into `rc0_guard.ps1`, unless explicitly listed as a full-regression-only exception in `check_tool_wiring.ps1`.
- Keep batch scripts ASCII where practical and keep toolchain paths centralized through `xl6_toolchain_env.bat`.
