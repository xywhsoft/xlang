# Process module global native handle admission — pending

`native_globals.xl` preserves the B32 real-provider reproducer. It is not an
accepted regression or a released behavior guarantee. Generate it with the
current `xlir_code_owner_binding_contract` source emitter (`--script`), derive
the module-level no-argument async entry from verified IR, and run the B32
provider/host protocol without the scoped Process factory. The native child
must receive real stdin EOF; both futures and any retained status must be
released before observing retirement. Never force FreeLibrary/tcc_delete.

Windows B32 probes win1/win2 observed both finally duties and completed native
waiter retirement, but actual module collection stayed deferred after every
external Future/status owner was dropped: `leases=2`, `active=0`, `phase=0`,
`prepared=1`, no plan and no latched semantic error. The remaining module
globals own ProcessCommand and Process objects. Their generated native fields
use private `xvaluehandleops` with generated Drop callbacks. XRT's default
`xrtValueOwnershipAdapterV1` admits only its known weak-handle policy, not these
unknown owning handles. This refusal is necessary; deleting the module would
leave callbacks pointing into unloaded code.

Required core work before claiming global-native provider acceptance:

- Add an explicit native resource ownership/lifecycle contract to declarations
  and typed IR; verify it rather than inferring leaf status from class names,
  destructor names, purity, no-raise or nonblocking effects.
- Reuse a resident native-handle capability implementation with exact policy
  identity, real code-lifetime ownership, traceable owned script edges (if any),
  and independently defined physical terminal duties. Process command/live/
  status payloads have no script-owning edges, but that does not certify other
  stdlib handles with callbacks, values, cursors or async activity.
- Admit only known validated policy identities before Count/Trace. Preserve
  refusal of arbitrary foreign handles and unknown native children. Keep joins
  and generated Drop outside the ownership freeze/API mutex.
- Verify actual DLL/TCC/ELF unloading with global native classes, escaped
  statuses, unknown-policy rejection, failure rollback, races and exact ledgers.

The passing B32 `xlir_process_future_module_lifetime.xl` intentionally has no
owning module globals: the host creates one Process under a real entry lease,
starts two verified canonical async entries with that same Process, drops its
own Process reference, then requests module destruction. This validates scoped
Future/class code residency but does not resolve the global admission gap.
