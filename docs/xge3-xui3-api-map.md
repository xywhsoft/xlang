# XGE 3 / XUI 3 declaration inventory

The current [machine-readable inventory](xge3-xui3-api-map.json) uses the
transfer-r3 frozen public headers, public ABI schema 2 and Proxy version 18.
It records declarations, types, fields, callbacks, conditional macros and
callable fields. It also pins the current language declaration files. This is
an inventory of source contracts, not a claim that every native function needs
a managed method or has been executed.

| Surface | Native functions | Types | Callable fields |
| --- | ---: | ---: | ---: |
| `xge.h` | 1,035 | 281 | 0 |
| `xui.h` | 3,548 | 583 | 162 |
| Separate Document headers | 383 | 85 | 12 |
| Combined | 4,966 | 949 | 174 |

There are zero parse errors. The primary headers contain 1,820 object macro
definitions and seven function macros; the separate headers additionally
contain 40 standalone enums and 316 enum constants. Enum typedefs are also
represented in the type inventory. The independent portable
`xnative_transfer.h` is recorded as a dependency rather than counted twice.
Its actual type declarations seed parsing; unknown type names are not guessed.

The current declaration annotation finds 405 XGE functions, 2,582 XUI functions
and 65 callable fields in the primary headers. Supplementary managed APIs,
lifecycle controls and multiple logical overloads have different counts. The
[Document](xge3-xui3-document-api-map.json),
[Document UI](xge3-xui3-document-ui-api-map.json),
[resource-array](xge3-xui3-array-api-map.json) and
[owning-sequence](xge3-xui3-native-sequence-api-map.json) maps describe those
contracts individually. The complete [supplementary header
inventory](xge3-xui3-api-map-documents.json) preserves native APIs that remain
explicit C interoperability.

## Mapping and ownership

| Native contract | Language representation |
| --- | --- |
| Pointer-free complete record | Exact C record with size, alignment and field proofs |
| Shared native resource | Owned foreign class; independent native acquire on `share()` |
| Context-owned Widget/control | Checked lifetime view and native watch/call scope |
| Persistent callback | Typed Native Owned registration with its own closure owner |
| Callback-period Event/Draw/output | Epoch view; expired access raises a checked error |
| Synchronous pointer and byte count | Reviewed span pinned for the complete call |
| Pointer-bearing input descriptor | Typed stack pack with exact native field proofs |
| Native status and outputs | One native call followed by atomic logical value/tuple admission |
| Retained resource output | Owner reserved before the native producer/acquire |
| Immutable native array owned by a resource | Readonly `NativeSequence<T>` retaining that resource |
| Unproved raw pointer storage/ownership | Explicit C interoperability |

Proxy methods call the actual native function-pointer field. Descriptor packs
initialize native stack structs; there is no per-method C forwarding layer.
`Context.create(proxy)` binds an owning backend at construction. Native Context,
Font, Surface, Draw, retained TextShape and widget slots keep their actual
resource graph edges. `finish()` ends a manual Draw session; `close()` releases
its language owner. The native single-manual-session rule rejects overlap.
Callback Draw views remain callback-period views.

`TextShape.clusters()` and `carets()` return readonly owning sequences. Closing
the original Shape does not invalidate the retained sequence. Traversal reads
the native C arrays directly, without per-element allocation, boxing or record
copy. Acquiring a sequence, retaining its owner and opening its read scope have
real costs. No borrow or byref syntax is introduced.

[Array projection](foreign-array-projections.md) is a different operation:
`array<Surface>` has a different layout from a C pointer array. The compiler
projects and pins an O(n) temporary pointer array before the single native
call. It does not describe that conversion as zero cost. Nullable Buffer query
outputs and native-supported NULL descriptor branches follow their explicit
contracts; unrelated pointer-compatible language overloads are not merged by
name or guessed representation.

The [Document model](xge3-xui3-document-api-map.md) provides seven owned roots,
152 native business entries and exact POD/value projections. Binary fragment
serialization uses the native `XUIFRAG1` envelope, including NUL bytes. The
[Document UI layer](xge3-xui3-document-ui-api-map.md) separately describes
Renderer/View/Selection/Editor resources and its remaining C boundaries.
A deferred checked retirement is waiting work, not successful destruction and
not an error to discard.

[DataObject and drag contracts](xge3-xui3-data-object.md) cover both native
families, counted string/bytes/Buffer input, nullable output queries, independent
sharing and typed callback payloads. Lazy providers belong to the shared Native
generation, rather than the ForeignView that registered them. Native operation,
generation and owner pins cover the callback and cleanup tail. Callback output
views still expire when the callback returns.

## Storage review and costs

The [storage review ledger](xge3-xui3-api-map-storage.json) records reviewed
consumer bodies and their hashes. `--existing-bindings` annotates current
source without regenerating library declarations; a missing or changed body
remains visible. The generator requires an unchanged reviewed contract before
creating a pointer/length span. A `const` pointer alone does not prove that it
cannot escape.

`xuiListViewSetSelectionBuffer` retains a mutable external array. Retaining a
Buffer would not keep its address stable through reallocation, so this raw
optimization is not exposed as an ordinary Buffer span. Normal selection APIs
remain available. Arbitrary mini-program host bridges likewise need an explicit
consume/copy contract before accepting safe byte spans.

The [CPU cost report](xge3-xui3-binding-costs.md) compares raw C, handwritten
checked C and xlang using the same pinned SDK/workload on each platform. Common
ownership scopes, code leases, native watches, callbacks and materialized values
do real work. Owned creation allocates control records; text results copy;
C-string interop validates or scans terminators. Ordinary direct declarations
and zero-copy sequence loops do not imply universal zero elapsed-time overhead.

Cold Run/OS drag/clipboard escape uses four small adapters retaining the actual
caller code capability. Ordinary methods keep their direct native calls.
[Conditional source groups](package-c-source-groups.md) select that source and
its resident COM dependency only when a reachable call needs it. An ordinary
import does not select the COM component. The OS-facing capsule's code leases,
entry pins, graph and checked retirement are necessary lifetime work.

## Matched delivery and evidence

Public headers, engine configuration, runtime policy, generated host contract
and image must be staged together. Language programs use the host-XRT engine
with an independent `xge3_host` image name. The separate IDE process uses the
IDE-profile `xge.dll`, which owns its shared XRT. Merely renaming an old DLL
leaves GNU PE imports referring to its original internal image name.
The stager rejects the ambiguous older host image name.

[ABI fingerprints](xge3-sdk-abi-fingerprint.md) cover every public header and
the canonical full XRT header. The package ships declarations and one hosted
engine image, with no embedded second XRT implementation. Older compiler,
policy or SDK binaries cannot safely consume these source packages.

Acceptance receipts under `dev/release-materials/provenance` keep each exact
source/compiler/SDK cohort and its limits. Historical reports remain unchanged;
a source or partial frontend gate is not a final provider or release gate.
The [maintained example status](xge3-xui3-example-status.md) distinguishes real
CPU execution from interactive examples that only have compile/ABI checks.
The 76 older wrapper tutorials are archived under
`dev/release-materials/sources/legacy-ui-examples-20261105`; they are not counted
as runnable examples for the new packages. GPU/window/real clipboard/browser
and network execution are outside this CPU acceptance scope.
