# WebView event and zoom declarations

`xui.WebView.setEvent(xui.WebViewEventProc?)` calls
`xuiWebViewSetEventOwned` directly. The callback receives a watched `Widget`,
an event integer and the native error integer. Passing `null` clears the
callback. Registration retains the language closure and its captured values;
native publication, dispatch and teardown use the existing callback owner
protocol. A saved sender remains governed by its Context and physical Widget
lifetime. Access after physical destruction is checked, including access
through a saved bound method or typed container.

Callbacks follow the native UI-thread contract. They can replace/clear the
callback or close their sender. A callback exception is delivered at the
enclosing checked language boundary, preserving the original error. This
declaration does not introduce a scheduler or a callback on a different thread.

`getZoomFactor()` calls `xuiWebViewGetZoomFactor` once and returns `c_double`.
The status and output pointer are projected by the generic `@c_output`
contract: an unsuccessful call throws before an output is delivered. The
browser must be ready. `getNativeError()` continues to return the native
integer, including valid negative HRESULT values; it is not a nonnegative
count. Other existing WebView methods keep their previous status-returning
signatures.

The generator now uses the reviewed callback identity when producing both
function signatures and typedefs. This preserves `WebViewEventProc` instead of
also leaving an unused `WebviewEventProc` alias. Physical signatures and the
resolved callback type shape remain pinned and fail closed when changed.

## Qualification and limits

The exact full O2 compiler cohorts and matched 09d XRT SDKs were tested on
Windows and Linux. CPU tests use actual native WebView Close events and the
existing owned test proxy. They cover callback replacement, self-clear,
physical self-close, retained sender invalidation, checked error delivery,
unready/expired zoom queries, and 32 allocation-failure prefixes for callback
replacement. Two warmed sweeps check balanced live allocations. Each case
runs as JIT, dynamic/static EXE and dynamic/static shared module, with checked
module initialization/retirement and import inspection. Header checks cover
C11, C++11 and C++17.

The separate HEAD-only source replay excludes the pending FlowGraph language
candidate. It verifies all nine selected generated files and includes missing,
changed and resolved-shape negative fixtures for the new contracts. The
unrelated Graph working changes remain outside this batch's commit.

The readable `webview.xl` example registers its event before initialization,
loads in-memory HTML on Ready and queries zoom there. It no longer polls state
every frame. The two-frame example checks use SDKs with WebView2 disabled;
they exercise the visible unsupported-backend result, not successful browser
startup, asynchronous Ready/Navigation events, or successful browser zoom.
Those backend checks remain open. No network or security testing was run.

The final batch has 76 build/run/header/import steps and 763 source checks.
Independent verification checks 2,918 Windows and 2,871 Linux files. The six
example readbacks match within each platform. Their original external runner
used a `.png` suffix and incorrectly interpreted raw pixels as a PNG header;
the verification records correct this to raw RGBA, validate stdout dimensions
and byte counts, and retain all original report bytes. Decoded PNGs are only
inspection artifacts, not additional runtime tests.

Generated calls add no business forwarding function. Managed view admission,
closure retention, native callback ownership/snapshots, lifetime watches and
checked errors still have costs. This batch makes no CPU-equivalence or
zero-allocation claim for callback registration or dispatch.

External acceptance reports are indexed by
`dev/release-materials/provenance/xui-webview-events-20261116.json`.
They qualify isolated candidates, not an installed release, the latest pending
XRT producer, arbitrary provider unloading, or complete XGE integration.
