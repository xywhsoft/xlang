# Default Document model bindings

The authoritative machine map is [xge3-xui3-document-api-map.json](xge3-xui3-document-api-map.json). It pins `xui_document.h`, all 237 functions, 27 pointer-free C records and 255 language constants. The allocator marker is compiler/provider ABI metadata. The map distinguishes declarations from compiled or executed paths. The default package declares 172 native business entries, nine resource `close` controls and callback-view methods.

The classes are `Document`, `DocumentSnapshot`, `DocumentChangeSet`, `DocumentPrepare`, `DocumentFragment`, `DocumentSourceIndex`, `DocumentMatches`, `DocumentMarkdownLosses` and `DocumentTransaction`. The first eight share actual native references; `share()` calls the native status/output factory once and reserves its language owner before that call. The transaction has one unique owner. `commit()` and `abort()` end its writer but do not consume its shell; `close()` releases that owner. Default-provenance Owned factories are required. Raw custom-allocator objects cannot be promoted to managed roots.

`createDocument()` packs ordinary settings into one native stack descriptor, leaving custom allocator callbacks NULL. `openDocument` and `deserializeDocument` use their native Owned variants. To preserve native parameter-group order, the latter exposes configuration arguments before its final named `data` argument. `prepareStreamSource(null, utf8="...")` starts a candidate; use `continueStreamSource(previous,"...")` to continue it. The latter supplies the required NULL descriptor and inherits the first options; passing previous through the packed start entry is rejected by native code. See [streaming qualification](xui-document-streaming.md). Run may execute on a worker according to the native contract; publication and live Document/Transaction operations belong to their creator thread. The native immutable snapshot/index APIs support their documented native thread contracts. The current safe ForeignView declarations still enforce their existing creator-thread admission; this binding does not claim cross-thread language-handle access.

`deserializeDocumentFragment(bytes)` accepts the binary `XUIFRAG1` envelope,
including its zero-containing eight-byte metadata block and JSON payload.
Its C `const char*` parameter is a byte carrier. HTML import and the pure-JSON
Document deserializer remain text APIs. C callers release raw serialization
buffers with `xuiDocumentFreeBuffer`. With the matched resident-XRT allocator
contract, `DocumentFragment.serialize()` transfers the native buffer into
owned `bytes` and lets value cleanup release it.

`subscribe` installs the real Native Owned callback. Its `CallbackDocument` and `CallbackDocumentChangeSet` are valid through that callback's tail. Pure reads and POD change info are available there; saving a long-lived handle requires explicit `share()`. A nonzero callback result is a notification failure. Committed content is already published; `notificationFailureStatus()` preserves its original int and `checkNotifications(clear)` delivers the checked failure. No implicit retry or undo occurs.

`nodeInfo` returns these POD values in order: kind, id, parent id, child count, text byte length, source start/end, source-exact flag, syntax start/end, source-segment count, extension-payload byte length, extension version and required flag. `DocumentChangeSet.info()` returns flags, before/after revision, origin, input generation, operation count, undo flag and domain. `linkState()` returns linked/mixed flags. Each is one real native call on a zeroed, size-initialized stack output. Failure prevents all field projection. The native pointer-bearing records are never exposed as plain language values.

`copyText`, `copySource`, `copyExtensionPayload`, and `Read` methods write directly into initialized `Buffer` storage. The buffer's logical byte length is the capacity supplied to native code and stays unchanged on success or failure. Text Copy methods require the reported byte length plus one terminator byte. Binary Read and extension methods follow their exact native byte limits. Explicit decoding or `Buffer.text(size)` turns copied text into an independent language string. Native status and UTF-8 validation remain authoritative.

`DocumentMatches.ranges()` and `DocumentMarkdownLosses.entries()` expose
`NativeSequence` views. Each sequence retains the result and directly reads
its contiguous records. Closing the result or the snapshot leaves retained
sequences readable. Ranges still carry the original document revision.
The search and analysis allocate their result storage and may copy accumulated
records as capacity grows. Sequence publication does not copy those records.

Capability boundaries and available safe paths:

