#include "foreign_dereference.h"
#include <assert.h>
struct DerefRoot {DerefWatch* watches;unsigned depth;bool closing;DerefCallback callback;void* user;DerefOwner owner;};
static void unwatch(void* raw)
{
    DerefWatch* link=raw;
    if(link->previous){*link->previous=link->next;if(link->next)link->next->previous=link->previous;}
    *link=(DerefWatch){0};
}
static bool watch(void* raw,void* storage,void(*notify)(void*),void* user)
{
    DerefRoot* root=raw;DerefWatch* link=storage;
    if(root->closing || link->previous || link->next || link->notify || link->user)return false;
    link->next=root->watches;link->previous=&root->watches;link->notify=notify;link->user=user;
    if(link->next)link->next->previous=&link->next;
    root->watches=link;return true;
}
static void flush(DerefRoot* root){if(root->closing && !root->depth)xrtFree(root);}
static bool destroy(void* raw)
{
    DerefRoot* root=raw;if(root->closing)return true;
    ++root->depth;root->closing=true;
    DerefOwner owner=root->owner;root->callback=NULL;root->user=NULL;root->owner=(DerefOwner){0};
    while(root->watches){DerefWatch* link=root->watches;void(*notify)(void*)=link->notify;void* user=link->user;unwatch(link);notify(user);}
    bool result=!owner.Drop || owner.Drop(owner.reference.Data);--root->depth;flush(root);return result;
}
void deref_destroy(DerefRoot* root){if(root && !destroy(root))xrtSetErrorKind(XERR_STATE);}
static bool begin(void* raw,void* storage)
{
    DerefRoot* root=raw;DerefScope* scope=storage;if(root->closing || scope->self || scope->root)return false;
    scope->root=root;scope->self=scope;++root->depth;return true;
}
static bool end(void* storage)
{
    DerefScope* scope=storage;if(scope->self!=scope || !scope->root || !scope->root->depth)return false;
    DerefRoot* root=scope->root;*scope=(DerefScope){0};--root->depth;flush(root);return true;
}
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{const DerefRoot* root=raw;return !root->owner.reference.Ops || visit(root->owner.reference,user);}
static bool busy(const void* raw){return ((const DerefRoot*)raw)->depth!=0;}
const XL6_ForeignViewDescriptorV1 derefRootPolicy={sizeof(derefRootPolicy),XL6_FOREIGN_VIEW_V1_TAG,"deref.test.root",
    sizeof(DerefWatch),_Alignof(DerefWatch),watch,unwatch,destroy,sizeof(DerefScope),_Alignof(DerefScope),begin,end,trace,busy,XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH,NULL,NULL};
int deref_create(DerefRoot** out){*out=xrtCalloc(1,sizeof(**out));return *out ? 0 : -1;}
void deref_on(DerefRoot* root,DerefCallback fn,void* user,const DerefOwner* incoming)
{
    DerefScope scope={0};if(!begin(root,&scope)){xrtSetErrorKind(XERR_STATE);return;}
    DerefOwner next=incoming ? *incoming : (DerefOwner){0};
    if(next.Hold && (!fn || !next.Hold(next.reference.Data))){xrtSetErrorKind(XERR_STATE);(void)end(&scope);return;}
    if(root->closing){if(next.Drop)(void)next.Drop(next.reference.Data);xrtSetErrorKind(XERR_STATE);(void)end(&scope);return;}
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation)){if(next.Drop)(void)next.Drop(next.reference.Data);xrtSetErrorKind(XERR_STATE);(void)end(&scope);return;}
    DerefOwner old=root->owner;root->callback=fn;root->user=user;root->owner=next;
    assert(xrtOwnershipScopeEnd(&mutation));
    if(old.Drop && !old.Drop(old.reference.Data))xrtSetErrorKind(XERR_STATE);
    (void)end(&scope);
}
int deref_fire(DerefRoot* root)
{
    DerefCallback callback=root->callback;void* user=root->user;DerefOwner owner=root->owner;
    if(!callback)return 0;
    if(owner.Hold && !owner.Hold(owner.reference.Data))return -8;
    int value=7;const char text[]="view";
    int status=callback(root,&value,text,user);
    if(owner.Drop && !owner.Drop(owner.reference.Data))return -9;
    return status ? status : value;
}
static xmemdebugsnapshot counter;
void deref_counter_begin(void){xrtMemDebugSnapshot(&counter);}
void deref_counter_end(void){xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);assert(counter.AllocCount==now.AllocCount && counter.FreeCount==now.FreeCount && counter.ReallocCount==now.ReallocCount);}
