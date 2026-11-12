/* Exercises the production ELF linker, not a mock relocation algorithm. */
#define ONE_SOURCE 1
#include "tcc.h"
#include "libtcc.c"
int main(int argc, char **argv)
{
    TCCState *s;
    int rc;
    if (argc != 5) return 2;
    s = tcc_new();
    if (!s) return 3;
    tcc_set_lib_path(s, argv[1]);
    rc = tcc_set_output_type(s, !strcmp(argv[2], "dll") ? TCC_OUTPUT_DLL :
        !strcmp(argv[2], "exe") ? TCC_OUTPUT_EXE : TCC_OUTPUT_MEMORY);
    if (!rc) rc = tcc_add_file(s, argv[3]);
    if (!rc) rc = !strcmp(argv[2], "jit") ? tcc_relocate(s) :
        tcc_output_file(s, argv[4]);
    tcc_delete(s);
    return rc ? 1 : 0;
}
