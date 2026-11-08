#include "ui/association_view.h"
#include "ui/catalog_view.h"
#include "platform/file_association.h"
#include "platform/startup_alias.h"
#include "services/ui_font.h"
#include "theme/theme.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum { ASSOCIATION_PROBE,ASSOCIATION_APPLY };
typedef struct association_job {
    volatile LONG references,done;
    HANDLE thread;
    wchar_t* exe;
    unsigned mask;
    int operation,prompt;
    xide_association_result result;
    DWORD error;
} association_job;
struct xide_association_view {
    xui_widget window,status,types[4],states[4],skip,confirm,close;
    association_job* job;
    int startupStarted,startupSettled,promptPending,manual,resultReady;
    xide_association_result result;
    char message[1024];
};
static void release(association_job* j)
{
    if(!j || InterlockedDecrement(&j->references))return;
    if(j->thread)CloseHandle(j->thread);
    free(j->exe);free(j);
}
static DWORD WINAPI execute(void* raw)
{
    association_job* j=raw;
    if(j->operation==ASSOCIATION_APPLY)j->error=xideAssociationApply(NULL,j->exe,j->mask);
    if(!j->error)j->error=xideAssociationProbe(NULL,j->exe,&j->result);
    if(!j->error && j->operation==ASSOCIATION_APPLY){
        int confirm=0;
        for(int i=0;i<4;++i)if((j->mask&(1u<<i)) && !j->result.isDefault[i])confirm=1;
        if(confirm)xideAssociationOpenDefaults();
    }
    InterlockedExchange(&j->done,1);release(j);return 0;
}
static struct xide_association_view* state(xide_app* a)
{
    if(!a->associationView)a->associationView=calloc(1,sizeof(*a->associationView));
    return a->associationView;
}
static void updateStates(xide_app* a)
{
    struct xide_association_view* v=a->associationView;
    for(int i=0;i<4;++i)if(v->states[i]){
        int linked=v->resultReady && v->result.registered[i] && v->result.isDefault[i];
        const char* text=!v->resultReady?"正在检测…":linked?"当前已关联":v->result.registered[i] && !v->result.defaultKnown[i]?"状态未知":"当前未关联";
        xuiLabelSetText(v->states[i],text);
        ideThemeSetTextRole(v->states[i],linked?IDE_SUCCESS:IDE_TEXT);
    }
}
static void controls(struct xide_association_view* v)
{
    if(!v->window)return;
    int applying=v->job && v->job->operation==ASSOCIATION_APPLY;
    xuiWidgetSetEnabled(v->confirm,!v->job);xuiWidgetSetEnabled(v->close,!applying);
    xuiWidgetSetEnabled(v->skip,!applying);
    for(int i=0;i<4;++i)xuiWidgetSetEnabled(v->types[i],!applying);
}
static int start(xide_app* a,int operation,unsigned mask,int prompt)
{
    struct xide_association_view* v=state(a);if(!v || v->job)return 0;
    wchar_t module[32768];DWORD length=GetModuleFileNameW(NULL,module,32768);
    if(!length || length>=32768)return 0;
    const wchar_t* exe=xideStartupResourceModule(module);
    association_job* j=calloc(1,sizeof(*j));if(!j)return 0;
    j->exe=malloc((wcslen(exe)+1)*sizeof(wchar_t));if(!j->exe){free(j);return 0;}
    wcscpy(j->exe,exe);j->references=2;j->operation=operation;j->mask=mask;j->prompt=prompt;
    j->thread=CreateThread(NULL,0,execute,j,0,NULL);
    if(!j->thread){j->references=1;release(j);return 0;}
    SetThreadPriority(j->thread,THREAD_PRIORITY_BELOW_NORMAL);v->job=j;
    if(operation==ASSOCIATION_PROBE){v->resultReady=0;updateStates(a);}
    controls(v);return 1;
}
static unsigned selectedMask(struct xide_association_view* v)
{
    unsigned mask=0;
    for(int i=0;i<4;++i)if(xuiCheckBoxGetChecked(v->types[i]))mask|=1u<<i;
    return mask;
}
static void message(xide_app* a,const char* text)
{
    struct xide_association_view* v=a->associationView;
    snprintf(v->message,sizeof(v->message),"%s",text);
    if(v->status){xuiLabelSetText(v->status,v->message);xuiWidgetSetVisible(v->status,*text!=0);}
    xgeRenderRequest();
}
static void populate(xide_app* a)
{
    struct xide_association_view* v=a->associationView;
    unsigned mask=xideSettingsAssociationMask(&a->preferences);
    for(int i=0;i<4;++i)xuiCheckBoxSetChecked(v->types[i],!!(mask&(1u<<i)));
    xuiCheckBoxSetChecked(v->skip,!xideSettingsAssociationPrompt(&a->preferences));
}
static void action(xui_widget widget,void* raw)
{
    xide_app* a=raw;struct xide_association_view* v=a->associationView;
    if(widget==v->close){
        if(xideAssociationViewBusy(a)){message(a,"正在应用文件关联，请完成后关闭。");return;}
        /* Close, Escape and the title-bar X discard the entire draft. */
        v->promptPending=0;populate(a);xideDialogSetOpen(v->window,0);return;
    }
    if(widget!=v->confirm || v->job)return;
    int prompt=!xuiCheckBoxGetChecked(v->skip);
    if(start(a,ASSOCIATION_APPLY,selectedMask(v),prompt)){
        v->manual=1;message(a,"正在根据勾选结果应用文件关联 ...");
    }else message(a,"无法启动文件关联任务，请重试。");
}
static void cancelled(xui_widget widget,void* raw)
{(void)widget;xide_app* a=raw;action(a->associationView->close,a);}
#define TRY(e) do{int code=(e);if(code!=XUI_OK)goto failed;}while(0)
static int openView(xide_app* a,int refresh)
{
    struct xide_association_view* v=state(a);if(!v)return 0;
    if(!v->window){
        TRY(xideDialogCreate(a,&v->window,"文件关联",460,240,12));
        TRY(xuiWidgetAddChild(a->root,v->window));TRY(xideDialogFormBegin(v->window,380,230));
        TRY(xideDialogFormText(v->window,"勾选要关联的文件类型；取消勾选会取消本程序的关联。\n默认应用需要时请在 Windows 设置中确认。"));
        const char* labels[]={"工作空间 .xlw","工程 .xlp","xlang 脚本 .xl","C 源文件 .c（独立选项）"};
        xui_widget body=xideDialogGetContentWidget(v->window);
        for(int i=0;i<4;++i){
            xui_checkbox_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=labels[i];
            TRY(xuiCheckBoxCreate(a->context,&v->types[i],&d));TRY(xuiWidgetAddChild(body,v->types[i]));
            xui_label_desc_t stateLabel={0};stateLabel.iSize=sizeof(stateLabel);stateLabel.pFont=a->uiFont;stateLabel.sText="正在检测…";stateLabel.iTextFlags=XUI_TEXT_ALIGN_MIDDLE;
            TRY(xuiLabelCreate(a->context,&v->states[i],&stateLabel));TRY(xuiWidgetAddChild(body,v->states[i]));
            xui_widget row[]={v->types[i],v->states[i]};
            TRY(xideDialogFormRow(v->window,row,2,26,0));
        }
        xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;
        label.sText=v->message;label.iWrapMode=XUI_TEXT_WRAP_WORD;
        TRY(xuiLabelCreate(a->context,&v->status,&label));TRY(xuiWidgetAddChild(body,v->status));
        TRY(xideDialogFormBlock(v->window,v->status,0,0));TRY(xuiWidgetSetVisible(v->status,*v->message!=0));
        xui_widget* buttons[]={&v->close,&v->confirm};const char* names[]={"关闭","确定"};xui_widget footer[2];
        for(int i=0;i<2;++i){
            xui_button_desc_t d={0};d.iSize=sizeof(d);d.pFont=a->uiFont;d.sText=names[i];
            TRY(xuiButtonCreate(a->context,buttons[i],&d));TRY(xuiWidgetAddChild(body,*buttons[i]));
            TRY(xuiButtonSetClick(*buttons[i],action,a));footer[i]=*buttons[i];
        }
        TRY(xideDialogFormActions(v->window,footer,2));
        xui_checkbox_desc_t skip={0};skip.iSize=sizeof(skip);skip.pFont=a->uiFont;skip.sText="不再自动提示";
        TRY(xuiCheckBoxCreate(a->context,&v->skip,&skip));TRY(xuiWidgetAddChild(body,v->skip));
        TRY(xideDialogFormFooterLeading(v->window,v->skip));
        TRY(xideDialogFormEnd(v->window));TRY(xideDialogSetCancel(v->window,cancelled,a));
        TRY(xideUiFontBindWidget(a,v->window));
    }
    if(!xideAssociationViewIsOpen(a))populate(a);
    updateStates(a);
    TRY(xideDialogSetOpen(v->window,1));v->promptPending=0;
    if(refresh && !v->job){
        v->manual=1;v->startupStarted=1;
        if(!start(a,ASSOCIATION_PROBE,xideSettingsAssociationMask(&a->preferences),1))message(a,"无法启动文件关联检查。");
    }
    controls(v);xideActionsRefresh(a);xgeRenderRequest();return 1;
failed:
    xideDialogDestroy(v->window);v->window=v->status=v->skip=v->confirm=v->close=NULL;
    memset(v->types,0,sizeof(v->types));memset(v->states,0,sizeof(v->states));return 0;
}
int xideAssociationViewOpen(xide_app* a){return openView(a,1);}
int xideAssociationViewAction(xide_app* a,int id){(void)id;return xideAssociationViewOpen(a)?XUI_OK:XUI_ERROR;}
int xideAssociationViewIsOpen(xide_app* a){return a && a->associationView && xideDialogIsOpen(a->associationView->window);}
xui_widget xideAssociationViewWindow(xide_app* a){return a && a->associationView?a->associationView->window:NULL;}
int xideAssociationViewBusy(xide_app* a){return a && a->associationView && a->associationView->job && a->associationView->job->operation==ASSOCIATION_APPLY;}
int xideAssociationStartupSettled(xide_app* a){return a && (a->hidden || (a->associationView && a->associationView->startupSettled));}
void xideAssociationViewPoll(xide_app* a)
{
    struct xide_association_view* v=a->associationView;
    if(!a->hidden && a->frames>0 && xideCatalogViewSettled(a) && (!v || !v->startupStarted)){
        v=state(a);if(!v)return;v->startupStarted=1;
        if(!v->job && !start(a,ASSOCIATION_PROBE,xideSettingsAssociationMask(&a->preferences),1))v->startupSettled=1;
    }
    if(!v)return;
    if(v->job)xgeRenderRequestAfter(0.025f);
    if(v->job && InterlockedCompareExchange(&v->job->done,0,0)){
        association_job* j=v->job;v->job=NULL;v->startupSettled=1;controls(v);
        if(j->error){
            char text[160];snprintf(text,sizeof(text),"文件关联操作未完成，Windows 错误码：%lu。可重试。",(unsigned long)j->error);message(a,text);
        }else{
            int missing=0;v->result=j->result;v->resultReady=1;updateStates(a);
            for(int i=0;i<4;++i){
                if(j->mask&(1u<<i))missing|=!j->result.registered[i] || (j->result.defaultKnown[i] && !j->result.isDefault[i]);
            }
            message(a,"");
            if(j->operation==ASSOCIATION_APPLY){
                xide_model_error error={0};
                if(xideSettingsSetAssociations(&a->preferences,j->mask,j->prompt,&error)){
                    v->promptPending=0;xideDialogSetOpen(v->window,0);
                }else{
                    char warning[700];snprintf(warning,sizeof(warning),"文件关联已更新，但设置保存失败：%.500s",error.message);message(a,warning);
                }
            }else if(!v->manual && missing && xideSettingsAssociationPrompt(&a->preferences))v->promptPending=1;
        }
        release(j);v->manual=0;
    }
    if(v->promptPending && !a->hidden && !xideActionsHasModal(a))openView(a,0);
}
void xideAssociationViewDestroy(xide_app* a)
{
    if(!a || !a->associationView)return;
    release(a->associationView->job);free(a->associationView);a->associationView=NULL;
}
