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
callback error protocols. The runtime symbol visitor exports the new getter;
the artifact symbol directory records its callback support dependency.

The qualified development gates use a complete, hashed public-source cohort,
then explicitly replace only its native-callback and runtime-symbol objects.
Windows and Linux each pass 11 commands: C11/C++11/C++17 headers plus O0/O2
object builds and executable tests. Tests cover aliasing, nullable members,
ordinary retained callbacks, wrong signatures, legacy closure refusal, code
capability rejection, accepted work after capability close, manual graph
retirement, 20 OOM prefixes and 1000 warm calls with zero heap activity.

For the measured native sample (three slots, two non-null members), construction
uses four allocations and 320 live bytes: the group/vector, two member records
and one shared code lease. The earlier separate-lease prototype used six
allocations and 448 bytes. This excludes the preexisting closure and is not a
CPU-time or complete language-bridge equivalence claim. The actual language
scope may already hold a shareable adapter lease; it must be measured separately.

Before language delivery, the group component/domain must be isolated for
pruning, and the approved pack contract must be implemented and qualified.
Real module unload, concurrent/foreign-thread callback entry, whole cyclic
language/native graphs, full compiler/VFS products and matched latest-XRT/native
SDK/IDE convergence are not certified by this source-only foundation test.

Exact source/object/log hashes and limits are indexed by
`dev/release-materials/provenance/native-callback-group-foundation-20261116.json`.
