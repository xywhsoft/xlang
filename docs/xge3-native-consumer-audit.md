# Native consumers in the full API inventory

## Counted find and replace

`xui.replaceAllText()` takes a string, bytes or Buffer and returns the complete
replacement bytes and the match count. The range defaults to the complete
input, and follows the native byte-offset contract. Flags select regex,
whole-word and escape behavior. An optional Buffer receives native diagnostics.

The generated call passes the three output slots directly to
`xuiFindReplaceAllText`. The runtime takes the payload from the resident XRT
allocator, which also backs `xuiFindFreeText`. The result and count are assigned
only after successful admission. The tests cover interior NUL, empty and
unchanged results, captures, range-restricted replacement, invalid patterns,
and allocation failures. The find example also prints the replacements and
their count.

The Windows and Linux preflight compilers pass JIT, dynamic AOT and static AOT
for both the focused test and the example. Both IR probes reject 28 invalid
contracts before C or public-header emission. Full O2 compiler acceptance is
tracked separately.

[Replace-all preflight evidence](../dev/release-materials/provenance/xui-find-replace-preflight-20261124.json)

## Engine startup, scene ownership and global callbacks

`xge.initNative()` accepts the default or an explicit native descriptor.
`xge.init()` packs the ordinary configuration. Both reach `xgeInitWithCode`
through the same code-owner admission path. `xge.run()` calls
`xgeRunOwnedWithTransfer` with the retained frame callback and the transfer
services for that run. `xge.createScene()` reaches `xgeSceneCreateOwnedWithCode`,
which keeps scene cleanup callable even if the language reference is released.
The audit checks the compiled relocation from each adapter to its native entry.

`xge.requestQuit()` preserves the veto result and raises a checked error when
the engine is uninitialized. `xge.unit()` calls the checked native shutdown.
The owned quit and error registrations can replace legacy C callbacks in the
same native slots. Tests exercise self-removal, frame callback replacement,
quit veto, original exceptions, and an initialization failure followed by retry.

Windows and Linux pass JIT and dynamic and static AOT, including windowed Run
recovery after startup allocation failures, Scene lifecycle exceptions and
allocation rollback, and the `scene_stack.xl` example. The remaining 93
functions without a declaration, compiled consumer or verified replacement
comprise 26 XGE and 67 XUI entries out of 5,884 native functions.

[Engine and scene evidence](../dev/release-materials/provenance/xge-engine-controls-20261124.json)

## Managed edit notifications

`Widget.setEditEvent(captureText, callback)` registers an owned native
notification handler. It covers changes to text, selection, validation,
commit, cancel and composition as exposed by the widget's editing adapter.
Event views expire at callback return. A handler can replace its registration
while the original closure remains alive for the current call.

The callback sees byte counts and selection and composition offsets. For
Input and TextEdit, opt-in capture stabilizes the available text before the
callback runs, and `text()` copies it to an independent string. A CodeEdit
event can report its document byte count with absent text. It does not
flatten its document for this callback.

The tests cover exceptions from notifications whose producers ignore the
return status, allocation failure for a large capture, and replacement
with a legacy C handler. The supplied example observes an Input selection
without opening a window. Native Widget layout probes retain the original
2,208-byte size on Windows and Linux x64.

Windows and Linux pass JIT and dynamic and static AOT. In each warm JIT
sample, 1,000 Input validation notifications with metadata only perform
zero XRT heap operations in either language. Including the loop and
callback, C/xlang take 0.39/3.14 ms on Windows and 0.18/2.21 ms on Linux.

Document source-mode metadata uses the source byte count and selection offsets
without flattening the source. Visual mode uses the plain-text projection.
Opt-in capture keeps one counted copy for the update's notifications, including
embedded NUL bytes. The snapshot stays readable if the handler replaces itself
and edits the document again. If capture cannot allocate, the pending notification
is retried on the next update and the failure reaches the checked caller.

The Terminal adapter reports selection offsets as row times the column count
plus column. The input queue sends counted IME preedit text and clears it on
cancel. The Document and Terminal tests also cover expired views and propagation
of the original callback error. Both platforms pass JIT and dynamic and static AOT.

[Document and Terminal event evidence](../dev/release-materials/provenance/xui-document-terminal-edit-events-20261124.json)

The matched inventories cover 5,884 functions with 4,822 direct declarations.
The remaining 101 entries comprise 34 XGE and 67 XUI functions.

[Managed edit notification evidence](../dev/release-materials/provenance/xui-edit-event-owned-20261124.json)

## Nine-patch style

`xge.ninePatchStyleDefault()` provides the defaults of
`xgeNinePatchInitSimple`. Set `tSource` to supply a crop. The four cut fields
carry the arguments of `xgeNinePatchInit`, and `iMode` and `iColor` carry the
legacy mode and tint. `Texture.drawNinePatch()` passes the style and managed
texture directly to the checked native renderer.

The pixel comparisons exercise full-texture and cropped simple draws, custom
cuts, stretch, tile, and tint against the legacy C rendering path. Windows
and Linux pass JIT and dynamic and static AOT. The warm queue sample records
zero XRT heap operations for 20 draws in each language. The checked API
rejects invalid modes.

With these four replacements recorded, the 102 pending entries comprise
34 XGE and 68 XUI functions out of 5,882.

[Nine-patch style evidence](../dev/release-materials/provenance/xge-nine-patch-style-audit-20261124.json)

## Dynamic tab pages

`Tabs.addPage(title)` appends one page through `xuiTabsAddPage`, with a
checked UTF-8 title and a typed null for the optional raw page output.
`Tabs.getPageWidget(index)` exposes the observed widget separately. The
Tabs control owns its page containers. Existing page identities stay stable
when a page is appended. Removing a page invalidates its widget and attached
children, and closing the context invalidates the remaining views.

Windows and Linux pass JIT and dynamic and static AOT. The tests exercise
empty and Unicode titles, embedded-NUL rejection, and allocation failures
with both empty and populated tab strips. The example renders after
building its pages one at a time.

The matched inventories cover 5,882 functions with 4,820 direct declarations.
The remaining 106 entries comprise 38 XGE and 68 XUI functions.

[Dynamic tab page evidence](../dev/release-materials/provenance/xui-tabs-add-page-20261124.json)

## Tab close callbacks and common edit selection

`Tabs.setClose()` registers through the native owned callback slot. Pointer
dispatch keeps the in-flight closure alive while the handler replaces itself
and removes its page. The callback widget expires when the handler returns.
The original callback error reaches the checked dispatch boundary. Legacy C
registration can replace the owned handler. Injected registration failures
preserve the previous callback and its close button.

`Widget.editGetSelection()` calls the native editing adapter once and returns
both offsets as a checked pair. Tests cover UTF-8 byte offsets in Input,
TextEdit and CodeEdit, collapsed selections, and an unsupported widget that
leaves the caller's destinations intact.

Windows and Linux pass JIT and dynamic and static AOT. A small JIT comparison
of 10,000 prewarmed selection reads recorded zero XRT heap operations in both
C and xlang. The elapsed times, including loop and checks, were 0.55/12.64 ms
on Windows and 0.31/9.10 ms on Linux for C/xlang.

The matched inventories cover 5,882 functions with 4,819 direct declarations.
The remaining 107 entries comprise 38 XGE and 69 XUI functions.

[Tab close and edit selection evidence](../dev/release-materials/provenance/xui-tabs-close-20261124.json)

## Zero-initialized GPU buffers and native mesh factories

`xge.createGpuBuffer(kind, usage, size)` passes a typed NULL and the byte
count to `xgeBufferCreateOwned`. A compiled alias probe checks that it links
to the original native factory. The native factory allocates and initializes
the backing storage in place. A dedicated OpenGL transfer target keeps index
uploads and updates from changing the active VAO's element binding.

The audit follows the owned buffer and mesh factories to their native
implementations and verifies each close policy's actual release relocation.
The matched inventories contain 5,881 functions and 4,817 direct declarations.
The 109 pending entries comprise 38 XGE and 71 XUI functions.

[GPU buffer and mesh evidence](../dev/release-materials/provenance/xge-zeroed-gpu-buffer-20261124.json)

## Standalone XUI code commands

`xui.executeCodeCommand()` packs the native context from managed input views
and calls `xuiCodeCommandExecute` directly. Built-in history commands now
respect read-only mode. The command-map close policy calls the checked native
destroy function. Its relocation covers the legacy void destroy entry.

Matching Windows and Linux audits cover 5,881 functions, including 4,817 direct
declarations. The remaining 113 entries comprise 42 XGE and 71 XUI functions.

