#include "foreign_decl_callback.h"
#include <assert.h>
#include <stddef.h>
#include "xrt_decl.h"
static xmemdebugsnapshot before;
void foreign_callback_allocation_begin(void)
{
    /* Enable() refuses while tracked allocations are alive. The host selects
     * debug recording before module creation; this operation only observes. */
    assert(xrtMemDebugEnabled());
    xrtMemDebugSnapshot(&before);
}
int foreign_callback_allocation_unchanged(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    return before.AllocCount == after.AllocCount && before.FreeCount == after.FreeCount &&
        before.ReallocCount == after.ReallocCount && before.LiveCount == after.LiveCount &&
        before.LiveBytes == after.LiveBytes && before.InvalidFreeCount == after.InvalidFreeCount &&
        before.DoubleFreeCount == after.DoubleFreeCount;
}
struct foreign_callback_widget { int value; };
static struct foreign_callback_widget widget = {71};
void* foreign_callback_widget_at(void) { return &widget; }
int foreign_callback_widget_value(foreign_callback_widget* value) { assert(value == &widget); return value->value; }
foreign_callback_record foreign_callback_apply_record(foreign_callback_record_proc callback, foreign_callback_record input)
{
    assert(callback);
    foreign_callback_record result = callback(&widget, input);
    assert(result.number == 71 && result.scale == input.scale);
    return result;
}
int foreign_callback_apply_pointer(foreign_callback_pointer_proc callback)
{
    assert(callback);
    assert(callback(&widget) == &widget);
    assert(callback(NULL) == NULL);
    return 1;
}
