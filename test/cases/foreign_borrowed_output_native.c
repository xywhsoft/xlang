#include "foreign_managed_view_native.c"
#include "foreign_borrowed_output.h"
#undef NDEBUG
#include <assert.h>
static unsigned queryCalls,watchCalls,destroyCalls;
static bool guardedWatch(void* raw,void* storage,void(*notify)(void*),void* user)
{
    /* A status failure may leave an unspecified output. A bounded assertion
     * catches observation of it before the existing Watch dereferences it. */
    assert(raw!=(void*)(uintptr_t)1);++watchCalls;
    return childWatch(raw,storage,notify,user);
}
static bool guardedDestroy(void* raw){++destroyCalls;return destroyChild(raw);}
const XL6_ForeignViewDescriptorV1 borrowedChildPolicy={
    sizeof(borrowedChildPolicy),XL6_FOREIGN_VIEW_V1_TAG,"borrowed.output.child",
    sizeof(ManagedWatch),_Alignof(ManagedWatch),guardedWatch,unwatch,guardedDestroy,
    0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
int borrowed_seed(ManagedRoot* root)
{ManagedChild* child=NULL;return managed_child_create(root,0,&child);}
int borrowed_status(ManagedRoot* root,int mode,ManagedChild** out)
{
    ++queryCalls;
    if(mode==1){*out=(ManagedChild*)(uintptr_t)1;return -17;}
    if(mode==2){*out=NULL;return 0;}
    if(mode==3){managed_root_destroy(root);*out=(ManagedChild*)(uintptr_t)1;return -17;}
    *out=managed_child_first(root);return 0;
}
int borrowed_get(ManagedRoot* root,ManagedChild** out){return borrowed_status(root,0,out);}
int borrowed_optional(ManagedRoot* root,int mode,ManagedChild** out){return borrowed_status(root,mode,out);}
void borrowed_void(ManagedRoot* root,ManagedChild** out){assert(!borrowed_status(root,0,out));}
int borrowed_owner(ManagedChild* child,ManagedRoot** out){++queryCalls;*out=managed_child_owner(child);return 0;}
int borrowed_free(int mode,ManagedRoot* root,ManagedChild** out){return borrowed_status(root,mode,out);}
int borrowed_span(ManagedRoot* root,const uint8_t* data,int count,ManagedChild** out,int mode)
{assert(count==3 && data && data[0]=='a' && !data[1] && data[2]=='b');return borrowed_status(root,mode,out);}
void borrowed_child_destroy(ManagedChild* child){++destroyCalls;managed_child_destroy(child);}
unsigned borrowed_calls(void){return queryCalls;}
unsigned borrowed_watches(void){return watchCalls;}
unsigned borrowed_destroys(void){return destroyCalls;}
void borrowed_fail_after(int count){(void)xrtMemDebugFailAfter(count);}
static int readChild(BorrowedReader* reader,ManagedRoot* root,ManagedChild** out)
{assert(reader);return borrowed_get(root,out);}
BorrowedReader* borrowed_reader(void){static BorrowedReader reader={readChild};return &reader;}
