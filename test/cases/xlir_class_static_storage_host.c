#include <stddef.h>

typedef struct xvalue xvalue;

int xlang_main(xvalue* arg);

int main(void)
{
    return xlang_main(NULL);
}
