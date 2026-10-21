#include "app/xide_app.h"
#include "ui/message_box.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct decision { uint64_t key; int result; struct decision* next; } decision;
typedef struct save_path { uint64_t document; char* path; struct save_path* next; } save_path;
typedef struct file_choice { uint64_t key;char* path;struct file_choice* next; } file_choice;
struct xide_messages {
    xui_msgbox box; xui_widget restore;
    decision* decisions; save_path* paths;
    file_choice* choices; uint64_t fileKey,fileWorkspace,fileDocument;uint32_t fileVersion;char* filePath;int fileReady;
    uint64_t key,document; char* project;
    int action,quit,replaying,pending,ready,result,buttons;
};
static char* copy(const char* s) { if(!s) return NULL; size_t n=strlen(s)+1;char* r=malloc(n);if(r) memcpy(r,s,n);return r; }
static xide_messages* manager(xide_app* a) { if(!a->messages) a->messages=calloc(1,sizeof(*a->messages));return a->messages; }
static void clear(xide_messages* m) {
    while(m->decisions) { decision* d=m->decisions;m->decisions=d->next;free(d); }
    while(m->paths) { save_path* p=m->paths;m->paths=p->next;free(p->path);free(p); }
    while(m->choices) { file_choice* p=m->choices;m->choices=p->next;free(p->path);free(p); }
    free(m->filePath);m->filePath=NULL;m->fileReady=0;
    free(m->project);m->project=NULL;m->action=0;m->quit=0;m->document=0;m->pending=0;
}
static uint64_t hash(uint64_t h,const void* data,size_t size) { const unsigned char* p=data;for(size_t i=0;i<size;++i) h=(h^p[i])*UINT64_C(1099511628211);return h; }
uint64_t xideMessageDocumentKey(xide_document* d,int kind,int disk) {
    uint64_t h=hash(UINT64_C(14695981039346656037),&kind,sizeof(kind));
    h=hash(h,&d->id,sizeof(d->id));uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));h=hash(h,&version,sizeof(version));
    if(d->path) h=hash(h,d->path,strlen(d->path));
    if(disk && d->path) { xide_file_snapshot s={0};xide_model_error e={0};int okay=xideFileSnapshotInspect(d->path,&s,&e);h=hash(h,&okay,sizeof(okay));if(okay) h=hash(h,&s,sizeof(s)); }
    return h;
}
uint64_t xideMessageWorkspaceKey(xide_app* a,int kind) {
    uint64_t h=hash(UINT64_C(14695981039346656037),&kind,sizeof(kind));xide_model_error e={0};
    xvalue* v=xideWorkspaceExport(&a->workspaceModel,&e);size_t n=0;char* s=v ? xrtJsonStringify(v,false,&n) : NULL;
    if(s) h=hash(h,s,n);
    xrtFree(s);xrtValueRelease(v);
    for(size_t i=0;i<a->workspaceModel.projectCount;++i) { v=xideProjectExport(&a->workspaceModel.projects[i],&e);s=v ? xrtJsonStringify(v,false,&n) : NULL;if(s) h=hash(h,s,n);xrtFree(s);xrtValueRelease(v); }
    if(a->selectedProject) h=hash(h,a->selectedProject,strlen(a->selectedProject));
    return h;
}
static void result(xui_msgbox box,int answer,void* user) { xide_app* a=user;a->messages->result=answer;a->messages->ready=1;xgeRenderRequest(); }
static void fileSelected(xide_app* a,const char* path,void* user) {
    xide_messages* m=a->messages;int valid=xideMessageWorkspaceKey(a,90)==m->fileWorkspace;
    if(m->fileDocument) {
        xide_document* d=NULL;for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open && a->documents.items[i]->id==m->fileDocument) d=a->documents.items[i];
        valid=valid && d && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))==m->fileVersion;
    }
    m->filePath=path && valid ? copy(path) : NULL;m->fileReady=1;m->ready=1;m->result=m->filePath ? XUI_MSGBOX_RESULT_OK : XUI_MSGBOX_RESULT_CANCEL;xgeRenderRequest();
}
char* xideMessageChoosePath(xide_app* a,int mode,const char* title,const char* filter,const char* initial,const char* extension,uint64_t key,uint64_t document) {
    xide_messages* m=manager(a);if(!m || (!m->action && !m->quit) || a->hidden) return NULL;
    for(file_choice* p=m->choices;p;p=p->next) if(p->key==key) return copy(p->path);
    if(m->pending || m->box || xideFileDialogBusy(a)) return NULL;
    m->fileKey=key;m->fileWorkspace=xideMessageWorkspaceKey(a,90);m->fileDocument=document;m->fileVersion=0;
    for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open && a->documents.items[i]->id==document) m->fileVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(a->documents.items[i]->editor));
    m->pending=xideFileDialogChoose(a,NULL,mode,title,filter,initial,extension,fileSelected,a);return NULL;
}
static int show(xide_app* a,const char* title,const char* text,int icon,int buttons) {
    xide_messages* m=manager(a);if(!m || m->box) return 0;
    xui_msgbox_desc_t d={0};d.iSize=sizeof(d);d.sTitle=title;d.sMessage=text;d.pFont=a->uiFont;d.iType=icon;d.iButtons=buttons;d.bModal=1;d.bHasModal=1;
    int ret=xuiMsgBoxCreate(a->context,&m->box,&d);if(ret!=XUI_OK) { fprintf(stderr,"XUI msgbox creation failed: %d: %s\n",ret,text);return 0; }
    float scale=xideUiFontScale(a);xui_msgbox_metrics_t metrics={0};metrics.iSize=sizeof(metrics);xuiMsgBoxGetMetrics(m->box,&metrics);
    metrics.fPaddingX*=scale;metrics.fPaddingY*=scale;metrics.fIconSize*=scale;metrics.fIconGap*=scale;
    metrics.fButtonWidth*=scale;metrics.fButtonHeight*=scale;metrics.fButtonGap*=scale;metrics.fFooterHeight*=scale;metrics.fMinMessageHeight*=scale;
    metrics.fMinWidth*=scale;metrics.fMaxWidth*=scale;xuiMsgBoxSetMetrics(m->box,&metrics);
    xuiWindowSetChrome(xuiMsgBoxGetWindowWidget(m->box),32*scale,1,6*scale,24*scale);
    const char* labels[]={"确定","取消","是","否"};
    const int ids[]={XUI_MSGBOX_BUTTON_TITLE_OK,XUI_MSGBOX_BUTTON_TITLE_CANCEL,XUI_MSGBOX_BUTTON_TITLE_YES,XUI_MSGBOX_BUTTON_TITLE_NO};
    for(size_t i=0;i<4;++i) xuiMsgBoxSetPresetButtonTitle(m->box,ids[i],labels[i]);
    m->restore=xuiGetFocusWidget(a->context);m->buttons=buttons;m->ready=0;
    xuiMsgBoxSetResult(m->box,result,a);xuiMsgBoxSetOpen(m->box,1);
    xideUiFontBindWidget(a,xuiMsgBoxGetWindowWidget(m->box));xideActionsRefresh(a);xgeRenderRequest();return 1;
}
void xideMessageShow(xide_app* a,const char* title,const char* text,int icon) {
    if(xideMessagePending(a)) return;
    if(a->hidden || !a->context) { fprintf(stderr,"%s: %s\n",title,text);return; }
    if(!show(a,title,text,icon,XUI_MSGBOX_BUTTON_OK)) fprintf(stderr,"%s: %s\n",title,text);
}
int xideMessageConfirm(xide_app* a,const char* title,const char* text,int buttons,uint64_t key) {
    xide_messages* m=manager(a);if(!m || a->hidden || !a->context || (!m->action && !m->quit)) return XUI_MSGBOX_RESULT_CANCEL;
    for(decision* d=m->decisions;d;d=d->next) if(d->key==key) return d->result;
    if(m->box) return XUI_MSGBOX_RESULT_CLOSE;
    m->key=key;m->pending=show(a,title,text,XUI_MSGBOX_ICON_QUEST,buttons);return XUI_MSGBOX_RESULT_CLOSE;
}
void xideMessageBeginAction(xide_app* a,int action) {
    xide_messages* m=manager(a);if(!m || m->replaying || m->pending || m->box) return;
    clear(m);m->action=action;m->document=a->activeDocument ? a->activeDocument->id : 0;m->project=copy(a->selectedProject);
}
void xideMessageBeginClose(xide_app* a,xide_document* d) { xideMessageBeginAction(a,XIDE_ACTION_CLOSE_FILE);if(a->messages) a->messages->document=d->id; }
void xideMessageBeginQuit(xide_app* a) { xide_messages* m=manager(a);if(!m || m->replaying || m->action || m->pending) return;clear(m);m->quit=1;m->project=copy(a->selectedProject); }
void xideMessageEndOperation(xide_app* a) { if(a->messages && !a->messages->pending) clear(a->messages); }
int xideMessagePending(xide_app* a) { return a && a->messages && a->messages->pending; }
int xideMessageBusy(xide_app* a) { return a && a->messages && (a->messages->box || a->messages->ready || a->messages->pending); }
void xideMessagePoll(xide_app* a) {
    xide_messages* m=a->messages;if(!m || !m->ready) return;
    int pending=m->pending,answer=m->result,buttons=m->buttons;m->ready=0;m->pending=0;
    if(m->box) xuiMsgBoxDestroy(m->box);
    m->box=NULL;
    if(m->restore) xuiSetFocusWidget(a->context,m->restore);
    m->restore=NULL;
    int continueOperation=pending && (answer==XUI_MSGBOX_RESULT_OK || (answer==XUI_MSGBOX_RESULT_NO && buttons==XUI_MSGBOX_BUTTON_YES_NO_CANCEL));
    if(!continueOperation) { clear(m);xideActionsRefresh(a);return; }
    if(m->fileReady) {
        file_choice* p=calloc(1,sizeof(*p));if(!p) { clear(m);return; }p->key=m->fileKey;p->path=m->filePath;m->filePath=NULL;p->next=m->choices;m->choices=p;m->fileReady=0;
    } else { decision* d=calloc(1,sizeof(*d));if(!d) { clear(m);return; }d->key=m->key;d->result=answer;d->next=m->decisions;m->decisions=d; }
    xide_document* document=NULL;
    for(size_t i=0;i<a->documents.count;++i) if(a->documents.items[i]->open && a->documents.items[i]->id==m->document) document=a->documents.items[i];
    if(m->document && !document) { clear(m);xideActionsRefresh(a);return; }
    if(m->project && (!a->selectedProject || strcmp(m->project,a->selectedProject))) { clear(m);xideActionsRefresh(a);return; }
    m->replaying=1;
    if(m->quit) xgeRequestQuit();
    else { if(document) { a->activeDocument=document;xideEditorShow(a,document); }xideDispatchAction(a,m->action); }
    m->replaying=0;xideMessageEndOperation(a);xideActionsRefresh(a);
}
void xideMessageDestroy(xide_app* a) { if(a->messages) { if(a->messages->box) xuiMsgBoxDestroy(a->messages->box);clear(a->messages);free(a->messages);a->messages=NULL; } }
xui_msgbox xideMessageCurrent(xide_app* a) { return a && a->messages ? a->messages->box : NULL; }
char* xideMessageSavePath(xide_app* a,uint64_t document) { xide_messages* m=a->messages;if(m) for(save_path* p=m->paths;p;p=p->next) if(p->document==document) return copy(p->path);return NULL; }
int xideMessageRememberSavePath(xide_app* a,uint64_t document,const char* path) {
    xide_messages* m=a->messages;if(!m || (!m->action && !m->quit)) return 1;
    save_path* p=calloc(1,sizeof(*p));if(!p) return 0;p->path=copy(path);if(!p->path) { free(p);return 0; }p->document=document;p->next=m->paths;m->paths=p;return 1;
}
