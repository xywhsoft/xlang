#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/font_view.h"
#include "platform/path.h"
#include "platform/window_state.h"
#include "services/editor_font.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"dpi_test phase=%d line=%d: %s; %s\n",f->phase,__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
typedef struct fixture { xide_app app; HDESK desktop; char* directory; int phase,failed,started; xide_document *c,*xl; uint32_t versions[2]; xui_code_selection_t selections[2]; xui_font_metrics_t metrics; } fixture;
static int closeInt(int a,int b) { return abs(a-b)<=2; }
static int layout(xide_app* app)
{
    int ids[]={XIDE_XLANG_DEMO,XIDE_HELP,XIDE_COMMANDS,XIDE_WORKSPACE,XIDE_PROPERTIES}; xui_dock_pane_info_t panes[5]={0};
    for(int i=0;i<5;++i) { xui_dock_window_info_t w={0}; w.iSize=sizeof(w); panes[i].iSize=sizeof(panes[i]);
        if(xuiDockPanelGetWindowInfo(app->dock,i==0 ? app->activeDocument->window : app->windows[ids[i]],&w)!=XUI_OK || xuiDockPanelGetPaneInfo(app->dock,w.iPane,&panes[i])!=XUI_OK) return 0; }
    return closeInt(panes[0].tRect.fX,panes[1].tRect.fX) && closeInt(panes[0].tRect.fW,panes[1].tRect.fW) &&
        panes[0].tRect.fW>250 && panes[1].tRect.fY>=panes[0].tRect.fY+panes[0].tRect.fH &&
        closeInt(panes[2].tRect.fY+panes[2].tRect.fH,panes[1].tRect.fY+panes[1].tRect.fH) &&
        closeInt(panes[3].tRect.fX,panes[4].tRect.fX) && closeInt(panes[3].tRect.fW,panes[4].tRect.fW) &&
        panes[4].tRect.fY>=panes[3].tRect.fY+panes[3].tRect.fH && closeInt(panes[4].tRect.fY+panes[4].tRect.fH,panes[1].tRect.fY+panes[1].tRect.fH);
}
static int input(xide_app* app,int type,float x,float y,const char* text)
{
    xge_input_event_t e={0}; e.iSize=sizeof(e); e.iType=type; e.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC;
    e.fX=x*(float)xgeGetWidth()/app->viewportWidth; e.fY=y*(float)xgeGetHeight()/app->viewportHeight;
    if(type==XGE_EVENT_MOUSE_DOWN || type==XGE_EVENT_MOUSE_UP) { e.iButton=XGE_MOUSE_LEFT; e.iButtons=type==XGE_EVENT_MOUSE_DOWN ? XGE_MOUSE_LEFT : 0; }
    e.sText=text; e.iTextSize=text ? (int)strlen(text) : 0; e.iCursor=e.iSelectStart=e.iSelectEnd=e.iTextSize;
    return xgeInputEventPost(&e)==XGE_OK;
}
static int inspect(fixture* f,float scale)
{
    xide_app* app=&f->app; xui_font_metrics_t logical={0},physical={0}; xui_code_selection_t selection={0};
    if(!layout(app) || fabsf(app->dpiScale-scale)>0.001f || fabsf(xuiGetVirtualDpi(app->context)-scale)>0.001f ||
       app->proxy.fontGetMetrics(&app->proxy,app->codeFont,&logical)!=XUI_OK || xideDpiFontPixelMetrics(app->dpiProxy,app->codeFont,&physical)!=XUI_OK ||
       memcmp(&logical,&f->metrics,sizeof(logical)) || fabsf(physical.fSize-logical.fSize*scale)>0.01f) return 0;
    xide_document* docs[]={f->xl,f->c};
    for(int i=0;i<2;++i) { selection.iSize=sizeof(selection);
        if(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(docs[i]->editor))!=f->versions[i] || xuiCodeSelectionGetState(xuiCodeEditGetSelection(docs[i]->editor),&selection)!=XUI_OK ||
           selection.iAnchorOffset!=f->selections[i].iAnchorOffset || selection.iCaretOffset!=f->selections[i].iCaretOffset || !docs[i]->dirty) return 0; }
    return !strcmp(f->xl->language,"xlang") && !strcmp(f->c->language,"c") && !app->frameError;
}
static int retainedTextPixels(xui_proxy proxy,xui_font font)
{
    const char* context="X office Z"; int okay=0,stage=0; xui_surface target=NULL; xui_draw_context draw=NULL;
    xui_text_shape_t shape={0}; xui_vec2_t measured={0},range={0};
    xui_text_item_t item={0}; item.iSize=sizeof(item); item.pFont=font; item.sText=context+2; item.iTextSize=6;
    item.iFlags=XUI_TEXT_SHAPE_DEFAULT|XUI_TEXT_SHAPE_RETAIN_PAINT;
    item.sContext=context; item.iContextSize=-1; item.iContextOffset=2;
    xui_surface_desc_t desc={XUI_SURFACE_KIND_TEXTURE,XUI_SURFACE_FORMAT_RGBA8,256,40,XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED};
    size_t bytes=256*40*4; unsigned char* first=calloc(1,bytes); unsigned char* second=calloc(1,bytes);
    xui_text_paint_span_t span={0}; span.iSize=sizeof(span); span.iStart=0; span.iEnd=6; span.iColor=0xff20a040u;
    if(!first || !second || proxy->textShape(proxy,&item,&shape)!=XUI_OK || !shape.pPaint || !shape.iPaintBytes) goto cleanup;
    stage=1;
    int measureResult=proxy->textMeasure(proxy,&item,&measured);
    int rangeResult=proxy->textShapeRangeMeasure(proxy,&shape,0,6,&range);
    if(measureResult!=XUI_OK || rangeResult!=XUI_OK ||
       fabsf(range.fX-shape.fWidth)>0.01f || fabsf(ceilf(shape.fWidth)-measured.fX)>0.01f || measured.fX<=0) {
        fprintf(stderr,"retained geometry: measure=%d range=%d width=%g measured=%g retained=%g\n",measureResult,rangeResult,shape.fWidth,measured.fX,range.fX); goto cleanup;
    }
    if(proxy->surfaceCreate(proxy,&target,&desc)!=XUI_OK || proxy->surfaceClear(proxy,target,0)!=XUI_OK || proxy->drawBegin(proxy,&draw,target)!=XUI_OK) goto cleanup;
    stage=2;
    if(proxy->drawTextSpans(proxy,draw,&item,(xui_rect_t){0,0,256,40},XUI_COLOR_WHITE,XUI_TEXT_CLIP,&span,1)!=XUI_OK) goto cleanup;
    if(proxy->drawEnd(proxy,draw)!=XUI_OK) { draw=NULL; goto cleanup; } draw=NULL;
    if(proxy->surfaceReadRGBA(proxy,target,first,256*4)!=XUI_OK || proxy->surfaceClear(proxy,target,0)!=XUI_OK || proxy->drawBegin(proxy,&draw,target)!=XUI_OK) goto cleanup;
    stage=3;
    if(proxy->drawTextShapeRangeSpans(proxy,draw,&shape,0,6,(xui_rect_t){0,0,256,40},XUI_COLOR_WHITE,XUI_TEXT_CLIP,0,&span,1)!=XUI_OK) goto cleanup;
    if(proxy->drawEnd(proxy,draw)!=XUI_OK) { draw=NULL; goto cleanup; } draw=NULL;
    if(proxy->surfaceReadRGBA(proxy,target,second,256*4)!=XUI_OK || memcmp(first,second,bytes)) goto cleanup;
    int painted=0; for(size_t i=3;i<bytes;i+=4) if(first[i]) ++painted;
    okay=painted>10;
cleanup:
    if(!okay) fprintf(stderr,"XGE 3 retained text pixel check stage=%d\n",stage);
    if(draw) proxy->drawEnd(proxy,draw);
    if(target) proxy->surfaceDestroy(proxy,target);
    xuiTextShapeFree(&shape); free(first); free(second); return okay;
}
static int pixels(void)
{
    xide_dpi_proxy* state=NULL; xui_proxy_t proxy={0}; xui_surface target=NULL,wide=NULL,image=NULL; xui_font font=NULL; xui_draw_context draw=NULL; char* path=NULL; int okay=0,w=0,h=0,stage=0;
    unsigned char rgba[12*2]={255,0,0,255,0,255,0,255,99,99,99,99,0,0,255,255,255,255,255,255,99,99,99,99};
    unsigned char bytes[48*36*4]={0},logical[36*24*4]; xui_font_metrics_t before={0},after={0}; xui_surface_sampler_t sampler={XUI_SURFACE_FILTER_NEAREST,XUI_SURFACE_FILTER_NEAREST,XUI_SURFACE_WRAP_CLAMP,XUI_SURFACE_WRAP_CLAMP},readSampler={0}; xui_rect_t clipped={0}; int hasClip=0;
    xui_surface_desc_t description={XUI_SURFACE_KIND_TEXTURE,XUI_SURFACE_FORMAT_RGBA8,32,24,XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED};
    if(xideDpiProxyCreate(&state,&proxy,1.5f)!=XUI_OK || proxy.surfaceCreate(&proxy,&target,&description)!=XUI_OK || proxy.surfaceClear(&proxy,target,0)!=XUI_OK || proxy.drawBegin(&proxy,&draw,target)!=XUI_OK) goto cleanup;
    stage=1; if(xideDpiProxySetScale(state,2)!=XUI_ERROR_INVALID_ARGUMENT || proxy.drawClipSet(&proxy,draw,(xui_rect_t){8,4,12,8})!=XUI_OK || proxy.drawClipGet(&proxy,draw,&clipped,&hasClip)!=XUI_OK || !hasClip || clipped.fX!=8 || clipped.fY!=4 || clipped.fW!=12 || clipped.fH!=8 || proxy.drawRectFill(&proxy,draw,(xui_rect_t){0,0,32,24},0xff0000ffu)!=XUI_OK) goto cleanup;
    if(proxy.drawEnd(&proxy,draw)!=XUI_OK) { draw=NULL; goto cleanup; } draw=NULL;
    stage=2; if(xideDpiSurfaceReadPixels(state,target,bytes,48*4,&w,&h)!=XUI_OK || w!=48 || h!=36) goto cleanup;
    stage=3; for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const unsigned char* color=bytes+(y*w+x)*4; int inside=x>=12 && x<30 && y>=6 && y<18;
        if(inside ? color[0]!=255 || color[1] || color[2] || color[3]!=255 : color[0] || color[1] || color[2] || color[3]) goto cleanup;
    }
    stage=4; memset(logical,0x5a,sizeof(logical)); if(proxy.surfaceReadRGBA(&proxy,target,logical,36*4)!=XUI_OK) goto cleanup;
    for(int y=0;y<24;++y) for(int x=32*4;x<36*4;++x) if(logical[y*36*4+x]!=0x5a) goto cleanup;
    stage=5; if(proxy.surfaceCreateRGBA(&proxy,&image,2,2,rgba,12,XUI_SURFACE_ALPHA_PREMULTIPLIED)!=XUI_OK) goto cleanup;
    unsigned char restored[24]={0}; if(proxy.surfaceReadRGBA(&proxy,image,restored,12)!=XUI_OK || memcmp(restored,rgba,8) || memcmp(restored+12,rgba+12,8)) goto cleanup;
    stage=6; if(proxy.surfaceSetSampler(&proxy,target,&sampler)!=XUI_OK || proxy.surfaceUpdateRGBA(&proxy,target,(xui_rect_i_t){2,2,2,2},rgba,12)!=XUI_OK || xideDpiSurfaceReadPixels(state,target,bytes,48*4,&w,&h)!=XUI_OK || bytes[(3*48+3)*4]!=255 || bytes[(3*48+3)*4+1] || bytes[(5*48+5)*4]!=255 || bytes[(5*48+5)*4+1]!=255) goto cleanup;
    proxy.surfaceDestroy(&proxy,image); image=NULL;
    if(proxy.surfaceCreateRGBA(&proxy,&image,2,2,rgba,12,XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED)!=XUI_OK || proxy.surfaceReadRGBA(&proxy,image,restored,12)!=XUI_OK || memcmp(restored,rgba,8) || memcmp(restored+12,rgba+12,8) || xideDpiSurfaceReadPixels(state,image,NULL,0,&w,&h)!=XUI_OK || w!=3 || h!=3) goto cleanup;
    stage=7; path=xideWindowsFontPath("consola.ttf"); if(!path || proxy.fontLoadFile(&proxy,&font,path,16,XUI_FONT_FORMAT_TTF)!=XUI_OK || xideDpiFontPixelMetrics(state,font,&before)!=XUI_OK || fabsf(before.fSize-24)>0.01f) goto cleanup;
    stage=8; description.iWidth=5000; description.iHeight=1;
    if(proxy.surfaceCreate(&proxy,&wide,&description)!=XUI_OK || xideDpiProxySetScale(state,8)!=XUI_ERROR_INVALID_ARGUMENT || xideDpiProxyScale(state)!=1.5f || xideDpiFontPixelMetrics(state,font,&after)!=XUI_OK || memcmp(&before,&after,sizeof(before)) || xideDpiSurfaceReadPixels(state,target,NULL,0,&w,&h)!=XUI_OK || w!=48 || h!=36) goto cleanup;
    stage=9; if(xideDpiProxySetScale(state,1)!=XUI_OK || xideDpiFontPixelMetrics(state,font,&after)!=XUI_OK || fabsf(after.fSize-16)>0.01f || xideDpiSurfaceReadPixels(state,target,NULL,0,&w,&h)!=XUI_OK || w!=32 || h!=24 || proxy.surfaceGetSampler(&proxy,target,&readSampler)!=XUI_OK || memcmp(&sampler,&readSampler,sizeof(sampler))) goto cleanup;
    okay=1;
