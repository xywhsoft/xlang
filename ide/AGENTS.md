# IDE build and validation

- Use the repository root `release/` for normal local IDE and language-server builds and runtime validation. Keep compiler objects in `ide/build/Release/` and logs or temporary fixtures in `release/tmp/`.
- Create a separate runtime environment only for a concrete need, such as a version comparison, fault injection or protecting the user's running session. Explain that need. Test-only desktop windows do not require a separate runtime directory.
- When updating the language server, rebuild and validate the IDE against the same `release/xlang-ls.exe`, matched XGE SDK/DLL and root compiler. Protocol tests alone do not establish that the IDE is usable.
- Include C and xlang typing, deletion, undo, Unicode/IME and click-to-focus checks, plus the language features affected by the change. Keep test executables out of the final release root after validation.
- Record source/header and runtime identities for a build. Do not link tests compiled with changed public headers against older objects; do not publish a build when its inputs change during compilation.
- Preserve concurrent work. A changed `xide_app` layout or action enum requires recompiling all of its consumers together. Preserve the last working release while resolving a build mismatch.
- C LSP is optional and disabled by default (`XIDE_ENABLE_C_LSP=0`). Retain its implementation behind the macro, omit clangd from default packages, and preserve C syntax editing and xlang-driven C build/run. The xlang service `xlang-ls.exe` stays enabled. Use `-EnableCLsp` only for an explicitly requested developer build and recompile all consumers consistently.
