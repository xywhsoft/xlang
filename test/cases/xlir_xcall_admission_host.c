/* Generated C is preincluded by the gate; both assertions and finalizers run. */
#include <assert.h>
int main(void)
{
    int ok = xlir_module_init();
    if (!ok && xrtGetError()) fprintf(stderr, "admission failure: %s\n", xrtErrorMessage((xerror*)xrtGetError()));
    assert(ok);
    assert(xl_script_module_unit_checked());
    xl_script_error_unit();
    return 0;
}
