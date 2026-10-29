/* The suite generates this file immediately before compiling this host. */
#include "../../build/test/xlir_dynamic_typed_collection_projection_generated.c"

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    return xlir_test_dynamic_typed_collection_projection() == 73 ? 0 : 2;
}
