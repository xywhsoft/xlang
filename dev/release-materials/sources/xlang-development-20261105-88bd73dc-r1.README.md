# Final development source bundle

Git source: `88bd73dc2b4615b6dcda189eb896aaf028c173f1`. Compiler/runtime Core C/header inputs are byte-identical to qualified Core `a1d18117b935c4e14c93b59c25b8db0e500826f9`. Other source/tools/docs are from this committed HEAD, never the working tree. No live IDE3 changes, Python caches, old flat release files, validation outputs or historical dev snapshots are included.

The IDE GUI uses the separately frozen `ide-final-source-r1` build inputs and matching dependency SDK; the committed qualified LSP layer has 455 frontend pins and comes from `lsp-final-qualified-r1` / commit `a3276b85d2d`. GUI qualification is the earlier GUI-only build, not a claim that the later LSP was part of that build. The source manifest identifies the explicit frozen GUI input overrides.

Core SDK r3 headers are under `release/win_x64/include` and `release/linux_x64/include`. They include implementation-capable XRT/xruntime headers and templates needed for standalone output. Required XACME source SDK files and their dependency-owned metadata are retained. The GUI dependency SDK includes its matched binary/import-library build inputs; production xlang binaries are excluded.

Rebuild on Windows with MinGW GCC, Python and PowerShell available: run `build.bat` at the extracted root. It uses the current platform SDK, creates the real O2 production profiles and retains normal auxiliary tasks. On x86_64 glibc Linux with GCC/make/Python: run `JOBS=1 bash build_linux.sh`. These are reproduction commands only; no compiler rebuild is performed when creating this bundle. For GUI/LSP use the included IDE tools and `ide/docs/lsp-development.md`; `xlang_ide.exe` is the sole GUI image.

Production qualification: `dev/release-materials/provenance/xge3-production-profiles-20261105/receipt.json` in the repository/development materials. The original immutable evidence paths are preserved there. Runtime Native extension packages remain in the paired runtime delivery; they are not silently rebuilt or substituted by this archive.

Verify every ZIP entry against the adjacent `.manifest.json` SHA256 inventory before extraction. SDK metadata and notices are dependency-owned.

Archive SHA256: `d6152ffbe21499ecf74ee64d7266661d31af42d285c36291c2a6c41f35e49787`.
Manifest SHA256: `26ba28940a0da86ebbe970dae419ee6b45ced38062367d392f21585a75727833`.
