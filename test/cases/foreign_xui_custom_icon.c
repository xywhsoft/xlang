/* Same-stem companion; the compiler discovers it once. The provider is an
 * actual owning CPU frame backend, with test-only payload observation. */
#include "test/cases/foreign_xui_custom_icon.h"
#include "test/cases/foreign_xui_render_backend.c"
static xui_draw_surface_quad_proc originalQuad;
static xui_draw_mesh_triangles_proc originalMesh;
static xui_draw_path_proc originalPath;
static xui_draw_svg_path_proc originalSvg;
static unsigned observedFlags;static bool observing;
static int quad(xui_proxy proxy,xui_draw_context draw,xui_surface surface,const xui_surface_vertex_t* vertices,uint32_t flags)
{
    if(observing){assert(vertices && vertices[0].fX==0 && vertices[1].fX==16 && vertices[2].fY==16 && vertices[3].fY==16);for(int i=0;i<4;++i)assert(vertices[i].fW==1 && vertices[i].iColor==XUI_COLOR_WHITE);observedFlags|=1;}
    return originalQuad(proxy,draw,surface,vertices,flags);
}
static int mesh(xui_proxy proxy,xui_draw_context draw,const xui_mesh_vertex_t* vertices,int count,const uint32_t* indices,int indexCount,uint32_t flags)
{
    if(observing){assert(count==3 && indexCount==3 && vertices[1].fX==16 && vertices[2].fY==16 && indices[0]==0 && indices[1]==1 && indices[2]==2);observedFlags|=2;}
    return originalMesh(proxy,draw,vertices,count,indices,indexCount,flags);
}
static void style(const xui_path_style_t* value,unsigned flag)
{
    assert(value && value->iSize==sizeof(*value));
    if(value->iDashCount){assert(value->iDashCount==2 && value->pDashPattern[0]==2 && value->pDashPattern[1]==3 && value->iStrokeColor==XUI_COLOR_WHITE && value->fStrokeWidth==1);observedFlags|=flag;}
}
static int path(xui_proxy proxy,xui_draw_context draw,const xui_path_command_t* commands,int count,const xui_path_style_t* value,float tolerance)
{if(observing)style(value,4);return originalPath(proxy,draw,commands,count,value,tolerance);}
static int svg(xui_proxy proxy,xui_draw_context draw,const char* text,xui_rect_t viewBox,xui_rect_t target,const xui_path_style_t* value,float tolerance)
{if(observing)style(value,8);return originalSvg(proxy,draw,text,viewBox,target,value,tolerance);}
int fixtureCustomOtherProxy(xui_proxy* output)
{
    xui_proxy_t table;adapt(&table);const xui_callback_owner_t owner={{&references,&operations},hold,drop};
    return xuiProxyCreateOwned(&table,&owner,output);
}
fixture_custom_quad fixtureCustomQuad(void)
{
    const fixture_custom_quad value={{{0,0,0,1,0,0,XUI_COLOR_WHITE},{16,0,0,1,0,0,XUI_COLOR_WHITE},
        {16,16,0,1,0,0,XUI_COLOR_WHITE},{0,16,0,1,0,0,XUI_COLOR_WHITE}}};
    return value;
}
void fixtureCustomMesh(void* vertexData,size_t vertexBytes,void* indexData,size_t indexBytes)
{
    const xui_mesh_vertex_t vertices[]={{0,0,XUI_COLOR_WHITE},{16,0,XUI_COLOR_WHITE},{0,16,XUI_COLOR_WHITE}};
    const uint32_t indices[]={0,1,2};assert(vertexBytes==sizeof(vertices) && indexBytes==sizeof(indices));
    memcpy(vertexData,vertices,sizeof(vertices));memcpy(indexData,indices,sizeof(indices));
}
void fixtureCustomDashes(void* output,size_t bytes)
{const float values[]={2,3};assert(bytes==sizeof(values));memcpy(output,values,sizeof(values));}
void fixtureCustomDrawBegin(void)
{
    if(backend.tProxy.drawSurfaceQuad!=quad){
        originalQuad=backend.tProxy.drawSurfaceQuad;originalMesh=backend.tProxy.drawMeshTriangles;
        originalPath=backend.tProxy.drawPath;originalSvg=backend.tProxy.drawSvgPath;
        backend.tProxy.drawSurfaceQuad=quad;backend.tProxy.drawMeshTriangles=mesh;backend.tProxy.drawPath=path;backend.tProxy.drawSvgPath=svg;
    }
    observedFlags=0;observing=true;
}
void fixtureCustomDrawEnd(void){observing=false;assert(observedFlags==15);}
static xmemdebugsnapshot customBefore;
void fixtureCustomHeapBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&customBefore);}
int fixtureCustomHeapUnchanged(void)
{xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);return now.AllocCount==customBefore.AllocCount && now.FreeCount==customBefore.FreeCount && now.ReallocCount==customBefore.ReallocCount;}
static unsigned customNativeCalls;
static int nativeDraw(xui_icon icon,xui_painter painter,xui_rect_t rect,const xui_icon_draw_desc_t* style,void* user)
{(void)icon;(void)painter;(void)rect;(void)style;(void)user;return XUI_OK;}
static int nativeMeasure(xui_icon icon,xui_vec2_t* size,void* user)
{(void)icon;(void)user;++customNativeCalls;*size=(xui_vec2_t){16,16};return XUI_OK;}
static int nativePrepare(xui_icon icon,int width,int height,void* user)
{(void)icon;(void)user;++customNativeCalls;assert(width==16 && height==16);return XUI_OK;}
int fixtureCustomNative(xui_icon_category category,xui_icon* output)
{
    customNativeCalls=0;
    return xuiIconAddCustomOwned(category,"native",nativeDraw,NULL,NULL,nativeMeasure,NULL,NULL,
        nativePrepare,NULL,NULL,NULL,NULL,NULL,NULL,output);
}
void fixtureCustomPerformanceResult(int native)
{
    xmemdebugsnapshot now;xrtMemDebugSnapshot(&now);
    if(native)assert(customNativeCalls==2000);
    printf("Custom callback heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(now.AllocCount-customBefore.AllocCount),
        (unsigned long long)(now.FreeCount-customBefore.FreeCount),
        (unsigned long long)(now.ReallocCount-customBefore.ReallocCount));
}
