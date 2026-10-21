#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "ui/editor_view.h"
#include "services/catalog_sources.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include "platform/file_io.h"
#include "theme/theme.h"
#include "lang/languages.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { char* root; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"catalog_source_view_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
static int settle(xide_app* a)
{ if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int key(xide_app* a,int k,uint32_t mods)
{ return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int pointer(xide_app* a,xui_widget widget)
{ xui_rect_t r=xuiWidgetGetWorldRect(widget); int x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int sourceClick(xide_app* a,int document)
{ xui_widget w=xideCatalogSourceButton(a,document); return xuiScrollViewEnsureChildVisible(a->help,w)==XUI_OK && settle(a) && pointer(a,w); }
static int capture(xide_app* a,const char* p)
{ xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static size_t named(xide_app* a,const char* name)
{ for(size_t i=0;i<a->catalog.count;++i) if(!strcmp(a->catalog.entries[i].name,name)) return i; return a->catalog.count; }
static int hasKeyword(xui_widget editor)
{
    xui_code_token_t tokens[256]; int count=0;
    if(xideRefreshEditorSyntax(editor)!=XUI_OK || xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),0,1024,tokens,256,&count)!=XUI_OK) return 0;
    for(int i=0;i<count && i<256;++i) if(tokens[i].iKind==XUI_CODE_TOKEN_KEYWORD) return 1;
    return 0;
}
static const char* text(xvalue* row,const char* key)
{ xstrview s={0}; xvalue* v=xrtValueObjectGet(row,(xstrview){key,strlen(key)}); return xrtValueGetString(v,&s) ? s.Data : ""; }
static int allSources(xide_app* app,const char* folder,xide_model_error* error)
{
    char* index=xidePathResolve(folder,"index.json"); size_t size=0;
    unsigned char* bytes=index ? xideIoReadAllLimit(index,1024*1024,&size) : NULL; free(index);
    xvalue* value=bytes ? xrtJsonParse((xstrview){(char*)bytes,size}) : NULL; xrtFree(bytes);
    xvalue* rows=value ? xrtValueObjectGet(value,(xstrview){"entries",7}) : NULL; int count=0;
    xide_text_file file={0};
    for(size_t i=0;rows && i<xrtValueCount(rows);++i) {
        xvalue* row=xrtValueArrayGet(rows,i);
        if(strcmp(text(row,"state"),"available") || !xideCatalogSourceRead(&app->catalog,text(row,"path"),&file,error)) { count=0; break; }
        ++count; xideTextFileClear(&file);
    }
    xideTextFileClear(&file); xrtValueRelease(value); return count;
}
static int markUnavailable(const char* index,const unsigned char* bytes,size_t size,const char* source)
{
    xvalue* value=xrtJsonParse((xstrview){(char*)bytes,size});
    xvalue* rows=value ? xrtValueObjectGet(value,(xstrview){"entries",7}) : NULL; int found=0;
    for(size_t i=0;rows && i<xrtValueCount(rows);++i) {
        xvalue* row=xrtValueArrayGet(rows,i);
        if(!strcmp(text(row,"path"),source)) {
            found=xrtValueObjectSetNew(row,(xstrview){"state",5},xrtValueString((xstrview){"unavailable",11})) && xrtValueObjectSetNew(row,(xstrview){"reason",6},xrtValueString((xstrview){"owned negative fixture",22})); break;
        }
    }
    size_t length=0; char* json=found ? xrtJsonStringify(value,true,&length) : NULL;
    int okay=json && xideIoWriteAtomic(index,(xbytesview){(unsigned char*)json,length});
    xrtFree(json); xrtValueRelease(value); return okay;
}
static int copyResources(const char* root)
{
    wchar_t module[32768]; if(!GetModuleFileNameW(NULL,module,32768)) return 0;
    char path[131072]; if(!WideCharToMultiByte(CP_UTF8,0,module,-1,path,sizeof(path),NULL,NULL)) return 0;
    char* base=xidePathDirectory(path); char* data=base ? xidePathResolve(base,"data") : NULL; free(base);
    char* input=data ? xidePathResolve(data,"catalog-sources") : NULL; char* output=xidePathResolve(root,"catalog-sources");
    char* catalog=data ? xidePathResolve(data,"commands.json") : NULL; char* target=xidePathResolve(root,"commands.json");
    char* index=input ? xidePathResolve(input,"index.json") : NULL; char* outIndex=output ? xidePathResolve(output,"index.json") : NULL;
    size_t size=0; unsigned char* bytes=index ? xideIoReadAllLimit(index,1024*1024,&size) : NULL;
    xvalue* value=bytes ? xrtJsonParse((xstrview){(char*)bytes,size}) : NULL; xrtFree(bytes);
    xvalue* entries=value ? xrtValueObjectGet(value,(xstrview){"entries",7}) : NULL;
    int okay=entries && xideIoCreateDirectories(output) && xideIoCopy(catalog,target,0) && xideIoCopy(index,outIndex,0);
    for(size_t i=0;okay && i<xrtValueCount(entries);++i) {
        xvalue* row=xrtValueArrayGet(entries,i); if(strcmp(text(row,"state"),"available")) continue;
        char name[80]; snprintf(name,sizeof(name),"%s.source",text(row,"sha256"));
        char* from=xidePathResolve(input,name); char* to=xidePathResolve(output,name);
        okay=from && to && xideIoCopy(from,to,0); free(from); free(to);
    }
    xrtValueRelease(value); free(data); free(input); free(output); free(catalog); free(target); free(index); free(outIndex); return okay;
}
static int verify(void* user)
{
    fixture* f=user; if(f->started) return XGE_OK; f->started=1;
    xide_app app={0}; xide_model_error error={0}; xide_text_file file={0}; int began=0;
    char *settings=xidePathResolve(f->root,"settings.json"),*catalog=xidePathResolve(f->root,"commands.json"),*folder=xidePathResolve(f->root,"catalog-sources"),*index=NULL,*payload=NULL;
    char* images[4]={0}; unsigned char *original=NULL,*indexBytes=NULL; size_t originalSize=0,indexSize=0;
    const char* imageNames[]={"native-source-c.png","native-source-xlang.png","native-source-document.png","native-source-unavailable.png"};
    for(int i=0;i<4;++i) { images[i]=xidePathResolve(f->root,imageNames[i]); CHECK(images[i]); }
    app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.catalogPath=catalog;
    CHECK(settings && catalog && folder && xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1; CHECK(settle(&app));
    CHECK(allSources(&app,folder,&error)>=335);
    CHECK(xideActionCount()==73 && !xideCatalogSourceIsOpen(&app) && !xideCatalogSourceEditor(&app));
    CHECK(xideActionNamed("help.sourceDeclaration")->id==90 && xideActionNamed("help.sourceDocument")->id==91);
    xide_document* draft=xideDocumentNew(&app.documents,"xlang",&error); CHECK(draft && xideEditorShow(&app,draft)==XUI_OK);
    CHECK(xuiCodeEditingInsertText(xuiCodeEditGetDocument(draft->editor),xuiCodeEditGetSelection(draft->editor),"int 草稿 = 42; // 保留\n",0)==XUI_OK && settle(&app) && draft->dirty);
    uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(draft->editor)); size_t documentCount=app.documents.count;
    xui_code_selection_t saved={0}; CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(draft->editor),&saved)==XUI_OK);
    size_t c=named(&app,"bool"),xl=named(&app,"aho_corasick.Match"); CHECK(c<app.catalog.count && xl<app.catalog.count);
    xideWorkbenchShowHelp(&app,(int)c); CHECK(settle(&app) && xideActionQuery(&app,XIDE_ACTION_SOURCE_DOCUMENT).enabled==(app.catalog.entries[c].documentCount>0));
    CHECK(sourceClick(&app,0) && xideCatalogSourceIsOpen(&app)); xui_widget editor=xideCatalogSourceEditor(&app);
    CHECK(editor && xuiCodeEditIsReadonly(editor) && !strcmp(xuiCodeEditGetLanguage(editor),"c") && strstr(xuiCodeEditGetText(editor),"bool"));
    CHECK(hasKeyword(editor));
    xui_code_selection_t selected={0}; int line=0; CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&selected)==XUI_OK && xuiCodeDocumentOffsetToLineColumn(xuiCodeEditGetDocument(editor),selected.iCaretOffset,&line,NULL)==XUI_OK && line+1==app.catalog.entries[c].sourceLine);
    CHECK(!xideActionQuery(&app,XIDE_ACTION_SAVE_FILE).enabled && xideActionExecute(&app,XIDE_ACTION_SAVE_FILE)==XIDE_ACTION_DISABLED);
    uint32_t readonlyVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor));
    CHECK(xuiInputTextEx(app.context,'Q',NULL)==XUI_OK && key(&app,XUI_KEY_BACKSPACE,0) && key(&app,'V',XUI_MOD_CTRL) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor))==readonlyVersion);
    CHECK(capture(&app,images[0]) && pointer(&app,xideCatalogSourceCloseWidget(&app)) && !xideCatalogSourceIsOpen(&app));
    xideWorkbenchShowHelp(&app,(int)xl); CHECK(settle(&app) && xuiSetFocusWidget(app.context,draft->editor)==XUI_OK);
    CHECK(xideActionExecute(&app,XIDE_ACTION_SOURCE_DECLARATION)==XIDE_ACTION_DISPATCHED && settle(&app));
    CHECK(!strcmp(xuiCodeEditGetLanguage(editor),"xlang") && strstr(xuiCodeEditGetText(editor),"class Match") && capture(&app,images[1]));
    CHECK(hasKeyword(editor));
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && !xideCatalogSourceIsOpen(&app) && xuiGetFocusWidget(app.context)==draft->editor);
    size_t help=app.catalog.count;
    for(size_t i=0;i<app.catalog.count;++i) if(app.catalog.entries[i].documentCount) {
        const xide_help_document* d=app.catalog.entries[i].documents[0];
        if(xideCatalogSourceRead(&app.catalog,d->source,&file,&error)) { help=i; break; }
    }
    CHECK(help<app.catalog.count); xideTextFileClear(&file); xideWorkbenchShowHelp(&app,(int)help); CHECK(settle(&app));
    CHECK(sourceClick(&app,1) && xideCatalogSourceIsOpen(&app) && !strcmp(xuiCodeEditGetLanguage(editor),"text") && xuiCodeDocumentGetLength(xuiCodeEditGetDocument(editor))>1000 && capture(&app,images[2]));
    CHECK(key(&app,XUI_KEY_ESCAPE,0));
    /* Chapter groups are navigation pages, not indices into the document list. */
    xide_help_view* helpView=xideCatalogHelpDocument(&app); const xide_help_document* first=app.catalog.entries[help].documents[0]; const xide_help_document* second=NULL;
    for(size_t i=0;i<app.catalog.documentCount;++i) if(strcmp(app.catalog.documents[i].source,first->source) && xideCatalogSourceRead(&app.catalog,app.catalog.documents[i].source,&file,&error)) { second=&app.catalog.documents[i]; break; }
    CHECK(second); xideTextFileClear(&file); char chapters[4096]={0};
    for(int i=0;i<40;++i) { char heading[80]; snprintf(heading,sizeof(heading),"# Chapter %d\ntext\n\n",i); strcat(chapters,heading); }
    xide_help_document firstCopy=*first,secondCopy=*second; firstCopy.markdown=chapters; secondCopy.markdown=chapters;
    const xide_help_document* grouped[]={&firstCopy,&secondCopy};
    CHECK(xideHelpSet(helpView,grouped,2)==XUI_OK && settle(&app) && xideHelpChapterCount(helpView)>64);
    CHECK(xideHelpJump(helpView,xideHelpChapterCount(helpView)-1) && settle(&app) && xideHelpSourceDocument(helpView)==&secondCopy && xideActionQuery(&app,XIDE_ACTION_SOURCE_DOCUMENT).enabled);
    CHECK(xideActionExecute(&app,XIDE_ACTION_SOURCE_DOCUMENT)==XIDE_ACTION_DISPATCHED && settle(&app) && strstr(xideCatalogSourceMessage(&app),"已校验") && !strcmp(xuiCodeEditGetLanguage(editor),"text"));
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && xideHelpSet(helpView,app.catalog.entries[help].documents,app.catalog.entries[help].documentCount)==XUI_OK && settle(&app));
    /* Negative cases alter only the copied fixture resources, never release data. */
    xvalue* sources=xrtValueObjectGet(app.catalog.source,(xstrview){"sources",7}); const char* digest=NULL;
    for(size_t i=0;i<xrtValueCount(sources);++i) { xvalue* row=xrtValueArrayGet(sources,i); if(!strcmp(text(row,"path"),app.catalog.entries[c].source)) digest=text(row,"sha256"); }
    CHECK(digest); char leaf[80]; snprintf(leaf,sizeof(leaf),"%s.source",digest); payload=xidePathResolve(folder,leaf);
    original=xideIoReadAllLimit(payload,4*1024*1024,&originalSize); CHECK(original && originalSize);
    CHECK(xideIoWriteAtomic(payload,(xbytesview){(unsigned char*)"changed",7}));
    xideWorkbenchShowHelp(&app,(int)c); CHECK(settle(&app) && sourceClick(&app,0) && xideCatalogSourceIsOpen(&app) && !*xuiCodeEditGetText(editor) && strstr(xideCatalogSourceMessage(&app),"内容已改变"));
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && xideIoWriteAtomic(payload,(xbytesview){original,originalSize}));
    index=xidePathResolve(folder,"index.json"); indexBytes=xideIoReadAllLimit(index,1024*1024,&indexSize); CHECK(indexBytes);
    CHECK(xideIoWriteAtomic(index,(xbytesview){(unsigned char*)"{}",2}) && sourceClick(&app,0) && !*xuiCodeEditGetText(editor) && strstr(xideCatalogSourceMessage(&app),"不匹配"));
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && xideIoWriteAtomic(index,(xbytesview){indexBytes,indexSize}));
    size_t missing=app.catalog.count;
    for(size_t i=0;i<app.catalog.count;++i) if(!strcmp(app.catalog.entries[i].source,"demo6/src/stdlib/xl6_stdlib_data.c")) { missing=i; break; }
    CHECK(missing<app.catalog.count && xideCatalogSourceRead(&app.catalog,app.catalog.entries[missing].source,&file,&error)); xideTextFileClear(&file);
    CHECK(markUnavailable(index,indexBytes,indexSize,app.catalog.entries[missing].source));
    xideWorkbenchShowHelp(&app,(int)missing); CHECK(settle(&app) && sourceClick(&app,0) && !*xuiCodeEditGetText(editor) && strstr(xideCatalogSourceMessage(&app),"未找到") && capture(&app,images[3]));
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && xideIoWriteAtomic(index,(xbytesview){indexBytes,indexSize}));
    int oldLine=app.catalog.entries[c].sourceLine; app.catalog.entries[c].sourceLine=2147483647; xideWorkbenchShowHelp(&app,(int)c);
    CHECK(settle(&app) && sourceClick(&app,0) && strstr(xideCatalogSourceMessage(&app),"超出文件范围")); app.catalog.entries[c].sourceLine=oldLine; CHECK(key(&app,XUI_KEY_ESCAPE,0));
    CHECK(xideCatalogViewReload(&app) && settle(&app)); c=named(&app,"bool"); xideWorkbenchShowHelp(&app,(int)c);
    CHECK(settle(&app) && sourceClick(&app,0) && strstr(xideCatalogSourceMessage(&app),"已校验"));
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && xideUiFontApply(&app,app.uiFontPath,24,&error) && settle(&app) && sourceClick(&app,0));
    CHECK(xuiButtonGetFont(xideCatalogSourceCloseWidget(&app))==app.uiFont && key(&app,XUI_KEY_ESCAPE,0));
    CHECK(xideUiFontApply(&app,app.uiFontPath,16,&error) && settle(&app));
    CHECK(xgeEnd()==XGE_OK); began=0; app.dpiOverride=2; CHECK(xideAppFrame(&app)==XGE_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(app.dpiScale==2 && settle(&app) && sourceClick(&app,0) && settle(&app));
    xui_rect_t area=xuiWidgetGetWorldRect(editor); CHECK(area.fW>100 && area.fH>60 && area.fX>=0 && area.fY>=0 && area.fX+area.fW<=app.viewportWidth && area.fY+area.fH<=app.viewportHeight);
    CHECK(key(&app,XUI_KEY_ESCAPE,0) && app.documents.count==documentCount && app.activeDocument==draft && draft->dirty && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(draft->editor))==version && !strcmp(xuiCodeEditGetText(draft->editor),"int 草稿 = 42; // 保留\n"));
    CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(draft->editor),&selected)==XUI_OK && selected.iAnchorOffset==saved.iAnchorOffset && selected.iCaretOffset==saved.iCaretOffset);
    CHECK(xuiSetFocusWidget(app.context,draft->editor)==XUI_OK && key(&app,'Z',XUI_MOD_CTRL) && !*xuiCodeEditGetText(draft->editor) && !draft->dirty);
    CHECK(xideActionExecute(&app,XIDE_ACTION_SOURCE_DECLARATION)==XIDE_ACTION_DISPATCHED && settle(&app));
    printf("catalog_source_view_test passed: %d assertions; actual native buttons/Escape, read-only C/xlang/document source, exact source line, digest/index/unavailable/bounds rejection, reload, UI font and 125/200%% DPI, preserved draft/selection/undo, four screenshots\n",f->checks);
cleanup:
    if(began) xgeEnd();
    xideTextFileClear(&file); xideAppDestroy(&app);
    free(settings); free(catalog); free(folder); free(index); free(payload); xrtFree(original); xrtFree(indexBytes); for(int i=0;i<4;++i) free(images[i]); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang IDE catalog source verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int ret=xgeInit(&d); if(ret==XGE_OK) ret=xgeRun(verify,f); xgeUnit(); return ret==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    char* id=xideCreateId("source"); char* root=id ? xidePathResolve(argv[1],id) : NULL; free(id);
    if(!root || !xideIoCreateDirectories(root) || !copyResources(root)) { free(root); return 2; }
    fixture f={.root=root}; wchar_t name[100]; swprintf(name,100,L"XlangCatalogSource-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) { free(root); return 1; }
    HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
    if(t) { if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0) { fputs("catalog source verification timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(t,&code); CloseHandle(t); }
    CloseDesktop(f.desktop); free(root); return (int)code;
}
