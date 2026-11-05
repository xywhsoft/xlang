# XGE direct-XRT static source overlay

This small overlay reuses `../xge-ide-update-20261106/xge3-ide-current-source.tar.gz`.
Do not duplicate that full source archive. Verify both archives against manifest.json.
Extract the base archive (remove its xge3/ prefix), then apply native-overlay/.
Apply xlang-overlay/ to the recorded language HEAD with the matching native-link
compiler integration. The three existing Native source changes are captured
from the current source snapshot and are not owned edits to be broadly staged.
Evidence files retain original report bytes, paths, commands and compiler/member hashes.
No binary, release directory, user setting or historical report is overwritten.

Build with Python -B tools/build/static.py --root FROZEN_NATIVE --out NEW_OUT.
Use --profile default and exact --runtime-notice texts from notices/. On Linux
use python3 and the matching GCC/binutils, glibc and X11/OpenGL development SDK.
Then run tools/test/check_static.py with --root, --build, --xlang-root and --out.
Create a separate language package root with tools/stage_xge3_static_packages.py
and validate its headers/policy with tools/check_xge3_static_packages.py.
The detailed reusable commands and dependency boundaries are in the overlaid docs.

Both platforms passed ten GNU whole-profile/GC CPU steps and six static-policy
compile/header steps. All 145 final Native object bytes equal the corresponding
CPU-qualified set. The normal dynamic-policy branch also compiles unchanged.
These are separate from the Root's actual TinyCC COFF/GC/import-alias/TLS gates.
The Windows Markdown TU has actual TLS templates/SECREL relocations. PE AOT
must provide correct OS TLS metadata; unsupported JIT use must reject it.
Do not call the prepared static package fully TinyCC-qualified until those gates pass.
