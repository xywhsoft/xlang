#include "lib/xrt/xrt.h"

/* The declarative xlang c_int parameter must remain C-compatible with the
 * xconsolestream enum accepted by the underlying console service. */
extern bool xrtConsoleFlush(int stream);

int main(void)
{
    return xrtConsoleFlush(1) ? 0 : 1;
}
