#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "platform/dpi_proxy.h"
#include <math.h>
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PULSE_MESSAGE (WM_APP+183)
typedef struct fixture {
 xide_app app; const char *directory,*input,*language,*event; char *settings,*image;
 HWND window; WNDPROC original; HANDLE pulse; volatile LONG stopping,phase;
 unsigned long long maxLatency[32],pulseCount[32]; LARGE_INTEGER frequency;
 int started,failed,tick,checks; size_t bytes; xide_document* doc;
} fixture;
static fixture* active;
static double now(fixture* f) {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart*1000.0/f->frequency.QuadPart;}
static void metric(fixture* f,const char* label,double start,int code) {
 PROCESS_MEMORY_COUNTERS_EX memory={0};memory.cb=sizeof(memory);GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&memory,sizeof(memory));
 printf("{\"event\":\"metric\",\"phase\":%d,\"label\":\"%s\",\"ms\":%.3f,\"code\":%d,\"privateBytes\":%llu,\"peakWorkingSet\":%llu}\n",(int)f->phase,label,now(f)-start,code,(unsigned long long)memory.PrivateUsage,(unsigned long long)memory.PeakWorkingSetSize);fflush(stdout);
}
static LRESULT CALLBACK pulseProcedure(HWND window,UINT message,WPARAM w,LPARAM l) {
 fixture* f=active;
 if(f && message==PULSE_MESSAGE) {unsigned long long delay=GetTickCount64()-(unsigned long long)w;int phase=(int)l;if(phase>=0 && phase<32) {if(delay>f->maxLatency[phase])f->maxLatency[phase]=delay;++f->pulseCount[phase];}return 0;}
 return f && f->original ? CallWindowProcW(f->original,window,message,w,l) : DefWindowProcW(window,message,w,l);
}
static DWORD WINAPI pulseWorker(void* data) {
 fixture* f=data;while(!InterlockedCompareExchange(&f->stopping,0,0)) {int phase=InterlockedCompareExchange(&f->phase,0,0);PostMessageW(f->window,PULSE_MESSAGE,(WPARAM)GetTickCount64(),phase);Sleep(10);}return 0;
}
static int key(fixture* f,int key,uint32_t mods) {return xuiInputKeyDownEx(f->app.context,key,mods,NULL)==XUI_OK && xuiInputKeyUpEx(f->app.context,key,0,NULL)==XUI_OK && xuiDispatchPendingEvents(f->app.context)==XUI_OK;}
#define REQUIRE(v) do {++f->checks;if(!(v)) {fprintf(stderr,"large-file probe:%d: %s; frame=%d\n",__LINE__,#v,f->app.frameError);f->failed=1;xgeQuit();return XGE_OK;}} while(0)
typedef struct precision_oracle_t {long double advance;unsigned long long positions;int lastEnd;} precision_oracle_t;
static int precisionOracleCluster(const xui_text_cluster_t* cluster,void* user) {
 precision_oracle_t* o=user;o->advance+=(long double)cluster->fAdvance;++o->positions;o->lastEnd=cluster->iTextEnd;return XUI_OK;
}
static int verifyPrecision(fixture* f,xui_rect_t caret,float sx) {
 xide_app* a=&f->app;char* text=malloc(f->bytes+1);precision_oracle_t oracle={0};xui_text_shape_t shape={0};xui_rect_t origin={0};
 REQUIRE(text);
 REQUIRE(xuiCodeDocumentCopyRange(xuiCodeEditGetDocument(f->doc->editor),0,(int)f->bytes,text,(int)f->bytes+1,NULL)==XUI_OK);
 REQUIRE(xideDpiVisitTextShape(&a->proxy,a->codeFont,text,(int)f->bytes,XUI_TEXT_SHAPE_DEFAULT,precisionOracleCluster,&oracle,&shape,NULL)==XUI_OK);
 free(text);REQUIRE(oracle.positions>0 && oracle.lastEnd==(int)f->bytes);
 float rounded=(float)oracle.advance;long double tolerance=fmaxl(.5L,(long double)nextafterf(rounded,INFINITY)-rounded);
 printf("{\"event\":\"precision-oracle\",\"bytes\":%llu,\"positions\":%llu,\"advance\":%.17g,\"shapeWidth\":%.9g,\"publicFloatTolerance\":%.9g,\"scrollX\":%.9g}\n",(unsigned long long)f->bytes,oracle.positions,(double)oracle.advance,(double)shape.fWidth,(double)tolerance,(double)sx);fflush(stdout);
 REQUIRE(fabsl((long double)shape.fWidth-oracle.advance)<=tolerance);
 xuiTextShapeFree(&shape);
 REQUIRE(xuiCodeEditGetOffsetRect(f->doc->editor,0,&origin)==XUI_OK);
 REQUIRE(fabsl((long double)origin.fX+oracle.advance-caret.fX)<=1.0L);
 for(int at=(int)f->bytes-2;at<(int)f->bytes;++at) {
  xui_rect_t first={0},next={0};xui_code_edit_text_hit_t hit={0};hit.iSize=sizeof(hit);
  REQUIRE(xuiCodeEditGetOffsetRect(f->doc->editor,at,&first)==XUI_OK);
  REQUIRE(xuiCodeEditGetOffsetRect(f->doc->editor,at+1,&next)==XUI_OK);
  REQUIRE(next.fX>first.fX);
  REQUIRE(xuiCodeEditHitTestText(f->doc->editor,(float)first.fX+1,(float)first.fY+2,&hit)==XUI_OK);
  printf("{\"event\":\"precise-tail-hit\",\"offset\":%d,\"hit\":%d,\"clusterStart\":%d,\"clusterEnd\":%d,\"firstX\":%d,\"nextX\":%d}\n",at,hit.iByteOffset,hit.iClusterStart,hit.iClusterEnd,first.fX,next.fX);fflush(stdout);
  REQUIRE(hit.iByteOffset==at && hit.iClusterStart==at && hit.iClusterEnd==at+1);
 }
 return XUI_OK;
}
static int tick(void* data) {
 fixture* f=data;xide_app* a=&f->app;xide_model_error error={0};double start=now(f);int result=0;InterlockedExchange(&f->phase,f->tick);
 printf("{\"event\":\"phase-begin\",\"phase\":%d}\n",f->tick);fflush(stdout);
 if(!f->started) {
  f->started=1;a->hidden=1;a->noSession=1;a->highDpi=1;a->dpiOverride=1.25f;const char* dpi=getenv("XLANG_PROBE_DPI");if(dpi)a->dpiOverride=strtof(dpi,NULL);REQUIRE(a->dpiOverride>=1.0f && a->dpiOverride<=2.0f);a->settingsPath=f->settings;
  REQUIRE(xideAppCreate(a)==XUI_OK);
  xui_interaction_policy_t policy={0};policy.iSize=sizeof(policy);
  REQUIRE(xuiGetInteractionPolicy(a->context,&policy)==XUI_OK);policy.fCaretBlinkSeconds=0;
  REQUIRE(xuiSetInteractionPolicy(a->context,&policy)==XUI_OK);f->window=(HWND)xgePlatformNativeHandle();REQUIRE(f->window);
  SetLastError(0);f->original=(WNDPROC)SetWindowLongPtrW(f->window,GWLP_WNDPROC,(LONG_PTR)pulseProcedure);REQUIRE(f->original);
  f->pulse=CreateThread(NULL,0,pulseWorker,f,0,NULL);REQUIRE(f->pulse);metric(f,"create-workbench",start,0);
 }
 if(f->tick==4) {
  result=xideEditorOpen(a,f->input,&error);metric(f,"open",start,result);if(!result)fprintf(stderr,"open error: %s\n",error.message);REQUIRE(result);
  f->doc=a->activeDocument;REQUIRE(f->doc && !strcmp(f->doc->language,f->language) && !f->doc->dirty);
  REQUIRE((size_t)xuiCodeDocumentGetLength(xuiCodeEditGetDocument(f->doc->editor))==f->bytes);
  const char* option=getenv("XLANG_PROBE_LAYOUT");if(!option)option="default";
  if(!strcmp(option,"all-margins"))REQUIRE(xuiCodeEditSetDefaultMargins(f->doc->editor,1,1,1,1)==XUI_OK);
  else if(!strcmp(option,"minimap"))REQUIRE(xuiCodeEditSetMinimap(f->doc->editor,1,120.0f)==XUI_OK);
  else if(!strcmp(option,"wrap"))REQUIRE(xuiCodeEditSetWordWrap(f->doc->editor,1)==XUI_OK);
  else REQUIRE(!strcmp(option,"default"));
 } else if(f->tick==5 || f->tick==9) {
  REQUIRE(xuiSetFocusWidget(a->context,f->doc->editor)==XUI_OK);result=key(f,XUI_KEY_END,XUI_MOD_CTRL);metric(f,"Ctrl+End",start,result);REQUIRE(result);
  xui_code_selection_t s={0};REQUIRE(xuiCodeSelectionGetState(xuiCodeEditGetSelection(f->doc->editor),&s)==XUI_OK && (size_t)s.iCaretOffset==f->bytes);
 } else if(f->tick==6) {
  result=xuiInputText(a->context,'x');metric(f,"insert-one-character",start,result);REQUIRE(result==XUI_OK);
 } else if(f->tick==7) {
  result=xideActionExecute(a,XIDE_ACTION_EDITOR_UNDO);metric(f,"undo",start,result);REQUIRE(result==XIDE_ACTION_DISPATCHED);
 } else if(f->tick==8 || f->tick==10) {
  result=key(f,XUI_KEY_HOME,XUI_MOD_CTRL);metric(f,"Ctrl+Home",start,result);REQUIRE(result);
 }
 start=now(f);result=xideAppFrame(a);metric(f,f->tick>=11 ? "idle-frame" : "frame",start,result);REQUIRE(result==XGE_OK && !a->frameError);
 if(f->tick==5 || f->tick==8) {
  const char* imageName=f->tick==5?"native-editor-end.png":"native-editor-home.png";
  char* image=xidePathResolve(f->directory,imageName);REQUIRE(image);
  result=xideAppCapture(a,image);free(image);REQUIRE(result==XGE_OK);
  float sx=0,sy=0;xui_rect_t caret={0},exact={0},world=xuiWidgetGetWorldRect(f->doc->editor);
  xui_code_selection_t selection={0};
  REQUIRE(xuiCodeSelectionGetState(xuiCodeEditGetSelection(f->doc->editor),&selection)==XUI_OK && (size_t)selection.iCaretOffset==(f->tick==5?f->bytes:0));
  REQUIRE(xuiCodeEditGetScroll(f->doc->editor,&sx,&sy)==XUI_OK);
  REQUIRE(xuiCodeEditGetOffsetRect(f->doc->editor,selection.iCaretOffset,&caret)==XUI_OK);
  REQUIRE(xuiCodeEditGetTextOffsetRect(f->doc->editor,selection.iCaretOffset,&exact)==XUI_OK);
  printf("{\"event\":\"viewport-position\",\"phase\":%d,\"caretOffset\":%d,\"scrollX\":%.6f,\"scrollY\":%.6f,\"caret\":[%.6f,%.6f,%.6f,%.6f],\"exact\":[%.6f,%.6f,%.6f,%.6f],\"editor\":[%.6f,%.6f,%.6f,%.6f],\"dpi\":%.6f}\n",f->tick,selection.iCaretOffset,sx,sy,(double)caret.fX,(double)caret.fY,(double)caret.fW,(double)caret.fH,(double)exact.fX,(double)exact.fY,(double)exact.fW,(double)exact.fH,(double)world.fX,(double)world.fY,(double)world.fW,(double)world.fH,a->dpiScale);fflush(stdout);
  REQUIRE(selection.iCaretOffset!=0 || (sx==0.0f && sy==0.0f));
  if(f->tick==5)REQUIRE(verifyPrecision(f,caret,sx)==XUI_OK);
  xui_scroll_model_t* model=xuiCodeEditGetScrollModel(f->doc->editor);REQUIRE(model);
  printf("{\"event\":\"viewport-state\",\"phase\":%d,\"viewport\":[%d,%d,%d,%d]}\n",f->tick,model->tViewportRect.fX,model->tViewportRect.fY,model->tViewportRect.fW,model->tViewportRect.fH);fflush(stdout);
  REQUIRE(caret.fX>=world.fX && caret.fX+caret.fW<=world.fX+model->tViewportRect.fW && caret.fY>=world.fY && caret.fY+caret.fH<=world.fY+model->tViewportRect.fH);

 }
 if(f->tick==6)REQUIRE(f->doc->dirty && (size_t)xuiCodeDocumentGetLength(xuiCodeEditGetDocument(f->doc->editor))==f->bytes+1);
 if(f->tick==7)REQUIRE(!f->doc->dirty && (size_t)xuiCodeDocumentGetLength(xuiCodeEditGetDocument(f->doc->editor))==f->bytes);
 if(++f->tick==23) {
  start=now(f);result=xideAppCapture(a,f->image);metric(f,"capture",start,result);REQUIRE(result==XGE_OK);
  printf("{\"event\":\"completed\",\"checks\":%d,\"frames\":%d,\"bytes\":%llu,\"language\":\"%s\",\"lineCount\":%d,\"nativePulses\":[",f->checks,a->frames,(unsigned long long)f->bytes,f->language,xuiCodeDocumentGetLineCount(xuiCodeEditGetDocument(f->doc->editor)));
  for(int i=0;i<32;++i) {printf("%s{\"phase\":%d,\"count\":%llu,\"maxLatencyMs\":%llu}",i ? "," : "",i,f->pulseCount[i],f->maxLatency[i]);}
  puts("]}");fflush(stdout);xgeQuit();
 }
 return XGE_OK;
}
int main(int unusedCount,char** unusedValues) {
 int argc=0;wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&argc);
 if(!wide || argc!=5) {fprintf(stderr,"probe arguments: expected 5 got %d\n",argc);if(wide)LocalFree(wide);return 2;}
 char* argv[5]={0};
 for(int i=0;i<5;++i) {
  int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,NULL,0,NULL,NULL);
  argv[i]=bytes ? malloc((size_t)bytes) : NULL;
  if(!argv[i] || !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],-1,argv[i],bytes,NULL,NULL)) {fputs("probe argument UTF-8 conversion failed\n",stderr);LocalFree(wide);return 2;}
 }
 LocalFree(wide);
 fixture f={.directory=argv[1],.input=argv[2],.language=argv[3],.event=argv[4]};active=&f;
 wchar_t event[128];if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,f.event,-1,event,128))return 2;
 HANDLE ready=OpenEventW(SYNCHRONIZE,FALSE,event);if(!ready) {fprintf(stderr,"probe event open failed: %lu\n",GetLastError());return 2;}DWORD wait=WaitForSingleObject(ready,5000);CloseHandle(ready);if(wait!=WAIT_OBJECT_0)return 2;
 QueryPerformanceFrequency(&f.frequency);f.settings=xidePathResolve(f.directory,"preferences.json");f.image=xidePathResolve(f.directory,"native-editor-large-file.png");
 wchar_t* file=xidePathNativeWide(f.input);HANDLE input=file ? CreateFileW(file,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;free(file);LARGE_INTEGER length={0};
 if(input==INVALID_HANDLE_VALUE || !GetFileSizeEx(input,&length) || length.QuadPart<=0 || length.QuadPart>16*1024*1024) {fprintf(stderr,"probe file open or size failed: %lu; %s\n",GetLastError(),f.input);return 2;}
 CloseHandle(input);f.bytes=(size_t)length.QuadPart;
 xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="xlang IDE large source performance probe";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;
 int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(tick,&f);
 InterlockedExchange(&f.stopping,1);if(f.pulse) {WaitForSingleObject(f.pulse,3000);CloseHandle(f.pulse);}if(f.window && f.original)SetWindowLongPtrW(f.window,GWLP_WNDPROC,(LONG_PTR)f.original);
 xideAppDestroy(&f.app);xgeUnit();free(f.settings);free(f.image);active=NULL;for(int i=0;i<5;++i)free(argv[i]);return result==XGE_OK && f.started && !f.failed && f.tick==23 ? 0 : 1;
}
