# Native callable-field receivers

`@c_field_call` is a direct call through a function-pointer field on an explicit
native pointer class. Its default first physical argument is the native table
pointer. Existing declarations keep that behavior.

For a C table whose callbacks receive a pointer stored in the table instead:

```c
struct Driver {
    void* pUser;
    int (*poll)(void*);
};
```

declare the receiver field and its exact C pointer type:

```xl
extern "poll" @c_field_call("pUser", "void*") @no_raise func poll() as c_int
```

The logical receiver remains the `Driver` view. Its scope, lifetime, module/code
retention, checked admission and expiry rules are unchanged. Generated C obtains
the checked native table, checks the function pointer and calls it once with
`table->pUser`. It adds no C forwarding function, box, text scan, data copy or
allocation for the receiver projection itself. Existing foreign-view safety
bookkeeping remains; this is not a claim that an entire managed call has no cost.

The field name must be a single C identifier. The physical receiver type must be
an explicit bounded C pointer spelling, such as `void*` or `const Payload*`.
Nested expressions, arbitrary C expressions, scalar receivers and pointer
typedef spellings without `*` are rejected. Header-based compile assertions
check both the field type and the full function-pointer signature. A missing
field or ABI mismatch fails compilation.

An empty native table is a checked state error; a missing callback is checked
unsupported. A null value in an existing receiver field is passed unchanged:
some C callback contracts intentionally allow null userdata. The binding must
not invent a non-null requirement.

The same projection applies to direct calls, bound methods, protocol dispatch
and callable-field queries used by native region observers. The receiver stays
logical parameter zero. Extra native inputs and outputs follow the existing
physical expansion rules; the projection replaces the receiver's existing
pointer slot and does not add a parameter.

Receiver metadata is sparse, attached only to declarations using callable
fields. Ordinary calls and the 88-byte `XL6_ForeignCallFacts` remain unchanged.
IR validation rejects malformed sparse storage, receiver names/types and
incompatible call-operation substitutions before emitting C or a public header.

Qualification is performed by `tools/check_foreign_field_receiver.py` using an
explicit full compiler build report. Test artifacts and exact results are
external development materials; this source document alone is not an
acceptance report or a runtime-delivery claim.

The 2026-11-16 batch passes 40 build/run commands per OS on Windows and Linux:
the full O2/TCC/XRT/VFS compiler runs JIT, static and shared consumers (the shared
self-test runs twice), while saved generated C runs at GCC O0/O2. Embedded TCC
and GCC both reject missing fields and incompatible receiver/function types.
Supplementary public-source IR tests reject 44 new and 164 legacy malformed
contracts with empty C/header output, using O0/O2 fixture consumers. The public
source compiler for those mutation tests is O0; it is a separate proof from the
actual full O2 compiler products.

All tested execution paths report zero allocation/free/reallocation deltas for
1000 warm language calls and for the explicit native comparison loop. This does
not establish CPU-time equivalence or remove existing per-call view checks.
Full compiler freezes precede one whitespace-only statement-separation cleanup;
the exact delta and supplementary current-format source test are recorded. The
remaining whole-workspace compiler/SDK/IDE convergence is a separate task.

Exact inputs, logs and artifact verification are indexed by
`dev/release-materials/provenance/xge-field-receiver-20261116.json`.
