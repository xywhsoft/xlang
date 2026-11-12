#define _WIN32_WINNT 0x0601
/* Include the actual producer only to inspect exhaustion/prefix counters and
 * to compile its direct allocation sites under the linker's probes. */
#include "editor/documents.c"
#include "services/editor_font.h"
static unsigned checks,tracking,allocations,frees,reads,writes,paths,callbacks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"document-source:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void* __real_realloc(void*,size_t);void __real_free(void*);
void* __wrap_malloc(size_t n){if(tracking)++allocations;return __real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){if(tracking)++allocations;return __real_calloc(n,z);}
void* __wrap_realloc(void* p,size_t n){if(tracking)++allocations;return __real_realloc(p,n);}
void __wrap_free(void* p){if(tracking)++frees;__real_free(p);}
int __real_xideTextFileRead(const char*,xide_text_file*,xide_model_error*);
int __wrap_xideTextFileRead(const char* p,xide_text_file* t,xide_model_error* e){if(tracking)++reads;return __real_xideTextFileRead(p,t,e);}
char* __real_xidePathResolve(const char*,const char*);
char* __wrap_xidePathResolve(const char* b,const char* p){if(tracking)++paths;return __real_xidePathResolve(b,p);}
extern unsigned char* (*__real___imp_xrtFileReadAllLimit)(const char*,size_t,size_t*);
static unsigned char* readProbe(const char* p,size_t n,size_t* z){if(tracking)++reads;return __real___imp_xrtFileReadAllLimit(p,n,z);}
unsigned char* (*__wrap___imp_xrtFileReadAllLimit)(const char*,size_t,size_t*)=readProbe;
extern bool (*__real___imp_xrtFileWriteAtomic)(const char*,xbytesview);
static bool writeProbe(const char* p,xbytesview b){if(tracking)++writes;return __real___imp_xrtFileWriteAtomic(p,b);}
bool (*__wrap___imp_xrtFileWriteAtomic)(const char*,xbytesview)=writeProbe;
static void changedProbe(xide_document* d,void* user)
{(void)user;if(tracking)++callbacks;if(user){xide_model_error e={0};CHECK(d->store->sourceActivity && !xideDocumentsSuspendSources(d->store,&e) && e.code==XIDE_MODEL_CONFLICT);}}
static int beforeSave(xide_document* d,const char* path,xide_model_error* error,void* user)
{(void)path;(void)error;(void)user;if(tracking)++callbacks;xide_model_error e={0};CHECK(d->store->sourceActivity && !xideDocumentsSuspendSources(d->store,&e) && e.code==XIDE_MODEL_CONFLICT);return 1;}
static char* mapper(const char* path,void* user)
{xide_document_store* s=user;xide_model_error e={0};CHECK(s->sourceActivity && !xideDocumentsSuspendSources(s,&e));size_t n=strlen(path)+1;char* result=malloc(n);CHECK(result);memcpy(result,path,n);return result;}
static int textFile(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int key(xui_context c,int k,unsigned mods)
{return xuiInputKeyDown(c,k,mods)==XUI_OK && xuiInputKeyUp(c,k,0)==XUI_OK;}
static void input(xui_context c,xide_document* d)
{
 xui_code_document core=xuiCodeEditGetDocument(d->editor);xui_rect_t r=xuiWidgetGetWorldRect(d->editor);
 CHECK(xuiInputPointerMove(c,r.fX+90,r.fY+12,0)==XUI_OK && xuiInputPointerDown(c,r.fX+90,r.fY+12,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(c,r.fX+90,r.fY+12,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && xuiGetFocusWidget(c)==d->editor);
 CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),core,0,0)==XUI_OK);
 CHECK(xuiInputKeyDown(c,'X',0)==XUI_OK && xuiInputText(c,'X')==XUI_OK && xuiInputKeyUp(c,'X',0)==XUI_OK && d->dirty);
 CHECK(key(c,XUI_KEY_BACKSPACE,0) && !strcmp(xuiCodeEditGetText(d->editor),d->baseline.text) && !d->dirty);
 CHECK(key(c,'Z',XUI_MOD_CTRL) && d->dirty && key(c,'Z',XUI_MOD_CTRL) && !d->dirty);
 uint32_t version=xuiCodeDocumentGetVersion(core);xui_ime_composition_t ime={.iSize=sizeof(ime),.sText="中文😀",.iTextSize=10,.iCursor=10,.iSelectionStart=10,.iSelectionEnd=10,.bActive=1};
 CHECK(xuiInputImeCompositionEx(c,&ime)==XUI_OK && xuiCodeDocumentGetVersion(core)==version);ime.sText="";ime.iTextSize=ime.iCursor=ime.iSelectionStart=ime.iSelectionEnd=0;ime.bActive=0;
 CHECK(xuiInputImeCompositionEx(c,&ime)==XUI_OK && xuiInputText(c,0x4e2d)==XUI_OK && d->dirty && strstr(xuiCodeEditGetText(d->editor),"中"));
 CHECK(key(c,'Z',XUI_MOD_CTRL) && !d->dirty && !strcmp(xuiCodeEditGetText(d->editor),d->baseline.text));
}
typedef struct fixture {const char* directory;HDESK desktop;int started,passed;} fixture;
static int verify(void* value)
{
 fixture* f=value;if(f->started)return XGE_OK;f->started=1;const char* directory=f->directory;CHECK(xgeBegin()==XGE_OK);xide_model_error error={0};xui_context context=NULL;xui_widget root=NULL;xui_proxy_t proxy={0};xide_dpi_proxy* dpi=NULL;xui_font font=NULL;xide_document_store store={0};
 CHECK(xrtDirCreateAll(directory));char* xl=xidePathResolve(directory,"main.xl"),*c=xidePathResolve(directory,"native.c"),*preparedPath=xidePathResolve(directory,"prepared.xl"),*other=xidePathResolve(directory,"other.xl");CHECK(xl && c && preparedPath && other);
 CHECK(textFile(xl,"func main(...) as int\n return 0\nend\n") && textFile(c,"int main(void) { return 0; }\n") && textFile(preparedPath,"func cached() as int\n return 1\nend\n"));
 CHECK(xuiCreate(&context)==XUI_OK);char* fontPath=xideEditorFontDefaultPath();CHECK(fontPath && xideDpiProxyCreate(&dpi,&proxy,1)==XUI_OK && xuiSetProxy(context,&proxy)==XUI_OK && proxy.fontLoadFile(&proxy,&font,fontPath,14,0)==XUI_OK && xuiSetDefaultFont(context,font)==XUI_OK);free(fontPath);
 xideDocumentsInit(&store,context,font,changedProbe,(void*)1);CHECK(xideDocumentsSourceEpoch(&store)==1 && !xideDocumentsSourcesHeld(&store));
 xide_document* first=xideDocumentOpen(&store,xl,&error),*second=xideDocumentOpen(&store,c,&error);CHECK(first && second && !store.sourceActivity);store.user=NULL;store.beforeSave=beforeSave;
 CHECK(xuiWidgetCreate(context,&root)==XUI_OK && xuiWidgetSetLayoutType(root,XUI_LAYOUT_MANUAL)==XUI_OK && xuiSetRootWidget(context,root)==XUI_OK && xuiInputViewport(context,720,500)==XUI_OK && xuiWidgetAddChild(root,first->editor)==XUI_OK && xuiWidgetAddChild(root,second->editor)==XUI_OK);
 CHECK(xuiWidgetSetRect(first->editor,(xui_rect_t){0,0,700,220})==XUI_OK && xuiWidgetSetRect(second->editor,(xui_rect_t){0,240,700,220})==XUI_OK && xuiLayout(context)==XUI_OK);
 uint64_t old=xideDocumentsSourceEpoch(&store);xide_text_file prepared={0},external={0},recovery={0};CHECK(xideTextFileRead(preparedPath,&prepared,&error) && xideTextFileRead(xl,&external,&error) && xideTextFileRead(c,&recovery,&error));unsigned char* preparedRaw=prepared.raw,*externalRaw=external.raw,*recoveryRaw=recovery.raw;
 xide_document_relocation* relocation=xideDocumentsPrepareRelocation(&store,mapper,&store,&error);CHECK(relocation && xideDocumentsValidateRelocation(relocation,&error));
 tracking=1;uint64_t token=xideDocumentsSuspendSources(&store,&error);CHECK(token>old && xideDocumentsSourceEpoch(&store)==token && xideDocumentsSourcesHeld(&store) && xideDocumentsSourcesSuspended(&store,token));
 CHECK(!xideDocumentsSuspendSources(&store,&error) && !xideDocumentsSourcesSuspended(&store,0) && !xideDocumentsResumeSources(&store,old,&error) && xideDocumentsSourcesHeld(&store));
 CHECK(!xideDocumentOpen(&store,xl,&error) && error.code==XIDE_MODEL_CONFLICT && !xideDocumentOpen(&store,other,&error));
 CHECK(!xideDocumentOpenPrepared(&store,preparedPath,&prepared,token,&error) && prepared.raw==preparedRaw && !xideDocumentRecover(&store,other,"recovery","c",&recovery,recovery.text,token,&error) && recovery.raw==recoveryRaw);
 CHECK(!xideDocumentNew(&store,"xlang",&error) && !xideDocumentSave(first,other,3,&error) && !xideDocumentRenameSource(first,xl,other,NULL,&error) && !xideDocumentRename(first,other,&error));
 CHECK(!xideDocumentReload(first,1,&error) && !xideDocumentAcceptExternal(first,&external,1,token,&error) && external.raw==externalRaw && xideDocumentCheckExternal(first));
 CHECK(!xideDocumentClose(first,1,&error) && first->open && !xideDocumentSetLanguage(first,"text",&error));
 CHECK(!xideDocumentsPrepareRelocation(&store,NULL,NULL,&error) && !xideDocumentsValidateRelocation(relocation,&error));xideDocumentsApplyRelocation(relocation);CHECK(!relocation->applied && !store.sourceActivity);
 CHECK(!allocations && !frees && !reads && !writes && !paths && !callbacks);tracking=0;
 input(context,first);input(context,second);CHECK(xideDocumentsSourcesSuspended(&store,token));
 CHECK(xideDocumentsResumeSources(&store,token,&error) && !xideDocumentsSourcesHeld(&store) && !xideDocumentsResumeSources(&store,token,&error));
 tracking=1;CHECK(!xideDocumentOpenPrepared(&store,preparedPath,&prepared,old,&error) && prepared.raw==preparedRaw && !xideDocumentAcceptExternal(first,&external,1,old,&error) && external.raw==externalRaw && !xideDocumentRecover(&store,other,"recovery","c",&recovery,recovery.text,old,&error) && recovery.raw==recoveryRaw);
 CHECK(!xideDocumentsValidateRelocation(relocation,&error));xideDocumentsApplyRelocation(relocation);CHECK(!relocation->applied && !allocations && !frees && !reads && !writes && !paths && !callbacks && !store.sourceActivity);tracking=0;
 CHECK(xideDocumentOpenPrepared(&store,preparedPath,&prepared,token,&error) && !prepared.raw && !prepared.text);xideTextFileClear(&external);xideTextFileClear(&recovery);xideDocumentsDestroyRelocation(relocation);
 CHECK(xideDocumentSave(first,NULL,0,&error) && !store.sourceActivity);CHECK(!xideDocumentNew(&store,"unsupported",&error) && !store.sourceActivity);
 xide_document_store empty={0};xideDocumentsInit(&empty,context,font,NULL,NULL);uint64_t emptyToken=xideDocumentsSuspendSources(&empty,&error);CHECK(emptyToken && !empty.count && xideDocumentsSourcesSuspended(&empty,emptyToken));xideDocumentsClear(&empty);CHECK(!xideDocumentsSourcesSuspended(&empty,emptyToken) && !xideDocumentsResumeSources(&empty,emptyToken,&error));
 store.sourceActivity=UINT_MAX;CHECK(!xideDocumentOpen(&store,xl,&error) && !xideDocumentsSuspendSources(&store,&error));store.sourceActivity=0;store.sourceEpoch=UINT64_MAX-1;
 token=xideDocumentsSuspendSources(&store,&error);CHECK(token==UINT64_MAX && xideDocumentsResumeSources(&store,token,&error) && !xideDocumentsSuspendSources(&store,&error) && !xideDocumentsSourcesHeld(&store));
 xideDocumentsClear(&store);CHECK(!xideDocumentsSourceEpoch(&store) && !xideDocumentsSourcesSuspended(&store,token) && !xideDocumentsResumeSources(&store,token,&error));CHECK(xuiDestroyChecked(context)==XUI_OK);proxy.fontDestroy(&proxy,font);xideDpiProxyDestroy(dpi);free(xl);free(c);free(preparedPath);free(other);xrtClearError();printf("PASS document source lease: %u checks; real C/xlang stores, exact/exhausted tokens, callback exclusion, no admitted I/O or direct allocation during hold/stale acceptance, prepared/recovery/relocation generations, typing/delete/undo/IME/focus\n",checks);CHECK(xgeEnd()==XGE_OK);f->passed=1;xgeQuit();return XGE_OK;
}

static DWORD WINAPI run(void* value)
{
 fixture* f=value;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={.iWidth=720,.iHeight=500,.sTitle="Document source admission verification",.iFlags=XGE_INIT_WINDOW,.iRunMode=XGE_RUN_GAME_LOOP};int status=xgeInit(&d);if(status==XGE_OK)status=xgeRun(verify,f);xgeUnit();return status==XGE_OK && f->passed?0:1;
}
int main(int argc,char** argv)
{
 if(argc!=2)return 2;
 fixture f={.directory=argv[1]};wchar_t name[80];swprintf(name,80,L"DocumentSourceVerification-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE thread=CreateThread(NULL,0,run,&f,0,NULL);DWORD result=1;if(thread){if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&result);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)result;
}
