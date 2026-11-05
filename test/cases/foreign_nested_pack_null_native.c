#include "test/cases/foreign_nested_pack_null.h"
#include <assert.h>
static int calls;
static bool destroy(void* raw){xrtFree(raw);return true;}
const XL6_ForeignViewDescriptorV1 nestedRootPolicy={sizeof(nestedRootPolicy),XL6_FOREIGN_VIEW_V1_TAG,"nested.root",0,1,NULL,NULL,destroy,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
int nested_create(NestedRoot** output,const void* absent)
{
    assert(output && !absent);*output=xrtCalloc(1,sizeof(**output));
    if(!*output)return -1;
    (*output)->value=11;(*output)->evaluate=nested_call;return 0;
}
void nested_drop(NestedRoot* raw){xrtFree(raw);}
static int validate(const NestedOuter* d,const int* absent,int* output)
{
    assert(d && !absent && output && d->size==sizeof(*d));
    assert(d->view.size==sizeof(d->view));
    assert(d->view.renderer.size==sizeof(d->view.renderer));
    assert(!d->view.renderer.fonts.source || d->view.renderer.fonts.source->value==11);
    assert(d->view.renderer.length==3 && !memcmp(d->view.renderer.text,"a\0b",3));
    ++calls;*output=d->factor+(d->view.renderer.fonts.source ? d->view.renderer.fonts.source->value:0)+3;return 0;
}
int nested_call(NestedRoot* root,const NestedOuter* d,const int* absent,int* output)
{int status=validate(d,absent,output);*output+=root->value;return status;}
int nested_free(const NestedOuter* d,const int* absent,int* output){return validate(d,absent,output);}
int nested_null_only(const int* absent,int* output){assert(!absent && output);++calls;*output=17;return 0;}
int nested_trailing(int input,const NestedRoot* one,const void* two){assert(!one && !two);++calls;return input;}
int nested_multi(const int* one,int* first,const void* two,int* second){assert(!one && !two && first && second);++calls;*first=23;*second=29;return 0;}
int nested_span(const unsigned char* data,size_t count,const int* absent,int* output){assert(data && count==3 && !absent && output);assert(!memcmp(data,"a\0b",3));++calls;*output=31;return 0;}
int nested_calls(void){return calls;}
void nested_fail_after(int n){assert(xrtMemDebugFailAfter(n));}
void nested_reset(void){assert(xrtMemDebugFailAfter(-1));}
