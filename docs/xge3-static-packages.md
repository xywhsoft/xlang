# Optional static XGE packages

The dynamic packages remain supported and installed unchanged. Prepare a
separate static package root from a successful Native static build:

```powershell
python -B tools/stage_xge3_static_packages.py --root D:/GIT/x-lang --engine-root F:/frozen-xge --engine-build F:/build/xge-static --out F:/packages/xge-static --report F:/packages/xge-static.json
python -B tools/check_xge3_static_packages.py --root D:/GIT/x-lang --packages F:/packages/xge-static --out F:/checks/static-package
```

`links` lists the real archive and declared compiler/system support;
`loadlibs` is empty. `artifact.nativeLink` requires function sections, data
sections and link GC. The generated config rejects compilers without
`XL6_NATIVE_LINK_SCHEMA=1`, so an old compiler cannot silently ignore this
link requirement. The Windows shell icon is excluded unless explicitly
requested with `--include-shell-resource`.

`artifact.xrtModules` is derived from actual archive undefined XRT symbols
and the compiler's exact native-symbol catalog. Multiple module witnesses
for one symbol are all required. Unknown XRT references fail staging.
`xl6HostXrtResolveV1` is removed from the package's resident requirements.
The three actual error/callback helpers remain explicit because the current
component planner does not inspect arbitrary C bodies.

The private cold bootstrap checks the native fingerprint/features and the
immutable static XRT data-model/header/layout contract. It never calls the
dynamic HostXrt binder or resolver. Ordinary declared native methods gain no
new branch, wrapper, boxing or copy. The archive and final linker still
carry normal native scope/RC/synchronization costs. The common policy source
is not split by family in this version; its flat source/component costs are
reported rather than called zero overhead.

The selected Native feature surface is preserved. Full default static C
archives are qualified independently on Windows and Linux; the current
language common policies reject reduced Native profiles. Static headers do
not enable XGE/XUI dllimport/dllexport. Public hosted-binder declarations in
the upstream C headers are not static implementations and are not used by
this package.

Windows support archives contain the original compiler compatibility code,
not GNU import table fragments. Imports are separate declarative .def data
from the real source import archive, including local-to-DLL export aliases
and GNU import kind/hint/ordinal metadata. All selected Windows system
libraries use declared package-local code/import inputs, rather than
undeclared adjacent libraries. MSVCRT/MinGW ANSI-stdio and the exact
compiler/runtime licenses and hashes are recorded. TinyCC must actually
support the selected COFF relocations, section GC and import metadata;
passing GNU links alone is not a TinyCC qualification. No runtime package
or release path is overwritten by these tools.

The full Windows default archive includes a Markdown TU with real static
TLS/SECREL relocations. AOT needs a correct PE TLS directory/template; a JIT
provider without corresponding OS static-TLS support must reject reachable
TLS code. Unselected sections may be discarded by the genuine linker; the
archive feature configuration itself is never silently reduced.
