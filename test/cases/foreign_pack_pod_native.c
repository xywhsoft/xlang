#include "foreign_pack_pod.h"
#include <assert.h>
static int calls;
static int inspect(const PackPodDesc* d)
{assert(d && d->size==sizeof(*d));return d->factor+(int)d->point.x+(int)d->point.y+(int)d->box.origin.x+(int)d->box.origin.y+d->box.width+d->box.height;}
int pack_pod_call(PackPodRoot* root,const PackPodDesc* d,int* output)
{assert(root && root->value==11 && output);++calls;*output=root->value+inspect(d);return 0;}
int pack_pod_free(const PackPodDesc* d,int* output)
{assert(output);++calls;*output=inspect(d);return 0;}
int pack_pod_create(PackPodRoot** output)
{*output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;(*output)->value=11;(*output)->evaluate=pack_pod_call;return 0;}
void pack_pod_drop(PackPodRoot* root){xrtFree(root);}
static bool destroy(void* raw){pack_pod_drop(raw);return true;}
const XL6_ForeignViewDescriptorV1 packPodPolicy={sizeof(packPodPolicy),XL6_FOREIGN_VIEW_V1_TAG,"pack.pod",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
int pack_pod_calls(void){return calls;}
void pack_pod_fail_after(int count){assert(xrtMemDebugFailAfter(count));}
void pack_pod_reset(void){assert(xrtMemDebugFailAfter(-1));}
