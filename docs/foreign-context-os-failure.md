# Execution-context OS-key failure probes

`tools/check_foreign_context_os_failure.py` compiles the real
`src/runtime/xl6_foreign_context.c` at O0/O2 on Windows/Linux. It includes that
file directly in a private test translation unit, changes only its exported
names to avoid interposing on the loaded runtime, and injects failures only at
its OS key allocation/write calls. XRT once, atomics and errors come from the
verified complete compiler/runtime cohort, not test replacements. Source and
header hashes must match that cohort before compilation.

Each new process starts cold. The probe refuses allocation at each of the five
FLS/pthread-key creation positions and verifies that all earlier allocations
are freed in reverse order. It checks the memory diagnostic, unchanged
allocation-free `TryGet`, successful retry after the OS fault is removed and
no further key allocation after readiness. A subsequent OS write failure must
leave the caller's scope and current chain untouched. Three nested scopes then
exercise membership and non-LIFO removal without allocating more OS keys.

Both platforms pass 15 commands: one private SDK assembly plus two builds and
twelve executions. Windows uses actual FLS keys, Linux actual pthread keys.
Only the selected failure is synthetic. No runtime implementation is changed.
Receipt: `dev/release-materials/provenance/foreign-context-os-failure-20261117.json`.

This validates context primitives, not complete language callback-group or
view admission under cold OS failure. It does not qualify a fiber switch,
process-wide key exhaustion, concurrent initialization, OS key-free failure,
module unload or claimed-cycle finalization. Those require their own gates.
The injected write failure occurs before scope publication; failures while
unwinding a published scope are not recoverable through this test contract.
