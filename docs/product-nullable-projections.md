# Protected nullable product projections

Tuple assignment previously passed semantic checking for a nonnullable field
assigned to its nullable element type, then failed IR lowering with
`XLIR product binding has no exact protected field transaction`. This affected
ordinary scalar/string/bytes/container values and retained foreign resources,
including XGE/XUI record results.

The lowering now recognizes only the existing exact `T -> T?` relation, with
the destination's element TypeId equal to the already evaluated field's
TypeId. It acquires an independent owner, emits the existing nullable-wrap
instruction, then releases the source field's protected anchor. The RHS is
never reevaluated. The nullable ABI/ownership verifier remains unchanged;
matching C representations cannot substitute for matching language TypeIds.
Other conversion families retain their existing checked paths.

This preserves left-to-right destination evaluation and commits. The complete
native output tuple is admitted before assignment begins: a native/status or
output-snapshot failure changes no destinations. A later destination
conversion may fail after an earlier destination has committed. This batch
does not introduce whole-assignment rollback.

`test/cases/foreign_product_nullable_lift.*` exercises scalar, string, bytes,
array and retained-root fields, locals, object members, array elements and
bound/protocol results. A counted native producer proves one invocation on
success, at most one on failure; output reservation may fail before entry.
The 64-prefix resource test checks remaining values and heap balance even
when a later destination fails. The old compiler is an explicit rejected
baseline. The new C/C++ public headers, CLI JIT, standalone AOT and generated
C under GCC O0/O2 are checked by `tools/check_product_nullable_lift.py`.

## Owning XRT output admission

For `xvalue**` outputs, a successful admission transfers the pointer and clears
the raw slot. Raw cleanup now runs only in the failed, non-NULL branch. The
successful path no longer takes/restores the error or releases a NULL raw
value. A native error is latched before releasing a rejected value or tearing
down its input scope. The earlier bridge could replace producer code 73 with
finalizer code 91 when the C status indicated success but XRT held an error.
Both non-NULL rejected values and NULL outputs with failing input-scope
cleanup now preserve the original producer diagnostic. Failed non-NULL raw
results still receive checked cleanup.

The same gate emits a 1000-call program through the old and new compilers,
links both against the same new resident runtime and instruments explicit
calls in generated C. Handwritten test wrappers forward to the real APIs;
they are not included in product bindings. Included runtime-header internals
and separately compiled native producer calls are excluded from these counts.

| Explicit generated calls, 1000 successful returns | Before | After |
| --- | ---: | ---: |
| `xrtGetError` | 4000 | 3000 |
| `xrtTakeError` | 1000 | 0 |
| `xrtValueRelease` | 2000 | 1000 |
| `xrtSetErrorTake` | 1000 | 0 |
| NULL arguments to `xrtValueRelease` | 1000 | 0 |

O0 and O2 must show the same differences. Allocation/identity, NULL success,
producer/admission failure and finalizer-error regressions remain in
`tools/check_foreign_output_snapshots.py`; real Document/Dock integration is
checked separately by `tools/check_xui_state_outputs.py`.
`test/cases/foreign_owned_error_priority.*` distinguishes producer/finalizer
diagnostics, verifies NULL and non-NULL outputs, input-scope failure, unchanged
destinations, exactly-once finalization and heap balance. The old bridge is
an explicitly failing baseline; JIT/AOT and GCC O0/O2 must preserve code 73.

## Costs and remaining work

Owning value transfer still performs no value clone or extra value-shell
allocation. Required status, scope, resource/code lifetime and assignment
checks remain; the counters are not a CPU timing or whole-bridge zero-cost
claim. Nullable conversion from a protected borrow acquires an independent
owner: strings can copy/allocate, bytes/arrays and retained views retain an
owner. Member/index writes may also perform their ordinary boxing work.
Directly transferring exact nullable-compatible product owners is a separate
performance follow-up; this implementation does not conceal those costs.

The core changes require complete matching O2 compiler/runtime/TCC/VFS
qualification before delivery. Partial frontend probes are diagnostic evidence
only. Full XGE/XUI/Document integration and final matched SDK/compiler/IDE/help
delivery remain active; no release installation is implied by these tests.
