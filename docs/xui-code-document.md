# Owned CodeDocument

`xui.createCodeDocument()` creates an independent, uniquely owned native text
document. It needs no window, Context, graphics backend or dynamic library
loader in application code. The direct declarations cover text, UTF-8 byte
positions, grouped history, dirty state, file I/O and an owned change callback.
See [the short console lesson](../lib/bindings/xui/examples/code_document.xl).

| Language operations | Native contract |
| --- | --- |
| createCodeDocument / close | Create / checked unique-owner destruction |
| setText / getText | counted input / checked counted independent snapshot |
| getLength / getLineCount / versions | native integer values |
| getDirty / setDirty | native dirty state |
| beginEdit / endEdit / undo / redo / canUndo / canRedo | native grouped history |
| insert / delete / replace | UTF-8 byte offsets; C-string inputs reject NUL |
| getLineRange / offsetToLineColumn / lineColumnToOffset | checked output values from one native call |
| getByte / copyRange / getLastEditRange | checked byte, writable Buffer copy, CodeRange |
| getLastError | nullable borrowed C-string snapshot |
| loadTextFile / saveTextFile | local XRT file API, explicit charset |
| setChangeCallback | typed native owned callback slot, nullable clear |

`CodeEdit.document()` and `CodeEdit.selection()` return observed views over the
models used by the widget. Each view retains the managed Context. Closing the
editor invalidates both views immediately. Closing a view detaches its
observation, invalidates aliases to that view, and leaves the editor alive.
The editor can issue a fresh view once detachment completes. If the view is
closed from a native callback, detachment can wait until that callback returns.

Selection commands that take a document require the document from the same
editor. Offsets are UTF-8 byte positions. The native selection model clamps
grapheme boundaries and invalidates the editor's rendering after a mutation.
Document edits update the live editor. For change notifications that outlive
a particular document view, register on `CodeEdit.setDocumentChangeCallback`.
See [the editor models lesson](../lib/bindings/xui/examples/code_edit_models.xl).

`CallbackCodeDocument` is an invocation-period view with a counted
`getText()` snapshot. Saved views reject later access. A callback also receives
the native `CodeRange` value and version. Keep the copied text or capture the
actual owned root for longer-lived work.

`getText()` calls `xuiCodeDocumentGetTextView` once. It checks status and copies
the explicit bytes before the real native scope ends; embedded NUL is retained.
The required string result distinguishes a present empty string from an invalid
null result. Copy/OOM failure preserves the old assignment. No `strlen` or
legacy pointer-only fallback is used. Methods returning nullable text preserve
null versus empty according to the generic declaration contract.

Callback registration acquires the candidate before publishing it and preserves
the previous slot on admission failure. Dispatch snapshots and holds the chosen
callback owner through replacement, clear or close reentry. The native ownership
trace includes the installed edge, including inside a CodeEdit-owned document.
There is no extra callback registry, fake reference count or forwarding method
per API. Calls are creator-thread-affine; close is checked and idempotent at the
language level. The native owner can be consumed while a scope is active, but
physical free waits for its final scope. Failures preserve the first diagnostic.

An edit may already be committed before its notification raises an error; an
exception does not mean rollback. `saveTextFile` permits autosave inside a change
callback by pinning lifetime without taking a second writer role. It uses the
ordinary local file API, not a virtual filesystem provider. Native text
normalization and history rules remain in effect.

## Multi-document edits

`prepareCodeDocumentBatch(documents, versions, before, after)` prepares
several document updates on their creator thread. Use
`prepareEditorDocumentBatch` for the views obtained from `CodeEdit.document()`.
Each row pairs a document with its change version, exact current text, and new
LF UTF-8 text. The default `size=-1` requires equally long arrays. An explicit
nonnegative size selects a prefix of their common length. The following optional
`budget` argument limits native staging bytes. Zero uses the native default
of 128 MiB. The native limits are 512 documents, 8 MiB per body, 64 MiB total
per before/after group and a maximum staging budget of 256 MiB. Embedded NUL
and carriage returns are rejected.

