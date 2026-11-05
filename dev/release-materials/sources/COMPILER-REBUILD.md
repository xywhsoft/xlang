# Current compiler, IDE and native source materials

The matched 2026-11-05 delivery uses these source archives in this directory:

- `xlang-development-20261105-88bd73dc-r1.zip`, with its adjacent `.manifest.json` and `.README.md`, contains the committed compiler, build tools, GUI/language-server sources and explicit matched SDK inputs. Extract to a new writable directory and follow that README. Normal build outputs are `release/win_x64` and `release/linux_x64`.
- `xge3-native-20261105-r3-host2/xge3-native-r3-host-image-r2.tar.gz` contains one complete frozen XGE3/XUI3 native source tree with the four-file host-image layer. Its README and streaming `verify_archive.py` describe verification and the distinct host/embedded-IDE build profiles.

The compiler's implementation source matches qualified Core commit `a1d18117b935c4e14c93b59c25b8db0e500826f9`; later committed tools and the language-server cohort are recorded as separate layers. The qualified production images were built with O2. Source archives do not contain build-object caches and do not promise byte-identical binaries across toolchain paths/timestamps.

Current artifact hashes, CPU acceptance, SDK manifests, local installation and inherited runtime assets are indexed by `../provenance/current-delivery.json`. Original reports retain their original paths and bytes. Neither a developer source bundle nor hidden CLI checks substitute for interactive window/GPU acceptance.

Older `xlang/`, `xge/` and `tcc/` snapshots remain historical materials. Their original guide is preserved at `../provenance/xge3-local-publication-20261105/previous-COMPILER-REBUILD.md`; do not treat those earlier build paths or hashes as the current source cohort. Keep source materials and provenance outside the runtime folders.

Apply the five-file `source-provider-tools-successor-20261105/` overlay after extracting the xlang archive. Its README and manifest pin the subsequent SDK alias refresh, production acceptance tool and role documentation to Git commit `1f19a1dec`; no compiler/ABI rebuild is required by this tools-only layer.

Apply the `ide-startup-fix-20261106/` source overlay for the current Windows GUI startup correction. Its source manifest pins the optional metadata-read and DPI-paint ownership changes. The matched engine/CLI/language-server/SDK images remain unchanged. After rebuilding, use `python ide/tools/check_gui_startup.py --runtime release/win_x64 --out <new-external-validation-directory>` for real bounded workbench rendering and isolated session restore.
