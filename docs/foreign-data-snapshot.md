# Native data snapshot inputs

`@c_data_snapshot(index)` marks one explicit `var` argument of a synchronous
native function. Indices omit the instance receiver, as with `@c_ref`.
It also applies when that argument supplies a `@c_pack_field(..., "value")`
descriptor field. Such a field is a borrowed `xvalue*`; it does not transfer
ownership. The native header must prove the exact pointer/function ABI.

All language arguments are evaluated once, in their ordinary order, before
snapshot preparation. Each marked argument gets one data graph traversal in
parameter order. Public stored class fields use the compiler's PUBLIC plan:
private fields, runtime storage, getters and serialization-specific renames or
omission rules do not enter the snapshot. Native containers use authenticated
resident providers and the same traversal memo. Repeated children keep their
sharing inside one snapshot; distinct marked arguments get separate traversals.
Immutable scalar leaves may retain their existing reference.
Nominal classes need an exact closed PUBLIC schema; an unknown class is
checked-rejected instead of using runtime reflection or exposing its storage.

The strict graph mode rejects raw pointers, unhandled native resources and
callables with a checked error. It never invokes an opaque Clone fallback.
Cycles and unsupported values fail without changing the source. The snapshots
remain alive through the native call and output conversion, and are released
in reverse parameter order on success or failure. If preparation fails, the
native function is not entered. After native entry, explicitly retained native
references and other native side effects follow the native API's own contract.

The native producer may still declare `@no_raise`; the snapshot admission can
allocate or fail, so its logical language call is raising. `@pure` is refused.
Plain and snapshot declarations of the same native symbol retain independent
logical admissions with the same physical pointer ABI. Use the explicit native
Shared entry for model defaults; calling a DeepClone entry after a snapshot
would copy the data again. Native Clone/identity-sharing APIs stay advanced.

The compiler stores only sparse declaration/parameter/type rows and consumes
one previously unused byte of the existing 88-byte foreign fact record. The
PUBLIC plan is built and verified by IR; C emission does not inspect source
visibility. Direct calls and stored/erased or bound function adapters share the
same preparation and cleanup emitter. Unused targets do not select a PUBLIC
plan or its resident providers.

This conversion has deliberate cost: graph traversal and container allocations,
schema validation, native container admission/boxing where needed, provider
selection, and owned cleanup. It adds no per-method native forwarding function.
It is not a claim of zero total bridge overhead. The dedicated language and IR
mutation fixtures qualify the contract separately from final SDK/IDE delivery.

Windows and Linux complete O2 compiler builds passed with the new XRT804
strict-data implementation and regenerated TCC/VFS inputs. The 54 language
gate commands cover CLI JIT, self-contained TCC EXE/shared modules, C/C++ header
proofs, GCC O0/O2 execution, nine rejected declarations per platform, unused
target pruning and the existing public-field/template regression. Shared
modules passed checked shutdown and physical unload; Windows also performs
the terminal runtime retirement before FreeLibrary.

The cases include null, embedded NUL text/bytes, Array/List/Set/Dict, inherited
public fields and serialization overrides, opaque-resource refusal without
Clone, temporary-resource Drop, source evaluation order and isolation, owned
returned snapshots, checked native failure after entry, and the complete
eight-position two-input allocation failure prefix in each execution mode.
Two platform IR gates reject 93 mutations each and confirm 88-byte facts.
Windows uses the matching frozen frontend sources with the actual O2 runtime;
Linux also exercises the actual full O2 compiler ELF entries directly.
The source/provenance update does not install the final native SDK or IDE.
