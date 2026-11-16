# Signed C integer results with a lower bound

```xlang
extern "native_count" @no_raise @c_result("integer_min",0) func count() as c_int
extern "native_find" @no_raise @c_result("integer_min",-1) func find(c_int key) as c_int
```

The native function is called once and retains its exact C return ABI. A result
at or above the declared minimum is returned unchanged. A smaller result causes
a checked failure. Thus a count can accept zero, while a search can return -1
for not found and reject smaller error codes. Free functions, native methods,
function-backed properties and typed first-class/bound calls use the same rule.
`@no_raise` describes the native producer; it does not suppress admission checks.

The result must be one nonnullable signed C integer type. This includes c_schar,
c_short, c_int, c_long, c_longlong, c_ssize, c_ptrdiff and c_intptr. c_char and
c_wchar are accepted only when signed on the target. Aliases resolve to the same
underlying type. Unsigned integers, ordinary language int, bool, floating point,
fields and output/factory adaptations are rejected. Pure declarations are also
rejected because the bound introduces checked failure. The minimum is a signed
32-bit declaration constant, using the existing closed call-facts slot.

An existing producer diagnostic remains primary, even if the integer itself is
legal. Otherwise a failing result creates a State diagnostic in xlang.foreign,
with the native symbol as its operation. The error code preserves the result
when representable as int32; below INT32_MIN it saturates to INT32_MIN and the
message retains the full signed 64-bit decimal result. This follows XRT's int32
error-code ABI without silently truncating a wide native result. Diagnostic OOM
still produces an independently checked failure. Caller assignment does not
commit a rejected result.

IR derives and verifies the bound, conversion, physical result, effects and
native error scope from the declaration. Damage to these facts is rejected
before either C implementation or public-header output is written. No per-call
fact field, per-function forwarding layer or new scalar heap owner is added.
Generated C performs the direct native call and one integer comparison; failure
uses the common cold `xl6ForeignIntegerFailureV1` helper. Normal foreign scopes,
input protection and first-class invocation still have their existing costs.

## Validation

The focused offline gate is:

```text
python -B tools/check_foreign_integer_min.py --root <repository> --compiler <xl> --compiler-report <build.json> --out <new-directory>
```

For an explicitly recorded development frontend, add `--partial-frontend`.
For a complete public source-provider cohort use `--source-build <build.json>`
instead of the compiler/report options. These development modes do not qualify
embedded VFS/precompiled inputs, nested compilation or final delivery.

`test/cases/foreign_integer_min.xl` covers zero, -1 and positive minima; exact
wide values, direct/function-variable/bound/property calls; illegal results,
producer diagnostics accompanying legal and illegal results, OOM and preserved
destinations. A complete native call count guards against repeated evaluation.
The warmed 1000-count-call region checks zero XRT heap allocation/reallocation;
this is not a CPU timing or all-allocator zero-overhead claim.

The gate also checks C11/C++11/C++17 headers, O0/O2 generated executables,
diagnostic precedence, full wide diagnostics, heap balance, and semantic refusal
of unsigned/float/bool/nullable/language-integer results.
`foreign_integer_min_ir.c` separately damages seven target/declaration contracts
and verifies rejection of 63 mutations and 126 empty C/header outputs.

The shared FlowGraph/Workflow model APIs still require their approved native
reference-counted ownership integration and canvas handoff validation. This
generic result contract does not by itself qualify those model APIs or install
new compiler/SDK/IDE binaries.
