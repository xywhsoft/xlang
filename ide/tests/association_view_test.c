/* UI orchestration uses a deterministic backend: no real associations change. */
#include "platform/file_association.h"
#include "ui/association_view.h"
#include "ui/catalog_view.h"
#include "ui/dialog.h"
#include "ui/settings_view.h"
#include "theme/theme.h"
#include "catalog_ready.h"
#include <stdio.h>
#include <string.h>
static volatile LONG probes,registeredMask,opens,wrongThread;
static DWORD uiThread;
static DWORD fakeProbe(const wchar_t* prefix,const wchar_t* exe,xide_association_result* out)
{
    (void)prefix;(void)exe;if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&wrongThread);
    Sleep(25);memset(out,0,sizeof(*out));LONG mask=InterlockedCompareExchange(&registeredMask,0,0);
    for(int i=0;i<4;++i){out->registered[i]=!!(mask&(1<<i));out->defaultKnown[i]=1;}
    InterlockedIncrement(&probes);return 0;
}
static DWORD fakeRegister(const wchar_t* p,const wchar_t* exe,unsigned mask)
{(void)p;(void)exe;if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&wrongThread);Sleep(35);InterlockedExchange(&registeredMask,(LONG)mask);return 0;}
static DWORD fakeUnregister(const wchar_t* p,const wchar_t* exe,unsigned mask)
{(void)p;(void)exe;InterlockedAnd(&registeredMask,~(LONG)mask);return 0;}
static void fakeDefaults(void){if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&wrongThread);InterlockedIncrement(&opens);}
#define xideAssociationProbe fakeProbe
#define xideAssociationRegister fakeRegister
#define xideAssociationUnregister fakeUnregister
#define xideAssociationOpenDefaults fakeDefaults
#include "../src/ui/association_view.c"
#undef xideAssociationProbe
#undef xideAssociationRegister
#undef xideAssociationUnregister
#undef xideAssociationOpenDefaults
static int checks,testFailed,started;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"association_view_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
static int complete(xide_app* a)
{ULONGLONG until=GetTickCount64()+5000;while(a->associationView->job && GetTickCount64()<until){xideAssociationViewPoll(a);Sleep(1);}return !a->associationView->job;}
static int run(void* raw)
{
    xide_app a={0};int failed=0,began=0;char settings[32768];xide_settings reread={0};xide_model_error error={0};uiThread=GetCurrentThreadId();started=1;
    CHECK(raw);snprintf(settings,sizeof(settings),"%s/association-settings.json",(const char*)raw);DeleteFileA(settings);
    FILE* seed=fopen(settings,"wb");CHECK(seed);fputs("{\"format\":\"xlang-ide-settings-1\",\"associations\":{\"future\":88}}",seed);fclose(seed);
    a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideSettingsAssociationMask(&a.preferences)==7 && xideSettingsAssociationPrompt(&a.preferences));
    a.hidden=0;a.frames=1;xideAssociationViewPoll(&a);CHECK(!probes && !a.associationView); /* catalog must go first */
    CHECK(xideTestCatalogReady(&a));
    /* Keep the automatic prompt behind an existing modal; never steal its focus. */
    CHECK(xideSettingsViewOpen(&a,XIDE_SETTINGS_EDITOR));
    xideAssociationViewPoll(&a);CHECK(a.associationView && a.associationView->job && !xideAssociationStartupSettled(&a));
    CHECK(complete(&a) && xideAssociationStartupSettled(&a) && !xideAssociationViewIsOpen(&a));
    CHECK(a.associationView->promptPending && probes==1);
    xui_widget settingsWindow=xideSettingsViewWindow(&a);CHECK(settingsWindow && xideDialogSetOpen(settingsWindow,0)==XUI_OK);
    xideAssociationViewPoll(&a);CHECK(xideAssociationViewIsOpen(&a) && probes==1);
    struct xide_association_view* v=a.associationView;CHECK(selectedMask(v)==7 && !xuiCheckBoxGetChecked(v->types[3]));
    CHECK(xideDialogGetFooterWidget(v->window) && xideDialogGetDividerWidget(v->window));
    for(int i=0;i<3;++i){float w=i==0?480:i==1?680:1000,h=i==0?340:i==1?540:800;xui_rect_t rect=xuiWidgetGetRect(v->window);rect.fW=w;rect.fH=h;CHECK(xuiWidgetSetRect(v->window,rect)==XUI_OK);for(int n=0;n<3;++n){xideWorkbenchLayout(&a);CHECK(xuiLayout(a.context)==XUI_OK);xideDialogLayoutAll(&a);CHECK(xuiLayout(a.context)==XUI_OK);}xui_rect_t footer=xuiWidgetGetWorldRect(xideDialogGetFooterWidget(v->window)),body=xuiWidgetGetWorldRect(xideDialogGetScrollViewWidget(v->window));printf("association size %.0f,%.0f: body %d,%d footer %d,%d\n",w,h,body.fY,body.fH,footer.fY,footer.fH);CHECK(body.fY+body.fH<=footer.fY+1 && footer.fH>0);}
    action(v->install,&a);CHECK(xideAssociationViewBusy(&a));action(v->close,&a);CHECK(xideAssociationViewIsOpen(&a));CHECK(complete(&a) && registeredMask==7 && opens==1 && !wrongThread);
    CHECK(xuiCheckBoxSetChecked(v->types[3],1)==XUI_OK);CHECK(xuiCheckBoxSetChecked(v->skip,1)==XUI_OK);action(v->close,&a);CHECK(!xideAssociationViewIsOpen(&a));
    CHECK(xideSettingsLoad(&reread,settings,&error) && xideSettingsAssociationMask(&reread)==15 && !xideSettingsAssociationPrompt(&reread));
    int64 future=0;CHECK(xrtValueGetInt(xrtValueObjectGet(xrtValueObjectGet(reread.source,(xstrview){"associations",12}),(xstrview){"future",6}),&future) && future==88);
    CHECK(xideAssociationViewOpen(&a) && complete(&a));action(v->defaults,&a);CHECK(complete(&a) && opens==2 && !wrongThread);
    xideWorkbenchLayout(&a);CHECK(xuiLayout(a.context)==XUI_OK && xuiUpdate(a.context,0)==XUI_OK);xui_rect_i_t clip={0,0,a.viewportWidth,a.viewportHeight};CHECK(a.proxy.surfaceClear(&a.proxy,a.target,0xffffffff)==XUI_OK && xuiRender(a.context,a.target,&clip,1)==XUI_OK);
    char screenshot[32768];snprintf(screenshot,sizeof(screenshot),"%s/file-associations.png",(const char*)raw);CHECK(xideAppCapture(&a,screenshot)==XGE_OK);
cleanup:
    xideSettingsClear(&reread);if(began)xgeEnd();xideAppDestroy(&a);testFailed=failed;xgeQuit();
    if(!failed)printf("association_view_test passed: %d checks, background order, queued prompt, C opt-in, fixed footer, resize, close guard, preferences and no UI-thread OS calls\n",checks);
    return XGE_OK;
}
int main(int argc,char** argv)
{if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1200;d.iHeight=900;d.sTitle="IDE file associations";d.iFlags=XGE_INIT_WINDOW;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,argv[1]);xgeUnit();return result==XGE_OK && started && !testFailed ? 0:1;}