[Standalone command evidence](../dev/release-materials/provenance/xui-standalone-code-commands-20261124.json)

## Owned offscreen graphics contexts

`xge.createOffscreenContext()` creates a hidden WGL drawable on Windows
and an EGL PBuffer on EGL-enabled Linux. The selected package source connects
the managed constructor to `xgeGraphicsContextCreateOffscreenOwned` with a
code lease. Generated C calls the original native methods for context info,
sharing, rebinding and graphics operations.

The previous raw EGL descriptors and offscreen records remain separate audit
entries. The managed constructor owns an exclusive renderer and drawable.
An offscreen render target in an existing renderer is available independently
through `xge.createRenderTarget()`.

Matching Windows and Linux audits cover 5,881 functions, with 4,816 direct
declarations and 116 unresolved entries (42 XGE and 74 XUI). The new native
factory is accounted for through its compiled package consumer.

[Offscreen graphics verification](../dev/release-materials/provenance/xge-offscreen-context-20261124.json)

## Backend metadata and scoped native windows

The platform and graphics defaults, current engine descriptors and owned
backend descriptions return the backend kind, callback flags and a copied
nullable name. The native functions provide those fields without invoking
any hooks. The tests cover long names, empty and null names, creator-thread
checks, checked allocation failure, and independent results after changes
to the original descriptor or release of its owner.

`xge.withNativeWindow()` calls the native visitor directly. Windows and Linux
tests pass the live window to a C consumer, reject an expired view, and
preserve the callback error. A manual session without a window reports
`false`. Examples and tests pass JIT and dynamic and static AOT.

In each linkage mode, 1,000 successful visits with a prebound callback use
no XRT heap operations in either C or xlang. Generated C creates a closure
when the named function is passed directly inside the loop. The samples
include the native admission and synchronous callback costs.

The matching audits cover 5,880 functions, with 4,816 direct declarations and
116 unresolved functions. XGE accounts for 42 and XUI for 74.

[Backend metadata and native window evidence](../dev/release-materials/provenance/xge-backend-info-20261124.json)

## Owned Windows IME text clients

`xge.setImeTextClient()` uses the native owned registration. The callback
submits counted text through `ImeTextOutput.set()` while its view is valid.
The native code converts directly into a reusable UTF-16 scratch buffer,
then swaps it into the TSF store after the callback succeeds. It validates
the UTF-8 input, selection boundaries and document offsets. A failed
candidate leaves the previous client installed.

Windows native tests check allocation failure, owner Hold/Drop, creator
thread access, composition and text-store locking, raw client replacement,
and original error preservation. The xlang tests open a Windows window
and exercise real TSF registration and refresh, including stale output
rejection and window teardown. Linux verifies the unsupported result
and that the callback is never invoked.

The matched packages pass JIT and dynamic and static AOT. In each Windows
mode, 1,000 warmed refreshes use the same four-byte payload in handwritten
C and xlang. Both paths record zero XRT heap operations. The observed
elapsed times are 0.229–0.373 ms for C and 2.081–3.545 ms for xlang. These
samples include the loop and callback costs and exclude registration,
buffer growth and interactive input method use.

The matching audits cover 5,873 functions, with 4,809 direct declarations and
123 unresolved functions. XGE accounts for 49 and XUI for 74.

[Owned IME client verification](../dev/release-materials/provenance/xge-owned-ime-client-20261124.json)

## Counted reads from the native IME queue

`xge.readImeEvent()` calls `xgeImeEventRead()` directly with a caller-owned
Buffer. It replaces the borrowed result of the legacy IME getter. A short
buffer returns the required byte length and leaves the event queued. Ordered
reads now retire the corresponding legacy record by sequence. In the prior
engine, a synthetic IME update could retire a native update of the same type.

The native regression injects a counted UTF-8 payload containing NUL through
the production enqueue path. It covers field projection, sequence wrap,
cross-frame queue matching, rejected threads and retained legacy pointers.
The XRT counters show no allocation on reads and one free when consuming a
queued payload. On both matched SDKs, 1,000 empty polls used no XRT heap
operations in either handwritten C or xlang. The xlang qualification runs
through JIT and dynamic and static AOT.

The matching audits cover 5,871 functions, with 4,807 direct declarations and
124 unresolved functions. XGE accounts for 50 and XUI for 74.

[Counted IME verification](../dev/release-materials/provenance/xge-counted-ime-20261123.json)

## Checked drag dispatch and ordered input

`xge.dispatchDragEvent()` prepares a native event on the stack and calls
`xgeDragEventDispatchChecked()` directly. Native code assigns automatic
sequence numbers atomically. The test reproduces a duplicate sequence with
the previous engine and checks 8,000 unique sequences across two threads on
each platform with the fix.

`DataObject.startDrag()` holds a transfer code lease. The local cancellation
fixture closes its original payload view before reading the payload in the
cancel callback. The ordered input methods consume the same queue as the
legacy getter and preserve counted text, including interior NUL. The Actions
close policy uses `xgeActionsFreeChecked()`.

Both platforms passed JIT and dynamic and static AOT for drag, ordered input,
Actions and the drag example. In three warmed samples of 20,000 drag events,
the C and xlang paths both had zero XRT heap operations. Median elapsed times
were 6.206 ms for C and 125.293 ms for xlang on Windows, and 5.643 ms for C and
107.424 ms for xlang on Linux. The sample includes loop and managed callback
costs and excludes registration, payload processing and OS drag.

The matching audits now cover 5,870 functions, with 4,806 direct declarations
and 125 unresolved functions. XGE accounts for 51 and XUI for 74.

[Drag and ordered input evidence](../dev/release-materials/provenance/xge-input-drag-dispatch-20261123.json)

## Shared FlowGraph and Workflow canvas construction

The model overloads of `Context.flowGraphWidget()` and `workflowWidget()`
directly call the new native shared constructors. Both can bind a model and
set colors in one creation call. Workflow projects the colors into its
nested graph descriptor. The matched inventories cover 5,870 native functions,
including 4,805 direct declarations. XGE has 59 unresolved entries and XUI has 74.

[Canvas constructor verification](../dev/release-materials/provenance/xui-flow-canvas-create-20261123.json)

## FlowGraph and Workflow model ownership

The managed factories create shared model records. `FlowGraphWidget.getGraph()`,
`WorkflowWidget.getWorkflow()` and `Workflow.graph()` return independent owners.
The canvas setters acquire their own references. The compiled close policies
call the checked owner-release entries. The compiled canvas trace code visits
one edge for a managed model and traces the values of a legacy owning slot
before promotion.

`Workflow.validateConfigReport()` returns owned diagnostics in schema order.
The runtime fixture compares paths, codes and messages against the fixed-array
C API and checks the legacy capacity limit. It also exercises both model
replacements, independent release and failed allocations on the managed paths.

These findings cover 18 previously unresolved entries. The matched inventories
cover 5,868 native functions, with 133 entries still outstanding. XGE accounts
for 59 and XUI for 74.

[FlowGraph and Workflow verification](../dev/release-materials/provenance/xui-flow-workflow-audit-20261123.json)

## XUI resource sharing

`Proxy`, `Font`, `Surface`, `TextShape` and `DrawContext` expose a checked
`share()` method on the creator thread. Each call acquires one owner of the
same native object. Closing the initial view leaves the shared view usable.
For drawing, `finish()` ends the single session for every view. Its target
and backend survive until that session releases them. An ended session
rejects new shares, and separate manual sessions still reject overlap.

The compiled policies verify each `close()` against the corresponding native
owner-release entry. The default by-value proxy maps to `createProxy()`.
The native share calls allocate no storage. In a warmed sample of 10,000
`Proxy.share()` / `close()` pairs on each platform, the language path used two
managed allocations and two frees per pair. The C path used zero allocations.

The matched audits cover 5,868 native functions, including 4,803 direct
declarations. There are 151 entries without a direct declaration, compiled
consumer or explicit replacement. XGE accounts for 59 and XUI for 92.

[Shared surface example](../lib/bindings/xui/examples/share_surface.xl) ·
[XUI resource sharing evidence](../dev/release-materials/provenance/xui-resource-sharing-20261123.json)

## Audio resource entry points

The old caller-storage load and open functions map to `loadSound()`,
`loadMusic()` and `openAudioStream()`. Their owned factories call the same
native loaders with the requested group. `loadFallbackSound()`,
`createSoundFromClip()` and `Sound.share()` cover the fallback, clip playback
and retain entries.

