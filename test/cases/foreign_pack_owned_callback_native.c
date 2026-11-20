#include "foreign_pack_owned_callback.h"
#include <assert.h>
#include <string.h>
static GpDesc current;
static GpOwner current_owner;
static int installations;
static int failure_roots;
int gp_install(const GpDesc* desc,const GpOwner* owner)
{
    assert(desc && owner && owner->reference.Data==desc->user && owner->reference.Ops && owner->hold && owner->drop);
    if(!owner->hold(owner->reference.Data))return -1;
    GpOwner previous=current_owner;
    current=*desc;current_owner=*owner;++installations;
    return !previous.reference.Data || previous.drop(previous.reference.Data) ? 0:-1;
}
int gp_dispatch(int input)
{
    GpDesc desc=current;GpOwner owner=current_owner;
    if(!owner.reference.Data || !owner.hold(owner.reference.Data)){xrtSetErrorKind(XERR_STATE);return -1;}
    /* A native snapshot, not just the current slot, protects the return tail
     * when a callback replaces or removes its own installation. */
    int value=desc.value ? desc.value(input,desc.user):0;
    if(value>=0 && desc.notice)desc.notice(desc.user,input);
    if(!owner.drop(owner.reference.Data))return -1;
    return value<0 ? -1:value+desc.seed;
}
int gp_clear(void)
{
    GpOwner previous=current_owner;
    memset(&current,0,sizeof(current));memset(&current_owner,0,sizeof(current_owner));
    return !previous.reference.Data || previous.drop(previous.reference.Data) ? 0:-1;
}
static int invoke(const GpDesc* desc,const GpOwner* owner,int input)
{
    assert(desc && owner && desc->user==owner->reference.Data);
    if(!owner->hold(owner->reference.Data))return -1;
    int result=desc->value ? desc->value(input,desc->user):0;
    if(result>=0 && desc->notice)desc->notice(desc->user,input);
    if(!owner->drop(owner->reference.Data))return -1;
    return result<0 ? -1:result+desc->seed;
}
int gp_two(const GpDesc* left,const GpOwner* a,const GpDesc* right,const GpOwner* b)
{
    assert(left->user!=right->user);
    int x=invoke(left,a,2);if(x<0)return x;
    int y=invoke(right,b,3);return y<0 ? y:x+y;
}
int gp_optional(const GpDesc* desc,const GpOwner* owner)
{
    assert(desc);
    if (!desc->value && !desc->notice) {
        assert(!desc->user && !owner);
        return desc->seed;
    }
    assert(desc->user && owner);
    return invoke(desc,owner,2);
}
int gp_nested(const GpNested* desc,const GpOwner* owner){return gp_install(&desc->inner,owner);}
static int root_install(GpRoot* root,const GpDesc* desc,const GpOwner* owner)
{assert(root);return gp_install(desc,owner);}
int gp_root(GpRoot** output)
{
    assert(output);*output=xrtCalloc(1,sizeof(**output));
    if(!*output)return -1;
    (*output)->install=root_install;return 0;
}
void gp_root_drop(GpRoot* root)
{
    if(!root)return;
    GpOwner previous=root->failed_owner;root->failed_owner=(GpOwner){0};
    if(previous.reference.Data){--failure_roots;(void)previous.drop(previous.reference.Data);}
    xrtFree(root);
}
int gp_reject(const GpDesc* desc,GpRoot** output,const GpOwner* owner)
{
    if(gp_root(output))return -1;
    assert(desc->user==owner->reference.Data);
    if(!owner->hold(owner->reference.Data))return -1;
    /* This cleanup duty is NEVER published as a successful leaf object. The
     * language factory must roll it back before releasing its local group. */
    (*output)->failed_owner=*owner;++failure_roots;
    xrtSetErrorKind(XERR_STATE);return -37;
}
int gp_failure_roots(void){return failure_roots;}
static bool root_destroy(void* raw){gp_root_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 gpPolicy={sizeof(gpPolicy),XL6_FOREIGN_VIEW_V1_TAG,
    "gp.root",0,1,NULL,NULL,root_destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
int gp_installs(void){return installations;}
int gp_active(void){return current_owner.reference.Data!=NULL;}
void gp_fail_after(int n){assert(xrtMemDebugFailAfter(n));}
int gp_fail_reset(void){int triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();return triggered;}
void gp_heap(size_t* live,size_t* bytes,uint64_t* allocs,uint64_t* frees,uint64_t* reallocs)
{
    xmemdebugsnapshot snapshot;xrtMemDebugSnapshot(&snapshot);
    *live=snapshot.LiveCount;*bytes=snapshot.LiveBytes;*allocs=snapshot.AllocCount;
    *frees=snapshot.FreeCount;*reallocs=snapshot.ReallocCount;
}
