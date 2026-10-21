#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture { const char* directory; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)) { fprintf(stderr,"find_options_view_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int key(xide_app* a,int k,uint32_t mods) { return xuiInputKeyDownEx(a->context,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return r.fW>0 && r.fH>0 && xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static xui_widget control(xide_app* a,xui_widget parent,const char* type,const char* text,int* index)
{
    if(!parent) return NULL;
    if(xuiWidgetGetType(parent)==xuiWidgetFindType(a->context,type)) {
        const char* label=!strcmp(type,"button") ? xuiButtonGetText(parent) : !strcmp(type,"checkbox") ? xuiCheckBoxGetText(parent) : !strcmp(type,"label") ? xuiLabelGetText(parent) : "";
        if((!text || !strcmp(label,text)) && (*index)--==0) return parent;
    }
    for(xui_widget child=xuiWidgetGetFirstChild(parent);child;child=xuiWidgetGetNextSibling(child)) { xui_widget found=control(a,child,type,text,index); if(found) return found; }
    return NULL;
}
static xui_widget get(xide_app* a,xide_document* d,const char* type,int text,int index) { return control(a,xuiCodeEditGetFindWindow(d->editor),type,text ? xuiTranslate(a->context,text) : NULL,&index); }
static int button(xide_app* a,xide_document* d,int text) { xui_widget w=get(a,d,"button",text,0); return w && pointer(a,xuiWidgetGetWorldRect(w)); }
static int flag(xide_app* a,xide_document* d,int text,int on) { xui_widget w=get(a,d,"checkbox",text,0); return w && (xuiCheckBoxGetChecked(w)==on || pointer(a,xuiWidgetGetWorldRect(w))) && xuiCheckBoxGetChecked(w)==on; }
static int field(xide_app* a,xide_document* d,int index,const char* text) { xui_widget w=get(a,d,"input",0,index); return w && xuiInputSetText(w,text)==XUI_OK && settle(a); }
static int openSearch(xide_app* a,xide_document* d) { return xideEditorShow(a,d)==XUI_OK && settle(a) && key(a,'H',XUI_MOD_CTRL) && xuiWidgetGetVisible(xuiCodeEditGetFindWindow(d->editor)); }
static int dismiss(xide_app* a,xide_document* d) { xui_widget input=get(a,d,"input",0,0); return input && xuiSetFocusWidget(a->context,input)==XUI_OK && settle(a) && key(a,XUI_KEY_ESCAPE,0) && !xuiWidgetGetVisible(xuiCodeEditGetFindWindow(d->editor)); }
static int undo(xide_app* a,xide_document* d,const char* original) { return dismiss(a,d) && key(a,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(d->editor),original) && !d->dirty; }
static int capture(xide_app* a,const char* root,const char* language,const char* name) { char leaf[128];snprintf(leaf,sizeof(leaf),"%s-%s.png",language,name);char* p=xidePathResolve(root,leaf);xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};int ok=p && xuiUpdate(a->context,0)==XUI_OK && a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK;free(p);return ok; }
static char* replaced(const char* source,const char* old,const char* value) { const char* p=strstr(source,old);if(!p) return NULL;size_t n=strlen(source)-strlen(old)+strlen(value);char* out=malloc(n+1);if(out) { size_t start=(size_t)(p-source);memcpy(out,source,start);memcpy(out+start,value,strlen(value));strcpy(out+start+strlen(value),p+strlen(old)); }return out; }
static int sameFile(const char* p,const char* text) { size_t n=0;unsigned char* raw=xrtFileReadAllLimit(p,4096,&n);int same=raw && n==strlen(text) && !memcmp(raw,text,n);xrtFree(raw);return same; }
static int verify(void* user)
{
    fixture* f=user;if(f->started) return XGE_OK;f->started=1;
    xide_app app={0};xide_model_error error={0};int began=0;char *root=NULL,*paths[2]={0},*settings=NULL,*expected=NULL;
    const char* texts[]={"// 中😀\n// item12 ITEM34 item56\n// a\tb\n// target outside\n// target inside\n",
                         "#doc\n中😀\nitem12 ITEM34 item56\na\tb\ntarget outside\ntarget inside\n#end\nfunc main() as int\n return 0\nend\n"};
    const char* languages[]={"c","xlang"};xide_document* docs[2]={0};
    char* id=xideCreateId("find-options");root=id?xidePathResolve(f->directory,id):NULL;free(id);CHECK(root && xrtDirCreateAll(root));
    paths[0]=xidePathResolve(root,"正文.c");paths[1]=xidePathResolve(root,"正文.xl");settings=xidePathResolve(root,"settings.json");CHECK(paths[0] && paths[1] && settings);
    for(int i=0;i<2;++i) CHECK(xrtFileWriteAtomic(paths[i],(xbytesview){(const unsigned char*)texts[i],strlen(texts[i])}));
    app.hidden=1;app.noSession=1;app.settingsPath=settings;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(settle(&app));
    for(int i=0;i<2;++i) { CHECK(xideEditorOpen(&app,paths[i],&error) && settle(&app));docs[i]=app.activeDocument;CHECK(!strcmp(docs[i]->language,languages[i])); }
    for(int i=0;i<2;++i) {
        xide_document* d=docs[i];xide_document* peer=docs[1-i];const char* original=texts[i];const char* language=languages[i];
        xui_code_document document=xuiCodeEditGetDocument(d->editor);uint32_t peerVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(peer->editor));
        CHECK(xideWorkbenchApplyTheme(&app,i?IDE_THEME_GRAPHITE:IDE_THEME_PAPER)==XUI_OK && openSearch(&app,d));
        CHECK(flag(&app,d,XUI_TR_FIND_SCOPE,0) && flag(&app,d,XUI_TR_FIND_SELECTION,0) && flag(&app,d,XUI_TR_FIND_REGEX,0) && flag(&app,d,XUI_TR_FIND_ESCAPE,0) && flag(&app,d,XUI_TR_FIND_WORD,0) && flag(&app,d,XUI_TR_FIND_CASE,0));
        CHECK(field(&app,d,0,"item") && button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==3);
        CHECK(flag(&app,d,XUI_TR_FIND_CASE,1) && button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==2);
        CHECK(flag(&app,d,XUI_TR_FIND_WORD,1) && button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==0);
        CHECK(field(&app,d,0,"item12") && button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==1);
        CHECK(flag(&app,d,XUI_TR_FIND_WORD,0) && flag(&app,d,XUI_TR_FIND_REGEX,1) && field(&app,d,0,"item([0-9]+)") && field(&app,d,1,"value$1"));
        CHECK(button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==2 && capture(&app,root,language,"regex"));
        expected=replaced(original,"item12 ITEM34 item56","value12 ITEM34 value56");CHECK(expected && button(&app,d,XUI_TR_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(d->editor),expected) && d->dirty);
        CHECK(undo(&app,d,original) && key(&app,'Y',XUI_MOD_CTRL) && !strcmp(xuiCodeEditGetText(d->editor),expected) && key(&app,'Z',XUI_MOD_CTRL) && !d->dirty);free(expected);expected=NULL;
        CHECK(openSearch(&app,d) && flag(&app,d,XUI_TR_FIND_REGEX,0) && flag(&app,d,XUI_TR_FIND_ESCAPE,1) && field(&app,d,0,"a\\tb") && field(&app,d,1,"中\\uD83D\\uDE00"));
        CHECK(button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==1);
        expected=replaced(original,"a\tb","中😀");CHECK(expected && button(&app,d,XUI_TR_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(d->editor),expected) && capture(&app,root,language,"unicode-escape"));
        CHECK(undo(&app,d,original));free(expected);expected=NULL;
        CHECK(openSearch(&app,d) && field(&app,d,0,"target"));
        const char* invalid[]={"\\x00tail","\\u0000tail","\\uD800","\\uDC00","\\uD83D\\u0041","\\x","\\u123"};
        for(int regex=0;regex<2;++regex) {
            CHECK(flag(&app,d,XUI_TR_FIND_REGEX,regex));
            for(size_t bad=0;bad<sizeof(invalid)/sizeof(invalid[0]);++bad) {
                uint32_t version=xuiCodeDocumentGetVersion(document);int canUndo=xuiCodeDocumentCanUndo(document),canRedo=xuiCodeDocumentCanRedo(document);
                CHECK(field(&app,d,1,invalid[bad]) && button(&app,d,XUI_TR_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(d->editor),original) && !d->dirty && get(&app,d,"label",XUI_TR_FIND_INVALID_PATTERN,0));
                CHECK(version==xuiCodeDocumentGetVersion(document) && canUndo==xuiCodeDocumentCanUndo(document) && canRedo==xuiCodeDocumentCanRedo(document));
                xui_find_options_t options={.iSize=sizeof(options),.sPattern="target",.sReplacement=invalid[bad],.iFlags=XUI_FIND_ESCAPE|(regex?XUI_FIND_REGEX:0),.iStartOffset=-1,.iRangeEnd=-1};int count=-1;
                CHECK(xuiCodeEditReplaceAll(d->editor,&options,&count)==XUI_ERROR_INVALID_ARGUMENT && count==0 && version==xuiCodeDocumentGetVersion(document));
            }
        }
        CHECK(field(&app,d,0,"item([0-9]+)") && field(&app,d,1,"$1\\uD83D\\uDE00"));
        expected=replaced(original,"item12 ITEM34 item56","12😀 ITEM34 56😀");CHECK(expected && button(&app,d,XUI_TR_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(d->editor),expected));
        CHECK(undo(&app,d,original));free(expected);expected=NULL;
        CHECK(openSearch(&app,d) && flag(&app,d,XUI_TR_FIND_ESCAPE,0) && field(&app,d,0,"(") && button(&app,d,XUI_TR_FIND_ALL));
        printf("invalid regex %s: count=%d dirty=%d invalidStatus=%d notFoundStatus=%d\n",language,xuiCodeEditGetFindResultCount(d->editor),d->dirty,get(&app,d,"label",XUI_TR_FIND_INVALID_PATTERN,0)!=NULL,get(&app,d,"label",XUI_TR_FIND_NOT_FOUND,0)!=NULL);
        CHECK(xuiCodeEditGetFindResultCount(d->editor)==0 && get(&app,d,"label",XUI_TR_FIND_INVALID_PATTERN,0) && !d->dirty && capture(&app,root,language,"invalid-regex"));
        CHECK(flag(&app,d,XUI_TR_FIND_SCOPE,1) && field(&app,d,0,"item([0-9]+)") && button(&app,d,XUI_TR_FIND_ALL) && xuiCodeFindScopeGetResultCount(app.editorFindScope)==4);
        xui_widget table=get(&app,d,"tableview",0,0);CHECK(table && xuiTableViewGetRowCount(table)==4);
        CHECK(field(&app,d,0,"(") && button(&app,d,XUI_TR_FIND_ALL) && xuiCodeFindScopeGetResultCount(app.editorFindScope)==0 && xuiTableViewGetRowCount(table)==0);
        CHECK(xuiCodeEditGetFindResultCount(docs[0]->editor)==0 && xuiCodeEditGetFindResultCount(docs[1]->editor)==0 && xuiCodeFindScopeActivateResult(app.editorFindScope,0)!=XUI_OK);
        CHECK(flag(&app,d,XUI_TR_FIND_SCOPE,0));
        CHECK(dismiss(&app,d));
        int selectedStart=(int)(strstr(original,"target inside")-original),selectedEnd=selectedStart+(int)strlen("target inside");
        CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(d->editor),document,selectedEnd,selectedStart)==XUI_OK && settle(&app) && openSearch(&app,d));
        CHECK(flag(&app,d,XUI_TR_FIND_REGEX,0) && flag(&app,d,XUI_TR_FIND_SELECTION,1) && field(&app,d,0,"target") && field(&app,d,1,"selected"));
        CHECK(button(&app,d,XUI_TR_FIND_ALL) && xuiCodeEditGetFindResultCount(d->editor)==1 && capture(&app,root,language,"selection"));
        expected=replaced(original,"target inside","selected inside");CHECK(expected && button(&app,d,XUI_TR_REPLACE_ALL) && !strcmp(xuiCodeEditGetText(d->editor),expected));
        CHECK(undo(&app,d,original));free(expected);expected=NULL;
        CHECK(!peer->dirty && !strcmp(xuiCodeEditGetText(peer->editor),texts[1-i]) && peerVersion==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(peer->editor)));
        CHECK(sameFile(paths[0],texts[0]) && sameFile(paths[1],texts[1]));
    }
    printf("find_options_view_test passed: %d native assertions; real C/xlang find/replace controls, case/whole-word, regex captures, literal tab and scalar UTF-8 escapes, invalid-input document/version/undo preservation, malformed regex status, reverse selected-range replacement, single undo/redo; eight images; disk and peer documents preserved\n",f->checks);
cleanup:
    if(began) { xgeEnd(); }
    xideAppDestroy(&app);free(root);free(paths[0]);free(paths[1]);free(settings);free(expected);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* data) { fixture* f=data;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang IDE native find options verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int ret=xgeInit(&d);if(ret==XGE_OK) ret=xgeRun(verify,f);xgeUnit();return ret==XGE_OK && f->started && !f->failed?0:1; }
int main(int argc,char** argv) { if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;fixture f={.directory=argv[1]};wchar_t name[100];swprintf(name,100,L"XlangFindOptions-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t) { if(WaitForSingleObject(t,90000)!=WAIT_OBJECT_0) { fputs("find_options_view_test timed out\n",stderr);ExitProcess(1); }GetExitCodeThread(t,&code);CloseHandle(t); }CloseDesktop(f.desktop);return (int)code; }