The compiled header policies verify the `AudioGroup`, `Sound`, `Music` and
`AudioStream` close paths. Music and stream release use the same checked
voice release as sound. This adds 15 mappings to the matched platform audits.
Of 5,863 native functions, 161 still lack a declaration, compiled consumer or
explicit replacement. XGE accounts for 59 and XUI for 102.

[Audio resource audit evidence](../dev/release-materials/provenance/xge-audio-resource-audit-20261123.json) ·
[Audio runtime acceptance](xge3-audio-integration.md)

## GPU resource sharing

`Texture.share()`, `Shader.share()` and `RenderTarget.share()` call the
owned native share entries directly. The original raw AddRef entries map
to these methods. Shader creation maps to the owned factory. The compiled
close policies cover shader, shader variant set, render target and pass
cleanup, including the old cleanup entries that do not return status.

The matched Windows and Linux inventories cover 5,863 native functions,
including 4,798 direct declarations. There are 176 entries without a direct
declaration, compiled consumer or explicit replacement. XGE accounts for 74
and XUI for 102.

[GPU resource sharing evidence](../dev/release-materials/provenance/xge-gpu-resource-sharing-20261123.json)

## Table column setup

`TableView.setColumnsWithCallbacks()` and `TableGrid.setColumnsWithCallbacks()`
replace the raw column setters with a single checked publication of the column
data and optional per-column callbacks. The temporary setup view holds each
candidate owner until the native table can transfer it to the callback directory.
The old configuration is readable during setup and remains active after a
prepublication failure.

The Windows and Linux inventories cover 5,860 native functions. There are
186 entries without a direct declaration, compiled consumer or explicit
replacement. XGE accounts for 84 and XUI for 102.

[Table column setup evidence](../dev/release-materials/provenance/xui-table-column-setup-20261123.json)

## ListView selection storage

`ListView.setSelectionBuffer()` uses a fixed-capacity native allocation shared
by the list and its language views. The old raw setter maps to the owned
setter. The compiled `selectionBuffer()` witness shows its native retain call,
and the resource policy releases the reference when the language view closes.

Both matched hosted package audits compile the Document allocator and font
paths and the TableView and TableGrid lifecycle paths. The inventory covers
5,856 native functions. There are 188 entries without a direct declaration,
compiled consumer or explicit replacement. XGE accounts for 84 and XUI for
104.

[ListView selection buffer evidence](../dev/release-materials/provenance/xui-list-selection-buffer-20261123.json)

## TreeView node data

The C bulk setter, adapter and three raw node queries map to the checked
descriptor, owned adapter and snapshot methods. `setNodesWithData()` and
`setDataAdapter()` publish rows and managed data together. `getNodeData()`
reads a snapshot by ID. The original native node and callback output records
retain their layout and continue to carry explicit C user pointers.

The data adapter moves an owned value into each staged row. A failed row
retires its unpublished output and leaves the previous tree available. Clear
and replacement detach old data before dropping references. The native slot
table is allocated when a value is first stored and the context ownership
graph visits the stored references.

Both platform audits cover 5,849 native functions and record five replacements
in this family. The inventory has 197 entries without a direct declaration,
compiled consumer or explicit replacement.

[TreeView node data evidence](../dev/release-materials/provenance/xui-tree-node-values-20261123.json)

## Widget data and tooltip configuration

`setUserData()` and `getUserData()` call the new retained native value slot
for any widget. The language methods use the existing snapshot contracts
and the native ownership graph visits the retained slot. The callback view
provides the same methods during live callbacks. Custom widget instance state
keeps its own storage and shared-identity semantics.

Raw tooltip setters map to the declared text, owned custom and resolver APIs.
`tooltipInfo()` gives the type, text and placement snapshot. The old C
descriptor and user pointer still serve native callers.

Both platform audits cover 5,844 native functions and record the four
replacements. There are 202 entries without a direct declaration, compiled
consumer or explicit replacement.

[Widget data evidence](../dev/release-materials/provenance/xui-widget-user-values-20261123.json)

## DockPanel window data

The two raw window user-data functions map to `DockPanel.setWindowData()`
and `getWindowData()`. Each language method directly calls the native
retained-value entry. Input and output use the existing independent data
snapshot rules. Data follows the window through layout changes and stays
outside layout serialization. Clearing the panel detaches its old values
before release, so native finalizers can reuse indices safely.

Both platform audits cover 5,842 native functions. The two new declarations
replace the two raw window data entries. There are 206 entries without a
direct declaration, compiled consumer or explicit replacement in this audit.

[DockPanel data evidence](../dev/release-materials/provenance/xui-dock-window-values-20261123.json)

## InventoryGrid per-slot animation

The legacy slot animation setter and query map to `setSlotAnimation()` and
`getSlotAnimation()` in both matched package audits. A typed retained closure
supplies the slot animation in xlang. The original C path continues to accept
the caller's opaque animation object. The managed binding calls one native
function per registration or query. The runtime tests exercise active
invocation retirement, style updates, reused indices and callback errors.

[Per-slot animation evidence](../dev/release-materials/provenance/xui-inventory-slot-painters-20261123.json)

## InventoryGrid slot descriptors

Both matched package audits map the raw slot update and query to
`InventoryGrid.setSlot()` and `getSlot()`. Each makes one call to the
native descriptor API. The native update validates the entire slot and
preserves an installed C animation object. The language query copies the
strings and obtains an independent retained icon reference.

[Slot descriptor evidence](../dev/release-materials/provenance/xui-inventory-slot-descriptor-20261123.json)

## Timeline record values

The six legacy layer, frame and span userdata entries map to the checked
`setLayerData` / `getLayerData`, `setFrameData` / `getFrameData` and
`setSpanData` / `getSpanData` methods in both matched package audits.
The methods call the native retained-value slots directly and project
independent data on input and output. Tests cover layer movement, frame
and span truncation, slot removal, clear-time reentry and ownership release.

[Timeline value evidence](../dev/release-materials/provenance/xui-timeline-values-20261123.json)

## CodeDocument batch transactions

The package binds synchronous preparation, both native commit entry points,
and the retained publication result. The batch close descriptor reaches
`xuiCodeDocumentBatchDestroy` in the compiled object. The creator-thread
guard runs before the native code reads mutable batch state.

The inventory contains 5,799 native functions. Pending entries are 101 for
XGE and 173 for XUI. Worker capture, description, staging and payload transfer
remain in the inventory.

[Batch transaction evidence](../dev/release-materials/provenance/xui-code-document-batch-20261123.json)

## Batch clipboard publication

The two package declarations project format names and counted byte payloads
into native records. The compiled transfer adapters reach
`xgeClipboardSetItemsWithTransfer` and `xuiClipboardSetItemsWithTransfer`, using
the runtime code and ownership capabilities. The XGE adapter is the checked
language replacement for raw `xgeClipboardSetItems`. Both platform audits
record 278 pending functions, 101 for XGE and 177 for XUI.

[Batch clipboard evidence](../dev/release-materials/provenance/xge-clipboard-batch-20261123.json)

## Render target pixel readback

The new `RenderTarget.readPixels()` overload adopts tightly packed RGBA as
owned bytes. It uses the same native readback and color conversion as the
caller-buffer overload. An asymmetric 67 × 5 render verifies row order,
transparency, padded strides and pixel equality. The tests also exercise
allocation failure and use the bytes after the target and renderer close.

The audit maps the raw offscreen read to the sized target read and compiles
the actual GraphicsContext descriptor and ownership domain. This run found
that the module-global EGL case stayed Busy after the host dropped its
reference. The subsequent image-lease build passes this case and the
retained-language-callback case on Linux.

The inventory has 5,797 native functions and 4,735 direct declarations. It
records 281 pending rows, 103 for XGE and 178 for XUI.

[Readback evidence](../dev/release-materials/provenance/xge-target-readback-20261123.json) ·
[Module retirement follow-up](../dev/release-materials/provenance/xge-egl-image-retirement-20261123.json)

## Ownership of native sequences

The audit compiles actual xlang callers for 18 sequence producers. For each
caller it follows the exported entry to the compiled body that reserves the
sequence, reads the native span, retains the owner and adopts the reference.
It then follows the selected policy through its registered consuming function
to the native release. The policy can live in a package header or in the
resident runtime. A symbol imported elsewhere in the object provides no
evidence for a sequence. The relocation tests remove policy and release edges
and verify that the audit rejects those paths.

