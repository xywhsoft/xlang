# Owned byte outputs

The approved declaration contract for a native `int` status producer is:

```xl
extern "nativeEncode" @header("provider.h") @no_raise @c_output_bytes(2,3,"void*","size_t","xrt",0) func encode(c_int width,c_int height) as bytes
```

The six arguments specify the physical data slot, physical length slot, exact
pointee type, unsigned length type, allocator and success status. The data slot
must precede the length slot, but intervening input slots are allowed. Instance
indices include the native receiver. Pointer spellings are `void*`, `char*` and
`unsigned char*`; lengths are `size_t` and `uint64_t`. The native header supplies
the authoritative prototype and generated C checks it. Exactly one nonnullable
language `bytes` is returned. This contract does not compose with a second
output mapping, borrowed result admission or native retain/release helpers.

`"xrt"` is an explicit provider promise: a non-NULL result is an independent
allocation from the **exact resident XRT allocator**, safe to release with its
`xrtFree`. Matching CRT/compiler/version names alone do not establish that
promise. Interior pointers, aliases of inputs, custom deleters, unrelated XRT
instances and `malloc` storage are not admitted through this declaration. The
length describes readable bytes within that allocation. Native providers are
trusted to honor C memory contracts; the compiler cannot discover an arbitrary
foreign allocation's allocator or bounds safely.

Both physical output slots start zeroed. After the direct native call and
status/error observation, one generic inline runtime operation takes the block
into the existing `xrtValueBytesTake` representation. It consumes the allocation
on every path. A nonmatching status, a native thread error, null/nonempty mismatch,
length beyond the language's signed length range and carrier allocation failure
all release an unadmitted block. Cleanup preserves the primary error. NULL/0
and allocated/0 are both empty bytes. Assignment only commits after admission,
so a failed call preserves the destination. Subsequent bytes assignments and
mutation keep ordinary language value semantics.

No encoded payload is copied at admission. Creating an `xvalue` shell still
allocates; error scopes, range checks, ownership transfer and later ordinary
value-copy/mutation work remain costs. There is no user release callback,
provider dispatch or extra provider-code lease stored in the bytes value.
All mapping facts reside in one sparse 24-byte XLIR row per declaration;
the module adds one vector pointer/count/capacity (24 bytes on 64-bit targets),
and existing per-node call facts keep their size. Cgen consumes the sealed contract
and does not search the source declaration to recover output meanings.

The first consumer is `xge.encodeImagePng(width,height,pixels,stride=0,flags=2)`
for bytes and Buffer. It reuses the native row-capacity checks and PNG encoder.
The matched dynamic XGE provider must use resident XRT services; static linkage
must have the single canonical XRT implementation. No copying PNG shim is added.

## Source preflight

Both platforms pass the source-driver JIT and O0/O2 generated-C executions,
C11/C++11/C++17 public-header checks, incompatible native prototypes and 13
invalid declaration controls. There are 102 sealed-IR mutations per platform;
each is rejected before emitting any C or public header, including disagreement
between bytes/Buffer overloads. Pointer identity proves payload adoption. Exact
NUL bytes, null/allocated empty values, nonadjacent output slots, unsigned count
ABIs, nonzero success status, destination preservation and allocation rollback
are exercised. Assignment/mutation isolates a copied bytes value.

After warmup, the same 100 native producers and takes in C and language each
perform 200 XRT allocations, 200 frees and zero reallocations: one payload and
one value shell per successful operation. This is an allocation measurement,
not a CPU throughput, stack-copy or all-allocator performance claim.

The actual PNG fixture and readable example pass source JIT and independent
dynamic/static executables on both platforms. Forty allocation budgets cover
both failure and success while preserving live allocation/byte counts and
invalid/double-free counters. Executables import no xlang/TCC compiler library;
the static form also omits the XGE host library. The native SDK payload is
unchanged. Indices pin the all-source diagnostic parents, explicit one-unit IR
overload correction, Linux diagnostic-driver path correction and each test.

These are source preflights, not a rebuilt shipping compiler/VFS certification.
Both full O2 builds, actual nested compiler delivery, final SDK/IDE installation
and the rest of XGE/XUI/Document integration remain open. The normal release
runtime has not been replaced by this batch.
