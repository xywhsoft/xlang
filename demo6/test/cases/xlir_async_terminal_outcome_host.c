#ifndef XLIR_TERMINAL_SOURCE
#define XLIR_TERMINAL_SOURCE "../../build/test/xlir_async_terminal_outcome.c"
#endif
#ifndef XLIR_TERMINAL_BINDING
#define XLIR_TERMINAL_BINDING "../../build/test/xlir_async_terminal_outcome.h.bind.h"
#endif
#include XLIR_TERMINAL_SOURCE
#include XLIR_TERMINAL_BINDING
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
#define XLIR_ASYNC_SOURCE_HOST
#include "xlir_async_source_sync.h"
int terminal_host_init(void) { xlir_async_test_bind(); return xlir_module_init(); }
void terminal_host_fini(void) { xlir_async_test_fini(); }
xfuture* terminal_host_start(int kind, xfuture* input, xfuture* second) { xlir_async_test_begin(); return xlir_test_observe_future_launch(TERMINAL_START(kind, input, second)); }
xfuture* terminal_host_return(bool first, xfuture* input) { xlir_async_test_begin(); return xlir_test_observe_future_launch(TERMINAL_ALL_RETURN(first, input)); }