The token buffer's full and ranged reads map to `capture()` and
`captureInRange()`. The resulting `tokens()` view keeps the selected generation
alive through later edits or cache closure. Windows and Linux also reran the
shape snapshots, 3D mesh geometry and CodeEdit token snapshot in JIT, dynamic
AOT and standalone static AOT.

The 5,796-function inventory now has 310 non-direct functions with compiled
consumers and 469 with explicit replacements. There are 291 rows without a
direct declaration, compiled consumer or explicit replacement. XGE accounts
for 113 and XUI for 178.

[Validation evidence](../dev/release-materials/provenance/xge-retained-sequence-consumers-20261123.json)

## CodeEdit margin layout

`CodeMarginModel.captureLayout()` and `CodeEditMargins.captureLayout()` return
a read-only numeric snapshot with an optional entry cap. The snapshot keeps
the full text rectangle and total visible count. Entries survive model changes
and editor closure through their own native reference. Native capture reads
numeric margin state with the same pixel rounding as the per-margin queries.

This batch mapped the raw clear, query, hit-test, layout and two destroy entries
to their managed APIs. Its header policy evidence covered release of the new
layout. It recorded 314 pending entries at the time. The later sequence audit
above also covers the layout retain call.

[Validation evidence](../dev/release-materials/provenance/xui-code-margin-layout-20261123.json)

## Scene model resolver

`Scene.loadColumnsWithResolver()` projects typed node columns and calls
`xge3dSceneLoadWithResolver` directly. The synchronous callback supplies a
model through a callback-scoped output. Native code retains one model
reference and resolves every URI before creating the staged hierarchy.
The language test covers exception propagation, missing and duplicate
output, mixed pre-resolved and callback rows, expired views, and model
lifetime after the caller closes its reference. The native fixture checks
same-scene nested load rejection and verifies that failed allocations leave
the existing hierarchy and model reference count intact.

The raw `xge3dSceneLoad` row now has an explicit managed replacement. The
audit has 5,791 native functions, 4,731 direct declarations, and 319 rows
without a direct declaration, compiled consumer or explicit replacement.
The remaining rows are 126 for XGE and 193 for XUI.

[Validation evidence](../dev/release-materials/provenance/xge3d-scene-resolver-20261123.json)

## SVG, resource providers and image alpha

`SvgCache.load()` supplies the explicit creator-thread cache path for the
legacy global SVG loader. `Svg.share()` acquires an independent reference to
the same mutable document. A regression loads an SVG from an owned XPack
provider, releases the temporary Resource, and closes the cache and the
original document owner. The shared document and its paint remain usable
until the document contents are cleared. A separate test covers provider
failure, cleared registrations, and scoped paint lifetime.

The scheme and XPack registration methods call the native owning variants.
The compiled `Resource.close()` policy descriptor points to the destructor
that calls `xgeResourceFree`. The checked `Image.premultiply()` applies alpha
conversion once to valid straight RGBA and validates the native layout on
repeat calls. The native repeat-transform function remains available to C.

Windows and Linux passed JIT, dynamic AOT, and standalone static AOT for
the SVG lifetime and Image alpha regressions. Both also passed the existing
pixel snapshot and Buffer read cases, including size checks, OOM, and use
after closing the source Image.

The inventory has 320 entries without a direct declaration, compiled consumer
or explicit replacement. XGE accounts for 127 and XUI for 193. The raw image
pixel address still requires independent qualification for a zero-copy owning
sequence.

[Validation evidence](../dev/release-materials/provenance/xge-svg-resource-audit-20261123.json)

## Particle world defaults and owned factories

`particleWorldDefaults()` makes one call to `xgeParticleWorldDescInit` and
returns its five capacity limits and fixed step. `createDefaultParticleWorld()`
passes a typed NULL descriptor to the existing world factory. The native
constructor chooses the defaults. The explicit options factory still packs
its selected values on the stack. Both paths retain code through the same
world owner and install collision callbacks with `ParticleWorld.onCollision()`.

The compiled adapter has a relocation from its factory to
`xgeParticleWorldCreateOwnedWithCode` and uses the code-lease acquire and
release functions in that same section. `createParticleRenderer()` calls
`xgeParticleRendererCreateOwned` directly and keeps a native graphics lifetime
reservation until checked close.

Windows and Linux passed JIT, dynamic AOT, and standalone static AOT for
the world and renderer. The world test checks all six returned defaults
against C and exercises 32 allocation-failure prefixes for the default
factory. The renderer test reads back pixels, checks the shutdown Busy
result while a renderer is alive, and retries after closing it. Its warmed
render loop used zero XRT heap operations in both C and xlang.

The inventory has 326 entries without a direct declaration, compiled consumer
or explicit replacement. XGE accounts for 133 and XUI for 193.

[Validation evidence](../dev/release-materials/provenance/xge-particle-world-defaults-20261123.json)

## Numeric input formatter

`NumericInput.setFormatter()` declares the native owned callback directly.
The callback returns an owned span. The control checks for NUL and validates
UTF-8 before copying at most 95 bytes into a stack buffer and freeing the
span. The existing Input setter copies the formatted text into editor storage.
The renderer reads the editor text and does not invoke the formatter.
Registration retains the candidate and reserves the owner slot before
formatting. Callback or allocation failure during preparation leaves the
prior formatter and text in place. The UI thread holds the active callback
through the result transfer. Recursive edits to the same numeric control
are rejected with `Busy`.

The native NumericInput regression passed on Windows and Linux. The same
platforms passed JIT, dynamic AOT, and standalone static AOT for the xlang
fixture and rendered the accompanying example.

Windows and Linux JIT both measured 1,000 allocations, 1,000 frees, and
zero reallocations over 1,000 warmed writes for each of two providers. The
generated xlang callback and handwritten C callback return the same six
bytes to the same native formatter slot. This measures heap operations on
the edit path. Registration and CPU timing are outside the measurement.

The native inventory has 330 entries without direct declarations, a
compiled consumer, or an explicit replacement. XGE accounts for 137 and
XUI for 193.

[Validation evidence](../dev/release-materials/provenance/xui-numeric-formatter-20261123.json)

## Toolbar overflow callback

`Toolbar.setOverflow()` configures the overflow button and a retained
handler. Dispatch passes the first hidden index and item count by value
with a callback-scoped sender view. The native dispatcher retains the
selected callback through its return. The handler can clear itself
without invalidating its active invocation. Callback failures reach
the calling event or accessibility operation through the checked
boundary.

Registration uses the existing widget callback ownership table and
invalidates layout before publishing the new configuration. Dispatch
does the callback ownership hold and drop around one typed call. The
example opens a menu listing the hidden commands.

Windows and Linux passed JIT, dynamic AOT and standalone static AOT,
and rendered the toolbar example.

The native inventory has 331 pending functions, with 137 in XGE and
194 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-toolbar-overflow-owned-20261123.json)

## 3D scene node data

`Scene.setUserData()` produces one independent public-data snapshot and
calls `xge3dNodeSetUserValueSnapshot`. The native call retains that graph in
the scene ownership table. `Scene.userData()` returns a copy that can be
changed independently and outlives the node. The raw C pointer slot has a
language alternative using plain data with checked lifetime and errors.

The native test measures 1,000 replacements with already prepared data and an
existing slot, with zero XRT allocations, frees or reallocations. First
insertion under allocation failure leaves the slot empty. A caller can release
its own prepared reference after publication.

Windows and Linux passed JIT, dynamic AOT, independent static AOT and the
node data example. In 1,000 writes of a one-key object to an existing slot,
the old binding made 8,000 XRT allocations and frees and the new binding
made 4,000. The comparison changes only the declared native setter symbol.

The audit has 332 pending functions, with 137 in XGE and 195 in XUI.

[Validation evidence](../dev/release-materials/provenance/xge3d-node-user-snapshot-20261123.json)

## Bulk code-fold queries

The C-language fold scanner writes ranges from source text or a standalone or
CodeEdit-owned document. Fold states copy ranges and visible row mappings
directly into a reusable Buffer. All five native queries preserve the
count-only and bounded-prefix contracts. The Buffer uses whole native
records or integers. The stateless visible-line function accepts a typed
range array and projects it before the call.

Independent native C and xlang fixtures measured zero XRT heap operations
over 1,000 rounds of three queries with pre-sized buffers and a prepared fold
state. The measured queries cover range output, visible-line output and the
source scanner. Generated code calls the five original XGE functions.
Windows and Linux passed JIT, dynamic AOT and standalone static AOT for
both independent fold models and live CodeEdit views.
The audit has 334 pending functions, with 139 in XGE and 195 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-code-fold-bulk-20261123.json)

