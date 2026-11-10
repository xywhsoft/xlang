#define main referencesOriginalMain
#include "references_view_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"generic_navigation_view_test:%d:%s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app));f->failed=1;goto cleanup;}}while(0)
static int genericVerify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;
 char *root=xidePathResolve(f->directory,"generic"),*entry=xidePathResolve(root,"入口😀.xl"),*dep=xidePathResolve(root,"dep.xl"),*image=xidePathResolve(root,"generic-navigation.png");
 const char* source="\xef\xbb\xbf" "import dep\r\nfunc identity<T>(T value) as T\r\n return value\r\nend\r\nfunc main(...) as int\r\n auto box=dep.😀盒::<int>(3)\r\n auto other=dep.😀盒::<string>(\"text\")\r\n return identity::<int>(box.value)\r\nend\r\n";
 const char* lib="\xef\xbb\xbf" "export class 😀盒<T>\r\n T value\r\n create(T value)\r\n self.value=value\r\n end\r\nend\r\n";
 CHECK(root && entry && dep && image && xrtDirCreateAll(root) && writeText(entry,source) && writeText(dep,lib));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error));xide_document* document=xideDocumentsFind(&app.documents,entry);CHECK(document && gotoSymbol(&app,document,"😀盒::<int>") && pump(&app));
 CHECK(key(&app,XUI_KEY_F12,XUI_MOD_SHIFT) && waitReferences(&app));struct xide_reference_view* r=app.languageView->references;
 CHECK(r->usable && xideLanguageViewReferenceCount(&app)==3 && !xideDocumentsFind(&app.documents,dep));
 size_t row=pathRow(r,dep);CHECK(row!=SIZE_MAX);xide_lsp_position expected=r->result->items[row].start;
 CHECK(enterRow(&app,row) && xidePathLexicalEqual(app.activeDocument->path,dep) && selectedStart(app.activeDocument,expected));
 CHECK(gotoSymbol(&app,document,"identity::<int>") && xideActionExecute(&app,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED && waitReferences(&app) && r->usable && xideLanguageViewReferenceCount(&app)==2);
 row=pathRow(r,entry);CHECK(row!=SIZE_MAX && r->result->items[row].start.line==1);expected=r->result->items[row].start;
 CHECK(enterRow(&app,row) && app.activeDocument==document && selectedStart(app.activeDocument,expected) && capture(&app,image));
 {xide_text_file disk={0};CHECK(xideTextFileRead(entry,&disk,&error) && disk.rawLength==strlen(source) && !memcmp(disk.raw,source,strlen(source)));xideTextFileClear(&disk);}
 printf("generic_navigation_view_test passed: %d checks; native generic class/function ShiftF12, standalone imports, UTF16/BOM/CRLF, guarded source jumps and actual pane rendering;render=%s\n",f->checks,image);
cleanup:
 if(began)xgeEnd();
 xideAppDestroy(&app);free(root);free(entry);free(dep);free(image);xgeQuit();return XGE_OK;
}
static DWORD WINAPI genericWorker(void* data)
{fixture* f=data;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang generic navigation verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(genericVerify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv)
{if(argc!=3 || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1],.compiler=argv[2]};wchar_t name[80];swprintf(name,80,L"xlang-generic-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());f.desktop=CreateDesktopW(name,NULL,NULL,0,GENERIC_ALL,NULL);if(!f.desktop)return 2;HANDLE thread=CreateThread(NULL,0,genericWorker,&f,0,NULL);DWORD code=2;if(thread){WaitForSingleObject(thread,INFINITE);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;}
