# Native artifact selection

`xl build` and the IDE build/run workflow use the same compiler policy. The IDE builds an executable before running it. No `XLANG_PACKAGE_PATH` override is needed for the shipped Windows XGE/XUI SDK.

```powershell
./release/win_x64/xl.exe build ./release/win_x64/examples/xui/button.xl -o button.exe
./release/win_x64/xl.exe build ./release/win_x64/examples/xui/button.xl --native-linkage dynamic -o button-dynamic.exe
```

`--native-linkage auto|static|dynamic` applies to declared native implementations:

| Mode | AOT executable/shared library and generated C | JIT/check |
| --- | --- | --- |
| `auto` (default) | Prefer a declared static variant | Prefer a declared dynamic variant |
| `static` | Require static for packages that declare a linkage | Same explicit choice; provider limitations still apply |
| `dynamic` | Require dynamic for packages that declare a linkage | Same explicit choice |

A package that does not declare a linkage retains its existing implementation. Pure language packages and ordinary source helpers are unaffected. Auto keeps the existing artifact when no preferred alternative is declared. A declared but unavailable or invalid alternative is an error. Explicit choices fail if a marked package cannot satisfy them. There is no heuristic directory search and no silent fallback after an error.

Projects may set the top-level `"nativeLinkage": "dynamic"` (or `"auto"` / `"static"`) in their `.xlp`. The command-line flag overrides this field. IDE project snapshots preserve the field. The public compiler API exposes `xlcSetNativeLinkage(ctx, XLC_NATIVE_LINKAGE_AUTO/STATIC/DYNAMIC)`. Changing the mode or switching between AOT and JIT invalidates incompatible cached native inputs within that context. This does not modify process environment variables.

## Package declaration

The ordinary installed Windows `_xge_native` manifest declares its alternative explicitly:

```json
"native": {
  "linkage": "dynamic",
  "variants": {
    "static": "../../static/xge3/packages/_xge_native/xlang.package.json"
  }
}
```

Paths are relative to the declaring package directory. Selection follows one hop. The selected manifest must declare the requested linkage and retain the same package identity (`name`, `version`, `abiVersion`), language-source list and dependency identities. Only native implementation metadata is substituted; the originally imported language declarations remain authoritative. Observed manifests and actual native files enter the build-input receipt, so IDE invalidation includes the selected archive.

`tools/stage_xge3_packages.py --static-variant <relative-manifest>` and `tools/stage_xge3_static_packages.py --dynamic-variant <relative-manifest>` declare the installed relationship during staging. Declare an alternative only when delivering its matching SDK. Platform SDKs without a delivered static archive keep their existing native implementation.

## XGE / XUI behavior

The shipped Windows static archive retains GCC `-O2`, function/data sections and relocation-based section garbage collection. Only referenced archive members and reachable sections enter the executable; an unused XGE module is not introduced by the selector. The static path resolves XRT against the program's single implementation. The host DLL remains available for JIT and explicit dynamic AOT.

Static executables still depend on Windows system DLLs and application assets. They do not require `xge3_host.dll`, the IDE's `xge.dll`, `xl.dll` or `xlc.dll`. True JIT defaults to dynamic: forced-static JIT remains limited by the existing native provider's OS static-TLS restrictions. Selection adds compile-time metadata work; it adds no per-frame wrapper or dispatch call.

## Verification

`python tools/check_native_linkage_selection.py --cli <real-xl> --out <new-directory>` exercises distinct static/dynamic implementations, CLI and project overrides, malformed/missing alternatives, legacy packages and AOT/JIT reuse of one public API context. Windows delivery also builds the real XUI gallery, inspects its PE imports and runs the resulting static program from a directory without engine/toolchain DLLs.
