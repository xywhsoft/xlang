#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/file_sync.h"
#include "services/semantic_color_result.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned paintedLookups;
static int targetStart,targetEnd;static unsigned targetPainted;
static const xide_color_range* observeFind(const xide_color_result* r,int start,int end)
{++paintedLookups;const xide_color_range* row=xideColorResultFind(r,start,end);if(row && start==targetStart && end==targetEnd && row->role==XIDE_COLOR_FUNCTION)++targetPainted;return row;}
#define xideColorResultFind observeFind
#include "../src/ui/language_view.c"
#undef xideColorResultFind
typedef struct fixture {const char *root,*compiler;HDESK desktop;int started,failed,checks;} fixture;
#define CHECK(e) do{++f->checks;if(!(e)){struct xide_color_view* c=app.languageView?app.languageView->colors:NULL;fprintf(stderr,"semantic_color_view:%d %s; %s; color=%p supported=%d valid=%d request=%s decoding=%d receipt=%d sync=%d\n",__LINE__,#e,error.message,(void*)c,c?c->supported:0,c?c->valid:0,c?c->request:"",c?c->decoding:0,app.languageView?app.languageView->receiptReady:0,app.languageView?app.languageView->syncMode:0);f->failed=1;goto cleanup;}}while(0)
static int writeText(const char* p,const char* text)
{return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int settle(xide_app* app)
{if(xuiDispatchPendingEvents(app->context)!=XUI_OK)return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError;}
static int waitColors(xide_app* app,int empty)
{
 uint64_t until=GetTickCount64()+20000;
 do{if(!settle(app))return 0;struct xide_color_view* c=app->languageView->colors;
  if(c && c->valid && colorCurrent(app->languageView,c->applied) && !*c->request && !c->decoding && !c->hasReady && (empty?!xideColorResultCount(c->result):xideColorResultCount(c->result)>5))return 1;
  Sleep(5);
 }while(GetTickCount64()<until);return 0;
}
static int capture(xide_app* app,const char* root,const char* name)
{
 char* path=xidePathResolve(root,name);xui_rect_i_t rect={0,0,app->viewportWidth,app->viewportHeight};
 int okay=path && settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&rect,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK;free(path);return okay;
}
static int run(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *entry=NULL,*other=NULL,*dep=NULL,*canonical=NULL;xvalue* fake=NULL;
 const char* body="\xef\xbb\xbf" "import dep\r\nclass Box\r\n int value\r\n func read() as int\r\n return self.value\r\n end\r\nend\r\nfunc pick(int 输入) as int\r\n return 输入\r\nend\r\nfunc main(...) as int\r\n string 文本=\"😀\"; int 结果=pick(7)\r\n auto box=Box()\r\n return 结果+box.read()+dep.value()\r\nend\r\n";
 const char* depGood="export func value() as int\n return 7\nend\n";
 entry=xidePathResolve(f->root,"入口😀.xl");other=xidePathResolve(f->root,"other.xl");dep=xidePathResolve(f->root,"dep.xl");CHECK(entry && other && dep && writeText(entry,body) && writeText(other,"func other() as int\n return 1\nend\n") && writeText(dep,depGood));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error));xide_document* xl=app.activeDocument;xui_code_document doc=xuiCodeEditGetDocument(xl->editor);
 /* File loading tracks/removes its encoding BOM. Put the exact BOM-bearing
    text into the editor to exercise raw unsaved coordinates, as the decoder sees. */
 CHECK(xuiCodeDocumentReplace(doc,0,xuiCodeDocumentGetLength(doc),body)==XUI_OK);
 /* CodeEdit canonicalizes line endings. Semantic coordinates address this
    exact in-memory snapshot, independently of the original disk encoding. */
 canonical=copy(xuiCodeDocumentGetText(doc));CHECK(canonical);
 CHECK(app.languageView->colors && waitColors(&app,0));xide_language_view* v=app.languageView;struct xide_color_view* c=v->colors;
 CHECK(c->supported && c->serial>0 && xideColorResultCount(c->result)>20);
 semantic_editor_binding* binding=v->bindings;while(binding && binding->editor!=xl->editor)binding=binding->next;CHECK(binding && binding->colorBound);
 color_style_proc style=NULL;void* styleUser=NULL;CHECK(c->get(xl->editor,&style,&styleUser)==XUI_OK && style==colorStyle && styleUser==binding);
 const char* function=strstr(canonical,"pick(7)");CHECK(function);xui_code_token_t token={(int)(function-canonical),(int)(function-canonical)+4,XUI_CODE_STYLE_DEFAULT,XUI_CODE_TOKEN_IDENTIFIER,0};int id=-1;
 targetStart=token.iStartOffset;targetEnd=token.iEndOffset;
 int styled=style(doc,&token,&id,styleUser);
 if(styled!=XUI_OK || id!=XUI_CODE_STYLE_NUMBER){
  fprintf(stderr,"style diagnostic: returned=%d style=%d expected=%d token=%d..%d kind=%d compose=%d appliedVersion=%u actualVersion=%u textBytes=%d\n",styled,id,XUI_CODE_STYLE_NUMBER,token.iStartOffset,token.iEndOffset,token.iKind,binding->composing,c->applied.version,xuiCodeDocumentGetVersion(doc),xuiCodeDocumentGetLength(doc));
  const xide_color_range* rows=xideColorResultRanges(c->result);for(size_t i=0;i<xideColorResultCount(c->result);++i)if(rows[i].start>=token.iStartOffset-12 && rows[i].start<token.iStartOffset+12)fprintf(stderr,"near range: %d..%d role=%u modifiers=%u\n",rows[i].start,rows[i].end,rows[i].role,rows[i].modifiers);
 }
 CHECK(styled==XUI_OK && id==XUI_CODE_STYLE_NUMBER);
 ++c->epoch;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);--c->epoch;
 ++v->highlightSession;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);--v->highlightSession;
 ++c->applied.id;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);--c->applied.id;
 token.iKind=XUI_CODE_TOKEN_COMMENT;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);token.iKind=XUI_CODE_TOKEN_IDENTIFIER;
 ++token.iEndOffset;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);--token.iEndOffset;
 uint32_t version=xuiCodeDocumentGetVersion(doc);int undo=xuiCodeDocumentCanUndo(doc),redo=xuiCodeDocumentCanRedo(doc);
 for(int theme=0;theme<2;++theme){
  CHECK(xideWorkbenchApplyTheme(&app,theme)==XUI_OK);paintedLookups=targetPainted=0;
  CHECK(capture(&app,f->root,theme?"semantic-colors-dark.png":"semantic-colors-light.png") && paintedLookups>0 && targetPainted>0);
  xui_rect_t a={0},b={0};CHECK(xuiCodeEditGetTextOffsetRect(xl->editor,targetStart,&a)==XUI_OK && xuiCodeEditGetTextOffsetRect(xl->editor,targetEnd,&b)==XUI_OK && b.fX>a.fX && b.fY==a.fY);
  printf("{\"semanticFunction\":{\"theme\":%d,\"rect\":[%d,%d,%d,%d],\"matchedPaints\":%u}}\n",theme,(int)a.fX,(int)a.fY,(int)(b.fX-a.fX),(int)a.fH,targetPainted);
 }
 CHECK(xuiCodeDocumentGetVersion(doc)==version && xuiCodeDocumentCanUndo(doc)==undo && xuiCodeDocumentCanRedo(doc)==redo && !strcmp(xuiCodeDocumentGetText(doc),canonical));
 /* A real edit revokes colors synchronously, before owner polling/analysis. */
 int literal=(int)(strstr(canonical,"pick(7)")-canonical)+5;CHECK(xuiCodeDocumentReplace(doc,literal,literal+1,"8")==XUI_OK && !c->valid);
 id=-1;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);CHECK(waitColors(&app,0) && c->deltaRequested && c->applied.version!=version);
 CHECK(xuiCodeDocumentUndo(doc)==XUI_OK && !c->valid && waitColors(&app,0) && !strcmp(xuiCodeDocumentGetText(doc),canonical));
 /* Other open input changes must revoke the active document's whole proof. */
 CHECK(xideEditorOpen(&app,other,&error));xide_document* dependent=app.activeDocument;CHECK(xideEditorShow(&app,xl)==XUI_OK && waitColors(&app,0));
 xui_code_document second=xuiCodeEditGetDocument(dependent->editor);CHECK(xuiCodeDocumentReplace(second,0,xuiCodeDocumentGetLength(second),"func other() as int\n return 2\nend\n")==XUI_OK && !c->valid);
 CHECK(waitColors(&app,0));
 xui_ime_composition_t composition={.iSize=sizeof(composition),.sText="",.bActive=1};CHECK(xuiSetFocusWidget(app.context,xl->editor)==XUI_OK && xuiInputImeCompositionEx(app.context,&composition)==XUI_OK && settle(&app) && !c->valid);
 for(int i=0;i<20;++i){CHECK(settle(&app));Sleep(5);}CHECK(!c->valid && !*c->request);composition.bActive=0;CHECK(xuiInputImeCompositionEx(app.context,&composition)==XUI_OK && waitColors(&app,0));
 CHECK(xideDocumentSetLanguage(xl,"c",&error) && !c->valid);id=-1;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);
 CHECK(xideDocumentSetLanguage(xl,"xlang",&error) && waitColors(&app,0));
 /* A full request must never interpret edits against an unrelated old base. */
 document_binding* input=inputPath(v,xl->path);CHECK(input && !c->lease && !*c->request && !c->decoding);
 c->lease=xlsSyncRetainText(v->sync,input->uri,input->wireVersion);CHECK(c->lease);c->wanted=colorIdentity(v);c->token=++c->serial;c->deltaRequested=0;
 snprintf(c->request,sizeof(c->request),"xlang-ide:colors:%llu:%llu",(unsigned long long)v->highlightSession,(unsigned long long)c->token);
 fake=xrtValueObject();CHECK(fake && put(fake,"jsonrpc",xrtValueString(sv("2.0"))) && put(fake,"id",xrtValueString(sv(c->request))) && put(fake,"result",xrtJsonParse(sv("{\"edits\":[]}"))));CHECK(takeColors(v,fake));fake=NULL;
 uint64_t until=GetTickCount64()+5000;do{CHECK(settle(&app));if(colorIdentityEqual(c->blocked,c->wanted))break;Sleep(5);}while(GetTickCount64()<until);
 CHECK(!c->valid && colorIdentityEqual(c->blocked,c->wanted));colorsInvalidate(v);CHECK(waitColors(&app,0));
 CHECK(xuiCodeDocumentReplace(doc,0,xuiCodeDocumentGetLength(doc),"func main(...) as int\n return missingName\nend\n")==XUI_OK && !c->valid && waitColors(&app,1));
 CHECK(xuiCodeDocumentReplace(doc,0,xuiCodeDocumentGetLength(doc),body)==XUI_OK && waitColors(&app,0));
 CHECK(writeText(dep,"export func value() as int\n return \"bad\"\nend\n") && waitColors(&app,1));CHECK(writeText(dep,depGood) && waitColors(&app,0));
 CHECK(xideDocumentClose(xl,1,&error) && !c->valid);id=-1;CHECK(style(doc,&token,&id,styleUser)!=XUI_OK);
 xideLanguageViewStop(&app);CHECK(c->get(xl->editor,&style,&styleUser)==XUI_OK && style!=colorStyle);
 printf("semantic_color_view_test passed: %d native checks; actual standard full/delta to visible styles, both themes, synchronous edit/other-input/IME/language/disk/close invalidation, rejected full-delta and source recovery.\n",f->checks);
cleanup:
 xrtValueRelease(fake);if(began)xgeEnd();xideAppDestroy(&app);free(entry);free(other);free(dep);free(canonical);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
 fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Semantic color integration";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;
 int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;
}
int main(int argc,char** argv)
{
 if(argc!=3 || !xrtDirCreateAll(argv[1]))return 2;
 fixture f={.root=argv[1],.compiler=argv[2]};wchar_t name[96];swprintf(name,96,L"SemanticColors-%lu",GetCurrentProcessId());
 f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread){if(WaitForSingleObject(thread,180000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
