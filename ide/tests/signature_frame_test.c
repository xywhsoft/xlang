#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "lang/languages.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Inspect adopted identity, but measure actual input/poll/layout/render frames. */
#include "../src/ui/completion_view.c"
typedef struct fixture {
 const char *root,*compiler;HDESK desktop;xide_app app;xide_document *document,*otherDocument;
 xide_text_file file;char* entry;int failed,started,phase,mode,modeIndex,reverse,sample,samples,revision,offset,digit,frames,checks,priorRequest,helpers,cacheHits,indexBuilds,indexHits;
 LARGE_INTEGER frequency;uint64_t deadline;double inputAt,inputMs,peakFrame,totalFrame,cpuAt,coldAt;
 char* other;int scope,nativeBuilds,scopeHits;
} fixture;
static double now(fixture* f){LARGE_INTEGER t;QueryPerformanceCounter(&t);return 1000.0*t.QuadPart/f->frequency.QuadPart;}
static double cpu(void){FILETIME c,e,k,u;if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return -1;ULARGE_INTEGER a={.LowPart=k.dwLowDateTime,.HighPart=k.dwHighDateTime},b={.LowPart=u.dwLowDateTime,.HighPart=u.dwHighDateTime};return (a.QuadPart+b.QuadPart)*.0001;}
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"signature_frame:%d %s phase=%d mode=%d sample=%d\n",__LINE__,#v,f->phase,f->mode,f->sample);f->failed=1;xgeQuit();return XGE_OK;}}while(0)
static int key(xui_context c,int k,uint32_t mods){return xuiInputKeyDownEx(c,k,mods,NULL)==XUI_OK && xuiInputKeyUpEx(c,k,mods,NULL)==XUI_OK && xuiDispatchPendingEvents(c)==XUI_OK;}
static int frame(void* user)
{
 fixture* f=user;xide_app* a=&f->app;
 if(!f->started){f->started=1;CHECK(xideAppFrame(a)==XGE_OK);xide_model_error error={0};
  if(f->scope){CHECK(xideEditorOpen(a,f->entry,&error));CHECK(xideEditorOpen(a,f->other,&error));f->otherDocument=a->activeDocument;}
  CHECK(xideEditorOpen(a,f->entry,&error));f->document=a->activeDocument;
  CHECK(!strcmp(xuiCodeEditGetText(f->document->editor),f->file.text));
  CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(f->document->editor),xuiCodeEditGetDocument(f->document->editor),f->offset,0)==XUI_OK && xuiCodeEditEnsureCaretVisible(f->document->editor)==XUI_OK);
  f->coldAt=now(f);CHECK(key(a->context,XUI_KEY_SPACE,XUI_MOD_CTRL|XUI_MOD_SHIFT));f->phase=1;f->deadline=GetTickCount64()+30000;xgeRenderRequest();return XGE_OK;
 }
 CHECK(GetTickCount64()<f->deadline);xide_completion_view* v=a->completionView;signature_view* s=&v->signature;xui_widget editor=f->document->editor;xui_code_document document=xuiCodeEditGetDocument(editor);
 if(f->phase==2){
  f->mode=f->reverse ? (f->scope?4:3)-f->modeIndex : f->modeIndex;
  if(f->mode!=3){cvdismiss(v);svdismiss(v);}
  if(f->mode<2){char digit[2]={(char)('1'+(++f->revision%8)),0};f->file.text[f->digit]=digit[0];CHECK(xuiCodeDocumentReplace(document,f->digit,f->digit+1,digit)==XUI_OK);
   if(!f->mode)CHECK(xuiCodeDocumentReplace(document,f->offset-1,f->offset,"")==XUI_OK);
   CHECK(xideRefreshEditorSyntax(editor)==XUI_OK);
  }
  if(f->mode!=3){CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(editor),document,f->mode==0 ? f->offset-1 : f->offset,0)==XUI_OK && xuiSetFocusWidget(a->context,editor)==XUI_OK);}
  f->priorRequest=s->request;f->frames=0;f->peakFrame=f->totalFrame=0;f->cpuAt=cpu();f->inputAt=now(f);
  if(f->mode==4){char body[120];snprintf(body,sizeof(body),"// unrelated input %u 中文😀\n",++f->revision);xui_widget other=f->otherDocument->editor;CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(other),0,(int)strlen(xuiCodeEditGetText(other)),body)==XUI_OK && xideRefreshEditorSyntax(other)==XUI_OK);CHECK(key(a->context,XUI_KEY_SPACE,XUI_MOD_CTRL|XUI_MOD_SHIFT));}
  else if(!f->mode){CHECK(xuiInputSetModifiers(a->context,0)==XUI_OK && xuiInputTextEx(a->context,',',NULL)==XUI_OK && xuiDispatchPendingEvents(a->context)==XUI_OK);}
  else if(f->mode==3){CHECK(key(a->context,f->sample%2 ? XUI_KEY_LEFT : XUI_KEY_RIGHT,0));}
  else CHECK(key(a->context,XUI_KEY_SPACE,XUI_MOD_CTRL|XUI_MOD_SHIFT));
  f->inputMs=now(f)-f->inputAt;f->phase=3;f->deadline=GetTickCount64()+30000;
 }
 double start=now(f);CHECK(xideAppFrame(a)==XGE_OK);double end=now(f),elapsed=end-start;
 if(f->phase==3){if(!f->frames)elapsed=end-f->inputAt;++f->frames;f->totalFrame+=elapsed;if(elapsed>f->peakFrame)f->peakFrame=elapsed;}
 if((f->phase==1 || f->phase==3) && s->ready && s->result && xideSignatureViewIsOpen(a) && (f->phase==1 || s->request!=f->priorRequest)){
  CHECK(s->result->valid && s->result->count==2 && s->result->items[s->selected].activeParameter==1 && strstr(s->result->items[s->selected].label,"int left"));
  CHECK(s->document==f->document && s->id==f->document->id && s->version==xuiCodeDocumentGetVersion(document) && s->epoch==v->inputEpoch && !strcmp(xuiCodeEditGetText(editor),f->file.text));
  const xide_signature_item* item=s->result->items+s->selected;xui_code_selection_t selected={0};CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(s->preview),&selected)==XUI_OK && selected.iAnchorOffset==(int)item->parameters[1].start && selected.iCaretOffset==(int)item->parameters[1].end && !strcmp(xuiCodeEditGetText(s->preview),item->label) && xuiGetFocusWidget(a->context)==editor);
  const xvalue* analysis=cvget(cvget(s->result->message,"result"),"analysis");int cacheHits=0,indexBuilds=0,indexHits=0,reused=0,builds=0,scopeHits=0;int scoped=cvget(analysis,"inputScopeHits")!=NULL;
  CHECK(cvnumber(cvget(analysis,"cacheHits"),&cacheHits) && cvnumber(cvget(analysis,"indexBuilds"),&indexBuilds) && cvnumber(cvget(analysis,"indexHits"),&indexHits) && cvnumber(cvget(analysis,"reused"),&reused) && indexBuilds>0);
  CHECK(cvnumber(cvget(analysis,"builds"),&builds) && (!scoped || cvnumber(cvget(analysis,"inputScopeHits"),&scopeHits)));
  if(f->phase==3){
   if(f->mode<2)CHECK(reused>=f->helpers && indexBuilds>f->indexBuilds);
   if(f->mode==2 && f->sample)CHECK(cacheHits>f->cacheHits && indexBuilds==f->indexBuilds);
   if(f->mode==3)CHECK(indexBuilds==f->indexBuilds && indexHits>f->indexHits);
   if(f->mode==4){CHECK(indexBuilds==f->indexBuilds);if(scoped)CHECK(scopeHits>f->scopeHits && builds==f->nativeBuilds && cacheHits>f->cacheHits);else CHECK(builds>f->nativeBuilds && reused>=f->helpers);}
  }
  f->cacheHits=cacheHits;f->indexBuilds=indexBuilds;f->indexHits=indexHits;f->nativeBuilds=builds;f->scopeHits=scopeHits;
  if(f->phase==1){printf("{\"coldMs\":%.6f,\"bytes\":%zu}\n",end-f->coldAt,strlen(f->file.text));f->phase=2;xgeRenderRequest();}
  else{double consumed=cpu();CHECK(consumed>=f->cpuAt && f->cpuAt>=0);printf("{\"mode\":%d,\"sample\":%d,\"bytes\":%zu,\"documentVersion\":%u,\"inputMs\":%.6f,\"renderSubmittedMs\":%.6f,\"maxFrameMs\":%.6f,\"totalFrameMs\":%.6f,\"guiCpuMs\":%.6f,\"frames\":%d,\"cacheHits\":%d,\"indexBuilds\":%d,\"indexHits\":%d,\"semanticReused\":%d,\"builds\":%d,\"inputScopeHits\":%d,\"inputScopeSupported\":%d}\n",f->mode,f->sample,strlen(f->file.text),s->version,f->inputMs,end-f->inputAt,f->peakFrame,f->totalFrame,consumed-f->cpuAt,f->frames,cacheHits,indexBuilds,indexHits,reused,builds,scopeHits,scoped);fflush(stdout);
   if(!f->sample){char name[60];snprintf(name,sizeof(name),"mode-%d.png",f->mode);char* path=xidePathResolve(f->root,name);CHECK(path && xideAppCapture(a,path)==XGE_OK);free(path);}
   if(++f->sample==f->samples){f->sample=0;if(++f->modeIndex==(f->scope?5:4)){CHECK(key(a->context,XUI_KEY_ESCAPE,0));f->phase=4;}else f->phase=2;}else f->phase=2;xgeRenderRequest();
  }
 }
 if(f->phase==4){CHECK(xideAppFrame(a)==XGE_OK && !s->wanted && !xideSignatureViewIsOpen(a));xide_text_file disk={0};xide_model_error error={0};CHECK(xideTextFileRead(f->entry,&disk,&error) && disk.rawLength==f->file.rawLength && !memcmp(disk.raw,f->file.raw,disk.rawLength));xideTextFileClear(&disk);if(f->scope){CHECK(xideTextFileRead(f->other,&disk,&error) && !strcmp(disk.text,"// unrelated0\n"));xideTextFileClear(&disk);}printf("signature_frame_test passed: %d assertions;actual native labels/active parameter/version/source/selection/render submission and Escape;%dmodes/%dsamples;no disk changes\n",f->checks,f->scope?5:4,f->samples);xgeQuit();}
 return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="Signature frame verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI|XGE_INIT_VSYNC|XGE_INIT_ON_DEMAND;d.iRunMode=XGE_RUN_GAME_LOOP;d.iTargetFPS=30;int r=xgeInit(&d);if(r==XGE_OK)r=xgeRun(frame,f);xideAppDestroy(&f->app);xgeUnit();return r==XGE_OK && f->started && !f->failed ? 0 : 1;}
