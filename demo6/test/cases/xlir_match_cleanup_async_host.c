#include "../../build/test/xlir_match_cleanup_async.c"
#include "../../build/test/xlir_match_cleanup_async.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
int match_async_host_init(void) { return xlir_module_init(); }
void match_async_host_fini(void) { xlir_module_fini(); }
xfuture* match_async_start(int64_t kind, xfuture* pending) { return xlir_test_observe_future_launch(MATCH_ASYNC_START(kind, pending)); }
bool match_async_execution_idle(void) { return xl6ContinuationExecutorIsIdle(xlir_continuation_executor); }
