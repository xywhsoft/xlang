#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "lang/languages.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Include the real controller only to inspect exact adopted identity/ranges.
 * Every measured input, poll, layout and draw uses the real application frame. */
#include "../src/ui/completion_view.c"
typedef struct fixture {
 const char *root,*compiler,*expectedName;HDESK desktop;xide_app app;xide_document* document;
 int failed,started,phase,sample,mode,modeIndex,samples,reverse,offset,frames,checks,revision;uint64_t deadline;
 LARGE_INTEGER frequency;double inputAt,inputMs,peakFrame,totalFrame,lastCallback,peakGap,cpuAt;
 char* path;char* source;
} fixture;
static double now(fixture* f) {LARGE_INTEGER t;QueryPerformanceCounter(&t);return 1000.0*(double)t.QuadPart/f->frequency.QuadPart;}
#define CHECK(e) do{++f->checks;if(!(e)){fprintf(stderr,"frame_probe:%d %s phase=%d mode=%d sample=%d wanted=%d pending=%d ready=%d\n",__LINE__,#e,f->phase,f->mode,f->sample,f->app.completionView?f->app.completionView->wanted:0,f->app.completionView?f->app.completionView->pending:0,f->app.completionView?f->app.completionView->ready:0);f->failed=1;xgeQuit();return XGE_OK;}}while(0)
static double cpuNow(void)
{ FILETIME created,exited,kernel,user;if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))return -1;ULARGE_INTEGER k={.LowPart=kernel.dwLowDateTime,.HighPart=kernel.dwHighDateTime},u={.LowPart=user.dwLowDateTime,.HighPart=user.dwHighDateTime};return (double)(k.QuadPart+u.QuadPart)*.0001; }
static int has(xide_completion_view* v,const char* name)
{for(int i=0;i<v->count;++i)if(!strcmp(v->items[i].sLabel,name))return 1;return 0;}
static int frame(void* user)
{
 fixture* f=user;xide_app* app=&f->app;double callbackAt=now(f),gap=f->lastCallback ? callbackAt-f->lastCallback : 0;f->lastCallback=callbackAt;
 if(!f->started) {f->started=1;CHECK(xideAppFrame(app)==XGE_OK);xide_model_error error={0};CHECK(xideEditorOpen(app,f->path,&error));f->document=app->activeDocument;
  xui_code_document document=xuiCodeEditGetDocument(f->document->editor);
  CHECK(xuiCodeDocumentReplace(document,0,xuiCodeDocumentGetLength(document),f->source)==XUI_OK && xideRefreshEditorSyntax(f->document->editor)==XUI_OK);
  CHECK(xuiCodeDocumentReplace(document,f->offset,f->offset+1,"")==XUI_OK && xideRefreshEditorSyntax(f->document->editor)==XUI_OK);
  CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(f->document->editor),document,f->offset,0)==XUI_OK && xuiSetFocusWidget(app->context,f->document->editor)==XUI_OK);
  /* Cold initialization must start from ordinary typing, without an explicit
     completion request. This warmup is outside the measured samples. */
  CHECK(xuiInputTextEx(app->context,'R',NULL)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK);
  f->phase=1;f->deadline=GetTickCount64()+30000;xgeRenderRequest();return XGE_OK;
 }
 CHECK(GetTickCount64()<f->deadline);
 xide_completion_view* v=app->completionView;xui_widget editor=f->document->editor;xui_code_document document=xuiCodeEditGetDocument(editor);
 if(f->phase==2) {
  cvdismiss(v);f->expectedName=(++f->revision&1) ? "zzRecent" : "zzResult";
  char* declaration=strstr(f->source,"int zzR");CHECK(declaration);int nameAt=(int)(declaration-f->source)+4;
  memcpy(f->source+nameAt,f->expectedName,8);CHECK(xuiCodeDocumentReplace(document,nameAt,nameAt+8,f->expectedName)==XUI_OK);
  CHECK(xuiCodeDocumentReplace(document,f->offset,f->offset+1,"")==XUI_OK && xideRefreshEditorSyntax(editor)==XUI_OK);
  CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(editor),document,f->offset,0)==XUI_OK && xuiSetFocusWidget(app->context,editor)==XUI_OK);
  f->phase=3;xgeRenderRequest();
 } else if(f->phase==3) {
  f->frames=0;f->peakFrame=f->totalFrame=f->peakGap=0;f->cpuAt=cpuNow();f->inputAt=now(f);CHECK(xuiInputTextEx(app->context,'R',NULL)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK);f->inputMs=now(f)-f->inputAt;
  if(f->mode) {CHECK(xuiInputKeyDownEx(app->context,XUI_KEY_SPACE,XUI_MOD_CTRL,NULL)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK && xuiInputKeyUpEx(app->context,XUI_KEY_SPACE,XUI_MOD_CTRL,NULL)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK);CHECK(v->wanted && !v->repeatEditor && v->due<=GetTickCount64());}
  f->phase=4;f->deadline=GetTickCount64()+30000;
 }
 if(f->phase==5){CHECK(xuiInputKeyDownEx(app->context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,XUI_KEY_ESCAPE,0,NULL)==XUI_OK);f->phase=6;f->deadline=GetTickCount64()+30000;}
 double before=now(f);CHECK(xideAppFrame(app)==XGE_OK);double after=now(f),elapsed=after-before;
 if(f->phase==4){if(!f->frames)elapsed=after-f->inputAt;else if(gap>f->peakGap)f->peakGap=gap;++f->frames;f->totalFrame+=elapsed;if(elapsed>f->peakFrame)f->peakFrame=elapsed;}
 if(f->phase==1 && v->ready) {CHECK(v->result && has(v,"zzResult"));f->phase=2;xgeRenderRequest();}
 else if(f->phase==4 && v->ready && v->result && has(v,f->expectedName) && xuiCodeEditIsCompletionOpen(editor)) {
  CHECK(v->nativeBackend && v->document==f->document && v->id==f->document->id && v->count==1 && v->version==xuiCodeDocumentGetVersion(document) && v->start==f->offset-2 && v->end==f->offset+6 && !strcmp(xuiCodeEditGetText(editor),f->source));
  double cpu=cpuNow();CHECK(cpu>=f->cpuAt && f->cpuAt>=0);
  printf("{\"mode\":%d,\"sample\":%d,\"bytes\":%zu,\"candidate\":\"%s\",\"documentVersion\":%u,\"dpiScale\":%.6f,\"inputMs\":%.6f,\"renderSubmittedMs\":%.6f,\"maxFrameMs\":%.6f,\"totalFrameMs\":%.6f,\"maxCallbackGapMs\":%.6f,\"guiCpuMs\":%.6f,\"frames\":%d}\n",f->mode,f->sample,strlen(f->source),f->expectedName,v->version,app->dpiScale,f->inputMs,after-f->inputAt,f->peakFrame,f->totalFrame,f->peakGap,cpu-f->cpuAt,f->frames);fflush(stdout);
  if(f->sample==0){char leaf[50];snprintf(leaf,sizeof(leaf),"mode-%d.png",f->mode);char* path=xidePathResolve(f->root,leaf);CHECK(path && xideAppCapture(app,path)==XGE_OK);free(path);}
  if(++f->sample==f->samples){f->sample=0;if(++f->modeIndex==2){f->phase=5;xgeRenderRequest();return XGE_OK;}f->mode=f->reverse ? 0 : 1;}
  f->phase=2;xgeRenderRequest();
 }
 if(f->phase==6){CHECK(!v->wanted && !v->result && !xuiCodeEditIsCompletionOpen(editor));printf("completion_frame_test passed: %d native assertions; actual app frame, ordinary typing and Ctrl+Space, exact native candidate/version/identity/whole-token/source, real render submission and cancel; samples=%d per mode.\n",f->checks,f->samples);xgeQuit();}
 return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
 fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;
 xge_desc_t desc={0};desc.iWidth=1280;desc.iHeight=800;desc.sTitle="Completion frame probe";desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI|XGE_INIT_VSYNC|XGE_INIT_ON_DEMAND;desc.iRunMode=XGE_RUN_GAME_LOOP;desc.iTargetFPS=30;
 int result=xgeInit(&desc);if(result==XGE_OK)result=xgeRun(frame,f);xideAppDestroy(&f->app);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
