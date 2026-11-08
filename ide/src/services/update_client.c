/* The independent updater owns network/verification, separately from XGE. */
#include "services/update_client.h"
#include "app/xide_app.h"
#include "platform/startup_alias.h"
#include "services/ui_font.h"
#include "ui/catalog_view.h"
#include "ui/association_view.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct update_launch {volatile LONG references,done,cancelled;HANDLE thread,process;int cached;DWORD error;wchar_t root[32768],exe[32768],log[32768];} update_launch;
struct xide_update {update_launch* launch;HANDLE check;DWORD started;int initialized,manual,phase,ready,pendingInstall,transition,mandatory,optional;wchar_t log[32768];xui_msgbox box;};
static int locations(wchar_t root[32768],wchar_t exe[32768]) {
    wchar_t binary[32768];if(!GetModuleFileNameW(NULL,binary,32768))return 0;
    const wchar_t* resource=xideStartupResourceModule(binary);if(wcslen(resource)>32000)return 0;wcscpy(root,resource);
    wchar_t* slash=wcsrchr(root,L'\\');if(!slash)slash=wcsrchr(root,L'/');if(!slash)return 0;*slash=0;
    swprintf(exe,32768,L"%ls\\update.exe",root);return GetFileAttributesW(exe)!=INVALID_FILE_ATTRIBUTES;
}
static struct xide_update* state(xide_app* app){
    if(!app->updater){app->updater=calloc(1,sizeof(*app->updater));if(app->updater){
        bool enabled=true;xvalue* update=app->preferences.source?xrtValueObjectGet(app->preferences.source,(xstrview){"update",6}):NULL;
        xvalue* option=update?xrtValueObjectGet(update,(xstrview){"enabled",7}):NULL;if(option)xrtValueGetBool(option,&enabled);app->updater->optional=enabled;
    }}return app->updater;
}
static void launchRelease(update_launch* j)
{
    if(!j || InterlockedDecrement(&j->references))return;
    if(j->process){TerminateProcess(j->process,1);CloseHandle(j->process);}
    if(*j->log)DeleteFileW(j->log);
    if(j->thread)CloseHandle(j->thread);
    free(j);
}
static DWORD WINAPI launchWorker(void* raw)
{
    update_launch* j=raw;wchar_t temp[32768];HANDLE input=INVALID_HANDLE_VALUE,log=INVALID_HANDLE_VALUE;wchar_t* command=NULL;
    if(InterlockedCompareExchange(&j->cancelled,0,0))goto done;
    if(GetFileAttributesW(j->exe)==INVALID_FILE_ATTRIBUTES || !GetTempPathW(32768,temp) || !GetTempFileNameW(temp,L"xlu",0,j->log)){j->error=GetLastError();goto done;}
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};log=CreateFileW(j->log,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);
    command=calloc(65536,sizeof(wchar_t));if(!command || log==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE){j->error=command?GetLastError():ERROR_NOT_ENOUGH_MEMORY;goto done;}
    swprintf(command,65536,L"\"%ls\" --root \"%ls\" %ls --no-launch --plain --staged --timeout 10",j->exe,j->root,j->cached?L"--cached-policy":L"--check-version");
    if(InterlockedCompareExchange(&j->cancelled,0,0))goto done;
    STARTUPINFOW si={0};PROCESS_INFORMATION pi={0};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=input;si.hStdOutput=log;si.hStdError=log;
    if(CreateProcessW(j->exe,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,j->root,&si,&pi)){CloseHandle(pi.hThread);j->process=pi.hProcess;}else j->error=GetLastError();
