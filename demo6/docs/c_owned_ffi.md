# Compiler-managed C strings and bytes

`cstring` and `cbytes` mark C-backed `string` and `bytes` values in the compiler.
They do not introduce XRT value kinds or change ordinary string/bytes layouts.
Typed code carries a private pointer, length and owner reference. Reading its
contents borrows the C allocation; replacing or writing a binding constructs
its replacement automatically. There is no per-read tag test on ordinary
string/bytes paths.

```xlang
extern "library_text" @no_raise func libraryText() as point
extern "library_free" @no_raise func libraryFree(point data)

func readText() as cstring
    return cstring.take(libraryText(), libraryFree)
end

func example()
    string text = readText()
    print(text)          // reads the original C allocation
    auto saved = text   // shares its owner; no payload copy
    text[0] = 'H'        // constructs the replacement; saved keeps the original
    text = "replacement"
end
```

## Adoption and release

`cstring.take(pointer, releaser)` adopts a NUL-terminated UTF-8 allocation.
It scans the byte length once; the producer guarantees the encoding and
terminator. Embedded NUL terminates C text. `cbytes.take(pointer, length,
releaser)` adopts arbitrary bytes with an explicit nonnegative byte length.
Text length/indexing use Unicode scalar units; binary indexing returns `uint8`.

A non-null zero-length allocation is still released. Null with zero length
represents empty data; null with a positive length is rejected. The pointer
must be the allocation address accepted by the releaser. After adoption the
producer must neither mutate nor free it.

The releaser is an unambiguous named `extern @no_raise func(point)` with no
result, using the target C calling convention. Its code must remain loaded
until the last reference is dropped. Other signatures require a small C adapter.
The compiler does not assume that the producer and xlang use the same allocator.

Adoption allocates one C-only owner record. Aliasing increments its reference
count without copying the payload or allocating another record. Scope exits,
early returns and exception unwinding release the allocation exactly once when
the last owner disappears. Storage is detached before invoking the releaser.
Owner-record allocation failure also releases the incoming C allocation.
Binary adoption anchors the pointer before evaluating its length; a throwing or
invalid length therefore cannot leak it.

## Read, write and escape rules

| Operation | Behavior |
| --- | --- |
| Typed local `string/bytes` or `auto` initialized from a C-backed result | Preserves C storage and shares ownership |
| Synchronous result declared `as cstring/cbytes` | Preserves the C owner across the return |
| Length, integer indexing, comparisons, iteration | Reads the original allocation |
| Borrowed string parameters and string reader methods | Receive a length-bearing borrowed string view |
| Direct named synchronous nongeneric language functions | Compiler specializes matching string/bytes parameters and single results to preserve C storage |
| Direct `print` / `println` | Text goes directly to the console; bytes stream through the XLON sink encoder |
| String interpolation and concatenation; binary concatenation | Read original inputs and construct the new output; no preliminary input materialization |
| `type(value)` / `value is string/bytes` | Reports the corresponding ordinary type |
| Whole replacement, indexed write, string `+=` | Automatically constructs/replaces the value; other aliases keep their old contents |
| Explicit `string(cstring)` / `bytes(cbytes)` | Requests an independent payload copy |
| `var`, containers, fields, closure captures, nullable joins | Materializes the ordinary representation at the boundary |

A writable by-value language parameter first acquires its own owner. Changing
it cannot free or mutate the caller's value. Replacement publishes the new
value only after construction succeeds. Negative indexes and failures follow
the existing sequence rules. C text and C binary data are never interchangeable.

Compiler specialization currently covers fixed positional/named arguments and
default arguments of directly resolved, synchronous nongeneric functions, with
up to 63 parameters. Indirect function values, virtual/generic calls and legacy
native binary readers can cross an ordinary-value boundary. They are not a
promise of universal zero-copy dispatch. Explicit source-language C parameter
types remain restricted to extern declarations; ordinary language readers
continue declaring `string` / `bytes`.

## Native borrowed span parameters

An extern `cstring` parameter has the C physical type `xstrview`; an extern
`cbytes` parameter has `xbytesview`. Both are readonly, default-mode borrowed
parameters. They accept either C-backed or ordinary values without copying the
payload or allocating an owner record at the call boundary.

```xlang
extern "library_inspect_text" @no_raise func inspectText(cstring text) as bool
extern "library_inspect_bytes" @no_raise func inspectBytes(cbytes data) as bool
```

```c
bool library_inspect_text(xstrview text);
bool library_inspect_bytes(xbytesview data);
```

The callee may read the span only for the duration of the call and must not
retain, modify or free it. `byval`, `byref` and `out` modes are rejected.
C result markers are not raw extern return ABIs: use a synchronous language
`take` adapter as above to state the releaser explicitly.

Existing native declarations keep their ABI: legacy `string` parameters use
C string pointers where applicable; legacy `bytes` parameters expect managed
`xvalue*`. A raw C bytes allocation must materialize when passed to the latter.
Migrate such native readers to a span entry point before claiming zero-copy
extension-library coverage. No XRT modification is needed for the new span ABI.

## Costs and supported scope

Zero-copy means no duplicate input payload on the documented static read paths.
C adoption still pays for its owner record, reference-count maintenance and
length scanning for NUL-terminated text. Output creation, explicit copying and
ordinary escape boundaries have their normal allocation costs. The reference
count is local/non-atomic; C owner locals cannot persist in coroutine/generator
frames. Capturing for asynchronous work stores an ordinary value snapshot.
C markers are not allowed in fields, global declarations, aliases, nullable
marker declarations or generic elements.

Mixed printing prepares ordinary argument formatting before the first write.
The console may then receive several writes; a line-atomic output guarantee
is not provided. The binary format budget is checked before output, while
console I/O failures propagate through the normal error path.

## Validation

From `demo6`, with a compatible debug-capable `release/xl.dll`:

```powershell
pwsh -NoProfile -File tools/check_xlir_c_owned.ps1
```

This builds current compiler sources, runtime units and generated-C programs.
Tests check original pointer identity at native/console boundaries, allocation
counters for reads, shared release counts, writes and writable parameters,
conditional joins, iteration with source rebinding, dynamic boundaries,
adoption/copy failure and invalid declaration rejection.

For source-runtime JIT and standalone EXE acceptance, build
`tools/build_xlir_public_source_driver.ps1` in a fresh directory, then pass its
`public_source.exe` to `check_xlir_c_owned.ps1 -PublicDriver <path>`. Source
shipping builds without memory debug still run value and lifetime assertions;
allocation-counter checks use the separate debug runtime lane. Acceptance
records belong under `build/c-owned/`; this targeted check is not a claim that
all repository-wide inventories and unrelated release gates are green.
