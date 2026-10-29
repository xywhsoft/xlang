#ifndef XL6_NET_BACKENDS_ABI_H
#define XL6_NET_BACKENDS_ABI_H
XL6_TEXT_API bool xl6NetBackendCompiledV1(int64_t);
XL6_TEXT_API int64_t xl6NetBackendCapabilitiesV1(int64_t);
XL6_TEXT_API xstrview xl6NetBackendNameV1(int64_t);
XL6_TEXT_API int64_t xl6NetDefaultBackendV1(void);
#endif
