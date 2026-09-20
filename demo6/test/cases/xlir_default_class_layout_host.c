#include <stdio.h>
#include "xlir_default_class_layout_support.c"

/* The generated XLIR module initializer intentionally has internal linkage.
 * The runtime gate compiles this host with `-include generated.c`, exercising
 * exactly the one translation unit that production embedding uses. */
static int xlir_module_init(void);
typedef struct xerror xerror;
extern const xerror* xrtGetError(void);
extern const char* xrtErrorMessage(const xerror* error);

int main(void)
{
    if ( xlir_module_init() ) return 0;
    {
        const xerror* error = xrtGetError();
        if ( error != NULL ) fprintf(stderr, "XLIR module failure: %s\n", xrtErrorMessage(error));
    }
    return 1;
}
