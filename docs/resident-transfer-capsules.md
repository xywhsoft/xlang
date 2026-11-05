# Resident COM transfer capsules

The portable `xn_transfer_services_v1` ABI exports an accepted code capability
to a native publisher. `xl6TransferServicesFromLeaseV1` requires a non-NULL real
CodeLease; `xl6TransferServicesProcessResidentV1` is a separate, explicit C-host
promise. Both construct borrowed services on Windows and Linux. Linux's three
wrap functions return checked unsupported, clear the output and consume nothing.
The services and domain are borrowed only for the synchronous wrapping call.

Windows implements the complete typed IDataObject, IDropSource and IDropTarget
vtables. QueryInterface recognizes IUnknown and the capsule's exact interface;
it never forwards QueryInterface or publishes the native implementation pointer.
Each of the 15 forwarded methods pins the COM object and enters a ForeignView
call until after the native method returns. IUnknown's identity/reference methods
are resident metadata operations and can run on other threads. Other methods
reject the wrong creator thread before reading mutable native payloads.

The capsule consumes one unique implementation-owning duty only after successful
ForeignView creation. It copies the native owner descriptor and retains a child
of the accepted CodeLease, without reopening host acquisition. Its ForeignView
uses the exact supplied ownership domain; TraceRaw expands the implementation's
actual owning slots. The capsule is not an invented Count=1 native graph node.

COM's last Release releases one ForeignView external owner. It does not free
the capsule. Existing checked creator retirement retains the implementation,
descriptor and code while Ready is false or checked release returns deferred.
Only `consumed=true` frees the resident capsule; a negative cleanup status is
reported independently, even with a NULL diagnostic. Native methods may release
the last external COM reference: their internal pin keeps the entire native
return tail resident. No native retirement queue or new cleanup scheduler exists.

The real COM reference count is 64-bit. ULONG AddRef/Release results saturate at
ULONG_MAX and remain diagnostic; a 32-bit result cannot refuse an acquired
reference. A genuine 64-bit overflow is a contract violation, as for CodeLease.

This v1 contract supports providers whose output interfaces and medium cleanup
code are OS-owned or explicitly process-resident. It does not wrap arbitrary
third-party DLL child interfaces or infer code residency from addresses. The
default XGE provider returns HGLOBAL with NULL pUnkForRelease, an OS format
enumerator, and unsupported advice interfaces. A different provider asserting
v1 must satisfy the same output-residency promise. A future nonresident child
interface requires its own independent owning carrier.

COM business HRESULT failures remain HRESULTs. An independently escaping checked
failure changes a successful HRESULT to E_FAIL and propagates the separate error
bit to the current checked scope or resident callback-failure mailbox. Existing
ambient diagnostics are isolated. On such a post-call checked failure, successful
GetData/enum outputs are released through their promised resident cleanup code.

## Costs and verification

On validated x64 Windows, warm wrapping allocates twice: 80 bytes for the typed
interface/common record and 464 bytes for its ForeignView, 544 payload bytes in
total. This excludes allocator metadata and one-time execution-context setup.
The service record is 64 bytes and the copied native owner is 48 bytes. Continuing
an existing CodeLease adds a real retained reference without a new host acquire.
No native payload or transferred medium is copied by the capsule.

Every forwarded call adds real resident reference/scope/error-boundary work and
one native vtable call. It adds no warmed XRT allocation, payload copy or owning
graph scan. This is a safety bridge with measured bookkeeping, not a claim of
zero total overhead or a throughput benchmark.

`tools/check_native_com_capsules.py` rebuilds only ForeignView and the new capsule
TU over an explicitly verified parent build. Windows tests exercise every typed
method, fixed QI, NULL checked failures, ambient isolation, cold OOM, real graph
slot tracing, concurrent references and the 32-bit counter boundary. A separate
real DLL supplies the native implementation/vtable/descriptors; code remains
loaded after the original LoadLibrary and parent lease are released, through
worker last Release and native wait, and unloads after creator checked retry.
The DLL also releases the last external COM reference inside a method and writes
its native tail marker before unloading. No OLE initialization, OS clipboard,
drag loop, browser, window or device is used. Test metadata hooks are compiled
only with XL6_NATIVE_COM_TEST and are absent from the production component.
