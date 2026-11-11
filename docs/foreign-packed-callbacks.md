# Synchronous callbacks inside native descriptors

This extends the existing immutable `c_pack` schema. It composes with scoped
`c_sync_callback`; there is no audio-specific adapter, runtime dispatch table or
new callback lifetime. The native function is called directly once.

For a native declaration such as:

```c
typedef struct NativeDecode {
    const void* data;
    size_t bytes;
    int (*cancelled)(void* user);
    void* user;
} NativeDecode;
int nativeDecode(const NativeDecode* input);
```

the paired fields share one logical callback source:

```xl
typedef CancelPoll=func() as c_int
extern "nativeDecode" @header("native_decode.h") @no_raise
@c_span(0,"const void*","size_t")
@c_sync_callback(1,-1,-19,"first")
@c_pack(0,"NativeDecode",0,2)
@c_pack_field(0,"data",0,"span_data")
@c_pack_field(0,"bytes",0,"span_count")
@c_pack_field(0,"cancelled",1,"callback")
@c_pack_field(0,"user",1,"callback_user")
func decode(bytes input,CancelPoll? cancelled) as c_int
```

The authoritative header proves every field type. `callback` requires the exact
generated function-pointer type, including userdata position; `callback_user`
requires `void*`. Missing or duplicate halves, mismatched logical sources,
retained callbacks and incompatible effects are rejected. Independent XLIR
verification rejects damaged metadata before either C or header output.

## Lifetime and failure rules

The descriptor and callback invocation contexts are stack storage. The existing
closure/code/resource admission remains active through the synchronous native
call. Native code must wait for all callback attempts and must not retain either
pointer. An invocation on another thread is rejected before entering the language
closure. A callback-period view cannot be used after its invocation expires.
Arbitrary C use of userdata after the enclosing call has returned remains a
native use-after-return contract violation; this feature cannot make it safe.

A nullable callback initializes both fields to NULL. Reentrant synchronous calls
have independent scopes. A failure is latched **per logical callback parameter**:
later attempts on that parameter return its failure sentinel without re-entering
the closure. Other callback parameters retain the native implementation's call
order. The earliest observed callback error is preserved, subject to the existing
priority of an already failing outer checked scope. This does not introduce a
global stop flag that would suppress another parameter's cleanup callback.

Completion closes callback admission before output adoption. The bounded test
provider attempts a callback during failed in-place adoption, while the context
still exists, and verifies rejection plus output rollback. It never calls a saved
pointer after scope teardown. Owned output factories continue to reserve their
required allocation/code admission before invoking the native producer.

`@no_raise` describes the native implementation; it does not remove checked
closure, preparation or cleanup failures. Callback declarations cannot be marked
`pure` or `nonblocking`. The native record is zero-initialized, then its fields
receive the existing thunk and invocation pointer; unspecified fields stay zero.

## Verification and costs

`tools/check_foreign_pack_callbacks.py` requires a verified complete O2 compiler
build, including TinyCC, XRT, VFS and the embedded compiler. It exercises generic
fixtures, real XGE `xgeAudioClipDecodeMemory`, nested compilation, JIT, static AOT,
dynamic XGE AOT, strict C11/C++11/C++17 headers, existing pack/callback regressions,
invalid source declarations and incompatible native field types. The separate
IR gate checks callback pairs, signatures, owners, preparations and effects.

The real decoder test uses a valid generated stereo WAV and no audio device,
engine, graphics context or network. Its private creator-thread Clip fixture is
not a shipping AudioClip threading decision. It checks exact PCM, output limits,
nullable polling, cancellation, original error propagation and allocation failure.

No runtime ABI or metadata row grows: ForeignCallFacts stays 88 bytes and the
pack field row stays 220 bytes. Added work is compile-time validation and two
stack field stores per callback, reusing the existing scoped callback machinery.
Closure invocation, checked boundaries, code/resource pins and user callback
work still have costs. A warmed scalar-only callback completes 1,000 calls with
zero additional heap allocations; this does not imply arbitrary captured-state
mutation or all callbacks are allocation-free. Raw results and compiler/SDK hashes
are indexed by `xge-packed-callbacks-20261111.json` in delivery provenance.
