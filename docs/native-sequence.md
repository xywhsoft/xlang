# Owning read-only Native sequences

`NativeSequence<T>` keeps a trusted immutable C array alive through an independent
Native owning reference and a code lease. Closing the original language resource
does not close this private owner. The language exposes values, not element
pointers or mutable references.

```xlang
auto clusters = shape.clusters()
shape.close()
auto first = clusters[0]       // one independent C POD copy
auto tail = clusters.slice(1) // owning range; no backing-array copy
for index, value in tail
    println(index)
end
```

The empty constructor is `NativeSequence::<c_int>()`. Non-null global, local and
field declarations without an expression also create an empty sequence. Queries
are `.length`, `.count()`, checked `[index]` and `.slice(start, count=-1)`.
Slicing accepts `start == length` only with an empty resulting range. Other
negative bounds, except the `count=-1` remainder sentinel, are rejected.

Copying a sequence, passing/returning it, capturing it in a closure, and boxing it
as `var` preserve the owning range. A checked cast from `var` verifies both the
complete sequence identity and exact element identity. A nullable cast returns
null on mismatch. Neither cast allocates a projection witness or retains the
Native object again. `sequence[i].field = ...` and compound writes are rejected;
assign the element to a local value before modifying the independent copy.

An iteration holds one internal read scope for the entire loop. All break,
continue, return, throw and finally routes use the ordinary verified loop cleanup.
Await/yield inside that scope are rejected by both source semantics and sealed IR.
An async function or generator may suspend before or after the loop. Native
creator-thread requirements still apply; this feature does not make a GUI
resource usable from arbitrary worker threads.

The initial element contract accepts full C-domain `c_*` scalars and exact,
pointer-free C records composed recursively of those scalars. Language integers,
characters, enums with additional invariants, strings, pointers, nullable elements
and reference-bearing records are rejected. Ordinary records are copied by value
when read. Direct NativeSequence elements, including nullable/aliased forms, in
mutable native containers (array/list/dict/set/Stack/queues/maps) are explicitly
rejected until their separate closed container domains are qualified. Nested
mutable containers hit the same inner-type check; there is no ANY fallback.
Direct NativeSequence payloads of typed futures/generators, including an async
function returning a sequence, are likewise outside this first closed domain.
An ordinary user class may own a NativeSequence field, and closure capture and
plain function parameters/results remain supported. `array<var>` remains an
ordinary dynamic-value container and requires an explicit exact sequence cast.

## Trusted Native producer contract

The direct declaration uses:

```xlang
@c_owned_sequence(dataOutput, countOutput, ownerInput, "int", 0)
@c_retain(retain, "zero", release)
```

The data and count occupy distinct physical C output slots, in that order.
The status is a real C int with the declared success code. Count is explicitly
`int` or `size_t`; header/type proofs reject a different pointer/count ABI.
The logical owning input includes the receiver of an instance method. Its exact
typed retainer follows the established `zero` or `positive` success contract.
The owner must have a checked direct destructor: either no external CallBegin,
or an explicit destructor-owns-native-scope policy. Watched owners are outside
this first contract.

The producer promises that the data pointer, count, and every element remain
stable and valid for the entire independently retained owner's lifetime.
Retaining a mutable vector alone is insufficient. This is a trusted FFI storage
contract, like a valid C output pointer; the runtime cannot inspect arbitrary
Native allocation lifetimes.

Before the Native retainer runs, the cold path reserves the owner control, range,
value shell and code lease. It validates negative int counts, language-length
limits, full byte extent, alignment, and address/stride overflow without reading
elements. Zero count ignores and normalizes the data pointer. After a successful
Retain, result materialization requires no allocation. A separate escaped checked
failure still transfers the physically retained reference into the reserved
carrier; existing VIEW_CLOSE retirement handles deferred or failed destruction.
There is no new global cleanup queue or permanent code root.

## Costs

Zero-copy describes the backing array only. On x64 the private backing is 40 B,
an owning range is 80 B, and its ForeignView control is 464 B. The XRT value shell
and an optional code-lease record are additional real allocations. With no code
owner, the tested producer has four allocation points before Native Retain; a
code owner additionally acquires/allocates its existing lease. Slice creates a
range and value shell while sharing the existing private owner. Ordinary shared
assignment retains the value shell and does not copy the backing or add a Native
reference. Deep clone copies range metadata and retains the private control.

Indexing opens/closes one read scope and copies one POD value. Traversal opens
and closes one scope per loop, then uses a typed C array, scalar index increment,
bound comparison and direct value load. It performs no per-element allocation,
boxing, Native getter, descriptor dispatch or conversion. Scope, reference,
ownership-mutation and creator checks remain real costs. O2 timing comparisons
are qualified separately from development-compiler correctness tests.

The CPU fixtures cover primitive and real Shape ownership, pre-Retain OOM,
post-Retain allocation prohibition, slices/copies, worker rejection and deferred
cleanup, code leases, graph cycles, both count ABIs, typed Retain contracts,
strict C/C++ headers, sealed IR mutations, source rejection and all loop exits.
No device, window, browser or network activity is involved.
