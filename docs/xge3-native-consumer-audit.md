# Native consumers in the full API inventory

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
keeps the selected Font so the caller can retry. The renderer disposes the
output with `xuiDocumentFontOutputDisposeChecked` when the callback returns.
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
