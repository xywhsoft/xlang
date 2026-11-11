#ifndef XL6_FOREIGN_SPAN_REGION_PREFIX_H
#define XL6_FOREIGN_SPAN_REGION_PREFIX_H
#include <stddef.h>
#include <stdint.h>
#include <__xl_runtime/xl6_foreign_view_abi.h>
typedef struct PrefixRegionRoot PrefixRegionRoot;
struct PrefixRegionRoot {int (*render)(PrefixRegionRoot*,const uint8_t*,int,int,int,void*,int);};
extern const XL6_ForeignViewDescriptorV1 prefixRegionPolicy;
int prefix_region_create(PrefixRegionRoot**);
void prefix_region_destroy(PrefixRegionRoot*);
int prefix_region_render(const uint8_t*,int,int,int,void*,int);
int prefix_region_sum(const uint8_t*,int,int*,int,int,void*,int);
int prefix_region_calls(void);
void prefix_region_budget(int);
/* Small header authority lets an independently embedded compiler exercise
 * both projections without resolving a second provider/library. */
static inline int prefix_region_inline(const uint8_t* data,int size,int width,int height,void* pixels,int stride)
{
    if(size<0 || width<0 || height<0 || stride<width || !pixels || (size && !data))return -1;
    uint8_t* output=(uint8_t*)pixels;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        output[y*stride+x]=size ? data[(y*width+x)%size]:0;
    return 0;
}
#endif
