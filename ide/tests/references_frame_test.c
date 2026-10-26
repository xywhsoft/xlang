#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "lang/languages.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui/language_view.c"
/* Actual application callbacks,not a polling mock. Timing ends after the
   real input/poll/layout/render submission;full result checks follow it. */
typedef struct fixture {
 const char *root,*compiler;HDESK desktop;xide_app app;xide_text_file file;
 char *entry,*shared,*target,*unused,*manifest;char** members;
 xide_document *source,*unrelated;int started,failed,checks,phase,mode,sample,samples,roots,automatic,offset,digit,revision,frames;
 uint64_t deadline,priorToken;LARGE_INTEGER frequency;double began,peak,total,cpuAt;
} fixture;
static double now(fixture* f){LARGE_INTEGER t;QueryPerformanceCounter(&t);return 1000.0*t.QuadPart/f->frequency.QuadPart;}
static double cpu(void){FILETIME c,e,k,u;if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return -1;ULARGE_INTEGER a={.LowPart=k.dwLowDateTime,.HighPart=k.dwHighDateTime},b={.LowPart=u.dwLowDateTime,.HighPart=u.dwHighDateTime};return (a.QuadPart+b.QuadPart)*.0001;}
static int writeText(const char* path,const char* text){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"references_frame:%d:%s phase=%d mode=%d sample=%d; %s\n",__LINE__,#v,f->phase,f->mode,f->sample,f->app.languageView && f->app.languageView->references ? f->app.languageView->references->message : "");f->failed=1;xgeQuit();return XGE_OK;}}while(0)
static int sourceCaret(fixture* f)
{return xideEditorShow(&f->app,f->source)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(f->source->editor),xuiCodeEditGetDocument(f->source->editor),f->offset,0)==XUI_OK && xuiCodeEditEnsureCaretVisible(f->source->editor)==XUI_OK && xuiSetFocusWidget(f->app.context,f->source->editor)==XUI_OK;}
static int frame(void* user)
{
 fixture* f=user;xide_app* a=&f->app;
 if(!f->started){f->started=1;CHECK(xideAppFrame(a)==XGE_OK);xide_model_error error={0};CHECK(xideWorkspaceCreate(&a->workspaceModel,"引用性能",&error) && xideWorkspaceAddProject(&a->workspaceModel,f->manifest,NULL,&error) && xideWorkspaceViewRefresh(a)==XUI_OK && xideWorkspaceViewSelectProject(a,a->workspaceModel.projects[0].id));CHECK(xideEditorOpen(a,f->entry,&error));f->source=a->activeDocument;CHECK(xideEditorOpen(a,f->unused,&error));f->unrelated=a->activeDocument;CHECK(sourceCaret(f));f->began=now(f);f->cpuAt=cpu();f->peak=f->total=0;f->frames=0;CHECK(xideActionExecute(a,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED);f->priorToken=0;f->phase=1;f->deadline=GetTickCount64()+60000;xgeRenderRequest();return XGE_OK;}
 CHECK(GetTickCount64()<f->deadline);struct xide_reference_view* r=a->languageView->references;
 if(f->phase==2){f->peak=f->total=0;f->frames=0;f->cpuAt=cpu();f->began=now(f);f->priorToken=r->token;CHECK(sourceCaret(f));
  if(f->mode==1){char digit[2]={(char)('1'+(++f->revision%8)),0};f->file.text[f->digit]=digit[0];CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(f->source->editor),f->digit,f->digit+1,digit)==XUI_OK && xideRefreshEditorSyntax(f->source->editor)==XUI_OK);}
  if(f->mode==2){char digit[2]={(char)('0'+(++f->revision%10)),0};xui_code_document other=xuiCodeEditGetDocument(f->unrelated->editor);int length=xuiCodeDocumentGetLength(other);CHECK(length>0 && xuiCodeDocumentReplace(other,length-1,length,digit)==XUI_OK && xideRefreshEditorSyntax(f->unrelated->editor)==XUI_OK);}
  if(f->mode==3){size_t at=SIZE_MAX;for(size_t i=0;r->result && i<r->result->count;++i)if(xidePathLexicalEqual(r->result->items[i].path,f->target)){at=i;break;}CHECK(at!=SIZE_MAX && xideLanguageViewReferenceJump(a,at));}
  else CHECK(xideActionExecute(a,XIDE_ACTION_FIND_REFERENCES)==XIDE_ACTION_DISPATCHED);
  f->phase=3;
 }
 double begin=now(f);CHECK(xideAppFrame(a)==XGE_OK);double end=now(f),elapsed=f->frames ? end-begin : end-f->began;double consumed=cpu();++f->frames;f->total+=elapsed;if(elapsed>f->peak)f->peak=elapsed;
 if((f->phase==1 || f->phase==3) && r->usable && r->result && !r->pending && !r->request && !r->decoding && r->result->token==r->token && r->token!=f->priorToken){
  CHECK(r->result->valid && r->result->count==(size_t)f->roots+1 && a->languageView->autoEnabled==f->automatic);CHECK(f->mode!=3 || xidePathLexicalEqual(a->activeDocument->path,f->target));CHECK(!strcmp(xuiCodeEditGetText(f->source->editor),f->file.text));
  int sourceFound=0,sharedFound=0;for(size_t i=0;i<r->result->count;++i){xide_reference_item* item=r->result->items+i;CHECK(item->document && item->range && item->label && *item->label);if(xidePathLexicalEqual(item->path,f->entry)){int line=0,character=0;CHECK(xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(f->source->editor),f->offset,&line,&character) && item->start.line==line && item->start.character==character);++sourceFound;}else if(xidePathLexicalEqual(item->path,f->shared)){CHECK(item->start.line==0 && item->start.character==12);++sharedFound;}else{CHECK(item->start.line==2 && item->start.character==15);}}
  CHECK(sourceFound==1 && sharedFound==1 && consumed>=f->cpuAt);printf("{\"cold\":%s,\"mode\":%d,\"sample\":%d,\"roots\":%d,\"automaticRequested\":%d,\"automaticEnabled\":%d,\"sourceBytes\":%zu,\"locations\":%zu,\"renderSubmittedMs\":%.6f,\"maxFrameMs\":%.6f,\"totalFrameMs\":%.6f,\"guiCpuMs\":%.6f,\"frames\":%d,\"fullLocationProofs\":true}\n",f->phase==1?"true":"false",f->mode,f->sample,f->roots+1,f->automatic,a->languageView->autoEnabled,strlen(f->file.text),r->result->count,end-f->began,f->peak,f->total,consumed-f->cpuAt,f->frames);fflush(stdout);
  if(f->phase==1){char* image=xidePathResolve(f->root,"cold-references.png");CHECK(image && xideAppCapture(a,image)==XGE_OK);free(image);f->phase=2;}
  else if(++f->sample==f->samples){f->sample=0;if(++f->mode==4)f->phase=4;else f->phase=2;}
  else f->phase=2;
 }
 if(f->phase==4){xide_text_file disk={0};xide_model_error error={0};CHECK(xideTextFileRead(f->entry,&disk,&error) && disk.rawLength==f->file.rawLength && !memcmp(disk.raw,f->file.raw,disk.rawLength));xideTextFileClear(&disk);CHECK(xideActionExecute(a,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !xideLanguageViewReferenceCount(a));printf("references_frame_test passed: %d assertions;real input/poll/layout/render,complete source positions/proofs,hot/local/unrelated512KiB/guarded jump,%d samples per mode;original disk preserved\n",f->checks,f->samples);xgeQuit();}
 xgeRenderRequest();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t desc={0};desc.iWidth=1280;desc.iHeight=800;desc.sTitle="References frame verification";desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI|XGE_INIT_VSYNC|XGE_INIT_ON_DEMAND;desc.iRunMode=XGE_RUN_GAME_LOOP;desc.iTargetFPS=30;int result=xgeInit(&desc);if(result==XGE_OK)result=xgeRun(frame,f);xideAppDestroy(&f->app);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
static int runMain(int argc,char** argv)
{
 if(argc!=7 || !xrtDirCreateAll(argv[1]))return 2;
 fixture data={.root=argv[1],.compiler=argv[2],.samples=atoi(argv[4]),.roots=atoi(argv[5]),.automatic=atoi(argv[6])};fixture* f=&data;xide_model_error error={0};int result=2;
 if(f->samples<2 || f->samples>120 || f->roots<2 || f->roots>256 || (f->automatic!=0 && f->automatic!=1) || !QueryPerformanceFrequency(&f->frequency) || !xideTextFileRead(argv[3],&f->file,&error))return 2;
 char* call=strstr(f->file.text,"shared.compute(7)");if(!call)goto done;f->offset=(int)(call-f->file.text)+7;f->digit=f->offset+8;f->entry=xidePathResolve(f->root,"main.xl");f->shared=xidePathResolve(f->root,"shared.xl");f->unused=xidePathResolve(f->root,"unused.xl");f->manifest=xidePathResolve(f->root,"project.xlp");f->members=calloc((size_t)f->roots,sizeof(*f->members));if(!f->entry || !f->shared || !f->unused || !f->manifest || !f->members || !xrtFileWriteAtomic(f->entry,(xbytesview){f->file.raw,f->file.rawLength}) || !writeText(f->shared,"export func compute(int value) as int\n return value+1\nend\n"))goto done;
 {char* unused=malloc(512u*1024u+1);if(!unused)goto done;memset(unused,'x',512u*1024u);unused[0]='/';unused[1]='/';unused[512u*1024u-1]='0';unused[512u*1024u]=0;int okay=writeText(f->unused,unused);free(unused);if(!okay)goto done;}
 {xvalue* project=xrtJsonParse(sv("{\"format\":\"xlang-project-1\",\"entry\":\"main.xl\",\"ide\":{\"name\":\"引用性能\",\"files\":[{\"id\":\"shared\",\"path\":\"shared.xl\",\"kind\":\"xlang\"}],\"activeConfiguration\":\"debug\",\"configurations\":[{\"id\":\"debug\",\"output\":\"build/app.exe\"}]}}"));if(!project)goto done;xvalue* files=get(get(project,"ide"),"files");int okay=1;
  for(int i=1;okay && i<f->roots;++i){char name[48];snprintf(name,sizeof(name),"member-%04d.xl",i);f->members[i]=xidePathResolve(f->root,name);xvalue* item=xrtValueObject();okay=f->members[i] && writeText(f->members[i],"import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n") && item && put(item,"id",xrtValueString(sv(name))) && put(item,"path",xrtValueString(sv(name))) && put(item,"kind",xrtValueString(sv("xlang"))) && xrtValueArrayAppend(files,item);xrtValueRelease(item);}
  size_t size=0;char* json=okay ? xrtJsonStringify(project,false,&size) : NULL;okay=json && xrtFileWriteAtomic(f->manifest,(xbytesview){(const unsigned char*)json,size});xrtFree(json);xrtValueRelease(project);if(!okay)goto done;
 }
 f->target=f->members[1];f->app.hidden=1;f->app.noSession=1;f->app.highDpi=1;f->app.compilerPath=f->compiler;f->app.disableAutomaticDiagnostics=!f->automatic;
 {wchar_t name[96];swprintf(name,96,L"ReferencesFrame-%lu-%llu",GetCurrentProcessId(),(unsigned long long)GetTickCount64());f->desktop=CreateDesktopW(name,NULL,NULL,0,GENERIC_ALL,NULL);if(!f->desktop)goto done;HANDLE thread=CreateThread(NULL,0,worker,f,0,NULL);DWORD code=1;if(thread){if(WaitForSingleObject(thread,600000)!=WAIT_OBJECT_0){fputs("references frame timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f->desktop);result=(int)code;}
done:
 if(f->members)for(int i=1;i<f->roots;++i)free(f->members[i]);
 free(f->members);free(f->entry);free(f->shared);free(f->unused);free(f->manifest);xideTextFileClear(&f->file);return result;
}
int main(void)
{int count=0;wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&count);if(!wide)return 2;char** args=calloc((size_t)count,sizeof(*args));int okay=args!=NULL;for(int i=0;okay && i<count;++i){int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,NULL,0,NULL,NULL);args[i]=size ? malloc((size_t)size) : NULL;if(!args[i] || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,args[i],size,NULL,NULL))okay=0;}LocalFree(wide);int result=okay ? runMain(count,args) : 2;if(args){for(int i=0;i<count;++i)free(args[i]);free(args);}return result;}
