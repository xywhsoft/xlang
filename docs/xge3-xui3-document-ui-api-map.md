# Document UI direct declarations

The adjacent JSON pins the current `include/xui_document_ui.h` and lists its
175 API functions with their logical mappings or C interoperability and
lifecycle reasons. The initial qualification used the frozen Model Native
source r5/r6, SHA `e343a8600039805be3c5e481fe1d15dbcd426490ce2ad2b5dca65f78b6574bb2`. The
additional MessageList `addNode` maps `xuiMessageListAddNode` from `xui.h`;
`xui_message_list.c:940-970` validates and copies each supplied string before
publication. No caller userdata is supplied.

The declarations cover seven classes, including the independently owned
`DocumentRangeRects` selection result, unique owned DocumentRenderer,
Context-owned Widget subtypes DocumentView and DocumentEditor; and the
Context-owned composite HtmlInteraction, WebProvider and ImageRequest.
MessageList's Document methods remain in its existing class. Eleven exact
pointer-free C records and all three enum groups preserve their Native layout
and values. Font set, renderer, view and editor descriptors use typed nested
stack packing; they are not fake POD records containing language pointers.

The private renderer policy has no invented native call scope, RC or watch.
Its real native active-call admission and Resident view pin defer destruction;
its policy traces the actual Snapshot, Font, Proxy, callback and cache edges.
Its Context is weak. Keep the Context passed to its constructor when needed.
The three composite child policies watch identity and use the parent Context
scope. Their `context()` method returns the original managed Context through
that owner relationship. The Context traces their native directory.
Callback outputs expose actual Native set/get/clear or scoped POD fields,
never their private owning fields. Saved callback Snapshot values require
Native `share()`; copying counted callback text creates a normal owned string.

`xui.documentViewType(context)` and `xui.documentEditorType(context)` expose the
Context's WidgetType registry views. The first call can register the type.
`DocumentEditor` inherits `DocumentView`. Use `widget.isType(type)` to test
inheritance. Closing the Context expires either type view.
See the [headless example](../lib/bindings/xui/examples/document_ui_headless.xl).

`DocumentImageRequest.poll()` pumps completed image work and returns the native
status. `pollResource()` performs one poll and returns a Context-owned
`Resource?` on success. Other statuses raise a checked error from this method.
Successful tickets return `null` if their published generation has since been
removed or replaced. Closing the ticket leaves the published image in the
Context registry. `resource.surface()` retains a separate Surface reference.
The two declarations share the same native poll implementation. The status
path skips registry lookup. The resource path resolves the current generation
after the native cleanup tail, then admits the watched resource view.
See the [async image example](../lib/bindings/xui/examples/document_image_async.xl).
On Windows and Linux, 1,000 repeated polls of the same completed request
recorded no XRT heap allocations, frees or reallocations in either the xlang
getter or handwritten C. The first managed exposure may allocate its watch
record. Repeated exposure still checks thread, owner and resource lifetime.
See the [image poll acceptance record](../dev/release-materials/provenance/xui-document-image-poll-20261120.json).

`xui.loadDocumentImageFileResource()` and `xui.loadDocumentImageMemoryResource()`
publish a named image and return a watched `Resource?` owned by the Context.
Use its `name` in a Document or call `surface()` to obtain a separate Surface
reference. Loader failures raise a checked error. A successful load returns
`null` if a cleanup callback removes or replaces its exact resource before
return. If managed view admission fails after publication, the registry entry
can still be found by name. The original `loadDocumentImageFile()` and
`loadDocumentImageMemory()` retain their status-only contracts. The loaders
share their native implementations. Typed resource output adds a native stack
watch and managed view admission. File loading reads the file once and uses the
same decoder and publication path as memory loading.
See the [named image example](../lib/bindings/xui/examples/document_image_resource.xl).
In a warmed 100-call memory-load loop replacing one PNG entry, xlang recorded
700 XRT heap allocations and frees. The equivalent handwritten C loop
recorded 400 allocations and frees. The language path prepares a C string and
admits a new managed view for each replacement. It passes the encoded bytes
directly to the native loader. See the [synchronous image acceptance record](../dev/release-materials/provenance/xui-document-image-sync-20261120.json).

The optional Windows WebView2 build carries the matching Loader DLL and its
SDK notices. Its XUI package includes the local math and diagram renderer in
`xui/resources/document_web`. Pass that directory to
`xui.createDocumentWebProvider()`. The Windows WebView2 Runtime must also be
installed. Hosted JIT resolves the Loader from the engine DLL's directory.
For native Windows deployments, place the Loader beside the executable and
include the engine DLL when using dynamic linkage.

`DocumentHtmlInteraction.webView()` returns a watched `WebView?` from the same
Context. Closing the interaction expires that child view. The
[local HTML example](../lib/bindings/xui/examples/document_html_interaction.xl)
opens a Document node from memory. Builds without the WebView2 backend report
`XUI_ERROR_UNSUPPORTED` when initializing the browser.

`DocumentRenderer.setWebProvider(provider)` attaches browser measurement and
drawing together. The provider must belong to the same Context. Passing `null`
detaches it. A Context-owned provider that closes detaches automatically, and
layout returns to the ordinary placeholders. An attached provider and custom
object callbacks are mutually exclusive. The application drives
`provider.update()` as before. The renderer observes the cache generation and
reuses its existing object layout invalidation. The application's invalidate
callback remains available. See the [formula example](../lib/bindings/xui/examples/document_web_formula.xl).
The [renderer acceptance record](../dev/release-materials/provenance/xui-document-web-renderer-20261120.json)
includes Windows pixel readback, Linux's checked unsupported path, and native
lifetime and allocation checks on both platforms.

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
replaced by their Owned variants. `DocumentRenderer.collectRangeRects(range)`
runs one layout query and owns the complete result. Its `entries()` sequence
retains the result and reads the rectangle array in place. The geometry stays
readable after the renderer, context and snapshot close. The result buffer
may grow while the query runs. The original `GetRangeRects` C entry accepts
a caller buffer and still reports the total count. The two raw WebObject
adapters accept C callback userdata. The managed Renderer attachment calls
these native operations directly with the observed provider.
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

The current [Document UI regression](../dev/release-materials/provenance/xui-document-view-types-20261120.json)
covers Windows and Linux JIT, dynamic AOT and static AOT. It checks first type
registration under allocation failure, type inheritance, and expiry after
Context close. A local PNG exercises asynchronous decode, publication and
cancellation. The renderer test dispatches a drawing callback on an owned CPU
target, checks expiry of the callback draw view and preserves the original
error. The DocumentEditor example also renders through the actual graphics
backend. The [browser acceptance record](../dev/release-materials/provenance/xui-document-browser-20261120.json)
covers local HTML navigation, the math provider and watched WebView retirement
on Windows, plus checked unavailability on Linux.