## Menu, menu bar, toolbar and status bar item queries

Each control's `getItem(index)` makes one checked native descriptor query.
The C result is a fixed-size copy of the item record with borrowed pointers.
xlang copies text during the call and keeps null distinct from empty text.
Menu and menu bar results can include a live submenu view watched by the
Context. The view expires when the submenu closes. A toolbar result can
include a Surface with its own native reference, so the icon can outlive the
toolbar and Context. An invalid index or failed conversion leaves the caller's
assignment targets unchanged and releases any intermediate output.
The status bar returns progress and geometry by value.

The native fixture called each query 1,000 times with zero XRT allocations,
frees or reallocations. Language results incur the applicable text copies and
lifetime bookkeeping. Toolbar paint preparation now checks shared icon
generations on each visited frame, including when the widget is clean.
Unchanged generations reuse the existing render cache. The upstream toolbar
regression reproduced a stale cache before the fix and passed after it on
Windows and Linux, alongside the other three control regressions.

Windows and Linux ran the language fixture in JIT, dynamic AOT and standalone
static AOT modes. The fixture exercises watched views, nullable text,
independent icon ownership, and failure rollback. Generated headers compile
as C11, C++11 and C++17. The menu example drew a complete frame on both
platforms.

The native inventory has 339 pending functions, with 139 in XGE and 200
in XUI. The four existing raw item-pointer functions have typed replacements.

[Validation evidence](../dev/release-materials/provenance/xui-bar-item-snapshots-20261123.json)

## Code layout and visible rows

`codeLayoutHitTest()` maps a point to a line, grapheme column and UTF-8 byte
offset. `codeLayoutCaretRect()` returns the rectangle for a document position.
They use the native scroll, margin, tab and wrapping rules. The folded variants
pass start, end, level and flag columns as one counted native record array.
The packed description borrows both the document and the fold data for the
duration of the call.

`captureCodeLayout()` and `captureFoldedCodeLayout()` use the same native
visible-row traversal as the C buffer API. Capture grows its result storage
as needed during that traversal. It returns an independent snapshot with
content geometry and an immutable row array. `lines()` retains the allocation
for its read-only sequence. The sequence survives changes to the document and
closing the document or the snapshot. Capture failure releases partial results.

The no-fold calls avoid projecting fold columns. Folded calls build temporary
native records. Capture allocates a snapshot and grows the visible-row array.
Iteration reads those records directly. Compile-time C aliases make the
standalone and CodeEdit document projections call the same native functions.

The original C layout regression and the xlang JIT, dynamic AOT and standalone
static AOT cases passed on Windows and Linux. The native fixture compares the
complete snapshot with the caller-buffer result and injects allocation failure
during row-array growth. The xlang fixture checks the retained rows after
closing both the document and the snapshot. Reading 3,000 rows with the
sequence already acquired used zero XRT heap operations in each execution mode.
The native inventory has 343 pending rows, including the snapshot retain
helper whose generated call is recorded separately.

[Validation evidence](../dev/release-materials/provenance/xui-code-layout-snapshot-20261123.json)

## Cascader items and selection paths

`Cascader.setSelectedPath(values)` projects a typed array directly to the
native path setter. Its elements are values at each level. The parent column
of `setItems()` holds indices. The optional count selects a prefix. An empty
path clears the selection, and a missing or disabled item reports a checked
error and keeps the previous path. Programmatic selection follows the native
setter's callback behavior.

`selectedPath()` returns the depth and eight inline integers as a value.
Unused slots are zero. `getItem(index)` returns copied text, value, parent
index and flags. The existing C item pointer carries a native user pointer.
The language query projects the four data fields. Both results survive control
destruction, and an invalid index or failed text copy leaves the assignment
targets unchanged. The existing Buffer path getter keeps its prefix and depth
behavior. Each query calls one native entry. The numeric snapshot performs
zero XRT heap operations over 1,000 queries in the paired C and xlang fixture.
The setter projects its input array and the item getter copies its text.

Windows and Linux ran the upstream Cascader regression and the xlang fixture
in JIT, dynamic AOT and independent static AOT. Both platforms also ran the
graphical lesson. The native audit also records the already available ComboBox
item query as a replacement for the legacy borrowed pointer. This brings the
unmapped function count to 345, with 139 in XGE and 206 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-cascader-path-20261123.json)

## Render target readback and managed replacements

The timeline capture failure also occurred in a dynamic AOT program. Under
single-worker llvmpipe the compositor received complete source and target
rectangles, but the original OpenGL pixel read returned a stale image. In the
same process configuration, waiting for the draw queue before the raw pixel read
restored the highlight. XGE commit `a7eaa14b` adds that wait to synchronous
OpenGL render target readback. The CPU shadow path and ordinary drawing and
presentation keep their existing behavior.

The matched Windows and Linux packages passed the JIT, dynamic AOT and static
AOT renderer fixture. Both platforms captured all 2,632 highlight pixels in the
graphical JIT and standalone static examples. The Linux dynamic example
produced the same full image in six single-worker runs. The existing typed GL
loader test passed on both platforms with and without 3D. The Linux C readback
probe reported pooled medians of 1.060 and 1.061 ms before and after the change
for an 800 × 600 target under two-worker llvmpipe. The three runs in each group
varied more than this difference. Hardware readback latency still depends on the
driver and queued work.

The same review recorded 90 already available managed replacements. XGE
replacements cover owned font loading and creation, font fallback and family
resolution, FontLoad and SoundLoad requests, PNG output and owned SVG textures.
The XUI entries pair legacy callback registration with the managed method for the
same native hook. The native-consumer checker resolves each public language name
in the staged package and verifies the checked request close through the
descriptor and consuming release relocations. The audit now has 348 functions
without a declaration, compiled consumer or recorded replacement, with 139 in
XGE and 209 in XUI. The review records interface paths. Per-interface runtime
qualification and the final matching release remain separate work.

[Validation evidence](../dev/release-materials/provenance/xge-render-target-readback-20261123.json)

## Timeline renderers and scroll state

`TimeLineView.setRenderers()` sets the layer, ruler, frame and span
renderers as one native callback update. Each installed renderer owns its
closure. The selected renderer remains live through invocation and cleanup,
including when it clears the registration from inside the callback. Returning
zero uses the default drawing and a nonzero result replaces it.

The native layer, frame and span records are stack snapshots. A callback can
still read the snapshot after editing the timeline. The language views and
draw context expire on return. Reading a text field copies it to a string
that can outlive the callback. The paired C and xlang checks each read three
frame scalars 1,000 times with zero heap operations.

`scrollSnapshot()` returns the complete scroll model by value. Callers can
keep it after edits or after the widget is closed.

The render fixture passes on Windows and Linux in JIT, dynamic AOT and static
AOT. The graphical example shows the painted span on Windows. In the Linux
Xvfb OpenGL run the renderer callback runs, but the capture clips the timeline.
The preceding engine exhibits the same clipping with built-in span coloring.

[Validation evidence](../dev/release-materials/provenance/xui-timeline-renderers-20261123.json)

## Icon registration and independent references

`IconCategory` can register an icon with a display name, tags and flags.
`addStyledSvgPath()` also accepts fill and stroke settings and a counted
dash pattern in `bytes` or `Buffer`. The generated call validates the
pattern length and alignment and passes the native descriptions directly.
XUI copies the metadata and the dash pattern it keeps. A null tag remains
distinct from an empty tag.

`Icon.retain()` and `IconCategory.retain()` take independent native
references. Unregistering removes the registry entry immediately and the
retained object stays usable. Closing the originating `Context` expires
both its ordinary views and its retained references. `IconReference.close()`
releases its own reference and reports a checked error if that final
release runs a failing custom destroy callback.

Retaining an object allocates a native reference holder and registers an
observer. Prepared liveness and reference-count queries use no heap
operations in the paired C and xlang checks.

[Validation evidence](../dev/release-materials/provenance/xui-icon-registration-20261123.json)

## Widget accessibility providers

`Widget.setAccessibilityProvider()` registers an owned count, node and
action callback group. Count and node callbacks are paired. An empty
group clears the registration. A native callback snapshot protects
the selected closure through the complete call and checked cleanup,
including when a callback replaces its own provider.

The node callback fills an `AccessibleNodeOutput`. The native reply
copies any supplied strings into one allocation and publishes them
only after the callback and mutation checks succeed. The native
snapshot remains readable through a failed query while the same
provider stays registered. xlang reads the result into an independent
snapshot, preserving empty text and missing text.