static int runMain(int argc,char** argv)
{
 if(argc<3 || argc>6 || !xrtDirCreateAll(argv[1]))return 2;
 fixture f={.root=argv[1],.compiler=argv[2],.samples=argc>3 ? atoi(argv[3]) : 3,.reverse=argc>5 ? atoi(argv[5]) : 0};size_t bytes=argc>4 ? (size_t)strtoull(argv[4],NULL,10) : 8192;
 if(f.samples<1 || f.samples>120 || bytes<128 || bytes>7u*1024u*1024u || (f.reverse!=0 && f.reverse!=1))return 2;
 f.mode=f.reverse ? 1 : 0;
 if(!QueryPerformanceFrequency(&f.frequency))return 1;
 f.path=xidePathResolve(f.root,"frame-probe.xl");xstrbuf source;xrtStrBufInit(&source);
 const char* body="func main(...) as int\n int zzResult = 7\n return zzROther\nend\n";
 while(source.Size+strlen(body)+34<bytes)if(!xrtStrBufAppend(&source,xrtStrView("// 0123456789abcdef0123456789abcde\n")))return 1;
 if(!xrtStrBufAppend(&source,xrtStrView(body)))return 1;
 f.source=source.Data;f.offset=(int)(strstr(source.Data,"return zzR")-source.Data)+9;
 if(!f.path || !xrtFileWriteAtomic(f.path,(xbytesview){(const unsigned char*)body,strlen(body)}))return 1;
 f.app.hidden=1;f.app.noSession=1;f.app.highDpi=1;f.app.compilerPath=f.compiler;
 wchar_t name[80];swprintf(name,80,L"CompletionFrame-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread){if(WaitForSingleObject(thread,600000)!=WAIT_OBJECT_0){fputs("completion frame test timed out\n",stderr);ExitProcess(1);}GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);free(f.path);xrtStrBufFree(&source);return (int)code;
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
