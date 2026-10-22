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
static int capture(xide_app* app,const char* root,int scene,int theme,const char* state)
{
    char name[100];snprintf(name,sizeof(name),"scene%d-theme%d-%s.png",scene,theme,state);char* path=xidePathResolve(root,name);
    int okay=path && xuiWidgetInvalidate(app->activeDocument->editor,XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER)==XUI_OK && settle(app) && paint(app) && xideAppCapture(app,path)==XGE_OK;free(path);return okay;
}
static int clearOwned(xui_code_annotation_store store)
{ int code=xuiCodeAnnotationClearIndicators(store,1005);return code==XUI_OK || code==XUI_ERROR_UNSUPPORTED; }
static int run(void* user)
{
    fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char* path=NULL;xstrbuf body;xrtStrBufInit(&body);
    const char* saved="func main(...) as int\n return 0\nend\n";
    path=xidePathResolve(f->root,"scene.xl");CHECK(path && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)saved,strlen(saved)}));
    app.hidden=1;app.noSession=1;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    CHECK(xideEditorOpen(&app,path,&error));xui_widget editor=app.activeDocument->editor;xui_code_document document=xuiCodeEditGetDocument(editor);xui_code_annotation_store store=xuiCodeEditGetAnnotations(editor);
    for(int theme=0;theme<2;++theme) for(int scene=0;scene<4;++scene) {
        CHECK(clearOwned(store) && xuiCodeEditClearFind(editor)==XUI_OK && xuiCodeAnnotationClearDiagnostics(store)==XUI_OK);
        int ignored=xuiCodeAnnotationClearIndicators(store,1008);CHECK(ignored==XUI_OK || ignored==XUI_ERROR_UNSUPPORTED);
        xrtStrBufFree(&body);xrtStrBufInit(&body);int starts[3]={0},ends[3]={0},visible=0,total=0;
        if(scene==0) {
            CHECK(xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf" "func main(...) as int\n\tstring 标记 = \"😀é\"; int 结果 = 7\n\treturn 结果\nend\n")));
            const char* first=strstr(body.Data,"结果");const char* second=first ? strstr(first+1,"结果") : NULL;CHECK(first && second);
            starts[0]=(int)(first-body.Data);ends[0]=starts[0]+6;starts[1]=(int)(second-body.Data);ends[1]=starts[1]+6;visible=total=2;
        } else if(scene==1) {
            for(int i=0;i<120;++i) CHECK(xrtStrBufAppendFormat(&body,"line%d: abc item%d\n",i,i));
            const char *shown=strstr(body.Data,"item110"),*hidden=strstr(body.Data,"item50");CHECK(shown && hidden);
            starts[0]=(int)(shown-body.Data);ends[0]=starts[0]+7;starts[1]=(int)(hidden-body.Data);ends[1]=starts[1]+6;visible=1;total=2;
        } else {
            CHECK(xrtStrBufAppend(&body,xrtStrView("// ")));
            for(int i=0;i<800;++i) CHECK(xrtStrBufAppend(&body,xrtStrView("ab\t中😀é ")));
            starts[0]=(int)body.Size;CHECK(xrtStrBufAppend(&body,xrtStrView("TARGET")));ends[0]=(int)body.Size;visible=total=1;
            for(int i=0;i<40;++i) CHECK(xrtStrBufAppend(&body,xrtStrView(" suffix")));
            CHECK(xrtStrBufAppend(&body,xrtStrView("\n")));
        }
        CHECK(xuiCodeEditSetText(editor,body.Data)==XUI_OK && xuiCodeEditSetWordWrap(editor,scene==3)==XUI_OK && xuiCodeEditSetScroll(editor,0,0)==XUI_OK && xideWorkbenchApplyTheme(&app,theme)==XUI_OK && settle(&app) && paint(&app));
        if(scene==1) {
            xui_code_fold_range_t fold={.iSize=sizeof(fold),.iStartLine=2,.iEndLine=100,.iFlags=XUI_CODE_FOLD_COLLAPSED|XUI_CODE_FOLD_HEADER|XUI_CODE_FOLD_CUSTOM};int shown=1;
            CHECK(xuiCodeFoldStateSetRanges(xuiCodeEditGetFoldState(editor),&fold,1)==XUI_OK && xuiWidgetInvalidate(editor,XUI_WIDGET_DIRTY_LAYOUT|XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER)==XUI_OK && settle(&app));
            CHECK(xuiCodeFoldStateIsLineVisible(xuiCodeEditGetFoldState(editor),50,&shown)==XUI_OK && !shown);
        }
        CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(editor),document,ends[0],0)==XUI_OK && xuiCodeEditEnsureCaretVisible(editor)==XUI_OK && xuiSetFocusWidget(app.context,NULL)==XUI_OK && settle(&app));
        int onScreen=0;
        for(int attempt=0;attempt<3;++attempt) {
            xui_rect_t point={0},world=xuiWidgetGetWorldRect(editor);
            float beforeX=0,beforeY=0,paintX=0,paintY=0,pointX=0,pointY=0;
            CHECK(xuiCodeEditGetScroll(editor,&beforeX,&beforeY)==XUI_OK && paint(&app) && xuiCodeEditGetScroll(editor,&paintX,&paintY)==XUI_OK && xuiCodeEditGetTextOffsetRect(editor,ends[0],&point)==XUI_OK && xuiCodeEditGetScroll(editor,&pointX,&pointY)==XUI_OK);
            xui_rect_t viewport=xuiScrollModelGetViewport(xuiCodeEditGetScrollModel(editor));
            if(point.fY>=world.fY+viewport.fY && point.fY+point.fH<=world.fY+viewport.fY+viewport.fH) { onScreen=1;break; }
            CHECK(xuiCodeEditEnsureCaretVisible(editor)==XUI_OK && settle(&app));
        }
        CHECK(onScreen);
        float scrollX=0,scrollY=0;CHECK(xuiCodeEditGetScroll(editor,&scrollX,&scrollY)==XUI_OK);
        if(scene==2) CHECK(scrollX>0);
        if(scene==3) CHECK(scrollY>0);
        int findCount=0;
        if(scene==0) {xui_find_options_t options={.iSize=sizeof(options),.sPattern="标记",.iFlags=XUI_CODE_SEARCH_CASE_SENSITIVE};CHECK(xuiCodeEditFindAll(editor,&options,&findCount)==XUI_OK && findCount==1);}
        xui_code_diagnostic_t diagnostic={.iSize=sizeof(diagnostic),.tRange={scene==0 ? 3 : 0,scene==0 ? 4 : 1},.iSeverity=2,.sMessage="Retained diagnostic"};
        CHECK(xuiCodeAnnotationSetDiagnostics(store,&diagnostic,1)==XUI_OK && xuiCodeAnnotationSetIndicator(store,1008,XUI_CODE_INDICATOR_UNDERLINE,starts[0],ends[0],0,0)==XUI_OK);
        CHECK(settle(&app) && paint(&app) && capture(&app,f->root,scene,theme,"base"));
        uint32_t version=xuiCodeDocumentGetVersion(document);int undo=xuiCodeDocumentCanUndo(document),redo=xuiCodeDocumentCanRedo(document);xui_code_selection_t selection={0},after={0};CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&selection)==XUI_OK);
        for(int i=0;i<total;++i) CHECK(xuiCodeAnnotationSetIndicator(store,1005,XUI_CODE_INDICATOR_BACKGROUND,starts[i],ends[i],0,0)==XUI_OK);
        CHECK(capture(&app,f->root,scene,theme,"marked"));
        /* Duplicate/adjacent backgrounds must have the same coverage and
         * opacity. Hidden folded tokens remain absent from visible pixels. */
        for(int i=0;i<total;++i) CHECK(xuiCodeAnnotationSetIndicator(store,1005,XUI_CODE_INDICATOR_BACKGROUND,starts[i],ends[i],0,0)==XUI_OK);
        CHECK(capture(&app,f->root,scene,theme,"duplicate"));
        printf("{\"scene\":%d,\"theme\":%d,\"scroll\":[%.3f,%.3f],\"find\":%d,\"rects\":[",scene,theme,(double)scrollX,(double)scrollY,findCount);
        int comma=0;
        for(int i=0;i<visible;++i) for(int at=starts[i];at<ends[i];) {
            uint32 scalar=0;size_t read=0;xui_rect_t rect={0},next={0};
            CHECK(xrtUtf8Decode((xstrview){body.Data+at,body.Size-(size_t)at},&scalar,&read)==XUTF_OK && read>0 && xuiCodeEditGetTextOffsetRect(editor,at,&rect)==XUI_OK);
            CHECK(xuiCodeEditGetTextOffsetRect(editor,at+(int)read,&next)==XUI_OK);
            if(next.fY==rect.fY && next.fX>rect.fX) rect.fW=next.fX-rect.fX;
            else {
                xui_code_edit_text_hit_t hit={.iSize=sizeof(hit)};
                CHECK(xuiCodeEditHitTestText(editor,(float)rect.fX+1,(float)rect.fY+(float)rect.fH/2,&hit)==XUI_OK && hit.iClusterStart<=at && at<hit.iClusterEnd);
                rect=hit.tCharacterRect;
            }
            if(comma++) putchar(',');
            printf("[%.3f,%.3f,%.3f,%.3f]",(double)rect.fX,(double)rect.fY,(double)rect.fW,(double)rect.fH);at+=(int)read;
        }
        puts("]}");
        CHECK(clearOwned(store) && capture(&app,f->root,scene,theme,"cleared"));
        CHECK(version==xuiCodeDocumentGetVersion(document) && undo==xuiCodeDocumentCanUndo(document) && redo==xuiCodeDocumentCanRedo(document) && !strcmp(xuiCodeEditGetText(editor),body.Data) && xuiCodeAnnotationGetDiagnosticCount(store)==1 && xuiCodeEditGetFindResultCount(editor)==findCount);
        CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&after)==XUI_OK && selection.iCaretOffset==after.iCaretOffset && selection.iAnchorOffset==after.iAnchorOffset);
        xui_code_indicator_t other[4];int otherCount=0;CHECK(xuiCodeAnnotationGetIndicatorsAt(store,starts[0],other,4,&otherCount)==XUI_OK && otherCount>=1);
    }
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
