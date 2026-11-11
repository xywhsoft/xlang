# XGE 3 examples

Use a matched xlang compiler/runtime, XGE engine and staged `xge` /
`_xge_native` packages. Set `XLANG_PACKAGE_PATH` to their **parent** directory.
The package sources alone do not provide the native SDK or engine binary.
Run from the installed runtime directory; do not combine an older DLL with
these declarations. The commands below assume `$runtime` names that directory.

```powershell
$env:XLANG_PACKAGE_PATH = "$runtime\extlibs"
& "$runtime\xl.exe" "$runtime\examples\xge\headless_frame.xl"
& "$runtime\xl.exe" "$runtime\examples\xge\headless_shape.xl"
```

| Example | Prerequisites and expected assertions | Cleanup |
| --- | --- | --- |
| `headless_frame.xl` | Owned native callback support; no window or device. Explicit dispatch returns 1, then 2, and the captured counter changes. | Clears the callback and calls checked `unit()`. |
| `headless_shape.xl` | Native Shape feature; no engine initialization or device. Clone has an independent ID after modification. | Both owners close in `finally`. |
| `gui_shapes.xl` | Interactive desktop and working 2D graphics backend. Opens the native default window and draws a rectangle/circle until the user closes it. `begin`/`nativeEnd` must succeed. | Clears the persistent frame callback, then checked engine shutdown. |
| `offscreen_pass.xl` | A working desktop graphics backend. Creates a 2x2 texture target, clears through a Pass, prints its RGBA bytes and exits after one frame. | Closes Pass and target, then clears the callback and calls checked `unit()`. |

Run the GUI example explicitly:

```powershell
& "$runtime\xl.exe" "$runtime\examples\xge\gui_shapes.xl"
```

`initNative()` passes the native-supported NULL descriptor default. This does
not demonstrate construction of a caller-managed descriptor with nested raw
pointers. `run()` retains its closure; its `c_int` result is 0 to continue,
positive to stop normally and negative for checked failure.

Both headless examples also pass actual generated-C execution at O0 and O2
on Windows and Linux against the matched Model engine. The interactive
example has compilation/ABI acceptance only. The headless runner never opens
a window. See `docs/xge3-xui3-example-status.md` for the exact evidence and
compiler qualification; this does not qualify a final shipped runtime or GPU.

`resource_provider.xl` demonstrates exact resource bytes, a typed provider callback,
and explicit global registration cleanup. It needs no window or external data.

`texture_pixels.xl` creates two RGBA pixels, changes the second to blue and
asserts the readback bytes without a window. `system_font.xl` resolves the
platform monospace font through the corrected native owned factory. Both have
Windows JIT/static AOT execution evidence in `docs/xge3-texture-operations.md`;
use the matched SDK/engine/compiler cohort rather than an older installed DLL.

`audio_playback.xl` plays the bundled short tone through Sound, Music and
AudioStream, using one AudioGroup at quarter volume. Run it from this examples
directory with a matching audio-enabled SDK and a working system output device:

```powershell
& "$runtime\xl.exe" "$runtime\examples\xge\audio_playback.xl"
```

Expect three short tones, then clean exit. Every voice closes before its group
and checked audio/engine shutdown. Its generated C and dynamic/static builds
are checked; physical output-device playback is a manual example check. The
automated audio suite instead exercises the same public APIs with an explicitly
private, real no-device mixer SDK. It does not install those test images.
