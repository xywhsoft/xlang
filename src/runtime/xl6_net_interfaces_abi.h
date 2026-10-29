#ifndef XL6_NET_INTERFACES_ABI_H
#define XL6_NET_INTERFACES_ABI_H
struct xnetinterface;
struct xnetinterfaceaddress;
/* Internal copy helpers accept borrowed native records only for this call. */
XL6_TEXT_API void* xl6NetInterfaceCopyV1(const struct xnetinterface*);
XL6_TEXT_API void* xl6NetInterfaceAddressCopyV1(const struct xnetinterfaceaddress*);
XL6_TEXT_API void* xl6NetInterfacesCreateV1(void);
XL6_TEXT_API void xl6NetInterfacesDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetInterfacesCountV1(void*);
XL6_TEXT_API void* xl6NetInterfacesGetV1(void*,int64_t);
XL6_TEXT_API int64_t xl6NetInterfacesLengthV1(void*);
XL6_TEXT_API void* xl6NetInterfacesItemV1(void*,int64_t);
XL6_TEXT_API void xl6NetInterfaceDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceIPv4IndexV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceIPv6IndexV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceFlagsV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceMtuV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceAddressCountV1(void*);
XL6_TEXT_API void* xl6NetInterfaceAddressV1(void*,int64_t);
XL6_TEXT_API bool xl6NetInterfaceHasFlagV1(void*,int64_t);
XL6_TEXT_API bool xl6NetInterfaceIsUpV1(void*);
XL6_TEXT_API bool xl6NetInterfaceIsRunningV1(void*);
XL6_TEXT_API bool xl6NetInterfaceIsLoopbackV1(void*);
XL6_TEXT_API bool xl6NetInterfaceSupportsBroadcastV1(void*);
XL6_TEXT_API bool xl6NetInterfaceIsPointToPointV1(void*);
XL6_TEXT_API bool xl6NetInterfaceSupportsMulticastV1(void*);
XL6_TEXT_API xvalue* xl6NetInterfaceHardwareV1(void*);
XL6_TEXT_API xstrview xl6NetInterfaceNameV1(void*);
XL6_TEXT_API xstrview xl6NetInterfaceDisplayNameV1(void*);
XL6_TEXT_API void xl6NetInterfaceAddressDestroyV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceAddressFamilyV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceAddressScopeV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceAddressPrefixV1(void*);
XL6_TEXT_API bool xl6NetInterfaceAddressPrefixKnownV1(void*);
XL6_TEXT_API void* xl6NetInterfaceAddressValueV1(void*);
XL6_TEXT_API xstrview xl6NetInterfaceAddressTextV1(void*);
XL6_TEXT_API int64_t xl6NetInterfaceIndexV1(xstrview,int64_t);
XL6_TEXT_API xstrview xl6NetInterfaceNameByIndexV1(int64_t,int64_t);
XL6_TEXT_API xstrview xl6NetHostNameV1(void);
XL6_TEXT_API void* xl6NetLocalAddressV1(int64_t);
XL6_TEXT_API xstrview xl6NetLocalIPV1(int64_t);
XL6_TEXT_API xstrview xl6NetLocalMACV1(void);
XL6_TEXT_API xvalue* xl6NetLocalHardwareV1(void);
#endif
