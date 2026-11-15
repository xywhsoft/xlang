# XUI error notifications

`Context.reportError(code,stage,recoverable=1,operation=null,message=null)`
directly calls `xuiReportError` once. The full overload additionally accepts
`widget,rectangle` between `recoverable` and the nullable text fields. The
short form leaves the native Widget and rectangle zeroed. Both forms use the
existing generic sized-record packing contract, with a 56-byte stack record on
x64; there is no XUI forwarding function or language heap payload object.

The code being reported is distinct from the producer's return status. Code
zero is invalid; dispatch status zero succeeds. The native producer preserves
stage and recoverable values without normalizing them, and copies the supplied
rectangle rather than recomputing it from the Widget. A missing observer is a
successful no-op. Recursive reports from inside an observer are suppressed by
the native Context. Calls require the Context's creator thread.

Register with `Context.setErrorCallback` and clear it explicitly when finished.
Native owned registration and its invocation pin keep a self-removing observer
alive through return. The callback's Context, ErrorInfo and optional Widget
views expire when it returns; saving a view does not extend that epoch. An
ordinary owned Widget remains usable afterward. See
[owned observer contracts](xui-input-owned.md) for registration, retirement and
graph edges.

`info.operation` and `info.message` use the generic C string field snapshot
contract: they check the epoch, scan for NUL and copy a present value into an
independent `string?`. NULL and an empty non-NULL string remain distinct. An
embedded NUL in native text terminates that C string; an embedded NUL in a
language input is rejected before calling the producer. Snapshot allocation
failure is checked and leaves an existing assignment unchanged. Null field
reads require no allocation. Copied text survives source mutation and callback
return. Callback failures propagate at the enclosing checked boundary; an
existing business diagnostic remains the first error while the observer runs.

The linear, window-free lesson is
[`error_report.xl`](../lib/bindings/xui/examples/error_report.xl). With matched
packages installed, run it with `xl run error_report.xl`; it prints the reported
code and `Example notification`, then removes the observer and closes its
Context and Proxy. It disables system-font discovery because no text rendering
is needed.

`tools/check_xui_error_report.py` qualifies the real publisher using mutable
stack text, NULL/empty/NUL inputs, both packed forms, supplied Widget/rectangle,
expired views, two independent copy-failure injections, self-removal, recursion,
observer exceptions, preserved ambient diagnostics and foreign-thread refusal.
It also builds/runs the lesson, checks C11/C++11/C++17 public headers and checks
independent dynamic/static executable imports against a complete O2 compiler
cohort and matched native SDK. No window or network service is needed.

For 1000 notifications with two copied fields, both the language callback and
handwritten C callback using the same native owned-registration protocol perform
2000 XRT allocations, 2000 frees and zero reallocations. This measures prepared,
stable local input strings; registration and closure construction are outside
the counted loop. Input NUL validation, output scans/copies, epoch checks,
Context scopes, callback/private reference pins and checked error bookkeeping
remain work. Equal allocation counts do not establish CPU equivalence or
zero-cost dispatch. Reentrant mutation of captured/global inputs may also
require generic input-owner copies, as documented for
[the XGE error family](xge-error-fields.md).

This is incremental source qualification. Matched final SDK/compiler/IDE
installation and the remaining ecosystem contracts are separate work.
