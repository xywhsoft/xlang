# Compiler-managed C strings and bytes

`cstring` and `cbytes` mark C-backed `string` and `bytes` values in the compiler.
They do not introduce XRT value kinds or change ordinary string/bytes layouts.
Typed code carries a private pointer, length and owner reference. Reading its
contents borrows the C allocation; replacing or writing a binding constructs
its replacement automatically. There is no per-read tag test on ordinary
string/bytes paths.

```xlang
extern "library_text" @no_raise func libraryText() as point
extern "library_free" @no_raise func libraryFree(point data)

func readText() as cstring
    return cstring.take(libraryText(), libraryFree)
end

func example()
    string text = readText()
    print(text)          // reads the original C allocation
    auto saved = text   // shares its owner; no payload copy
    text[0] = 'H'        // constructs the replacement; saved keeps the original
    text = "replacement"
end
```

## Adoption and release

`cstring.take(pointer, releaser)` adopts a NUL-terminated UTF-8 allocation.
It scans the byte length once; the producer guarantees the encoding and
terminator. Embedded NUL terminates C text. `cbytes.take(pointer, length,
releaser)` adopts arbitrary bytes with an explicit nonnegative byte length.
Unsigned C sizes (including `c_size` and `uint64`) are accepted; values above
`INT64_MAX` fail through a checked conversion after the pointer is anchored.
Text length/indexing use Unicode scalar units; binary indexing returns `uint8`.

A non-null zero-length allocation is still released. Null with zero length
represents empty data; null with a positive length is rejected. The pointer
must be the allocation address accepted by the releaser. After adoption the
producer must neither mutate nor free it.

The releaser is an unambiguous named `extern @no_raise func(point)` with no
result, using the target C calling convention. Its code must remain loaded
until the last reference is dropped. Other signatures require a small C adapter.
The compiler does not assume that the producer and xlang use the same allocator.

Adoption allocates one C-only owner record. Aliasing increments its reference
count without copying the payload or allocating another record. Scope exits,
early returns and exception unwinding release the allocation exactly once when
the last owner disappears. Storage is detached before invoking the releaser.
Owner-record allocation failure also releases the incoming C allocation.
Binary adoption anchors the pointer before evaluating its length; a throwing or
invalid length therefore cannot leak it.

## Read, write and escape rules

| Operation | Behavior |
| --- | --- |
| Typed local `string/bytes` or `auto` initialized from a C-backed result | Preserves C storage and shares ownership |
| Synchronous result declared `as cstring/cbytes`, including optional results | Preserves the C owner across the return |
| Length, integer indexing, comparisons, iteration | Reads the original allocation |
| Borrowed string parameters and string reader methods | Receive a length-bearing borrowed string view |
| Direct named language functions and concrete generic instances | Compiler specializes matching string/bytes parameters and single results to preserve C storage |
| Direct `print` / `println` | Text goes directly to the console; bytes stream through the XLON sink encoder |
| String interpolation and concatenation; binary concatenation | Read original inputs and construct the new output; no preliminary input materialization |
| `type(value)` / `value is string/bytes` | Reports the corresponding ordinary type |
| Whole replacement, indexed write, string `+=` | Automatically constructs/replaces the value; other aliases keep their old contents |
| Explicit `string(cstring)` / `bytes(cbytes)` | Requests an independent payload copy |
| Closure captures, mutable captured cells, async/generator locals | Shares the C owner, including across suspension and worker tasks |
| Nullable joins, `??`, `?=`, null tests and flow narrowing | Preserve C storage; null and present-empty remain distinct |
| `var`, containers, fields, module globals (including `auto` and multi-result bindings) | Materializes the ordinary representation at the boundary |

A writable by-value language parameter first acquires its own owner. Changing
it cannot free or mutate the caller's value. Replacement publishes the new
value only after construction succeeds. Negative indexes and failures follow
the existing sequence rules. C text and C binary data are never interchangeable.

