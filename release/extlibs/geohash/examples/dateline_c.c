#define XRT_MODULE_STRING
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_NUMBER_FLOAT
#include "../geohash_native.c"
#include <stdio.h>

int main(void)
{
    char *edge=xgeohash_encode(0,180,1), *east=NULL, *west=NULL, *bbox=NULL, *polar=NULL, *north=NULL, *recovered=NULL;
    int result=1;
    if (!edge) goto done;
    east=xgeohash_neighbor(edge,2);
    if (!east) goto done;
    west=xgeohash_neighbor(east,3); bbox=xgeohash_bbox(east);
    if (!west || !bbox) goto done;
    printf("edge=%s\neast=%s\nwest=%s\neastBox=%s\n",edge,east,west,bbox);
    polar=xgeohash_encode(90,0,1);
    if (!polar) goto done;
    north=xgeohash_neighbor(polar,0);
    if (north || xrtErrorKind(xrtGetError())!=XERR_RANGE) goto done;
    puts("north=blocked");
    // 失败后可直接再次调用；每个返回字符串仍由调用方独立释放。
    recovered=xgeohash_encode(0,0,1);
    if (!recovered || xrtGetError()) goto done;
    printf("recovered=%s\n",recovered);
    result=ferror(stdout) ? 1 : 0;
done:
    xrtFree(recovered); xrtFree(north); xrtFree(polar); xrtFree(bbox);
    xrtFree(west); xrtFree(east); xrtFree(edge);
    return result;
}
