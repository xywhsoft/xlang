# XGE 3 examples

Use a matched xlang compiler/runtime, XGE engine and staged `xge` /
`_xge_native` packages. Set `XLANG_PACKAGE_PATH` to their **parent** directory.
The package sources alone do not provide the native SDK or engine binary.
Run from the installed runtime directory; do not combine an older DLL with
these declarations. The commands below assume `$runtime` names that directory.

```powershell
$env:XLANG_PACKAGE_PATH = "$runtime\extlibs"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xge\examples\headless_frame.xl"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xge\examples\headless_shape.xl"
```

| Example | Prerequisites and expected assertions | Cleanup |
| --- | --- | --- |
| `headless_frame.xl` | Owned native callback support; no window or device. Explicit dispatch returns 1, then 2, and the captured counter changes. | Clears the callback and calls checked `unit()`. |
| `headless_shape.xl` | Native Shape feature; no engine initialization or device. Clone has an independent ID after modification. | Both owners close in `finally`. |
| `gui_shapes.xl` | Interactive desktop and working 2D graphics backend. Opens the native default window and draws a rectangle/circle until the user closes it. `begin`/`nativeEnd` must succeed. | Clears the persistent frame callback, then checked engine shutdown. |

Run the GUI example explicitly:

```powershell
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xge\examples\gui_shapes.xl"
```

`initNative()` passes the native-supported NULL descriptor default. This does
not demonstrate construction of a caller-managed descriptor with nested raw
pointers. `run()` retains its closure; its `c_int` result is 0 to continue,
positive to stop normally and negative for checked failure.

The automated example gate only generates C/IR and compiles against the exact
SDK headers. It never executes these examples or opens a window. Runtime,
forced-OOM and module-unload evidence is tracked separately in the source
repository's `docs/xge3-xui3-example-status.md`. No zero-overhead performance
claim follows from an ABI compile.
