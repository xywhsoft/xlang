# Standalone CodeMargin model

`xui.createCodeMargin()` returns one unique, watched owner. `CodeMarginModel`
declares native clear/defaults/add/remove/width/visibility/query methods directly,
with native scopes, a real callback ownership graph and checked consumption.
`add(id,kind,width,flags=VISIBLE)` constructs one zeroed native descriptor on
the generated C stack. There is no per-method C forwarding wrapper.

`getState(index)` returns the pointer-free `CodeMarginState`; `getTotalWidth`
and `getRect` use checked native outputs. Invalid geometry/indices and native
allocation errors leave language output assignments unchanged. GetCount is
admitted by the model scope before using the native count-or-zero function.
Unknown or hidden ids keep the native unsupported checked outcome in getRect;
hit-test's normal no-hit result is not silently mapped to an exception API.
`setRenderCallback` and `setEventCallback` hold independent closures and can be
cleared with null. Native replacement or clear can commit before a consuming
callback Drop reports failure. Explicit close invalidates further use.

The render callback receives a callback-period Widget, margin id, line,
callback-period DrawContext and numeric Rect. The event callback receives
Widget, id, line, event type and Rect. Long-lived data must be copied; saving a
callback view does not extend its invocation. The binding uses the existing
generic owned-callback, record, stack-pack, scope and native-view contracts.
It changes no compiler or language rule.

The matched native SDK owns per-role callback slots, rereads admission after
reentrant Hold, detaches before Drop, and pins a whole CodeEdit layout plus
each invocation. The native tests exercise real rendering and input, including
clear during a frame and closure of the editor. Built-in margins retain direct
drawing. Legacy pointer-bearing Get/Layout refuse owned callbacks rather than
export a closure without its owner or conflate the two user pointers.

Native storage measured on x64: model64 requested bytes, slot72, one allocation
each; cached layout entry72 and caller frame96 (including model scope72). The previous
lifetime foundation used model56 and layout entry64. Prepared native frame
loops allocate nothing and make no external Hold/Drop calls, while private
reference changes, graph/Context/checked admission and callback bookkeeping
remain. Numeric language/C loops are measured separately. These results do
not imply CPU equivalence or a zero-cost ownership layer. Frame preparation
copies M layout entries and looks up their lazy slots, O(M*S) in the worst
case for S registered role slots; an invocation uses the cached node directly.
The packed input descriptor uses 48 stack bytes on x64. Language closure
creation/registration costs and actual callback execution remain unqualified;
the one-allocation slot measurement describes the native slot alone.

`examples/code_margin.xl` is a linear, window-free lesson. The language fixture
tests direct packing/numeric queries, output preservation, callback publication,
registration OOM, explicit cycle cleanup and closed-handle rejection. It does
not claim execution of a language Margin render/event callback: real native
consumers are tested in C. CodeEdit's existing internally owned model is not
exposed as a second owner. A long-lived safe alias, hit-test result projection,
writable layout arrays, full UI callback examples and complete release/IDE
delivery retain their separate work/contracts.

See `dev/release-materials/provenance/xui-margin-owned-20261115.json` for exact
builds, hashes, diagnostics, acceptance commands and costs. The full O2 native
parents are followed by a one-unit hosted/two-unit static fast-path successor;
the successor is never described as a full rebuild. No shipping release,
current-delivery, version or online update is installed by this batch.
