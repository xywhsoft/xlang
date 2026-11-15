# Typed native Array input projections

These declarations adapt synchronous native inputs without a C forwarding
function. Generated C prepares arguments, calls the original native symbol
once, and keeps inputs alive through native cleanup. Native `pure`, `no_raise`
and `nonblocking` describe the producer; Array validation, allocation and
scoped admission retain their independent checked failure paths.

```xl
extern "nativeSetIcons" @header("native.h") @no_raise @c_result("status",0) @c_array_group(1,0,2,"int",2) @c_array_input(1,0,"NativeSurface*","view_raw") @c_array_input(1,1,"const NativeRect*","value") func setIcons(array<Surface?> icons,array<Rect> sources,int size=-1)
```

The group arguments are physical first slot, first explicit logical parameter,
column count, native count type, and logical size parameter. The size must
immediately follow the columns. Physical indices include the receiver;
logical indices exclude it. Every column has exactly one typed mapping.
Only `int`, `size_t` and `uint64_t` counts are supported. An output/span/callback group
cannot split the native pointers and their shared count.

`size=-1` requires equal column lengths. A nonnegative size selects at most the
common available prefix; oversized requests truncate. Other negative sizes
raise a checked error. Count conversions, multiplication, allocation and
element preflight occur before the business native call. An empty selected
prefix passes NULL pointer(s) and zero count. Source arrays are nonnullable;
resource/string elements may be nullable when their declared element type is.

## Storage and lifetime costs

| Profile | Temporary storage and work |
| --- | --- |
| Exact contiguous C scalar array to `const T*` | O(1) backing read pin per column and direct Data pointer. No data allocation, element copy or boxing. |
| C scalar input to physical `T*` | One temporary native array and O(n) copies; no source mutation or writeback. |
| Pointer-free CRecord array | One native array and O(n) validated unboxing/copy. Existing language storage contains Value pointers and cannot be cast to a native record array. No new boxing. |
| Resource array | O(n) control-reference snapshots, one native pointer array and one typed scope/owner carrier array. Present elements get real ForeignView/native CallBegin/End pins. Native payloads are not copied. |
| String array | One pointer array, O(n) checks and scans of selected text for embedded NUL. Actual string backing is pinned; string bytes are not copied. |
| Counted string/bytes record column | The record array stores each original data pointer and actual byte length. One read pin per column; O(n) checks and field writes, no text copy or NUL scan. |
| Columnar descriptor array | One zero-initialized native record array, O(n) field writes/unboxing, and resource scope carriers where required. No additional pointer arrays. |

The read pin retains the actual Sequence owner even if native reentry releases
its last language shell. Reads remain available; mutation of the pinned array,
including through erased aliases, raises a checked state error. No borrowing
or parameter modifiers are added to the language. Resource controls are
snapshotted before the first native scope hook. Their physical pins cover the
business call, ordinary input ScopeEnd and projected-element cleanup. String
backing pins last through all these cleanup hooks. Explicit element close
cannot leave a dangling native pointer. The last read pin consumes its owner
once and reports both diagnostics and failed cleanup with a NULL diagnostic.

Native APIs and custom providers must consume/copy/retain inputs within the
declared call. They cannot retain pointers into temporary native arrays, text
backing or POD source. This input contract does not expose writable language
arrays.

## Columnar native record input

```xl
extern "nativeSetButtons" @header("native.h") @no_raise @c_result("status",0) @c_array_group(1,0,3,"int",3) @c_array_record(1,"NativeButton") @c_array_field(1,"text",0,"c_string") @c_array_field(1,"result",1,"value") @c_array_field(1,"semantic",2,"value") func setButtonsColumns(array<string> texts,array<c_int> results,array<c_int> semantics,int size=-1)
```

This emits one NativeButton pointer and count. It does not accept an array of
ordinary mutable language objects or read fields/getters from them. Fields are
exact pointer-free C scalar/CRecord values, scoped resource pointers, or
terminated strings. Unspecified native fields remain zero. Union descriptors,
arbitrary C expressions and nested managed records are rejected. Header proofs
verify the real struct, field/function types, alignment and trivial C++ layout.

XUI exposes `Tabs.setItems`, `Tabs.setIcons`, `Toolbar.setItemsColumns`, and
`MsgBox.setCustomButtonsColumns`. `Columns` identifies the strict parallel-array
contract and O(n) native-record preparation. Tabs icon source rectangles are
explicit. Backend mismatch and native transaction/validation errors retain
their original checked status.

## Counted string and bytes fields

```xl
extern "nativePatches" @header("native.h") @no_raise @c_result("status",0) @c_array_group(0,0,3,"uint64_t",3) @c_array_record(0,"NativePatch","iSize","uint32_t") @c_array_field(0,"start",0,"value") @c_array_field(0,"end",1,"value") @c_array_span_field(0,"text","textBytes",2,"const char*","uint64_t") func patches(array<uint64> starts,array<uint64> ends,array<string> text,int size=-1)
```

`c_array_record` optionally names a size field and its exact C integer type.
Every selected record receives `sizeof(NativePatch)` after a compile-time range
proof. `c_array_span_field` names the physical group, data field, length field,
logical column, exact data-pointer type and exact length type. Data pointers
must be `const char*`, `const uint8_t*` or `const void*`; length and size fields
must be `int`, `uint32_t`, `uint64_t` or `size_t`. The real header must match.
Narrow lengths get checked conversion before native entry. All field names,
including the optional size field, must be distinct.

The source is a nonnullable `array<string>` or `array<bytes>` with nonnullable
elements. Embedded NUL, empty values and arbitrary bytes retain their actual
lengths; native format/UTF-8 rules still apply. This is a synchronous readonly
input: native code cannot save the temporary records or their borrowed pointers.
Identical readonly slice schemas may provide text and bytes declarations of
the same native entry. Other physical ABI, effects and ownership contracts
must still agree. Disjoint descriptor packs and array groups may share a call;
overlapping logical arguments or physical group slots are rejected.

This is not zero-cost record materialization. The current implementation makes
one checked native-array allocation for a nonempty record group and fills it
in O(n). On the qualified x64 fixture, three 32-byte records require 96 payload
bytes per call; 1,000 warmed calls make 1,000 XRT allocations and frees versus
zero for the handwritten C stack-record baseline. These counters exclude CRT
allocations, container setup and CPU time. Default empty array arguments may
allocate their ordinary input containers. Document source processing also
performs its own native copies; the projection adds no text copy or NUL scan.

## Acceptance boundary

`tools/check_foreign_array_projection.py` is CPU-only: direct/bound/protocol
calls, nullable elements, empty/default/truncated lengths, NUL rejection,
reentry/close, OOM before business entry, typed/erased/cursor paths, last-owner
cleanup, native header mismatches and sealed IR corruption with zero C/header
output. Public XUI ABI compilation is recorded separately from executable
fake-native tests. Neither implies GUI/device or shipping/TCC acceptance.

The newer `tools/check_foreign_array_slices.py` requires a complete O2 compiler
and exercises the counted-field path with real CLI JIT/TCC AOT, GCC O0/O2,
C11/C++11/C++17 headers, pointer identity, callback read pins, checked cleanup,
allocation budgets and declaration/header rejection cases. Document has its
own source-patch and native custom-allocator gates; see
[Document source patches](xui-document-source-patches.md).