Compiler specialization covers fixed positional/named arguments and default
arguments of directly resolved functions, including async inputs and explicit and
inferred generic instances. Signature keys include concrete types and have no
63-parameter mask limit. A C-backed default argument also selects the C input
and result specialization when that default is used; explicit overrides do not
evaluate it. Default evaluation order and exception cleanup are unchanged.
`task.spawn` specializes statically resolved named language readers to keep C
inputs in their task frames. Function-value variables keep their fixed signature
and its ordinary conversion rules; Future payloads keep their ordinary result
representation. Borrowed string inputs also keep their C view through
function-value calls. Indirect results with an ordinary signature, virtual calls and unmigrated native
binary readers can still cross an ordinary-value boundary. A function-value
signature explicitly returning `cstring/cbytes` retains the C owner. They are not a promise of
universal zero-copy dispatch. Explicit source-language C parameter
types remain restricted to extern declarations; ordinary language readers
continue declaring `string` / `bytes`.

## Native borrowed span parameters

An extern `cstring` parameter has the C physical type `xstrview`; an extern
`cbytes` parameter has `xbytesview`. Both are readonly, default-mode borrowed
parameters. They accept either C-backed or ordinary values without copying the
payload or allocating an owner record at the call boundary.

```xlang
extern "library_inspect_text" @no_raise func inspectText(cstring text) as bool
extern "library_inspect_bytes" @no_raise func inspectBytes(cbytes data) as bool
```

```c
bool library_inspect_text(xstrview text);
bool library_inspect_bytes(xbytesview data);
```

The callee may read the span only for the duration of the call and must not
retain, modify or free it. `byval`, `byref` and `out` modes are rejected.
C result markers are not raw extern return ABIs: use a synchronous language
`take` adapter as above to state the releaser explicitly.

Existing native declarations keep their ABI: legacy `string` parameters use
C string pointers where applicable; legacy `bytes` parameters expect managed
`xvalue*`. A raw C bytes allocation must materialize when passed to the latter.
Migrate such native readers to a span entry point before claiming zero-copy
extension-library coverage. Eighty resident synchronous entries now have span
overloads: Buffer input construction/writes, codec encoders, CRC32, digests and
stream updates, HMAC, HKDF/PBKDF2, timing-safe comparison, selected key setup and
signing, compression, Unicode validation and encoding conversion/detection.
Mixed C/ordinary byte parameters select the span overload without allocating
input owners; ordinary-only calls retain their legacy ABI.
No XRT modification is needed for the new span ABI. Async/retaining external
APIs still require an ownership-aware adapter; a borrowed span is never a
promise to keep the input after the call returns.

## Costs and supported scope

Zero-copy means no duplicate input payload on the documented static read paths.
C adoption still pays for its owner record, reference-count maintenance and
length scanning for NUL-terminated text. Output creation, explicit copying and
ordinary escape boundaries have their normal allocation costs. The reference
count is atomic only on C owner acquisition/release. C owner locals and captures
use a private frame leaf profile; they remain outside XRT dynamic values and
collections. Reads have no atomic operation or ordinary-value tag test.
C markers are not allowed in fields, explicit global declarations, aliases, uninitialized
nullable marker declarations or generic elements. An inferred module global
uses ordinary string/bytes storage just like an explicitly typed ordinary global;
the initializer's C owner is released after the copy, including on failure. This
rule also covers nullable and multi-result globals. Optional synchronous return
markers (`as cstring?` / `as cbytes?`) and inferred optional locals are supported.
The compiler-private optional descriptor uses `{NULL, SIZE_MAX, NULL}` for null;
present-empty uses zero length. It remains three words and shares the existing
owner/capture machinery. Presence checks occur only on optional paths. Ordinary
XRT nullable types, dynamic values and string/bytes layouts are unchanged.

Mixed printing prepares ordinary argument formatting before the first write.
The console may then receive several writes; a line-atomic output guarantee
is not provided. The binary format budget is checked before output, while
console I/O failures propagate through the normal error path.

## Validation

From `demo6`, with a compatible debug-capable `release/xl.dll`:

```powershell
pwsh -NoProfile -File tools/check_xlir_c_owned.ps1
```

