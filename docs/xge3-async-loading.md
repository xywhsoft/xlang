# Owned image and texture loading

`xge.createImageLoad()` and `xge.createTextureLoad()` return distinct owning
request types. Both expose `share`, `info`, `start`, `poll`, `cancel`, `reset`,
`throwIfFailed`, `onComplete`, `take` and `close`. `LoadInfo` is the exact native
record, including status/result, progress, result presence and threading state.

Register completion before `start`: resource providers can finish synchronously.
File-backed image/texture decoding can use a native worker; GPU submission and
completion run when the creator polls. These are not Future methods and do not
silently allocate language coroutines or promise that providers are nonblocking.
The existing native implementation uses a system thread per active file request.
Font/Sound requests remain in scope with their later resource families.

See [the small image example](../lib/bindings/xge/examples/async_image.xl). It uses
one TGA pixel so no asset installation is needed. Providers are cleared in
`finally`: a registered provider owns its callback code until removed.

## Result, error and cancellation rules

Requests own their URI, private result, stored diagnostic, callback and independent
code owner. No caller-owned output address is saved. `take` succeeds once; reset
discards any unclaimed result/error and retains the completion registration.
Reset/start reentry and unfinished-worker reset return checked Busy.

An accepted operation may already be Failed when `start` returns. `info` and
`poll` preserve ordinary IO results; `throwIfFailed` reports the stored error.
A source callback failure is instead checked immediately, retained as the failed
result and never followed by completion. It remains observable even if the source
also requested cancellation. Completion failures are checked at Start/Poll but do
not rewrite an already Ready result. Completion is attempted at most once.

Cancellation suppresses notification and result publication; it does not interrupt
OS IO or decoding. Final release requests cancellation and defers physical cleanup
until the real XRT worker has returned. It never infers return from an early worker
flag or frees URI/result/code storage while the worker can still use it.

Image take moves pixels into empty in-place Image storage. Texture take creates
an independent native owned Texture record and adopts the result; failed transfer
keeps the request's result. Neither transfer adds a pixel copy. Pending texture
work and its unclaimed result hold the graphics lifetime, so checked engine
shutdown reports Busy. Taking the texture transfers that obligation to Texture.

## Native and language mapping

Ordinary methods directly declare the native `xgeAsyncLoad*Owned`,
`xgeAsyncImageStartOwned`/`TakeOwned` and Texture counterparts. The two cold
creation adapters only admit the module's code lease; they do not decode, poll,
dispatch operations or wrap every method. Common native input symbols use the
[generic input-policy rule](foreign-owned-policy-inputs.md); the two nominal
types and producer/rollback policies remain separate.

Native Count/Trace, preparation, quiescence, finalization and completion describe
the real shared references, callback edges, errors and deferred worker cleanup.
The code owner exists even on an empty request, without a callback. Callback
snapshots hold activity through self-removal, result transfer and complete native
return. Only the creator can operate or physically finalize a request; cross-thread
language release uses the existing runtime retirement queue. Creator death and
OS thread-ID reuse remain part of the broader thread-lifetime audit.

| Native capability | Language entry or lifecycle replacement |
| --- | --- |
| AsyncRequestInit / Free | createImageLoad / createTextureLoad; close and automatic owning cleanup |
| AsyncRequestCancel / AsyncPoll | cancel / poll on the typed request |
| AsyncImageLoad / AsyncTextureLoad | matching request.start, with its private native result |
| Image/TextureCreateOwned / ShareOwned | typed create and share, with separate nominal policies |
| LoadInfoGet / ResetOwned / ThrowIfFailed | info / reset / throwIfFailed |
| LoadCallbackSetOwned | onComplete with an owned exact callback, or null to clear |
| ImageTakeOwned / TextureTakeOwned | typed take factory and rollback contract |
| ReleaseChecked / ReleaseOwnerChecked | rollback or consuming checked destruction, including Deferred |
| Ownership / Adapter / Preparation / Quiescence / Completion / Finalization / ReadReady | native ownership domain used by the existing runtime graph protocols |
| AsyncThreadingSet / Get | existing asyncThreadingSet / asyncThreadingGet declarations |
| AsyncFontLoad / AsyncSoundLoad | pending owning resource integration; not replaced by an unsafe target pointer |

Mutable raw request targets/payload/user pointers remain the native C protocol.
They are not advertised as safe language fields; their business functions use
the owning request/result/callback model above.

## Acceptance and measured costs

The matched native r22 snapshot has 136 native O0/O2 checks across Windows/Linux.
It uses the async source at `21379e49` plus the relative-path fix in `6bc967f6`,
without concurrent XUI changes. The checks cover
sync/file-worker loading, private results, cancellation, source/completion errors,
callback removal/last-reference release/reentry, wrong-thread refusal, OOM
prefixes, failed texture transfer rollback and deferred physical worker return.
Relative file tests place the executable and working directories apart: a failed
application-directory probe must not poison a successful fallback. Real missing
files remain checked failures. The old snapshot fails this control on both OSes;
using only absolute paths would have hidden the defect and was not the fix.

The Windows full O2 compiler with matching dynamic/static SDKs passes 45 image/
texture/resource checks, including JIT, standalone static AOT and PE import
inspection. The generated fixture header and actual policy/domain header pass six
strict C11/C++11/C++17 checks and a C++ link/run.
Each platform's six actual module checks use real native shared references: empty requests and
threaded self-capturing requests survive module Close, callbacks still execute,
then stopping publishers permits checked collection and native image unload.
The full Linux O2/TCC/XRT/VFS candidate passes 36 corresponding language checks,
six module checks and the same eight header/link checks. These are CPU/local-file
qualifications; they do not replace GPU, audio or hardware performance acceptance.

A warmed native 1,000-iteration Info/Poll loop adds zero heap allocations. The
Windows and Linux language empty ImageLoad creation has 4 live allocations / 840 requested
heap bytes after warmup; leaving its scope returns to baseline. This is a cold
ownership-admission cost, not a claim that the entire operation has no overhead.
The corresponding process-resident C empty request uses 1 allocation / 248 bytes
on both platforms, without a module code owner. The language difference
is 3 allocations / 592 requested bytes; this comparison does not equate that C
case to a JIT module with an independent code lease.
Decoder allocations/copies, worker creation, callback bookkeeping, native Texture
record allocation and language lifetime bookkeeping remain explicit costs.
`start` also checks the input byte span for embedded NUL before passing its C
string; native XGE copies its URI normally. Taking a result creates the language
owning view/storage metadata without copying pixels. Ordinary calls retain the
existing checked admission, error, reference and ownership bookkeeping around
the direct native call, unlike an unchecked C pointer call.

All results use isolated matched candidates. Full-source release/IDE delivery,
Font/Sound requests and the remaining XGE/XUI/Document ecosystem are unfinished.
Exact input, package, build, failure-control and acceptance hashes are recorded in
`dev/release-materials/provenance/xge-async-loading-20261110.json`.
