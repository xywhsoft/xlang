#include <shellapi.h>
#include "services/ui_font.h"
typedef struct xide_symbol_rename_view {
 xui_widget window,content,message,controls[XIDE_SYMBOL_CONTROL_COUNT];
 uint64_t document,revision,token;int caret,phase,closeWanted,quitPending;
 document_binding inputs[SNAPSHOT_DOCS];size_t inputCount;
 uint64_t prepareToken,prepareClient,prepareSession,prepareSource,prepareWatch,prepareDeadline,prepareDocuments;
 int prepareStage,prepared;char prepareId[40];
 xide_lsp_position prepareAt;
 char *origin,*recovery,**rows;size_t count,edits,open,closed;
 char text[1536];
} xide_symbol_rename_view;
static xide_symbol_rename_view* symbolView(xide_app* a)
{return a&&a->languageView?a->languageView->renameSymbol:NULL;}
static void symbolMessage(xide_symbol_rename_view* r,const char* text)
{snprintf(r->text,sizeof(r->text),"%s",text);xuiLabelSetText(r->message,r->text);xgeRenderRequest();}
static void symbolRowsFree(xide_symbol_rename_view* r)
{for(size_t i=0;i<r->count;++i)free(r->rows[i]);free(r->rows);r->rows=NULL;r->count=r->edits=r->open=r->closed=0;}
static void symbolInputsFree(xide_symbol_rename_view* r)
{for(size_t i=0;i<r->inputCount;++i)freeInput(r->inputs+i);r->inputCount=0;}
static void symbolEndPrepare(xide_language_view* v,int cancel)
{
 xide_symbol_rename_view* r=v->renameSymbol;if(!r)return;
 if(cancel && r->prepareStage==4 && *r->prepareId && v->client && xideLanguageClientIdentity(v->client)==r->prepareClient){
  xvalue* p=xrtValueObject();if(p && put(p,"id",xrtValueString(sv(r->prepareId))))sendMessage(v,"$/cancelRequest",p,0);else xrtValueRelease(p);
 }
 xideRenamePrepareCancel(v->renamePrepare);if(v->syncMode==7)cancelSyncPlan(v);
 r->prepareStage=0;r->prepareToken=0;*r->prepareId=0;
}
static void destroySymbolRename(xide_language_view* v)
{xide_symbol_rename_view* r=v->renameSymbol;if(r){symbolInputsFree(r);symbolRowsFree(r);free(r->origin);free(r->recovery);free(r);v->renameSymbol=NULL;}}
static xide_document* symbolDocument(xide_app* a,xide_symbol_rename_view* r)
{for(size_t i=0;i<a->documents.count;++i){xide_document* d=a->documents.items[i];if(d->open&&d->id==r->document)return d;}return NULL;}
static int symbolPreparing(xide_language_view* v)
{return v && v->renameSymbol && v->renameSymbol->phase==XIDE_SYMBOL_PREPARING;}
static int symbolPrepareCurrent(xide_app* a)
{
 xide_language_view* v=a->languageView;xide_symbol_rename_view* r=v->renameSymbol;xide_document* d=r?symbolDocument(a,r):NULL;
 if(!r || !d || a->activeDocument!=d || d->revision!=r->revision || !d->path || !xidePathLexicalEqual(d->path,r->origin) || xuiCodeEditIsReadonly(d->editor) ||
    xideDocumentsSourcesHeld(&a->documents) || xideSourceJobsHeld(&a->sourceJobs) || xideDocumentsSourceEpoch(&a->documents)!=r->prepareDocuments ||
    v->sourceToken || v->sourceDrain || v->sourceSerial!=r->prepareSource || v->highlightSession!=r->prepareSession ||
    xideLanguageClientIdentity(v->client)!=r->prepareClient || ((r->prepareStage>=2||r->prepared) && xideFileSyncRevision(a)!=r->prepareWatch))return 0;
 xui_code_selection_t selection={0};if(xuiCodeSelectionGetCount(xuiCodeEditGetSelection(d->editor))!=1 || xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)!=XUI_OK || selection.iCaretOffset!=r->caret)return 0;
 size_t count=0;
 for(size_t i=0;i<a->documents.count;++i){xide_document* now=a->documents.items[i];if(!namedXlang(now))continue;++count;size_t j=0;
  for(;j<r->inputCount && r->inputs[j].id!=now->id;++j){}
  if(j==r->inputCount)return 0;
  document_binding* b=r->inputs+j;
  if(!now->revision || now->revision!=b->revision || snapshotConflict(now) || xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(now->editor))!=b->version || strcmp(now->path,b->path) || strcmp(now->language,b->language))return 0;
 }
 return count==r->inputCount;
}
static int symbolInputsCapture(xide_language_view* v)
{
 xide_symbol_rename_view* r=v->renameSymbol;symbolInputsFree(r);size_t labels=0;
 for(size_t i=0;i<v->app->documents.count;++i){xide_document* d=v->app->documents.items[i];if(!namedXlang(d))continue;
  if(r->inputCount==SNAPSHOT_DOCS || !d->revision || snapshotConflict(d))return 0;
  size_t path=strlen(d->path)+1,language=strlen(d->language)+1;
  if(path>262145 || language>64 || path+language>2u*1024u*1024u-labels)return 0;
  labels+=path+language;
  document_binding* b=r->inputs+r->inputCount++;*b=(document_binding){.id=d->id,.revision=d->revision,.version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.path=copy(d->path),.language=copy(d->language)};
  if(!b->path || !b->language)return 0;
 }
 r->prepareClient=xideLanguageClientIdentity(v->client);r->prepareSession=v->highlightSession;r->prepareSource=v->sourceSerial;r->prepareWatch=xideFileSyncRevision(v->app);r->prepareDocuments=xideDocumentsSourceEpoch(&v->app->documents);
 return r->inputCount>0;
}
static void symbolPrepareSynced(xide_language_view* v)
{if(symbolPreparing(v) && v->renameSymbol->prepareStage==2)v->renameSymbol->prepareStage=3;}
static void symbolControls(xide_symbol_rename_view* r)
{
 int input=r->phase==XIDE_SYMBOL_INPUT,confirm=r->phase==XIDE_SYMBOL_CONFIRM,recovery=r->phase==XIDE_SYMBOL_UNCERTAIN;
 xuiWidgetSetEnabled(r->controls[XIDE_SYMBOL_NAME],input);
 xuiWidgetSetEnabled(r->controls[XIDE_SYMBOL_PREVIEW],input);
 xuiWidgetSetEnabled(r->controls[XIDE_SYMBOL_APPLY],confirm||recovery);
 xuiButtonSetText(r->controls[XIDE_SYMBOL_APPLY],recovery?"退出 IDE…":"确认重命名");
 xuiButtonSetText(r->controls[XIDE_SYMBOL_CANCEL],recovery?"返回编辑器":r->phase==XIDE_SYMBOL_RESULT?"完成":r->phase==XIDE_SYMBOL_CANCELLING?"正在取消…":"取消");
 xuiWidgetSetEnabled(r->controls[XIDE_SYMBOL_CANCEL],r->phase!=XIDE_SYMBOL_CANCELLING);
 xuiWidgetSetVisible(r->controls[XIDE_SYMBOL_RECOVERY],recovery);
 /* Disabling the name/preview control can otherwise leave keyboard focus
  * outside the modal window. Keep Escape routed to its owned cancellation.
  * Do not steal editor focus from nonmodal progress/results. */
 if(xideDialogIsModal(r->window)&&r->phase!=XIDE_SYMBOL_INPUT&&r->phase!=XIDE_SYMBOL_CANCELLING)xuiSetFocusWidget(xuiWidgetGetContext(r->window),r->controls[XIDE_SYMBOL_CANCEL]);
}
static void symbolOpenWindow(xide_symbol_rename_view* r,int modal)
{if(xideDialogIsOpen(r->window)&&xideDialogIsModal(r->window)!=modal)xideDialogSetOpen(r->window,0);if(!xideDialogIsOpen(r->window)){xideDialogSetModal(r->window,modal);xideDialogSetOpen(r->window,1);}}
static void symbolCancel(xide_app* a,int quit)
{
 xide_symbol_rename_view* r=symbolView(a);if(!r)return;r->quitPending=quit;r->closeWanted=1;
 if(r->phase==XIDE_SYMBOL_PREPARING){symbolEndPrepare(a->languageView,1);symbolInputsFree(r);r->prepared=0;xideDialogSetOpen(r->window,0);r->phase=XIDE_SYMBOL_CLOSED;return;}
 if(r->phase==XIDE_SYMBOL_UNCERTAIN){if(!quit)xideDialogSetOpen(r->window,0);return;}
 if(r->phase==XIDE_SYMBOL_INPUT||r->phase==XIDE_SYMBOL_RESULT){symbolEndPrepare(a->languageView,0);symbolInputsFree(r);r->prepared=0;xideDialogSetOpen(r->window,0);r->phase=XIDE_SYMBOL_CLOSED;return;}
 if(r->token)xideLanguageEditApplyCancel(a,r->token);else xideLanguageEditCandidateEnd(a);
 r->phase=XIDE_SYMBOL_CANCELLING;symbolMessage(r,"正在取消并收尾；已发生的未提交写入会先恢复。请稍候。");symbolControls(r);
}
static void symbolSelected(xui_widget w,int node,void* user)
{(void)w;xide_app* a=user;xide_symbol_rename_view* r=symbolView(a);if(!r)return;const xide_workspace_edit* plan=xideLanguageEditApplyPlan(a,r->token);xide_workspace_edit_document d={0};xuiEditSetText(r->controls[XIDE_SYMBOL_PATH],node>0&&plan&&xideWorkspaceEditDocumentAt(plan,(size_t)node-1,&d)?d.path:r->recovery?r->recovery:r->origin?r->origin:"");}
static void symbolPreview(xide_app* a)
{
 xide_language_view* v=a->languageView;xide_symbol_rename_view* r=symbolView(a);if(!r||r->phase!=XIDE_SYMBOL_INPUT)return;
 xide_document* d=symbolDocument(a,r);const char* name=xuiEditGetText(r->controls[XIDE_SYMBOL_NAME]);size_t length=0;if(name)while(length<=4096&&name[length])++length;
 if(!name||!length||length>4096||!xrtUtf8Valid((xstrview){name,length},NULL)){symbolMessage(r,"请输入有效的新名称（最多 4096 字节）；名称是否合法由语言服务检查。");return;}
 if(!d||d->revision!=r->revision||!d->path||!xidePathLexicalEqual(d->path,r->origin)||(r->prepared&&!symbolPrepareCurrent(a))){symbolMessage(r,"原文档或分析输入已改变，请关闭窗口后重新选择符号。");return;}
 int line=0,character=0;const char* uri=NULL;for(size_t i=0;i<v->inputCount;++i)if(v->inputs[i].id==d->id&&v->inputs[i].revision==d->revision){uri=v->inputs[i].uri;break;}
 xide_model_error e={0};if(!uri||!xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(d->editor),r->caret,&line,&character)||!xideLanguageEditCandidateBegin(a,uri,line,character,(xstrview){name,length},&e)){symbolMessage(r,e.message[0]?e.message:"当前输入尚未同步，或另一项分析正在收尾；请稍后重试。");return;}
 symbolEndPrepare(v,0);symbolInputsFree(r);r->prepared=0;r->phase=XIDE_SYMBOL_PLANNING;symbolMessage(r,"正在分析符号、引用与文件来源，生成完整修改预览…");symbolControls(r);xgeRenderRequest();
}
static void symbolApply(xide_app* a)
{
 xide_symbol_rename_view* r=symbolView(a);if(!r)return;
 if(r->phase==XIDE_SYMBOL_UNCERTAIN){xgeRequestQuit();return;}
 if(r->phase!=XIDE_SYMBOL_CONFIRM)return;
 xide_model_error e={0};if(!xideLanguageEditApplyConfirm(a,r->token,&e)){symbolMessage(r,e.message[0]?e.message:"预览已失效，未应用修改；请取消后重新预览。");return;}
 r->phase=XIDE_SYMBOL_APPLYING;symbolMessage(r,"正在准备并应用修改。可以继续编辑；代码变化会取消本次修改。保存、文件操作及新构建将在收尾后恢复。");symbolControls(r);symbolOpenWindow(r,0);
 xide_document* d=symbolDocument(a,r);if(d)xuiSetFocusWidget(a->context,d->editor);xideActionsRefresh(a);
}
static void symbolClicked(xui_widget w,void* user)
{
 xide_app* a=user;xide_symbol_rename_view* r=symbolView(a);if(!r)return;
 if(w==r->controls[XIDE_SYMBOL_PREVIEW])symbolPreview(a);
 else if(w==r->controls[XIDE_SYMBOL_APPLY])symbolApply(a);
 else if(w==r->controls[XIDE_SYMBOL_RECOVERY]){wchar_t* path=r->recovery?xidePathNativeWide(r->recovery):NULL;if(!path||(INT_PTR)ShellExecuteW(NULL,L"open",path,NULL,NULL,SW_SHOWNORMAL)<=32)symbolMessage(r,"无法打开恢复目录。完整路径已显示在下方，可复制后手动打开。");free(path);}
 else symbolCancel(a,0);
 xideActionsRefresh(a);
}
static void symbolClose(xui_widget w,void* user){(void)w;symbolCancel(user,0);xideActionsRefresh(user);}
static int createSymbolRename(xide_language_view* v)
{
 if(v->renameSymbol)return XUI_OK;
 xide_app* a=v->app;xide_symbol_rename_view* r=calloc(1,sizeof(*r));if(!r)return XUI_ERROR_OUT_OF_MEMORY;v->renameSymbol=r;
#define SR_TRY(call) do{int code_=(call);if(code_!=XUI_OK)return code_;}while(0)
 SR_TRY(xideDialogCreate(a,&r->window,"重命名符号",740,450,12));SR_TRY(xuiWidgetAddChild(a->root,r->window));SR_TRY(xideDialogSetCancel(r->window,symbolClose,a));r->content=xideDialogGetContentWidget(r->window);SR_TRY(xideDialogFormBegin(r->window,440,290));
 xui_input_desc_t input={.iSize=sizeof(input),.pFont=a->uiFont};SR_TRY(xuiInputCreate(a->context,&r->controls[XIDE_SYMBOL_NAME],&input));SR_TRY(xuiWidgetSetAccessibleName(r->controls[XIDE_SYMBOL_NAME],"符号的新名称"));SR_TRY(xuiInputSetPlaceholder(r->controls[XIDE_SYMBOL_NAME],"输入新名称，然后生成预览"));
 xui_button_desc_t button={.iSize=sizeof(button),.pFont=a->uiFont};const int ids[]={XIDE_SYMBOL_PREVIEW,XIDE_SYMBOL_APPLY,XIDE_SYMBOL_CANCEL,XIDE_SYMBOL_RECOVERY};const char* titles[]={"生成预览","确认重命名","取消","打开恢复目录"};
 for(size_t i=0;i<4;++i){button.sText=titles[i];SR_TRY(xuiButtonCreate(a->context,&r->controls[ids[i]],&button));SR_TRY(xuiButtonSetClick(r->controls[ids[i]],symbolClicked,a));}
 SR_TRY(xideDialogFormField(r->window,"新名称",r->controls[XIDE_SYMBOL_NAME],r->controls[XIDE_SYMBOL_PREVIEW],NULL));
 xui_label_desc_t label={.iSize=sizeof(label),.pFont=a->uiFont,.sText="",.iWrapMode=XUI_TEXT_WRAP_WORD};SR_TRY(xuiLabelCreate(a->context,&r->message,&label));SR_TRY(xideDialogFormBlock(r->window,r->message,0,0));
 xui_tree_view_desc_t tree={.iSize=sizeof(tree),.pFont=a->uiFont,.fItemHeight=(float)xideUiHeight(a,28),.fPadding=(float)xideUiHeight(a,4),.fIndent=(float)xideUiHeight(a,12)};SR_TRY(xuiTreeViewCreate(a->context,&r->controls[XIDE_SYMBOL_FILES],&tree));SR_TRY(xuiWidgetSetAccessibleName(r->controls[XIDE_SYMBOL_FILES],"重命名影响的全部文件"));SR_TRY(xuiTreeViewSetSelect(r->controls[XIDE_SYMBOL_FILES],symbolSelected,a));SR_TRY(xideDialogFormBlock(r->window,r->controls[XIDE_SYMBOL_FILES],220,1));
 SR_TRY(xuiInputCreate(a->context,&r->controls[XIDE_SYMBOL_PATH],&input));SR_TRY(xuiInputSetReadonly(r->controls[XIDE_SYMBOL_PATH],1));SR_TRY(xuiWidgetSetAccessibleName(r->controls[XIDE_SYMBOL_PATH],"完整文件或恢复目录路径"));SR_TRY(xideDialogFormField(r->window,"完整路径",r->controls[XIDE_SYMBOL_PATH],NULL,NULL));
 const xui_widget buttons[]={r->controls[XIDE_SYMBOL_RECOVERY],r->controls[XIDE_SYMBOL_APPLY],r->controls[XIDE_SYMBOL_CANCEL]};SR_TRY(xideDialogFormActions(r->window,buttons,3));SR_TRY(xideDialogFormEnd(r->window));SR_TRY(xideDialogSetFixedBody(r->window,0));symbolControls(r);return XUI_OK;
#undef SR_TRY
}
static void symbolCreateFailed(xide_language_view* v)
{
 xide_symbol_rename_view* r=v->renameSymbol;if(!r)return;
 xui_widget all[XIDE_SYMBOL_CONTROL_COUNT+1];memcpy(all,r->controls,sizeof(r->controls));all[XIDE_SYMBOL_CONTROL_COUNT]=r->message;
 for(size_t i=0;i<XIDE_SYMBOL_CONTROL_COUNT+1;++i)if(all[i]){xui_widget parent=all[i];while(parent&&parent!=r->window)parent=xuiWidgetGetParent(parent);if(!parent)xuiWidgetDestroy(all[i]);}
 if(r->window)xideDialogDestroy(r->window);
 destroySymbolRename(v);if(v->app->statusBar)xuiStatusBarSetItemText(v->app->statusBar,1,"无法创建重命名窗口；原文件未修改，请稍后重试。");
}
int xideSymbolRenameCanOpen(xide_app* a)
{
 xide_language_view* v=a?a->languageView:NULL;xide_symbol_rename_view* r=symbolView(a);if(r&&(r->phase==XIDE_SYMBOL_APPLYING||r->phase==XIDE_SYMBOL_CANCELLING||r->phase==XIDE_SYMBOL_UNCERTAIN))return 1;
 xide_document* d=a?a->activeDocument:NULL;xide_edit_controller_status s={0};
 return v&&v->initialized&&v->renameSupported&&!v->sourceToken&&!v->sourceDrain&&!v->editManaged&&!v->editJob.owner&&!nativeEditBusy(v->nativeEdit)&&d&&d->open&&d->path&&d->revision&&!d->externalState&&!strcmp(d->language,"xlang")&&xidePathHasExtension(d->path,".xl")&&!xuiCodeEditIsReadonly(d->editor)&&xuiCodeSelectionGetCount(xuiCodeEditGetSelection(d->editor))==1&&xideEditControllerStatus(v->editController,&s)&&s.state==XIDE_EDIT_IDLE&&!s.active&&!s.paused;
}
int xideSymbolRenameOpen(xide_app* a)
{
 if(!xideSymbolRenameCanOpen(a))return 0;
 xide_language_view* v=a->languageView;if(createSymbolRename(v)!=XUI_OK){symbolCreateFailed(v);return 0;}xide_symbol_rename_view* r=v->renameSymbol;
 if(r->phase==XIDE_SYMBOL_APPLYING||r->phase==XIDE_SYMBOL_CANCELLING||r->phase==XIDE_SYMBOL_UNCERTAIN){symbolOpenWindow(r,0);return 1;}
 if(xideDialogIsOpen(r->window))return 1;
 xide_document* d=a->activeDocument;xui_code_selection_t selection={0};if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)!=XUI_OK)return 0;
 char* origin=copy(d->path);if(!origin)return 0;symbolEndPrepare(v,1);symbolInputsFree(r);r->prepared=0;free(r->origin);r->origin=origin;free(r->recovery);r->recovery=NULL;symbolRowsFree(r);xuiTreeViewSetNodes(r->controls[XIDE_SYMBOL_FILES],NULL,0);r->document=d->id;r->revision=d->revision;r->caret=selection.iCaretOffset;r->token=0;r->phase=XIDE_SYMBOL_INPUT;r->closeWanted=r->quitPending=0;xuiEditSetText(r->controls[XIDE_SYMBOL_NAME],"");xuiEditSetText(r->controls[XIDE_SYMBOL_PATH],origin);
 if(v->prepareSupported){
  r->phase=XIDE_SYMBOL_PREPARING;r->prepareStage=1;r->prepareDeadline=GetTickCount64()+5000;
  if(!symbolInputsCapture(v)){symbolInputsFree(r);r->prepareStage=0;r->phase=XIDE_SYMBOL_RESULT;symbolMessage(r,"无法准备当前输入；请处理文件冲突后重新选择符号。");}
  else symbolMessage(r,"正在查询符号名称… 可继续编辑或取消。");
  symbolControls(r);symbolOpenWindow(r,0);xuiSetFocusWidget(a->context,r->controls[XIDE_SYMBOL_CANCEL]);xideActionsRefresh(a);xgeRenderRequest();return 1;
 }
 symbolMessage(r,"修改光标处符号及其引用。先生成完整预览，再确认；预览阶段不会写入文件。");symbolControls(r);symbolOpenWindow(r,1);xuiSetFocusWidget(a->context,r->controls[XIDE_SYMBOL_NAME]);xideActionsRefresh(a);return 1;
}
static void symbolPrepareFailed(xide_language_view* v,const char* message)
{xide_symbol_rename_view* r=v->renameSymbol;symbolEndPrepare(v,1);symbolInputsFree(r);r->prepared=0;r->phase=XIDE_SYMBOL_RESULT;symbolMessage(r,message);symbolControls(r);}
static int symbolSendPrepare(xide_language_view* v)
{
 xide_symbol_rename_view* r=v->renameSymbol;xide_document* d=symbolDocument(v->app,r);const char* uri=NULL;int line=0,character=0;
 for(size_t i=0;i<v->inputCount;++i)if(v->inputs[i].id==r->document && v->inputs[i].revision==r->revision)uri=v->inputs[i].uri;
 if(!uri || !d || !symbolPrepareCurrent(v->app) || !xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(d->editor),r->caret,&line,&character))return 0;
 r->prepareAt=(xide_lsp_position){.line=line,.character=character,.offset=r->caret};
 r->prepareToken=xideRenamePrepareArm(v->renamePrepare,r->prepareId,sizeof(r->prepareId));if(!r->prepareToken)return 0;
 xvalue *m=xrtValueObject(),*p=xrtValueObject(),*td=xrtValueObject(),*position=xrtValueObject();int okay=m&&p&&td&&position;
 if(okay)okay=put(td,"uri",xrtValueString(sv(uri)))&&put(position,"line",xrtValueInt(line))&&put(position,"character",xrtValueInt(character));
 if(okay){okay=put(p,"textDocument",td);td=NULL;}if(okay){okay=put(p,"position",position);position=NULL;}
 if(okay)okay=put(m,"jsonrpc",xrtValueString(sv("2.0")))&&put(m,"id",xrtValueString(sv(r->prepareId)))&&put(m,"method",xrtValueString(sv("textDocument/prepareRename")));
 if(okay){okay=put(m,"params",p);p=NULL;}
 xide_model_error error={0};if(okay)okay=xideLanguageClientSend(v->client,m,&error);
 xrtValueRelease(m);xrtValueRelease(p);xrtValueRelease(td);xrtValueRelease(position);
 if(!okay){xideRenamePrepareCancel(v->renamePrepare);r->prepareToken=0;*r->prepareId=0;return 0;}
 r->prepareStage=4;r->prepareDeadline=0;return 1;
}
static int symbolPreparedMatches(xide_document* d,xide_symbol_rename_view* r,const xide_rename_prepared* ready)
{
 int start=0,end=0,length=0;xstrview name={0};xui_code_document document=xuiCodeEditGetDocument(d->editor);
 if(!xrtValueGetString(ready->placeholder,&name) || !xideRenamePreparedSpan(ready,r->prepareAt,r->caret,xuiCodeDocumentGetLength(document),&start,&end))return 0;
 char* text=malloc(name.Size+1);if(!text)return 0;
 int okay=xuiCodeDocumentCopyRange(document,start,end,text,(int)name.Size+1,&length)==XUI_OK && length==(int)name.Size && !memcmp(text,name.Data,name.Size);free(text);return okay;
}
static void pollSymbolPrepare(xide_language_view* v)
{
 xide_symbol_rename_view* r=v->renameSymbol;xide_app* a=v->app;
 if(!v->prepareSupported || !symbolPrepareCurrent(a)){symbolPrepareFailed(v,"文档、光标、连接或分析范围已改变，请重新选择符号。");return;}
 if(r->prepareStage==1 && !v->busy && !v->syncMode && xideFileSyncSettled(a)){
  r->prepareWatch=xideFileSyncRevision(a);
  xide_document* d=symbolDocument(a,r);char* path=d?copy(d->path):NULL;
  if(!path){symbolPrepareFailed(v,"无法准备当前文档，请稍后重试。");return;}
  cancelQuery(v,v->receiptRequest);v->receiptRequest=v->receiptReady=0;
  free(v->path);v->path=path;v->document=d;v->documentId=d->id;v->documentRevision=d->revision;v->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
  v->snapshotMode=v->onDemandMode=1;v->invalidated=v->automatic=0;v->recheckAt=v->semanticRefreshAt=0;
  if(v->autoEnabled&&v->autoView)v->autoView->syncAdopted=0;
  if(!canSnapshot(a,symbolDocument(a,r)) || !captureInputs(a) || !beginSyncPlan(a,7)){symbolPrepareFailed(v,"无法同步当前输入，请重新选择符号。");return;}r->prepareStage=2;
 }
 if(r->prepareStage==3 && !v->busy && !v->syncMode && v->receiptReady && !v->receiptRequest && !automaticWatchPending(v) && !symbolSendPrepare(v)){symbolPrepareFailed(v,"无法发送重命名准备请求，请检查连接后重试。");return;}
 if(r->prepareStage<4 && GetTickCount64()>=r->prepareDeadline){symbolPrepareFailed(v,"同步当前输入超时；可以关闭窗口后重试。");return;}
 xide_rename_prepared ready={0};
 if(xideRenamePrepareTake(v->renamePrepare,&ready)){
  xide_document* d=symbolDocument(a,r);
  if(ready.token!=r->prepareToken || !d || !symbolPrepareCurrent(a) || !symbolPreparedMatches(d,r,&ready)){
   symbolPrepareFailed(v,ready.error[0]?ready.error:"准备结果与当前源码不匹配；未修改名称或源码。");
  }else {
   xstrview name={0};xrtValueGetString(ready.placeholder,&name);xui_widget focus=xuiGetFocusWidget(a->context);int ownsFocus=0;
   for(xui_widget at=focus;at;at=xuiWidgetGetParent(at))if(at==r->window){ownsFocus=1;break;}
   symbolEndPrepare(v,0);r->prepared=1;r->phase=XIDE_SYMBOL_INPUT;
   if(xuiEditSetText(r->controls[XIDE_SYMBOL_NAME],name.Data)!=XUI_OK){symbolPrepareFailed(v,"无法填入名称，请关闭窗口后重试。");}
   else {symbolMessage(r,"已填入当前名称。输入新名称并预览全部修改，再确认。");symbolControls(r);if(ownsFocus){symbolOpenWindow(r,1);xuiSetFocusWidget(a->context,r->controls[XIDE_SYMBOL_NAME]);xuiEditSelectAll(r->controls[XIDE_SYMBOL_NAME]);}}
  }
  xideRenamePreparedClear(&ready);
 }
 if(r->phase==XIDE_SYMBOL_PREPARING)xgeRenderRequestAfter(.02f);
}
static int symbolRows(xide_symbol_rename_view* r,const xide_workspace_edit* plan)
{
 xide_workspace_edit_info info={0};if(!xideWorkspaceEditInfo(plan,&info)||!info.documents||info.documents>512)return 0;
 char** labels=calloc(info.documents,sizeof(*labels));xui_tree_view_node_t nodes[512];if(!labels)return 0;size_t bytes=0,open=0,closed=0,i=0;int okay=1;
 for(;okay&&i<info.documents;++i){xide_workspace_edit_document d={0};if(!xideWorkspaceEditDocumentAt(plan,i,&d)){okay=0;break;}size_t length=strlen(d.path);if(length>1024u*1024u-bytes||length+100>1024u*1024u-bytes){okay=0;break;}bytes+=length+100;labels[i]=malloc(length+100);if(!labels[i]){okay=0;break;}snprintf(labels[i],length+100,"%s  ·  %zu 处  ·  %s",d.hasVersion?"编辑器中修改":"写入磁盘",d.count,d.path);nodes[i]=(xui_tree_view_node_t){.iId=(int)i+1,.iParent=-1,.sText=labels[i],.bEnabled=1};if(d.hasVersion)++open;else ++closed;}
 if(okay)okay=xuiTreeViewSetNodes(r->controls[XIDE_SYMBOL_FILES],nodes,(int)info.documents)==XUI_OK;
 if(!okay){for(size_t j=0;j<info.documents;++j)free(labels[j]);free(labels);return 0;}
 symbolRowsFree(r);r->rows=labels;r->count=info.documents;r->edits=info.edits;r->open=open;r->closed=closed;return 1;
}
static void pollSymbolRename(xide_language_view* v)
{
 xide_symbol_rename_view* r=v->renameSymbol;if(!r)return;int before=r->phase;xide_app* a=v->app;
 if(r->phase==XIDE_SYMBOL_PREPARING)pollSymbolPrepare(v);
 if(r->phase==XIDE_SYMBOL_INPUT && r->prepared && !symbolPrepareCurrent(a)){symbolEndPrepare(v,1);r->prepared=0;r->phase=XIDE_SYMBOL_RESULT;symbolMessage(r,"文档、光标或分析范围已改变，请关闭窗口后重新选择符号。");symbolControls(r);}
 if(r->phase==XIDE_SYMBOL_PLANNING){
  const xide_edit_candidate* candidate=xideLanguageEditCandidate(a);
  if(candidate){xide_model_error e={0};r->token=xideLanguageEditApplyBeginManaged(a,0,&e);if(r->token&&symbolRows(r,xideLanguageEditApplyPlan(a,r->token))){r->phase=XIDE_SYMBOL_CONFIRM;char text[512];snprintf(text,sizeof(text),"将修改 %zu 个文件中的 %zu 处位置。%zu 个已打开文件保留为未保存编辑；%zu 个未打开文件会写入磁盘。确认前请核对下面的完整文件列表。",r->count,r->edits,r->open,r->closed);symbolMessage(r,text);symbolControls(r);}else {if(r->token)xideLanguageEditApplyCancel(a,r->token);else xideLanguageEditCandidateEnd(a);r->phase=XIDE_SYMBOL_CANCELLING;r->closeWanted=0;symbolMessage(r,e.message[0]?e.message:"无法准备完整预览，正在取消；未执行写入。");symbolControls(r);}}
  else if(!v->editManaged){r->phase=XIDE_SYMBOL_RESULT;symbolMessage(r,v->message[0]?v->message:"无法获得完整重命名候选；请检查新名称和当前符号。未写入文件。");symbolControls(r);}
  else xgeRenderRequestAfter(.02f);
 }
 if(r->phase==XIDE_SYMBOL_CONFIRM&&!xideLanguageEditApplyPlan(a,r->token)){xideLanguageEditApplyCancel(a,r->token);r->phase=XIDE_SYMBOL_CANCELLING;r->closeWanted=0;symbolMessage(r,"输入或工程状态已改变，正在撤销预览；未执行写入。");symbolControls(r);}
 if(r->phase==XIDE_SYMBOL_APPLYING||r->phase==XIDE_SYMBOL_CANCELLING){
  xide_native_apply_status s={0};xideLanguageEditApplyStatus(a,&s);
  if(r->token&&s.token==r->token&&s.phase==XIDE_APPLY_RECOVERY){r->phase=XIDE_SYMBOL_UNCERTAIN;r->quitPending=0;free(r->recovery);r->recovery=s.directory?copy(s.directory):NULL;xuiEditSetText(r->controls[XIDE_SYMBOL_PATH],r->recovery?r->recovery:"");char text[1536];snprintf(text,sizeof(text),"本次操作需要人工恢复，源码文件操作已暂停。%.350s\n恢复资料保持原样，不会自动重放。请先复制需要保留的未保存编辑，再查看恢复目录；退出会再次确认放弃未保存修改。",s.error.message);symbolMessage(r,text);symbolControls(r);symbolOpenWindow(r,0);}
  else {xide_edit_controller_status c={0};int done=r->token?s.token==r->token&&!s.active:xideEditControllerStatus(v->editController,&c)&&c.state==XIDE_EDIT_IDLE&&!c.active&&!v->editManaged&&!v->editJob.owner;
   if(done){r->phase=XIDE_SYMBOL_RESULT;char text[1536];if(s.transaction.committed)snprintf(text,sizeof(text),"重命名完成：%zu 个文件、%zu 处位置。已打开文档保留为未保存编辑，可在编辑器中撤销；未打开文件已写入磁盘。",r->count,r->edits);else snprintf(text,sizeof(text),"本次修改未提交。%s %.400s",s.transaction.rolledBack?"已恢复本次未提交的文件写入。":"",s.error.message);symbolMessage(r,text);symbolControls(r);int quit=r->quitPending;r->quitPending=0;if(r->closeWanted){xideDialogSetOpen(r->window,0);r->phase=XIDE_SYMBOL_CLOSED;}if(quit)xgeRequestQuit();}
   else xgeRenderRequestAfter(.02f);
  }
 }
 if(r->phase!=before)xideActionsRefresh(a);
}
int xideSymbolRenameIsModal(xide_app* a){xide_symbol_rename_view* r=symbolView(a);return r&&xideDialogIsModal(r->window);}
int xideSymbolRenameAllowsAction(xide_app* a,int id)
{
 if(!a||!a->languageView||!nativeEditHeld(a->languageView))return 1;
 if(id==XIDE_ACTION_RENAME_SYMBOL||id==XIDE_ACTION_STOP||id==XIDE_ACTION_STOP_ALL)return 1;
 switch(id){case XIDE_ACTION_EDITOR_UNDO:case XIDE_ACTION_EDITOR_REDO:case XIDE_ACTION_EDITOR_CUT:case XIDE_ACTION_EDITOR_COPY:case XIDE_ACTION_EDITOR_PASTE:case XIDE_ACTION_EDITOR_SELECT_ALL:case XIDE_ACTION_EDITOR_INDENT:case XIDE_ACTION_EDITOR_OUTDENT:case XIDE_ACTION_EDITOR_COMMENT:case XIDE_ACTION_EDITOR_FIND:case XIDE_ACTION_EDITOR_REPLACE:case XIDE_ACTION_EDITOR_GOTO:return 1;default:return xideWorkbenchWindowForAction(id)>=0;}
}
int xideSymbolRenameBeforeQuit(xide_app* a)
{
 xide_symbol_rename_view* r=symbolView(a);xide_native_apply_status s={0};xideLanguageEditApplyStatus(a,&s);
 if(s.phase==XIDE_APPLY_RECOVERY){
  if(!r||!r->recovery)return 0;
  xideMessageBeginQuit(a);uint64_t key=xideMessageWorkspaceKey(a,98)^s.token;for(size_t i=0;i<a->documents.count;++i){xide_document* d=a->documents.items[i];if(d->open)key=(key^xideMessageDocumentKey(d,98,0)^d->revision)*UINT64_C(1099511628211);}
  int answer=xideMessageConfirm(a,"退出并保留恢复资料","文件状态尚未核实，恢复资料会保留。退出将放弃所有未保存的文档及工程修改；恢复资料不包含此后继续输入的全部正文。请先复制需要保留的内容。确定退出？",XUI_MSGBOX_BUTTON_OK_CANCEL,key);xideMessageEndOperation(a);
  return answer==XUI_MSGBOX_RESULT_OK?2:0;
 }
 if(r&&(r->phase==XIDE_SYMBOL_PREPARING||r->phase==XIDE_SYMBOL_INPUT||r->phase==XIDE_SYMBOL_RESULT)){symbolCancel(a,0);return 1;}
 if(r&&(r->phase==XIDE_SYMBOL_PLANNING||r->phase==XIDE_SYMBOL_CONFIRM||r->phase==XIDE_SYMBOL_APPLYING||r->phase==XIDE_SYMBOL_CANCELLING)){symbolCancel(a,1);return 0;}
 if(s.active){xideLanguageEditApplyCancel(a,s.token);return 0;}
 return 1;
}
xui_widget xideSymbolRenameWindow(xide_app* a){xide_symbol_rename_view* r=symbolView(a);return r?r->window:NULL;}
xui_widget xideSymbolRenameControl(xide_app* a,int id){xide_symbol_rename_view* r=symbolView(a);return r&&id>=0&&id<XIDE_SYMBOL_CONTROL_COUNT?r->controls[id]:NULL;}
int xideSymbolRenamePhase(xide_app* a){xide_symbol_rename_view* r=symbolView(a);return r?r->phase:XIDE_SYMBOL_CLOSED;}
const char* xideSymbolRenameMessage(xide_app* a){xide_symbol_rename_view* r=symbolView(a);return r?r->text:"";}
