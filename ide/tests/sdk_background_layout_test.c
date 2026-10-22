#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char* root;HDESK desktop;int started,failed; } fixture;
#define CHECK(v) do {if(!(v)){fprintf(stderr,"background_view:%d %s: %s\n",__LINE__,#v,error.message);f->failed=1;goto cleanup;}}while(0)
static int settle(xide_app* app)
{ if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;xideEditorPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && xuiUpdate(app->context,0)==XUI_OK && !app->frameError; }
static int paint(xide_app* app)
{ xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight};return app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK; }
static int run(void* user)
{
    fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char* path=NULL;xstrbuf body;xrtStrBufInit(&body);
    const char* saved="func main(...) as int\n return 0\nend\n";
    path=xidePathResolve(f->root,"scene.xl");CHECK(path && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)saved,strlen(saved)}));
    app.hidden=1;app.noSession=1;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideEditorOpen(&app,path,&error));xui_widget editor=app.activeDocument->editor;xui_code_document document=xuiCodeEditGetDocument(editor);

    for(int i=0;i<800;++i) CHECK(xrtStrBufAppend(&body,xrtStrView("中😀é ab\t")));
    CHECK(xrtStrBufAppend(&body,xrtStrView("TARGET\n")));
    int target=(int)body.Size-2;
    CHECK(xuiCodeEditSetText(editor,body.Data)==XUI_OK && xuiCodeEditSetWordWrap(editor,1)==XUI_OK && settle(&app));
    for(int mutation=0;mutation<8;++mutation) {
        if(mutation==1) CHECK(xuiCodeEditSetTabColumns(editor,8)==XUI_OK);
        if(mutation==2) CHECK(xuiCodeEditSetMinimap(editor,1,160)==XUI_OK);
        if(mutation==3) CHECK(xuiCodeEditSetMinimap(editor,0,0)==XUI_OK);
        if(mutation==4) CHECK(xuiCodeEditSetFont(editor,app.uiFont)==XUI_OK);
        if(mutation==5) CHECK(xuiCodeEditSetFont(editor,app.codeFont)==XUI_OK);
        if(mutation==6) { xui_rect_t r=xuiWidgetGetWorldRect(editor);app.viewportWidth-=180;CHECK(xuiInputViewport(app.context,app.viewportWidth,app.viewportHeight)==XUI_OK);xideWorkbenchLayout(&app);CHECK(xuiLayout(app.context)==XUI_OK && xuiWidgetGetWorldRect(editor).fW<r.fW); }
        if(mutation==7) CHECK(xuiCodeDocumentInsert(document,target,"x")==XUI_OK);
        CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(editor),document,target,0)==XUI_OK && xuiCodeEditEnsureCaretVisible(editor)==XUI_OK);
        float x=0,y=0;CHECK(xuiCodeEditGetScroll(editor,&x,&y)==XUI_OK);printf("mutation %d scroll %.3f\n",mutation,(double)y);CHECK(y>0);
        for(int repeat=0;repeat<8;++repeat) {
            xui_rect_t point={0},world=xuiWidgetGetWorldRect(editor);float nextX=0,nextY=0;
            CHECK(xuiWidgetInvalidate(editor,XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER)==XUI_OK && paint(&app) && xuiCodeEditGetTextOffsetRect(editor,target,&point)==XUI_OK && xuiCodeEditGetScroll(editor,&nextX,&nextY)==XUI_OK);
            xui_rect_t view=xuiScrollModelGetViewport(xuiCodeEditGetScrollModel(editor));
            CHECK(point.fY>=world.fY+view.fY && point.fY+point.fH<=world.fY+view.fY+view.fH && nextX==x && nextY==y);
        }
    }
    CHECK(xuiCodeEditSetText(editor,"short\n")==XUI_OK && settle(&app) && paint(&app));
    float sx=0,sy=0,mx=0,my=0;CHECK(xuiCodeEditGetScroll(editor,&sx,&sy)==XUI_OK && xuiScrollModelGetMaxOffset(xuiCodeEditGetScrollModel(editor),&mx,&my)==XUI_OK && sy==0 && my==0);
    puts("layout cache invalidations passed: tab/minimap/font/resize/edit/shrink and 64 forced redraws");

cleanup:
    if(began) xgeEnd();
    xideAppDestroy(&app);xrtStrBufFree(&body);free(path);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{fixture* f=data;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Background render verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;
    fixture f={.root=argv[1]};wchar_t name[100];swprintf(name,100,L"BackgroundRender-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread) {if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
