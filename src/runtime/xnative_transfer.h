#ifndef XNATIVE_TRANSFER_H
#define XNATIVE_TRANSFER_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Independent portable ABI. Its visitor is type-compatible with XRT's public
 * visitor without making the resident runtime depend on the whole XGE SDK. */
struct xrtownershipref;
typedef bool (*xn_transfer_ownership_visitor)(struct xrtownershipref,void*);

#define XN_TRANSFER_SERVICES_V1_TAG UINT64_C(0x5847455452535631)
#define XN_TRANSFER_NATIVE_OWNER_V1_TAG UINT64_C(0x58474554524E5631)
#define XN_TRANSFER_PROCESS_RESIDENT UINT32_C(1)

/* Mandatory v1 provider promise: every output COM interface and every output
 * medium cleanup pointer is OS-owned code or explicitly process-resident.
 * v1 does not cover arbitrary third-party DLL child interfaces. Such outputs
 * require their own resident carrier/code capability; never infer residency
 * from an address. The default XGE producer meets this promise: GetData uses
 * HGLOBAL with pUnkForRelease=NULL, EnumFormatEtc uses the OS enumerator, and
 * advice interfaces are unsupported. Supplying this v1 descriptor explicitly
 * asserts the promise; the wrapper cannot discover it by inspecting a vtable.
 *
 * Unique Native implementation: exactly one interface-owning duty is
 * transferred on successful wrapping. The implementation must not escape
 * separately. Trace expands its actual owning resource slots; it is not an
 * invented Count=1 shared graph node. Ready is pure/no-allocation/no-diagnostic.
 * Release consumes that duty iff *consumed=true, independently of status.
 * Zero is success; positive 1 is pure unconsumed wait; negative is a checked
 * failure, including NULL diagnostics. Existing resident retirement retains
 * the real carrier/code on wait; no Native queue is introduced. */
typedef struct xn_transfer_native_owner_v1 {
    size_t iSize;
    uint64_t iAbiTag;
    uint64_t iCreatorThread;
    bool (*onReady)(const void* pNativeInterface);
    bool (*onTrace)(const void* pNativeInterface,xn_transfer_ownership_visitor visit,void* pUser);
    int (*onReleaseOwnerChecked)(void* pNativeInterface,bool* pConsumed);
} xn_transfer_native_owner_v1;

/* Borrowed synchronously at a cold export/Run boundary, never weak-stored on
 * a DataObject. Three separate factories create exact resident COM vtables,
 * not an opcode/FFI dispatcher. void* is only an opaque COM object pointer at
 * this portable factory boundary, never an incompatible function-pointer cast.
 *
 * Success transfers the Native duty once and retains one independent real
 * code lease in the resident carrier. Failure consumes neither the Native
 * duty nor the caller capability, and writes NULL output. Every forwarded
 * method holds a resident method pin through Native return. Final retirement
 * destroys the implementation before releasing its code capability.
 *
 * Without PROCESS_RESIDENT, pCodeCapability must be an actual resident lease.
 * The flag is an explicit Native C-host assertion, never inferred from NULL.
 * pOwnershipDomain is a borrowed exact XL6_OwnershipDomainV1* in the resident
 * implementation; Native code only passes it through. Its immutable code and
 * metadata remain readable under the retained capability. It resolves actual
 * children seen by onTrace, including typed finalization where applicable. */
typedef struct xn_transfer_services_v1 {
    size_t iSize;
    uint64_t iAbiTag;
    uint32_t iFlags;
    const void* pCodeCapability;
    const void* pOwnershipDomain;
    int (*wrapDataObject)(const struct xn_transfer_services_v1* services,
        void* pOwnedNativeIDataObject,const xn_transfer_native_owner_v1* nativeOwner,
        void** pResidentIDataObject);
    int (*wrapDropSource)(const struct xn_transfer_services_v1* services,
        void* pOwnedNativeIDropSource,const xn_transfer_native_owner_v1* nativeOwner,
        void** pResidentIDropSource);
    int (*wrapDropTarget)(const struct xn_transfer_services_v1* services,
        void* pOwnedNativeIDropTarget,const xn_transfer_native_owner_v1* nativeOwner,
        void** pResidentIDropTarget);
} xn_transfer_services_v1;

#ifdef __cplusplus
}
#endif
#endif
