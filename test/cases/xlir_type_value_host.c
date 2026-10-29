#include "cgen_class_host.c"

#if defined(XLIR_TYPE_VALUES_CASE)
#include "../../build/test/type_values.xlir_type.c"
#elif defined(XLIR_TYPE_ALIASES_CASE)
#include "../../build/test/type_aliases.xlir_type.c"
#elif defined(XLIR_GENERIC_CLASS_TYPE_VALUES_CASE)
#include "../../build/test/cgen_generic_class_type_values.xlir_type.c"
#else
#error "select one XLIR type-value generated-C case"
#endif

int main(void)
{
    return xlir_module_init() == 1 ? 0 : 1;
}
