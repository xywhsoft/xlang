# Default Document model bindings

The authoritative machine map is [xge3-xui3-document-api-map.json](xge3-xui3-document-api-map.json). It pins `xui_document.h`, all 218 functions, 26 pointer-free C records and 255 language constants. The allocator marker is compiler/provider ABI metadata. The map distinguishes declarations from compiled or executed paths. The default package declares 159 native business entries, seven resource `close` controls and callback-view methods. No per-method C forwarding functions are added.

The classes are `Document`, `DocumentSnapshot`, `DocumentChangeSet`, `DocumentPrepare`, `DocumentFragment`, `DocumentSourceIndex` and `DocumentTransaction`. The first six share actual native references; `share()` calls the native status/output factory once and reserves its language owner before that call. The transaction has one unique owner. `commit()` and `abort()` end its writer but do not consume its shell; `close()` releases that owner. Default-provenance Owned factories are required. Raw custom-allocator objects cannot be promoted to managed roots.

`createDocument()` packs ordinary settings into one native stack descriptor, leaving custom allocator callbacks NULL. `openDocument` and `deserializeDocument` use their native Owned variants. To preserve native parameter-group order, the latter exposes configuration arguments before its final named `data` argument. `prepareStreamSource(null, utf8="...")` starts a candidate; subsequent calls accept the previous prepare. Run may execute on a worker according to the native contract; publication and live Document/Transaction operations belong to their creator thread. The native immutable snapshot/index APIs support their documented native thread contracts. The current safe ForeignView declarations still enforce their existing creator-thread admission; this binding does not claim cross-thread language-handle access.

`deserializeDocumentFragment(bytes)` accepts the binary `XUIFRAG1` envelope,
including its zero-containing eight-byte metadata block and JSON payload.
Its C `const char*` parameter is a byte carrier. HTML import and the pure-JSON
Document deserializer remain text APIs. Native fragment serialization buffers
still require `xuiDocumentFreeBuffer`; the binding does not adopt them with
`xrtFree`.

`subscribe` installs the real Native Owned callback. Its `CallbackDocument` and `CallbackDocumentChangeSet` are valid through that callback's tail. Pure reads and POD change info are available there; saving a long-lived handle requires explicit `share()`. A nonzero callback result is a notification failure. Committed content is already published; `notificationFailureStatus()` preserves its original int and `checkNotifications(clear)` delivers the checked failure. No implicit retry or undo occurs.

`nodeInfo` returns these POD values in order: kind, id, parent id, child count, text byte length, source start/end, source-exact flag, syntax start/end, source-segment count, extension-payload byte length, extension version and required flag. `DocumentChangeSet.info()` returns flags, before/after revision, origin, input generation, operation count, undo flag and domain. `linkState()` returns linked/mixed flags. Each is one real native call on a zeroed, size-initialized stack output. Failure prevents all field projection. The native pointer-bearing records are never exposed as plain language values.

`copyText`, `copySource`, `copyExtensionPayload`, and `Read` methods write directly into initialized `Buffer` storage. The buffer's logical byte length is the capacity supplied to native code and stays unchanged on success or failure. Text Copy methods require the reported byte length plus one terminator byte. Binary Read and extension methods follow their exact native byte limits. Explicit decoding or `Buffer.text(size)` turns copied text into an independent language string. Native status and UTF-8 validation remain authoritative.

Some native capabilities remain explicit C interoperability:

| Native functions/fields | Boundary and available safe path |
|---|---|
| `SnapshotAnalyzeMarkdownConversion`, `SnapshotFind`, `SnapshotFindEx` | Caller output arrays plus capacity. A single result record would be unsafe when capacity exceeds one; no writable typed-array output declaration is fabricated. |
| `PrepareSource`, `PrepareContinueSource` | Arrays of size-prefixed patches with per-element counted text. Existing column projection cannot initialize each size and pair each text with its byte length. `prepareStreamSource` supports safe append/chunk input, with different semantics. |
| `FragmentSerialize`, `FragmentExportHtml`, `SnapshotCopyRange`, `SnapshotCopyTableMatrix`, `SnapshotCopyPlainText`, `DocumentSerialize`, `DocumentExportHtml` | Direct owned bytes outputs; HTML/plain/range/matrix also have UTF-8 string outputs. The actual resident-XRT allocation is transferred with its explicit byte count. The matched SDK allocator marker is required. Native `DocumentFreeBuffer` stays internal to language cleanup. |
| Node-info resource/info/title/link strings; nested attributes language | Borrowed pointers require explicit output-record text conversion, which this contract does not declare. No pointer is disguised as POD. Copy/Read methods expose node text/source safely; pointer metadata remains C-only. |
| Change-info `pOperations` | A borrowed array behind a record field. `operationCount` is available, but no raw pointer is exposed and no zero-copy sequence is invented. |
| Link-info URI/title strings | Only linked/mixed POD state is projected; borrowed string fields stay C-only. |
| Custom allocator callbacks and user data | Must outlive every document-derived native allocation and may run across native threads. Default Owned factories leave these NULL. |
| Raw `Create`/import/Deserialize and Retain/Release/ownership metadata | The safe package uses actual Owned factories, independent `share`, checked controls and private policies. Legacy raw promotion is forbidden. |
| `setAttributesColumns` fixed `sFontFamily[64]` | All 13 scalar fields and nullable language are packed; family is zeroed as part of whole-value replacement. Use range `setTextStyle` with `DocumentTextStyle` for fixed font-family bytes. |
| `insertNode` attributes/extension pointer | The descriptor uses default-zero attributes and copies its counted text/string fields. Subsequent style and `setExtensionPayload(bytes)` methods cover safe changes. Opaque payload pointers are not exposed. |
| Source-index constructor cancellation callback inside the descriptor | The ordinary constructor configures its working-memory limit. A nested raw callback/userdata pair is not synthesized. |

The machine map records every omitted function and pointer-bearing record field separately. The table is an interoperability boundary, not a promise that these native operations have been implemented as language conveniences.

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

Direct declarations eliminate C forwarding wrappers. Resource controls, code leases, checked scopes, native RC operations and callbacks still cost work. Descriptor packs add stack initialization and assignments; NUL strings require scans/validation, while counted inputs retain exact byte lengths. Copy methods perform their documented native byte copy. POD output projection copies fields; it does not box pointer payloads. This package does not claim equality with raw C execution time.

The focused gate is `tools/check_foreign_xge3_document.py`: it emits exported compile-only probes for every method, callback reads/shares and every POD record, checks C11/C++11/C++17 headers and compiles at O0/O2. A separate actual script executes all seven resource families, notification share preservation, checked notification delivery, POD projections, scoped buffers, source/index ownership, prepare publication and unique transaction cleanup against a matched CPU-only native engine. GUI, filesystem and network operations are not executed by that gate.
