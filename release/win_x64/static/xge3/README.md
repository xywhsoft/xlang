# Static XGE / XUI SDK

The ordinary Windows XGE/XUI packages now declare this matching static native artifact. `xl build` and IDE build/run select it automatically. There is no need to change `XLANG_PACKAGE_PATH`.

```powershell
./xl.exe build path/to/program.xl -o path/to/program.exe
./xl.exe build path/to/program.xl --native-linkage dynamic -o path/to/program-dynamic.exe
```

Run these commands from `release/win_x64`. Keep `static/xge3/packages` and `extlibs` together. The static archive uses the program's single XRT implementation. GCC O2, archive extraction and function/data section reachability discard unused native code. Matching headers, import definitions, compiler-support provenance and notices remain inside the SDK.

Standalone static programs need their Windows system DLLs and application assets, but no XGE host or xlang compiler/runtime DLL. True JIT continues to select the dynamic host. Explicit static JIT retains the provider's OS static-TLS restrictions.

The reciprocal manifest references preserve explicit `--native-linkage dynamic` even when this package root was selected manually. See `docs/native-linkage.md` in the repository for the package and project contracts.