cleanup:
    if(!okay) fprintf(stderr,"dpi pixel check stage=%d; at (3,3)=%u,%u,%u,%u at (5,5)=%u,%u,%u,%u\n",stage,bytes[(3*48+3)*4],bytes[(3*48+3)*4+1],bytes[(3*48+3)*4+2],bytes[(3*48+3)*4+3],bytes[(5*48+5)*4],bytes[(5*48+5)*4+1],bytes[(5*48+5)*4+2],bytes[(5*48+5)*4+3]);
    if(draw) proxy.drawEnd(&proxy,draw);
    free(path); xideDpiProxyDestroy(state); return okay;
}
static int verify(void* user)
{
    fixture* f=user; xide_app* app=&f->app; xide_model_error error={0}; f->started=1;
    if(f->phase==0) {
        app->hidden=1; app->noSession=1; app->maxFrames=10000; app->highDpi=1; app->dpiOverride=1;
        xge_build_info_t build={0}; build.size=sizeof(build);
        CHECK(xgeGetBuildInfo(&build)==XGE_OK && build.api_major==XGE_VERSION_MAJOR &&
              build.abi_fingerprint==XGE_BUILD_ABI_FINGERPRINT);
        CHECK(xideAppCreate(app)==XUI_OK && pixels());
        f->xl=xideDocumentNew(&app->documents,"xlang",&error); f->c=xideDocumentNew(&app->documents,"c",&error); CHECK(f->xl && f->c);
        CHECK(xuiCodeEditSetText(f->xl->editor,"func main(...) as int\n    int 中文 = 3\n    return 中文\nend\n")==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(f->xl->editor),0,"// kept\n")==XUI_OK);
        CHECK(xuiCodeEditSetText(f->c->editor,"#include <stdio.h>\nint main(void) { return 0; }\n")==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(f->c->editor),0,"/* kept */\n")==XUI_OK);
        CHECK(xideEditorShow(app,f->xl)==XUI_OK && xideEditorShow(app,f->c)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK);
        xide_document* docs[]={f->xl,f->c};
        for(int i=0;i<2;++i) { f->versions[i]=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(docs[i]->editor)); f->selections[i].iSize=sizeof(f->selections[i]); CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(docs[i]->editor),&f->selections[i])==XUI_OK); }
        CHECK(app->proxy.fontGetMetrics(&app->proxy,app->codeFont,&f->metrics)==XUI_OK);
        CHECK(xideDpiProxySetScale(app->dpiProxy,NAN)==XUI_ERROR_INVALID_ARGUMENT && xideDpiProxyScale(app->dpiProxy)==1);
        f->phase=1;
    } else if(f->phase<=5) {
        float scale=1+(f->phase-1)*0.25f; app->dpiOverride=scale;
        CHECK(xideAppFrame(app)==XGE_OK && !app->frameError);
        CHECK(xideWorkbenchReset(app)==XUI_OK && xideEditorShow(app,f->phase==5 ? f->xl : f->c)==XUI_OK);
        CHECK(xideAppFrame(app)==XGE_OK && inspect(f,scale));
        CHECK(retainedTextPixels(&app->proxy,app->codeFont));
        char name[64]; snprintf(name,sizeof(name),"native-dpi-%d.png",100+(f->phase-1)*25); char* image=xidePathResolve(f->directory,name); CHECK(image);
        int captured=xideAppCapture(app,image); free(image); CHECK(captured==XUI_OK);
        int w=0,h=0; CHECK(xideDpiSurfaceReadPixels(app->dpiProxy,app->target,NULL,0,&w,&h)==XUI_OK && closeInt(w,xgeGetWidth()) && closeInt(h,xgeGetHeight()));
        CHECK(xideFontViewOpen(app) && xideAppFrame(app)==XGE_OK);
        xui_widget field=xideFontViewControl(app,XIDE_FONT_PATH); CHECK(xuiSetFocusWidget(app->context,field)==XUI_OK);
        CHECK(input(app,XGE_EVENT_IME_START,0,0,NULL) && input(app,XGE_EVENT_IME_UPDATE,0,0,"中文") && xideAppFrame(app)==XGE_OK);
        xge_rect_t candidate={0}; xui_rect_t r=xuiWidgetGetWorldRect(field); float sx=(float)xgeGetWidth()/app->viewportWidth,sy=(float)xgeGetHeight()/app->viewportHeight;
        CHECK(xgeImeGetCandidateRect(&candidate)==XGE_OK && candidate.fH>0 && candidate.fX>=r.fX*sx-2 && candidate.fX<=(r.fX+r.fW)*sx+2 && candidate.fY>=r.fY*sy-2 && candidate.fY<=(r.fY+r.fH)*sy+2);
        r=xuiWidgetGetWorldRect(xideFontViewControl(app,XIDE_FONT_CANCEL));
        CHECK(input(app,XGE_EVENT_MOUSE_DOWN,r.fX+r.fW/2,r.fY+r.fH/2,NULL) && input(app,XGE_EVENT_MOUSE_UP,r.fX+r.fW/2,r.fY+r.fH/2,NULL) && xideAppFrame(app)==XGE_OK && !xideFontViewIsOpen(app));
        CHECK(inspect(f,scale)); ++f->phase;
    } else {
        app->dpiOverride=0; CHECK(xideAppFrame(app)==XGE_OK);
        float nativeScale=xideWindowDpi(xgePlatformNativeHandle())/96.0f;
        CHECK(inspect(f,nativeScale));
        xui_font stable=app->codeFont;
        CHECK(SetWindowPos((HWND)xgePlatformNativeHandle(),NULL,0,0,1599,997,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE));
        CHECK(xideAppFrame(app)==XGE_OK && app->codeFont==stable && fabsf(app->dpiScale-nativeScale)<0.0001f);
        CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(f->c->editor))==XUI_OK && !strstr(xuiCodeEditGetText(f->c->editor),"kept"));
        CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(f->xl->editor))==XUI_OK && !strstr(xuiCodeEditGetText(f->xl->editor),"kept"));
        CHECK(xgeInputEventDroppedCount()==0 && !app->frameError);
        puts("dpi_test passed: real high-DPI XGE/XUI at 100/125/150/175/200 percent, center-only bottom/right split, physical render surfaces and font instances, stable logical metrics, C/xlang versions/selection/undo, ordered physical pointer hit testing, modal input and IME candidate mapping, pixel-exact clip/raster/stride contracts, transactional resource failure, invalid scale preservation and native screenshots."); f->phase=7; goto cleanup;
    }
    return XGE_OK;
cleanup:
    xideAppDestroy(app); xgeSetQuitRequestCallback(NULL,NULL); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1;
    xge_desc_t desc={0}; desc.iWidth=1600; desc.iHeight=1000; desc.sTitle="xlang IDE DPI verification"; desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; desc.iRunMode=XGE_RUN_GAME_LOOP; desc.iTargetFPS=30;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,f); xideAppDestroy(&f->app); xgeUnit(); return result==XGE_OK && f->started && !f->failed && f->phase==7 ? 0 : 1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) { return 2; } fixture f={0}; f.directory=argv[1]; wchar_t name[100]; swprintf(name,100,L"XlangIDEDpiVerification-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
    if(thread) { if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0) { fputs("dpi_test timed out.\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); }
    CloseDesktop(f.desktop); return (int)code;
}
