#ifndef XL6_UDP_CONFIG_ABI_H
#define XL6_UDP_CONFIG_ABI_H
XL6_TEXT_API void* xl6UdpConfigCreateV1(void);
XL6_TEXT_API void xl6UdpConfigDestroyV1(void*);
#define XL6_UDP_CONFIG_SIZE(Field,Name) XL6_TEXT_API uint64_t xl6UdpConfig##Field##V1(const void*);
#define XL6_UDP_CONFIG_UINT(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
#define XL6_UDP_CONFIG_INT(Field,Name) XL6_TEXT_API int64_t xl6UdpConfig##Field##V1(const void*);
#define XL6_UDP_CONFIG_BOOL(Field,Name) XL6_TEXT_API bool xl6UdpConfig##Field##V1(const void*);
#include "xl6_udp_config_fields.h"
#undef XL6_UDP_CONFIG_SIZE
#undef XL6_UDP_CONFIG_UINT
#undef XL6_UDP_CONFIG_INT
#undef XL6_UDP_CONFIG_BOOL
XL6_TEXT_API int64_t xl6UdpConfigReceiveMetadataV1(const void*);
XL6_TEXT_API int64_t xl6UdpConfigOverflowV1(const void*);
XL6_TEXT_API int64_t xl6UdpConfigTruncationV1(const void*);
XL6_TEXT_API int64_t xl6UdpConfigErrorOverflowV1(const void*);
XL6_TEXT_API int64_t xl6UdpConfigPathMtuV1(const void*);
XL6_TEXT_API void* xl6UdpConfigWithReceiveV1(const void*,uint64_t,uint64_t,uint64_t);
XL6_TEXT_API void* xl6UdpConfigWithReceiveQueueV1(const void*,uint64_t,uint64_t,int64_t);
XL6_TEXT_API void* xl6UdpConfigWithReceiveMetadataV1(const void*,int64_t);
XL6_TEXT_API void* xl6UdpConfigWithTruncationV1(const void*,int64_t);
XL6_TEXT_API void* xl6UdpConfigWithSendQueueV1(const void*,uint64_t,uint64_t,uint64_t,uint64_t);
XL6_TEXT_API void* xl6UdpConfigWithSendConcurrencyV1(const void*,uint64_t);
XL6_TEXT_API void* xl6UdpConfigWithSocketBuffersV1(const void*,int64_t,int64_t);
XL6_TEXT_API void* xl6UdpConfigWithTrafficV1(const void*,int64_t,int64_t);
XL6_TEXT_API void* xl6UdpConfigWithPathMtuV1(const void*,int64_t);
XL6_TEXT_API void* xl6UdpConfigWithBindingV1(const void*,bool,bool,bool,bool);
XL6_TEXT_API void* xl6UdpConfigWithBroadcastV1(const void*,bool);
XL6_TEXT_API void* xl6UdpConfigWithErrorQueueV1(const void*,bool,uint64_t,uint64_t,uint64_t,int64_t);
#endif
