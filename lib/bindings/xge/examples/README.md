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
| `shader_preamble.xl` | Matched headers; prints a complete fragment-shader source using the exact native UV preamble. No window, device or shader compilation. | No resources created. |
| `headless_frame.xl` | Owned native callback support; no window or device. Explicit dispatch returns 1, then 2, and the captured counter changes. | Clears the callback and calls checked `unit()`. |
| `error_callback.xl` | No engine/window/device. Reports one typed error and prints `Resource unavailable` from an independent message snapshot. | Explicitly clears the global observer. |
| `headless_shape.xl` | Native Shape feature; no engine initialization or device. Clone has an independent ID after modification. | Both owners close in `finally`. |
| `shape_references.xl` | Native Shape feature; no device. A hit result still reports ID 7 after the scene and original shape close. | Closes all three managed references. |
| `shape_traversal.xl` | Native Shape feature; no device. A depth-first visitor prints `Visited shape 7`. | Closes the retained result, scene and original shape. |
| `shape_mask.xl` | Native Shape feature and OpenGL. Draws an orange layer through a circular mask. | Closes the scene, shape and engine after 90 frames. |
| `input_events.xl` | No window or device. Posts an IME update and a key-up event, then prints their text and key in queue order. | Calls checked `unit()` after consuming the events. |
| `ime_text_client.xl` | Windows with TSF. Opens a window for one frame and submits text and a selection to the native text store. | Clears the client and closes the engine. |
| `drag_event.xl` | No window or device. A local drag with a text payload negotiates a copy and prints `Drag effect: copy`. | Clears the callback and closes the payload. |
| `miniprogram_bridge.xl` | Uses local host callbacks to request a frame, load bytes and receive an audio command. Starts before the host supplies the canvas size, then sets the logical size and pixel ratio. | Closes the resource and detaches the bridge with `miniProgramUnitChecked()`. |
| `miniprogram_touch.xl` | Simulates two touch points at DPR 2 and prints the second point in framebuffer pixels (100, 70). | No window or device. Ends both touches and closes the host and engine. |
| `gui_shapes.xl` | Interactive desktop and working 2D graphics backend. Opens the native default window and draws a rectangle/circle until the user closes it. `begin`/`nativeEnd` must succeed. | Clears the persistent frame callback, then checked engine shutdown. |
| `offscreen_pass.xl` | A working desktop graphics backend. Creates a 2x2 texture target, clears through a Pass, prints its RGBA bytes and exits after one frame. | Closes Pass and target, then clears the callback and calls checked `unit()`. |
| `offscreen_context.xl` | Windows with OpenGL 3.3 core, or an EGL-enabled SDK on Linux. Uses a hidden drawable, clears a 2×2 target and prints four `0a141eff` pixels. On headless Linux use `EGL_PLATFORM=surfaceless`. | Closes the pass, target and context before checked `unit()`. |
| `nine_patch.xl` | A working desktop graphics backend. Draws a stretched panel on the left and a tiled panel on the right from the same texture, then exits. | Clears the frame callback and closes the texture before checked `unit()`. |
| `emoji_pack.xl` | A working desktop graphics backend. Matches an Emoji prefix and shapes a custom SVG glyph with the bundled example font. | The shaped run retains the Emoji pack and font. |
| `textured_quad.xl` | A working desktop graphics backend. Supplies four `Vertex` records, draws a textured quad into a 2×2 target, prints its blue RGBA pixels and exits after one frame. | Closes the Pass, target and texture before checked `unit()`. |

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

`audio_clip.xl` decodes the same tone and calculates its peak sample. Run it
beside `data/tone.wav`. It prints the format, frame count and peak, and shows
how the read-only PCM sequence keeps its own reference after the clip closes.
This example uses the CPU decoder and needs no audio output device.

`sound_loading.xl` uses the same tone through a SoundLoad request: register a
completion callback, start on the control thread, inspect failure and take an
independent Sound. Expect a completion line and one short tone. Run it beside
`data/tone.wav` with the matched audio SDK and a working output device; its
physical playback is manual. The request and result close before audio shutdown.

