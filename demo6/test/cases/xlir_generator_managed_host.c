#ifndef XLIR_GENERATOR_MANAGED_SOURCE
#define XLIR_GENERATOR_MANAGED_SOURCE "xlir_generator_managed.c"
#endif
#include XLIR_GENERATOR_MANAGED_SOURCE
#ifndef XLIR_GENERATOR_MANAGED_BINDING
#define XLIR_GENERATOR_MANAGED_BINDING "xlir_generator_managed.bind.h"
#endif
#include XLIR_GENERATOR_MANAGED_BINDING
XL6_Generator* managed_generator_create(xvalue* capture)
{return MANAGED_GENERATOR_START(capture);}
