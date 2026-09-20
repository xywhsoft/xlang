/* The legacy backend still declares native destroy hooks as integer-returning,
 * while verified XLIR models the source-level destroy() declaration as void.
 * Keep one differential support surface without weakening either generated
 * translation unit's prototype checks. */
#if defined(XLIR_GENERATED_MODULE_PREINCLUDED)
#define XLIR_NATIVE_PROPERTY_VOID_DESTROY
#endif

#include "cgen_native_class_property_host.c"
#include "cgen_class_host.c"
