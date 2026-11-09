#include "foreign_xge3_resource_oom.h"
static xmemdebugsnapshot before;
void resource_test_begin(void){xrtMemDebugEnable(true);xrtMemDebugSnapshot(&before);}
int resource_test_balanced(void){xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);return before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount;}
int resource_test_fail(int count){return xrtMemDebugFailAfter(count);}
void resource_test_clear(void){xrtMemDebugFailClear();xrtClearError();}
