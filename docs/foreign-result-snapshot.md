# Owned native value result snapshots

`@c_data_snapshot_result(ordinal)` converts an already-owned native `var`
result into an independent strict PUBLIC data snapshot. Ordinals start at
zero and refer to logical return positions, including tuple positions. They
do not refer to physical C output argument numbers or an instance receiver.
Use a direct owned `xvalue*` return or an existing
`@c_output_view(physicalIndex, "owned_value")` output. An explicitly
[retained borrowed record field](foreign-borrowed-value-record.md) also
qualifies after it acquires its own reference. Nullable `var?`, resources,
arbitrary records and asynchronous returns are rejected. An authoritative
header must prove the physical ABI.

The native producer executes once. The compiler first completes its existing
output conversions, then stages one strict traversal per marked result in
ordinal order. All original owning results remain alive during these
traversals. Only when every traversal succeeds are originals replaced and
retired in reverse order. Failure during conversion, traversal or retirement
releases the entire logical result carrier, including unmarked owned tuple
fields; the caller receives a checked error and no partial result.
Native side effects still follow the producer's contract.

PUBLIC projection includes public stored fields, including inherited fields;
it ignores private fields, getters and serialization renames/omission rules.
Unknown nominal classes and unsupported opaque resources fail closed. It
does not call an opaque Clone fallback. Native Array/List/Set/Dict providers
participate in the same graph traversal. Repeated children preserve sharing
within one result. Distinct results get independent traversals. Immutable
scalar leaves may be shared. A native C NULL maps to logical null through the
immutable XRT null singleton, without allocating a substitute container.

Input and result snapshot declarations can alias ordinary native declarations
of the same symbol without changing the physical ABI. Each declaration keeps
its own raising language admission, even when its producer is `@no_raise`.
`@pure` is rejected. Direct, stored/erased, bound and native function-pointer
field calls use the common result emitter. Unused declarations do not select
PUBLIC plans or resident graph providers.

Metadata reuses 12-byte sparse rows with a disjoint result tag and a presence
bit in the existing 88-byte facts record. No per-method C forwarding function
or new runtime export is introduced. The conversion deliberately adds graph
traversal, schema/provider validation, allocations and owned cleanup; it is
not a zero-total-overhead claim. Models should call a native Shared getter
here, avoiding an earlier DeepClone followed by another data projection.

Validation entry points are `tools/check_foreign_result_snapshot.py` and
`test/cases/foreign_result_snapshot_ir.c`. The language fixture covers NULL,
NUL text/bytes, public fields/getter exclusion, sharing/isolation, native
container providers, direct and function-value forms, tuple conversion,
native failure, repeated complete OOM prefixes and private-owner Drop failure.
Final compiler, SDK and IDE installation is a separate delivery step.

Windows and Linux complete O2 compiler/runtime/TCC/VFS builds passed with
unchanged frozen inputs. The result suite and existing input/output ownership
regressions passed 164 CLI/header/GCC commands in total. The result fixture
passed actual JIT, self-contained TCC EXE/shared modules, physical unload and
independent GCC O0/O2 execution. The complete scalar/pair/mixed allocation
failure prefixes were 8/16/19 in each result execution mode, repeated with
balanced live allocations. Eleven invalid declarations per platform were
refused before publishing generated output.

Each platform also refused 155 IR mutations before C/header emission and
confirmed 88-byte facts and 12-byte rows. Windows uses the matching frozen
frontend source probe against the actual O2 runtime; Linux exercises the
actual O2 compiler ELF entries. Independent verification rechecked 33,462
source, compiler, header, log and product files. The small receipt is
`dev/release-materials/provenance/foreign-result-snapshot-20261118.json`.
These checks qualify this conversion contract, not the whole XGE ecosystem.
