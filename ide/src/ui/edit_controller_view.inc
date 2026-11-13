static void resetEditController(xide_language_view* v)
{
 if(!v||!v->editManaged)return;
 xideEditControllerCancelSend(v->editController,v->client);xideEditControllerDrop(v->editController);
 xideLanguageEditRequestEnd(v->app,v->editRequest);v->editManaged=v->editPublished=0;
}
static void pollEditController(xide_language_view* v)
{
 xide_edit_controller_status s={0};if(!xideEditControllerStatus(v->editController,&s))return;
 if(s.state==XIDE_EDIT_IDLE&&!s.active&&v->editJob.owner&&!xideSourceJobsHeld(&v->app->sourceJobs))xideSourceJobUnlink(&v->editJob);
 if(!v->editManaged)return;
 int invalid=!s.request||s.request!=v->editRequest||xideEditRequestInvalidated(s.request)||xideLanguageEditRequestConnection(v->app,s.request)!=xideLanguageClientIdentity(v->client);
 if(invalid||s.state==XIDE_EDIT_CANCELLED||s.state==XIDE_EDIT_REJECTED||s.state==XIDE_EDIT_REMOTE_ERROR){
  if(*s.error)snprintf(v->message,sizeof(v->message),"重命名候选未就绪：%.255s",s.error);
  if(v->sourceToken||!v->client||xideLanguageClientState(v->client,NULL,0)>=XIDE_LS_STOPPED||xideEditControllerCancelSend(v->editController,v->client))resetEditController(v);
  else xgeRenderRequestAfter(.02f);
  return;
 }
 if(s.state==XIDE_EDIT_PREPARED){
  if(!xideLanguageEditRequestCurrent(v->app,v->editRequest)){xideEditControllerCancel(v->editController);xgeRenderRequest();return;}
  if(!xideEditControllerSend(v->editController,v->client))xgeRenderRequestAfter(.02f);
 }else if(s.state==XIDE_EDIT_READY&&!v->editPublished){
  if(xideLanguageEditRequestCurrent(v->app,v->editRequest)&&xideEditCandidateCurrent(s.candidate))v->editPublished=1;
  else {xideEditControllerCancel(v->editController);xgeRenderRequest();}
 }
}
int xideLanguageEditCandidateBegin(xide_app* app,const char* uri,int line,int character,xstrview name,xide_model_error* error)
{
 xide_language_view* v=app?app->languageView:NULL;xide_edit_controller_status s={0};
 if(!v||!v->renameSupported||v->editManaged||v->editJob.owner||v->sourceToken||v->sourceDrain||!xideEditControllerStatus(v->editController,&s)||s.state!=XIDE_EDIT_IDLE||s.active||s.paused||!xideSourceJobsBegin(&app->sourceJobs,error))return 0;
 xide_edit_request* p=xideLanguageEditRequestCapture(app,error);int okay=p&&xideSourceJobLink(&app->sourceJobs,&v->editJob)&&xideSourceJobBindProbe(&v->editJob,xideEditControllerQuiet,v->editController)&&xideEditControllerSubmit(v->editController,p,xideLanguageEditRequestConnection(app,p),uri,line,character,name);
 if(okay){v->editManaged=1;v->editPublished=0;}else{if(p)xideLanguageEditRequestEnd(app,p);xideSourceJobUnlink(&v->editJob);}
 xideSourceJobsEnd(&app->sourceJobs);xideEditRequestRelease(p);if(okay)xgeRenderRequest();return okay;
}
const xide_edit_candidate* xideLanguageEditCandidate(xide_app* app)
{
 xide_language_view* v=app?app->languageView:NULL;xide_edit_controller_status s={0};
 if(!v||!v->editManaged||!v->editPublished||!xideEditControllerStatus(v->editController,&s)||s.state!=XIDE_EDIT_READY||!xideEditCandidateCurrent(s.candidate)||!xideLanguageEditRequestCurrent(app,v->editRequest))return NULL;
 return s.candidate;
}
int xideLanguageEditCandidateEnd(xide_app* app)
{xide_language_view* v=app?app->languageView:NULL;if(!v||!v->editManaged)return 0;xideEditControllerCancel(v->editController);pollEditController(v);return 1;}
