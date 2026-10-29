#ifndef XL6_TCP_SERVER_ABI_H
#define XL6_TCP_SERVER_ABI_H
#include "xl6_runtime_services.h"
#include "xl6_native_class_abi.h"
XL6_TEXT_API void* xl6TcpServerOptionsCreateV1(void);
XL6_TEXT_API void* xl6TcpServerOptionsCreateListenV1(const void* a0);
XL6_TEXT_API void xl6TcpServerOptionsDestroyV1(void* a0);
XL6_TEXT_API uint64_t xl6TcpServerOptionsEndpointCountV1(const void* a0);
XL6_TEXT_API int64_t xl6TcpServerOptionsModeV1(const void* a0);
XL6_TEXT_API bool xl6TcpServerOptionsSharedPortV1(const void* a0);
XL6_TEXT_API int64_t xl6TcpServerOptionsAcceptQueueLimitV1(const void* a0);
XL6_TEXT_API void* xl6TcpServerOptionsEndpointV1(const void* a0,int64_t a1);
XL6_TEXT_API void* xl6TcpServerOptionsWithEndpointV1(const void* a0,int64_t a1,const void* a2);
XL6_TEXT_API void* xl6TcpServerOptionsWithAddedEndpointV1(const void* a0,const void* a1);
XL6_TEXT_API void* xl6TcpServerOptionsWithoutEndpointV1(const void* a0,int64_t a1);
XL6_TEXT_API void* xl6TcpServerOptionsWithModeV1(const void* a0,int64_t a1);
XL6_TEXT_API void* xl6TcpServerOptionsWithSharedPortV1(const void* a0,bool a1);
XL6_TEXT_API void* xl6TcpServerOptionsWithAcceptQueueLimitV1(const void* a0,int64_t a1);
XL6_TEXT_API void* xl6TcpServerCreateV1(XL6_ExecutionServicesV1 services,void* a0,const void* a1);
XL6_TEXT_API void xl6TcpServerDestroyV1(void* a0);
XL6_TEXT_API void xl6TcpServerCloseV1(void* a0);
XL6_TEXT_API int64_t xl6TcpServerStateV1(void* a0);
XL6_TEXT_API uint64_t xl6TcpServerEndpointCountV1(void* a0);
XL6_TEXT_API uint64_t xl6TcpServerListenerCountV1(void* a0);
XL6_TEXT_API void* xl6TcpServerLocalV1(void* a0,int64_t a1);
XL6_TEXT_API void* xl6TcpServerPrepareAcceptV1(XL6_ExecutionServicesV1 services,void* a0);
XL6_TEXT_API struct xfuture* xl6TcpServerAcceptStartV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* a0,const xvalue* a1);
XL6_TEXT_API struct xfuture* xl6TcpServerAcceptStartCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* a0,const xvalue* a1,void* a2);
XL6_TEXT_API struct xfuture* xl6TcpServerAcceptStartUntilV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* a0,const xvalue* a1,uint64_t a2);
XL6_TEXT_API struct xfuture* xl6TcpServerAcceptStartUntilCancelV1(XL6_RuntimeServicesV1 services,XL6_NativeClassFieldContextV1 context,void* a0,const xvalue* a1,uint64_t a2,void* a3);
XL6_TEXT_API void* xl6TcpServerAcceptV1(XL6_ExecutionServicesV1 services,void* a0,int64_t a1);
XL6_TEXT_API void* xl6TcpServerTryAcceptV1(XL6_ExecutionServicesV1 services,void* a0);
XL6_TEXT_API void* xl6TcpServerAcceptCancelV1(XL6_ExecutionServicesV1 services,void* a0,void* a1,int64_t a2);
XL6_TEXT_API void* xl6TcpServerAcceptUntilV1(XL6_ExecutionServicesV1 services,void* a0,uint64_t a1);
XL6_TEXT_API void* xl6TcpServerAcceptUntilCancelV1(XL6_ExecutionServicesV1 services,void* a0,uint64_t a1,void* a2);
XL6_TEXT_API void* xl6TcpServerSnapshotV1(void* a0);
XL6_TEXT_API void xl6TcpServerStatsDestroyV1(void* a0);
XL6_TEXT_API int64_t xl6TcpServerStatsStateV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsAcceptedV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsRejectedV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsErrorsV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsEndpointsV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsListenersV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsClosedListenersV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsQueuedAcceptsV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsPeakQueuedAcceptsV1(const void* a0);
XL6_TEXT_API uint64_t xl6TcpServerStatsAcceptWaitersV1(const void* a0);
#endif
