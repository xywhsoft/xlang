# Complete audio integration: current boundary

This is the active implementation ledger for the unfinished audio family, not
a completed language API or a release qualification. FontLoad is recorded
separately in [owned loading](xge3-async-loading.md).

## Existing language surface

Global init/unit/readiness, volume, listener and fallback-path operations are
joined by qualified AudioGroup, Sound, Music and AudioStream OOP declarations.
`createAudioGroup`, `loadSound`, `loadMusic` and `openAudioStream` create owned
resources; file factories accept an optional nullable group. Sound now uses a
stable native-owned record rather than the old in-place language allocation.
Playback, pause/resume/stop, loop, volume, fade, applicable position and state
queries call their declared native symbols directly. `audioUnit()` now maps to
checked native shutdown. Every resource has checked, creator-thread close.
`AudioClipInfo` and a private clip destruction policy are not proof of a complete
public clip API. Its thread/sequence contract, clip voices, remaining fallback
surface and owning SoundLoad requests remain unfinished.

## Native and compiler prerequisites

| Area | Confirmed source contract / remaining work |
| --- | --- |
| Immutable clip | Native decode/info/atomic retain/free and exact PCM span exist. The optional-shrink diagnostic bug and typed span were fixed in `4d0c686d` / `88a61edc`. A general cross-thread resource/sequence policy decision is pending; do not silently pick a provisional public policy. |
| Decode input | `xge_audio_clip_decode_t` packs data, byte count, output limit and synchronous cancellation callback/user fields. The [generic paired callback projection](foreign-packed-callbacks.md) now composes sealed `c_pack` with existing scoped callbacks and exact C signature proofs. The real CPU decoder is tested through a private Clip fixture; this does not publish a provisional AudioClip thread policy or complete playback ownership. |
| Engine lifetime | Native `29e6e561` adds audio-specific parent reservations, including in-flight constructors. AudioUnitChecked and global Unit reject live audio without teardown. These reservations are separate from graphics-context owners. |
| Group lifetime | Each voice now retains the private group backend. Free clears the public group handle; the last voice releases the actual mixer node and parent reservation. No language keep-alive field substitutes for native ownership. |
| Sound/Music/Stream | Stable owned native records, checked final release and direct OOP methods are qualified through Windows/Linux JIT and dynamic/static AOT. Public layouts remain unchanged. Shared scalar access proves types/offsets and avoids incompatible struct lvalues. Physical device and final module-global qualification remain separate. |
| Allocation/path diagnostics | Shared allocation callbacks isolate recovered OOM, while typed paths stop fatal memory failures. A further resident-engine test exposed a miniaudio connector failure retaining a cache node until engine shutdown; native `84ce4b4f` returns the acquired node/base before failure. Every tested language prefix now balances while the engine stays alive. |
| Sound references | `xgeSoundAddRef` previously revived zero/negative counts and overflowed at INT_MAX. Fixed by native `5d138435`; old Windows/Linux failures and 80 host/static O0/O2 boundary/PCM rows are pinned separately. No engine/device was initialized. |
| SoundLoad | Native SOUND requests are synchronous on the caller, as FONT requests are. Reuse the owning request graph/code/callback/result protocol after the sound resource parent/storage contract exists. |
| Code residency | The general module-global resource/code-lifetime gate remains false. Explicitly disposing an unclaimed result is a valid positive path, and is not evidence that automatic module-global shutdown is solved. |

The native foundation is now reproduced and qualified with real no-device PCM
mixing, stable file Sound/Music/Stream records, close/retry, wrong-thread checks,
allocation budgets, ordinary font/async regressions and independent admission
domains. There are 464 checks across Windows/Linux O0/O2 and matched native
host/static builds. Private no-device aggregate setup is not a claim of physical
device playback or complete language-level ownership/module acceptance.

## Acceptance order

1. Generic synchronous callback-in-descriptor projection is qualified: Windows
   r17/Linux r9 complete O2/TinyCC/XRT/VFS builds, 115 language/header/regression
   checks and 349 damaged IR rejections. Nullable callback, reentry, throw/error
   delivery, return tail, invalid source and the real decoder are covered.
   Ordinary code still calls the native decoder once. The paired callback fields
   reuse the existing invocation ABI; no per-audio forwarding method is added.
2. Native parent/record/error foundation is qualified in `29e6e561`. Real CPU
   mixing remains distinct from physical device and language/module acceptance.
3. Declare complete clip/group/Sound/Music/stream and fallback OOP APIs. Preserve
   exact byte/frame/sample and millisecond units, bounded output, independent
   cursors, sample retention, error/ordinary-state distinctions and native limits.
4. Complete SoundLoad using the same request protocol, then qualify matched
   Windows/Linux JIT, dynamic/static AOT, real modules, OOM, parent close/retry,
   callbacks, cleanup, strict C/C++ headers and generated calls.
5. Measure copies, cold allocations, scans, calls, synchronization and lifetime
   bookkeeping against the same native workload. Add readable examples and
   include the complete matched SDK/compiler/IDE/help set in final delivery.

Native prerequisites are recorded in
`dev/release-materials/provenance/xge-audio-reference-preflight-20261111.json`.
The generic decoder callback qualification is separately indexed in
`dev/release-materials/provenance/xge-packed-callbacks-20261111.json`.
The native parent/storage/error foundation is indexed in
`dev/release-materials/provenance/xge-audio-ownership-20261111.json`; its language
source candidate hashes are recorded explicitly as unqualified/uncommitted.
Formal release installation and full audio integration remain pending.

## Ordinary audio resources: language qualification

`xge-audio-resources-20261111.json` indexes 70 Windows/Linux language/header/
example rows using the complete Windows r17/Linux r9 O2/TinyCC/XRT/VFS compiler.
JIT, dynamic AOT and static AOT exercise actual PCM mixing, all ordinary resource
methods, typed bound calls, nullable groups, group-close-before-voice, checked
Busy/retry, closed aliases, missing/NUL paths and four 64-budget allocation
sweeps. A live audio engine is retained throughout the sweeps. Creator-thread
checks reject eight language operations on workers. Four separately owned test
views exercise final worker release: no failure-injection allocation, native
resource preserved until the creator drains the real runtime retirement queue.

Positive playback uses explicitly private SDK variants: their production
aggregate gets only setup/mix/inspection exports for a real miniaudio no-device
engine. Playback and destruction functions are unmodified. These images are
not shipped. Production host/static SDKs are separately covered by 148 native
lifetime/reference/PCM rows, 8 expected old-code control rows and strict
C11/C++11/C++17 generated headers/linkage. The public example builds against
those production SDKs; actual output-device listening remains manual. No
window, audio device or network tests run in automated qualification.

Ordinary methods generate one direct native function call, with existing
checked error scopes, creator-thread admission, view/code activity and lifetime
bookkeeping around it. File-path `c_cstr` validation still checks NUL boundaries.
There is no per-method C forwarding layer, dynamic operation dispatch or extra
PCM-array copy. That does not make the complete language call free of cost.
One AudioGroup retains 3 native allocations/8,776 bytes versus 5 allocations/
9,304 bytes through xlang: 2 extra allocations/528 bytes for the managed view
and code/lifetime ownership. These measurements agree across the six JIT/AOT
executions, for this exact workload and cohort.

The public immutable Clip/thread/PCM-sequence contract, clip voices, complete
fallback surface, SoundLoad, automatic module-global code residency and final
current-mainline SDK/compiler/IDE/help delivery remain unfinished. The private
setup fixture and explicitly drained worker references do not replace them.