`commit()` checks every document again and publishes the complete batch
before its change notifications. Each edited document receives a single undo
step. If an observer or cleanup throws, `wasCommitted()` tells the caller in
`catch` whether publication already occurred. A commit takes one attempt.
Use `close()` in `finally` to release the prepared data and detach its watches.
Closing a document before commit invalidates the corresponding batch row.

`commitWithNativeHooks()` accepts a borrowed C hook table for host model
publication. Its guard can refuse before publication. The `published` hook
must be infallible and run before the document change observers.

The binding allocates temporary descriptors and lifetime scopes proportional
to document count. It passes the original counted strings into the native
prepare call. XUI validates and stages replacement bodies, line indexes and
undo data. See [the batch editing lesson](../lib/bindings/xui/examples/code_document_batch.xl).

[Batch validation](../dev/release-materials/provenance/xui-code-document-batch-20261123.json)

### Prepare edits on a worker

1. On the document thread, read the texts and change versions. Call
   `captureCodeDocumentBatch()` for independent documents or
   `captureEditorDocumentBatch()` for documents owned by CodeEdit.
2. Call `batch.describe()` and send the snapshot and counted texts to the
   worker. The immutable snapshot owns scalar metadata in one native
   allocation and remains valid after its source documents and batch close.
   `source(index)` uses zero-based indexing.
3. Call `snapshot.stage(before, after)` on the worker. Supply one before and
   after string per captured document in the original order. Keep both
   arrays unchanged until staging returns. The optional budget defaults to
   the capture budget and can lower that ceiling. An
   optional cancellation callback executes synchronously on the staging
   thread. A nonzero result cancels staging. Any callback error reaches the
   caller after partial data is released.
4. Send the owned payload back to the document thread. `batch.accept(payload)`
   checks the capture identity, versions and original text. It moves the
   staged storage into the batch. `payload.wasTaken()` remains queryable
   even if cleanup returns an error. Release the payload with `close()`.
5. Call `batch.commit()` on the document thread to publish and notify observers.

Staging projects the two string arrays into temporary counted descriptors.
It retains the original strings through the native call. Accept compares the
original text against the current document and moves prepared storage without
copying the staged bodies or indexes. See the
[worker example](../lib/bindings/xui/examples/code_document_batch_worker.xl).
The console example waits on the worker for brevity. In a GUI, poll for the
result or post it to the document thread.

[Worker batch validation](../dev/release-materials/provenance/xui-code-document-batch-worker-20261123.json)

## Independent document validation

The exact O2 native SDK and unchanged full compiler pass 48 Windows/Linux
language commands: JIT, independent dynamic/static AOT, C11/C++11/C++17 public
headers, callback replacement/clear/error/close/expiry, OOM, wrong thread,
NUL/UTF-8 file round-trip, callback autosave and history. Native O0/O2 callback
and editor-text regression contributes 48 further commands. Test implementation
and evidence are separate from the readable lesson.

Prepared-payload 1000-edit runs with an installed owned callback measure the
same 2000 allocations, 2000 frees and 1000 reallocations in language and
handwritten C, in all three execution modes on both platforms. The ordinary
model/history work accounts for these operations. Constructing the payload
inside the language loop adds its ordinary string allocations and is not part
of this prepared-payload comparison. Text snapshots allocate/copy independent
strings. Real native scopes, XRT mutation gates, code leases, epoch checks and
callback Hold/Drop remain costs; matching heap counts does not prove CPU or
cold-path equivalence. No network/security tests or shipping installation are
part of this acceptance.

The same compiler and native static archive build the Slider lesson before and
after adding these declarations. Code-section sizes add zero bytes on both
platforms; the file delta is zero on Windows and +32 bytes on Linux (12
build/check commands). Linux retains the same total section payload and
section-count/header sizes; reordered sections add 32 bytes of file padding.
Its code bytes are not identical after relocation. This is a selected
unused-interface comparison, not a general size guarantee. The native
source commit is `c083fa3f`; the final packages change only README/catalogue
bytes after the qualified language run. The receipt
`dev/release-materials/provenance/xui-code-document-20261114.json` records the
input hashes, qualified-code reuse and remaining work.
