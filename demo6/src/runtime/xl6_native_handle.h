#ifndef XL6_NATIVE_HANDLE_H
#define XL6_NATIVE_HANDLE_H
#include "xl6_code_lease.h"
#include <stddef.h>
#include <stdlib.h>

struct xvalue;
struct xvaluehandleops;
struct xrtownershipref;
struct xrtownershipadapterv1;
struct xrtownershipfinishv1;

/* Immutable, resident resource declaration. These identities are not a
 * universal handle certificate: the host admits each audited policy by exact
 * Ops AND Trace identity, before running any user-supplied graph callback.
 * Payload callbacks and every transitive native dependency must stay resident.
 * Trace describes ALL owning language edges; it does not authorize children.
 * No policy or generated module address is retained as handle UserData. */
typedef struct XL6_NativeHandlePolicyV1 {
    size_t size;
    uint64_t abiTag;
    const struct xvaluehandleops* Ops;
    bool (*Trace)(const struct xvalue*,
        bool (*)(struct xrtownershipref, void*), void*);
} XL6_NativeHandlePolicyV1;
#define XL6_NATIVE_HANDLE_POLICY_V1_TAG UINT64_C(0x584C364E484E4431)

/* A valid resident policy consumes and clears the independent private slot on
 * both success and allocation/bind failure. Rollback retires its real payload
 * exactly once and preserves the first failure, even without a diagnostic.
 * Invalid slot/policy is a caller contract error and leaves the slot unchanged
 * (there is no trusted destructor with which to consume an unknown policy).
 * No enclosing ownership mutation may span this call's arbitrary cleanup. */
XL6_CODE_LEASE_API struct xvalue* xl6NativeHandleTakeV1(
    void** slot, const XL6_NativeHandlePolicyV1* policy);
XL6_CODE_LEASE_API bool xl6NativeHandleTraceLeaf(const struct xvalue*,
    bool (*)(struct xrtownershipref, void*), void*);

