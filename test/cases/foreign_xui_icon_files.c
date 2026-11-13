/* Actual owned CPU frame controls. File names are opaque markers: these
 * callbacks check native path retention/dispatch, not image decoding or I/O. */
#include "test/cases/foreign_xui_icon_files.h"
#include "test/cases/foreign_xui_render_backend.c"
static char expectedPaths[2][256];
static const char* callerPaths[2];
static unsigned observedFiles;
static xui_surface_load_file_proc originalRasterFile;
static xui_surface_load_svg_file_proc originalSvgFile;
static void checkPath(int raster,const char* path)
{
    assert(path && callerPaths[raster] && path!=callerPaths[raster]);
    assert(!strcmp(path,expectedPaths[raster]));observedFiles|=1u<<raster;
}
static int rasterFile(xui_proxy proxy,xui_surface* output,const char* path,uint32_t flags)
{checkPath(1,path);return originalRasterFile(proxy,output,path,flags);}
static int svgFile(xui_proxy proxy,xui_surface* output,const char* path,int width,int height,uint32_t flags)
{checkPath(0,path);return originalSvgFile(proxy,output,path,width,height,flags);}
int fixtureIconFilesProxy(xui_proxy* output)
{
    int status=fixtureRenderProxy(output);if(status!=XUI_OK)return status;
    originalRasterFile=backend.tProxy.surfaceLoadFile;originalSvgFile=backend.tProxy.surfaceLoadSvgFile;
    backend.tProxy.surfaceLoadFile=rasterFile;backend.tProxy.surfaceLoadSvgFile=svgFile;
    observedFiles=0;memset(callerPaths,0,sizeof(callerPaths));return XUI_OK;
}
void fixtureIconFilesObserve(int raster,const char* path)
{
    assert((raster==0 || raster==1) && path && strlen(path)<256);
    strcpy(expectedPaths[raster],path);callerPaths[raster]=path;
}
int fixtureIconFilesLoaded(void){return observedFiles==3;}
static xmemdebugsnapshot filesBefore;
void fixtureIconFilesHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&filesBefore);}
int fixtureIconFilesHeapUnchanged(void)
{xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);return now.AllocCount==filesBefore.AllocCount && now.FreeCount==filesBefore.FreeCount && now.ReallocCount==filesBefore.ReallocCount;}
void fixtureIconFilesHeapReport(int native)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    printf("Icon batch heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(now.AllocCount-filesBefore.AllocCount),
        (unsigned long long)(now.FreeCount-filesBefore.FreeCount),
        (unsigned long long)(now.ReallocCount-filesBefore.ReallocCount));
}
void fixtureIconFilesNativePerformance(xui_icon_category category,xui_widget picker)
{
    for(int i=0;i<1000;++i){
        assert(xuiIconCategoryBeginUpdate(category)==XUI_OK);
        assert(xuiIconCategoryEndUpdate(category)==XUI_OK);
        xuiIconClear(category);
        float border=0,scrollbar=0;
        assert(xuiIconPickerGetMetrics(picker,&border,&scrollbar)==XUI_OK && border==2 && scrollbar==6);
    }
}
static size_t failingRefs=1;
static bool failingHold(const void* raw){assert(raw==&failingRefs);++failingRefs;return true;}
static bool failingDrop(const void* raw){assert(raw==&failingRefs && failingRefs>1);--failingRefs;return false;}
static bool failingCount(const void* raw,size_t* output){assert(raw==&failingRefs);*output=failingRefs;return true;}
static bool failingTrace(const void* raw,xrtownershipvisitor visit,void* user)
{assert(raw==&failingRefs);(void)visit;(void)user;return true;}
static const xrtownershipops failingOps={failingCount,failingTrace};
static int emptyDraw(xui_icon icon,xui_painter painter,xui_rect_t rect,const xui_icon_draw_desc_t* style,void* user)
{(void)icon;(void)painter;(void)rect;(void)style;(void)user;return XUI_OK;}
int fixtureIconFilesFailingIcon(xui_icon_category category,xui_icon* output)
{
    assert(failingRefs==1);
    const xui_callback_owner_t owner={{&failingRefs,&failingOps},failingHold,failingDrop};
    return xuiIconAddCustomOwned(category,"failure",emptyDraw,NULL,&owner,NULL,NULL,NULL,
        NULL,NULL,NULL,NULL,NULL,NULL,NULL,output);
}
int fixtureIconFilesDropBalanced(void){return failingRefs==1;}
