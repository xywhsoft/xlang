#ifndef XL6_TLS_METADATA_ABI_H
#define XL6_TLS_METADATA_ABI_H
#include "xl6_tls_io_abi.h"
XL6_TEXT_API void* xl6TlsStreamSnapshotV1(void*,int64_t);
XL6_TEXT_API xfuture* xl6TlsStreamSnapshotAsyncV1(XL6_NativeClassResultContextV1,void*);
XL6_TEXT_API void* xl6TlsListenerSnapshotV1(void*);
XL6_TEXT_API void xl6TlsInfoDestroyV1(void*);
XL6_TEXT_API void xl6TlsServerStatsDestroyV1(void*);
XL6_TEXT_API int64_t xl6TlsInfoVersionV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoCipherV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoCertificateCountV1(const void*);
XL6_TEXT_API void* xl6TlsInfoCertificateAtV1(const void*,int64_t);
XL6_TEXT_API int64_t xl6TlsServerStatsStateV1(const void*);
/* Declaration-only SDK: do not depend on implementation field tables. */
XL6_TEXT_API int64_t xl6TlsInfoRoleV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoStateV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoSessionStateV1(const void*);
XL6_TEXT_API bool xl6TlsInfoReadyV1(const void*);
XL6_TEXT_API bool xl6TlsInfoHasSelectionV1(const void*);
XL6_TEXT_API bool xl6TlsInfoResumedV1(const void*);
XL6_TEXT_API uint64_t xl6TlsInfoAvailableV1(const void*);
XL6_TEXT_API uint64_t xl6TlsInfoPendingV1(const void*);
XL6_TEXT_API uint64_t xl6TlsInfoAsyncBytesV1(const void*);
XL6_TEXT_API uint64_t xl6TlsInfoAsyncCountV1(const void*);
XL6_TEXT_API bool xl6TlsInfoHasServerCookieV1(const void*);
XL6_TEXT_API uint64_t xl6TlsInfoServerCookieV1(const void*);
XL6_TEXT_API bool xl6TlsInfoHasPeerAlertV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoPeerAlertLevelV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoPeerAlertCodeV1(const void*);
XL6_TEXT_API bool xl6TlsInfoHasErrorV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoErrorKindV1(const void*);
XL6_TEXT_API int64_t xl6TlsInfoErrorCodeV1(const void*);
XL6_TEXT_API xvalue* xl6TlsInfoProtocolV1(const void*);
XL6_TEXT_API xvalue* xl6TlsInfoServerNameV1(const void*);
XL6_TEXT_API xstrview xl6TlsInfoErrorDomainV1(const void*);
XL6_TEXT_API xstrview xl6TlsInfoErrorOperationV1(const void*);
XL6_TEXT_API xstrview xl6TlsInfoErrorMessageV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsHandshakesV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsAcceptedV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsRejectedV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsHandshakeErrorsV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsActiveHandshakesV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsPeakHandshakesV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsQueuedAcceptsV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsPeakQueuedAcceptsV1(const void*);
XL6_TEXT_API uint64_t xl6TlsServerStatsAcceptWaitersV1(const void*);
#endif
