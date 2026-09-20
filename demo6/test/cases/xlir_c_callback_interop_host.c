/* Keep the foreign implementation identical between legacy and XLIR.  The
 * XLIR backend emits a library-shaped module, so this host includes that
 * translation unit and invokes its explicit module initializer. */
#define callback_box_destroy callback_box_destroy_legacy_host
#include "cgen_c_callback_interop_host.c"
#undef callback_box_destroy
#include "../../build/test/xlir_c_callback_interop.c"

void callback_box_destroy(void* self)
{
    free(self);
}

int main(void)
{
    if ( !xlir_module_init() ) return 1;
    xlir_module_fini();
    return 0;
}
