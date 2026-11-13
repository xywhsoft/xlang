typedef struct xide_native_apply_view {
 xide_native_edit_async* worker;xide_source_barrier barrier;uint64_t token,hold;
 char* directory;int phase,confirmed,cancelled,resuming,closing,taskInputsInvalidated;
 xide_edit_transaction_status transaction;xide_model_error error;
} xide_native_apply_view;
static int nativeEditHeld(const xide_language_view* v)
{return v&&v->nativeEdit&&v->nativeEdit->hold!=0;}
static int nativeEditBusy(const xide_native_apply_view* n)
{return n&&n->token&&n->phase!=XIDE_APPLY_DONE&&n->phase!=XIDE_APPLY_CANCELLED&&n->phase!=XIDE_APPLY_FAILED;}
static void nativeEditError(xide_native_apply_view* n,const xide_model_error* e)
{if(e&&e->code&&!n->error.code)n->error=*e;}
static int nativeEditGuard(void* raw)
{
 xide_language_view* v=raw;xide_native_apply_view* n=v->nativeEdit;xide_source_barrier_status b={0};xide_native_async_status s={0};
 if(!n||!n->confirmed||n->cancelled||n->closing||!n->hold||!xideNativeAsyncStatus(n->worker,&s)||s.token!=n->token||!s.candidate||!xideEditCandidateCurrent(s.candidate)||!xideSourceBarrierState(&n->barrier,n->hold,&b)||!b.ready)return XUI_ERROR_INVALID_STATE;
 return xideLanguageEditRequestCurrent(v->app,(xide_edit_request*)xideEditCallRequest(xideEditCandidateCall(s.candidate)))?XUI_OK:XUI_ERROR_INVALID_STATE;
}
uint64_t xideLanguageEditApplyBegin(xide_app* app,const char* directory,const xide_edit_alias_exclusion* excluded,size_t count,size_t budget,xide_model_error* error)
{
 if(error)*error=(xide_model_error){0};
 xide_language_view* v=app?app->languageView:NULL;const xide_edit_candidate* candidate=xideLanguageEditCandidate(app);
 if(!v||!candidate||v->sourceToken||v->sourceDrain||nativeEditBusy(v->nativeEdit)||!directory){if(error){error->code=XIDE_MODEL_CONFLICT;snprintf(error->message,sizeof(error->message),"Native edit requires a current admitted candidate and no other source operation.");}return 0;}
 if(!v->nativeEdit){v->nativeEdit=calloc(1,sizeof(*v->nativeEdit));if(!v->nativeEdit)return 0;v->nativeEdit->worker=xideNativeAsyncCreate(languageWake,app);if(!v->nativeEdit->worker){free(v->nativeEdit);v->nativeEdit=NULL;return 0;}}
 xide_native_apply_view* n=v->nativeEdit;xide_native_async_status s={0};if(!xideNativeAsyncStatus(n->worker,&s)||s.state!=XIDE_NATIVE_IDLE||s.active||n->hold)return 0;
 size_t length=0;while(length<65536&&directory[length])++length;if(!length||length>=65536)return 0;char* owned=malloc(length+1);if(!owned)return 0;memcpy(owned,directory,length+1);
 uint64_t token=xideNativeAsyncSubmit(n->worker,(xide_edit_candidate*)candidate,directory,excluded,count,budget);if(!token){free(owned);return 0;}
 free(n->directory);n->directory=owned;n->token=token;n->phase=XIDE_APPLY_CONFIRM;n->confirmed=n->cancelled=n->resuming=n->closing=n->taskInputsInvalidated=0;n->error=(xide_model_error){0};n->transaction=(xide_edit_transaction_status){0};xgeRenderRequest();return token;
}
const xide_workspace_edit* xideLanguageEditApplyPlan(xide_app* app,uint64_t token)
{
 xide_language_view* v=app?app->languageView:NULL;xide_native_apply_view* n=v?v->nativeEdit:NULL;xide_native_async_status s={0};
 return n&&n->token==token&&n->phase==XIDE_APPLY_CONFIRM&&!n->cancelled&&xideNativeAsyncStatus(n->worker,&s)&&s.token==token&&s.candidate&&xideEditCandidateCurrent(s.candidate)?xideEditCandidatePlan(s.candidate):NULL;
}
int xideLanguageEditApplyConfirm(xide_app* app,uint64_t token,xide_model_error* error)
{
 if(error)*error=(xide_model_error){0};
 xide_language_view* v=app?app->languageView:NULL;xide_native_apply_view* n=v?v->nativeEdit:NULL;xide_native_async_status s={0};
 if(!n||!token||token!=n->token||n->phase!=XIDE_APPLY_CONFIRM||n->cancelled||n->hold||!xideNativeAsyncStatus(n->worker,&s)||s.state!=XIDE_NATIVE_NEW||s.active||!s.candidate||!xideEditCandidateCurrent(s.candidate))return 0;
 xide_edit_request* request=(xide_edit_request*)xideEditCallRequest(xideEditCandidateCall(s.candidate));if(!xideLanguageEditRequestCurrent(app,request))return 0;
 n->hold=xideSourceBarrierBeginEdit(&n->barrier,app,request,error);if(!n->hold)return 0;
 n->confirmed=1;n->phase=XIDE_APPLY_HOLDING;nativeEditError(n,error);xgeRenderRequest();return 1;
}
int xideLanguageEditApplyCancel(xide_app* app,uint64_t token)
{
 xide_language_view* v=app?app->languageView:NULL;xide_native_apply_view* n=v?v->nativeEdit:NULL;
 if(!nativeEditBusy(n)||!token||token!=n->token)return 0;
 n->cancelled=1;xideNativeAsyncCancel(n->worker,token);xgeRenderRequest();return 1;
}
int xideLanguageEditApplyStatus(xide_app* app,xide_native_apply_status* out)
{
 if(out)*out=(xide_native_apply_status){0};
 if(!app||!out)return 0;
 xide_native_apply_view* n=app->languageView?app->languageView->nativeEdit:NULL;
 if(n)*out=(xide_native_apply_status){n->token,n->phase,nativeEditBusy(n),n->hold!=0,n->transaction,n->error,n->directory};
 return 1;
}
static void nativeEditInvalidateTasks(xide_language_view* v,const xide_edit_candidate* candidate)
{
 xide_native_apply_view* n=v->nativeEdit;if(n->taskInputsInvalidated)return;n->taskInputsInvalidated=1;
 if(!candidate||!(n->transaction.applied||n->transaction.restored||n->transaction.committed))return;
 const xide_workspace_edit* plan=xideEditCandidatePlan(candidate);xide_workspace_edit_info info={0};
 if(xideWorkspaceEditInfo(plan,&info))for(size_t i=0;i<info.documents;++i){xide_workspace_edit_document d;if(xideWorkspaceEditDocumentAt(plan,i,&d))xideTasksInvalidateInput(v->app->tasks,d.path);}
}
static void nativeEditRetire(xide_language_view* v,xide_native_async_status* s)
{
 xide_native_apply_view* n=v->nativeEdit;
 if(s->transaction.needsRecovery){n->phase=XIDE_APPLY_RECOVERY;return;}
 if(xideNativeAsyncDetach(n->worker,n->token)!=XUI_OK){n->phase=XIDE_APPLY_RECOVERY;return;}
 if(xideNativeAsyncStatus(n->worker,s)&&s->state==XIDE_NATIVE_PUBLISHED){if(xideNativeAsyncFinish(n->worker,n->token))n->phase=XIDE_APPLY_FINISHING;return;}
 if(xideNativeAsyncRetire(n->worker,n->token,0))n->phase=XIDE_APPLY_RETIRING;
}
static void pollNativeEdit(xide_language_view* v)
{
 xide_native_apply_view* n=v->nativeEdit;if(!nativeEditBusy(n))return;
 xide_native_async_status s={0};xide_source_barrier_status b={0};if(!xideNativeAsyncStatus(n->worker,&s))return;
 if(s.token==n->token){n->transaction=s.transaction;nativeEditError(n,&s.error);}
 int restore=0,apply=0;
 if(n->hold&&!n->resuming){
  int valid=xideSourceBarrierState(&n->barrier,n->hold,&b);restore=valid&&b.quiet&&!b.lost;
  if(!valid||b.lost||b.failed||b.invalidated)n->cancelled=1;
  apply=restore&&b.ready&&n->confirmed&&!n->cancelled&&s.candidate&&xideEditCandidateCurrent(s.candidate)&&xideLanguageEditRequestOwned(v->app,xideEditCallRequest(xideEditCandidateCall(s.candidate)));
 }else if(!n->hold&&s.candidate&&!xideEditCandidateCurrent(s.candidate))n->cancelled=1;
 xideNativeAsyncAuthorize(n->worker,n->token,apply,restore);
 if(n->cancelled)xideNativeAsyncCancel(n->worker,n->token);
 if(n->phase==XIDE_APPLY_RECOVERY)return;
 if(s.active){xgeRenderRequestAfter(.02f);return;}
 if(n->resuming){
  xide_model_error e={0};int released=xideSourceBarrierRelease(&n->barrier,n->hold,&e);nativeEditError(n,&e);
  if(released==1){n->hold=0;n->resuming=0;if(xideNativeAsyncDrop(n->worker,n->token))n->phase=XIDE_APPLY_RETIRING;}
  else if(released<0){xide_source_barrier_status after={0};if(!xideSourceBarrierState(&n->barrier,n->hold,&after)||after.lost)n->phase=XIDE_APPLY_RECOVERY;}
  xgeRenderRequestAfter(.02f);return;
 }
 if(s.state==XIDE_NATIVE_IDLE){n->phase=n->transaction.needsRecovery?XIDE_APPLY_RECOVERY:n->transaction.committed?XIDE_APPLY_DONE:n->cancelled?XIDE_APPLY_CANCELLED:XIDE_APPLY_FAILED;return;}
 if(s.state==XIDE_NATIVE_NEW){
  if(n->cancelled)nativeEditRetire(v,&s);
  else if(n->hold&&b.ready&&apply&&xideNativeAsyncPrepare(n->worker,n->token))n->phase=XIDE_APPLY_PREPARING;
 }else if(s.state==XIDE_NATIVE_PREPARED){
  if(!apply||nativeEditGuard(v)!=XUI_OK){n->cancelled=1;xideNativeAsyncCancel(n->worker,n->token);nativeEditRetire(v,&s);}
  else if(xideNativeAsyncCapture(n->worker,n->token,&v->app->documents)==XUI_OK&&xideNativeAsyncStage(n->worker,n->token))n->phase=XIDE_APPLY_STAGING;
  else {n->cancelled=1;xideNativeAsyncCancel(n->worker,n->token);}
 }else if(s.state==XIDE_NATIVE_CAPTURED){
  if(!apply)nativeEditRetire(v,&s);else if(xideNativeAsyncStage(n->worker,n->token))n->phase=XIDE_APPLY_STAGING;
 }else if(s.state==XIDE_NATIVE_STAGED){
  if(!apply||xideNativeAsyncArm(n->worker,n->token,nativeEditGuard,v)!=XUI_OK){n->cancelled=1;xideNativeAsyncCancel(n->worker,n->token);nativeEditRetire(v,&s);}
  else if(xideNativeAsyncApply(n->worker,n->token))n->phase=XIDE_APPLY_WRITING;
 }else if(s.state==XIDE_NATIVE_ARMED){
  if(!apply)nativeEditRetire(v,&s);else if(xideNativeAsyncApply(n->worker,n->token))n->phase=XIDE_APPLY_WRITING;
 }else if(s.state==XIDE_NATIVE_READY){
  xide_open_edit_outcome out={0};(void)xideNativeAsyncPublish(n->worker,n->token,nativeEditGuard,v,&out);
  xideNativeAsyncStatus(n->worker,&s);n->transaction=s.transaction;nativeEditError(n,&s.error);
  if(s.state==XIDE_NATIVE_READY)(void)xideNativeAsyncDetach(n->worker,n->token);
  if(xideNativeAsyncFinish(n->worker,n->token))n->phase=XIDE_APPLY_FINISHING;
 }else if(s.state==XIDE_NATIVE_PUBLISHED){if(xideNativeAsyncFinish(n->worker,n->token))n->phase=XIDE_APPLY_FINISHING;}
 else if(s.state==XIDE_NATIVE_FINISHED||s.state==XIDE_NATIVE_FAILED)nativeEditRetire(v,&s);
 else if(s.state==XIDE_NATIVE_RETIRED){
  nativeEditInvalidateTasks(v,s.candidate);
  if(n->hold){
   /* Work/source handles are already closed. Keep the driver's candidate until
    * barrier Release drops its request, then retire the large last lease off UI. */
   resetEditController(v);xide_model_error e={0};if(xideSourceBarrierResume(&n->barrier,n->hold,&e)){n->resuming=1;n->phase=XIDE_APPLY_RESUMING;}else n->phase=XIDE_APPLY_RECOVERY;nativeEditError(n,&e);
  }else if(xideNativeAsyncDrop(n->worker,n->token))n->phase=XIDE_APPLY_RETIRING;
 }
 if(n->phase!=XIDE_APPLY_CONFIRM&&n->phase!=XIDE_APPLY_RECOVERY)xgeRenderRequestAfter(.02f);
}
int xideLanguageEditApplyShutdown(xide_app* app)
{
 xide_language_view* v=app?app->languageView:NULL;xide_native_apply_view* n=v?v->nativeEdit:NULL;if(!n)return XUI_OK;
 n->closing=1;xide_source_barrier_status b={0};int restore=n->hold&&!n->resuming&&xideSourceBarrierState(&n->barrier,n->hold,&b)&&b.quiet&&!b.lost;
 xideNativeAsyncAuthorize(n->worker,n->token,0,restore);int status=xideNativeAsyncDestroy(n->worker);if(status!=XUI_OK)return status;
 n->worker=NULL;xideSourceBarrierAbandonFinal(&n->barrier);free(n->directory);free(n);v->nativeEdit=NULL;return XUI_OK;
}
