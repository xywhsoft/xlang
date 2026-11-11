#include "foreign_span_region_prefix.h"
#include <__xl_runtime/xl6_foreign_view.h>
#include <xrt_decl.h>
#include <assert.h>
static int calls;
int prefix_region_render(const uint8_t* data,int size,int width,int height,void* pixels,int stride)
{
    assert(size>=0 && size<=3 && (!size || data));
    assert(width>=0 && width<=8 && height>=0 && height<=8 && stride>=width && pixels);
    ++calls;uint8_t* output=pixels;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        output[y*stride+x]=size ? data[(y*width+x)%size]:0;
    return 0;
}
int prefix_region_sum(const uint8_t* data,int size,int* sum,int width,int height,void* pixels,int stride)
{
    int result=prefix_region_render(data,size,width,height,pixels,stride);assert(sum);
    *sum=0;for(int i=0;i<size;++i)*sum+=data[i];return result;
}
static int render(PrefixRegionRoot* root,const uint8_t* data,int size,int width,int height,void* pixels,int stride)
{assert(root);return prefix_region_render(data,size,width,height,pixels,stride);}
int prefix_region_create(PrefixRegionRoot** output)
{PrefixRegionRoot* root=xrtCalloc(1,sizeof(*root));if(!root)return -1;root->render=render;*output=root;return 0;}
void prefix_region_destroy(PrefixRegionRoot* root){xrtFree(root);}
static bool destroy(void* raw){prefix_region_destroy(raw);return true;}
const XL6_ForeignViewDescriptorV1 prefixRegionPolicy={
    .size=sizeof(prefixRegionPolicy),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,.family="span.region.prefix",
    .watchAlignment=1,.Destroy=destroy,.callAlignment=1
};
int prefix_region_calls(void){return calls;}
void prefix_region_budget(int count){assert(xrtMemDebugFailAfter(count));}
