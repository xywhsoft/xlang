#include "libtcc.h"
#include <stdio.h>
#include <string.h>
static char diagnostic[4096];
static int native_elf_gc_data = 42;
static void error(void* ignored, const char* text)
{
    (void)ignored;
    snprintf(diagnostic, sizeof(diagnostic), "%s", text);
    fprintf(stderr, "%s\n", text);
}
int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    int expectedFailure = strcmp(argv[3], "success") != 0;
    TCCState* state = tcc_new();
    if (!state) return 3;
    tcc_set_error_func(state, NULL, error);
    tcc_set_lib_path(state, argv[1]);
    if (tcc_set_output_type(state, TCC_OUTPUT_MEMORY) ||
        tcc_set_options(state, "-ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--undefined=pic_entry") < 0 ||
        tcc_add_symbol(state, "native_elf_gc_data", &native_elf_gc_data) ||
        tcc_compile_string(state, "extern int native_elf_gc_pic_read(void); int pic_entry(void) { return native_elf_gc_pic_read(); }") ||
        tcc_add_file(state, argv[2])) return 4;
    int relocated = tcc_relocate(state);
    if (expectedFailure) {
        int ok = relocated < 0 && strstr(diagnostic, "'_GLOBAL_OFFSET_TABLE_' defined twice");
        tcc_delete(state); return ok ? 0 : 5;
    }
    if (relocated < 0) return 6;
    int (*entry)(void) = (int (*)(void))tcc_get_symbol(state, "pic_entry");
    if (!entry || entry() != 42 || tcc_get_symbol(state, "native_elf_gc_dead")) return 7;
    tcc_delete(state);
    puts("GCC PIC live GOT relocation survived GC; dead unresolved function was removed");
    return 0;
}
