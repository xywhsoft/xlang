/* The suite generates this file immediately before compiling this host. */
#include <string.h>
#include "../../build/test/xlir_conditional_string_generated.c"

int main(void)
{
    char* yes;
    char* no;
    int ok;
    if ( xlir_module_init() != 1 ) return 1;
    yes = xlir_fn_1(true);
    no = xlir_fn_1(false);
    ok = yes != NULL && no != NULL && strcmp(yes, "yes") == 0 && strcmp(no, "no") == 0;
    xrtFree(yes);
    xrtFree(no);
    return ok ? 0 : 1;
}
