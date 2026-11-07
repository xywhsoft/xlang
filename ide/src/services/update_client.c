/* The independent updater owns network/verification, separately from XGE. */
#include "services/update_client.h"
#include "app/xide_app.h"
#include "platform/startup_alias.h"
#include "services/ui_font.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
struct xide_update {HANDLE check;DWORD started;int initialized,manual,phase,ready,pendingInstall,transition,mandatory,optional;wchar_t log[32768];xui_msgbox box;};
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
static int check(xide_app* app,int manual,int cached) {
    struct xide_update* u=state(app);if(!u||u->check)return 0;wchar_t root[32768],exe[32768],temp[32768];
    if(!locations(root,exe)){if(manual)xideMessageShow(app,"检查更新","未找到独立更新器 update.exe。",1);return 0;}
    if(!GetTempPathW(32768,temp)||!GetTempFileNameW(temp,L"xlu",0,u->log))return 0;
    SECURITY_ATTRIBUTES sa={sizeof(sa),NULL,TRUE};HANDLE log=CreateFileW(u->log,GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,NULL);
    wchar_t* command=calloc(65536,sizeof(wchar_t));if(!command||log==INVALID_HANDLE_VALUE||input==INVALID_HANDLE_VALUE){free(command);if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);DeleteFileW(u->log);return 0;}
    swprintf(command,65536,L"\"%ls\" --root \"%ls\" %ls --no-launch --plain --staged --timeout 10",exe,root,cached?L"--cached-policy":L"--check-version");
    STARTUPINFOW si={0};PROCESS_INFORMATION pi;si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=input;si.hStdOutput=log;si.hStdError=log;
    int ok=CreateProcessW(exe,command,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,root,&si,&pi);free(command);CloseHandle(log);CloseHandle(input);
    if(ok){CloseHandle(pi.hThread);u->check=pi.hProcess;u->manual=manual;u->phase=cached?1:2;u->started=GetTickCount();}
    else{DeleteFileW(u->log);if(manual)xideMessageShow(app,"检查更新","无法启动独立更新检查程序。",1);}return ok;
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
    if(id==XIDE_ACTION_UPDATE_REPAIR){if(u->check){xideMessageShow(app,"更新","更新检查尚未结束，请稍候。",0);return XUI_OK;}u->pendingInstall=1;u->transition=1;(void)xgeRequestQuit();return XUI_OK;}
    if(!check(app,1,0)&&u->check)xideMessageShow(app,"检查更新","正在检查，请稍候。",0);
    return XUI_OK;
}
void xideUpdatePoll(xide_app* app) {
    if(app->hidden)return;
    struct xide_update* u=state(app);if(!u)return;
    if(!u->initialized){u->initialized=1;bool enabled=true;xvalue* update=app->preferences.source?xrtValueObjectGet(app->preferences.source,(xstrview){"update",6}):NULL;xvalue* option=update?xrtValueObjectGet(update,(xstrview){"enabled",7}):NULL;if(option)xrtValueGetBool(option,&enabled);u->optional=enabled;if(!check(app,0,1))check(app,0,0);}
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
    struct xide_update* u=app->updater;if(!u)return;if(u->check){TerminateProcess(u->check,1);WaitForSingleObject(u->check,3000);CloseHandle(u->check);}
    if(u->log[0])DeleteFileW(u->log);
    if(u->box)xuiMsgBoxDestroy(u->box);
    free(u);app->updater=NULL;
}
int xideUpdateIsModal(xide_app* app){return app&&app->updater&&((app->updater->mandatory&&!app->updater->transition)||(app->updater->box&&xuiMsgBoxIsOpen(app->updater->box)));}
int xideUpdatePending(xide_app* app){return app&&app->updater&&app->updater->pendingInstall;}
int xideUpdateIsRequired(xide_app* app){return app&&app->updater&&app->updater->mandatory;}
xui_msgbox xideUpdateCurrentPrompt(xide_app* app){return app&&app->updater?app->updater->box:NULL;}