`audio_fallback.xl` configures the bundled tone before Init. A missing optional
effect uses it automatically; `loadFallbackSound()` creates an independent
second voice. Expect the intentional missing-file diagnostic and two short
tones. Run beside `data/tone.wav` with an output device. Its public production-
SDK builds are checked; physical playback remains manual. The preset is cleared
in `finally`, and a checked Unit also clears pre-init configuration.

`shape_bounds.xl` creates a rectangle and prints all four corners from its owning
Shape's `getOBB` fixed-array value. It requires the matched fixed-output compiler
and SDK, but no window or graphics initialization. Expect corners `(10,20)`,
`(40,20)`, `(40,60)` and `(10,60)` on separate lines, followed by clean cleanup.
Run with `xl run examples/xge/shape_bounds.xl` from the runtime directory.

`glyph_outline.xl` converts A from the bundled fixture font into a Shape and
prints positive outline width/height. Run from this examples directory so
`data/font_fixture.ttf` resolves. It uses no OS font or graphics device. The
glyph index must belong to the exact Font; shaped fallback text can instead
use `GlyphRun.appendOutline()`. The Shape owns copied commands and remains
usable after closing the Font; it is closed before checked engine shutdown.

`font_xrf_memory.xl` bakes A from the same fixture into owned XRF bytes, closes
the source Font, loads the bytes into another Font and prints the encoded size
and positive A advance. Run from this examples directory. It creates no file,
window or graphics device; baking copies native atlas pages into the XRF blob.

`image_alpha.xl` encodes two RGBA pixels in memory, loads straight-alpha Image
storage and calls `premultiply()` twice. The second call succeeds without
changing already-premultiplied pixels. Expect `Premultiplied image: 2 1`.
It needs the matched checked-alpha SDK, no file, window or engine initialization,
and runs from any working directory. The owned Image closes before exit.

`svg_documents.xl` loads `data/vector.svg` through an explicit `SvgCache`, closes
the cache while keeping its document, prints the 64x32 canvas and content bounds,
then changes a clone without changing the original. Run beside this examples
directory's `data/` folder with the matched SVG-enabled SDK. No window, graphics
device or engine initialization is required. Each owned document closes before
exit. Documents from one cache share mutable state; use `clone()` for independent
editing, and create a separate cache on each language thread.

`image_pixels.xl` decodes two in-memory RGBA pixels. `Image.pixels()` creates an
independent bytes snapshot; `readPixels(output)` copies directly into an already
sized Buffer. Both results remain usable after `image.close()`. The example
needs no engine initialization, window, file or service. Snapshot creation
allocates/copies its byte payload; the Buffer path adds no temporary Buffer.

`particle_definition.xl` starts from native emitter defaults, sets the emission
rate, creates an immutable CPU definition and restores its serialized JSON.
Run `xl run examples/xge/particle_definition.xl` from a matched runtime. Expect
`Emitter count: 1`, `Emission rate: 60.0` (spacing may vary), and
`JSON round trip passed`. No engine, window, GPU, file or service is needed.
Both definitions close in `finally`. Typed array creation projects/copies the
records; this lesson does not yet create a simulation world or renderer.

`particle_world.xl` creates one CPU simulation world, registers a typed collision
callback, emits one particle and checks that a collision removes it. Run
`xl run examples/xge/particle_world.xl` with a matched owned-collision SDK.
Expect `Collision removed the particle`. No window, engine Init, GPU, file or
service is needed. Hit fields are writable during the callback only. The World
and immutable definition close in `finally`; the lesson uses a lexical main
and does not require classes, manual DLL loading or a renderer.
# Native value lesson

`touch_value.xl` constructs an eight-slot TouchEvent value and demonstrates an
independent inline-array copy. It reads no device. Run it with `xl run
touch_value.xl`; assertions check the original coordinate and the output shows
the two X coordinates. See `docs/xge3-replay-contracts.md` in the source bundle
for the record/ABI qualification scope.
