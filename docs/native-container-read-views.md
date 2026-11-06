# Native container read views

`xl_value_read.h` is the public, synchronous C read boundary for XRT
Array/Object values and xlang native array/dict values. `cli`, `com` and
`clipboard` use it instead of constructing XRT container snapshots.

```c
#include <xl_value_read.h>
bool consume(const xvalue* input)
{
    XL_ValueReadV1 scope = {0};
    if (!xlValueReadBeginV1(xlValueBorrowV1(input), &scope)) return false;
    bool ok = scope.kind == XVALUE_ARRAY;
    for (size_t i = 0; ok && i < scope.count; ++i) {
        XL_ValueBorrowV1 item = xlValueReadAtV1(&scope, i, NULL);
        ok = item.kind == XVALUE_STRING;
        /* Consume item.data.text.Data/Size here, including embedded NULs. */
    }
    if (!xlValueReadEndV1(&scope)) ok = false;
    return ok;
}
```

The source owner must remain alive for the whole call. Begin authenticates
the resident native policy before accessing a payload. Every successful Begin
requires End on every exit, including failure. Do not copy active scopes,
suspend, transfer them to other threads or keep borrowed data after End.
`kind`, `count` and `source` are readable metadata; the remaining fields are
implementation state and must not be changed. A nested container must be
opened before invoking code that could change it. The same receiver cannot
have overlapping read scopes; reuse the already-open scope for duplicate
arguments. Separate owners follow their existing copy-on-write rules.

Native leaf reads support bool, signed/unsigned integers, float, string views
and raw pointers. Existing xvalue reference slots are borrowed directly.
Native pointers use `data.unsignedInteger` with a `uintptr_t` cast. Unknown
policies and unsupported native slot layouts fail with a checked error; they
never fall back to copying or dereference an unrecognized handle.

Reads allocate no snapshots or owning scalar shells and copy no text or
binary payloads. They do copy small scalar/borrow descriptors and perform
scope validation, native owner pin/unpin and the existing mutation guard.
This is not a claim of zero additional instructions against handwritten C.
Array At is constant time; Object/dict At scans to the requested index, as
the old XRT ObjectAt API did. Find uses the container's key lookup.
`xlValueBorrowCopyV1` explicitly creates an owning result when it must outlive
the input scope; this may allocate/copy a native leaf. Clipboard UTF-16 and
COM BSTR conversions remain required by the corresponding Windows APIs.

Source packages must declare the functions they call in
`artifact.runtimeSymbols`, for example:

```json
"runtimeSymbols": [
  "xlValueBorrowV1", "xlValueReadBeginV1", "xlValueReadAtV1",
  "xlValueReadEndV1"
]
```

These declarations select one resident component and its native dependencies
for standalone compilation. JIT, AOT and native SDK headers must come from the
same build. In particular, a stale physical `include/__xl_runtime/xrt.h` can
shadow the embedded implementation header.

`tools/check_example_runtime_regressions.py --out <new-directory>` runs the
repaired examples and focused regressions through JIT and standalone EXEs.
It runs clipboard tests in a private Windows window station. Run
`tools/run_redis_live_example.py --out <new-directory>` separately for an
authenticated disposable Redis fixture. Serial uses its built-in LOOPBACK
transport and needs no serial hardware.
