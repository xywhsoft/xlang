#include "test/cases/foreign_array_projection.h"
#include <string.h>
#include <assert.h>
static int calls;
int array_projection_sum(const int* p,int n) { ++calls;int sum=0;for(int i=0;i<n;++i)sum+=p[i];return sum; }
int array_projection_pair(const int* a,const int* b,int n) { ++calls;int sum=0;for(int i=0;i<n;++i)sum+=a[i]*b[i];return sum; }
int array_projection_pods(const ArrayPod* p,size_t n) { ++calls;int sum=0;for(size_t i=0;i<n;++i)sum+=p[i].x+p[i].y;return sum; }
int array_projection_items(const ArrayItem* p,int n) { ++calls;int sum=0;for(int i=0;i<n;++i)sum+=(int)strlen(p[i].title)+p[i].result+p[i].semantic;return sum; }
int array_projection_calls(void) { return calls; }
static int live;
int array_projection_create(int value,ArrayRoot** output) { *output=xrtCalloc(1,sizeof(**output));if(!*output)return -1;(*output)->value=value;(*output)->refs=1;++live;return 0; }
void array_projection_destroy(ArrayRoot* root) { if(root){assert(root->refs>0);if(!--root->refs){assert(!root->active);--live;xrtFree(root);}} }
static bool destroy(void* raw) { array_projection_destroy(raw);return true; }
static bool begin(void* raw,void* storage) { ArrayScope* scope=storage;scope->root=raw;++scope->root->refs;++scope->root->active;return true; }
static bool end(void* storage) { ArrayScope* scope=storage;ArrayRoot* root=scope->root;assert(root && root->active);--root->active;scope->root=NULL;array_projection_destroy(root);return true; }
const XL6_ForeignViewDescriptorV1 arrayRootPolicy={sizeof(arrayRootPolicy),XL6_FOREIGN_VIEW_V1_TAG,"array.projection.root",0,1,NULL,NULL,destroy,sizeof(ArrayScope),_Alignof(ArrayScope),begin,end,NULL,NULL,XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY,NULL,NULL};
int array_projection_roots(ArrayRoot* root,ArrayRoot* const* items,const char* const* texts,int n,ArrayCallback callback,void* user,int* output)
{
 ++calls;int result=root->value;
 for(int i=0;i<n;++i)result+=(items[i] ? items[i]->value:0)+(texts[i] ? (int)strlen(texts[i]):0);
 if(callback && callback(user)<0)return -16;
 int again=root->value;
 for(int i=0;i<n;++i)again+=(items[i] ? items[i]->value:0)+(texts[i] ? (int)strlen(texts[i]):0);
 assert(result==again);*output=result;return 0;
}
int array_projection_live(void) { return live; }
void array_projection_fail_after(int count) { assert(xrtMemDebugFailAfter(count)); }
void array_projection_reset_failure(void) { assert(xrtMemDebugFailAfter(-1)); }
