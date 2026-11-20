# Owning callback groups in C descriptor packs

An immutable C descriptor can project several retained language callbacks into
one shared userdata pointer. Calls still target the declared C entry directly.
The compiler emits typed thunk entries; there is no per-method C forwarding
layer, name dispatch or callback boxing into `var`.

The additional attribute is:

```text
@c_pack_callback_group(packSlot, "userdataField", ownerSlot)
@c_pack_callback_group(packSlot, "userdataField", ownerSlot, "optional")
```

`packSlot` identifies an existing `@c_pack`. `userdataField` is a closed member
path, with declared `@c_pack_record` parents for nested records. `ownerSlot` is
a separate **physical C argument position**, including a method receiver and
output pointers. It has no corresponding language argument. All positions are
zero based. A method receiver, output, descriptor, fixed input or another group
owner cannot occupy that same physical slot.

Each member uses `@c_pack_field(..., "owned_callback")` and the existing
`@c_owned_callback` declaration. Its logical owner binding, failure value,
`first`/`last` userdata direction and callback argument/result contracts remain
in force. The owner binding is distinct from `ownerSlot`. Every member in one
group must name the same native ownership-record C type. A group must contain
at least one declared retained member; individual members may be NULL.

Use the explicit `"optional"` mode when the native API accepts an absent
group. If all members are NULL, generated code allocates no callback-group
environment and passes NULL for both the descriptor userdata and the physical
owner argument. If any member is active, the existing checked group ownership
and rollback rules apply. The three-argument declaration continues to pass an
owner for an all-NULL group.

The complete runnable declaration/tests are in
`test/cases/foreign_pack_owned_callback.xl` with its authoritative `.h` and native
consumer. They include two groups in one call, nested descriptors, free and
bound methods, protocol dispatch and a factory output between descriptor and
owner slots. The existing synchronous `callback`/`callback_user` pack fields
retain their original nonescaping contract.

## Native publisher contract

The compiler initializes the descriptor on the stack and passes a separate
typed owner record containing a genuine `xrtownershipref`, Hold and consuming
checked Drop callbacks, in the same order as individual retained callbacks.
Native code must copy the descriptor data it needs and acquire an owner
reference before retaining any callback/userdata beyond the call. It must never
save pointers to either stack record.

A native publisher owns the **group**, or an enclosing real traced owner,
through the entire typed thunk return tail. A temporary native snapshot must
also retain that owner if a callback can replace or clear the current native
slot. Keeping just a member record does not protect the shared userdata.
The native owner graph must trace every retained group slot. Reference-counted
ownership alone does not make an untraced native/language cycle collectible.

A failed factory may still produce a cleanup duty. That duty must retain its
own group reference until the native cleanup consumes it. Ordinary factory
rollback runs within the checked language call; the fixture verifies a failed
output holding the group is released. General HostSession retryable cleanup and
real module unload are separate integration work.

## Generated representation and costs

Only sparse pack metadata grows. Ordinary IR nodes and the 88-byte foreign-call
fact record retain their layouts; pack-field rows remain 220 bytes. Closed IR
checks group membership, exact owner ABI, nested parents, field collisions and
every physical argument step before C/header output. Authoritative C headers
also prove the actual descriptor field and function-pointer ABI.

The compiler constructs bounded closure/context/signature arrays on the stack,
then creates one group with a dense member vector. It releases its temporary
group once after the checked call. Each typed thunk reads its constant member
index, checks the group prefix and enters the existing callback runtime. The
group adds no forwarding call; existing entry/leave, scope, ownership and error
bookkeeping still execute. This is not a zero-total-overhead or CPU-equivalence
claim.

For the independently measured native fixture with three slots/two non-NULL
members, the owning group uses four allocations/320 live bytes when a real
adapter-code lease is required, excluding existing closures. An externally
resident owner and language capture environments have different cold costs.
Construction/cleanup includes real allocation, graph and code-pin operations.
Warm language dispatch is checked separately for zero allocation/free/realloc.

Mutable captures now admit all exact C scalar ABIs and `float32`, using the
existing pointer-free byte-cell carrier. Values retain their declared width and
signedness; a failed checked arithmetic update leaves the cell unchanged.
This uses the carrier's existing cell and payload allocations; reads/writes do
not allocate. Native pointers and resource handles are excluded from this scalar
rule and retain their explicit ownership contracts.

## Qualification tools

`tools/check_foreign_pack_owned_callbacks.py` verifies a complete public-source
cohort, including O0/O2 mutation checkers, JIT/static/shared programs, C11/C++
header/ABI negatives, repeated OOM sweeps, warm dispatch and legacy pack/callback
regressions. This source proof does not rebuild the shipping compiler/VFS.

`tools/check_foreign_pack_owned_callbacks_full.py` requires a hash-matched full
O2 compiler/TCC/XRT/VFS build. It stages a matching private SDK beside copies of
the exact reported binaries, then checks actual CLI JIT/static/shared programs
and a standalone nested compiler that parses these new declarations. Workspace
`release/` and immutable build inputs remain untouched. Final matching SDK/IDE
delivery and HostSession rendering integration are not implied by either gate.

The qualified batch passes 60 source commands and 19 complete-compiler commands
on each OS. Each O0/O2 IR checker rejects 222 damaged variants before any C or
header bytes are emitted. The memory-debug executable repeats 24-prefix OOM
sweeps, retains no live allocations across repeated sweeps and checks 1,000 warm
dispatches per sweep without allocation/free/reallocation. The native scalar
fixture covers 20 exact C scalar types plus `float32`, including checked overflow
with unchanged stored value. Nested fixtures run from the repository root for
their explicit source-relative native headers and use isolated temporary paths.
Exact inputs, commands, limits and build hashes are recorded in
`dev/release-materials/provenance/native-callback-group-pack-20261116.json`.
