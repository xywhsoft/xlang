#ifndef XL6_ARRAY_IMPORT_H
#define XL6_ARRAY_IMPORT_H
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_IMPORT_API __declspec(dllexport)
#else
#define XL6_ARRAY_IMPORT_API
#endif

/* Explicit data-to-native boundary, not a cast/rebind or a wire fallback.
 * Consumes the seed on BOTH paths and observes the source. The seed must be
 * an empty, admitted ARRAY with its final resident projection already bound;
 * no unprofiled shell or generated-code converter is created here. Success
 * returns that same owned seed. Failure destroys the partial destination.
 * Matching native tuples, equivalent admission and identical resident codecs
 * copy typed storage directly under two pins, without boxing/snapshotting.
 * Otherwise the source is snapshotted before insertion, so its slots are never
 * borrowed across converters. Its count/items/identity stay unchanged on failure.
 * Every item passes the destination's Value admission and actual slot codec.
 * Nested wire Arrays are NOT recursively guessed or rebound: their explicit
 * schema conversion belongs to the compiler's boundary plan. Native children,
 * callable boxes and Future references keep their actual owned identities.
 * This API requires a private construction seed; aliasing it as source is an
 * argument error. It is not an atomic replacement of a published container. */
XL6_ARRAY_IMPORT_API xvalue* xl6ArrayImportTakeV1(xvalue*, const xvalue*);
#endif
