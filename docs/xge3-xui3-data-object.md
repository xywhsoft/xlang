# Data objects and callback views

`xge.DataObject` and `xui.DataObject` expose the corresponding Native owned
objects directly. `createDataObject()`, `share()`, `set()`, `setProvider()`,
`has()`, `formatCount`, `formatAt()` and `readInto()` use the authoritative C
symbols. There is no C forwarding function per method.

`set(format, data)` accepts `string`, `bytes` or `Buffer` without a language-side
encoding or payload copy. Native stores its own copy. The format is a C string:
embedded NUL is rejected before the Native call. Payloads are counted bytes;
embedded NUL is preserved for all three input types.

`readInto(format)` queries the required count with a genuine NULL/0 output.
`readInto(format, buffer)` writes within the Buffer's existing logical length;
it does not resize it. A nonnull empty Buffer follows the normal Buffer scope.
An immediate value that does not fit leaves the output unchanged. A lazy
provider can have delivered successful earlier `write()` commands before a
later command exceeds the capacity: those bytes remain after the checked
error, and the Buffer length remains unchanged. A failed call does not return a
partial language result tuple or required-count result. Query first and allow
for providers changing their output between calls.

Providers have the exact typed shape
`func(DataFormat, DataOutput) as c_int`. Zero succeeds; an exception is carried
through the existing checked callback channel. `DataOutput.set()` resets the
required size; `write(offset, data)` extends it and zero-fills a delivered gap.
Both accept the same three counted input types. Querying has no output copy;
an actual read copies directly into the caller Buffer.
Provider registration retains the closure and its code lease independently of
the registering ForeignView. Native holds the object, generation and callback
owner through the callback and its cleanup tail. An independent `share()` can
therefore keep invoking the lazy provider after the original view closes.

`DataFormat` and `DataOutput` are callback-period views. Saving one in a global,
closure or container does not extend its validity. Access after the callback
returns raises a checked error. `DataFormat.text` is an explicit counted string
copy that can be saved independently. `formatAt()` likewise copies its borrowed
C string result. The original Native strings remain borrowed until that copy.

`xge.DragEvent.data` returns an optional `CallbackDataObject` with read-only
format inspection and `readInto()`. It expires with the event callback.
`share()` performs the actual Native checked acquire and returns a separate
owned `DataObject`; closing the original DataObject does not close this new
owner. `setDragCallbackChecked()` separates a checked status from its
`DragEffectOutput.value`: Native publishes the effect only after callback and
owner cleanup succeed. The output view also expires at callback return.
An arbitrary legacy C object is not upgraded to owned provenance by `share()`;
Native rejects that acquire. Custom C producers remain responsible for the
callback payload's lifetime.

XUI's general `xui_event_t.pData` is an untyped, event-dependent pointer.
`CallbackConstEvent.dragData()` calls the authoritative `xuiEventDragData()`
inline accessor: only ENTER, OVER, LEAVE and DROP with sized event/payload
descriptors yield `CallbackDragData`. Geometric DRAG_BEGIN/MOVE/END/CANCEL
events yield null. The data, effect and external fields remain callback-period
views. `CallbackDragData.data.share()` acquires a separate Native owner. The
source/target raw Widget pointers are not promoted to owned objects.

Resource creation, sharing and a retained callback registration have real
allocation, reference and ownership-graph costs. Ordinary methods keep the
existing checked input scopes and lifetime checks. String results copy;
payload Set copies once in Native; Buffer output uses a scoped writable pin.
Format arguments incur the C-string NUL check; borrowed C-string results also
require a terminator scan, whereas `DataFormat.text` already has a byte count.
These direct declarations do not imply zero elapsed-time overhead compared
with unchecked C.

| Native surface | Language contract |
| --- | --- |
| `CreateOwned`, `ShareOwned`, checked release | Owned factory, independent `share()`, policy-controlled `close()`. |
| Raw `Create`, raw provider/free/user pointers | Explicit C interoperability; no implicit promotion to owned provenance. |
| Raw `AddRef` / `Release` | Not exposed as ordinary methods that could invalidate a managed root; use `share()` / `close()`. |
| Ownership/adapter/preparation/finalization metadata | Used by the private exact-domain policy, not a mutable user record. |
| DataOutput underscore fields | Private Native consumer state; only readonly capacity/required and direct `set`/`write` methods are exposed. |
| Native event source/target pointers | No implicit owner construction from a callback payload. |

## Validation

`tools/check_foreign_xge3_data_object.py` generates all-reachable public ABI
probes, exact public-header checks and O0/O2 client builds. Its `headers-only`
mode explicitly uses fingerprint zero and cannot qualify a runtime SDK. Its
`cpu` mode requires a normal stager report, schema-2 Native build and matching
binary. The CPU fixture exercises both data families and stack-event checked
drag dispatch; no OS drag operation runs.

`tools/check_foreign_xge3_data_cycle.py` uses two actual compiler modules and a
weak Context lifetime observer to verify the real Context → DataObject →
provider closure → Context cycle. Live module globals survive compiler
destruction. Releasing each module clears its own graph; the last checked
retirement drains the engine code lease. The Context watch observes logical
close; final engine unloading is a separate retirement check.

Clipboard and OS drag entry points are compiled against the real headers, but
are not exercised by these CPU tests. They require the matched platform runtime
and current module code-owner support. Historical FP0 compile reports and
development compiler overlays do not establish shipping qualification.
