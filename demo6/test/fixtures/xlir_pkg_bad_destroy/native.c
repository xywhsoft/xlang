#include <stdlib.h>
void* xlir_pkg_box_open(void) { return malloc(1); }
long long xlir_pkg_box_close(void* handle) { free(handle); return 0; }