Action callbacks receive a callback-scoped payload with an explicit
kind. The language methods carry selection, numeric and text payloads
through their typed native entries. The action data and node output
expire on return, while values already copied to xlang remain usable.

[Validation evidence](../dev/release-materials/provenance/xui-widget-accessibility-20261123.json)

## Managed terminal sessions

`TerminalSession` can attach to a `Terminal`, echo local input, write
counted bytes, poll, resize, and terminate. The attachment is weak.
An owned session view keeps it live, `share()` creates an independent
owner, and final release detaches and retires the transport. The
creator thread owns session operations and finalization.

The resize registration owns its language closure in the native
ownership graph. It pins the session through the callback and its
cleanup. During the callback, explicit close and replacement of that
registration report Busy. The callback session view expires on
return. Errors from the callback cross the checked language boundary.

The fake session works on Windows and Linux. The existing process
transport, including SSH via a child process, is Windows-only.
The SSH command formatter can run on both. The validation uses a
local Windows child and checks command formatting with a dummy
host value. Sixel and iTerm2 image handling is still an unsupported
native stub.

The generated calls select the native functions directly. In 1,000
resize and poll pairs with a prepared fake session and no callback,
both xlang and C used zero allocations, frees, and reallocations.

[Validation evidence](../dev/release-materials/provenance/xui-terminal-session-20261123.json)

## Shared particle definitions and XGE header policies

`ParticleDefinition.share()` calls `xgeParticleDefinitionShareOwned` directly.
The engine retains the same immutable emitter data once. The new xlang
view has its own close obligation, so closing the original leaves the
shared view usable. If creating the language view fails, the compiler
releases the acquired native reference before reporting the error.

The consumer audit now compiles policies selected by eleven package
classes for scenes, backends, host sessions, asynchronous loads, and
particles. COFF and ELF relocations lead from each declaration's policy
or ownership domain to the actual native consumers. The checked-close
evidence follows the descriptor's consuming callback specifically.
This verifies the existing ownership paths and identifies the matching
language entry for the legacy releases.

One thousand native share-and-release pairs used zero allocations, frees
or reallocations. One thousand xlang pairs used 2,000 allocations and
2,000 frees for the independently owned language views. Both share
the original emitter data. [Validation evidence](../dev/release-materials/provenance/xge-particle-sharing-20261123.json)

## TableView column painters and grid table views

`TableView.setColumnRenderer()` runs after the individual cell renderer and
before the whole-table renderer. Returning zero continues that sequence.
A positive result completes the cell paint. Checked errors still reach
the render caller when a callback rebuilds the columns during dispatch.
Replacing columns retires their formatters and renderers together. Cell and
draw views stay valid through the current callback and expire on return.

`xui.tableView(grid)` returns a `TableViewPart` for either a TableGrid or a
PropertyGrid. It calls the existing native getter and exposes table queries,
geometry, scroll state, and paint customization. The grid manages its table,
rows, and editors. Closing the grid invalidates saved part views. The
original `getTableView()` remains available for callers using `Widget`.

`xui.cellText()` and `xui.cellTooltip()` expose nullable reads from a cached
cell. An unset or cleared native text span returns `null`. The existing
non-null `text()` and `tooltip()` methods retain their checked contract.

Passing `--table-engine-build` with the matching hosted report adds COFF and
ELF evidence for the native cell-output initialization and cleanup, the
call scope used by `getCell()`, and the initialization of the TableGrid
editor configuration. The audit reports these as compiled native consumers.

The paired measurements run 500 CPU-rendered frames with one active column
callback, the same cell and target, and the same return value. The C and
xlang paths both record two XRT heap allocations and frees per frame. The
sample callback leaves cell text unread. The local timing samples are kept
with the allocation results.

The audit now covers 5,747 native functions, with 4,682 direct declarations,
223 compiled native consumer entries, and 293 explicit replacements. The
552 outstanding entries include 220 in XGE and 332 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-table-column-renderer-20261123.json)

## TableView column formatting and typed rows

`TableView.setColumnFormatter()` supplies text for a column when the cell
has empty text and no formatter of its own. The callback writes
directly to the table's existing output buffer. Text beyond its 255-byte
limit is truncated without splitting a UTF-8 character. The cell and output
views expire on callback return. A new column configuration releases the
old formatters together. Failure before publication preserves the old
configuration. An error from retiring an old callback reaches the caller
after the new configuration is in place.

`TableView.setRows()` and `TableGrid.setRows()` accept `array<TableViewRow>`.
The compiler projects the array into temporary C records for the call. XUI
copies the row data into its own storage. The Buffer overload still calls
the same native entry point. The return value is the native status.

The tests verify that replacing a full 64-column configuration releases its
registered callbacks, failed registration or column allocation preserves the
previous state, and a reentrant change keeps its result when the outer
registration is rejected. Each matched sample reads 5,000 accessible cells.
The native formatter recorded no heap allocations. The xlang formatter
recorded one allocation and free per call. The reports retain the local
timing samples for each build mode.

At this milestone, the audit covered 5,746 native functions, with 4,681
direct declarations, 218 compiled native consumer entries, and 293 explicit
replacements. The 557 outstanding entries include 220 in XGE and 337 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-table-column-formatter-20261123.json)

## TableView body rendering and scroll snapshots

`TableView.setCellRenderer()` owns the callback and keeps it live during
paint. The table passes read-only cell data and a draw context for that call. A
callback can remove itself or change columns. The current paint pass then
stops, and a thrown error still reaches the caller of `render()`. Returning
zero without changing the layout continues with built-in cell drawing.
Reading the cell's C strings produces independent text snapshots.

`TableView.scrollSnapshot()` and `TreeView.scrollSnapshot()` return the
viewport, content size, and offset by value. The snapshot remains readable
after the widget closes. Both return the same values as their native model
getters. The tests run 1,000 reads of each widget from C and xlang under a
zero-allocation budget. The existing native provider tests exercise raw cell
and header rendering, replacement, and destruction from callbacks. These
tests passed 996 cases on each platform. JIT, dynamic AOT, and static AOT
also passed on Windows and Linux.

At this milestone, the audit covered 5,744 native functions, with 4,679 direct
declarations, 218 compiled native consumer entries, and 292 explicit
replacements. The 558 outstanding entries include 220 in XGE and 338 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-table-render-scroll-20261123.json)

## TableView header rendering and shared cell outputs

`TableView.setHeaderRenderer()` accepts an owned callback. Returning zero lets
the built-in renderer draw the title and sort indicator. The callback can
replace columns or unregister itself. Its column descriptor and backing text
stay valid through the current call, and saved descriptors report expiration
after it returns. ID and title reads copy their text into independent strings.

`TableCellOutput.share()` retains the same native heap cell on its creator
thread. Each returned handle closes independently. Native sharing allocates
no memory and copies no payload. In 1,000 paired calls with the same native
cell, the language path allocated and freed 2,000 ownership blocks. The
allocation-failure checks cover both blocks and confirm rollback without
losing the original cell. This comparison measures allocation counts.

At this milestone, the Windows and Linux audits found 5,741 native functions,
including 4,676 with direct declarations, 218 with compiled native consumer evidence,
and 289 with explicit replacements. Another 561 have none of these entries
yet, with 220 in XGE and 341 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-tableview-owned-20261123.json)

## Document Font replacement and export cleanup

The renderer keeps the previous selection if releasing its Font owner is
refused. The pending selection's retained reference is rolled back. When a
finalizer consumes the old owner and then reports a failure, the new selection
stays in place and the failure reaches the caller. Reentrant mutations of the
same output return `Busy` while the native release is running.

Passing the matched hosted engine build record with `--document-engine-build`
adds two native compilation witnesses. COFF and ELF relocations show the
renderer initializing and disposing the callback's Font output and the raw
Document buffer release calling the host's `xrtFree` slot. The audit also checks
the allocator marker and the owning declarations of all seven byte/text export
producers in the staged package.

Document and Document UI now have evidence or explicit replacements for every
remaining function in this inventory. The outstanding count at that milestone was 578, with 220
in XGE and 358 in XUI.

[Validation evidence](../dev/release-materials/provenance/xui-document-font-replace-20261122.json)

## 3D ownership policies and checked close

The audit compiles a reference to each `@policy` and `@domain` selected by the
3D package. COFF and ELF relocations trace those references through the
descriptor to the native release or scene ownership operations. The checked
`close()` replacements require a verified path through the registered
consuming callback.

