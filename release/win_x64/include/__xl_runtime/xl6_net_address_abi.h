#ifndef XL6_NET_ADDRESS_ABI_H
#define XL6_NET_ADDRESS_ABI_H
/* Included after the common resident ABI types. Every factory returns one
 * immutable POD owner, never an Engine/Resolver or borrowed address slot. */
struct xnetaddr;
XL6_TEXT_API void* xl6NetAddressCopyV1(const struct xnetaddr*);
XL6_TEXT_API void* xl6NetAddressDefaultV1(void);
XL6_TEXT_API void* xl6NetAddressParseV1(xstrview,int64_t);
XL6_TEXT_API void* xl6NetAddressEndpointV1(xstrview,int64_t);
XL6_TEXT_API void* xl6NetAddressAnyV1(int64_t,int64_t);
XL6_TEXT_API void* xl6NetAddressLoopbackV1(int64_t,int64_t);
XL6_TEXT_API void xl6NetAddressDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetAddressFamilyV1(void*);
XL6_TEXT_API int64_t xl6NetAddressPortV1(void*);
XL6_TEXT_API int64_t xl6NetAddressScopeV1(void*);
XL6_TEXT_API xstrview xl6NetAddressTextV1(void*);
XL6_TEXT_API xstrview xl6NetAddressEndpointTextV1(void*);
XL6_TEXT_API xvalue* xl6NetAddressBytesV1(void*);
XL6_TEXT_API bool xl6NetAddressEqualV1(void*,void*);
XL6_TEXT_API bool xl6NetAddressSameIPV1(void*,void*);
XL6_TEXT_API int64_t xl6NetAddressCompareV1(void*,void*);
XL6_TEXT_API bool xl6NetAddressIsUnspecifiedV1(void*);
XL6_TEXT_API bool xl6NetAddressIsLoopbackV1(void*);
XL6_TEXT_API bool xl6NetAddressIsMulticastV1(void*);
XL6_TEXT_API bool xl6NetAddressIsLinkLocalV1(void*);
XL6_TEXT_API bool xl6NetAddressIsPrivateV1(void*);
XL6_TEXT_API bool xl6NetAddressIsMappedV1(void*);
XL6_TEXT_API void* xl6NetAddressUnmapV1(void*);
XL6_TEXT_API void* xl6NetAddressWithPortV1(void*,int64_t);
XL6_TEXT_API void* xl6NetAddressWithScopeV1(void*,int64_t);
XL6_TEXT_API void* xl6NetResolveAddressV1(xstrview,int64_t);
#endif
