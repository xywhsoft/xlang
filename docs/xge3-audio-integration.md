# Complete audio integration: current boundary

This is the active implementation ledger for the unfinished audio family, not
a completed language API or a release qualification. FontLoad is recorded
separately in [owned loading](xge3-async-loading.md).

## Existing language surface

`functions.xl` declares global audio init/unit/readiness, volume, listener and
fallback-path operations. `audio_resources.xl` currently exposes only Sound
load/play/stop/volume/close, using an in-place raw Sound record. `AudioClipInfo`
is declared and the private runtime has a clip destruction policy; neither is
proof that the clip API, decoding callbacks, sample sequence or playback family
has been safely exposed. Music, streams, groups, clip voices, other Sound methods
and owning SoundLoad requests remain unfinished.

## Native and compiler prerequisites

| Area | Confirmed source contract / remaining work |
| --- | --- |
| Immutable clip | Native decode/info/atomic retain/free and exact PCM span exist. The optional-shrink diagnostic bug and typed span were fixed in `4d0c686d` / `88a61edc`. A general cross-thread resource/sequence policy decision is pending; do not silently pick a provisional public policy. |
| Decode input | `xge_audio_clip_decode_t` packs data, byte count, output limit and synchronous cancellation callback/user fields. The [generic paired callback projection](foreign-packed-callbacks.md) now composes sealed `c_pack` with existing scoped callbacks and exact C signature proofs. The real CPU decoder is tested through a private Clip fixture; this does not publish a provisional AudioClip thread policy or complete playback ownership. |
| Engine lifetime | Legacy `xgeAudioUnit` unconditionally uninitializes the engine. Core Unit's graphics admission does not describe live audio resources. A checked close/real parent lifetime must precede exposing more owning audio objects. The existing raw functions do not establish that contract. |
| Group lifetime | `xge_sound_clip_desc_t.group` is explicitly borrowed and must outlive the sound. `xge_audio_group_t` currently has volume/backend only, with no owned reference protocol. Safe groups and child voices need actual native owning references and defined release behavior. |
| Sound/Music/Stream | Raw records share the ref/type/flags/backend prefix. Their methods call the same native audio implementation. Add native owned storage/parent contracts, then direct OOP declarations, preserving file/decode/stream, trim, volume/loop/fade/position and ordinary playback states. |
| Sound references | `xgeSoundAddRef` previously revived zero/negative counts and overflowed at INT_MAX. Fixed by native `5d138435`; old Windows/Linux failures and 80 host/static O0/O2 boundary/PCM rows are pinned separately. No engine/device was initialized. |
| SoundLoad | Native SOUND requests are synchronous on the caller, as FONT requests are. Reuse the owning request graph/code/callback/result protocol after the sound resource parent/storage contract exists. |
| Code residency | The general module-global resource/code-lifetime gate remains false. Explicitly disposing an unclaimed result is a valid positive path, and is not evidence that automatic module-global shutdown is solved. |

These are contract gaps to implement, not a reason to substitute unsafe raw
handles or language-only keep-alive fields for native ownership. Current native
group/engine source behavior has been inspected; actual owning playback and
shutdown tests remain to be written. The reference overflow is independently
reproduced and fixed; other lifecycle findings are not being presented as
already qualified runtime reproductions.

## Acceptance order

1. Generic synchronous callback-in-descriptor projection is qualified: Windows
   r17/Linux r9 complete O2/TinyCC/XRT/VFS builds, 115 language/header/regression
   checks and 349 damaged IR rejections. Nullable callback, reentry, throw/error
   delivery, return tail, invalid source and the real decoder are covered.
   Ordinary code still calls the native decoder once. The paired callback fields
   reuse the existing invocation ABI; no per-audio forwarding method is added.
2. Establish native engine/group/voice/owned record lifetimes and checked retryable
   shutdown. Use a device-independent real mixing setup where possible; retain
   separate actual device acceptance rather than counting CPU decode as playback.
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
Formal release installation and full audio integration remain pending.
