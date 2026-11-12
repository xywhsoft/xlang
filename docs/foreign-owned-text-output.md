# Owned UTF-8 output

Use a declaration to call a native status producer directly:

```xlang
extern "exportText" @header("provider.h") @no_raise
 @c_output_text(1,2,"char*","uint64_t","xrt",0)
 func exportText(c_int mode=0) as string
```

The C prototype is `int exportText(int, char**, uint64_t*)`. The two indices
are physical C output positions; the last value is the success status. Counts
can be `size_t` or `uint64_t`. Text requires `char*`; arbitrary output bytes
use the existing `@c_output_bytes` contract. Both can name the same producer
when their physical ABI, allocator, status, effects and inputs match exactly.
Each language result retains its own admission and cleanup contract.

The provider promises an independent allocation from the exact resident XRT,
with at least `size + 1` readable bytes and a NUL terminator at `data[size]`.
Sharing a CRT is insufficient. The count excludes that final terminator.
Interior NULs are valid UTF-8 U+0000 and remain part of the string. NULL with
zero length is a valid empty result; NULL with nonzero length is rejected.
The compiler cannot establish arbitrary foreign allocation bounds: providers
must uphold the declared readable-capacity and allocator promises.

The generated code calls the native producer once, then transfers its actual
buffer into the two-field string. There is no second payload allocation,
copy, `strlen`, dynamic box or provider release callback. Admission performs
one UTF-8 validation scan and constant-size status/range/terminator checks.
Language copies and later mutations retain their ordinary value semantics.
Normal foreign resource/error scopes still apply; this is not a claim that
an entire managed native call has no bookkeeping cost.

Every failure consumes and frees the native output, preserves the first
diagnostic and prevents assigning the destination. The Boolean success path
is authoritative even if creating an error object fails. Destruction uses
resident `xrtFree`, so the admitted string has no provider-code dependency.

Owned text and bytes share one sparse typed IR output schema with an explicit
kind. Exact result type, header, output slots, count/pointer ABI, allocator
promise, effects and success status close before C emission. Contradictory or
missing metadata rejects both C and public-header generation. The existing
88-byte declaration fact record has not grown; each sparse producer row is
28 bytes, four more than the earlier byte-only row.

## Explicit input lengths and descriptors

Readonly `@c_span` accepts `uint64_t` counts in addition to `size_t` and `int`.
The optional language prefix uses bytes: `-1` consumes the complete input,
nonnegative lengths clamp to its actual byte count, and other negative values
fail before calling native code. The prefix parameter does not become a C
parameter and must remain disjoint from descriptor/array input groups.

Descriptor `@c_pack` positions define the C argument order independently of
the logical input group's position. Its members remain complete, disjoint
and typed. This permits `deserialize(data, size=-1, options...)` to call a
native `(const Options*, data, count, result**)` function with a stack
descriptor, without a forwarding function or heap allocation. Source and
sealed IR both validate the physical plan, and Cgen consumes that plan.

## Source acceptance

`tools/check_foreign_owned_text.py` checks public-source JIT, GCC O0/O2,
pointer identity and native/language allocation counts, copied-value
isolation, empty/NUL/Unicode text, bad UTF-8/terminators/status/counts,
prefix/descriptor projections, C/C++ headers, declaration negatives and IR
mutation refusal. The runtime fixture additionally forces diagnostic
allocation failure. The byte-output gate is rerun against the same schema.

Document adds paired text/byte HTML and plain-text exports, range/table text,
fragment exports and safe serialization/import inputs. A matched native SDK
must carry `XUI_DOCUMENT_EXPORT_XRT_ALLOCATOR == 1`; prototypes alone cannot
authenticate old CRT-backed export buffers. Native allocator qualification is
recorded in XGE commit `d902e6b8`.

The initial source acceptance does not certify shipping O2/VFS/embedded
compiler products or final SDK/IDE installation. Their evidence must be
recorded separately before delivery is declared complete.

The Windows r31 full O2 build subsequently passed 72 top-level qualification
commands plus five continuation checks after its three products and 13,517
frozen inputs were reverified. This includes the actual nested language
compiler in independent EXE/shared-library hosts, not only a frontend overlay.
See `dev/release-materials/provenance/foreign-owned-text-windows-20261113.json`.
The host reboot interrupted the first qualification report; its zero-filled
bytes are preserved and were not counted. Linux and final installation remain
separate requirements.

The Linux r24 successor has now passed 77 top-level commands plus five
continuation checks, including independent nested EXE/shared hosts and both
Document link modes. Its 15,076 frozen input files and three products were
rechecked unchanged. Relative to Linux r22, the CLI is unchanged and both
shared libraries add 12,288 bytes. See
`dev/release-materials/provenance/foreign-owned-text-linux-20261113.json`.
Final SDK/compiler/IDE installation remains separate.
