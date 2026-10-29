#ifndef XL6_NATIVE_VALUE_ABI_H
#define XL6_NATIVE_VALUE_ABI_H
/* Owning native records are not POD and are not Value/Future pointers. This
 * transport number is shared by XLIR and resident storage, not inferred from
 * sizeof(), a type spelling or the presence of a copy callback. */
enum { XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 = 5 };
#endif
