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

The native editor still owns its internal document. Its pointer must not be
turned into a second unique root; this batch does not publish that unsafe
conversion. `CallbackCodeDocument` is an invocation-period view with a counted
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
normalization and history rules remain in effect. Worker batches, transferable
payloads, internal-document views and explicit partial-commit outcomes still
need their own safe contracts and acceptance.

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
