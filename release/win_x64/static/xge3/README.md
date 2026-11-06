# Static XGE / XUI packages

This optional package root embeds selected XGE/XUI native code in the generated
program. It uses the program's single XRT implementation. Select the complete
matching package set explicitly before compiling:

```powershell
$env:XLANG_PACKAGE_PATH = Join-Path $PWD 'static/xge3/packages'
./xl.exe build path/to/program.xl -o path/to/program.exe
```

Run these commands from `release/win_x64`. The root includes `_xge_native`,
`xge` and `xui`; keep all three together. The compiler uses function/data
sections and relocation reachability to discard unused native code. Native
code retains its O2 optimization. Dependencies, import definitions and notices
are included in the packages.

Generated standalone programs need their declared Windows system DLLs but do
not need `xge.dll`, `xl.dll` or `xlc.dll`. Resources or data explicitly required
by the application must still be distributed. JIT execution supports the CPU
paths tested here; reachable native OS static-TLS code requires AOT and is
rejected by the current JIT provider.

The ordinary dynamic packages remain available in `extlibs`. Remove this
environment override to return to the usual package search.