done:
    free(command);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);InterlockedExchange(&j->done,1);launchRelease(j);return 0;
}
static int check(xide_app* app,int manual,int cached)
{
    struct xide_update* u=state(app);if(!u || u->check || u->launch)return 0;
    update_launch* j=calloc(1,sizeof(*j));if(!j)return 0;wchar_t module[32768];if(!GetModuleFileNameW(NULL,module,32768)){free(j);return 0;}
    const wchar_t* resource=xideStartupResourceModule(module);if(wcslen(resource)>32000){free(j);return 0;}wcscpy(j->root,resource);wchar_t* slash=wcsrchr(j->root,L'\\');if(!slash)slash=wcsrchr(j->root,L'/');if(!slash){free(j);return 0;}*slash=0;
    swprintf(j->exe,32768,L"%ls\\update.exe",j->root);j->cached=cached;j->references=2;j->thread=CreateThread(NULL,0,launchWorker,j,0,NULL);
    if(!j->thread){j->references=1;launchRelease(j);return 0;}SetThreadPriority(j->thread,THREAD_PRIORITY_BELOW_NORMAL);u->launch=j;u->manual=manual;u->phase=cached?1:2;u->started=GetTickCount();return 1;
}
static void closePrompt(struct xide_update* u){if(u->box){xuiMsgBoxDestroy(u->box);u->box=NULL;}}
static void promptResult(xui_msgbox box,int result,void* user) {
    xide_app* app=user;struct xide_update* u=app->updater;if(!u)return;
    if(u->mandatory&&result!=XUI_MSGBOX_RESULT_YES&&result!=XUI_MSGBOX_RESULT_NO){xuiMsgBoxSetOpen(box,1);return;}
    closePrompt(u);
    if(result==XUI_MSGBOX_RESULT_YES||(u->mandatory&&result==XUI_MSGBOX_RESULT_NO)){
        u->pendingInstall=result==XUI_MSGBOX_RESULT_YES;u->transition=1;(void)xgeRequestQuit();
    }
}
static void prompt(xide_app* app) {
    struct xide_update* u=app->updater;if(!u||u->transition||u->box||xideActionsHasOtherModal(app))return;
    static const xui_msgbox_button_t optional[]={{"下载并安装",XUI_MSGBOX_RESULT_YES,XUI_BUTTON_SEMANTIC_PRIMARY},{"稍后再说",XUI_MSGBOX_RESULT_NO,XUI_BUTTON_SEMANTIC_DEFAULT}};
    static const xui_msgbox_button_t required[]={{"立即更新",XUI_MSGBOX_RESULT_YES,XUI_BUTTON_SEMANTIC_PRIMARY},{"退出程序",XUI_MSGBOX_RESULT_NO,XUI_BUTTON_SEMANTIC_DEFAULT}};
    if(!u->box){xui_msgbox_desc_t d={0};d.iSize=sizeof(d);d.sTitle=u->mandatory?"xlang IDE 必须更新":"xlang IDE 更新";d.sMessage=u->mandatory?"当前版本需要更新后才能继续使用。可先保存未保存的文件，再由独立更新器安装。":"发现可用更新。保存并关闭 IDE 后，将由独立更新器显示更新说明并安装。";d.pFont=app->uiFont;d.iType=XUI_MSGBOX_ICON_INFO;d.iButtons=XUI_MSGBOX_BUTTON_CUSTOM;d.pCustomButtons=u->mandatory?required:optional;d.iCustomButtonCount=2;d.bModal=1;d.bHasModal=1;
        if(xuiMsgBoxCreate(app->context,&u->box,&d)!=XUI_OK)return;
        xuiMsgBoxSetResult(u->box,promptResult,app);xideUiFontBindWidget(app,xuiMsgBoxGetWindowWidget(u->box));}
    xuiMsgBoxSetOpen(u->box,1);xideDialogStyleMessage(app,u->box);u->ready=0;
}
/* The caller accepts exit codes only from the authenticated local checker. */
void xideUpdateApplyCheckResult(xide_app* app,int code,int manual) {
    struct xide_update* u=state(app);if(!u)return;u->initialized=1;
    if(code==4){if(!u->mandatory)closePrompt(u);u->mandatory=1;u->ready=1;}
    else if(code==0||code==2){
        if(u->mandatory)closePrompt(u);
        u->mandatory=0;u->ready=code==2&&(manual||u->optional);
        if(manual&&code==0)xideMessageShow(app,"检查更新","当前已是最新版本。需要检查缺失文件时，可选择“更新／修复安装”。",0);
    }else if(manual)xideMessageShow(app,"检查更新","无法获取或验证更新信息。请检查网络或单独运行 update.exe 查看详情。",1);
    xideActionsRefresh(app);
}
int xideUpdateAction(xide_app* app,int id) {
    struct xide_update* u=state(app);if(!u)return XUI_ERROR_OUT_OF_MEMORY;u->initialized=1;
    if(id==XIDE_ACTION_UPDATE_REPAIR){if(u->check || u->launch){xideMessageShow(app,"更新","更新检查尚未结束，请稍候。",0);return XUI_OK;}u->pendingInstall=1;u->transition=1;(void)xgeRequestQuit();return XUI_OK;}
    if(!check(app,1,0)&&(u->check || u->launch))xideMessageShow(app,"检查更新","正在检查，请稍候。",0);
    return XUI_OK;
}
void xideUpdatePoll(xide_app* app) {
    if(app->hidden)return;
    struct xide_update* u=state(app);if(!u)return;
    if(!u->initialized && (!xideCatalogViewSettled(app) || !xideAssociationStartupSettled(app)))return;
    if(!u->initialized){u->initialized=1;bool enabled=true;xvalue* update=app->preferences.source?xrtValueObjectGet(app->preferences.source,(xstrview){"update",6}):NULL;xvalue* option=update?xrtValueObjectGet(update,(xstrview){"enabled",7}):NULL;if(option)xrtValueGetBool(option,&enabled);u->optional=enabled;if(!check(app,0,1))check(app,0,0);}
    if(u->launch || u->check)xgeRenderRequestAfter(0.05f);
    if(u->launch && InterlockedCompareExchange(&u->launch->done,0,0)){
        update_launch* j=u->launch;u->launch=NULL;u->check=j->process;j->process=NULL;wcscpy(u->log,j->log);j->log[0]=0;DWORD error=j->error;launchRelease(j);
        if(!u->check){DeleteFileW(u->log);u->log[0]=0;if(u->phase==1){u->phase=0;check(app,0,0);}else if(u->manual){char message[160];snprintf(message,sizeof(message),"无法启动更新检查，Windows 错误码：%lu。",(unsigned long)error);xideMessageShow(app,"检查更新",message,1);}}
    }
    if(u->check){DWORD result=WaitForSingleObject(u->check,0);if(result==WAIT_TIMEOUT&&GetTickCount()-u->started>60000){TerminateProcess(u->check,1);return;}
        if(result==WAIT_OBJECT_0){DWORD code=1;GetExitCodeProcess(u->check,&code);CloseHandle(u->check);u->check=NULL;DeleteFileW(u->log);u->log[0]=0;
            int cached=u->phase==1;u->phase=0;if(!cached||code==4)xideUpdateApplyCheckResult(app,(int)code,u->manual);if(cached)check(app,0,0);}}
    if(u->transition&&!xideMessagePending(app)&&!xideMessageBusy(app)){u->transition=0;u->pendingInstall=0;if(u->mandatory)u->ready=1;}
    if(u->ready)prompt(app);
}
int xideUpdateOnQuit(xide_app* app) {
    struct xide_update* u=app->updater;if(!u||!u->pendingInstall)return 1;wchar_t root[32768],exe[32768];
    if(!locations(root,exe)){xideMessageShow(app,"更新","未找到 update.exe，IDE 保持打开。",1);u->pendingInstall=0;return 0;}
    wchar_t* command=calloc(65536,sizeof(wchar_t));if(!command)return 0;
    swprintf(command,65536,L"\"%ls\" --root \"%ls\" --parent-pid %lu",exe,root,(unsigned long)GetCurrentProcessId());STARTUPINFOW si={0};PROCESS_INFORMATION pi;si.cb=sizeof(si);
    int ok=CreateProcessW(exe,command,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,root,&si,&pi);free(command);u->pendingInstall=0;
    if(ok){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}else xideMessageShow(app,"更新","无法启动更新器，IDE 保持打开。",1);return ok;
}
void xideUpdateDestroy(xide_app* app) {
    struct xide_update* u=app->updater;if(!u)return;if(u->launch){InterlockedExchange(&u->launch->cancelled,1);launchRelease(u->launch);u->launch=NULL;}if(u->check){TerminateProcess(u->check,1);WaitForSingleObject(u->check,3000);CloseHandle(u->check);}
    if(u->log[0])DeleteFileW(u->log);
    if(u->box)xuiMsgBoxDestroy(u->box);
    free(u);app->updater=NULL;
}
int xideUpdateIsModal(xide_app* app){return app&&app->updater&&((app->updater->mandatory&&!app->updater->transition)||(app->updater->box&&xuiMsgBoxIsOpen(app->updater->box)));}
int xideUpdatePending(xide_app* app){return app&&app->updater&&app->updater->pendingInstall;}
int xideUpdateIsRequired(xide_app* app){return app&&app->updater&&app->updater->mandatory;}
xui_msgbox xideUpdateCurrentPrompt(xide_app* app){return app&&app->updater?app->updater->box:NULL;}
