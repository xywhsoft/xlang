/* C test hosts expose pointer-returning entry points to their runtime harness.
 * Observe the language launch carrier explicitly at that foreign boundary;
 * never silently extract .value and lose a diagnostic-free failure bit. */
#ifndef XLIR_LAUNCH_TEST_H
#define XLIR_LAUNCH_TEST_H
#include <assert.h>
#ifdef XLIR_TEST_LAUNCH_FUTURE
static xfuture* xlir_test_observe_future_launch(xlir_result_future result)
{
    assert(result.ok == (result.value != NULL));
    if (!result.ok) xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()});
    return result.value;
}
#endif
#ifdef XLIR_TEST_LAUNCH_GENERATOR
static xlir_generator* xlir_test_observe_generator_launch(xlir_result_generator result)
{
    assert(result.ok == (result.value != NULL));
    if (!result.ok) xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()});
    return result.value;
}
#endif
#endif
