#include "xlir_generator_managed.c"
#include "xlir_generator_managed.bind.h"
XL6_Generator* managed_generator_create(xvalue* capture)
{return MANAGED_GENERATOR_START(capture);}
