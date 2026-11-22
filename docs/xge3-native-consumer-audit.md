# Native consumers in the full API inventory

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
