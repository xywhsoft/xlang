#include "src/runtime/xl6_text_value.h"
#include "src/api/xl_task_group_bridge.h"

/* The signal route retains the ordinary managed TaskGroup object. These
 * checked entries merely expose its existing resident bridge, not a new
 * scheduler or a raw pointer captured by XRT's signal dispatcher. */
void* xl6SignalTaskGroupCreateV1(void) { return xl6TaskGroupCreate(); }
void xl6SignalTaskGroupDestroyV1(void* group)
{
    xerror* prior=xrtTakeError();xl6TaskGroupDestroy(group);
    xrtClearError();xrtSetErrorTake(prior);
}
bool xl6SignalTaskGroupCloseV1(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupClose(group); }
bool xl6SignalTaskGroupCancelV1(void* group)
{ if(!group){xrtSetErrorKind(XERR_ARGUMENT);return false;}return xl6TaskGroupCancel(group); }
