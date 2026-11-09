/* Test-only observations of the program's actual shared XRT instance. */
#include "test/cases/foreign_xge3_input_actions.h"
#include <xrt_decl.h>
static xmemdebugsnapshot before;
void actions_test_begin(void){xrtMemDebugSnapshot(&before);}
int actions_test_balanced(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 return before.Enabled && after.Enabled && before.LiveCount==after.LiveCount &&
  before.LiveBytes==after.LiveBytes && before.InvalidFreeCount==after.InvalidFreeCount &&
  before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount;
}
uint64_t actions_test_allocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return after.AllocCount-before.AllocCount;}
int actions_test_fail(int count){return xrtMemDebugFailAfter((uint64_t)count);}
void actions_test_clear(void){xrtMemDebugFailClear();}
