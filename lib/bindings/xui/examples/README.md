# XUI 3 examples

Use the matching compiler/runtime, engine and staged `xui`, `xge` and
`_xge_native` packages. `XLANG_PACKAGE_PATH` names their parent directory.
The ordinary Context constructor requires an owned Proxy: `xui.create(proxy)`.
There is no backend-less safe Context constructor or raw-handle reconstruction.
Run from the installed runtime directory named by `$runtime`:

```powershell
$env:XLANG_PACKAGE_PATH = "$runtime\extlibs"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xui\examples\headless_path.xl"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xui\examples\headless_cell.xl"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xui\examples\context_graph.xl"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xui\examples\manager_lifetime.xl"
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xui\examples\document_model.xl"
```

| Example | Prerequisites and assertions | Cleanup |
| --- | --- | --- |
| `headless_path.xl` | Native Path; no Proxy, Context or graphics device. Two commands are stored; clear resets the count. | Path closes in `finally`. |
| `headless_cell.xl` | Owned native Cell support; no window/device. Text and the full unsigned 64-bit value round-trip. | Cell closes. Native UI text rejects embedded NUL. |
| `context_graph.xl` | Shared default XGE Proxy and safe Context construction. Builds a Panel/Button graph without rendering; verifies text and registers a typed owning callback. It does **not** dispatch a click or assert event delivery. | Clears the callback, closes Button, then Context graph and Proxy. |
| `manager_lifetime.xl` | Context-owned Manager support. Creates an unopened MsgBox and a logical MsgTip; dismiss invokes a typed callback exactly once. No OS dialog, frame, browser or clipboard operation is requested. | Clears the callback and closes the children, Context and Proxy in order. |
| `document_model.xl` | Matched default-provenance Model engine, leased-domain runtime and sized-record compiler. Builds a Rich document with no Context/Proxy/window, commits a unique transaction, reads exact UTF-8 text into a scoped Buffer and keeps the immutable snapshot readable after Document closes. | Transaction shell, Document, Snapshot and ChangeSet each release their real native owner. |
| `gui_surface_draw.xl` | Interactive desktop, matching XGE 2D backend, target-Surface and Draw support. Allocates only after a frame begins. Draw ends before release, then the image is presented for one frame and the demo exits. | Releases Draw, Surface, callback, Proxy and engine, including exceptional exits. |

The GUI example is separate and must be requested explicitly:

```powershell
& "$runtime\xl.exe" "D:\GIT\x-lang\lib\bindings\xui\examples\gui_surface_draw.xl"
```

`panelNative()` and `buttonNative()` here use only the native-validated NULL
descriptor default. No caller descriptor containing untracked resources or
callbacks is created. Control setters call the original C symbols; Proxy
drawing calls its typed native function-pointer fields. Font/Surface ownership,
callback epochs and checked error propagation still perform their documented
runtime bookkeeping. This is not a claim of zero total cost.

For a long-running renderer, retain and reuse size-matched Surfaces between
frames. This deliberately single-frame lifetime example does not recommend
reallocating render targets every frame. It does not cover input pumping,
window resize, font selection, layout policy or a full application framework.

Automated example acceptance is Cgen/IR plus strict authoritative-header ABI
compilation only; it never runs these scripts or starts graphics. Allocator
fault injection and lifetime/module-unload tests remain separate test fixtures,
not ordinary application examples. See `docs/xge3-xui3-example-status.md` in
the source repository for the exact boundary and old-example migration gaps.
