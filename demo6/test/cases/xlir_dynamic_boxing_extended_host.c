#include "xlir_dynamic_boxing_extended_support.c"

static int xlir_module_init(void);
extern const xerror* xrtGetError(void);

int main(void)
{
    if ( xlir_module_init() ) return 0;
    {
        const xerror* error = xrtGetError();
        fprintf(stderr, "XLIR dynamic boxing module failed: %s\n",
                error != NULL ? xrtErrorMessage(error) : "(no error)");
    }
    return 1;
}
