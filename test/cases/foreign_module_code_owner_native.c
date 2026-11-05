#include "test/cases/foreign_module_code_owner.h"
#include <assert.h>
static XL6_CodeOwner expected;
static XL6_CodeLease* held;
static unsigned captures;
void module_owner_expect(const XL6_CodeOwner* owner)
{assert(!held && !captures);expected=owner ? *owner:(XL6_CodeOwner){0};}
int module_owner_expected(void){return expected.size!=0;}
static void check(const XL6_CodeOwner* owner)
{
    ++captures;
    if(!expected.size){assert(!owner);return;}
    assert(owner && owner->size>=sizeof(*owner) && owner->context==expected.context &&
        owner->acquire==expected.acquire && owner->release==expected.release);
    if(!held)assert(xl6CodeLeaseAcquireScoped(owner,&held)==XL6_CODE_LEASE_ACQUIRED && held);
}
int module_owner_capture(const XL6_CodeOwner* owner,int value)
{check(owner);return value+module_owner_expected();}
int module_owner_after(int value,const XL6_CodeOwner* owner,const void* empty)
{assert(!empty);return module_owner_capture(owner,value);}
int module_owner_dual(const XL6_CodeOwner* first,int value,const XL6_CodeOwner* second)
{assert(first==second);return module_owner_capture(first,value);}
int module_owner_only(const XL6_CodeOwner* owner,int* output)
{check(owner);assert(output);*output=41+module_owner_expected();return 0;}
int module_owner_method(OwnerReceiver* receiver,const XL6_CodeOwner* owner,int value)
{assert(receiver && receiver->value==7);return module_owner_capture(owner,value)+receiver->value;}
int module_owner_create(OwnerReceiver** output)
{
    assert(output);*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;
    (*output)->value=7;(*output)->invoke=module_owner_method;return 0;
}
void module_owner_drop(OwnerReceiver* receiver){xrtFree(receiver);}
static bool destroy(void* raw){module_owner_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 moduleOwnerPolicy={sizeof(moduleOwnerPolicy),XL6_FOREIGN_VIEW_V1_TAG,
    "test.owner.receiver",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
void module_owner_release(void)
{assert(captures>=8);xl6CodeLeaseRelease(held);held=NULL;}
