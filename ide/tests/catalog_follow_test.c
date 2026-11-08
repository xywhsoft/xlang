#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "catalog_ready.h"
#include "ui/editor_view.h"
#include "ui/help_document.h"
#include "ui/workbench_windows.h"
#include "ui/settings_view.h"
#include "services/catalog_lookup.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture {HDESK desktop;const char* directory;int started,failed,checks;} fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"catalog_follow_test:%d: %s; %s\n",__LINE__,#e,error.message);f->failed=1;goto cleanup;}}while(0)
static xstrview sv(const char* s){return (xstrview){s,strlen(s)};}
static int settle(xide_app* a) {
 if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;
 xideWorkbenchLayout(a);
 return xuiLayout(a->context)==XUI_OK && xideCatalogViewPrepareRender(a)==XUI_OK && !a->frameError;
}
static int mark(xide_app* a,const char* needle) {
 xui_widget editor=a->activeDocument->editor;const char* text=xuiCodeEditGetText(editor),*p=strstr(text,needle);if(!p)return 0;
 return xuiSetFocusWidget(a->context,editor)==XUI_OK && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(editor),xuiCodeEditGetDocument(editor),(int)(p-text),(int)(p-text+strlen(needle)))==XUI_OK && settle(a);
}
static int markOffset(xide_app* a,int first,int last) {xui_widget editor=a->activeDocument->editor;return xuiSetFocusWidget(a->context,editor)==XUI_OK && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(editor),xuiCodeEditGetDocument(editor),first,last)==XUI_OK && settle(a);}
static int tick(xide_app* a,uint64_t t){xideCatalogFollowPollAt(a,t);xideCatalogFollowPollAt(a,t+81);return settle(a);}
static int chosen(xide_app* a,const char* name){return a->selectedCommand>=0 && (size_t)a->selectedCommand<a->catalog.count && !strcmp(a->catalog.entries[a->selectedCommand].name,name);}
static int pointer(xide_app* a,xui_rect_t r){float x=r.fX+r.fW/2,y=r.fY+r.fH/2;return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a);}
static int choose(xide_app* a,int index) {
 xui_widget c=xideCatalogFollowChoices(a);if(xuiComboBoxOpen(c)!=XUI_OK || !settle(a))return 0;
 xui_widget menu=xuiComboBoxGetMenuWidget(c);xui_rect_t r=xuiMenuGetItemRect(menu,index),w=xuiWidgetGetWorldRect(menu);r.fX+=w.fX;r.fY+=w.fY;
 return pointer(a,r) && xuiComboBoxGetSelected(c)==index;
}
static int waitHelp(xide_app* a) {
 xide_help_view* v=xideCatalogHelpDocument(a);uint64_t end=GetTickCount64()+10000;
 do {if(xideHelpPoll(v)!=XUI_OK || !settle(a))return 0;if(!xideHelpPending(v))return 1;Sleep(1);}while(GetTickCount64()<end);
 return 0;
}
static int put(xvalue* v,const char* key,const char* value){return xrtValueObjectSetNew(v,sv(key),xrtValueString(sv(value)));}
static int replaceCatalog(xide_app* a,const char* path,const char* markdown) {
 xvalue* root=xrtValueObject(),*entries=xrtValueArray(),*docs=xrtValueArray();int ok=root && entries && docs;
 for(int i=0;i<3 && ok;++i) {
  xvalue* item=xrtValueDeepClone(xrtValueArrayGet(xrtValueObjectGet(a->catalog.source,sv("entries")),0)),*refs=xrtValueArray();
  const char* name=i==0 ? "followDoc":i==1 ? "followOther":"测试函数";char id[30];snprintf(id,sizeof(id),"follow:%d",i);
  ok=item && refs && put(item,"id",id) && put(item,"name",name) && put(item,"signature",name) && put(item,"example","") && put(item,"language","xlang") && xrtValueArrayAppendNew(refs,xrtValueString(sv(i==1 ? "doc-b":"doc-a"))) && xrtValueObjectSetNew(item,sv("sections"),refs) && xrtValueArrayAppendNew(entries,item);
 }
 for(int i=0;i<2 && ok;++i){xvalue* doc=xrtValueObject();ok=doc && put(doc,"id",i ? "doc-b":"doc-a") && put(doc,"title",i ? "Other document":"First document") && put(doc,"markdown",i ? "# Other\nIndependent body.\n":markdown) && put(doc,"source","fixture.md") && xrtValueObjectSetNew(doc,sv("sourceLine"),xrtValueInt(1)) && xrtValueArrayAppendNew(docs,doc);}
 ok=ok && put(root,"format","xlang-command-catalog-1") && xrtValueObjectSetNew(root,sv("entries"),entries) && xrtValueObjectSetNew(root,sv("documents"),docs);
 size_t bytes=0;char* text=ok ? xrtJsonStringify(root,false,&bytes):NULL;
 ok=text && xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,bytes});xrtFree(text);xrtValueRelease(root);return ok;
}
static int doubles(const void* a,const void* b){double x=*(const double*)a,y=*(const double*)b;return x<y ? -1:x>y;}
static double nowUs(void){LARGE_INTEGER t,f;QueryPerformanceCounter(&t);QueryPerformanceFrequency(&f);return (double)t.QuadPart*1000000/f.QuadPart;}
static int benchmark(xide_app* a,const char* directory) {
 enum {N=600,M=80};double off[N],on[N],publish[N],input[2][M],scroll[2][M];xui_widget editor=a->activeDocument->editor;xui_code_document core=xuiCodeEditGetDocument(editor);
 char* large=malloc(1024*1024+1);if(!large)return 0;const char* line="math.sin(1) math.abs(-1)\n";size_t bytes=strlen(line);
 for(size_t at=0;at<1024*1024;){size_t n=1024*1024-at;if(n>bytes)n=bytes;memcpy(large+at,line,n);at+=n;}large[1024*1024]=0;int ok=xuiCodeEditSetText(editor,large)==XUI_OK;free(large);if(!ok || !settle(a))return 0;
 for(int mode=0;mode<2;++mode){a->preferences.options.helpFollow=mode;a->preferences.optionsReady=1;for(int i=0;i<N;++i){xuiCodeSelectionSetRange(xuiCodeEditGetSelection(editor),core,5,8);double t=nowUs();xideCatalogFollowPollAt(a,100000+(uint64_t)i*100);(mode ? on:off)[i]=nowUs()-t;}}
 a->preferences.options.helpFollow=1;
 for(int i=0;i<N;++i){int p=i&1 ? 17:5;xuiCodeSelectionSetRange(xuiCodeEditGetSelection(editor),core,p,p+3);uint64_t t=200000+(uint64_t)i*100;xideCatalogFollowPollAt(a,t);double began=nowUs();xideCatalogFollowPollAt(a,t+81);publish[i]=nowUs()-began;}
 if(!waitHelp(a))return 0;
 for(int mode=0;mode<2;++mode) {
  a->preferences.options.helpFollow=mode;if(!markOffset(a,0,0))return 0;
  for(int i=-4;i<M;++i) {
   double t=nowUs();if(xuiInputText(a->context,'z')!=XUI_OK || xuiInputKeyDownEx(a->context,XUI_KEY_BACKSPACE,0,NULL)!=XUI_OK || xuiInputKeyUpEx(a->context,XUI_KEY_BACKSPACE,0,NULL)!=XUI_OK)return 0;
   xideCatalogFollowPollAt(a,GetTickCount64());if(!settle(a))return 0;if(i>=0)input[mode][i]=nowUs()-t;
  }
  xui_rect_t rect=xuiWidgetGetWorldRect(editor);if(xuiInputPointerMove(a->context,rect.fX+100,rect.fY+50,0)!=XUI_OK)return 0;
  for(int i=-4;i<M;++i){double t=nowUs();if(xuiInputPointerWheel(a->context,rect.fX+100,rect.fY+50,0,i&1 ? 1:-1,0)!=XUI_OK)return 0;xideCatalogFollowPollAt(a,GetTickCount64());if(!settle(a))return 0;if(i>=0)scroll[mode][i]=nowUs()-t;}
  qsort(input[mode],M,sizeof(double),doubles);qsort(scroll[mode],M,sizeof(double),doubles);
 }
 a->preferences.options.helpFollow=1;
 qsort(off,N,sizeof(double),doubles);qsort(on,N,sizeof(double),doubles);qsort(publish,N,sizeof(double),doubles);
 char* path=xidePathResolve(directory,"selection-performance.json");FILE* out=path ? fopen(path,"wb"):NULL;
 if(out){fprintf(out,"{\"samples\":%d,\"documentBytes\":1048576,\"observerOffP95Us\":%.3f,\"observerOffP99Us\":%.3f,\"observerOnP95Us\":%.3f,\"observerOnP99Us\":%.3f,\"lookupPublishP95Us\":%.3f,\"lookupPublishP99Us\":%.3f,\"interactionSamples\":%d,\"inputOffP95Us\":%.3f,\"inputOffP99Us\":%.3f,\"inputOnP95Us\":%.3f,\"inputOnP99Us\":%.3f,\"scrollOffP95Us\":%.3f,\"scrollOffP99Us\":%.3f,\"scrollOnP95Us\":%.3f,\"scrollOnP99Us\":%.3f,\"interactionScope\":\"native dispatch, follow observer, UI layout; excludes paint/presentation\"}\n",N,off[N*95/100],off[N*99/100],on[N*95/100],on[N*99/100],publish[N*95/100],publish[N*99/100],M,input[0][M*95/100],input[0][M*99/100],input[1][M*95/100],input[1][M*99/100],scroll[0][M*95/100],scroll[0][M*99/100],scroll[1][M*95/100],scroll[1][M*99/100]);fclose(out);}free(path);return out!=NULL;
}
static int verify(void* user) {
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app a={0};xide_model_error error={0};int began=0;
 char *settings=xidePathResolve(f->directory,"settings.json"),*catalog=xidePathResolve(f->directory,"commands.json");wchar_t exe[32768];char binary[131072];char *runtime=NULL,*shipping=NULL,*image=NULL,*markdown=NULL;
 CHECK(GetModuleFileNameW(NULL,exe,32768) && WideCharToMultiByte(CP_UTF8,0,exe,-1,binary,sizeof(binary),NULL,NULL));runtime=xidePathDirectory(binary);shipping=runtime ? xidePathResolve(runtime,"data/commands.json"):NULL;
 CHECK(settings && catalog && shipping && xrtFileCopy(shipping,catalog,true));a.hidden=1;a.noSession=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.catalogPath=catalog;
 CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&a) && settle(&a));
 CHECK(xideSettingsOptions(&a.preferences)->helpFollow);
 xide_catalog_lookup* index=xideCatalogLookupCreate(&a.catalog);CHECK(index && xideCatalogLookupBytes(index)<a.catalog.count*13+256);
 uint32_t items[64];size_t total=0;CHECK(xideCatalogLookupName(index,"math.sin","xlang",0,items,64,&total)==1 && total==1 && xideCatalogLookupId(index,a.catalog.entries[items[0]].id)==a.catalog.entries+items[0]);xideCatalogLookupDestroy(index);
 xide_document* d=xideDocumentNew(&a.documents,"xlang",&error);CHECK(d && xideEditorShow(&a,d)==XUI_OK);
 const char* code="math.sin(1)\nmath.abs(-1)\nobj.append(2)\ncounter\n\"math.sin\"\n// math.abs\n测试函数()\n";
 CHECK(xuiCodeEditSetText(d->editor,code)==XUI_OK && mark(&a,"sin"));int previous=a.selectedCommand;
 xideCatalogFollowPollAt(&a,1000);CHECK(a.selectedCommand==previous);xideCatalogFollowPollAt(&a,1079);CHECK(a.selectedCommand==previous);CHECK(tick(&a,1080) && chosen(&a,"math.sin") && xuiGetFocusWidget(a.context)==d->editor);
 uint32_t exampleVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(a.helpExample));CHECK(xuiScrollViewSetOffset(a.help,0,50)==XUI_OK);float scroll=0;xuiScrollViewGetOffset(a.help,NULL,&scroll);
 CHECK(mark(&a,"math.sin") && tick(&a,1300) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(a.helpExample))==exampleVersion);float after=0;xuiScrollViewGetOffset(a.help,NULL,&after);CHECK(after==scroll);
 CHECK(mark(&a,"abs") && tick(&a,1500) && chosen(&a,"math.abs") && xideCatalogFollowCandidateCount(&a)>1 && choose(&a,1));int overload=a.selectedCommand;
 CHECK(mark(&a,"math.abs") && tick(&a,1700) && a.selectedCommand==overload);
 CHECK(xuiCheckBoxSetChecked(xideCatalogFollowPin(&a),1)==XUI_OK && settle(&a) && mark(&a,"sin") && tick(&a,2000) && a.selectedCommand==overload);
 CHECK(xuiCheckBoxSetChecked(xideCatalogFollowPin(&a),0)==XUI_OK && settle(&a) && tick(&a,2200) && chosen(&a,"math.sin"));
 a.preferences.options=*xideSettingsOptions(&a.preferences);a.preferences.optionsReady=1;a.preferences.options.helpFollow=0;
 CHECK(mark(&a,"abs") && tick(&a,2400) && chosen(&a,"math.sin"));a.preferences.options.helpFollow=1;
 CHECK(xuiDockPanelHideWindow(a.dock,a.windows[XIDE_HELP])==XUI_OK && settle(&a) && tick(&a,2600) && chosen(&a,"math.sin") && xideWorkbenchWindowVisible(&a,XIDE_HELP)==0);
 CHECK(mark(&a,"abs") && xideActionExecute(&a,XIDE_ACTION_CONTEXT_HELP)==XIDE_ACTION_DISPATCHED && settle(&a) && chosen(&a,"math.abs") && xideWorkbenchWindowVisible(&a,XIDE_HELP)==1);
 CHECK(xuiDockPanelSetPaneActiveWindow(a.dock,a.helpPane,a.windows[XIDE_PROBLEMS])==XUI_OK && settle(&a) && mark(&a,"sin") && tick(&a,2800) && chosen(&a,"math.abs"));CHECK(xideWorkbenchRevealHelp(&a)==XUI_OK && settle(&a));
 CHECK(mark(&a,"counter") && tick(&a,3000) && chosen(&a,"math.abs"));
 const char* quoted=strstr(code,"\"math.sin\"")+1,*comment=strstr(code,"// math.abs")+3;
 CHECK(markOffset(&a,(int)(quoted-code),(int)(quoted-code+8)) && tick(&a,3050) && chosen(&a,"math.abs"));
 CHECK(markOffset(&a,(int)(comment-code),(int)(comment-code+8)) && tick(&a,3100) && chosen(&a,"math.abs"));
 CHECK(mark(&a,"obj.append") && tick(&a,3200) && xideCatalogFollowCandidateCount(&a)>1);
 int retained=a.selectedCommand;CHECK(mark(&a,"in") && tick(&a,3400) && a.selectedCommand==retained);
 CHECK(mark(&a,"sin") && xuiSetPointerCapture(a.context,d->editor)==XUI_OK && tick(&a,3600) && a.selectedCommand==retained && xuiReleasePointerCapture(a.context,d->editor)==XUI_OK && tick(&a,3800) && chosen(&a,"math.sin"));
 CHECK(mark(&a,"abs") && xuiInputImeComposition(a.context,"你",3,0,1)==XUI_OK && settle(&a) && tick(&a,4000) && chosen(&a,"math.sin"));CHECK(xuiInputImeComposition(a.context,"",0,0,0)==XUI_OK && settle(&a));
 size_t example=0;while(example<a.catalog.count && !*a.catalog.entries[example].example)++example;CHECK(example<a.catalog.count && xideCatalogViewSelect(&a,example) && settle(&a) && xuiSetFocusWidget(a.context,a.helpExample)==XUI_OK && tick(&a,4200) && a.selectedCommand==(int)example);
 CHECK(mark(&a,"sin") && tick(&a,4300) && chosen(&a,"math.sin"));
 CHECK(mark(&a,"abs"));xideCatalogFollowPollAt(&a,4400);CHECK(mark(&a,"sin"));xideCatalogFollowPollAt(&a,4420);xideCatalogFollowPollAt(&a,4481);CHECK(chosen(&a,"math.sin"));CHECK(tick(&a,4500));
 CHECK(xideDocumentSetLanguage(d,"c",&error) && xuiCodeEditSetText(d->editor,"return math.sin(1);")==XUI_OK && mark(&a,"return") && tick(&a,4600) && chosen(&a,"return") && !strcmp(a.catalog.entries[a.selectedCommand].language,"c"));
 CHECK(mark(&a,"sin") && tick(&a,4700) && chosen(&a,"return"));
 CHECK(xideDocumentSetLanguage(d,"xlang",&error) && xuiCodeEditSetText(d->editor,code)==XUI_OK && mark(&a,"sin") && tick(&a,4800) && chosen(&a,"math.sin"));
 CHECK(benchmark(&a,f->directory));
 CHECK(xideEditorClose(&a,d,1,&error));xide_document* reused=xideDocumentNew(&a.documents,"xlang",&error);CHECK(reused==d && xideEditorShow(&a,d)==XUI_OK && xuiCodeEditSetText(d->editor,code)==XUI_OK && mark(&a,"abs") && tick(&a,4900) && chosen(&a,"math.abs"));
 markdown=malloc(120000);CHECK(markdown);strcpy(markdown,"# First\n");for(int i=0;i<800;++i)strcat(markdown,"Paragraph with enough text for scrolling and virtualization.\n\n");
 CHECK(replaceCatalog(&a,catalog,markdown) && xideCatalogViewReload(&a) && xideTestCatalogReady(&a) && a.catalog.count==3);
 CHECK(xuiCodeEditSetText(d->editor,"followDoc()\nfollowOther()\n测试函数()\n")==XUI_OK && mark(&a,"followDoc") && tick(&a,5000) && waitHelp(&a));xide_help_view* help=xideCatalogHelpDocument(&a);unsigned builds=0,hits=0;xideHelpPreparationCounts(help,&builds,&hits);CHECK(builds>=1);
 CHECK(mark(&a,"followOther") && tick(&a,5200) && waitHelp(&a));unsigned two=0;xideHelpPreparationCounts(help,&two,NULL);CHECK(two>builds);
 CHECK(mark(&a,"followDoc") && tick(&a,5400) && waitHelp(&a));xideHelpPreparationCounts(help,&builds,&hits);unsigned warm=builds;CHECK(mark(&a,"followOther") && tick(&a,5460) && waitHelp(&a));CHECK(mark(&a,"followDoc") && tick(&a,5520) && waitHelp(&a));xideHelpPreparationCounts(help,&builds,&hits);CHECK(builds==warm && hits>0);
 CHECK(xuiWidgetGetChildCount(xuiWidgetGetLastChild(xideHelpContent(help)))<24 && xuiScrollViewSetOffset(a.help,0,1000000)==XUI_OK);
 for(int i=0;i<6;++i)CHECK(settle(&a));
 CHECK(xuiWidgetGetChildCount(xuiWidgetGetLastChild(xideHelpContent(help)))<32 && xuiScrollViewSetOffset(a.help,0,0)==XUI_OK);
 CHECK(mark(&a,"测试函数") && tick(&a,5600) && chosen(&a,"测试函数") && waitHelp(&a));
 CHECK(xideHelpSetDeferred(help,NULL,0,NULL)==XUI_ERROR_INVALID_ARGUMENT);
 CHECK(xideHelpSetDeferred(help,a.catalog.entries[1].documents,a.catalog.entries[1].documentCount,&a.catalog)==XUI_OK && xideHelpSet(help,a.catalog.entries[0].documents,a.catalog.entries[0].documentCount)==XUI_OK && xuiWidgetGetVisible(xideHelpContent(help)) && !xideHelpPending(help));
 CHECK(mark(&a,"followOther") && tick(&a,5800) && replaceCatalog(&a,catalog,"# New owner\nReload-safe body.\n") && xideCatalogViewReload(&a) && xideTestCatalogReady(&a) && waitHelp(&a));CHECK(xideHelpSourceDocument(help)==a.catalog.documents || xideHelpSourceDocument(help)==a.catalog.documents+1);
 for(int i=0;i<10;++i){CHECK(xideHelpPoll(help)==XUI_OK && settle(&a));Sleep(1);}
 image=xidePathResolve(f->directory,"follow-help.png");CHECK(image && xuiUpdate(a.context,0)==XUI_OK);xui_rect_i_t all={0,0,a.viewportWidth,a.viewportHeight};CHECK(a.proxy.surfaceClear(&a.proxy,a.target,0)==XUI_OK && xuiRender(a.context,a.target,&all,1)==XUI_OK && xideAppCapture(&a,image)==XGE_OK);
 printf("catalog_follow_test passed: %d checks; bounded/indexed qualified selection, debounce, overload preservation, pin/setting, hidden/inactive panes, F1, drag/IME exclusions, no focus stealing, worker/cache/reload ownership, Unicode and 1MiB selection performance.\n",f->checks);
cleanup:if(began)xgeEnd();xideAppDestroy(&a);free(settings);free(catalog);free(runtime);free(shipping);free(image);free(markdown);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user){fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="Selection help regression";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;int r=xgeInit(&d);if(r==XGE_OK)r=xgeRun(verify,f);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0:1;}
int main(int argc,char** argv){if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1]};wchar_t name[80];swprintf(name,80,L"XlangFollow-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t){if(WaitForSingleObject(t,120000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);return code;}
