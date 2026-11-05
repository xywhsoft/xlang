# Document UI direct declarations

Authority is the frozen `include/xui_document_ui.h` from final Model Native
source r5/r6, SHA `e343a8600039805be3c5e481fe1d15dbcd426490ce2ad2b5dca65f78b6574bb2`.
The machine-readable adjacent JSON lists every one of its 165 functions,
logical mappings or individual C-interoperability/lifecycle reasons. The
additional MessageList `addNode` maps `xuiMessageListAddNode` from `xui.h`;
`xui_message_list.c:940-970` validates and copies each supplied string before
publication. No caller userdata is supplied.

The declarations cover six classes: unique owned DocumentRenderer;
Context-owned Widget subtypes DocumentView and DocumentEditor; and the
Context-owned composite HtmlInteraction, WebProvider and ImageRequest.
MessageList's Document methods remain in its existing class. Eleven exact
pointer-free C records and all three enum groups preserve their Native layout
and values. Font set, renderer, view and editor descriptors use typed nested
stack packing; they are not fake POD records containing language pointers.

The private renderer policy has no invented native call scope, RC or watch.
Its real native active-call admission and Resident view pin defer destruction;
its policy traces the actual Snapshot, Font, Proxy, callback and cache edges.
Its Context is weak. The three composite child policies watch identity and
use the parent Context scope; only Context traces their native directory.
Callback outputs expose actual Native set/get/clear or scoped POD fields,
never their private owning fields. Saved callback Snapshot values require
Native `share()`; copying counted callback text creates a normal owned string.

Current independent Windows/Linux acceptance uses the frozen R38 successor
plus 46-frontend nested-pack/NULL overlay, inherited from real full R37.
`document-ui-full-win-r1/results.json` under the shared validation base and
`/home/ubuntu/xlang-validation/document-ui-full-linux-r1/results.json` have
14 passing process rows each. All declared calls are exported compile-only
probes, with C11/C++11/C++17 public-header proof and GCC O0/O2 compilation.
The separate script executes actual View/Editor/MessageList construction,
Document retention, a Renderer Native measurement callback with actual Vec2
and float writes and saved Snapshot ownership, and Context-expiry rejection.
These are CPU-only runs: no frame, drawing, browser, host-file decoding,
network, actual clipboard or shipped-runtime acceptance is asserted.

`lib/bindings/xui/examples/document_ui_headless.xl` is byte-identical to the
actually executed script fixture. A separate real Native busy-paint kernel
checks the private renderer policy: `DEFERRED` with an unconsumed owner keeps
both the checked scope and fallback mailbox healthy, also under diagnostic
OOM. A later creator-thread retry consumes exactly once and fires the physical
watch once. The old policy is retained as a failing negative, and the corrected
policy plus original script pass O0/O2 on both platforms.

Lifecycle/metadata accessors feed private policies; raw callback setters are
replaced by their Owned variants. `GetRangeRects` remains explicit C because
it writes a caller array: it is never mapped as one POD record plus arbitrary
capacity. The two old WebObject adapter functions erase their provider as
`void*`, so they remain explicit C; use typed Owned Renderer callbacks instead.
FontOutput Init is native fresh-storage infrastructure, not a resettable
language method. Typed NULL aliases expose these Native branches through the
same physical function, with no forwarding wrapper:

| Logical method | Native NULL meaning |
| --- | --- |
| Renderer `draw(draw,x,y,clip,color=0)` | Draw without selection highlighting |
| View `clearTableSelection()` | Clear the table rectangle |
| View `clearFindScope()` | Return to the default whole-document find scope |
| Editor `replaceAll(pattern,replacement)` | Replace within the Native default scope |
| Editor `replaceAllEx(pattern,replacement,flags)` | The same default scope with explicit matching flags |
| MessageList `clearDocument(id)` | Detach that item's Document binding |
| MessageList `clearDocumentTableSelection(id)` | Clear that item's table rectangle |

Native `SetActivePosition` and ordinary View `SetSelection` explicitly reject
NULL. No fake clear method or fabricated zero position is provided. The new
aliases retain the ordinary checked Native error behavior and ownership of
every unchanged input; the NULL itself needs no temporary descriptor storage.
The new declaration layer has separate Windows/Linux `document-ui-null-*-r1`
reports: all 159 logical calls have exact C/C++ header and O0/O2 compilation
proof. The CPU script executes both View clears, checks find-scope absence,
uses both Editor default-scope replace calls, and detaches a MessageList
Document while checking that its getter returns null. Renderer drawing remains
compile-only. These reports intentionally retain the previous matched Native
Model r5/r6 SDK and private layer, with the declaration changes listed in
`public-layer.json`; they are not qualification of the final Proxy18/schema2
SDK or shipped artifacts.

Direct declarations remove forwarding functions, not lifecycle work. Stack
packing adds assignments and exact size constants; C-string inputs require
validation and possible termination storage. Font inputs pin real resources;
Native constructors and Document attachment perform their own allocations
and retains. Native callback registration retains captures, scoped views
check their epochs, explicit text reads copy, and Snapshot `share()` adds a
real native owner. No general zero-overhead performance claim is made.
