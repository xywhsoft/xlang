# Borrowed XRT values in native output records

`@c_output_field(ordinal, "member", "borrowed_value")` maps an `xvalue*` member
of a `@c_output_record(..., "borrowed")` to an owned language `var`. The field
must belong to the same XRT as the caller. The native producer keeps its own
reference and must keep the pointer valid until the compiler can retain it.

```xl
extern "readRecord" @header("record.h") @no_raise @c_output_record(1,"NativeRecord","borrowed") @c_status(0) @c_output_field(0,"name","cstring") @c_output_field(1,"data","borrowed_value") @c_data_snapshot_result(1) func read(c_int index) as (string?,var)
```

The compiler checks producer status before inspecting the output and retains
each value before any output retention hook can run. Each selected field
contributes one reference, even when two fields contain the same pointer.
A native NULL maps to the XRT null singleton. Strings are copied using the
existing C-string result contract.

Use `@c_data_snapshot_result(ordinal)` to return an independent public data
snapshot. This traverses supported data and public stored fields, preserves
null and rejects unsupported resource values. The snapshot never runs getters.
With the snapshot attribute omitted, the returned `var` owns a reference to
the value itself and observes mutations through other references.

Failure in any field conversion or snapshot aborts publication of the whole
result. The call releases acquired references and copied fields while
preserving the checked failure. The native producer is called once. Shared
admission adds one XRT retain per nonnull field and no value copy; independent
snapshot traversal and C-string copies retain their usual allocation costs.