The policy audit provides compiled evidence for 32 additional native functions.
Another 66 entries gained explicit replacements across checked close, resource
creation, shared references, geometry snapshots and rendering. The native
population and direct declarations remain 5,739 and 4,674. The outstanding
function review is now 581 entries, including six in the 3D group.

The managed model-retirement token needs the sole model reference when it
starts. It reports `Busy` if other owners remain and holds the model through
incremental retirement. `Scene.collider()` and `Scene.colliderMesh()` expose
shape settings and the independently owned mesh. `Mesh3D.geometry()` keeps
retained geometry readable across updates and mesh shutdown.

The recorded runtime cohort exercises checked close, failed allocations,
retained snapshots and GPU shutdown on the same SDK builds. Warm scene and
scene-controls queries still show zero XRT heap allocations in JIT, dynamic
AOT and static AOT on both platforms.

[Validation evidence](../dev/release-materials/provenance/xge3d-owner-audit-20261122.json)

## Shared 3D billboard batches and environment light

`BillboardBatch.share()` obtains another native reference to the same immutable
batch. Closing one handle closes its language aliases and leaves other shares
and `geometry()` sequences usable. The CPU batch can outlive the engine.
Submitting it to a renderer still retains the queue's textures separately.

The language call directly invokes `xge3dBillboardBatchShareOwned`. In 1,000
share-and-close cycles the C path used no heap allocations. xlang allocated and
freed 2,000 owner-side blocks and kept the same native geometry pointer. Two
injected allocation failures rolled back without leaking a reference. These
results matched in Windows and Linux JIT, dynamic AOT and static AOT.

The Linux software-rendering tests cover the updated example, mixed textures,
offscreen target readback, and image-based lighting. The image-based lighting
test verifies that a live environment blocks shutdown with `XGE_ERROR_BUSY`.
`setIBLImages()` accepts the same face and mip data as the original C descriptor.
Use `clearIBL()` for the native null-descriptor case.

The current inventory contains 5,739 native functions and 4,674 direct language
declarations. Five legacy functions now have explicit replacements, leaving
679 functions without a direct declaration, compiled consumer or replacement.

[Validation evidence](../dev/release-materials/provenance/xge3d-billboard-share-20261122.json)

## Reusable 3D animation settings

`xge3d.animationOptions()` initializes a value record with the native defaults.
`Animator.playConfigured()` accepts that record along with an optional mask and
bone mapping. These calls use a C output pointer and a read-only input pointer.
The earlier by-value return worked in Windows JIT but returned incorrect
defaults in Linux JIT. The new and existing playback methods share the same
native implementation.

Windows and Linux passed the animation fixture and bundled example in JIT,
dynamic AOT and static AOT. The fixture covers retained clips, copied masks and
bone maps, playback rollback at three allocation failures, and checked engine
shutdown with live resources. The existing animation rendering fixture also
passed on Linux with Xvfb and llvmpipe.

Both paths made one native playback call per iteration. Across 1,000 warm
calls, C recorded 2,000 allocations and xlang recorded 3,000. Each allocation
was freed. The mask array is borrowed. The extra allocation holds an O(n)
projection of the bone mapping records. CPU time was not measured.

The matched inventory contains 5,738 native functions, including 4,673 direct
language declarations. Seven legacy animation functions now have verified
replacement entries. There are 684 functions still without direct, compiled
consumer or replacement evidence, including 323 in XGE, 358 in XUI and three
across Document and Document UI.

[Validation evidence](../dev/release-materials/provenance/xge3d-animation-options-20261122.json)

## CodeEdit's provider view and hover snapshots

`CodeEdit.providers()` borrows the provider set already owned by the editor,
with native lifetime observation. The view exposes direct requests for all
five provider roles and checked clearing. Registrations use the existing
CodeEdit methods, preserving the editor-specific hover and command behavior.

`captureHover` transfers the native reply block into one additional snapshot
object. The snapshot can outlive the editor and Context. `info()` copies
the text and optional content type when requested. The fixture verifies
that a callback can clear its own registration, that callback-only views
expire, and that a valid reply is discarded when its editor closes during
the request. Native allocation failure leaves an empty output handle.

Windows and Linux passed JIT, dynamic AOT and static AOT. The 1,000-command
comparison reported zero heap operations in both C and xlang on each path.
The hosted and static packages also passed the existing provider lifecycle
and ownership regression on both platforms.

[Validation evidence](../dev/release-materials/provenance/xui-code-edit-providers-20261122.json)

## Standalone XUI code providers

`xui.createCodeProviderSet()` owns five callbacks for completion, hover,
signature help, command execution and command enablement. The native scope
keeps registrations alive through invocation and delivers checked errors.
Completion and signature snapshots take the existing native reply storage.
They stay readable after the provider or editor closes. A single item or
parameter read copies the returned text into language strings.

Windows and Linux ran the same provider fixture through JIT, dynamic AOT and
static AOT. Each mode measured no heap operations in 1,000 warmed commands in
both xlang and C. The managed path still enters the owner, widget and checked
callback scopes. The fixtures also verify callback errors, allocation failure
and prior result preservation. The native owned-provider regression passed 46
checks per platform, including the existing CodeEdit consumers.

The matched inventory has 5,733 native functions and 4,668 direct language
declarations. There are 183 non-direct functions with compiled consumer
evidence and 192 with explicit replacements. These groups can overlap. The
692 remaining functions have neither kind of evidence. The CodeEdit-owned
provider set borrow is still a separate lifetime contract.

[Validation evidence](../dev/release-materials/provenance/xui-code-provider-set-20261122.json)

## XGE formatted clipboard snapshots

`xge.clipboardReadData(format)` calls `xgeClipboardGetDataOwned` once.
It adopts the returned payload as `bytes`. Windows reuses the existing HTML
fragment extraction and PNG conversion. An independently owned PNG from a
matching DIB cache can be handed to the caller directly. X11 copies the
selection property into XRT storage once. Binary formats distinguish an
empty representation from a missing format.

The X11 comparison measured 100 allocations and frees for 100 C reads.
The same xlang reads used 300 allocations and frees. Generated C shows that
the caller materializes the format string and that the bridge creates one
`bytes` value around the native payload. The fixtures inject allocation
failure at all three stages.

The 34-case Document clipboard suite now verifies rich copy and cut on
an isolated X11 server. A second X11 client compares the published HTML and
native fragment against exports captured before the edit. Cut tests also
verify undo, redo and dirty state.

[Validation evidence](../dev/release-materials/provenance/xge-clipboard-formats-20261122.json)

## XGE clipboard text snapshots

`xge.clipboardGetText()` directly calls `xgeClipboardGetTextOwned` and adopts
the resulting XRT buffer. Windows converts its UTF-16 input into that buffer.
X11 copies the returned selection once and tries the UTF-8 MIME target only
when the owner explicitly refuses `UTF8_STRING`. Missing formatted data now
reaches XUI as `XUI_ERROR_NOT_FOUND`, so Document can try plain text.

The X11 fixture observed one allocation and one free per nonempty read in
equivalent C and xlang loops. Generated C adopts the pointer and validates
UTF-8. The bridge also runs the checked-call machinery. The Windows native
fixture substitutes deterministic clipboard data at the link boundary, and
the Windows language fixture checks cold errors. Linux uses an isolated X11
server for native and language success cases. The wider Document fixture
still includes older expectations that X11 rejects formatted writes.

[Validation evidence](../dev/release-materials/provenance/xge-clipboard-text-20261122.json)

## CodeEdit shared find scope

`CodeEdit.setFindScope` joins, switches or leaves the native find scope. XUI
updates both the editor and the scope's member list. It validates the target
and allocates new member storage before detaching the editor from its old
scope. Scope destruction clears the back-reference even when the editor's
Context is already closing. Membership changes schedule the existing find
window refresh.

`Context.getCodeFindScope` verifies the editor's Context and returns the
existing managed `CodeFindScope` or null. The language retains that owner
through its existing native identity registry. The lookup adds a scope check,
registry synchronization and a search across registered roots. The warmed
lookup requires no heap allocation in the checked fixtures. A scope created
solely in C has no language owner and produces a checked error on lookup.

[Validation evidence](../dev/release-materials/provenance/xui-code-find-association-20261122.json)

## Canvas invalidation and verified replacements

XUI skips clean subtrees during render preparation. Canvas must still check the
generation of its backing surface because the application can update it
through the active Proxy. Canvas now opts into frame-dependent preparation.
An external update invalidates its viewport cache and triggers a repaint.
Subsequent prepares reuse the cache while the generation stays the same.
The complete native Canvas fixture and the xlang Proxy write regression
exercise both cases.

