#ifndef XL6_FLOAT_CONVERT_H
#define XL6_FLOAT_CONVERT_H
#include <stdbool.h>
typedef struct xvalue xvalue;
typedef struct xstrview xstrview;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_FLOAT_CONVERT_API __declspec(dllexport)
#else
#define XL6_FLOAT_CONVERT_API
#endif
/* Explicit conversion, not a typed projection. Borrowed input; commit the
 * trivial output only on success. The boolean survives failed diagnostics. */
XL6_FLOAT_CONVERT_API bool xl6Float32ValueConvertV1(const xvalue*, float*);
XL6_FLOAT_CONVERT_API bool xl6Float32TextConvertV1(xstrview, float*);
XL6_FLOAT_CONVERT_API bool xl6Float32ConvertV1(double, float*);
#endif
