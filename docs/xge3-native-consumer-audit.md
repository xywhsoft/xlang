# Native consumers in the full API inventory

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

The audit now covers 5,746 native functions, with 4,681 direct declarations,
218 compiled native consumer entries, and 293 explicit replacements. The
557 outstanding entries include 220 in XGE and 337 in XUI.

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
