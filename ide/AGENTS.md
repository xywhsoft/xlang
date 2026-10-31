# IDE build and validation

- Use the repository root `release/` for normal local IDE and language-server builds and runtime validation. Keep compiler objects in `ide/build/Release/` and logs or temporary fixtures in `release/tmp/`.
- Create a separate runtime environment only for a concrete need, such as a version comparison, fault injection or protecting the user's running session. Explain that need. Test-only desktop windows do not require a separate runtime directory.
- When updating the language server, rebuild and validate the IDE against the same `release/xlang-ls.exe`, matched XGE SDK/DLL and root compiler. Protocol tests alone do not establish that the IDE is usable.
- Include C and xlang typing, deletion, undo, Unicode/IME and click-to-focus checks, plus the language features affected by the change. Keep test executables out of the final release root after validation.
- Record source/header and runtime identities for a build. Do not link tests compiled with changed public headers against older objects; do not publish a build when its inputs change during compilation.
- Preserve concurrent work. A changed `xide_app` layout or action enum requires recompiling all of its consumers together. Preserve the last working release while resolving a build mismatch.
