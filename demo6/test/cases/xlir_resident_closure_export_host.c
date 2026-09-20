/* Exercise the original raw C callback export fixture and its 1000 retained
 * replacements unchanged, through XLIR's explicit module lifecycle. AOT code
 * and the native callbacks are externally resident in this lane. */
#include "cgen_c_callback_export.c"
#define XL_InitOptions int
#define xlInitOptions(argc, argv) ((void)(argc), (void)(argv), 0)
#define xlInit(options) ((void)(options), xlir_module_init())
#define xlUnit() xlir_module_fini()
#define main raw_callback_fixture_main
#include "cgen_c_callback_export_host.c"
#undef main
int main(void)
{
    int result = raw_callback_fixture_main();
    if (result != 0 || xrtGetError() != NULL) return result ? result : 7;
    return 0;
}