The audit records 18 additional public API mappings:

- `Shape` returns owned snapshots of paths, fill and stroke gradients, and
  dash patterns. The gradient's info record identifies its kind and includes
  the focal radius. Native sequences retain the snapshot for independent use.
- 3D scene queries use managed result buffers and expose the written and
  required counts. Shape settings preserve the scalar defaults, and the
  checked debug renderer accepts a managed mesh explicitly.
- `Texture.drawNinePatch` uses the same native geometry and queue as the old
  draw call. `CodeEdit.hitTestDiagnostic` reports a regular miss with index -1
  and returns independent string snapshots for a matching diagnostic.

[Validation evidence](../dev/release-materials/provenance/xui-canvas-frame-prepare-20261122.json)

## Canvas, ListView and Popup scroll snapshots

`Canvas.scrollSnapshot()`, `ListView.scrollSnapshot()` and
`Popup.scrollSnapshot()` return the last committed viewport, content extent
and offset as a `ScrollModel` value. They call their respective XUI functions
directly. Each native function uses the control's existing ScrollFrame or
ScrollView and copies its model once. The query runs on the Context thread and
does not perform layout or dispatch callbacks. The value stays usable after the
control closes. Apply deliberate changes through the control's offset and
content-size methods so its layout and invalidation stay consistent.

The standalone example reads a Canvas at offset (20, 60) without opening a
window. The initial snapshot SDK reproduced the earlier external-surface
cache failure on Windows and Linux. The subsequent fix is recorded above.
[Validation evidence](../dev/release-materials/provenance/xui-scroll-control-snapshots-20261122.json)

## Mini-program initialization and bridge

Call xlang's checked `xge.miniProgramInit()` before the host knows its canvas
size. Use `xge.miniProgramResize()` when the host provides the dimensions and
pixel ratio. Passing positive dimensions to `miniProgramInit` sets the size
immediately. XGE normalizes a finite nonpositive pixel ratio to 1 and rejects
framebuffer sizes that would overflow. The declaration builds one stack
descriptor and calls XGE directly. The platform-reserved canvas, context and
userdata pointers are zeroed. Host callbacks use the owned bridge interface.

An empty `xge.setMiniProgramBridge()` detaches the bridge with a null callback
owner. The fixture observes zero XRT heap operations when clearing an already
empty bridge. The nonempty path retains its callbacks and tests OOM rollback.
The local host example exercises deferred size, frame requests, a resource
provider and audio delivery without opening a window. Real WeChat/WebGL
execution is outside this desktop fixture.
[Validation evidence](../dev/release-materials/provenance/xge-miniprogram-init-20261122.json)

## Document callback output and export ownership

Each font resolver invocation receives an empty output initialized by
`xuiDocumentFontOutputInit`. Select a Font with `output.setFont(font)`.
`output.clear()` releases its selection through the checked native
`xuiDocumentFontOutputClear` operation and leaves the slot usable for another
selection during that callback. If native release refuses the owner, the slot
keeps the selected Font so the caller can retry. `output.setFont()` also
keeps the prior selection if release refuses it and drops the temporary
incoming reference. If the previous owner is consumed and its finalizer
reports an error, the incoming Font becomes the selection and the error is
delivered to the caller. Mutating the same slot from that finalizer returns
`Busy` and reading it produces null until the release finishes. The renderer
disposes the output with `xuiDocumentFontOutputDisposeChecked` when the callback
returns.
The language view expires at the callback boundary. A Font obtained with
`output.font()` owns its own reference.

`xuiDocumentFreeBuffer` remains the release function for raw C outputs.
The language uses the matched resident XRT allocator marker to adopt the
seven counted export outputs as owned values. The C reference fixture calls
`xuiDocumentFreeBuffer` and the language fixture checks the same exports under
allocation failure. [Ownership evidence](../dev/release-materials/provenance/xui-document-font-output-20261121.json)

## Document lifecycle audit at XGE 7fd0650a

The matched Windows and Linux packages provide managed counterparts for 39
previously unclassified Document and Document UI entries. The audit verifies
20 direct replacements for reference acquisition, subscriptions and release
aliases. It also verifies 19 `close()` replacements through 11 class policies.
For each policy, a COFF or ELF relocation chain reaches the descriptor, its
consuming destructor, and the checked native owner-release function.

The runtime coverage exercises shares after closing the original view,
self-unsubscription, removing a peer before delivery, rejecting registration
while notifying, allocation-failure rollback, and expiration of callback
views. It uses the default-provenance managed model factories. Raw custom
allocator objects retain their C ownership contract.

| Family | Remaining without compiled consumer or replacement evidence |
| --- | ---: |
| XGE | 373 |
| XUI | 398 |
| Document | 1 |
| Document UI | 3 |
| Total | 775 |

The remaining Document entries are `xuiDocumentFreeBuffer`,
`xuiDocumentFontOutputInit`, `xuiDocumentRendererGetContext`, and
`xuiDocumentRendererLifetimeWatch`. In particular, a renderer's Context link
is weak and can expire before the renderer. Its language exposure still needs
a verified lifetime contract. [Evidence index](../dev/release-materials/provenance/xui-document-lifecycle-audit-20261120.json)

## XGE 6d58ce7a and matched XUI packages

The November 20 audit found 4,604 directly declared function names in a
population of 5,686. The 1,082 remaining rows include 174 with a compiled
bridge consumer and 36 with a verified public replacement, including
30 from the XUI registry. Another 872 rows still require classification.

Both platforms compiled all seven package-selected bridge units and produced
identical classifications. The audit uses the matched static package's
native-link schema and verifies the language name and declaration of each
replacement against the staged sources. The counts describe source and
consumer evidence. ABI, lifetime and runtime qualification remain separate.
[Evidence index](../dev/release-materials/provenance/xge3-native-consumer-refresh-20261120.json)

## Earlier Native b44 audit

The audited binding population declared 3903 of the 5294 native
function names at Native b44. The other 1391 names are a review population,
not a missing-feature count or a completion percentage. This population
excludes the uncommitted Graph model candidate.

`tools/check_xge3_native_consumers.py` reads the complete surface record and
actual matched package. It checks public header hashes, replays the package
provenance checks, and uses the existing inventory parser on copies of the
package-selected declarations. An older valid package with the same native
headers cannot inherit the current inventory's declaration count.

The tool compiles all seven manifest-selected bridge translation units against
the complete qualified compiler SDK and real Native headers. Undefined object
symbols provide concrete C-consumer evidence. COFF import indirection is
normalized; names are not guessed from lifecycle suffixes. Logical source-group
selectors must have physical definitions, except explicitly reviewed header
aliases. The `xl6Xge3InitPacked` alias is proved through the actual preprocessor
definition and a C relocation to `xl6Xge3InitWithCode`.

Windows and Linux produce the same classification:

| Family | Non-direct names | Compiled bridge consumer | Existing explicit replacement | Neither evidence kind |
| --- | ---: | ---: | ---: | ---: |
| XGE, including 3D | 552 | 31 | 5 | 516 |
| XUI | 755 | 71 | 0 | 684 |
| Document | 57 | 20 | 0 | 37 |
| Document UI | 27 | 9 | 0 | 18 |
| Total | 1391 | 131 | 5 | 1255 |

The five replacement records are taken from the existing reviewed mapping
registry. Both the native target and exact language entry are checked against
the actual package declarations. This table does not establish equivalent
semantics for every replacement or safe lifetime behavior for every consumer.
Every non-direct row retains `contractReview=pending`; previous independently
qualified feature receipts remain valid within their own scope.

The complete reports retain every function's identity, feature conditions,
consumer translation units and replacement, including rows with no consumer
evidence. Missing object imports are not proof of missing functionality:
field projections, indirect tables, alternate factories and legacy interfaces
still require explicit review. The 3903 directly declared names also retain
their separate ABI, ownership, error, thread and runtime qualification duties.

Two platforms pass 48 compilation/symbol/preprocessor commands. Two additional
checks use the genuine previous package and its valid receipt; both reject
exactly the three newly added ray declarations before compiling native objects.
No production code, native library, compiler implementation or release tree is
changed by this inventory. No GPU or network test runs in this gate.

The immutable evidence index is
`dev/release-materials/provenance/xge3-native-consumer-audit-20261117.json`.
The full integration goal, all pending contracts and final matched delivery
remain open. This audit supplies concrete review evidence rather than treating
the absence of a language declaration as a decision to omit an API.
