# Distinct owning policies with a common C input

Two nominal owning foreign types can describe the same authoritative C pointer
without becoming interchangeable language values. ImageLoad and TextureLoad, for
example, use `xge_async_owned`, but retain distinct result and destruction policies.
Their common native query/release functions must not require forwarding wrappers.

Extern parameter compatibility now accepts this case only when the C name,
authoritative header, owning lifetime, scope, watch, admission domain and owner
agree. Parameter mode, physical ABI, effects and result contracts still agree
independently. Nullable inputs are not admitted by this rule. Factories retain
their declared output policy, including rollback; no result type equivalence or
nominal conversion is introduced. Dynamic casts and function values preserve the
original nominal type too.

The change belongs to semantic checking and the independent sealed IR verifier.
Cgen and runtime layouts are unchanged. It adds no call, allocation, copy, scan,
dispatch or synchronization to generated code. Each input still uses its own
existing ForeignView admission and lifetime protocol; those operations have their
normal costs and are not bypassed.

The fixture has two policies, direct and callable-value inputs, typed factories,
destruction balance and a rejected dynamic cross-cast. Five semantic negatives
cover nominal substitution, C pointer mismatch, domain mismatch, effect mismatch
and nullability. A separate C fixture damages ten sealed IR contracts, including
coherent status metadata and operand identity; rejection must emit zero C/header
bytes. Existing same-policy input/result specialization has its own positive
control to prevent over-restricting valid producer contracts.

`tools/check_foreign_policy_inputs.py` requires a qualified complete compiler for
normal acceptance. Its diagnostic mode is explicitly partial and cannot qualify
AOT. The Windows and Linux full O2/TCC/XRT/VFS candidates each pass 19 checks:
JIT, standalone AOT, semantic negatives and strict C11/C++11/C++17 generated
headers. The ten IR mutations pass against explicitly
identified diagnostic sem/IR objects; that result is not represented as a full
compiler rebuild.

Early diagnostic AOT failures and an over-broad IR restriction were retained as
failed evidence. The full Windows candidate fixes the latter and passes AOT;
partial diagnostic artifacts are not shipped or used as AOT acceptance inputs.
Exact source and report pins are recorded in
`dev/release-materials/provenance/foreign-owned-policy-inputs-20261110.json`.
No runtime release or IDE is replaced by this compiler prerequisite alone.
