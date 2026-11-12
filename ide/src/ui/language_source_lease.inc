/* Both source-reading UI workers are cancelled under their existing session/
 * request ownership. All other language decoders operate only immutable text
 * and JSON; their pending intents are cancelled without UI joins. */
static int sourceCleanup(xide_language_view* v)
{
 xide_automatic_diagnostics* a=v->autoView;
 if(!a)return 1;
 if(!automaticRetire(a))return 0;
 if(a->batch){a->garbage.batch=a->batch;a->batch=NULL;}
 if(a->held){a->garbage.root=a->held;a->held=NULL;a->heldBytes=0;}
 if(a->incoming){a->garbage.result=a->incoming;a->incoming=NULL;a->incomingAt=0;}
 return automaticRetire(a);
}
uint64_t xideLanguageSourcesSuspend(xide_app* app,xide_model_error* error)
{
 xide_language_view* v=app?app->languageView:NULL;
#if XIDE_ENABLE_C_LSP
 (void)v;if(error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"The optional C service has no source-reader lease participant.");}return 0;
#else
 if(!v || v->sourceToken || v->sourceDrain || v->sourceSerial==UINT64_MAX || v->highlightSession==UINT64_MAX ||
    (v->client && (!v->initialized || !xideLanguageClientCanSuspendAnalysis(v->client))) ||
    (v->autoView && v->autoView->session==UINT64_MAX) || !xideCompletionSourcePreflight(app)){
  if(error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"Language sources are not ready for a coordinated native analysis lease.");}return 0;
 }
 ++v->highlightSession;if(v->deferredMessage)v->deferredDiscard=1;
 uint64_t token=++v->sourceSerial;v->sourceToken=token;v->sourceClientToken=0;v->sourceInvalid=v->sourceReset=v->sourceResuming=v->sourceFailed=v->sourceStop=0;
 v->sourceAutoPaused=v->autoView && v->autoView->paused;v->sourceColorPaused=v->colors && v->colors->paused;
 if(v->client && !(v->sourceClientToken=xideLanguageClientSuspendAnalysis(v->client,error)))v->sourceFailed=1;
 if(!xideCompletionSourceSuspend(app,token))v->sourceFailed=1;
 cancelReferences(v);workspaceCancel(v);colorsStop(v);clearDiagnostics(app);v->busy=v->waitingInitialize=v->requestId=0;
 v->pendingSave=NULL;v->recheckAt=v->semanticRefreshAt=v->checkRetryAt=v->preemptDeadline=0;v->invalidated=1;v->onDemandMode=0;
 if(v->autoView){
  xide_automatic_diagnostics* a=v->autoView;automaticPause(v);
  if(!xideDiagnosticStreamReset(a->stream,++a->session))v->sourceFailed=1;
  /* Preserve sync/model/serial knowledge of the live server. Old proof/body
     work is quarantined; resumption republishes complete fresh contributions. */
  for(size_t i=0;i<a->groupCount;++i){automaticClearAnnotation(a->groups+i);a->groups[i].valid=0;a->groups[i].checkedEpoch=0;}
  a->routeCount=0;a->problemsDirty=1;xideTaskViewClearLanguageProblemsOwned(app,a);sourceCleanup(v);
 }
 xgeRenderRequest();return token;
#endif
}
int xideLanguageSourcesState(xide_app* app,uint64_t token,xide_language_sources_status* out)
{
 xide_language_view* v=app?app->languageView:NULL;xide_language_sources_status child={0};
 if(!v || !out || !token || v->sourceToken!=token || !xideCompletionSourceState(app,token,&child))return 0;
 int quiet=!v->client,resumed=!v->client,invalid=v->sourceInvalid,failed=v->sourceFailed;
 if(v->client){int remoteInvalid=0;enum xide_language_lease_state s=xideLanguageClientAnalysisState(v->client,v->sourceClientToken,&remoteInvalid);invalid|=remoteInvalid;failed|=s==XIDE_LS_LEASE_FAILED || s==XIDE_LS_LEASE_STALE;quiet=s==XIDE_LS_LEASE_HELD || (failed && xideLanguageClientStopped(v->client));resumed=v->sourceResuming && (v->sourceClientToken?xideLanguageClientAnalysisCanRelease(v->client,v->sourceClientToken):xideLanguageClientStopped(v->client));}
 xide_automatic_diagnostics* a=v->autoView;
 int local=(!a || (!a->batch && !a->held && !a->incoming && !a->retirement && !a->garbage.notices && !a->garbage.batch && !a->garbage.root && !a->garbage.result && xideDiagnosticStreamIdle(a->stream))) &&
  (!v->workspaceSearch || xideWorkspaceSearchDecoderIdle(v->workspaceSearch->decoder));
 *out=(xide_language_sources_status){.quiet=!v->sourceResuming && quiet && child.quiet && local,.resumed=v->sourceResuming && resumed && child.resumed && local,
  .invalidated=invalid || child.invalidated,.failed=failed || child.failed,.resuming=v->sourceResuming};return 1;
}
int xideLanguageSourcesResume(xide_app* app,uint64_t token)
{
 xide_language_view* v=app?app->languageView:NULL;if(!v || !token || v->sourceToken!=token)return 0;
 v->sourceResuming=1;int okay=xideCompletionSourceResume(app,token);
 if(v->client){if(v->sourceReset || !v->sourceClientToken)xideLanguageClientRequestStop(v->client);okay=(!v->sourceClientToken || xideLanguageClientResumeAnalysis(v->client,v->sourceClientToken)) && okay;}
 return okay;
}
int xideLanguageSourcesRelease(xide_app* app,uint64_t token)
{
 xide_language_view* v=app?app->languageView:NULL;xide_language_sources_status s={0};
 if(!xideLanguageSourcesState(app,token,&s) || !v->sourceResuming || !s.resumed)return 0;
 enum xide_language_lease_state own=v->client?xideLanguageClientAnalysisState(v->client,v->sourceClientToken,NULL):XIDE_LS_LEASE_RESUMED;
 int resetClient=v->sourceReset || v->sourceFailed || own==XIDE_LS_LEASE_FAILED || own==XIDE_LS_LEASE_STALE;
 /* Both sides have already acknowledged (or physically stopped). No owner
    event dispatch/reentrancy occurs between the two admission releases. */
 if(v->client && v->sourceClientToken && !xideLanguageClientReleaseAnalysis(v->client,v->sourceClientToken))return 0;
 if(!xideCompletionSourceRelease(app,token))return 0;
 v->sourceToken=0;v->sourceClientToken=0;v->sourceResuming=0;
 if(resetClient){xideLanguageClientRequestStop(v->client);if(xideLanguageClientStopped(v->client))reset(app);else v->sourceDrain=1;}
 if(!v->sourceStop && !v->sourceAutoPaused)automaticResume(v);else automaticPause(v);if(v->colors)v->colors->paused=v->sourceStop || v->sourceColorPaused;
 if(v->snapshotMode && v->automatic)v->recheckAt=GetTickCount64()+1;
 xgeRenderRequest();return 1;
}
