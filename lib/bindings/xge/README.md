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
