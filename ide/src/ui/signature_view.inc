/* Parameter hints share completion_view's connection, immutable sent inputs
 * and synchronization planner. Only request/decoder/window state is separate. */
static int svflush(xide_completion_view* v)
{
 signature_view* s=&v->signature;
 if(s->retired){if(!xideSignatureDecoderRetire(s->decoder,s->retired))return 0;s->retired=NULL;}
 if(s->result && !s->ready){if(!xideSignatureDecoderRetire(s->decoder,s->result))return 0;s->result=NULL;}
 return 1;
}
static void svdismiss(xide_completion_view* v)
{
 signature_view* s=&v->signature;
 if(s->request && v->client && v->initialized){xvalue* p=xrtValueObject();cvput(p,"id",xrtValueInt(s->request));cvsend(v,"$/cancelRequest",p,0);}
 xideSignatureDecoderCancel(s->decoder);s->decodeToken=0;s->wanted=s->pending=s->request=s->ready=s->resumeIme=0;
 if(s->window)xuiWindowSetOpen(s->window,0);
 svflush(v);
}
static int svfocus(xide_completion_view* v)
{
 signature_view* s=&v->signature;xui_widget focus=xuiGetFocusWidget(v->app->context);
 if(s->document && focus==s->document->editor)return 1;
 if(s->window && xuiWindowIsOpen(s->window))for(xui_widget w=focus;w;w=xuiWidgetGetParent(w))if(w==s->window)return 1;
 return 0;
}
static int svcontext(xide_completion_view* v,int exact)
{
 signature_view* s=&v->signature;xide_document* d=s->document;xui_code_selection_t selection={0};
 if(!s->wanted || !d || d!=v->app->activeDocument || d->id!=s->id || !cveligible(v->app,d) || !svfocus(v) ||
    xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection)!=XUI_OK || selection.iCaretOffset!=selection.iAnchorOffset)return 0;
 return !exact || (s->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) && selection.iCaretOffset==s->offset);
}
static void svqueue(xide_completion_view* v,xide_document* d,int offset,int kind,char trigger,int retrigger)
{
 signature_view* s=&v->signature;int moved=s->document!=d || s->id!=d->id;
 if(v->syncToken && !cvinputsCurrent(v))cvcancelSync(v);
 svdismiss(v);s->document=d;s->id=d->id;s->version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
 s->offset=offset;s->wanted=s->pending=1;s->triggerKind=kind;s->trigger[0]=trigger;s->trigger[1]=0;s->retrigger=retrigger;
 if(moved)s->placed=0;
 xgeRenderRequest();
}
void xideSignatureViewRequest(xide_app* app,xui_widget editor,int offset,int force)
{
 xide_completion_view* v=app ? app->completionView : NULL;xide_document* d=app ? app->activeDocument : NULL;
 if(!v || !d || d->editor!=editor || !cveligible(app,d) || xuiGetFocusWidget(app->context)!=editor)return;
 xui_code_selection_t selection={0};if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&selection)!=XUI_OK || selection.iCaretOffset!=offset || selection.iAnchorOffset!=offset)return;
 signature_view* s=&v->signature;if(!force && s->wanted && s->document==d && s->version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)) && s->offset==offset)return;
 svqueue(v,d,offset,force ? 1 : 3,0,s->wanted);
}
static int svsend(xide_completion_view* v)
{
 signature_view* s=&v->signature;if(!s->pending || !svcontext(v,1))return 1;
 if(!s->supported){svdismiss(v);return 1;}
 if(s->triggerKind==2 && !s->retrigger && s->offset>0){
  xui_widget editor=s->document->editor;xui_code_token_t tokens[8];int count=0;
  if(xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),s->offset-1,s->offset,tokens,8,&count)!=XUI_OK)return 0;
  for(int i=0;i<count && i<8;++i)if(tokens[i].iKind==XUI_CODE_TOKEN_COMMENT || tokens[i].iKind==XUI_CODE_TOKEN_STRING || tokens[i].iKind==XUI_CODE_TOKEN_CHAR){svdismiss(v);return 1;}
 }
 completion_input* target=NULL;for(size_t i=0;i<v->inputCount;++i)if(v->inputs[i].document==s->document)target=v->inputs+i;
 int line=0,character=0;if(!target || v->nextId==INT_MAX || s->serial==UINT64_MAX || !xideCodeDocumentOffsetToLsp(xuiCodeEditGetDocument(s->document->editor),s->offset,&line,&character))return -1;
 xvalue *p=xrtValueObject(),*doc=xrtValueObject(),*pos=xrtValueObject(),*context=xrtValueObject();
 int okay=p && doc && pos && context && cvput(doc,"uri",xrtValueString(cvsv(target->uri))) && cvput(pos,"line",xrtValueInt(line)) && cvput(pos,"character",xrtValueInt(character)) && cvput(context,"triggerKind",xrtValueInt(s->triggerKind)) && cvput(context,"isRetrigger",xrtValueBool(s->retrigger));
 if(okay && s->trigger[0])okay=cvput(context,"triggerCharacter",xrtValueString(cvsv(s->trigger)));
 if(okay){okay=cvput(p,"textDocument",doc);doc=NULL;}if(okay){okay=cvput(p,"position",pos);pos=NULL;}if(okay){okay=cvput(p,"context",context);context=NULL;}
 okay=okay && cvput(p,"version",xrtValueInt(target->wireVersion));xrtValueRelease(doc);xrtValueRelease(pos);xrtValueRelease(context);
 if(!okay){xrtValueRelease(p);return -1;}
 xls_text* text=xlsSyncRetainText(v->sync,target->uri,target->wireVersion);int request=v->nextId+1;uint64_t token=++s->serial;
 if(!text || !xideSignatureDecoderExpect(s->decoder,token,text,(xide_signature_expected){request,target->wireVersion,s->offset})){
  xlsTextRelease(text);xrtValueRelease(p);return 0;
 }
 v->nextId=request;s->request=request;s->decodeToken=token;s->session=v->session;s->epoch=v->inputEpoch;s->pending=0;
 return cvsend(v,"textDocument/signatureHelp",p,request) ? 1 : -1;
}
static void svlayers(xui_widget w)
{
 xuiWidgetSetLayer(w,XUI_LAYER_FLOATING,1500);
 for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child))svlayers(child);
}
static void svlayout(xide_completion_view* v)
{
 signature_view* s=&v->signature;if(!s->window || !xuiWindowIsOpen(s->window))return;
 xide_app* a=v->app;float scale=xideUiFontScale(a);xui_rect_t r=xuiWidgetGetRect(s->window);
 if(!s->placed){xui_rect_t caret={0};xuiCodeEditGetTextOffsetRect(s->document->editor,s->offset,&caret);r=(xui_rect_t){caret.fX,caret.fY+caret.fH+4*scale,600*scale,250*scale};}
 if(r.fW>a->viewportWidth-16)r.fW=fmaxf(160,a->viewportWidth-16);
 if(r.fH>a->viewportHeight-16)r.fH=fmaxf(120,a->viewportHeight-16);
 r.fX=fmaxf(0,fminf(r.fX,a->viewportWidth-r.fW));r.fY=fmaxf(0,fminf(r.fY,a->viewportHeight-r.fH));xuiWidgetSetRect(s->window,r);s->placed=1;
 xuiWindowSetChrome(s->window,28*scale,1,5*scale,22*scale);xuiWindowSetMinSize(s->window,200*scale,140*scale);
 xui_rect_t client=xuiWidgetGetRect(xuiWindowGetClientWidget(s->window));float width=client.fW>0 ? client.fW : r.fW-2,height=client.fH>0 ? client.fH : r.fH-28*scale;
 float pad=8*scale,bar=28*scale,code=fminf(78*scale,height*.4f);
 xuiWidgetSetRect(s->previous,(xui_rect_t){pad,pad,28*scale,bar});xuiWidgetSetRect(s->next,(xui_rect_t){pad+32*scale,pad,28*scale,bar});
 xuiWidgetSetRect(s->status,(xui_rect_t){pad+68*scale,pad,fmaxf(40,width-84*scale),bar});
 xuiWidgetSetRect(s->preview,(xui_rect_t){pad,pad+bar+4*scale,fmaxf(40,width-2*pad),code});
 xuiWidgetSetRect(s->documentation,(xui_rect_t){pad,pad+bar+code+8*scale,fmaxf(40,width-2*pad),fmaxf(24,height-bar-code-3*pad-8*scale)});
 if(s->theme!=a->theme){
  const int ids[]={XUI_CODE_STYLE_DEFAULT,XUI_CODE_STYLE_KEYWORD,XUI_CODE_STYLE_TYPE,XUI_CODE_STYLE_NUMBER,XUI_CODE_STYLE_STRING,XUI_CODE_STYLE_COMMENT};
  const ide_color_role roles[]={IDE_TEXT,IDE_KEYWORD,IDE_TYPE,IDE_NUMBER,IDE_STRING,IDE_COMMENT};
  for(size_t i=0;i<sizeof(ids)/sizeof(*ids);++i){xui_code_style_t style={sizeof(style),ideThemeColor(a->theme,roles[i]),ideThemeColor(a->theme,IDE_FIELD),0};xuiCodeEditSetStyle(s->preview,ids[i],&style);}xuiTextEditSetColors(s->documentation,ideThemeColor(a->theme,IDE_FIELD),ideThemeColor(a->theme,IDE_TEXT),ideThemeColor(a->theme,IDE_BORDER),ideThemeColor(a->theme,IDE_ACCENT));s->theme=a->theme;
 }
 svlayers(s->window);
}
static void svshow(xide_completion_view* v)
{
 signature_view* s=&v->signature;if(!s->result || !s->ready || s->selected>=s->result->count)return;
 xuiWindowSetFont(s->window,v->app->uiFont);xuiLabelSetFont(s->status,v->app->uiFont);xuiButtonSetFont(s->previous,v->app->uiFont);xuiButtonSetFont(s->next,v->app->uiFont);xuiCodeEditSetFont(s->preview,v->app->codeFont);xuiTextEditSetFont(s->documentation,v->app->uiFont);
 const xide_signature_item* item=s->result->items+s->selected;const char* docs=item->documentation ? item->documentation : "";char status[160];
 xuiCodeEditSetText(s->preview,item->label);
 if(item->activeParameter<item->count){const xide_signature_parameter* parameter=item->parameters+item->activeParameter;
  xui_code_selection_t selection={0};selection.iAnchorOffset=(int)parameter->start;selection.iCaretOffset=(int)parameter->end;xuiCodeSelectionSetState(xuiCodeEditGetSelection(s->preview),xuiCodeEditGetDocument(s->preview),&selection);xuiCodeEditEnsureCaretVisible(s->preview);
  if(parameter->documentation && *parameter->documentation)docs=parameter->documentation;
  snprintf(status,sizeof(status),"重载 %u / %u · 参数 %u / %u",(unsigned)s->selected+1,(unsigned)s->result->count,(unsigned)item->activeParameter+1,(unsigned)item->count);
 }else snprintf(status,sizeof(status),"重载 %u / %u · 参数位置待确定",(unsigned)s->selected+1,(unsigned)s->result->count);
 xuiLabelSetText(s->status,status);xuiTextEditSetText(s->documentation,*docs ? docs : "Ctrl+Shift+Space 查看参数 · Esc 关闭");
 xuiWidgetSetEnabled(s->previous,s->result->count>1);xuiWidgetSetEnabled(s->next,s->result->count>1);
 xuiWindowSetOpen(s->window,1);svlayout(v);xuiSetFocusWidget(v->app->context,s->document->editor);xgeRenderRequest();
}
static void svclose(xui_widget widget,void* user){(void)widget;svdismiss(user);}
static void svchoose(xui_widget widget,void* user)
{
 xide_completion_view* v=user;signature_view* s=&v->signature;
 if(!s->ready || !s->result || !svcontext(v,1) || !cvinputsCurrent(v) || s->epoch!=v->inputEpoch || v->deferredMessage || xideLanguageClientHasPending(v->client) || !cvconnectionCurrent(v)){svdismiss(v);return;}
 if(widget==s->previous)s->selected=s->selected ? s->selected-1 : s->result->count-1;
 else s->selected=(s->selected+1)%s->result->count;
 svshow(v);
}
static int svwindowEvent(xui_widget w,const xui_event_t* event,void* user)
{
 xide_completion_view* v=user;signature_view* s=&v->signature;
 /* Client widgets consume their own pointer presses. Let the native window
    own a press in its resize grip before routing the remaining client area. */
 int skip=event && event->iType==XUI_EVENT_POINTER_DOWN && event->iPhase==XUI_EVENT_PHASE_CAPTURE && event->pTarget!=w;
 xui_event_t frame={0};
 if(skip && xuiWindowGetResizeEdgesAt(w,event->fX,event->fY)){
  frame=*event;frame.iPhase=XUI_EVENT_PHASE_TARGET;frame.pTarget=frame.pCurrentTarget=w;event=&frame;skip=0;
 }
 int r=!skip && s->windowEvent ? s->windowEvent(w,event,s->windowEventUser) : XUI_OK;
 if(s->window && xuiWindowIsOpen(s->window))svlayers(s->window);
 return r;
}
static int svcreate(xide_completion_view* v)
{
 signature_view* s=&v->signature;xide_app* a=v->app;xui_window_desc_t window={0};window.iSize=sizeof(window);window.sTitle="参数提示";window.pFont=a->uiFont;window.bClosed=1;window.bHideCollapse=window.bHideMaximize=1;window.fMinWidth=200;window.fMinHeight=140;window.fTitleBarHeight=28;window.fBorderWidth=1;window.fResizeGrip=5;window.fButtonSize=22;
 int r=xuiWindowCreate(a->context,&s->window,&window);if(r!=XUI_OK)return r;
 xui_widget client=xuiWindowGetClientWidget(s->window);xuiWidgetSetLayoutType(client,XUI_LAYOUT_MANUAL);
 xui_label_desc_t label={0};label.iSize=sizeof(label);label.pFont=a->uiFont;label.sText="";r=xuiLabelCreate(a->context,&s->status,&label);if(r!=XUI_OK)return r;xuiWidgetAddChild(client,s->status);
 xui_button_desc_t button={0};button.iSize=sizeof(button);button.pFont=a->uiFont;button.sText="‹";r=xuiButtonCreate(a->context,&s->previous,&button);if(r!=XUI_OK)return r;button.sText="›";r=xuiButtonCreate(a->context,&s->next,&button);if(r!=XUI_OK)return r;
 xuiWidgetAddChild(client,s->previous);xuiWidgetAddChild(client,s->next);xuiButtonSetClick(s->previous,svchoose,v);xuiButtonSetClick(s->next,svchoose,v);
 xui_code_edit_desc_t code={0};code.iSize=sizeof(code);code.pFont=a->codeFont;code.sText="";code.bReadonly=1;
 r=xideDpiCodeEditCreate(a->context,&s->preview,&code);if(r!=XUI_OK)return r;xuiWidgetAddChild(client,s->preview);xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(s->preview));xideConfigureEditorLanguage(s->preview,"xlang");xuiCodeEditSetDefaultMargins(s->preview,0,0,0,0);xuiCodeEditSetWordWrap(s->preview,0);
 xui_text_edit_desc_t docs={0};docs.iSize=sizeof(docs);docs.pFont=a->uiFont;docs.sText="";docs.bReadonly=1;docs.bWordWrap=1;
 r=xuiTextEditCreate(a->context,&s->documentation,&docs);if(r!=XUI_OK)return r;xuiWidgetAddChild(client,s->documentation);
 xuiWidgetGetEventCallback(s->window,&s->windowEvent,&s->windowEventUser);xuiWidgetSetEventCallback(s->window,svwindowEvent,v);
 xuiWindowSetClose(s->window,svclose,v);xuiWidgetSetCancelAction(s->window,svclose,v);xideUiFontBindWidget(a,s->window);return XUI_OK;
}
static void svpoll(xide_completion_view* v)
{
 signature_view* s=&v->signature;svflush(v);
 if(s->resumeIme && (!s->document || s->document!=v->app->activeDocument || !s->document->open || s->id!=s->document->id || !svfocus(v)))s->resumeIme=0;
 if(s->wanted){
  if(!svcontext(v,0) || (v->initialized && !s->supported)){svdismiss(v);return;}
  xui_code_selection_t selection={0};xuiCodeSelectionGetState(xuiCodeEditGetSelection(s->document->editor),&selection);
  if(!svcontext(v,1) || ((s->request || s->ready) && (s->epoch!=v->inputEpoch || !cvinputsCurrent(v))))svqueue(v,s->document,selection.iCaretOffset,3,0,1);
 }
 if(!s->retired && !s->result && !v->deferredMessage && !xideLanguageClientHasPending(v->client)){
  xide_signature_result* result=xideSignatureDecoderTake(s->decoder);
  if(result){
   int current=s->wanted && result->token==s->decodeToken && s->session==v->session && s->epoch==v->inputEpoch && svcontext(v,1) && cvinputsCurrent(v) && cvconnectionCurrent(v);
   if(current && result->valid && result->available && result->count){s->decodeToken=0;s->ready=1;s->result=result;s->selected=result->activeSignature;result=NULL;svshow(v);}
   else if(current)svdismiss(v);
   if(result){s->retired=result;svflush(v);}
  }
 }
 if(s->ready)svlayout(v);
}
static void svinput(xide_completion_view* v,xui_widget editor,const xui_code_input_event_t* event)
{
 if(!event || event->iPhase!=XUI_CODE_INPUT_COMMITTED)return;
 signature_view* s=&v->signature;xide_document* d=v->app->activeDocument;if(!d || d->editor!=editor || !cvnamed(d) || strcmp(d->language,"xlang"))return;
 int active=(s->wanted || s->resumeIme) && s->document==d && s->id==d->id;char last=event->sText && event->iTextSize>0 ? event->sText[event->iTextSize-1] : 0;
 int trigger=event->iTextSize==1 && (last=='(' || last==',') && (event->iSource==XUI_CODE_INPUT_SOURCE_KEYBOARD || event->iSource==XUI_CODE_INPUT_SOURCE_IME);
 if(!active && !trigger)return;
 int offset=event->tSelectionAfter.iCaretOffset;

 svqueue(v,d,offset,trigger ? 2 : 3,trigger ? last : 0,active);
}
static void svevent(xide_completion_view* v,xui_widget editor,const xui_event_t* event)
{
 signature_view* s=&v->signature;
 if((s->wanted || s->resumeIme) && s->document && s->document->editor==editor){
  if(event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ESCAPE)svdismiss(v);
  else if(event->iType==XUI_EVENT_IME_COMPOSITION){int resume=s->wanted || s->resumeIme;svdismiss(v);s->resumeIme=resume;}
 }
}

static void svafterEvent(xide_completion_view* v,xui_widget editor,const xui_event_t* event)
{
 signature_view* s=&v->signature;
 if(event->iType==XUI_EVENT_IME_COMPOSITION && !event->bCompositionActive && s->resumeIme && s->document && s->document==v->app->activeDocument && s->document->editor==editor && s->document->id==s->id && cveligible(v->app,s->document)){
  xui_code_selection_t selection={0};if(xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&selection)==XUI_OK && selection.iCaretOffset==selection.iAnchorOffset)svqueue(v,s->document,selection.iCaretOffset,3,0,1);
 }
}
int xideSignatureViewIsOpen(xide_app* app)
{return app && app->completionView && app->completionView->signature.ready && xuiWindowIsOpen(app->completionView->signature.window);}
int xideSignatureViewAction(xide_app* app,int action)
{
 (void)action;xide_document* d=app ? app->activeDocument : NULL;if(!d || !cveligible(app,d))return 0;
 xui_code_selection_t selection={0};xuiCodeSelectionGetState(xuiCodeEditGetSelection(d->editor),&selection);xuiSetFocusWidget(app->context,d->editor);xideSignatureViewRequest(app,d->editor,selection.iCaretOffset,1);return 1;
}
