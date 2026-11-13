# Direct storage for trivial native outputs

The C emitter can write an already verified trivial output directly into the
call's independent SSA value or a witness's local result payload. This covers
ordinary scalars, exact records, fixed C arrays and tuples of those outputs.
The existing typed physical-output plan still determines every native pointer
and field; it is not inferred from a C type name or a function name.

This is a compiler optimization with the existing language and ABI contract.
Native code never receives the address of a caller's named destination. Input
values are evaluated first and the result storage is distinct. Even an
assignment such as value=read(value) therefore passes independent storage.

NATIVE_OUTPUT_VALUE already has fallible admission. Direct-call result storage
is initialized before entering the native preparation/error scope, including
on every loop iteration. Protocol and dynamic witnesses initialize their own
local result carrier. The emitter reuses that initialization, then writes any
declared size field into the actual output record or tuple member. It adds no
second compound-literal zero assignment before the native call.

On failed native status or pending error at result admission, the independent
trivial payload is reset. It is not published to the language destination.
Receiver/callback cleanup and final error propagation retain their existing
order. A later scope-cleanup error also prevents successful result publication.
This does not promise that a native operation rolls back its own side effects.

Borrowed text/bytes/C-string conversion, acquired native owners, owned bytes
and owned text, native sequences and projected output records retain separate
raw storage and transactional admission. Their acquisition, copy, rollback
and cleanup rules do not become trivial structure assignment.

## Costs and qualification

The ordinary path eliminates the extra raw output object, its initialization
and the raw-to-SSA assignment. Language value copies, argument materialization,
ABI return carriers, the initial result initialization, checked receiver/error
scopes and native synchronization still exist. Failed calls can clear the
payload again. No universal zero-overhead or equal-CPU-time claim is implied.

The new fixture uses a 48-byte record on the qualified x64 platforms and a
96-byte two-record tuple. It checks initialization before every partial native
write, independent input/output pointers, discarded results, failed destination
assignment, function values and 4000 native calls under a zero-allocation budget.
Existing array, protocol, bound/field, projected-record and borrowed/owned text
fixtures provide separate regression coverage.

The isolated Windows GCC comparison removes ten generated raw-to-SSA assignment
sites. For 1000 measured fixture rounds this removes 240,000 bytes of explicit
source-level raw-result assignment; that number is not measured machine memory
traffic. In the selected closure body, GCC O2 reports 288 to 256 bytes of stack
and 528 to 512 bytes of hot code; the 6-byte cold fragment is unchanged. GCC O0
reports an unchanged 512-byte stack and 741 to 687 bytes of code. The whole O0
object shrinks by 640 bytes, while the whole O2 object grows by 104 bytes. These
are compiler/workload-specific results, not a package-wide size or throughput
prediction. Full TCC/compiler and matching native-SDK acceptance is recorded
separately from this diagnostic host-GCC comparison.

tools/check_foreign_output_storage.py requires a complete compiler cohort.
The fixed-output gate also exercises shared modules, C++ clients and negative
native prototypes. The native-backed quota, Shape OBB, editor search and text
gates cover actual resource scopes and SDK calls. No network-security test is
required by these checks. Final runtime/SDK/IDE installation is a separate step.
