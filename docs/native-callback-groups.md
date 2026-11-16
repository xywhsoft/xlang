# Owning native callback-group foundation

The user approved a shared userdata environment for multiple retained callback
fields. The current implementation provides its **internal runtime foundation**;
language pack declarations, IR/lowering, generated thunks and HostSession are
still pending. No runtime, SDK or IDE installation is performed by this batch.

`XL6_NativeCallbackGroupRuntimeV1` constructs an owned group from borrowed closure,
signature and optional native-context arrays. Its read-only public prefix holds
the member count and pointer vector. A generated typed thunk can select its
constant member and enter the existing native-callback runtime directly. This
does not require an operation-code dispatcher or another forwarding function.

The group owns one real adapter-code lease shared by its native callback
records. Existing closure/context/code ownership and Enter/Leave remain in use;
ordinary callback construction is unchanged. Nullable members map to empty
slots. Even an all-null group retains its adapter capability. Native publishers
must retain the group, or a real enclosing owner, through the full thunk return
tail. An independent member pin does not keep the group userdata alive.

Group reference counting, graph tracing, claim/restore, clear/finish, consuming
checked Drop and failed-construction rollback use the existing mutation and
callback error protocols. The group now has an independent source component
and ownership domain. Its dependency includes ordinary callbacks, but ordinary
callback symbols do not request the group source/domain. The internal shared-
lease constructor stays beside the private member layout; ELF visibility hides
it from export-dynamic roots. No callback entry forwarding layer is added.

The development runtime gates use a complete, hashed public-source cohort,
replace its native-callback/runtime-symbol objects, and add the group object.
Windows and Linux each pass 17 commands: C11/C++11/C++17 headers plus O0/O2
object builds and two executable tests. Tests cover aliasing, nullable members,
ordinary retained callbacks, wrong signatures, legacy closure refusal, code
capability rejection, accepted work after capability close, manual graph
retirement, 20 OOM prefixes and 1000 warm calls with zero heap activity. Four raw
native threads execute 4,000 calls after the caller releases every group/closure
root and closes new code admission. Native publisher pins protect accepted
callbacks through the final thunk return; the final code pin count is zero.
These are real native threads, but the code owner is an instrumented pin owner,
not an actually unloaded DLL.

Six dependency-solver commands per OS verify ordinary/group source and domain
closures (11/12 sources, 5/6 domains in the fixture's direct roots). Separately,
complete O2 compiler/TCC/XRT/VFS builds pass on both OSes. Each passes 18 artifact
commands covering immutable object-byte hashes, JIT, actual executable entry
domain initializers, standalone programs and shared libraries invoked twice.
Standalone/shared imports require no xlang/TCC DLL. The CLI execution cohorts
assemble matching private SDK headers from the frozen source, because inherited
SDK defaults precede extra include paths. They do not alter the immutable build
or the workspace release tree. Fixed VFS objects remain protected from disk or
dynamic-mount replacement; the early old-VFS rejection is retained as evidence.
This qualifies component selection, not byte-for-byte machine-code pruning.

For the measured native sample (three slots, two non-null members), construction
uses four allocations and 320 live bytes: the group/vector, two member records
and one shared code lease. The earlier separate-lease prototype used six
allocations and 448 bytes. This excludes the preexisting closure and is not a
CPU-time or complete language-bridge equivalence claim. The actual language
scope may already hold a shareable adapter lease; it must be measured separately.

Before language delivery, the approved pack contract, IR/lowering and generated
thunks must be implemented and qualified. Real module unload, cold-thread OOM,
whole cyclic language/native graphs, HostSession renderer integration and matched
latest-XRT/native SDK/IDE convergence remain open. The full builds here still use
the previously qualified pinned XRT core; they are not a latest-XRT delivery.

Exact source/object/log hashes and limits are indexed by
`dev/release-materials/provenance/native-callback-group-foundation-20261116.json`.
The component/thread/full-artifact addendum is
`dev/release-materials/provenance/native-callback-group-isolation-20261116.json`.
Historical report bytes and paths are preserved.
