# Native static linking: Windows release qualification

The Windows O2 compiler/runtime and optional static XGE/XUI SDK are installed
in `release/win_x64`. The earlier dated integration report and all failed
attempt records are retained. No external publication was performed.

## Delivered files

- `xl.dll`: 15,164,928 bytes, SHA256
  `7a05d69bea0d4889735e115c2bd9bc1e74280c7a1245d7e955c97d7798310c9e`.
- `xlc.dll`: 21,232,128 bytes, SHA256
  `585edd3bceded526fb226cbe2f5321cc6f7ebcbae566af0da47d7315b5bba623`.
- Matched public `include/libtcc.h`.
- Optional static package root: `release/win_x64/static/xge3/packages`,
  77 files, 30,412,076 bytes. Select the matching `_xge_native/xge/xui`
  triple with `XLANG_PACKAGE_PATH`; see the adjacent README.

The ordinary dynamic packages and IDE/engine executables are unchanged by this
installation. The compiler/runtime continue to use O2.

## Defects corrected during actual PE acceptance

1. Per-function unwind sections caused the genuine 16-bit section limit to be
   reached before GC. Canonical PE `.pdata` storage now has per-record owners;
   relocation reachability selects individual records and their dependencies.
   COMDAT identity and the hard section-index limit remain intact. The full
   serialized XRT object dropped from 41,142 to 20,699 section headers and from
   11,222,441 to 9,332,020 bytes. This is an object-file saving, not a claim
   about the installed DLL size.
2. Separate GCC/MinGW code archives contained backward/cyclic CRT dependencies.
   One deterministic code-only archive preserves all 1,096 original member
   payloads and their order, allowing normal archive fixed-point selection.
   Typed OS import definitions remain separate. The 145 Native objects were
   not recompiled or reduced in feature scope.
3. TCC lost earlier GCC `__imp_` references when later C input introduced the
   same imported function. Import chain preservation and zero-offset thunk
   reuse now work for both input orders, without a math wrapper or guessed
   symbol alias.
4. The public API now preserves the useful compiler diagnostic rather than
   replacing it with a generic C compilation failure.

## Verification

| Scope | Result |
| --- | --- |
| Real Windows production DLL profiles | 46 rows passed: runtime, compiler, JIT/module, AOT, nested compilation, strings and relocated execution |
| PE COFF/TLS focused gates | Ordinary COFF 154 and TLS 212 assertions passed, including actual loader/thread initialization |
| PE import order/thunks | 160 assertions, 8 actual PEs and 8 JIT combinations passed; old-image negative control reproduced the defect |
| Complete static XGE PE | 7 steps passed; color, owned DataObject/share/close, embedded NUL and five volatile math results |
| Independent math-only PE | Five genuine CRT IAT names and numeric execution passed |
| Installed `xl.exe` | 7 steps passed: version, check, C generation, JIT, full static build, imports and relocated standalone execution |
| Fresh Linux serialized provider | 1,564 fresh target objects; 40 source-free and 6 static XGE checks passed |
| Final PE-only patch on Linux | All three actual Linux preprocessing profiles were byte-identical to the qualified prior source; original binary paths and hashes retained |

The complete static PE is 1,062,912 bytes (about 1.01 MiB), imports only
`msvcrt.dll`, `kernel32.dll`, `ole32.dll` and `SHELL32.dll`, and executed from
an otherwise empty directory with SDK/package overrides removed and PATH
restricted to Windows/System32. It has no XGE/xlang/TinyCC DLL dependency.
MinGW compatibility code may resolve math functions statically; numeric
coverage in this program is distinct from the five genuine IAT checks in the
independent math-only program.

These are CPU-only tests. GUI, GPU, clipboard and network tests were not run.
Windows JIT still rejects reachable OS static-TLS code; PE AOT has actual TLS
loader qualification. Linux development providers were verified, but installed
Linux production libraries were not rebuilt or replaced in this stage.

The installed CLI's first diagnostic attempt placed emitted C beside its XL
source with the same base name, which activated the language's native-sidecar
discovery. Correcting the diagnostic layout passed without compiler changes;
the original failure bytes remain archived.

## Source and evidence

The production image uses the frozen core source and the canonical unwind/IAT
overlays. The SDK builder has its own ten-file overlay. Unrelated live
workspace edits are excluded from these builds and commits.

- Final receipt: `dev/release-materials/provenance/native-static-release-20261106-r2/receipt.json`.
- Native/serialized final evidence: `dev/release-materials/provenance/static-native-final-windows-20261106-r1.json`.
- Base source: `dev/release-materials/sources/native-static-core-20261106/manifest.json`.
- Overlays: `sources/tcc-pdata-20261106`, `sources/tcc-iat-chain-20261106`,
  and `sources/static-support-20261106-r2` under `dev/release-materials`.

The final acceptance archive preserves original reports, input paths and
hashes, including prior failed attempts. It does not rewrite old evidence or
label SourceDriver/OnlyTCC test builds as production DLLs.