static int runMain(int argc,char** argv)
{
 if((argc!=6 && (argc!=7 || strcmp(argv[6],"scope"))) || !xrtDirCreateAll(argv[1]))return 2;
 fixture f={.root=argv[1],.compiler=argv[2],.samples=atoi(argv[4]),.reverse=atoi(argv[5]),.scope=argc==7};xide_model_error error={0};
 if(f.samples<2 || f.samples>120 || (f.reverse!=0 && f.reverse!=1) || !QueryPerformanceFrequency(&f.frequency) || !xideTextFileRead(argv[3],&f.file,&error))return 2;
 char* call=strstr(f.file.text,"return choose(7, \"hello\")");if(!call)return 2;f.offset=(int)(call-f.file.text)+16;f.digit=f.offset-2;
 for(const char* p=f.file.text;(p=strstr(p,"func work_"))!=NULL;++p)++f.helpers;
 f.entry=xidePathResolve(f.root,"frame-signature.xl");if(!f.entry || !xrtFileWriteAtomic(f.entry,(xbytesview){f.file.raw,f.file.rawLength}))return 1;
 if(f.scope){f.other=xidePathResolve(f.root,"unrelated.xl");if(!f.other || !xrtFileWriteAtomic(f.other,(xbytesview){(const unsigned char*)"// unrelated0\n",14}))return 1;}
 f.app.hidden=1;f.app.noSession=1;f.app.highDpi=1;f.app.disableAutomaticDiagnostics=1;f.app.compilerPath=f.compiler;
 wchar_t name[100];swprintf(name,100,L"SignatureFrame-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
 HANDLE t=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(t){if(WaitForSingleObject(t,600000)!=WAIT_OBJECT_0){fputs("signature frame timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(t,&code);CloseHandle(t);}CloseDesktop(f.desktop);free(f.entry);free(f.other);xideTextFileClear(&f.file);return (int)code;
}

/* Keep the measurement tool usable with UTF-8 workspace/output paths even
   when the Windows CRT command-line code page differs from UTF-8. */
int main(void)
{
 int count=0;wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&count);if(!wide)return 2;
 char** args=calloc((size_t)count,sizeof(*args));int okay=args!=NULL;
 for(int i=0;okay && i<count;++i){int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,NULL,0,NULL,NULL);args[i]=n ? malloc((size_t)n) : NULL;if(!args[i] || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,args[i],n,NULL,NULL))okay=0;}
 LocalFree(wide);int result=okay ? runMain(count,args) : 2;
 if(args){for(int i=0;i<count;++i)free(args[i]);free(args);}return result;
}
