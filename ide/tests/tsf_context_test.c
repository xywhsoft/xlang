#define COBJMACROS
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <msctf.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "app/xide_app.h"
#include "ui/editor_view.h"

static int started,failed,assertions;
#define CHECK(e) do { ++assertions; if (!(e)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e); failed=1; goto cleanup; } } while(0)
typedef struct edit_session {
    ITfEditSession iface;
    LONG references;
    ITfContext* context;
    const WCHAR* expected;
    const WCHAR* replacement;
    LONG anchor,length;
    int called;
    HRESULT result;
} edit_session;
static HRESULT STDMETHODCALLTYPE query(ITfEditSession* self,REFIID iid,void** out)
{
    if (!out) return E_POINTER;
    *out=NULL;
    if (!IsEqualIID(iid,&IID_IUnknown) && !IsEqualIID(iid,&IID_ITfEditSession)) return E_NOINTERFACE;
    *out=self; ++((edit_session*)self)->references; return S_OK;
}
static ULONG STDMETHODCALLTYPE retain(ITfEditSession* self) { return ++((edit_session*)self)->references; }
static ULONG STDMETHODCALLTYPE release(ITfEditSession* self) { return --((edit_session*)self)->references; }
static HRESULT STDMETHODCALLTYPE edit(ITfEditSession* self,TfEditCookie cookie)
{
    edit_session* task=(edit_session*)self;
    ITfRange *range=NULL,*end=NULL; ITfRangeACP* acp=NULL;
    TF_SELECTION selection={0}; ULONG fetched=0,count=0;
    WCHAR actual[256]={0}; HRESULT hr;
    task->called++;
    hr=ITfContext_GetStart(task->context,cookie,&range);
    if (FAILED(hr)) goto done;
    hr=ITfContext_GetEnd(task->context,cookie,&end);
    if (FAILED(hr)) goto done;
    hr=ITfRange_ShiftEndToRange(range,cookie,end,TF_ANCHOR_END);
    if (FAILED(hr)) goto done;
    hr=ITfRange_GetText(range,cookie,0,actual,255,&count);
    if (FAILED(hr)) goto done;
    actual[count]=0;
    if (wcscmp(actual,task->expected)) { hr=E_UNEXPECTED; fprintf(stderr,"TSF full text mismatch, got %lu UTF16 units\n",(unsigned long)count); goto done; }
    hr=ITfContext_GetSelection(task->context,cookie,TF_DEFAULT_SELECTION,1,&selection,&fetched);
    if (FAILED(hr) || fetched!=1) { if(SUCCEEDED(hr)) hr=E_UNEXPECTED; goto done; }
    hr=ITfRange_QueryInterface(selection.range,&IID_ITfRangeACP,(void**)&acp);
    if (FAILED(hr)) goto done;
    LONG anchor=0,length=0;
    hr=ITfRangeACP_GetExtent(acp,&anchor,&length);
    if (FAILED(hr)) goto done;
    if (anchor!=task->anchor || length!=task->length) {
        fprintf(stderr,"TSF selection mismatch %ld,%ld expected %ld,%ld\n",anchor,length,task->anchor,task->length);
        hr=E_UNEXPECTED; goto done;
    }
    if(task->replacement) hr=ITfRange_SetText(selection.range,cookie,0,task->replacement,(LONG)wcslen(task->replacement));
done:
    if(acp) ITfRangeACP_Release(acp);
    if(selection.range) ITfRange_Release(selection.range);
    if(end) ITfRange_Release(end);
    if(range) ITfRange_Release(range);
    task->result=hr; return hr;
}
static ITfEditSessionVtbl session_vtable={query,retain,release,edit};
static int pump(xide_app* app)
{
    int result=xidePlatformHandleInput(app); xideEditorPoll(app); xideWorkbenchLayout(app);
    return result==XUI_OK && xuiLayout(app->context)==XUI_OK && !app->frameError;
}
static int verify(void* unused)
{
    (void)unused; started=1;
    xide_app app={0}; xide_model_error error={0}; int began=0,activated=0;
    ITfThreadMgr* manager=NULL; ITfDocumentMgr* document=NULL; ITfContext* context=NULL; TfClientId client=0;
    app.hidden=1; app.noSession=1; app.maxFrames=1;
    CHECK(xideAppCreate(&app)==XUI_OK);
    CHECK(xgeBegin()==XGE_OK); began=1;
    CHECK(pump(&app));
    CHECK(SUCCEEDED(CoCreateInstance(&CLSID_TF_ThreadMgr,NULL,CLSCTX_INPROC_SERVER,&IID_ITfThreadMgr,(void**)&manager)) && manager);
    CHECK(SUCCEEDED(ITfThreadMgr_Activate(manager,&client))); activated=1;
    for(int language=0;language<2;++language) {
        const char* name=language ? "xlang" : "c";
        const char* original=language ? "// A中文😀Z\nlet value = 1\n" : "// A中文😀Z\nint value = 1;\n";
        const char* replaced=language ? "// A中文文字Z\nlet value = 1\n" : "// A中文文字Z\nint value = 1;\n";
        WCHAR wide[256]={0}; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,original,-1,wide,256)>0);
        xide_document* doc=xideDocumentNew(&app.documents,name,&error); CHECK(doc);
        CHECK(xideEditorShow(&app,doc)==XUI_OK && xuiCodeEditSetText(doc->editor,original)==XUI_OK);
        xui_code_document core=xuiCodeEditGetDocument(doc->editor);
        int offset=(int)(strstr(original,"😀")-original);
        CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),core,offset,0)==XUI_OK);
        CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(doc->editor),core,offset+4,1)==XUI_OK);
        CHECK(pump(&app));
        HWND window=(HWND)xgePlatformNativeHandle(); CHECK(window && IsWindow(window));
        SendMessageW(window,WM_SETFOCUS,0,0);
        CHECK(SUCCEEDED(ITfThreadMgr_GetFocus(manager,&document)) && document);
        CHECK(SUCCEEDED(ITfDocumentMgr_GetTop(document,&context)) && context);
        uint32_t version=xuiCodeDocumentGetVersion(core);
        edit_session task={{&session_vtable},1,context,wide,L"文字",6,2,0,E_FAIL}; HRESULT session_result=E_FAIL;
        HRESULT request=ITfContext_RequestEditSession(context,client,&task.iface,TF_ES_SYNC|TF_ES_READWRITE,&session_result);
        printf("language=%s request=0x%08lx session=0x%08lx callback=%d callbackResult=0x%08lx\n",name,(unsigned long)request,(unsigned long)session_result,task.called,(unsigned long)task.result);
        CHECK(request==S_OK && session_result==S_OK && task.called==1 && task.result==S_OK && task.references==1);
        CHECK(pump(&app));
        CHECK(!strcmp(doc->language,name) && !strcmp(xuiCodeEditGetText(doc->editor),replaced));
        CHECK(doc->dirty && xuiCodeDocumentGetVersion(core)==version+1);
        xideDispatchAction(&app,XIDE_ACTION_EDITOR_UNDO); CHECK(pump(&app));
        CHECK(!strcmp(xuiCodeEditGetText(doc->editor),original));
        xideDispatchAction(&app,XIDE_ACTION_EDITOR_REDO); CHECK(pump(&app));
        CHECK(!strcmp(xuiCodeEditGetText(doc->editor),replaced));
        ITfContext_Release(context); context=NULL; ITfDocumentMgr_Release(document); document=NULL;
    }
    puts("Actual Windows TSF context read/UTF16 selection/Chinese replacement/one-step undo/redo verified for C and xlang. No synthetic input posting.");
cleanup:
    if(context) ITfContext_Release(context);
    if(document) ITfDocumentMgr_Release(document);
    if(activated) ITfThreadMgr_Deactivate(manager);
    if(manager) ITfThreadMgr_Release(manager);
    if(began) xgeEnd();
    xideAppDestroy(&app); xgeSetQuitRequestCallback(NULL,NULL);
    printf("assertions=%d result=%s\n",assertions,failed ? "failed" : "passed");
    xgeQuit(); return XGE_OK;
}
int main(void)
{
    xge_desc_t desc={0}; desc.iWidth=1280;desc.iHeight=800;desc.sTitle="xlang IDE actual TSF context test";
    desc.iFlags=XGE_INIT_WINDOW;desc.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,NULL); xgeUnit();
    return result==XGE_OK && started && !failed ? 0 : 1;
}
