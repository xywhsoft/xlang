# Array records inside native descriptors

`@c_array_pack(groupKey, packSlot, "dataField", "countField")` composes a
typed record-array projection with an existing `@c_pack`. The compiler calls
the original native function once. No product-specific forwarding function,
dispatch table, language object traversal or getter is introduced.

```c
typedef struct NativePort { const char* name; int flags; } NativePort;
typedef struct NativeType {
    const NativePort* inputs; int inputCount;
    const NativePort* outputs; int outputCount;
} NativeType;
int registerType(const NativeType* type);
```

```xl
extern "registerType" @header("native.h") @no_raise @c_result("status",0) @c_pack(0,"NativeType",0,6) @c_array_group(10,0,2,"int",2) @c_array_record(10,"NativePort") @c_array_field(10,"name",0,"c_string") @c_array_field(10,"flags",1,"value") @c_array_pack(10,0,"inputs","inputCount") @c_array_group(11,3,2,"int",5) @c_array_record(11,"NativePort") @c_array_field(11,"name",0,"c_string") @c_array_field(11,"flags",1,"value") @c_array_pack(11,0,"outputs","outputCount") func registerPorts(array<string> inputs,array<c_int> inputFlags,int inputSize,array<string> outputs,array<c_int> outputFlags,int outputSize)
```

The group key identifies its column schema. For an embedded group it is not
a C argument position. Keys remain unique across all groups in a declaration.
`packSlot` identifies the descriptor's physical C slot, including a method's
receiver. Logical parameter indices exclude the receiver. The enclosing pack
must contain the entire group, including its logical size argument. An
embedded group adds no C arguments and cannot consume its inputs a second time.

One descriptor may contain several groups, or consist entirely of projected
array fields. Ordinary descriptor fields, independent C inputs, standalone
array groups, direct methods and function-pointer fields retain their normal
contracts. Destination fields may use bounded member paths such as
`ports.items`; each nested parent must have the corresponding
`@c_pack_record(packSlot,"ports","NativePorts")` declaration. Destination
paths, ordinary leaf fields and callback userdata cannot overlap. The real
header also proves disjoint byte ranges, rejecting anonymous-union aliases
whose names differ.

Only record-array groups are embedded. Each column uses the existing typed
`value`, `view_raw`, `c_string` or counted string/bytes field contract. Counts
are exactly `int`, `size_t` or `uint64_t`. The real native header proves the
destination's `const Record*` and count types, record layout, optional sizeof
field and every element field. Logical `size=-1` requires equal lengths;
nonnegative sizes select the common available prefix, and other negatives
raise. Selected empty groups pass NULL with count zero. Default parameters
still obey the ordinary language declaration rules.

All arguments are evaluated once before projection. Every source array keeps
a backing read pin until native execution, result admission and cleanup have
finished. Reentrant reads remain valid; mutation through an alias is checked.
Strings keep their original storage; resource elements retain real controls
and active native scopes. Closing an element during a callback cannot free the
raw object while the native call still uses it. Native code must consume,
copy or independently retain its inputs before returning; it cannot save a
pointer to a temporary record or borrowed text. Allocation, validation and
scope failures prevent native entry, and partial preparation unwinds in
reverse order. Native errors still unwind all prepared groups.

This projection has a deliberate conversion cost. Each nonempty group uses
one zero-initialized native record array, O(n) field preparation and a backing
pin per column. Resource columns additionally use O(n) control/scope carriers.
Terminated strings require NUL scans; counted string/bytes fields use original
data pointers and exact lengths without byte copies or NUL scans. Descriptor
assembly is on the stack, and ABI/offset proofs add no runtime work. The
per-declaration call-facts record stays 88 bytes; destination metadata is kept
only in sparse array-group rows. This is not a whole-bridge zero-cost claim.

`tools/check_foreign_descriptor_arrays.py` requires an actual complete O2
compiler. Its CPU-only cases exercise JIT, standalone/static AOT, shared
retirement/unload, nested compilation, independent GCC O0/O2, C11/C++ headers,
NUL/byte lengths, callback pins/close, full OOM prefixes, declaration refusals
and poisoned headers. `test/cases/foreign_descriptor_arrays_ir.c` separately
checks that damaged IR produces neither C nor header output. Measured XRT
allocation counters compare 1,000 double-group calls with handwritten C;
these counters exclude container construction, CRT allocations and CPU time.