This builds current compiler sources, runtime units and generated-C programs.
Tests check original pointer identity at native/console boundaries, allocation
counters for reads, shared release counts, writes and writable parameters,
conditional joins, iteration with source rebinding, dynamic boundaries,
adoption/copy failure and invalid declaration rejection.

For source-runtime JIT and standalone EXE acceptance, build
`tools/build_xlir_public_source_driver.ps1` in a fresh directory, then pass its
`public_source.exe` to `check_xlir_c_owned.ps1 -PublicDriver <path>`. Source
shipping builds without memory debug still run value and lifetime assertions;
allocation-counter checks use the separate debug runtime lane. Acceptance
records belong under `build/c-owned/`; this targeted check is not a claim that
all repository-wide inventories and unrelated release gates are green.

Current acceptance (`build/c-owned/audit-fixes-acceptance-2/acceptance.json`)
built 1,097 source translation units and verified 2,192 unchanged build inputs.
Two runtime units (the owner unit at both O0 and O2), 21 generated-C programs
and 12 negative declarations passed. O0/O2 contract harnesses each checked 495
RuntimeOps / 3,960 operand effects, 22 capture mutations, 73 managed-storage
mutations and four nullable-join mutations. Twenty-two C interoperability
programs and 14 ordinary programs passed both source JIT and standalone EXE;
ordinary generated C included no C owner runtime header. The source runtime
receipt is `build/c-owned/audit-fixes-public-1/build.json`.

The global initialization fault gate explicitly enables memory debugging before
module initialization and sweeps 32 allocation limits. Four limits trigger
allocation failures and 28 complete normally; all C producer allocations are
released, including when a preceding global result has already been published.
`tools/check_xlir_c_owned_globals_fault.ps1` also runs from the main C owner gate.

The first complete acceptance is recorded in `audit-fixes-acceptance-1.log`.
The final pass resumed after the system exhausted commit memory during linking;
`--no-keep-memory` reduced linker memory use without changing compiler inputs.
Its original and resumed logs, source hashes and fault results are retained in
the second acceptance directory. These targeted contracts do not assert that
unrelated repository-wide release gates have passed.

`xlir_c_owned_sizes.xl`, `xlir_c_owned_defaults.xl`, `xlir_c_owned_spawn.xl` and
`xlir_c_owned_globals.xl` cover unsigned lengths and overflow cleanup, C defaults
and explicit overrides, worker input ownership and fixed result boundaries,
and ordinary nullable/multi-result global storage and replacement.

`xlir_c_owned_calls.xl` checks explicit/inferred generic identities, concrete
signature separation, recursive generic forwarding, a 65-parameter function,
function-value string loans and cleanup of nested temporary C arguments.

`xlir_c_owned_frames.xl` checks original pointers through closure capture, task
workers, mutable cells, a genuinely suspended await, and generator locals. The
runtime unit races four threads acquiring/dropping the same C allocation and
verifies one final release. The resident profile is compiler-private and adds
no C kind to xvalue or ordinary string/bytes representations.

Async input specialization retains the C owner into the worker frame before
publishing the task, including inferred generic calls. Future and generator
payloads remain ordinary container elements; local C owners across suspension
do not materialize.

`xlir_c_owned_stdlib.xl` exercises resident span dispatch, mixed HMAC/KDF inputs,
Buffer construction/writes, Unicode, encoding, compression, prefix bounds and
error propagation. Its emitted C must contain no `xl6CBytesCopyV1` input
materialization. Ordinary codec, compression, encoding and validator programs
also run through JIT/EXE, and their emitted C must not include the C owner
runtime merely because span overloads were imported.

`xlir_c_owned_nullable.xl` covers optional returns, generic forwarding, indirect
string reads, present-empty, null tests and `is` narrowing, joins with ordinary
nullable values, coalescing, null-aware assignment, transparent writes, aliases,
worker captures, generator locals and exception cleanup. Emitted C must contain
no string/bytes input materialization. `xlir_c_owned_nullable_boundaries.xl`
separately checks copying into dynamic values, typed arrays and ordinary fields,
including absent values and logical `type()` identity. The runtime unit checks
the null sentinel independently of present-empty and validates its copy/drop
and ordinary-null conversions.