/* Commands own text/config/duplicated OS capabilities; statuses own plain data.
 * Live Process is NOT a leaf: its shared wrapper owns one XRT process user
 * endpoint, which can own a shared Future, Promise and error. The native waiter
 * holds its own real process reference. Each physical node has independent
 * lifecycle admission; native resource retirement runs outside graph Freeze.
 * No generated callback is retained; that alone does not certify a leaf.
 * This list does NOT certify custom IO, arbitrary extensions or other handles. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessCommandHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessHandlePolicyV1Get(void);
/* Managed wrapper, native group, endpoints and owned observers are separate
 * physical nodes; this handle is not a leaf and certifies no transitive node. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TaskGroupHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TaskGroupSnapshotHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PromiseHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ChannelCaseHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ChannelSelectionHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessStatusHandlePolicyV1Get(void);
XL6_CODE_LEASE_API struct xrtownershipref xl6ProcessLiveOwnershipV1(const void*);
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ProcessLiveOwnershipAdapterV1(struct xrtownershipref);
XL6_CODE_LEASE_API const struct xrtownershipfinishv1* xl6ProcessLiveOwnershipFinishV1Get(void);
/* A snapshot owns copied bytes and managed creator identity, never its source
 * Buffer or an owning language edge. The retained native thread's Data is a
 * borrowed callback argument, not a transitive language owner. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6BufferIteratorHandlePolicyV1Get(void);
/* The actual XRT buffer contains Data/Size/Capacity, not language values. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6BufferHandlePolicyV1Get(void);
/* XL_Context owns only native configuration/OS libraries. This does not
 * certify XLC_Context, XlangHandler, borrowed callback data or extensions. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6XlangContextHandlePolicyV1Get(void);

/* Prototypes are physical SDK declarations. The .inc export table belongs
 * to resident source units, never to a generated header's sibling closure. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6DateTimeHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetAddressHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetAddressListHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetResolverConfigHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetResolverSnapshotHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetResolverHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetResolveOperationHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetInterfacesHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetInterfaceHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetInterfaceAddressHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UnicodeValidatorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TextIteratorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6DeflaterHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6InflaterHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TemplateHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TemplateCallHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TemplateEngineGuardHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternMatchHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternBuilderHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternEditHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternKeyHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessPipelineHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PipelineResultHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessStageHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessResultHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexMatchHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexIteratorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexSplitHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexSplitPartHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexSetHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexReplacerHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6SignalQueueHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6SignalEventHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternPublicationHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessRunOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RandomStateHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsFileHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6VfsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6VfsMountHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6VfsMemoryHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6VfsDiskHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6VfsPackHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PackOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PackStatsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsInfoHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsDirHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6IoReaderHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6IoWriterHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6IoLinesHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsMapHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsLockHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsRootHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoMd5HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoSha1HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoSha224HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoSha256HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoSha384HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoSha512HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoSha512_256HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoHmacSha256HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoHmacSha384HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoHmacSha512HandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoAesGcmHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoChaChaAeadHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoX25519HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509CrlEntryHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509PathCriticalHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509PathOutcomeHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509PathHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509PathPolicyHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509AnchorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509CrlCriticalHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509RevocationCheckHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509RevocationHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509CrlSetHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509ValidatedCrlHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509CrlEntriesHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509IssuingDistributionPointHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509CertificateHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsIdentityHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsLimitsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsStreamOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsPolicyHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsParametersHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsVerificationPolicyHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsVerifierHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsClientContextHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsServerContextHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TlsSessionHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509CrlHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509NameHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509GeneralNamesHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509GeneralNameHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509OidsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509KeyUsageHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509BasicConstraintsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509AuthorityKeyIdentifierHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509DistributionPointsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509DistributionPointHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509NameConstraintsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509GeneralSubtreesHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509GeneralSubtreeHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509AttributeHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509ExtensionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509ExtensionHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509AlgorithmHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509SignatureInfoHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509PublicKeyInfoHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoX448HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoEd25519HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoP256HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoP384HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoRsaPublicHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CryptoRsaPrivateHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6DerValueHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6DerReaderHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6DerBuilderHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PemBlockHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PemReaderHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509StoreHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6X509StoreSnapshotHandlePolicyV1Get(void);

XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6AtomicInt32HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6AtomicUInt32HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6AtomicInt64HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6AtomicUInt64HandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6AtomicBoolHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6OnceHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ExecutorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetEngineHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetSocketHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetEngineConfigHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetEngineSnapshotHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TaskPoolHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TaskPoolSnapshotHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ExecutorSnapshotHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6EventHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ThreadLocalHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CancellationSourceHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CancellationTokenHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6CancellationRegistrationHandlePolicyV1Get(void);
/* Used only in audited resident implementation units, with complete XRT types.
 * Keep the same explicit error boundary as a generated native destructor; a
 * callback must propagate cleanup failure without replacing an earlier one. */
#define XL6_DEFINE_NATIVE_HANDLE_POLICY(Getter, Destroy, TraceCallback) \
    static void Getter##Drop(void* payload, void* userData) { \
        xl6_error_scope cleanup = {0}; xl6_error_result outcome; \
        (void)userData; \
        if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort(); \
        Destroy(payload); \
        outcome = xl6ErrorScopeEnd(&cleanup); \
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome); \
    } \
    XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* Getter(void) { \
        static const struct xvaluehandleops ops = {NULL, Getter##Drop, NULL, NULL}; \
        static const XL6_NativeHandlePolicyV1 policy = {sizeof(policy), \
            XL6_NATIVE_HANDLE_POLICY_V1_TAG, &ops, TraceCallback}; \
        return &policy; \
    }
#define XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(Getter, Destroy) \
    XL6_DEFINE_NATIVE_HANDLE_POLICY(Getter, Destroy, xl6NativeHandleTraceLeaf)
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetDatagramInfoHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetDatagramControlHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetDatagramErrorInfoHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetDatagramPacketHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetBatchProgressHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetBufferHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetBytesHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpConfigHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpPacketHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpErrorPacketHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpBatchHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpStatsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UdpMessageHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetFrameResultHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetLineFramerHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetLengthFramerHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetProxyHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetProxyHandshakeHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpStreamOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpDialOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpProxyDialOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpListenOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpStreamHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6NetFileHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpListenerHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpServerHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpServerOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpServerStatsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpStreamStatsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TcpListenerStatsHandlePolicyV1Get(void);
#endif
