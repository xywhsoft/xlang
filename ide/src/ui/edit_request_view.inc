static int editModelAt(void* user,size_t index,xide_edit_request_model* out)
{
 xide_document_store* store=user;if(index>=store->count||!store->items[index])return 0;
 xide_document* d=store->items[index];*out=(xide_edit_request_model){d->id,d->revision,d->open,d->dirty,d->externalState,d->path,d->language};return 1;
}
static int editConfigurationCurrent(xide_language_view* v,const xvalue* catalog)
{
 const char* compiler=xideTaskViewCompiler(v->app);char* package=NULL;xide_model_error error={0};
 int okay=compiler&&v->compiler&&xidePathLexicalEqual(compiler,v->compiler)&&xideLanguageCheckPackagePath(&package,&error);
 if(okay)okay=package&&v->packagePath?!strcmp(package,v->packagePath):package==v->packagePath;
 xvalue* packet=okay?xideExecutionAnalysisPacket(v->app,package,1,&error):NULL;free(package);
 okay=packet&&xrtValueEqual(catalog,get(packet,"catalog"));xrtValueRelease(packet);return okay;
}
int xideLanguageEditRequestOwned(xide_app* app,const xide_edit_request* request)
{
 xide_language_view* v=app?app->languageView:NULL;
 return request&&v&&v->editRequest==request&&v->client&&v->client==v->editClient&&app->context&&app->documents.context==app->context&&!xideEditRequestInvalidated(request);
}
int xideLanguageEditRequestCurrent(xide_app* app,xide_edit_request* request)
{
 if(!xideLanguageEditRequestOwned(app,request))return 0;
 xide_language_view* v=app->languageView;
 if(xideLanguageClientState(v->client,NULL,0)>=XIDE_LS_STOPPED){xideEditRequestInvalidate(request,XIDE_EDIT_REQUEST_CONNECTION);return 0;}
 if(app->documents.count!=xideEditRequestModelCount(request)){xideEditRequestInvalidate(request,XIDE_EDIT_REQUEST_DOCUMENT);return 0;}
 for(size_t i=0;i<app->documents.count;++i){xide_edit_request_model m={0};if(!editModelAt(&app->documents,i,&m)||!xideEditRequestModelMatches(request,i,&m)){xideEditRequestInvalidate(request,XIDE_EDIT_REQUEST_DOCUMENT);return 0;}}
 if(!editConfigurationCurrent(v,xideEditRequestCatalog(request))){xideEditRequestInvalidate(request,XIDE_EDIT_REQUEST_CONFIGURATION);return 0;}
 return !xideEditRequestInvalidated(request);
}
xide_edit_request* xideLanguageEditRequestCapture(xide_app* app,xide_model_error* error)
{
 if(error)*error=(xide_model_error){0};
 xide_language_view* v=app?app->languageView:NULL;char digest[65];size_t count=0;
 int okay=v&&app->context&&app->documents.context==app->context&&app->documents.count<=4096&&!v->editRequest&&v->editSerial<UINT64_MAX&&v->nextId<INT_MAX&&v->initialized&&v->nativeBackend&&v->onDemandSupported&&v->client&&xideLanguageClientState(v->client,NULL,0)==XIDE_LS_RUNNING&&v->sync&&v->analysisCatalog&&v->receiptReady&&!v->receiptRequest&&!v->syncMode&&!v->sourceToken&&!v->sourceDrain&&!v->busy&&snapshotCurrent(v)&&xlsSyncInputState(v->sync,digest,&count)&&count==v->receiptDocuments&&!strcmp(digest,v->receiptDigest);
 if(okay&&v->autoEnabled)okay=v->autoView&&v->autoView->syncAdopted&&v->autoView->receiptSerial==v->autoView->sentSerial&&!automaticWatchPending(v);
 for(size_t i=0;okay&&i<count;++i){xls_sync_binding b={0};document_binding* d=i<v->inputCount?v->inputs+i:NULL;okay=d&&xlsSyncBindingAt(v->sync,i,&b)&&b.identity==d->id&&b.revision==d->revision&&b.version==d->wireVersion&&!strcmp(b.uri,d->uri);}
 if(okay)okay=editConfigurationCurrent(v,v->analysisCatalog);
 if(!okay){if(error){error->code=XIDE_MODEL_CONFLICT;snprintf(error->message,sizeof(error->message),"Edit request requires an unused ID and the current acknowledged native input scope.");}return NULL;}
 xide_edit_request* request=xideEditRequestCreate(v->editSerial+1,v->nextId+1,v->sync,v->analysisCatalog,app->documents.count,editModelAt,&app->documents,error);
 if(!request)return NULL;
 v->editRequest=xideEditRequestRetain(request);if(!v->editRequest){xideEditRequestRelease(request);return NULL;}
 ++v->editSerial;++v->nextId;v->editClient=v->client;return request;
}
int xideLanguageEditRequestEnd(xide_app* app,xide_edit_request* request)
{
 xide_language_view* v=app?app->languageView:NULL;if(!request||!v||v->editRequest!=request)return 0;
 xideEditRequestInvalidate(request,XIDE_EDIT_REQUEST_ENDED);v->editRequest=NULL;v->editClient=NULL;xideEditRequestRelease(request);return 1;
}
