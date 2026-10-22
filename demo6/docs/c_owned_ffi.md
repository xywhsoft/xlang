# C allocation carriers: `cstring` and `cbytes`

These built-in types own a C allocation in a synchronous local scope. They are
read-only bridges to the existing `string` and `bytes` value types, not aliases,
classes, or additional `xvalue` kinds. XRT requires no ABI change.

```xlang
extern "library_text" @no_raise func libraryText() as point
extern "library_free" @no_raise func libraryFree(point data)

func readText() as string
    cstring native = cstring.take(libraryText(), libraryFree)
    assert native.length >= 0
    return native
end
```

`cstring.take(pointer, releaser)` adopts a NUL-terminated UTF-8 allocation.
Its byte length is scanned once. The producer guarantees the encoding and
terminator; no encoding conversion is performed. Embedded NUL terminates C
text. Use `cbytes` for arbitrary bytes or unknown encodings.

`cbytes.take(pointer, length, releaser)` adopts a binary allocation with an
explicit nonnegative length. A non-null allocation with zero length is still
released. A null pointer with zero length is an empty carrier; a null pointer
with a positive length is invalid.

Both constructors accept positional arguments. The releaser must resolve to
an unambiguous named `extern @no_raise func(point)` with no result. It uses the
target C calling convention, never an xlang closure. Its code must remain
loaded until cleanup. Producers with a different release signature need a C
adapter. Context-bearing release callbacks are outside this contract.

Ownership transfers to the carrier. The producer must not mutate or free the
allocation afterward. The pointer must be the address accepted by the releaser,
not an independently borrowed substring or interior address.

## Read and value boundaries

| Operation | Behavior |
| --- | --- |
| Explicit carrier initialization with `take` | Adopt without copying data |
| `.length`, integer indexing, six ordinary comparisons | Borrow C data |
| `cstring.length` / indexing | Same Unicode scalar units as `string` |
| `cbytes.length` / indexing | Byte units; indexing returns `uint8` |
| `string value = nativeText`, `auto value = nativeText` | One independent string copy |
| `bytes value = nativeBytes`, `auto value = nativeBytes` | One independent bytes copy |
| `string(nativeText)`, `bytes(nativeBytes)` | Explicit independent copy |
| `var value = native` | Copy directly into ordinary string/bytes representation |
| Function arguments, returns, fields, typed container values | Materialize the matching ordinary type |
| Closure capture | Store an ordinary string/bytes snapshot |
| Iteration and supported scalar conversions | Materialize the base type and reuse existing rules |
| Other string/bytes methods and allocating operators | Materialize and reuse existing implementations |

Negative indexes and range errors follow the existing sequence rules. Text and
binary carriers are not interchangeable. Ordinary assignment leaves the source
carrier alive; it never creates another owner of the same C allocation.

"One copy" describes carrier-to-value materialization. Existing ordinary-value
semantics may subsequently copy that value, for example into a typed string
container or a string closure environment. This does not change the ownership
contract or permit allocator-dependent adoption shortcuts.

## Scope restrictions and failure

Carrier bindings require explicit initialization with a fresh `take` expression.
Bindings and contents cannot be reassigned. Carrier parameters, return types,
fields, aliases, nullable carriers and generic carrier elements are rejected.
Declare the corresponding ordinary type at these boundaries.

Carrier locals cannot persist in coroutine or generator frames. Synchronous
code may capture a carrier into a closure and schedule that ordinary-value
snapshot for asynchronous work.

Normal scope exit, early return, loop exits and exception unwinding all release
the allocation exactly once. Storage is detached before invoking the releaser.
Binary construction anchors the allocation before evaluating its length, so a
throwing length expression also releases the allocation. Invalid lengths consume
and release the adopted allocation.

Failed materialization preserves the source owner. Assignment publishes the new
ordinary value only after a successful copy, preserving the old target on
failure. Normal `string` and `bytes` layouts and cleanup remain unchanged.

## Validation

Run from `demo6` with a compatible `release/xl.dll` available:

```powershell
pwsh -NoProfile -File tools/check_c_owned.ps1
```

The check builds compiler sources independently, executes generated C, checks
allocation failure and release counts, and rejects invalid carrier declarations.
`xlir_c_owned_storage.xl` additionally exercises typed container boundaries
against the current source runtime.

For full source API/JIT and standalone EXE acceptance, first build the existing
public source driver with `tools/build_xlir_public_source_driver.ps1`, then pass
its `public_source.exe` path through `check_c_owned.ps1 -PublicDriver <path>`.
Memory-debug allocation/failure checks run against a debug-capable runtime;
source shipping profiles without memory debug still execute the value/lifetime
programs, while allocation counters are checked separately by the debug run.

Two pre-existing regression fixtures fail identically with the pre-change
compiler at `1b4aef459`: `cgen_string_embedded_nul.xl` expects codec byte results
to be strings, and `cgen_var_string_concat.xl` fails its checked-conversion
assertion. They are not acceptance gates for the C owner feature.
