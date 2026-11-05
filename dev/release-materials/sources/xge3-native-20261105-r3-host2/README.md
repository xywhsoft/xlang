# XGE3 / XUI3 matched Native source materials — 2026-11-05

This is one complete, compressed source tree for the qualified Native transfer R3,
followed only by the four-file host-image R2 build/test/documentation layer. The
original immutable manifests and build receipts are inside `qualification/`.
The archive contains no live `D:/GIT/xge` working-tree files and excludes the
unowned Pass changes. All vendored dependency/resource source and license files,
including optional profiles and test assets, are preserved. Their presence in
this source archive does not enable optional features in the runtime delivery.

The single tree avoids copying the full R3 and host-image R2 directories twice.
Only `build/` object/dependency caches and Git internals are omitted. The frozen
source inputs include the generated emoji and widget atlas implementation assets.
The historical validation reports retain their original absolute paths unchanged;
those paths document the original run, not a required extraction location.

## Verify and extract

Run Python 3.10+ from this material directory:

```text
python verify_archive.py
tar -xzf xge3-native-r3-host-image-r2.tar.gz
```

`source-manifest.json` records each archive member's exact bytes and SHA-256.
The verifier streams the complete archive, rejects unexpected/duplicate entries,
and does not extract files, load a Native library, or execute tests.

## Rebuild in an isolated output directory

Use GCC and G++ 16.2.0, Python 3.10+, and matching GNU binutils. Windows needs
MinGW-w64 `windres`, `nm`, and `strip`; Linux needs X11, Xi, Xcursor, OpenGL, dl,
pthread, and math development/link libraries. HarfBuzz is vendored; there is no
required network download. `CC` and `CXX` select the toolchain. Build outputs
must remain outside the extracted source tree when comparing immutable inputs.

From this material directory after extraction:

```text
cd xge3
python tools/build/build.py default ../native-host-out --host-xrt
python tools/build/build.py ide ../native-ide-out
```

Pass absolute output paths when invoking from another current directory. The
builder normally derives all selected sources, XRT imports, ABI fingerprint,
resource inputs, object dependencies, and report metadata from this tree.
It uses `-O2`; no LTO or size-oriented code optimization was substituted.
No build was performed while creating this materials bundle.

The default host profile produces `xge3_host.dll` + `xge3_host.lib` on Windows,
or `libxge3_host.so` with that SONAME on Linux. It imports XRT through the host
binding contract and must be used with its matched package/host. The embedded
IDE profile keeps `xge.dll`/`xge.lib` (or `libxge.so`), and owns the IDE process's
XRT. These profiles are intentionally not interchangeable. See
`xge3/docs/host-runtime-image.md` and `xge3/tools/build/host_xrt.py`.

The original host-image R2 qualification used strict relinking of all 145 R3
objects (`0` compiled, `145` reused). Those build caches are intentionally not
redistributed as source. A fresh build above recompiles the same configuration;
exact byte-identical output is not promised across toolchain paths/timestamps.
The original Windows IDE profile compiled 171 units and reused one HarfBuzz
unit. See the immutable `qualification/build-*.json` for exact original flags.
WebView2 is disabled in the qualified build; do not set `XUI_ENABLE_WEBVIEW2=1`
unless deliberately adding its external SDK/loader and matching delivery.

## Runtime notices

`notice-whitelist.json` is a copy plan, not an operation. It lists exact frozen
sources, target paths, and SHA-256 for four default-profile additions, nine
existing matching notices to preserve, and six entries that are not default
runtime assets. It does not change release files or notice/delivery indexes.
The web renderer requires caller-supplied static assets; its DOMPurify/KaTeX/
Mermaid licenses travel with those assets whenever they are distributed, rather
than being treated as compiled Native dependencies. Optional 3D and test-only
licenses remain in this source archive.

Original licenses and copyright notices remain authoritative. This bundle does
not relicense XGE, its dependencies, XRT, the compiler, or the toolchain.
