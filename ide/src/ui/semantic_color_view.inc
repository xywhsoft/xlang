/* Owner-only controller. Rendering borrows an immutable decoder result; source
 * leases, JSON and replaced result arrays are retired by the XRT worker. */
typedef struct color_identity {xide_document* document;uint64_t id,epoch,session;uint32_t version;} color_identity;
typedef int (*color_set_proc)(xui_widget,color_style_proc,void*);
typedef int (*color_get_proc)(xui_widget,color_style_proc*,void**);
struct xide_color_view {
 color_set_proc set;color_get_proc get;xide_color_decoder* decoder;xide_color_legend legend;
 uint64_t epoch,serial,token,at,deadline;int supported,valid,decoding,hasReady,paused,deltaRequested;
 color_identity applied,wanted,scheduled,blocked;
 xide_color_result* result;xide_color_completion ready;xls_text* lease;char request[96];
};
static int colorIdentityEqual(color_identity a,color_identity b)
{return a.document==b.document && a.id==b.id && a.epoch==b.epoch && a.session==b.session && a.version==b.version;}
static color_identity colorIdentity(xide_language_view* v)
{
 xide_document* d=v->app->activeDocument;struct xide_color_view* c=v->colors;
 if(!c || !namedXlang(d) || !d->language || strcmp(d->language,"xlang") || snapshotConflict(d))return (color_identity){0};
 for(semantic_editor_binding* b=v->bindings;b;b=b->next)if(b->editor==d->editor && b->composing)return (color_identity){0};
 return (color_identity){d,d->id,c->epoch,v->highlightSession,xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))};
}
static int colorCurrent(xide_language_view* v,color_identity identity)
{return identity.document && colorIdentityEqual(identity,colorIdentity(v));}
static int colorInputsReady(xide_language_view* v)
{return v->initialized && v->sync && !v->syncMode && v->receiptReady && !v->receiptRequest && !automaticWatchPending(v) && (v->autoEnabled?automaticInputsCurrent(v):current(v));}
static void colorPaint(xide_language_view* v,color_identity identity)
{
 xide_document* d=identity.document;
 if(d && d->open && d->id==identity.id && d->editor)xuiWidgetInvalidate(d->editor,XUI_WIDGET_DIRTY_CACHE|XUI_WIDGET_DIRTY_RENDER);
 (void)v;
}
static int colorStyle(xui_code_document_t* doc,const xui_code_token_t* token,int* style,void* user)
{
 semantic_editor_binding* b=user;xide_language_view* v=b->app->languageView;struct xide_color_view* c=v?v->colors:NULL;
 xide_document* d=c?c->applied.document:NULL;
 /* Constant-time publication fence in the paint path: no model traversal,
    path/body scans, disk reads, reference changes or allocations. */
 if(c && c->valid && !b->composing && d && d->open && d==v->app->activeDocument && d->id==c->applied.id &&
    d->editor==b->editor && c->applied.epoch==c->epoch && c->applied.session==v->highlightSession &&
    d->language && !strcmp(d->language,"xlang") && !snapshotConflict(d) &&
    doc==xuiCodeEditGetDocument(b->editor) && xuiCodeDocumentGetVersion(doc)==c->applied.version &&
    (token->iKind==XUI_CODE_TOKEN_IDENTIFIER || token->iKind==XUI_CODE_TOKEN_TYPE)){
  const xide_color_range* row=xideColorResultFind(c->result,token->iStartOffset,token->iEndOffset);
  if(row){
   *style=row->role<=XIDE_COLOR_TYPE_PARAMETER || row->role==XIDE_COLOR_PROPERTY?XUI_CODE_STYLE_TYPE:
      row->role==XIDE_COLOR_FUNCTION || row->role==XIDE_COLOR_METHOD || row->role==XIDE_COLOR_ENUM_MEMBER || (row->modifiers&XIDE_COLOR_READONLY)?XUI_CODE_STYLE_NUMBER:XUI_CODE_STYLE_DEFAULT;
   return XUI_OK;
  }
 }
 return b->previousColor?b->previousColor(doc,token,style,b->previousColorUser):XUI_ERROR_NOT_FOUND;
}
static void colorsBind(xide_language_view* v,semantic_editor_binding* b)
{
 struct xide_color_view* c=v->colors;if(!c || b->colorBound)return;
 if(c->get(b->editor,&b->previousColor,&b->previousColorUser)==XUI_OK &&
    c->set(b->editor,colorStyle,b)==XUI_OK)b->colorBound=1;
}
static void colorsCreate(xide_language_view* v)
{
 HMODULE module=NULL;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (LPCWSTR)(uintptr_t)xuiCodeEditGetTheme,&module))return;
 FARPROC set=GetProcAddress(module,"xuiCodeEditSetTokenStyleResolver"),getStyle=GetProcAddress(module,"xuiCodeEditGetTokenStyleResolver");
 if(!set || !getStyle)return; /* Historical matched SDK keeps lexical editing. */
 struct xide_color_view* c=calloc(1,sizeof(*c));if(!c)return;
 _Static_assert(sizeof(c->set)==sizeof(set),"Win32 function pointer size");
 memcpy(&c->set,&set,sizeof(set));memcpy(&c->get,&getStyle,sizeof(getStyle));
 c->epoch=1;c->decoder=xideColorDecoderCreate(languageWake,v->app);
 if(!c->decoder){free(c);return;}v->colors=c;
}
static int colorSend(xide_language_view* v,xvalue* params,const char* id)
{
 xvalue* m=xrtValueObject();int okay=m && put(m,"jsonrpc",xrtValueString(sv("2.0"))) &&
  put(m,"method",xrtValueString(sv("textDocument/semanticTokens/full")));
 if(okay && get(params,"previousResultId"))okay=put(m,"method",xrtValueString(sv("textDocument/semanticTokens/full/delta")));
 if(okay){okay=put(m,"params",params);params=NULL;}
 if(okay)okay=put(m,"id",xrtValueString(sv(id)));
 xide_model_error e={0};if(okay)okay=xideLanguageClientSend(v->client,m,&e);
 xrtValueRelease(params);xrtValueRelease(m);return okay;
}
static void colorCancel(xide_language_view* v)
{
 struct xide_color_view* c=v->colors;if(!c)return;
 if(*c->request && v->client && v->initialized){xvalue* p=xrtValueObject();if(p && put(p,"id",xrtValueString(sv(c->request))))sendMessage(v,"$/cancelRequest",p,0);else xrtValueRelease(p);}
 c->request[0]=0;c->decoding=0;c->deadline=0;xideColorDecoderCancel(c->decoder);
 if(c->lease && xideColorDecoderDiscardInputs(c->decoder,NULL,c->lease,NULL))c->lease=NULL;
}
static void colorsInvalidate(xide_language_view* v)
{
 struct xide_color_view* c=v?v->colors:NULL;if(!c)return;
 c->valid=0;if(c->epoch==UINT64_MAX)c->supported=0;else ++c->epoch;
 colorCancel(v);colorPaint(v,c->applied);c->at=0;
}
static void colorsReset(xide_language_view* v)
{if(v->colors){colorsInvalidate(v);v->colors->supported=0;memset(&v->colors->legend,0,sizeof(v->colors->legend));}}
static void colorsStop(xide_language_view* v)
{
 struct xide_color_view* c=v->colors;if(!c)return;c->paused=1;
 for(semantic_editor_binding* b=v->bindings;b;b=b->next)if(b->colorBound){
  color_style_proc callback=NULL;void* user=NULL;
  if(c->get(b->editor,&callback,&user)==XUI_OK && callback==colorStyle && user==b)c->set(b->editor,b->previousColor,b->previousColorUser);
  b->colorBound=0;
 }
}
static void colorsDestroy(xide_language_view* v)
{
 struct xide_color_view* c=v->colors;v->colors=NULL;if(!c)return;
 xideColorDecoderDestroy(c->decoder);xlsTextRelease(c->lease);xideColorResultRelease(c->result);xideColorResultRelease(c->ready.result);free(c);
}
static int colorsAdvertise(xide_language_view* v,xvalue* textDocument)
{
 if(!v->colors)return 1;
 return put(textDocument,"semanticTokens",xrtJsonParse(sv("{\"dynamicRegistration\":false,\"requests\":{\"full\":{\"delta\":true}},\"tokenTypes\":[\"type\",\"class\",\"interface\",\"enum\",\"struct\",\"typeParameter\",\"parameter\",\"variable\",\"property\",\"enumMember\",\"function\",\"method\"],\"tokenModifiers\":[\"declaration\",\"readonly\",\"async\"],\"formats\":[\"relative\"],\"overlappingTokenSupport\":false,\"multilineTokenSupport\":false,\"augmentsSyntaxTokens\":true}")));
}
static void colorsCapabilities(xide_language_view* v,const xvalue* cap)
{if(v->colors){v->colors->supported=v->nativeBackend && xideColorLegendParse(get(cap,"semanticTokensProvider"),&v->colors->legend);v->colors->paused=0;}}
static int takeColors(xide_language_view* v,xvalue* message)
{
 struct xide_color_view* c=v->colors;const char* id=boundedString(get(message,"id"),95);
 if(!c || !id || strncmp(id,"xlang-ide:colors:",17))return 0;
 int taken=0;
 if(*c->request && !strcmp(c->request,id) && c->lease && colorCurrent(v,c->wanted) && colorInputsReady(v)){
  xide_color_result* base=c->deltaRequested?xideColorResultRetain(c->result):NULL;
  /* A full response ignores base. A delta is requested only with this exact
     immutable integer history; state cannot swap while a request is active. */
  taken=xideColorDecoderSubmit(c->decoder,c->token,message,c->lease,&c->legend,base,0);
  if(taken){c->lease=NULL;c->request[0]=0;c->decoding=1;}
  else xideColorResultRelease(base); /* view still owns another reference */
 }else{
  taken=xideColorDecoderDiscard(c->decoder,message);
  if(*c->request && !strcmp(c->request,id))colorCancel(v);
 }
 if(!taken){v->deferredMessage=message;v->deferredDiscard=0;v->deferredSession=v->highlightSession;}
 return 1;
}
void xideLanguageViewDocumentChanged(xide_app* app,xide_document* d)
{
 xide_language_view* v=app?app->languageView:NULL;struct xide_color_view* c=v?v->colors:NULL;if(!c || !d)return;
 if(!namedXlang(d) && c->applied.document!=d && c->wanted.document!=d)return;
 document_binding* b=d->path?inputPath(v,d->path):NULL;
 if(!d->open || !b || b->document!=d || b->id!=d->id || !bindingRevisionCurrent(b) || b->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) ||
    b->externalState!=d->externalState || !d->language || strcmp(b->language,d->language))colorsInvalidate(v);
 c->paused=0;
}
static void pollColors(xide_app* app)
{
 xide_language_view* v=app->languageView;struct xide_color_view* c=v->colors;if(!c)return;
 if(c->lease && !*c->request && !c->decoding && xideColorDecoderDiscardInputs(c->decoder,NULL,c->lease,NULL))c->lease=NULL;
 if(!c->hasReady && xideColorDecoderTake(c->decoder,&c->ready)){c->hasReady=1;c->decoding=0;}
 if(c->hasReady){
  if(c->ready.token!=c->token || !colorCurrent(v,c->wanted) || c->ready.error){
   if(c->ready.result && !xideColorDecoderRetire(c->decoder,c->ready.result))return;
   if(c->ready.error && colorCurrent(v,c->wanted)){c->blocked=c->wanted;c->valid=0;colorPaint(v,c->applied);}
   memset(&c->ready,0,sizeof(c->ready));c->hasReady=0;
  }else{
   if(!colorInputsReady(v))return;
   c->valid=0;
   if(c->result && !xideColorDecoderRetire(c->decoder,c->result))return;
   c->result=c->ready.result;c->applied=c->wanted;c->valid=1;c->hasReady=0;memset(&c->ready,0,sizeof(c->ready));colorPaint(v,c->applied);
  }
 }
 color_identity identity=colorIdentity(v);
 if((*c->request || c->decoding) && !colorIdentityEqual(identity,c->wanted))colorCancel(v);
 if(*c->request && GetTickCount64()>=c->deadline){c->blocked=c->wanted;colorCancel(v);}
 if(!identity.document || c->paused || c->hasReady || c->lease || *c->request || c->decoding)return;
 /* The existing automatic/explicit analysis lifecycle owns the connection.
    Coloring must not restart it after the user cancels automatic checking. */
 if(!v->client)return;
 if(!c->supported || !colorInputsReady(v) || (c->valid && colorIdentityEqual(identity,c->applied)) || colorIdentityEqual(identity,c->blocked))return;
 for(semantic_editor_binding* b=v->bindings;b;b=b->next)if(b->editor==identity.document->editor)colorsBind(v,b);
 if(!colorIdentityEqual(identity,c->scheduled)){c->scheduled=identity;c->at=GetTickCount64()+75;xgeRenderRequestAfter(.075f);return;}
 if(GetTickCount64()<c->at)return;
 if(c->serial==UINT64_MAX){c->supported=0;return;}
 document_binding* b=inputPath(v,identity.document->path);if(!b || b->id!=identity.id || !bindingRevisionCurrent(b) || b->version!=identity.version || xlsSyncVersion(v->sync,b->uri)!=b->wireVersion)return;
 c->lease=xlsSyncRetainText(v->sync,b->uri,b->wireVersion);if(!c->lease)return;
 xvalue *params=xrtValueObject(),*document=xrtValueObject();int okay=params && document && put(document,"uri",xrtValueString(sv(b->uri)));
 if(okay){okay=put(params,"textDocument",document);document=NULL;}
 if(okay)okay=put(params,"version",xrtValueInt(b->wireVersion));
 const char* previous=xideColorResultId(c->result);
 c->deltaRequested=0;
 if(okay && c->legend.delta && c->applied.document==identity.document && c->applied.id==identity.id && c->applied.session==identity.session && previous && strnlen(previous,129)<=128)
  {okay=put(params,"previousResultId",xrtValueString(sv(previous)));c->deltaRequested=okay;}
 c->wanted=identity;c->token=++c->serial;snprintf(c->request,sizeof(c->request),"xlang-ide:colors:%llu:%llu",(unsigned long long)identity.session,(unsigned long long)c->token);
 if(!okay || !colorSend(v,params,c->request)){if(!okay)xrtValueRelease(params);c->blocked=identity;colorCancel(v);}
 else{c->deadline=GetTickCount64()+5000;xgeRenderRequestAfter(5.0f);}
 xrtValueRelease(document);
}
