# Owned image, texture and font loading

`xge.createImageLoad()`, `xge.createTextureLoad()` and `xge.createFontLoad()` return
distinct owning request types. They expose `share`, `info`, `start`, `poll`, `cancel`, `reset`,
`throwIfFailed`, `onComplete`, `take` and `close`. `LoadInfo` is the exact native
record, including status/result, progress, result presence and threading state.

Register completion before `start`: resource providers can finish synchronously.
File-backed image/texture decoding can use a native worker; GPU submission and
completion run when the creator polls. These are not Future methods and do not
silently allocate language coroutines or promise that providers are nonblocking.
The existing native implementation uses a system thread per active file request.
Font requests preserve the native synchronous caller-thread behavior, including
file paths when threading is enabled. `start(path, size)` can invoke completion
before returning. This is an owning request API, not a promise of nonblocking
font IO. Sound requests remain in scope with the audio resource family.

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

Font requests allocate a private stable Font record on demand. `take()` adopts
and detaches that record without moving it, copying its contents or performing
a native allocation. The Font survives request close, with its own graphics
lifetime obligation. Reset/close destroys an unclaimed font; admission allocation
failure leaves the request Pending and releases its temporary graphics hold.
The 96-byte native Font is not embedded into the common target union: empty
ImageLoad/TextureLoad request storage retains its prior size. Language result
ownership still needs its normal view metadata and code-lifetime bookkeeping.

## Native and language mapping

Ordinary methods directly declare the native `xgeAsyncLoad*Owned`,
`xgeAsyncImageStartOwned`/`TakeOwned` and Texture/Font counterparts. The three cold
creation adapters only admit the module's code lease; they do not decode, poll,
dispatch operations or wrap every method. Common native input symbols use the
[generic input-policy rule](foreign-owned-policy-inputs.md); the three nominal
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
| AsyncRequestInit / Free | createImageLoad / createTextureLoad / createFontLoad; close and automatic owning cleanup |
| AsyncRequestCancel / AsyncPoll | cancel / poll on the typed request |
| AsyncImageLoad / AsyncTextureLoad / AsyncFontLoad | matching request.start, with its private native result |
| Image/Texture/FontCreateOwned / ShareOwned | typed create and share, with separate nominal policies |
| LoadInfoGet / ResetOwned / ThrowIfFailed | info / reset / throwIfFailed |
| LoadCallbackSetOwned | onComplete with an owned exact callback, or null to clear |
| ImageTakeOwned / TextureTakeOwned / FontTakeOwned | typed take factory and rollback contract |
| ReleaseChecked / ReleaseOwnerChecked | rollback or consuming checked destruction, including Deferred |
| Ownership / Adapter / Preparation / Quiescence / Completion / Finalization / ReadReady | native ownership domain used by the existing runtime graph protocols |
| AsyncThreadingSet / Get | existing asyncThreadingSet / asyncThreadingGet declarations |
| AsyncSoundLoad | pending owning audio resource integration; not replaced by an unsafe target pointer |

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
Sound requests and the remaining XGE/XUI/Document ecosystem are unfinished.
Exact input, package, build, failure-control and acceptance hashes are recorded in
`dev/release-materials/provenance/xge-async-loading-20261110.json`.

## Font loading qualification

The Font extension uses native commit `dd6585ab` and matched frozen r44 SDKs.
The three ordinary native font operations map to `createFontLoad`, `start`, and
`take`; the create code-lease adapter is the only additional cold function.
The shared request domain, exact callback and factory protocols are reused.
See [the small font example](../lib/bindings/xge/examples/font_loading.xl), which
uses the generated valid font already shipped with the examples.

Windows/Linux native host/static O0/O2 tests pass 232 rows, including the existing
Image/Texture gate. Language JIT, dynamic AOT and standalone static AOT test normal
loading, synchronous callback self-clear, independent results, reuse, failures
and 80 allocation budgets. Actual module Close tests cover an empty request and
a ready request with a captured callback, followed by native reset, checked
retirement and native image unload. Strict C11/C++11/C++17 headers and actual C++
policy/domain linkage are checked against both matched SDK variants.

These module tests explicitly discard the private ready Font while the external
publisher still owns the request. Before that reset, checked Unit returns Busy as
required. The earlier failing no-reset control remains in the evidence: the
general module-global resource/code-lifetime shutdown gate is still **not passed**.
Explicit reset is not counted as proof of automatic module-global cleanup.
Formal release/IDE delivery, audio and the rest of the full coverage plan remain
unfinished; this batch does not relax those gates.

After warmup on both tested x64 ABIs, empty native Font/Image/Texture requests all
retain 1 XRT allocation / 248 requested bytes. Language empty FontLoad retains
4 allocations / 840 bytes. Native Font `take` succeeds under a zero-allocation
budget, without copying the 96-byte record or font data; its language owning
result adds 2 retained allocations / 528 bytes. These are requested live XRT
allocations, not allocator capacity, complete system heap cost or timing results.
The C baseline is process-resident without a JIT module lease; status/ownership
checks, code lifetime, URI NUL validation/copy, decode work and callback activity
remain costs. Exact pins are in
`dev/release-materials/provenance/xge-font-loading-20261111.json`.
