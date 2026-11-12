# XGE 3 declarations

This source package uses the matching `_xge_native` SDK and its single host XRT.
Version `3.0.0`, package ABI `2`, identifies these language declarations; it does
not rewrite the engine's own version constants.

Methods map directly to the named C symbol. The package contains no C method
wrappers. For example, `shape.moveTo(x, y)` calls `xgeShapeExMoveTo`; `close()`
on an owned root uses the common checked lifetime control and its native policy.
Native failures remain checked language errors. Do not call a native free
function separately on a managed resource.

`setFrameCallback` and `run` retain their closure until replaced, cleared or
engine shutdown. Frame results are `0` to continue, positive to stop normally,
and negative to report failure. `dispatchFrame` permits an explicit event loop.
These are owned native callbacks, not a synchronous-callback approximation of
`xgeRun`. `unit()` maps to `xgeUnitChecked`.

`getBuildInfo()` initializes the native record's `size` through the compiler's
`c_output_size` contract and returns the exact value record. It requires a
compiler containing that declaration contract.

The default module exposes the matching 2D SDK. The optional `xge3d` module is
staged only when the native engine enables 3D. `xgeDebug` additionally needs a
matching debug definition in the SDK's first-include build configuration.
Importing a module never changes feature macros after a header was included.

Names ending in `Native`, such as `initNative`, accept an explicitly caller
managed C descriptor. A nullable descriptor default is exposed only where the
native implementation accepts `NULL`. Init copies the title; other nested
pointers keep their explicit caller lifetime. Both forms retain an independent
engine code owner until final checked shutdown.

The inventory, remaining ownership contracts, measured costs and acceptance
boundaries are described in `docs/xge3-xui3-api-map.md` in the source repository.
The source package is under active completion and is not an assertion that every
native engine subsystem has already passed managed runtime acceptance.

`examples/egl_context.xl` uses the matched optional EGL SDK for actual headless
rendering. `GraphicsContext` is a source candidate: local-object JIT/static
acceptance passes, but module-global/code-residency retirement still blocks
delivery. See `docs/xge3-owning-context.md` and its module-retirement gate before
treating it as fully qualified. The normal runtime/IDE has not been replaced.

## Example resources

`examples/font_faces.xl` uses the bundled generated font to show native Face,
Family, Font creation and value queries. Run it from the examples directory.
Family stretch uses the native percentage scale (normal=100). Font bytes are
copied once by the native loader; the language adds its common checked resource
control. Glyph-run/bitmap and full audio acceptance remain separate work; see
`docs/xge3-font-resources.md` for the exact current boundary.

`init(width,height,title)` packs a stack `xge_desc_t` and calls `xgeInitWithCode`
through one cold code-lease admission;
`initNative(descriptor=null)` preserves the explicit caller-owned descriptor API.
Both declaration forms share that admission through a C macro alias. The title
is copied natively; `setTitle` uses transactional native checked replacement.
The measured language cold path adds one 64-byte code lease allocation compared
with a process-resident C owner; ordinary drawing methods remain direct. See
`docs/xge3-initialization.md` in the source repository. `Font.drawText`
uses `xgeTextDraw`, and `drawTexture` packs one stack draw descriptor for `xgeDrawEx`.
`Sound` shares one native sound record through a checked owned view; its policy
frees the native sound and its outer record once, before engine shutdown.
`examples/games/` contains nine bounded, static-AOT game examples.

## Affine matrices

`examples/shape_matrix.xl` composes a scale and translation, transforms a point
and restores it through the inverse. It requires no window. The eleven
`shapeExMatrix*` functions return plain native value records/scalars through
direct declarations. Translate/Scale/Rotate/Skew construct a complete matrix;
use Multiply(parent, local) to compose them. Point includes translation; Vector
does not. Angles are radians. A singular inverse reports a checked error.
Matrix assignment copies six floats and creates no resource owner. See
`docs/xge3-shape-matrix.md` in the source repository for measured boundaries.

## Image I/O

`examples/image_io.xl` saves two RGBA pixels as PNG, queries their dimensions
and loads an owned Image, without opening a window. `imageInfo(bytes/Buffer,
size=-1)` returns width/height. `loadImage(bytes/Buffer, flags=1, size=-1)` and
`loadImage(path, flags=1)` return the same checked Image owner; close it after
use. Its width/height/stride/format/flags fields are readonly native queries.
Loading defaults to premultiplied alpha; `saveImagePng` defaults to straight
RGBA and accepts bytes or Buffer with explicit width/height/optional stride.
Bad input regions fail before file opening. A later I/O/encoding failure may
leave a partial file. `encodeImagePng(width,height,pixels,stride=0,flags=2)`
encodes bytes or Buffer input directly to owned bytes without a temporary file
or encoded-payload copy. It requires the matching compiler's generic owning
byte-output declaration support. `examples/encode_image_png.xl` needs no window.
Full pixel views remain separate work. See `docs/xge3-image-io.md` and
`docs/foreign-owned-bytes-output.md` for costs and qualification boundaries.

## System fonts

`loadSystemFont(role=..., size=..., weight=..., slant=..., language=null)`
loads the native platform font as an owned `Font`. Roles are `XGE_SYSTEM_FONT_UI`,
`XGE_SYSTEM_FONT_DOCUMENT` and `XGE_SYSTEM_FONT_MONOSPACE`; size 0 requests the
platform default. Reserved flags remain zero. Loading resolves a system font
and creates a native font resource; it is not a per-frame operation.
See `examples/system_font.xl`.

`init(width,height,title,flags=0,runMode=0)` now also projects flags and run mode
into the same stack descriptor. Manual mode initializes engine state without
creating a native window; interactive controls require the normal `run` loop.

## Particle values

`examples/particle_curves.xl` samples a curve and configures a native emitter
without a window or simulation world. `ParticleCurve`, `ParticleGradient`,
`ParticleEmitter` and `ParticleEvent` are exact C value records. Assignment
copies their fixed arrays as part of the value. Curves/gradients have eight key
slots; emitters have sixteen burst slots and three subemitter slots.

`particleCurveEval(curve,time,fallback)` and
`particleGradientEval(gradient,time,fallback)` directly call the native bounded
samplers. Times are normalized lifetime values. Invalid/empty keys or nonfinite
sample times return the supplied fallback; this is not a checked failure.
`particleEmitterInit()` returns all native defaults, while `ParticleEmitter()`
only constructs a zero value. Names remain fixed UTF-8 C byte arrays and must
contain a NUL terminator before passing a description to a native definition.
`ParticleEvent.iEffect` is a world-scoped numeric identity, not an owned resource.

These declarations require `XGE_ENABLE_PARTICLES`. World/definition/renderer
ownership, persistent collision callbacks and simulation operations are a
separate unfinished integration. The value helper example does not certify
those paths or whole-engine performance.

## Window title snapshots

`getTitle()` directly calls `xgeGetTitle` and returns an independent `string?`
snapshot. Native NULL means the engine/title is unavailable on this thread;
an initialized empty title is a present empty string. Call from the engine
creator thread. The copy survives `setTitle()` and engine shutdown.

`setTitle(text)` uses the checked native setter. A read performs a length scan,
one allocation and a copy; keep a snapshot when reading repeatedly. See
`examples/window_title.xl` and `docs/foreign-cstring-results.md`. No forwarding
C wrapper or borrowed pointer escapes into language code.
