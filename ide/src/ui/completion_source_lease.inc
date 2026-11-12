int xideCompletionSourcePreflight(xide_app* app)
{
 xide_completion_view* v=app?app->completionView:NULL;
 return v && v->session!=UINT64_MAX && !v->sourceToken && !v->sourceDrain && (!v->client || (v->initialized && xideLanguageClientCanSuspendAnalysis(v->client)));
}
int xideCompletionSourceSuspend(xide_app* app,uint64_t token)
{
 xide_completion_view* v=app?app->completionView:NULL;if(!token || !v || v->sourceToken || v->sourceDrain)return 0;
 ++v->session;v->sourceToken=token;v->sourceInvalid=v->sourceReset=v->sourceResuming=v->sourceFailed=0;v->sourceClientToken=0;
 if(v->client && !(v->sourceClientToken=xideLanguageClientSuspendAnalysis(v->client,NULL)))v->sourceFailed=1;
 cvcancelSync(v);svdismiss(v);cvdismiss(v);return 1;
}
int xideCompletionSourceState(xide_app* app,uint64_t token,xide_language_sources_status* out)
{
 xide_completion_view* v=app?app->completionView:NULL;if(!out || !v || !token || v->sourceToken!=token)return 0;
 *out=(xide_language_sources_status){.quiet=!v->client,.resumed=!v->client,.invalidated=v->sourceInvalid,.failed=v->sourceFailed,.resuming=v->sourceResuming};
 if(v->client){int invalid=0;enum xide_language_lease_state s=xideLanguageClientAnalysisState(v->client,v->sourceClientToken,&invalid);out->invalidated|=invalid;out->failed|=s==XIDE_LS_LEASE_FAILED || s==XIDE_LS_LEASE_STALE;out->quiet=s==XIDE_LS_LEASE_HELD || (out->failed && xideLanguageClientStopped(v->client));out->resumed=v->sourceResuming && (v->sourceClientToken?xideLanguageClientAnalysisCanRelease(v->client,v->sourceClientToken):xideLanguageClientStopped(v->client));}
 if(v->sourceResuming)out->quiet=0;
 return 1;
}
int xideCompletionSourceResume(xide_app* app,uint64_t token)
{
 xide_completion_view* v=app?app->completionView:NULL;if(!v || !token || v->sourceToken!=token)return 0;
 v->sourceResuming=1;
 if(v->client){if(v->sourceReset || !v->sourceClientToken)xideLanguageClientRequestStop(v->client);return !v->sourceClientToken || xideLanguageClientResumeAnalysis(v->client,v->sourceClientToken);}
 return 1;
}
int xideCompletionSourceRelease(xide_app* app,uint64_t token)
{
 xide_completion_view* v=app?app->completionView:NULL;xide_language_sources_status s={0};
 if(!xideCompletionSourceState(app,token,&s) || !v->sourceResuming || !s.resumed)return 0;
 if(v->client && v->sourceClientToken && !xideLanguageClientReleaseAnalysis(v->client,v->sourceClientToken))return 0;
 int reset=v->sourceReset || s.failed;v->sourceToken=0;v->sourceClientToken=0;v->sourceResuming=0;
 if(reset){xideLanguageClientRequestStop(v->client);if(xideLanguageClientStopped(v->client))cvstopClient(v);else v->sourceDrain=1;}
 return 1;
}
