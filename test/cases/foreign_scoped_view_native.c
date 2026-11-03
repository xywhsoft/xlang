/* One fixture translation unit; reuse the tested native publisher machinery.
 * The callback payload below is genuinely automatic C storage. */
#include "foreign_managed_view_native.c"
#include "foreign_scoped_view.h"
int scoped_read(ManagedChild* event){return event->value;}
ManagedChild* scoped_identity(ManagedChild* event){return event;}
ManagedChild* scoped_optional(ManagedChild* event){return event;}
int scoped_fire(ManagedRoot* root,int code)
{
    ManagedCallback fn=root->callback;void* data=root->userdata;
    ManagedCallbackOwner snapshot=root->callbackOwner;
    if(!fn)return -99;
    if(snapshot.Hold && !snapshot.Hold(data)){xrtSetErrorKind(XERR_STATE);return -98;}
    ManagedChild event={0};event.root=root;event.value=code;
    int result=fn(&event,code,data);
    if(snapshot.Drop && !snapshot.Drop(data)){xrtSetErrorKind(XERR_STATE);return -97;}
    return result;
}
