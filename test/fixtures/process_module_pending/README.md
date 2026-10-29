# Process module global native handle admission — B32 diagnosis / B33 resolution

The former `native_globals.xl` reproducer is now the accepted regression
`test/cases/xlir_process_future_module_globals.xl`. This directory retains the
B32 diagnosis; it is not a blanket approval of other native policies. Generate
the case with `xlir_code_owner_binding_contract` (`--script`), derive
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

Implemented B33 contract and required acceptance boundaries:

- Add an explicit native resource ownership/lifecycle contract to declarations
  and typed IR; verify it rather than inferring leaf status from class names,
  destructor names, purity, no-raise or nonblocking effects.
- Reuse a resident native-handle capability implementation with exact policy
  identity, real class code-lifetime ownership, traceable owned script edges (if any),
  and independently defined physical terminal duties. Native handle Ops and
  Trace reside in the runtime and need no generated code lease. Process command/live/
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

B33's resident Process command/live/status resource descriptors resolve this
gap with XRT's existing exact Handle Ops+Trace adapter and phased physical Drop.
Typed class fields and lowered destruction targets carry independently verified
closed policy metadata; Cgen emits only the resident Take and declared getter.
Unknown policies still refuse before Trace. The provider gates run both scoped
and owning-global variants, including a raw status handle read/released AFTER
actual provider unmapping, native boxing OOM and 29 corrupt-IR rejections.
See `docs/三组标准库接入长期任务.md` for frozen acceptance evidence and scope.
