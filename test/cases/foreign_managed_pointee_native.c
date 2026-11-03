/* Native publisher owns the pointer slot until the complete typed thunk returns. */
#include "foreign_managed_view_native.c"
#include "foreign_managed_pointee.h"
int managed_pointee_fire(ManagedRoot* root,ManagedPointeeCallback fn,void* data,
    const ManagedCallbackOwner* owner,ManagedChild* expected)
{
    assert(root && fn && owner && owner->Hold && owner->Drop);
    if(!owner->Hold(data)){xrtSetErrorKind(XERR_STATE);return -8;}
    ManagedChild* slot=managed_child_first(root);
    int result=fn(&slot,data);
    assert(slot==expected);
    if(!owner->Drop(data)){xrtSetErrorKind(XERR_STATE);return -7;}
    return result;
}
