# Native return values with output parameters

An `int` return can be ordinary data while pointer parameters carry additional
results. Declare the return as the first tuple item with `-1`:

```text
extern "query" @header("native.h") @no_raise
 @c_outputs(-1,1,2) @c_result("integer_min",0)
 func query(c_int mode) as (c_int,c_double,c_int)
```

The physical prototype is `int query(int, double*, int*)`. The first tuple item
is the actual C return, followed by the two output values. Physical parameter
indices include the implicit receiver of a native instance method. For example,
`int read(Receiver*, int, double*, int*)` uses `@c_outputs(-1,2,3)`.

`@c_result("integer_min",0)` rejects negative returns without interpreting 0 or
1 as a success code. A bound of -1 can preserve a normal not-found result.
Without this attribute, the complete signed C int range is data, including
`INT_MIN`. An XRT error still rejects the call even if its integer return is
within the declared range; the original error remains primary.

This closed contract requires an exact `c_int` first result, at least one
trivial pointer output, and increasing, unique physical output slots from 0 to
63. The return sentinel must occur only once and first. It cannot combine with
`c_status`, managed output conversion, initialized-size output flags or another
result conversion. Existing void/status output declarations keep their rules.
Async multiple returns remain outside the language's current result contract.

The compiler emits one native call assigning the C return and addresses of
local output fields. It publishes the whole tuple only after the existing
receiver/code/error scopes succeed. Failed assignment leaves all language
destinations intact. Native side effects are not rolled back. C and C++ ABI
witnesses check the exact return and pointer types against the original header.

Sparse IR includes one return row with no physical argument. Verification checks
its ordinal, sentinel, exact C int type, physical slots and minimum-predicate
presence. The row remains 12 bytes; no AST/IR node, target or runtime ABI grows.
Direct, function-value, explicitly typed bound-method, protocol, dynamic and
native function-pointer-field calls use the same contract. The existing
`auto bound = receiver.method` limitation for native multiple results is
unchanged; use an explicit function type.

`tools/check_foreign_return_outputs.py` requires an actual complete O2 compiler
build report. It exercises JIT, independent executable/shared outputs, GCC
O0/O2 clients, C11/C++11/C++17 headers, malformed declarations and prototypes,
and real embedded compilers in executable and shared hosts.
`tools/check_foreign_return_outputs_ir.py` separately requires a genuine
all-source frontend build without object overrides. Its 135 mutations per
optimization must reject before emitting C or header bytes.

The warmed direct-query comparison runs 1,000 language calls and 1,000 native
calls with enabled XRT memory accounting. Both intervals require zero XRT
allocation, free and reallocation operations and unchanged live totals. This
does not establish CPU or OS allocator equivalence. Receiver admission, code
activity, native error scopes, tuple copies and dynamic-call boxing still have
their normal costs. See the qualification receipt in
`dev/release-materials/provenance/foreign-return-outputs-20261117.json`.
