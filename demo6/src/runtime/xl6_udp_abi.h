#ifndef XL6_UDP_ABI_H
#define XL6_UDP_ABI_H
XL6_TEXT_API void* xl6UdpBindDefaultV1(XL6_ExecutionServicesV1,void*,const void*);
XL6_TEXT_API void* xl6UdpBindV1(XL6_ExecutionServicesV1,void*,const void*,const void*,uint64_t);
XL6_TEXT_API void* xl6UdpConnectDefaultV1(XL6_ExecutionServicesV1,void*,const void*);
XL6_TEXT_API void* xl6UdpConnectV1(XL6_ExecutionServicesV1,void*,const void*,const void*,uint64_t);
XL6_TEXT_API void* xl6UdpOpenV1(XL6_ExecutionServicesV1,void*,const void*,const void*,const void*,uint64_t);
XL6_TEXT_API void xl6UdpDestroyV1(void*);
XL6_TEXT_API void xl6UdpCloseV1(void*);
XL6_TEXT_API void xl6UdpAbortV1(void*);
XL6_TEXT_API int64_t xl6UdpStateV1(void*);
XL6_TEXT_API bool xl6UdpConnectedV1(void*);
XL6_TEXT_API int64_t xl6UdpSendControlAvailableV1(void*);
XL6_TEXT_API void* xl6UdpLocalV1(void*);
XL6_TEXT_API void* xl6UdpPeerV1(void*);
XL6_TEXT_API void* xl6UdpTryReceiveV1(void*);
XL6_TEXT_API void* xl6UdpTryReceiveErrorV1(void*);
XL6_TEXT_API void xl6UdpPacketDestroyV1(void*);
XL6_TEXT_API uint64_t xl6UdpPacketLengthV1(const void*);
XL6_TEXT_API bool xl6UdpPacketTruncatedV1(const void*);
XL6_TEXT_API xvalue* xl6UdpPacketBytesV1(const void*,int64_t,int64_t);
XL6_TEXT_API void* xl6UdpPacketRemoteV1(const void*);
XL6_TEXT_API void* xl6UdpPacketInfoV1(const void*);
XL6_TEXT_API void xl6UdpErrorPacketDestroyV1(void*);
XL6_TEXT_API uint64_t xl6UdpErrorPacketLengthV1(const void*);
XL6_TEXT_API xvalue* xl6UdpErrorPacketBytesV1(const void*,int64_t,int64_t);
XL6_TEXT_API void* xl6UdpErrorPacketInfoV1(const void*);
XL6_TEXT_API void xl6UdpBatchDestroyV1(void*);
XL6_TEXT_API uint64_t xl6UdpBatchCountV1(const void*);
XL6_TEXT_API void* xl6UdpBatchTakeV1(void*,int64_t);
XL6_TEXT_API void* xl6UdpSnapshotV1(void*);
XL6_TEXT_API void xl6UdpStatsDestroyV1(void*);
XL6_TEXT_API int64_t xl6UdpStatsStateV1(const void*);
XL6_TEXT_API bool xl6UdpStatsConnectedV1(const void*);
#define XL6_UDP_STAT(Field,Name) XL6_TEXT_API uint64_t xl6UdpStats##Field##V1(const void*);
#include "xl6_udp_stats_fields.h"
#undef XL6_UDP_STAT
#define XL6_UDP_SEND(Name,Type) \
XL6_TEXT_API int64_t xl6UdpSend##Name##V1(void*,Type,int64_t); \
XL6_TEXT_API int64_t xl6UdpSendTo##Name##V1(void*,const void*,Type,int64_t); \
XL6_TEXT_API int64_t xl6UdpSendMessage##Name##V1(void*,const void*,const void*,Type,int64_t); \
XL6_TEXT_API int64_t xl6UdpSendConnectedMessage##Name##V1(void*,const void*,Type,int64_t);
XL6_UDP_SEND(Text,xstrview)
XL6_UDP_SEND(Bytes,const xvalue*)
XL6_UDP_SEND(Buffer,void*)
#undef XL6_UDP_SEND
XL6_TEXT_API int64_t xl6UdpSendMoveV1(void*,void*);
XL6_TEXT_API int64_t xl6UdpSendMoveToV1(void*,const void*,void*);
XL6_TEXT_API int64_t xl6UdpSendMoveMessageV1(void*,const void*,const void*,void*);
XL6_TEXT_API int64_t xl6UdpSendMoveConnectedMessageV1(void*,const void*,void*);
#define XL6_UDP_REF(Name) \
XL6_TEXT_API int64_t xl6UdpSendRef##Name##V1(void*,void*,int64_t,int64_t); \
XL6_TEXT_API int64_t xl6UdpSendRefTo##Name##V1(void*,const void*,void*,int64_t,int64_t); \
XL6_TEXT_API int64_t xl6UdpSendRefMessage##Name##V1(void*,const void*,const void*,void*,int64_t,int64_t); \
XL6_TEXT_API int64_t xl6UdpSendRefConnectedMessage##Name##V1(void*,const void*,void*,int64_t,int64_t);
XL6_UDP_REF(Bytes)
XL6_UDP_REF(Packet)
#undef XL6_UDP_REF
#define XL6_UDP_WAIT(Name) \
XL6_TEXT_API int64_t xl6Udp##Name##V1(void*,int64_t,int64_t); \
XL6_TEXT_API int64_t xl6Udp##Name##CancelV1(void*,void*,int64_t,int64_t); \
XL6_TEXT_API int64_t xl6Udp##Name##UntilV1(void*,uint64_t,int64_t); \
XL6_TEXT_API int64_t xl6Udp##Name##UntilCancelV1(void*,uint64_t,void*,int64_t);
XL6_UDP_WAIT(Wait)
XL6_UDP_WAIT(WaitWritable)
#undef XL6_UDP_WAIT
#define XL6_UDP_RECEIVE(Name) \
XL6_TEXT_API void* xl6Udp##Name##V1(void*,int64_t); \
XL6_TEXT_API void* xl6Udp##Name##CancelV1(void*,void*,int64_t); \
XL6_TEXT_API void* xl6Udp##Name##UntilV1(void*,uint64_t); \
XL6_TEXT_API void* xl6Udp##Name##UntilCancelV1(void*,uint64_t,void*);
XL6_UDP_RECEIVE(Receive)
XL6_UDP_RECEIVE(ReceiveError)
#undef XL6_UDP_RECEIVE
XL6_TEXT_API void* xl6UdpReceiveBatchV1(void*,int64_t,int64_t);
XL6_TEXT_API void* xl6UdpReceiveBatchCancelV1(void*,void*,int64_t,int64_t);
XL6_TEXT_API void* xl6UdpReceiveBatchUntilV1(void*,uint64_t,int64_t);
XL6_TEXT_API void* xl6UdpReceiveBatchUntilCancelV1(void*,uint64_t,void*,int64_t);
#endif
