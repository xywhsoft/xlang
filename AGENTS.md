# Runtime and development materials

- Keep normal IDE, language-server and engine execution in the repository root `release/`.
- Keep extra release source snapshots in `dev/release-materials/sources/` and delivery/acceptance reports in `dev/release-materials/provenance/`. Do not recreate `release/sources/` or `release/provenance/`. The active delivery index is `dev/release-materials/provenance/current-delivery.json`. Preserve dependency-owned metadata inside matched SDKs and extension packages.
- Runtime packages keep their resources, extension libraries, public headers, notices and user settings. Detailed development materials belong in a separate sibling bundle. Preserve historical report bytes and their recorded paths; update the mutable delivery/index files and tools when paths move.
- The IDE ships only `xlang_ide.exe`; do not recreate or package `xlang_ide_console.exe`. Use the GUI executable's bounded/hidden diagnostic arguments for validation. `xl.exe` remains the language command-line tool.

- `release/` is a locally generated or externally delivered runtime tree, ignored by Git. Never force-add its files, including SDKs, extension packages, examples, binaries and updater stores. Record delivery hashes in `dev/release-materials/provenance/`; distribute runtime bytes through release archives and the update channel.