| Native functions/fields | Boundary and available safe path |
|---|---|
| `SnapshotAnalyzeMarkdownConversion`, `SnapshotFind`, `SnapshotFindEx` | `collectMarkdownLosses()`, `find()` and `findIn()` return immutable result owners with all entries from one analysis or search. `findIn()` restricts the search to a range. The original C functions retain their caller-supplied buffer API. |
| `PrepareSource`, `PrepareContinueSource` | Now mapped by `prepareSource` / `continueSource` and `Bytes` variants. Parallel start/end/text columns form size-prefixed records with exact byte lengths and scoped backing pins. See [source-patch qualification](xui-document-source-patches.md). |
| `FragmentSerialize`, `FragmentExportHtml`, `SnapshotCopyRange`, `SnapshotCopyTableMatrix`, `SnapshotCopyPlainText`, `DocumentSerialize`, `DocumentExportHtml` | Direct owned bytes outputs; HTML/plain/range/matrix also have UTF-8 string outputs. The actual resident-XRT allocation is transferred with its explicit byte count. The matched SDK allocator marker is required. Native `DocumentFreeBuffer` stays internal to language cleanup. |
| Node-info resource/info/title/link strings and attributes | `nodeTextInfo()` copies the five nullable metadata strings. `nodeAttributes()` returns the POD attributes, font family and optional language. `copyText`/`copySource` and the Read methods expose counted content. |
| Change-info `pOperations` | `DocumentChangeSet.operations()` returns an immutable `NativeSequence<DocumentOperation>` with its own native reference. It reads the existing array in place and keeps it available after the original change-set view closes. |
| Link-info URI/title strings | `linkInfo()` copies nullable URI and title with the existing output-record string contract. `linkState()` only reads the flags. |
| Custom allocator callbacks and user data | Must outlive every document-derived native allocation and may run across native threads. Default Owned factories leave these NULL. |
| `Create`, `OpenFile`, `Deserialize`, `FragmentImportHtml`, `FragmentDeserialize` | `createDocument`, `openDocument`, `deserializeDocument`, `importDocumentFragmentHtml` and `deserializeDocumentFragment` use the corresponding Native Owned factory. `openDocument` preserves the input file byte limit. Custom allocators stay in the raw C APIs. |
| Retain/Release and ownership metadata | The managed package exposes independent views through `share`, with checked cleanup and private ownership policies. Raw objects cannot be promoted into the managed domain. |
| Whole node attributes | `setNodeAttributes()` replaces all 13 scalar fields, the font family and language. `setAttributesColumns()` retains its earlier behavior of clearing the font family. |
| Styled and extension nodes | `insertNodeWithAttributes()` submits the complete style, counted node text and optional binary extension payload in a single native transaction operation. `insertNode()` supplies default attributes. |
| Source-index constructor cancellation callback inside the descriptor | `createSourceIndex(maxWorkingBytes=0, shouldCancel=null)` calls the supplied closure on the constructing thread. A nonzero result cancels the build with `XUI_DOC_ERROR_CANCELLED`. A thrown language error cancels the build and is delivered after native cleanup. |

The machine map records every omitted function and pointer-bearing record field separately. The table is an interoperability boundary, not a promise that these native operations have been implemented as language conveniences.

`nodeAttributes()` reads the stored style on a transaction or snapshot.
Its font family and language are independent strings and remain usable after
closing the owner. The language tag is normalized to lowercase by the
Document model. A font family uses at most 63 UTF-8 bytes and cannot
contain an embedded NUL. An empty family is returned as an empty string.
An inherited language is returned as null. Use `setNodeAttributes()`
with the returned values to replace the whole style.

Source-index cancellation uses a stack callback context. The builder polls
before starting, periodically during indexing, and before publishing the
result. It discards a partial index on cancellation. The callback ends with
the call, and the snapshot can be used to retry. [Source-index example](../lib/bindings/xui/examples/document_source_index.xl)

The warmed query was measured on Windows and Linux with both strings
present. Each language call allocated twice for the owned text. The raw
C query borrowed the same strings and allocated zero times. The native
setter and styled insert use the document allocator and follow the existing
transaction failure rules. [Node attributes example](../lib/bindings/xui/examples/document_node_attributes.xl)

Owned export additions pass source-compiler JIT, a readable example, independent
dynamic/static artifacts on Windows/Linux and public C/C++ header checks. Their
index is `dev/release-materials/provenance/xui-document-exports-source-20261112.json`.
This does not mean every declared entry executes in that fixture or certify
the complete O2/VFS/embedded compiler and final IDE delivery. Earlier family
qualification remains explicitly historical in the machine map.

`tools/gen_xge3_document_bindings.py --check` verifies both declaration files
without writing source. Generation uses matching XUI typedef context as well
as the Document header, and reproduces owned output, exact U64 prefix and
descriptor-order contracts. It cannot silently restore the old CRT output
boundary.

The later full Windows r31 boundary gate executes all seven native producers
through language ownership, including range and table-matrix text/bytes and
the reference-valued range argument. It checks empty/reversed ranges, invalid
document identity/table extents, retained assignment on failure and 448
allocation budgets. JIT and independent hosted/static executables plus three
public header modes pass 11 commands. In 100 iterations of five text exports,
native C and xlang each make 600 allocations, 600 frees and no reallocations.
Text still adds its UTF-8 scan and language scope/status checks; stack/value
transfer and CPU throughput are not equated. The Windows receipt is
`xui-document-exports-boundaries-windows-20261113.json`; Linux and the remaining
Document interfaces remain separate qualification requirements.

Linux full r24 subsequently passes the same 11 boundary commands, all seven
producers, 448 language allocation budgets and real hosted/static ELF execution.
Native C and language text loops each make 600 allocations, 600 frees and no
reallocations there as well. The successor receipt is
`xui-document-exports-boundaries-linux-20261113.json`. Other Document operations,
rendering and the final delivery remain separate; the earlier Windows receipt
keeps its historical pending-platform note.

Direct declarations eliminate C forwarding wrappers. Resource controls, code leases, checked scopes, native RC operations and callbacks still cost work. Descriptor packs add stack initialization and assignments; NUL strings require scans/validation, while counted inputs retain exact byte lengths. Copy methods perform their documented native byte copy. POD output projection copies fields; it does not box pointer payloads. This package does not claim equality with raw C execution time.

The focused gate is `tools/check_foreign_xge3_document.py`: it emits exported compile-only probes for every method, callback reads/shares and every POD record, checks C11/C++11/C++17 headers and compiles at O0/O2. A separate actual script executes all seven resource families, notification share preservation, checked notification delivery, POD projections, scoped buffers, source/index ownership, prepare publication and unique transaction cleanup against a matched CPU-only native engine. GUI, filesystem and network operations are not executed by that gate.
