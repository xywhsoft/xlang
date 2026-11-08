/* Deterministic OS backend: UI acceptance never changes actual associations. */
#include "platform/file_association.h"
#include "ui/association_view.h"
#include "ui/catalog_view.h"
#include "ui/dialog.h"
#include "ui/settings_view.h"
#include "services/ui_font.h"
#include "theme/theme.h"
#include "catalog_ready.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static volatile LONG probes,registeredMask,defaultMask,opens,wrongThread,applies,applyError;
static DWORD uiThread;
static DWORD fakeProbe(const wchar_t* prefix,const wchar_t* exe,xide_association_result* out)
{
    (void)prefix;(void)exe;if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&wrongThread);
    Sleep(25);memset(out,0,sizeof(*out));LONG mask=InterlockedCompareExchange(&registeredMask,0,0);
    for(int i=0;i<4;++i){out->registered[i]=!!(mask&(1<<i));out->defaultKnown[i]=1;out->isDefault[i]=!!(mask&defaultMask&(1<<i));}
    InterlockedIncrement(&probes);return 0;
}
static DWORD fakeApply(const wchar_t* prefix,const wchar_t* exe,unsigned mask)
{
    (void)prefix;(void)exe;if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&wrongThread);
    Sleep(35);InterlockedIncrement(&applies);
    DWORD error=(DWORD)InterlockedCompareExchange(&applyError,0,0);
    if(!error)InterlockedExchange(&registeredMask,(LONG)mask);
    return error;
}
static void fakeDefaults(void){if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&wrongThread);InterlockedIncrement(&opens);}
#define xideAssociationProbe fakeProbe
#define xideAssociationApply fakeApply
#define xideAssociationOpenDefaults fakeDefaults
#include "../src/ui/association_view.c"
#undef xideAssociationProbe
#undef xideAssociationApply
#undef xideAssociationOpenDefaults
static int checks,testFailed,started;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"association_view_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
static int complete(xide_app* a)
{ULONGLONG until=GetTickCount64()+5000;while(a->associationView->job && GetTickCount64()<until){xideAssociationViewPoll(a);Sleep(1);}return !a->associationView->job;}
static int settle(xide_app* a)
{
    for(int i=0;i<3;++i){xideWorkbenchLayout(a);if(xuiDispatchPendingEvents(a->context)!=XUI_OK || xuiLayout(a->context)!=XUI_OK)return 0;}
    return !a->frameError;
}
static int click(xide_app* a,xui_widget button)
{
    if(!settle(a))return 0;
    xui_rect_t r=xuiWidgetGetWorldRect(button);int x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);
}
static void choose(struct xide_association_view* v,unsigned mask,int skip)
{for(int i=0;i<4;++i)xuiCheckBoxSetChecked(v->types[i],!!(mask&(1u<<i)));xuiCheckBoxSetChecked(v->skip,skip);}
static int buttonCount(xide_app* a,xui_widget parent)
{int count=xuiWidgetGetType(parent)==xuiButtonGetType(a->context);for(xui_widget child=xuiWidgetGetFirstChild(parent);child;child=xuiWidgetGetNextSibling(child))count+=buttonCount(a,child);return count;}
static uint32_t color(xui_widget widget,const char* key)
{xui_style_property_t p={0};p.iSize=sizeof(p);return xuiWidgetGetResolvedStyleProperty(widget,key,&p)==XUI_OK && p.tValue.iType==XUI_STYLE_VALUE_COLOR?p.tValue.iColor:0;}
static int ticksVisible(xide_app* a)
{
    size_t bytes=(size_t)a->viewportWidth*a->viewportHeight*4;unsigned char* pixels=malloc(bytes);if(!pixels)return 0;
    int okay=a->proxy.surfaceReadRGBA(&a->proxy,a->target,pixels,a->viewportWidth*4)==XUI_OK;
    for(int i=0;i<4 && okay;++i){
        xui_rect_t box=xuiCheckBoxGetIndicatorRect(a->associationView->types[i]),world=xuiWidgetGetWorldRect(a->associationView->types[i]);box.fX+=world.fX;box.fY+=world.fY;
        uint32_t mark=color(a->associationView->types[i],"checkbox.check.color"),fill=color(a->associationView->types[i],"choice.accent.color");int marks=0,fills=0;
        if(mark==fill || box.fW<8){okay=0;break;}
        for(int y=box.fY+3;y<box.fY+box.fH-3;++y)for(int x=box.fX+3;x<box.fX+box.fW-3;++x){
            if(x<0 || y<0 || x>=a->viewportWidth || y>=a->viewportHeight){okay=0;continue;}
            unsigned char* p=pixels+((size_t)y*a->viewportWidth+x)*4;
            int dm=abs((int)p[0]-(int)(mark>>24))+abs((int)p[1]-(int)((mark>>16)&255))+abs((int)p[2]-(int)((mark>>8)&255));
            int df=abs((int)p[0]-(int)(fill>>24))+abs((int)p[1]-(int)((fill>>16)&255))+abs((int)p[2]-(int)((fill>>8)&255));
            marks+=dm<40;fills+=df<40;
        }
        if(marks<4 || fills<8){printf("checkbox pixel failure theme=%d row=%d mark=%d fill=%d box=%d,%d,%d,%d\n",a->theme,i,marks,fills,box.fX,box.fY,box.fW,box.fH);okay=0;}
    }
    free(pixels);return okay;
}
static int footerFits(xide_app* a)
{
    struct xide_association_view* v=a->associationView;
    xui_rect_t skip=xuiWidgetGetWorldRect(v->skip),close=xuiWidgetGetWorldRect(v->close),confirm=xuiWidgetGetWorldRect(v->confirm),footer=xuiWidgetGetWorldRect(xideDialogGetFooterWidget(v->window)),body=xuiWidgetGetWorldRect(xideDialogGetScrollViewWidget(v->window));
    int fits=buttonCount(a,xideDialogGetFooterWidget(v->window))==2 && skip.fX>=footer.fX && skip.fX+skip.fW<=close.fX && close.fX+close.fW<=confirm.fX && confirm.fX+confirm.fW<=footer.fX+footer.fW && body.fY+body.fH<=footer.fY+1 && abs(2*skip.fY+skip.fH-2*close.fY-close.fH)<=2;
    if(!fits)printf("footer font=%d skip=(%d,%d,%d,%d) close=(%d,%d,%d,%d) OK=(%d,%d,%d,%d) footer=(%d,%d,%d,%d) body bottom=%d\n",a->uiFontSize,skip.fX,skip.fY,skip.fW,skip.fH,close.fX,close.fY,close.fW,close.fH,confirm.fX,confirm.fY,confirm.fW,confirm.fH,footer.fX,footer.fY,footer.fW,footer.fH,body.fY+body.fH);
    return fits;
}
static int run(void* raw)
{
    xide_app a={0};int failed=0,began=0;char settings[32768];xide_settings reread={0};xide_model_error error={0};uiThread=GetCurrentThreadId();started=1;
    CHECK(raw);snprintf(settings,sizeof(settings),"%s/association-settings.json",(const char*)raw);DeleteFileA(settings);
    const char seed[]="{\"format\":\"xlang-ide-settings-1\",\"associations\":{\"future\":88}}";
    FILE* f=fopen(settings,"wb");CHECK(f);fputs(seed,f);fclose(f);
    a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    a.hidden=0;a.frames=1;xideAssociationViewPoll(&a);CHECK(!probes && !a.associationView);
    CHECK(xideTestCatalogReady(&a) && xideSettingsViewOpen(&a,XIDE_SETTINGS_EDITOR));
    xideAssociationViewPoll(&a);CHECK(a.associationView && a.associationView->job && !xideAssociationStartupSettled(&a));
    CHECK(complete(&a) && xideAssociationStartupSettled(&a) && !xideAssociationViewIsOpen(&a));
    CHECK(a.associationView->promptPending && probes==1);
    CHECK(xideDialogSetOpen(xideSettingsViewWindow(&a),0)==XUI_OK);xideAssociationViewPoll(&a);
    CHECK(xideAssociationViewIsOpen(&a) && probes==1 && settle(&a));
    struct xide_association_view* v=a.associationView;xui_rect_t compact=xuiWidgetGetRect(v->window);
    CHECK(compact.fW<=500 && compact.fH<=310 && selectedMask(v)==7 && footerFits(&a));
    CHECK(!xuiWidgetGetVisible(v->status));
    for(int i=0;i<4;++i){CHECK(!strcmp(xuiLabelGetText(v->states[i]),"当前未关联") && color(v->states[i],"text.color")==ideThemeColor(a.theme,IDE_TEXT));CHECK(xuiWidgetGetParent(v->types[i])==xuiWidgetGetParent(v->states[i]));}
    CHECK(!strcmp(xuiButtonGetText(v->close),"关闭") && !strcmp(xuiButtonGetText(v->confirm),"确定"));
    CHECK(!xuiWindowIsResizable(v->window) && !xuiWindowGetShowMaximize(v->window) && !xuiWindowIsMaximized(v->window));
    /* Native pointer gestures must not resize/maximize; programmatic layout
       remains available for font/viewport adaptation below. */
    xui_rect_t fixed=xuiWidgetGetWorldRect(v->window);int right=fixed.fX+fixed.fW-1,bottom=fixed.fY+fixed.fH-1;
    CHECK(xuiInputPointerMove(a.context,right,bottom,0)==XUI_OK && xuiInputPointerDown(a.context,right,bottom,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerMove(a.context,right+80,bottom+60,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a.context,right+80,bottom+60,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&a));
    xui_rect_t afterDrag=xuiWidgetGetWorldRect(v->window);CHECK(afterDrag.fW==fixed.fW && afterDrag.fH==fixed.fH);
    for(int i=0;i<2;++i){int x=fixed.fX+80,y=fixed.fY+14;CHECK(xuiInputPointerDown(a.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a.context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(&a));}
    CHECK(!xuiWindowIsMaximized(v->window));
    /* Close discards all drafts, including skip; it does not even rewrite settings. */
    choose(v,15,1);CHECK(click(&a,v->close) && !xideAssociationViewIsOpen(&a) && !applies && !opens);
    char unchanged[sizeof(seed)+10]={0};f=fopen(settings,"rb");CHECK(f);size_t n=fread(unchanged,1,sizeof(unchanged)-1,f);fclose(f);CHECK(n==strlen(seed) && !strcmp(unchanged,seed));
    CHECK(xideAssociationViewOpen(&a) && complete(&a) && selectedMask(v)==7 && !xuiCheckBoxGetChecked(v->skip));
    choose(v,0,1);cancelled(v->window,&a);CHECK(!xideAssociationViewIsOpen(&a) && !applies && !opens);
    CHECK(xideAssociationViewOpen(&a) && complete(&a));
    choose(v,5,1);CHECK(click(&a,v->confirm) && xideAssociationViewBusy(&a));cancelled(v->window,&a);CHECK(xideAssociationViewIsOpen(&a));
    CHECK(complete(&a) && !xideAssociationViewIsOpen(&a) && registeredMask==5 && applies==1 && opens==1 && !wrongThread);
    CHECK(xideSettingsLoad(&reread,settings,&error) && xideSettingsAssociationMask(&reread)==5 && !xideSettingsAssociationPrompt(&reread));
    int64 future=0;CHECK(xrtValueGetInt(xrtValueObjectGet(xrtValueObjectGet(reread.source,(xstrview){"associations",12}),(xstrview){"future",6}),&future) && future==88);
    /* An empty selection removes every registration and does not open Default Apps. */
    CHECK(xideAssociationViewOpen(&a) && complete(&a));choose(v,0,0);CHECK(click(&a,v->confirm) && complete(&a) && !xideAssociationViewIsOpen(&a) && !registeredMask && applies==2 && opens==1);
    CHECK(xideSettingsAssociationMask(&a.preferences)==0 && xideSettingsAssociationPrompt(&a.preferences));
    CHECK(xideAssociationViewOpen(&a) && complete(&a));choose(v,8,0);CHECK(click(&a,v->confirm) && complete(&a) && registeredMask==8 && applies==3 && opens==2);
    /* Failure keeps the dialog/draft and the previously accepted preferences. */
    CHECK(xideAssociationViewOpen(&a) && complete(&a));choose(v,7,1);InterlockedExchange(&applyError,ERROR_ACCESS_DENIED);
    CHECK(click(&a,v->confirm) && complete(&a) && xideAssociationViewIsOpen(&a) && registeredMask==8 && xideSettingsAssociationMask(&a.preferences)==8 && xideSettingsAssociationPrompt(&a.preferences));
    CHECK(strstr(v->message,"5") && selectedMask(v)==7 && xuiWidgetGetEnabled(v->confirm));InterlockedExchange(&applyError,0);
    CHECK(click(&a,v->close) && xideAssociationViewOpen(&a) && complete(&a) && selectedMask(v)==8 && !xuiCheckBoxGetChecked(v->skip));
    const int fonts[]={16,24,40};
    for(int i=0;i<3;++i){
        CHECK(xideUiFontApply(&a,a.uiFontPath,fonts[i],&error));
        const int sizes[][2]={{420,300},{640,480},{1000,800}};
        for(int k=0;k<3;++k){xui_rect_t rect=xuiWidgetGetRect(v->window);rect.fW=sizes[k][0];rect.fH=sizes[k][1];CHECK(xuiWidgetSetRect(v->window,rect)==XUI_OK && settle(&a) && footerFits(&a));
            float maxX=0,maxY=0;CHECK(xuiScrollModelGetMaxOffset(xuiScrollViewGetModel(xideDialogGetScrollViewWidget(v->window)),&maxX,&maxY)==XUI_OK && maxX==0);xui_rect_t before=xuiWidgetGetWorldRect(v->skip);CHECK(xideDialogSetScroll(v->window,0,maxY)==XUI_OK && settle(&a));xui_rect_t after=xuiWidgetGetWorldRect(v->skip);CHECK(before.fY==after.fY && before.fH==after.fH);}
    }
    CHECK(xideUiFontApply(&a,a.uiFontPath,16,&error));CHECK(xuiWidgetSetRect(v->window,compact)==XUI_OK && xideDialogSetScroll(v->window,0,0)==XUI_OK && settle(&a) && footerFits(&a));
    /* Actual association state stays independent of the editable checkmarks. */
    CHECK(click(&a,v->close));registeredMask=15;defaultMask=5;CHECK(xideAssociationViewOpen(&a) && complete(&a));choose(v,15,1);CHECK(settle(&a));
    for(int theme=0;theme<IDE_THEME_COUNT;++theme){
        CHECK(xideWorkbenchApplyTheme(&a,theme)==XUI_OK && settle(&a));
        for(int i=0;i<4;++i){int linked=!!(5u&(1u<<i));CHECK(!strcmp(xuiLabelGetText(v->states[i]),linked?"当前已关联":"当前未关联"));CHECK(color(v->states[i],"text.color")==ideThemeColor(theme,linked?IDE_SUCCESS:IDE_TEXT));xui_rect_t type=xuiWidgetGetWorldRect(v->types[i]),status=xuiWidgetGetWorldRect(v->states[i]);CHECK(status.fX>=type.fX+type.fW && abs(status.fY-type.fY)<=1);}
        CHECK(!xuiWidgetGetVisible(v->status) && xuiUpdate(a.context,0)==XUI_OK);xui_rect_i_t clip={0,0,a.viewportWidth,a.viewportHeight};CHECK(a.proxy.surfaceClear(&a.proxy,a.target,0xffffffff)==XUI_OK && xuiRender(a.context,a.target,&clip,1)==XUI_OK && ticksVisible(&a));
        char screenshot[32768];snprintf(screenshot,sizeof(screenshot),"%s/file-associations-theme-%d.png",(const char*)raw,theme);CHECK(xideAppCapture(&a,screenshot)==XGE_OK);
    }
    printf("compact association dialog: %d x %d; inline actual state; visible checkmark pixels in all 4 themes\n",compact.fW,compact.fH);
cleanup:
    xideSettingsClear(&reread);if(began)xgeEnd();xideAppDestroy(&a);testFailed=failed;xgeQuit();
    if(!failed)printf("association_view_test passed: %d checks; Close/X discard, byte-identical settings, mixed/all-clear/C selection, OK persistence, failures, fixed footer at 3 fonts x 3 sizes, background order and worker-only OS calls\n",checks);
    return XGE_OK;
}
int main(int argc,char** argv)
{if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1200;d.iHeight=900;d.sTitle="IDE file associations";d.iFlags=XGE_INIT_WINDOW;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,argv[1]);xgeUnit();return result==XGE_OK && started && !testFailed ? 0:1;}
