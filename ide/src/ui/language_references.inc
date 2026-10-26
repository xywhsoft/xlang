/* Same client/sync/input contracts as semantic queries. Rows are decoded and
   retired off-owner;only a bounded200-row page reaches the XUI tree. */
#define REFERENCE_PAGE 200u
struct xide_reference_view {
 xui_widget panel,bar,tree,status,refresh,cancel,previous,next,include,clear;
 xide_reference_decoder* decoder;xide_reference_result* result;
 document_binding inputs[SNAPSHOT_DOCS];size_t count,page;
 xide_document* source;uint64_t sourceId,serial,token;uint32_t sourceVersion;
 int caret,supported,pending,request,decoding,usable,includeValue;
 char* jumpPath;wchar_t* jumpKey;xide_lsp_position jumpStart,jumpEnd;
 char message[768];
};
static void referenceMessage(xide_language_view* v,const char* text)
{struct xide_reference_view* r=v->references;snprintf(r->message,sizeof(r->message),"%s",text);xuiLabelSetText(r->status,r->message);xgeRenderRequest();}
static int referenceReady(xide_language_view* v)
{return v->initialized && v->snapshotMode && !v->busy && !v->syncMode && current(v) && (v->onDemandMode ? v->receiptReady : v->snapshotResult!=NULL);}
static int referenceReceiverInstall(xide_language_view* v)
{return v->references && xideLanguageClientSetReceiver(v->client,xideReferenceDecoderReceive,v->references->decoder);}
static void referenceInputsFree(struct xide_reference_view* r)
{for(size_t i=0;i<r->count;++i)freeInput(r->inputs+i);r->count=0;}
static int referenceInputsCapture(xide_app* app)
{
 struct xide_reference_view* r=app->languageView->references;referenceInputsFree(r);
 for(size_t i=0;i<app->documents.count;++i){xide_document* d=app->documents.items[i];if(!namedXlang(d))continue;if(r->count>=SNAPSHOT_DOCS)return 0;
  document_binding* b=r->inputs+r->count++;*b=(document_binding){.document=d,.id=d->id,.version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)),.externalState=d->externalState,.path=copy(d->path),.language=copy(d->language)};if(!b->path || !b->language)return 0;
 }return 1;
}
static int referenceInputsCurrent(xide_app* app)
{
 struct xide_reference_view* r=app->languageView->references;size_t count=0;for(size_t i=0;i<app->documents.count;++i)if(namedXlang(app->documents.items[i]))++count;if(count!=r->count)return 0;
 for(size_t i=0;i<r->count;++i){document_binding* b=r->inputs+i;xide_document* d=b->document;
  if(!namedXlang(d) || d->id!=b->id || d->externalState!=b->externalState || strcmp(d->path,b->path) || strcmp(d->language,b->language) || xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))!=b->version)return 0;
 }return r->source && r->source->open && r->source->id==r->sourceId && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(r->source->editor))==r->sourceVersion;
}
static void referenceJumpFree(struct xide_reference_view* r){free(r->jumpPath);free(r->jumpKey);r->jumpPath=NULL;r->jumpKey=NULL;}
static int referenceRetire(struct xide_reference_view* r)
{if(r->result && !xideReferenceDecoderRetire(r->decoder,r->result))return 0;r->result=NULL;return 1;}
static void referenceRows(xide_language_view* v)
{
 struct xide_reference_view* r=v->references;xui_tree_view_node_t nodes[REFERENCE_PAGE+1];size_t count=r->usable && r->result ? r->result->count : 0;
 if(r->page && r->page>=count)r->page=count ? ((count-1)/REFERENCE_PAGE)*REFERENCE_PAGE : 0;
 size_t shown=count-r->page;if(shown>REFERENCE_PAGE)shown=REFERENCE_PAGE;
 for(size_t i=0;i<shown;++i)nodes[i]=(xui_tree_view_node_t){.iId=(int)i+1,.iParent=-1,.sText=r->result->items[r->page+i].label,.bEnabled=1};
 if(!shown){nodes[0]=(xui_tree_view_node_t){.iId=0,.iParent=-1,.sText="在源码符号处按 Shift+F12 查找引用。",.bEnabled=0};shown=1;}
 /* Keep keyboard focus through Enter-triggered async work. Request/metadata
    gates reject another activation while pending. */
 xuiTreeViewSetNodes(r->tree,nodes,(int)shown);
 xuiWidgetSetEnabled(r->previous,count && r->page>0);xuiWidgetSetEnabled(r->next,count>r->page+REFERENCE_PAGE);
}
static void cancelReferences(xide_language_view* v)
{
 struct xide_reference_view* r=v ? v->references : NULL;if(!r)return;cancelQuery(v,r->request);r->pending=r->request=r->decoding=r->usable=0;xideReferenceDecoderCancel(r->decoder);referenceJumpFree(r);referenceInputsFree(r);r->source=NULL;referenceRows(v);referenceRetire(r);referenceMessage(v,"引用查询已取消或清除。");
}
static void resetReferences(xide_language_view* v){if(v && v->references){cancelReferences(v);v->references->supported=0;}}
static void destroyReferences(xide_language_view* v)
{struct xide_reference_view* r=v ? v->references : NULL;if(!r)return;xideReferenceDecoderDestroy(r->decoder);xideReferenceResultRelease(r->result);referenceInputsFree(r);referenceJumpFree(r);free(r);v->references=NULL;}
static int referencesBusy(xide_language_view* v)
{struct xide_reference_view* r=v ? v->references : NULL;return r && (r->pending || r->request || r->decoding || r->usable);}
static void referencesCapabilities(xide_language_view* v,const xvalue* cap)
{bool supported=false,proof=false;const xvalue* provider=get(cap,"referencesProvider");v->references->supported=((xrtValueGetBool(provider,&supported) && supported) || xrtValueType(provider)==XVALUE_OBJECT) && xrtValueGetBool(get(get(cap,"experimental"),"referenceInputProof"),&proof) && proof;}
int xideLanguageViewCanFindReferences(xide_app* app)
{xide_language_view* v=app ? app->languageView : NULL;return v && v->references && !v->busy && canSnapshot(app,app->activeDocument) && xideTaskViewCompiler(app) && (!v->initialized || (v->onDemandSupported && v->references->supported));}
static int referenceShow(xide_app* app)
{
 xui_dock_window_info_t info={.iSize=sizeof(info)};if(xuiDockPanelGetWindowInfo(app->dock,app->windows[XIDE_USAGES],&info)!=XUI_OK)return 0;
 if(info.iState==XUI_DOCK_PANEL_WINDOW_FLOATING)return 1;
 int pane=info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED ? info.iPane : -1;
 for(int i=XIDE_HELP;i<=XIDE_OUTLINE && pane<0;++i){xui_dock_window_info_t other={.iSize=sizeof(other)};if(xuiDockPanelGetWindowInfo(app->dock,app->windows[i],&other)==XUI_OK && other.iState==XUI_DOCK_PANEL_WINDOW_DOCKED)pane=other.iPane;}
 if(pane<0){int editor=app->activeDocument ? app->activeDocument->window : app->windows[XIDE_XLANG_DEMO];if(xuiDockPanelGetWindowInfo(app->dock,editor,&info)!=XUI_OK || info.iState!=XUI_DOCK_PANEL_WINDOW_DOCKED || xuiDockPanelDockWindowToPaneSide(app->dock,app->windows[XIDE_USAGES],info.iPane,XUI_DOCK_PANEL_SIDE_BOTTOM,.28f,&pane)!=XUI_OK)return 0;}
 else if(xuiDockPanelDockWindowToPane(app->dock,app->windows[XIDE_USAGES],pane)!=XUI_OK)return 0;
 return xuiDockPanelSetPaneActiveWindow(app->dock,pane,app->windows[XIDE_USAGES])==XUI_OK;
}
static int referenceSend(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_reference_view* r=v->references;if(!r->pending || !referenceReady(v) || v->nextId==INT_MAX)return 0;
 xvalue *params=queryParams(v,r->source,r->caret),*context=xrtValueObject();int okay=params && context && put(context,"includeDeclaration",xrtValueBool(r->includeValue!=0));if(okay){okay=put(params,"context",context);context=NULL;}xrtValueRelease(context);r->pending=0;r->request=++v->nextId;
 if(!okay || !xideReferenceDecoderArm(r->decoder,r->token,r->request)){xrtValueRelease(params);r->request=0;referenceMessage(v,"无法准备引用查询。");return 0;}
 if(!sendMessage(v,"textDocument/references",params,r->request)){r->request=0;xideReferenceDecoderCancel(r->decoder);referenceMessage(v,"无法发送引用查询。");return 0;}return 1;
}
static int referenceStart(xide_app* app,xide_document* source,int caret,int keep)
{
 xide_language_view* v=app->languageView;struct xide_reference_view* r=v->references;
 if(!source || !source->open || !canSnapshot(app,source) || v->busy || !prepareSymbolsDocument(app,source))return 0;
 cancelQuery(v,r->request);xideReferenceDecoderCancel(r->decoder);r->request=r->decoding=0;r->pending=1;r->source=source;r->sourceId=source->id;r->sourceVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(source->editor));r->caret=caret;r->includeValue=xuiCheckBoxGetChecked(r->include);r->token=++r->serial;
 if(!r->token || !referenceInputsCapture(app)){cancelReferences(v);return 0;}
 if(!keep){r->usable=0;r->page=0;referenceJumpFree(r);referenceRows(v);referenceRetire(r);}else referenceRows(v);referenceMessage(v,keep ? "正在核对引用位置…" : "正在查找引用…");referenceShow(app);if(referenceReady(v) && !referenceSend(app))return 0;xgeRenderRequest();return 1;
}
static int requestReferences(xide_app* app)
{if(!xideLanguageViewCanFindReferences(app))return 0;xui_code_selection_t caret={0};xide_document* d=app->activeDocument;return xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&caret)==XUI_OK && referenceStart(app,d,caret.iCaretOffset,0);}
size_t xideLanguageViewReferenceCount(xide_app* app)
{struct xide_reference_view* r=app && app->languageView ? app->languageView->references : NULL;return r && r->usable && r->result ? r->result->count : 0;}
int xideLanguageViewReferenceJump(xide_app* app,size_t index)
{
 xide_language_view* v=app ? app->languageView : NULL;struct xide_reference_view* r=v ? v->references : NULL;
 if(!r || !r->usable || !r->result || index>=r->result->count || r->pending || r->request || r->decoding || !referenceInputsCurrent(app))return 0;
 xide_reference_item* row=r->result->items+index;char* path=copy(row->path);size_t bytes=(wcslen(row->key)+1)*sizeof(wchar_t);wchar_t* key=malloc(bytes);if(!path || !key){free(path);free(key);return 0;}memcpy(key,row->key,bytes);xide_lsp_position start=row->start,end=row->end;
 if(!referenceStart(app,r->source,r->caret,1)){free(path);free(key);referenceMessage(v,"无法核对该引用，请重新查找。");return 0;}referenceJumpFree(r);r->jumpPath=path;r->jumpKey=key;r->jumpStart=start;r->jumpEnd=end;return 1;
}
static int takeReferences(xide_language_view* v,xvalue* message)
{
 struct xide_reference_view* r=v->references;int id=0;if(!r || !r->request || !number(get(message,"id"),&id) || id!=r->request)return 0;
 if(!referenceInputsCurrent(v->app) || !referenceReady(v)){cancelReferences(v);referenceMessage(v,"源码状态已更新，请重新查找引用。");return 0;}
 const xvalue* error=get(message,"error");if(error){const char* text=boundedString(get(error,"message"),512);r->request=0;r->usable=0;referenceJumpFree(r);referenceRows(v);referenceMessage(v,text ? text : "引用查询未完成。");return 0;}
 if(!xideReferenceDecoderSubmit(r->decoder,r->token,message)){v->deferredMessage=message;v->deferredDiscard=0;v->deferredSession=v->highlightSession;return 1;}
 r->request=0;r->decoding=1;return 1;
}
static void referenceSummary(xide_language_view* v)
{struct xide_reference_view* r=v->references;char text[256];size_t count=r->result ? r->result->count : 0;if(!count){referenceMessage(v,"没有找到当前位置的引用。");return;}snprintf(text,sizeof(text),"找到 %zu 处引用 · 第 %zu/%zu 页 · 双击或按 Enter 跳转",count,r->page/REFERENCE_PAGE+1,(count+REFERENCE_PAGE-1)/REFERENCE_PAGE);referenceMessage(v,text);}
static void pollReferences(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_reference_view* r=v->references;if(!r)return;
 if(!r->usable && r->result)referenceRetire(r);
 if((r->pending || r->request || r->decoding || r->usable) && !referenceInputsCurrent(app)){cancelReferences(v);referenceMessage(v,"源码已变化，请重新查找引用。");}
 if(r->pending && v->initialized && !v->waitingInitialize && !v->receiptRequest && !v->syncMode){
  if(!r->supported){cancelReferences(v);referenceMessage(v,"当前语言服务不支持查找引用，请更新语言服务。");}
  else if(v->invalidated){cancelReferences(v);referenceMessage(v,"源码状态已更新，请重新查找引用。");}
  else if(v->nextId==INT_MAX){reset(app);referenceMessage(v,"语言服务连接需要更新，请重新查找引用。");}
  else if(referenceReady(v) && v->nextId<INT_MAX){referenceSend(app);referenceRows(v);}
 }
 int ready=(r->request || r->decoding) && xideReferenceDecoderHasResult(r->decoder);
 if(ready && r->result && r->usable){r->usable=0;referenceRows(v);}
 if(ready && (!r->result || referenceRetire(r))){xide_reference_result* incoming=xideReferenceDecoderTake(r->decoder);if(incoming){r->request=r->decoding=0;r->result=incoming;r->usable=incoming->valid && incoming->token==r->token && referenceInputsCurrent(app) && referenceReady(v);referenceRows(v);
   if(!r->usable){const char* error=boundedString(get(get(incoming->message,"error"),"message"),512);referenceMessage(v,error ? error : "引用结果无效或已过期，请重新查找。");referenceJumpFree(r);}
   else {referenceSummary(v);if(r->jumpKey){size_t at=xideReferenceResultFind(incoming,r->jumpKey,r->jumpStart,r->jumpEnd);int jumped=at!=SIZE_MAX && jumpDocumentProof(app,incoming->items[at].path,incoming->items[at].range,incoming->items[at].document,referenceReady(v));referenceJumpFree(r);if(jumped){if(!referenceInputsCapture(app)){cancelReferences(v);}}else referenceMessage(v,"引用位置已改变，未跳转；列表已更新。");}}
  }}
 xuiWidgetSetEnabled(r->cancel,r->pending || r->request || r->decoding);
}
static int referenceEvent(xui_widget widget,const xui_event_t* event,void* user)
{xide_app* app=user;struct xide_reference_view* r=app->languageView->references;if(event->iType==XUI_EVENT_POINTER_DOUBLE_CLICK || (event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ENTER)){int selected=xuiTreeViewGetSelected(widget);if(selected>0 && xideLanguageViewReferenceJump(app,r->page+(size_t)selected-1))return XUI_EVENT_DISPATCH_STOP;}if(event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ESCAPE){cancelReferences(app->languageView);return XUI_EVENT_DISPATCH_STOP;}return XUI_OK;}
static void referenceClicked(xui_widget widget,void* user)
{
 xide_app* app=user;xide_language_view* v=app->languageView;struct xide_reference_view* r=v->references;
 if(widget==r->cancel || widget==r->clear)cancelReferences(v);
 else if(widget==r->previous && r->page>=REFERENCE_PAGE){r->page-=REFERENCE_PAGE;referenceRows(v);referenceSummary(v);}
 else if(widget==r->next && r->result && r->page+REFERENCE_PAGE<r->result->count){r->page+=REFERENCE_PAGE;referenceRows(v);referenceSummary(v);}
 else if(widget==r->refresh){if(r->source && r->source->open && r->source->id==r->sourceId && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(r->source->editor))==r->sourceVersion)referenceStart(app,r->source,r->caret,0);else requestReferences(app);}
}
static int createReferences(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_reference_view* r=calloc(1,sizeof(*r));if(!r)return XUI_ERROR_OUT_OF_MEMORY;v->references=r;r->decoder=xideReferenceDecoderCreate(languageWake,app);if(!r->decoder)return XUI_ERROR_OUT_OF_MEMORY;
#define REF_TRY(call) do{int code=(call);if(code!=XUI_OK)return code;}while(0)
 REF_TRY(xuiWidgetCreate(app->context,&r->panel));REF_TRY(xuiWidgetSetLayoutType(r->panel,XUI_LAYOUT_COLUMN));REF_TRY(xuiWidgetSetGap(r->panel,4));REF_TRY(xuiWidgetSetPadding(r->panel,(xui_thickness_t){4,4,4,4}));REF_TRY(xuiWidgetCreate(app->context,&r->bar));REF_TRY(xuiWidgetSetLayoutType(r->bar,XUI_LAYOUT_ROW));REF_TRY(xuiWidgetSetGap(r->bar,6));REF_TRY(xuiWidgetSetSizeMode(r->bar,XUI_SIZE_FILL,XUI_SIZE_FIXED));REF_TRY(xuiWidgetSetPreferredSize(r->bar,(xui_vec2_t){0,30}));REF_TRY(xuiWidgetAddChild(r->panel,r->bar));
 xui_widget* buttons[]={&r->refresh,&r->cancel,&r->previous,&r->next,&r->clear};const char* titles[]={"重新查找","取消","上一页","下一页","清除"};for(size_t i=0;i<5;++i){xui_button_desc_t button={.iSize=sizeof(button),.pFont=app->uiFont,.sText=titles[i]};REF_TRY(xuiButtonCreate(app->context,buttons[i],&button));REF_TRY(xuiWidgetSetSizeMode(*buttons[i],XUI_SIZE_FIXED,XUI_SIZE_FILL));REF_TRY(xuiWidgetSetPreferredSize(*buttons[i],(xui_vec2_t){i?64:88,30}));REF_TRY(xuiButtonSetClick(*buttons[i],referenceClicked,app));REF_TRY(xuiWidgetAddChild(r->bar,*buttons[i]));}
 xui_checkbox_desc_t check={.iSize=sizeof(check),.pFont=app->uiFont,.sText="包含声明",.fIndicatorSize=18,.fGap=6,.bUseBuiltinAtlas=1};REF_TRY(xuiCheckBoxCreate(app->context,&r->include,&check));REF_TRY(xuiWidgetSetSizeMode(r->include,XUI_SIZE_FIXED,XUI_SIZE_FILL));REF_TRY(xuiWidgetSetPreferredSize(r->include,(xui_vec2_t){110,30}));REF_TRY(xuiCheckBoxSetChecked(r->include,1));REF_TRY(xuiWidgetAddChild(r->bar,r->include));
 xui_label_desc_t label={.iSize=sizeof(label),.pFont=app->uiFont,.sText="在源码符号处按 Shift+F12 查找引用。"};REF_TRY(xuiLabelCreate(app->context,&r->status,&label));REF_TRY(xuiWidgetSetSizeMode(r->status,XUI_SIZE_FILL,XUI_SIZE_FIXED));REF_TRY(xuiWidgetSetPreferredSize(r->status,(xui_vec2_t){0,24}));REF_TRY(xuiWidgetAddChild(r->panel,r->status));
 xui_tree_view_desc_t tree={.iSize=sizeof(tree),.pFont=app->uiFont,.fItemHeight=26,.fPadding=4,.fIndent=12};REF_TRY(xuiTreeViewCreate(app->context,&r->tree,&tree));REF_TRY(xuiWidgetSetSizeMode(r->tree,XUI_SIZE_FILL,XUI_SIZE_FILL));REF_TRY(xuiWidgetAddChild(r->panel,r->tree));REF_TRY(xuiWidgetSetEventHandler(r->tree,XUI_EVENT_POINTER_DOUBLE_CLICK,referenceEvent,app));REF_TRY(xuiWidgetSetEventHandler(r->tree,XUI_EVENT_KEY_DOWN,referenceEvent,app));app->panels[XIDE_USAGES]=r->panel;referenceRows(v);xuiWidgetSetEnabled(r->cancel,0);return XUI_OK;
#undef REF_TRY
}
