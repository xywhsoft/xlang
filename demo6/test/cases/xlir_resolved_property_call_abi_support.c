/* Verified XLIR models the source-level destroy() declaration as void. Keep
 * the support surface explicit so the host fixture's historical integer
 * declaration cannot weaken generated prototype checks. */
#if defined(XLIR_GENERATED_MODULE_PREINCLUDED)
#define XLIR_NATIVE_PROPERTY_VOID_DESTROY
#endif

#include "cgen_native_class_property_host.c"
#include "cgen_class_host.c"
