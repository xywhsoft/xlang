# Windows static XGE AOT acceptance

Run `tools/check_static_xge_aot_windows.py` with explicit frozen inputs:

```text
python -B tools/check_static_xge_aot_windows.py --root SOURCE --fixtures-root REPO --production PRODUCT --headers SDK_INCLUDE --packages STATIC_PACKAGES --out FRESH_OUT
```

`PRODUCT` contains the actual O2 production `xl.dll` and compiler-capable
`xlc.dll`. `SDK_INCLUDE` supplies matching private/compiler aliases; the frozen
source supplies its matching public headers and standard TinyCC distribution.
The GCC-built diagnostic host links only the public compiler DLL, with no
compiler/runtime implementation objects. The target PE is produced by that
compiler's ordinary AOT path and serialized/VFS inputs.

The fixture checks color, local owned DataObject creation, independent share,
explicit close and embedded NUL bytes. Its test-only C sidecar calls
sin/cos/sqrt/pow/fmod with volatile runtime operands. The full PE import table must contain only declared OS DLLs and no
XGE/xlang/TinyCC DLL dependency. Genuine byte-preserved MinGW compatibility
definitions may satisfy the five numeric functions statically; actual imports
are reported without inventing five IAT entries. A separate math-only PE using
the same corrected derived MSVCRT definition must show all five real IAT names
and execute the same volatile numeric checks. The binary is
then copied into an otherwise empty directory and executed with package/SDK
variables removed and PATH restricted to Windows/System32. No window, GPU,
network or OS clipboard operation runs.

This tool explicitly emits and executes a TinyCC PE. It is separate from the
JIT-only provider/VFS gate. A failed emission or execution is not an AOT pass;
keep its fresh output and diagnostics. The optional static package root remains
separate from normal dynamic packages and is selected explicitly.

Historical R7 regression: adding the full 41,142-header XRT object after the
real static XGE pipeline reached 55,525 sections exceeded the unchanged 65,280
section-index boundary. A smaller direct API reproduction reached 34,266
sections with the color-containing archive member alone, then rejected that
same XRT object. The independently emitted five-function CRT math PE passed;
it did not qualify full static-XGE AOT. The source/object boundary and raw
production state were retained for the record-aware metadata fix.
