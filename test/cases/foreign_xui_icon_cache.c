/* The compiler discovers this same-stem C companion once. The provider has
 * real owning controls and process-owned code; no window/GPU or network. */
#include "test/cases/foreign_xui_icon_cache.h"
#include "test/cases/foreign_xui_render_backend.c"
static unsigned char expectedInput[2][64];
static int expectedSize[2];static uintptr_t originalInput[2];static unsigned copiedInputs;
static xui_surface_load_memory_proc originalRasterLoad;
static xui_surface_load_svg_memory_proc originalSvgLoad;
static void checkInput(int raster,const void* data,int size)
{
    assert(size==expectedSize[raster] && size>0 && (uintptr_t)data!=originalInput[raster]);
    assert(!memcmp(data,expectedInput[raster],(size_t)size));copiedInputs|=1u<<raster;
}
static int cacheRasterLoad(xui_proxy proxy,xui_surface* output,const void* data,int size,uint32_t flags)
{checkInput(1,data,size);return originalRasterLoad(proxy,output,data,size,flags);}
static int cacheSvgLoad(xui_proxy proxy,xui_surface* output,const void* data,int size,int width,int height,uint32_t flags)
{checkInput(0,data,size);return originalSvgLoad(proxy,output,data,size,width,height,flags);}
int fixtureCacheProxy(xui_proxy* output)
{
    int status=fixtureRenderProxy(output);if(status!=XUI_OK)return status;
    originalRasterLoad=backend.tProxy.surfaceLoadMemory;originalSvgLoad=backend.tProxy.surfaceLoadSvgMemory;
    backend.tProxy.surfaceLoadMemory=cacheRasterLoad;backend.tProxy.surfaceLoadSvgMemory=cacheSvgLoad;
    copiedInputs=0;return XUI_OK;
}
void fixtureCacheObserveInput(int raster,const void* data,int size)
{
    assert((raster==0 || raster==1) && data && size>0 && size<=64);
    memcpy(expectedInput[raster],data,(size_t)size);expectedSize[raster]=size;originalInput[raster]=(uintptr_t)data;
}
int fixtureCacheCopiedInputs(void){return copiedInputs==3;}
int fixtureCacheLoads(int raster)
{return raster?backend.iSurfaceLoadCount:backend.iSvgSurfaceLoadCount;}
static xmemdebugsnapshot mutationBefore;
void fixtureCacheHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&mutationBefore);}
int fixtureCacheHeapUnchanged(void)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    return now.AllocCount==mutationBefore.AllocCount && now.FreeCount==mutationBefore.FreeCount && now.ReallocCount==mutationBefore.ReallocCount;
}
