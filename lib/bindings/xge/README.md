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
native implementation accepts `NULL`. This does not make a descriptor's nested
pointers owned or extend their lifetime.

The inventory, remaining ownership contracts, measured costs and acceptance
boundaries are described in `docs/xge3-xui3-api-map.md` in the source repository.
The source package is under active completion and is not an assertion that every
native engine subsystem has already passed managed runtime acceptance.
