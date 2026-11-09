/* Shares the live client,catalog and background sync planner. No xide_app
 * layout change. Decoder owns all source verification and large retirement. */
struct xide_workspace_search_view {
 xui_widget window,input,tree,search,status,pathPreview;
 xide_workspace_search_decoder* decoder;xide_workspace_search_result* result;xide_language_batch* batch;
 document_binding inputs[SNAPSHOT_DOCS];size_t count,selected;
 uint64_t serial,token,session,watch,due,deadline;int supported,stage,request,usable,navigation,keepSelection,composing;
 char query[513],message[512];
 xui_widget_event_proc inputKey,inputIme,treeKey,treeDouble;void *inputKeyUser,*inputImeUser,*treeKeyUser,*treeDoubleUser;
};
static void workspaceSelected(xui_widget widget,int selected,void* user)
{
 (void)widget;xide_app* app=user;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;char display[1024]={0};
 if(r->usable && r->result && selected>0 && (size_t)selected<=r->result->count){const char* path=r->result->items[(size_t)selected-1].path;size_t n=strlen(path);if(n<sizeof(display))memcpy(display,path,n+1);else{size_t at=n-(sizeof(display)-8);while(((unsigned char)path[at]&0xc0u)==0x80u)++at;memcpy(display,"… ",4);memcpy(display+4,path+at,n-at+1);}}
 if(r->pathPreview)xuiEditSetText(r->pathPreview,display);
}
static void workspaceMessage(xide_language_view* v,const char* text)
{struct xide_workspace_search_view* r=v->workspaceSearch;snprintf(r->message,sizeof(r->message),"%s",text);xuiLabelSetText(r->status,r->message);xgeRenderRequest();}
static void workspaceRows(xide_language_view* v)
{
 struct xide_workspace_search_view* r=v->workspaceSearch;xui_tree_view_node_t rows[201];size_t count=r->usable && r->result?r->result->count:0;
 for(size_t i=0;i<count;++i)rows[i]=(xui_tree_view_node_t){.iId=(int)i+1,.iParent=-1,.sText=r->result->items[i].label,.bEnabled=1};
 if(!count)rows[0]=(xui_tree_view_node_t){.iId=0,.iParent=-1,.sText="输入名称搜索项目及打开文件中的符号。",.bEnabled=0};
 xuiTreeViewSetNodes(r->tree,rows,(int)(count?count:1));if(count)xuiTreeViewSetSelected(r->tree,1);workspaceSelected(r->tree,count?1:0,v->app);
}
static void workspaceInputsFree(struct xide_workspace_search_view* r)
{for(size_t i=0;i<r->count;++i)freeInput(r->inputs+i);r->count=0;}
static int workspaceEligible(xide_app* app)
{
 size_t count=0,bytes=0;for(size_t i=0;i<app->documents.count;++i){xide_document* d=app->documents.items[i];if(!namedXlang(d))continue;int n=xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor));if(snapshotConflict(d) || ++count>SNAPSHOT_DOCS || n<0 || (size_t)n>TEXT_LIMIT || (size_t)n>32u*1024u*1024u-bytes)return 0;bytes+=(size_t)n;}return 1;
}
static int workspaceInputsCapture(xide_app* app)
{
 struct xide_workspace_search_view* r=app->languageView->workspaceSearch;workspaceInputsFree(r);if(!workspaceEligible(app))return 0;
 for(size_t i=0;i<app->documents.count;++i){xide_document* d=app->documents.items[i];if(!namedXlang(d))continue;document_binding* b=r->inputs+r->count++;*b=(document_binding){.document=d,.id=d->id,.version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.externalState=d->externalState,.path=copy(d->path),.language=copy(d->language)};if(!b->path || !b->language)return 0;}
 r->watch=xideFileSyncRevision(app);r->session=app->languageView->highlightSession;return 1;
}
static int workspaceSearchInputsCurrent(xide_app* app)
{
 struct xide_workspace_search_view* r=app->languageView->workspaceSearch;if(!r || r->session!=app->languageView->highlightSession || r->watch!=xideFileSyncRevision(app))return 0;size_t count=0;
 for(size_t i=0;i<app->documents.count;++i)if(namedXlang(app->documents.items[i]))++count;
 if(count!=r->count)return 0;
 for(size_t i=0;i<r->count;++i){document_binding* b=r->inputs+i;xide_document* d=b->document;if(!namedXlang(d) || snapshotConflict(d) || d->id!=b->id || d->externalState!=b->externalState || strcmp(d->path,b->path) || strcmp(d->language,b->language) || xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))!=b->version)return 0;}return 1;
}
static void workspaceCancel(xide_language_view* v)
{struct xide_workspace_search_view* r=v?v->workspaceSearch:NULL;if(!r)return;cancelQuery(v,r->request);xideWorkspaceSearchDecoderCancel(r->decoder);if(v->syncMode==5)cancelSyncPlan(v);r->request=r->stage=r->usable=r->navigation=r->keepSelection=0;r->due=r->deadline=0;workspaceRows(v);}
static void resetWorkspaceSearch(xide_language_view* v)
{if(!v || !v->workspaceSearch)return;workspaceCancel(v);workspaceInputsFree(v->workspaceSearch);v->workspaceSearch->supported=0;if(xideLanguageViewWorkspaceSymbolsIsOpen(v->app))workspaceMessage(v,"分析配置或连接已更新，请重新搜索。");}
static void workspaceSearchDiskChanged(xide_language_view* v)
{if(!v || !v->workspaceSearch)return;workspaceCancel(v);if(xideLanguageViewWorkspaceSymbolsIsOpen(v->app))workspaceMessage(v,"工程文件已变化，请重新搜索。");}
static void destroyWorkspaceSearch(xide_language_view* v)
{struct xide_workspace_search_view* r=v->workspaceSearch;if(!r)return;xideWorkspaceSearchDecoderDestroy(r->decoder);xideWorkspaceSearchResultRelease(r->result);xideLanguageBatchDestroy(r->batch);workspaceInputsFree(r);free(r);v->workspaceSearch=NULL;}
static int workspaceSearchBusy(xide_language_view* v)
{struct xide_workspace_search_view* r=v?v->workspaceSearch:NULL;return r && (r->stage || r->request || r->due);}
static void workspaceSearchCapabilities(xide_language_view* v,const xvalue* cap)
{bool list=false,proof=false;if(v->workspaceSearch)v->workspaceSearch->supported=xideMetaBool(get(cap,"workspaceSymbolProvider"),&list) && list && xideMetaBool(get(get(cap,"experimental"),"workspaceSymbolInputProof"),&proof) && proof;}
static int workspaceSearchReceive(xvalue* message,size_t bytes,void* user)
{xide_language_view* v=user;if(v->workspaceSearch && xideWorkspaceSearchDecoderReceive(message,bytes,v->workspaceSearch->decoder))return 1;return v->references?xideReferenceDecoderReceive(message,bytes,v->references->decoder):0;}
static int takeWorkspaceSearch(xide_language_view* v,xvalue* message,size_t bytes)
{struct xide_workspace_search_view* r=v->workspaceSearch;int id=0;if(!r || !r->request || !number(get(message,"id"),&id) || id!=r->request)return 0;if(xideWorkspaceSearchDecoderSubmit(r->decoder,r->token,message,bytes))return 1;v->deferredMessage=message;v->deferredDiscard=0;v->deferredSession=v->highlightSession;return 1;}
static void workspaceSearchSynced(xide_language_view* v)
{if(v->workspaceSearch && v->workspaceSearch->stage==2)v->workspaceSearch->stage=3;}
static int workspaceBegin(xide_app* app,int navigation,size_t selected)
{
 xide_language_view* v=app->languageView;struct xide_workspace_search_view* r=v->workspaceSearch;if(!r || v->busy || !workspaceEligible(app))return 0;
 const char* query=xuiEditGetText(r->input);if(!query || strlen(query)>512 || !xrtUtf8Valid(xrtStrView(query),NULL)){workspaceCancel(v);workspaceMessage(v,"搜索内容最多 512 字节；请缩短查询。");return 0;}
 if(navigation){if(!r->usable || !r->result || selected>=r->result->count || !workspaceSearchInputsCurrent(app) || !v->client || !v->initialized)return 0;}
 else{const char* path=namedXlang(app->activeDocument)?app->activeDocument->path:app->workspaceModel.manifest;if(!path && app->workspaceModel.projectCount)path=app->workspaceModel.projects[0].manifest;if(!path)path=xideTaskViewCompiler(app);if(!connectClientPath(app,path)){workspaceMessage(v,"无法连接语言服务，请核对工具链设置。");return 0;}}
 workspaceCancel(v);r->selected=selected;r->navigation=navigation;r->keepSelection=navigation;memcpy(r->query,query,strlen(query)+1);if(r->serial==UINT64_MAX || !workspaceInputsCapture(app)){workspaceMessage(v,"无法准备当前输入，或输入超过分析资源限制。");return 0;}
 r->token=++r->serial;r->stage=1;r->deadline=GetTickCount64()+5000;workspaceMessage(v,navigation?"正在核对符号位置及来源…":"正在搜索工作空间符号…");return 1;
}
static int workspaceArm(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_workspace_search_view* r=v->workspaceSearch;const xvalue *selection=NULL,*text=NULL;int version=0;if(v->nextId==INT_MAX)return 0;
 if(r->navigation){if(!r->result || r->selected>=r->result->count)return 0;xide_workspace_search_item* item=r->result->items+r->selected;selection=item->symbol;for(size_t i=0;i<v->inputCount;++i)if(!xidePathNativeKeyCompare(item->key,v->inputs[i].key)){text=v->inputs[i].textValue;version=v->inputs[i].wireVersion;break;}}
 if(!xideWorkspaceSearchDecoderArm(r->decoder,r->token,v->nextId+1,r->query,selection,text,version))return 0;
 r->request=++v->nextId;r->keepSelection=0;r->stage=4;return 1;
}
static int workspaceApplyNavigation(xide_app* app,xide_workspace_search_result* incoming)
{
 xide_language_view* v=app->languageView;if(!workspaceSearchInputsCurrent(app) || incoming->count!=1)return 0;xide_workspace_search_item* item=incoming->items;document_binding* binding=NULL;
 for(size_t i=0;i<v->inputCount;++i)if(!xidePathNativeKeyCompare(item->key,v->inputs[i].key)){binding=v->inputs+i;break;}
 xide_document* target=openPath(app,item->path);int disk=incoming->snapshot.raw!=NULL;
 if(disk){if(binding || target || !diskIdentity(item->path,item->document))return 0;xide_model_error e={0};target=xideDocumentOpenPrepared(&app->documents,item->path,&incoming->snapshot,&e);if(!target)return 0;if(!diskIdentity(item->path,item->document)){xideDocumentClose(target,0,&e);return 0;}}
 else if(!binding || target!=binding->document || !target || target->id!=binding->id || snapshotConflict(target) || xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(target->editor))!=binding->version)return 0;
 if(item->start.offset<0 || item->end.offset> xuiCodeDocumentGetLength(xuiCodeEditGetDocument(target->editor)) || item->end.offset<=item->start.offset || xideEditorShow(app,target)!=XUI_OK || xideGotoRevealLine(target->editor,item->start.line)!=XUI_OK)return 0;
 if(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(target->editor),xuiCodeEditGetDocument(target->editor),item->start.offset,item->end.offset)!=XUI_OK)return 0;
 xuiCodeEditEnsureCaretVisible(target->editor);if(disk)xideHomeRemember(app,item->path);return 1;
}
static void workspaceClose(xui_widget widget,void* user)
{(void)widget;xide_app* app=user;if(!app || !app->languageView || !app->languageView->workspaceSearch)return;xide_language_view* v=app->languageView;workspaceCancel(v);workspaceInputsFree(v->workspaceSearch);xideDialogSetOpen(v->workspaceSearch->window,0);xideActionsRefresh(app);}
static void pollWorkspaceSearch(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_workspace_search_view* r=v->workspaceSearch;if(!r)return;
 if(r->batch && !r->stage && xideWorkspaceSearchDecoderRetireBatch(r->decoder,r->batch))r->batch=NULL;
 if(r->result && !r->usable && !r->keepSelection && xideWorkspaceSearchDecoderRetire(r->decoder,r->result))r->result=NULL;
 if(!xideDialogIsOpen(r->window))return;
 if(r->due && !r->composing && GetTickCount64()>=r->due){r->due=0;workspaceBegin(app,0,0);}
 if((r->stage || r->usable) && !workspaceSearchInputsCurrent(app)){workspaceCancel(v);workspaceMessage(v,"源码或工作空间状态已变化，请重新搜索。");}
 if(r->stage==1 && v->initialized && !v->waitingInitialize && !v->busy && !v->syncMode){
  if(!r->supported){workspaceCancel(v);workspaceMessage(v,"当前语言服务不支持工作空间符号及安全导航，请更新语言服务。");}
  else {if(v->autoEnabled && v->autoView)v->autoView->syncAdopted=0;if(!captureInputsMode(app,1) || !beginSyncPlan(app,5)){workspaceCancel(v);workspaceMessage(v,"无法同步当前输入，请重新搜索。");}else r->stage=2;}
 }
 if(r->stage==3 && !v->syncMode && !r->batch && !workspaceArm(app))xgeRenderRequestAfter(0.02f);
 if(r->stage==4){if(!r->batch)r->batch=xideWorkspaceSearchDecoderTakeBatch(r->decoder,r->token);if(r->batch && xideLanguageBatchSend(v->client,r->batch)){r->batch=NULL;r->stage=5;r->deadline=0;}else if(r->batch)xgeRenderRequestAfter(0.02f);}
 if(r->stage && r->stage!=5 && r->deadline && GetTickCount64()>=r->deadline){workspaceCancel(v);workspaceMessage(v,"准备搜索超时，请重试或检查语言服务。");}
 if(r->result && !r->usable && !r->keepSelection && xideWorkspaceSearchDecoderRetire(r->decoder,r->result))r->result=NULL;
 if(!r->result){xide_workspace_search_result* incoming=xideWorkspaceSearchDecoderTake(r->decoder);if(incoming){int valid=incoming->valid && incoming->token==r->token && workspaceSearchInputsCurrent(app) && r->stage;
   if(!valid){workspaceMessage(v,*incoming->error?incoming->error:"结果已过期，请重新搜索。");r->stage=r->request=0;r->usable=0;r->result=incoming;workspaceRows(v);}
   else if(incoming->navigation){r->stage=r->request=0;int jumped=workspaceApplyNavigation(app,incoming);r->result=incoming;r->usable=0;if(jumped){xui_widget target=app->activeDocument?app->activeDocument->editor:NULL;workspaceClose(NULL,app);if(target)xuiSetFocusWidget(app->context,target);}else workspaceMessage(v,"目标内容或位置已改变，未跳转；请重新搜索。");}
   else {r->stage=r->request=0;r->result=incoming;r->usable=1;workspaceRows(v);char message[160];snprintf(message,sizeof(message),"显示 %zu 项（最多 200）· Enter 或双击跳转 · 可继续输入缩小范围",incoming->count);workspaceMessage(v,message);}
  }}
 if(r->due && !r->composing){uint64_t now=GetTickCount64();xgeRenderRequestAfter(r->due>now?(float)(r->due-now)/1000.0f:0.001f);}
 if(r->stage && r->stage!=5 && r->deadline){uint64_t now=GetTickCount64();xgeRenderRequestAfter(r->deadline>now?(float)(r->deadline-now)/1000.0f:0.001f);}
}
static int workspaceInputEvent(xui_widget widget,const xui_edit_event_t* event,void* user)
{(void)widget;xide_app* app=user;xide_language_view* v=app->languageView;struct xide_workspace_search_view* r=v->workspaceSearch;if(event->iType==XUI_EDIT_EVENT_COMPOSITION_CHANGED && !event->iTextSize && !event->iCompositionStart && !event->iCompositionEnd)r->composing=0;if(event->iType==XUI_EDIT_EVENT_TEXT_CHANGED && xideDialogIsOpen(r->window)){workspaceCancel(v);r->due=GetTickCount64()+150;workspaceMessage(v,"正在准备搜索…");xgeRenderRequestAfter(0.15f);}return XUI_OK;}
static int workspaceKey(xui_widget widget,const xui_event_t* event,void* user)
{
 xide_app* app=user;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;int input=widget==r->input;
 if(event->iPhase==XUI_EVENT_PHASE_TARGET && !(input && r->composing)){
  if(event->iKey==XUI_KEY_ESCAPE){workspaceClose(widget,app);return XUI_EVENT_DISPATCH_STOP;}
  if(event->iKey==XUI_KEY_ENTER && !event->iModifiers){if(r->usable && r->result && r->result->count){int selected=xuiTreeViewGetSelected(r->tree);xideLanguageViewWorkspaceSymbolJump(app,selected>0?(size_t)selected-1:0);}else workspaceBegin(app,0,0);return XUI_EVENT_DISPATCH_STOP;}
  if(input && event->iKey==XUI_KEY_DOWN && !event->iModifiers && r->usable){xuiSetFocusWidget(app->context,r->tree);return XUI_EVENT_DISPATCH_STOP;}
 }
 xui_widget_event_proc original=input?r->inputKey:r->treeKey;void* originalUser=input?r->inputKeyUser:r->treeKeyUser;return original?original(widget,event,originalUser):XUI_OK;
}
static int workspaceIme(xui_widget widget,const xui_event_t* event,void* user)
{xide_app* app=user;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;int result=r->inputIme?r->inputIme(widget,event,r->inputImeUser):XUI_OK;if(event->iPhase==XUI_EVENT_PHASE_TARGET)r->composing=event->bCompositionActive;return result;}
static int workspaceDouble(xui_widget widget,const xui_event_t* event,void* user)
{xide_app* app=user;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;if(event->iPhase==XUI_EVENT_PHASE_TARGET && event->iType==XUI_EVENT_POINTER_DOUBLE_CLICK){int selected=xuiTreeViewGetSelected(widget);if(selected>0 && xideLanguageViewWorkspaceSymbolJump(app,(size_t)selected-1))return XUI_EVENT_DISPATCH_STOP;}return r->treeDouble?r->treeDouble(widget,event,r->treeDoubleUser):XUI_OK;}
static void workspaceClicked(xui_widget widget,void* user){(void)widget;workspaceBegin(user,0,0);}
static int createWorkspaceSearch(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_workspace_search_view* r=calloc(1,sizeof(*r));if(!r)return XUI_ERROR_OUT_OF_MEMORY;v->workspaceSearch=r;r->decoder=xideWorkspaceSearchDecoderCreate(languageWake,app);if(!r->decoder)return XUI_ERROR_OUT_OF_MEMORY;
#define WS_TRY(call) do{int code=(call);if(code!=XUI_OK)return code;}while(0)
 WS_TRY(xideDialogCreate(app,&r->window,"搜索工作空间符号",760,480,12));WS_TRY(xuiWidgetAddChild(app->root,r->window));WS_TRY(xideDialogSetCancel(r->window,workspaceClose,app));WS_TRY(xideDialogFormBegin(r->window,460,260));
 xui_input_desc_t input={.iSize=sizeof(input),.pFont=app->uiFont};WS_TRY(xuiInputCreate(app->context,&r->input,&input));WS_TRY(xuiInputSetPlaceholder(r->input,"输入符号名称；支持多个词项"));WS_TRY(xuiWidgetSetAccessibleName(r->input,"工作空间符号查询"));
 xui_button_desc_t button={.iSize=sizeof(button),.pFont=app->uiFont,.sText="搜索"};WS_TRY(xuiButtonCreate(app->context,&r->search,&button));WS_TRY(xuiButtonSetClick(r->search,workspaceClicked,app));WS_TRY(xideDialogFormField(r->window,"符号",r->input,r->search,NULL));
 xui_label_desc_t label={.iSize=sizeof(label),.pFont=app->uiFont,.sText="搜索已配置的项目及打开的 xlang 文件。",.iWrapMode=XUI_TEXT_WRAP_WORD};WS_TRY(xuiLabelCreate(app->context,&r->status,&label));WS_TRY(xideDialogFormBlock(r->window,r->status,0,0));
 xui_tree_view_desc_t tree={.iSize=sizeof(tree),.pFont=app->uiFont,.fItemHeight=28,.fPadding=4,.fIndent=12};WS_TRY(xuiTreeViewCreate(app->context,&r->tree,&tree));WS_TRY(xuiWidgetSetAccessibleName(r->tree,"工作空间符号结果"));WS_TRY(xuiTreeViewSetSelect(r->tree,workspaceSelected,app));WS_TRY(xideDialogFormBlock(r->window,r->tree,300,1));
 WS_TRY(xuiInputCreate(app->context,&r->pathPreview,&input));WS_TRY(xuiInputSetReadonly(r->pathPreview,1));WS_TRY(xuiWidgetSetAccessibleName(r->pathPreview,"所选符号文件路径"));WS_TRY(xideDialogFormField(r->window,"文件",r->pathPreview,NULL,NULL));WS_TRY(xideDialogFormEnd(r->window));WS_TRY(xideDialogSetFixedBody(r->window,1));
 WS_TRY(xuiWidgetGetEventHandler(r->input,XUI_EVENT_KEY_DOWN,&r->inputKey,&r->inputKeyUser));WS_TRY(xuiWidgetSetEventHandler(r->input,XUI_EVENT_KEY_DOWN,workspaceKey,app));WS_TRY(xuiWidgetGetEventHandler(r->input,XUI_EVENT_IME_COMPOSITION,&r->inputIme,&r->inputImeUser));WS_TRY(xuiWidgetSetEventHandler(r->input,XUI_EVENT_IME_COMPOSITION,workspaceIme,app));WS_TRY(xuiEditSetEvent(r->input,workspaceInputEvent,app));
 WS_TRY(xuiWidgetGetEventHandler(r->tree,XUI_EVENT_KEY_DOWN,&r->treeKey,&r->treeKeyUser));WS_TRY(xuiWidgetSetEventHandler(r->tree,XUI_EVENT_KEY_DOWN,workspaceKey,app));WS_TRY(xuiWidgetGetEventHandler(r->tree,XUI_EVENT_POINTER_DOUBLE_CLICK,&r->treeDouble,&r->treeDoubleUser));WS_TRY(xuiWidgetSetEventHandler(r->tree,XUI_EVENT_POINTER_DOUBLE_CLICK,workspaceDouble,app));workspaceRows(v);return XUI_OK;
#undef WS_TRY
}
int xideLanguageViewCanSearchWorkspace(xide_app* app){return app && app->dock && app->languageView && !app->languageView->busy && xideTaskViewCompiler(app)!=NULL;}
int xideLanguageViewWorkspaceSymbolsIsOpen(xide_app* app){return app && app->languageView && app->languageView->workspaceSearch && xideDialogIsOpen(app->languageView->workspaceSearch->window);}
int xideLanguageViewWorkspaceSymbolsOpen(xide_app* app)
{if(!xideLanguageViewCanSearchWorkspace(app) || xideActionsHasModal(app))return 0;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;xideDialogSetMaxSize(r->window,app->viewportWidth-32,app->viewportHeight-32);xideDialogSetOpen(r->window,1);xuiSetFocusWidget(app->context,r->input);xuiEditSelectAll(r->input);r->composing=0;r->due=GetTickCount64();xgeRenderRequest();xideActionsRefresh(app);return 1;}
size_t xideLanguageViewWorkspaceSymbolCount(xide_app* app){struct xide_workspace_search_view* r=app && app->languageView?app->languageView->workspaceSearch:NULL;return r && r->usable && r->result?r->result->count:0;}
int xideLanguageViewWorkspaceSymbolJump(xide_app* app,size_t index){return app && app->languageView && xideLanguageViewWorkspaceSymbolsIsOpen(app) && workspaceBegin(app,1,index);}
xui_widget xideLanguageViewWorkspaceSymbolsControl(xide_app* app,int control)
{struct xide_workspace_search_view* r=app && app->languageView?app->languageView->workspaceSearch:NULL;if(!r)return NULL;switch(control){case XIDE_WORKSPACE_SYMBOL_QUERY:return r->input;case XIDE_WORKSPACE_SYMBOL_RESULTS:return r->tree;case XIDE_WORKSPACE_SYMBOL_SEARCH:return r->search;case XIDE_WORKSPACE_SYMBOL_STATUS:return r->status;default:return NULL;}}
xui_widget xideLanguageViewWorkspaceSymbolsWindow(xide_app* app){return app && app->languageView && app->languageView->workspaceSearch?app->languageView->workspaceSearch->window:NULL;}
