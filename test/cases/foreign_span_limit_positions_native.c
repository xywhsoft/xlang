#include "foreign_span_limit_positions.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <xrt_decl.h>
#include <assert.h>
static int calls;
static xmemdebugsnapshot before;
int limit_before(const uint8_t* data,int count)
{
    assert(count>=0 && count<=8 && (!count || data));++calls;
    int sum=0;for(int i=0;i<count;++i)sum+=data[i];return sum;
}
int limit_shared(const uint8_t* a,int ac,const uint8_t* b,int bc)
{return limit_before(a,ac)+limit_before(b,bc);}
int limit_distinct(const uint8_t* a,int ac,const uint8_t* b,int bc)
{return limit_before(a,ac)+limit_before(b,bc);}
static int measure(LimitRoot* root,const uint8_t* data,int count,int hint)
{assert(root && hint==17);return limit_before(data,count);}
static int render(LimitRoot* root,const uint8_t* data,int count,int width,int height,void* pixels,int stride)
{
    assert(root && width==2 && height==2 && stride==3 && pixels);
    assert(count>=0 && count<=3 && (!count || data));++calls;
    uint8_t* output=pixels;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        output[y*stride+x]=count ? data[(y*width+x)%count]:0;
    return 0;
}
int limit_render(const uint8_t* data,int count,int* sum,int width,int height,void* pixels,int stride)
{
    LimitRoot root={0};int status=render(&root,data,count,width,height,pixels,stride);
    assert(sum);*sum=limit_before(data,count);return status;
}
int limit_create(LimitRoot** output)
{
    LimitRoot* root=xrtCalloc(1,sizeof(*root));if(!root)return -1;
    root->measure=measure;root->render=render;*output=root;return 0;
}
void limit_destroy(LimitRoot* root){xrtFree(root);}
static bool destroy(void* raw){limit_destroy(raw);return true;}
const XL6_ForeignViewDescriptorV1 limitPolicy={
    .size=sizeof(limitPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="span.limit.positions",
    .watchAlignment=1,.Destroy=destroy,.callAlignment=1
};
int limit_calls(void){return calls;}
void limit_counter_begin(void){xrtMemDebugSnapshot(&before);}
void limit_counter_end(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
}
