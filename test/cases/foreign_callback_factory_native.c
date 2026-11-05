/* Pure-C shared leaf: no hidden language references in the Node payload. The
 * global retained callback is an actual external owner until explicitly clear. */
#include "foreign_callback_factory.h"
#include <assert.h>
static int live,calls,created,freed;
static CallbackFactoryVisit visit;
static void* userdata;
static CallbackFactoryOwner owner;
int callback_factory_share(CallbackFactoryNode* node,CallbackFactoryNode** out)
{
 assert(node && node->refs>0 && out);*out=NULL;++calls;
 if(node->mode==2)return -9;
 ++node->refs;*out=node;
 return node->mode==1 ? -7:0; /* A failed producer may still transfer an owner. */
}
int callback_factory_drop(CallbackFactoryNode* node)
{
 if(!node)return 0;
 assert(node->refs>0);
 if(!--node->refs){--live;++freed;xrtFree(node);}return 0;
}
static bool destroy(void* node){return callback_factory_drop(node)==0;}
const XL6_ForeignViewDescriptorV1 callbackFactoryPolicy={
 sizeof(callbackFactoryPolicy),XL6_FOREIGN_VIEW_V1_TAG,"callback.factory.leaf",
 0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL
};
int callback_factory_set(CallbackFactoryVisit next,void* user,const CallbackFactoryOwner* proposed)
{
 CallbackFactoryOwner incoming={0};
 if(next){
  assert(proposed && proposed->onHold && proposed->onDrop);
  incoming=*proposed;if(!incoming.onHold(incoming.tReference.Data))return -16;
 }else assert(!user && !proposed);
 CallbackFactoryOwner previous=owner;visit=next;userdata=user;owner=incoming;
 return !previous.onDrop || previous.onDrop(previous.tReference.Data) ? 0:-16;
}
int callback_factory_dispatch(int mode)
{
 assert(visit);CallbackFactoryNode* node=xrtCalloc(1,sizeof(*node));if(!node)return -1;
 node->refs=1;node->mode=mode;node->value=42;++created;++live;
 CallbackFactoryVisit selected=visit;void* user=userdata;CallbackFactoryOwner snapshot=owner;
 if(!snapshot.onHold(snapshot.tReference.Data)){callback_factory_drop(node);return -16;}
 int status=selected(node,user);bool dropped=snapshot.onDrop(snapshot.tReference.Data);
 callback_factory_drop(node);return status ? status:(dropped ? 0:-16);
}
int callback_factory_live(void){return live;}
int callback_factory_calls(void){return calls;}
void callback_factory_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
int callback_factory_reset(void){bool triggered=xrtMemDebugFailTriggered();assert(xrtMemDebugFailAfter(-1));return triggered;}
int callback_factory_verify(void){return !live && created==4 && freed==4 && !visit && !owner.onDrop && !xl6ForeignViewPendingV1(0);}
