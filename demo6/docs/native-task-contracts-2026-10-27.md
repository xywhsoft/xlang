# Native task contract completion

All five gaps from the task/owned-string audit are closed in the compiler:

- Non-null and flow-refined arguments are explicitly lifted into the declared nullable parameter before callback capture, using the ordinary call conversion.
- `task.all` represents an absent nullable string with the existing null value. Optional C scalar frames and aggregate boxers preserve presence, exact width, signedness and floating bits.
- Generic `Future.then/catch/finally` specialization substitutes and revalidates both source and result payload types and ABIs.
- Explicit closed generic references such as `echo::<c_int>` select an existing monomorphic instance and use the ordinary capture-free named adapter. Locals, task callbacks, returned callbacks, nested types and async functions share this path.

The implementation changes no runtime layout or allocator contract. Implicit inference for an open generic function reference is outside this change; supply its explicit type arguments. `task.all` retains its existing array element boxing cost.

## Validation

The Windows candidate is a complete source compiler/runtime cohort (driver-50, O0); Linux uses the corresponding source cohort (linux-50, Os). Each platform runs:

| Gate | Checks |
| --- | ---: |
| Public compiler: emitted C, JIT, AOT, rejection cases | 105 |
| Independent C consumer and compiler contract programs | 23 |
| Existing task/closure/native Future regression controls | 18 |

The independent gate builds separate C translation units at O0 and O2, checks 120 controlled OOM prefixes at each optimization, rejects 6 damaged native variadic contracts, 6 scalar task contracts, 9 task composition contracts and 10 new optional/continuation/reference contracts before C/header emission, and checks 5 metadata allocation failures.

The new fixtures cover all 20 optional C scalar forms, platform extrema, NaN, negative zero, null string aggregation, non-null flow injection, generic success/failure/cleanup callbacks and closed generic callable composition. `extern_c_variadic_value.xl` performs asynchronous work after synchronous OOM accounting: Future readiness does not imply worker frame retirement.

Generated C for `extern_owned_string`, `extern_owned_string_package`, `extern_owned_string_export` and `extern_owned_string_forward` is byte-identical to the preceding driver-43 acceptance. This verifies these ordinary direct-string paths; it is not a claim of zero overhead for every asynchronous or aggregate operation.

Local build receipts, hashes, logs and frozen Linux sources are recorded under `build/owned-string/`, with `task-final-50.json` as the acceptance index. Reproduce the durable gates with `tools/check_extern_owned_string.py` and `tools/check_extern_owned_string_public.py`, providing a fresh evidence directory and a verified current-source compiler/runtime receipt.

Published release binaries are not replaced by these source changes. Concurrent mainline work is preserved during integration.
