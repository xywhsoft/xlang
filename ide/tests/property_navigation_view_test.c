#define main referencesOriginalMain
#include "references_view_test.c"
#undef main
#undef CHECK
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"property_navigation_view_test:%d:%s; %s; %s\n",__LINE__,#v,error.message,xideLanguageViewMessage(&app));f->failed=1;goto cleanup;}}while(0)
static int propertyVerify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;
 char *root=xidePathResolve(f->directory,"properties"),*entry=xidePathResolve(root,"入口😀.xl"),*dep=xidePathResolve(root,"dep.xl"),*image=xidePathResolve(root,"property-references.png");
 const char* source="\xef\xbb\xbf" "import dep\r\nfunc main(...) as int\r\n auto box=dep.Box()\r\n box.尺寸😀=1\r\n box.尺寸😀[2]=3\r\n return box.尺寸😀+box.尺寸😀[1]\r\nend\r\n";
 const char* lib="\xef\xbb\xbf" "export class Box\r\n int data\r\n get 尺寸😀() as int\r\n return self.data\r\n end\r\n get 尺寸😀(int index) as int\r\n return self.data+index\r\n end\r\n let 尺寸😀(int value)\r\n self.data=value\r\n end\r\n let 尺寸😀(int index,int value)\r\n self.data=value+index\r\n end\r\nend\r\n";
 CHECK(root && entry && dep && image && xrtDirCreateAll(root) && writeText(entry,source) && writeText(dep,lib));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error));xide_document* document=xideDocumentsFind(&app.documents,entry);CHECK(document);
 const char* needles[]={"尺寸😀=1","尺寸😀[2]=3","尺寸😀+box","尺寸😀[1]"};const int lines[]={8,11,2,5};
 for(size_t i=0;i<4;++i){
  CHECK(gotoSymbol(&app,document,needles[i]) && pump(&app) && key(&app,XUI_KEY_F12,XUI_MOD_SHIFT) && waitReferences(&app));struct xide_reference_view* r=app.languageView->references;
  CHECK(r->usable && xideLanguageViewReferenceCount(&app)==2);size_t row=pathRow(r,dep);CHECK(row!=SIZE_MAX && r->result->items[row].start.line==lines[i]);xide_lsp_position expected=r->result->items[row].start;
  CHECK(enterRow(&app,row) && xidePathLexicalEqual(app.activeDocument->path,dep) && selectedStart(app.activeDocument,expected));
 }
 CHECK(capture(&app,image));printf("property_navigation_view_test passed: %d checks; native simple/indexed getter/setter ShiftF12, selected-accessor reference sets, UTF16/BOM/CRLF and guarded source jumps;render=%s\n",f->checks,image);
cleanup:
 if(began)xgeEnd();
 xideAppDestroy(&app);free(root);free(entry);free(dep);free(image);xgeQuit();return XGE_OK;
}
static DWORD WINAPI propertyWorker(void* data)
{fixture* f=data;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang property navigation verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(propertyVerify,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv)
{if(argc!=3 || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1],.compiler=argv[2]};wchar_t name[80];swprintf(name,80,L"xlang-property-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());f.desktop=CreateDesktopW(name,NULL,NULL,0,GENERIC_ALL,NULL);if(!f.desktop)return 2;HANDLE thread=CreateThread(NULL,0,propertyWorker,&f,0,NULL);DWORD code=2;if(thread){WaitForSingleObject(thread,INFINITE);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;}
