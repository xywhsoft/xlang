/* Guarded worker jobs and primitive history; never borrow an old AST. */
#define TOKEN_JOB_LIMIT 4u
#define TOKEN_HISTORY_BYTES (16u*1024u*1024u)
static void clearTokenHistory(server* s,int document)
{
 for(unsigned slot=0;slot<2;++slot){xide_token_data* data=s->tokenHistory[document][slot].data;
  if(data)s->tokenHistoryBytes-=xideTokenDataBytes(data);
  xideTokenDataRelease(data);memset(&s->tokenHistory[document][slot],0,sizeof(s->tokenHistory[document][slot]));
 }
}
static xide_token_data* previousTokenData(server* s,int document,const char* id)
{
 for(unsigned slot=0;slot<2;++slot){xide_token_data* data=s->tokenHistory[document][slot].data;
  if(data && !strcmp(id,xideTokenDataId(data)))return xideTokenDataRetain(data);
 }return NULL;
}
static void rememberTokenData(server* s,int document,xide_token_data* data)
{
 if(!data)return;
 size_t bytes=xideTokenDataBytes(data);if(bytes>TOKEN_HISTORY_BYTES){xideTokenDataRelease(data);return;}
 xide_token_data* old=s->tokenHistory[document][1].data;if(old)s->tokenHistoryBytes-=xideTokenDataBytes(old);xideTokenDataRelease(old);
 s->tokenHistory[document][1]=s->tokenHistory[document][0];memset(&s->tokenHistory[document][0],0,sizeof(s->tokenHistory[document][0]));
 while(bytes>TOKEN_HISTORY_BYTES-s->tokenHistoryBytes){
  size_t first=DOC_LIMIT,second=0;unsigned long long order=ULLONG_MAX;
  for(size_t i=0;i<DOC_LIMIT;++i)for(size_t j=0;j<2;++j)if(s->tokenHistory[i][j].data && (first==DOC_LIMIT || s->tokenHistory[i][j].order<order)){first=i;second=j;order=s->tokenHistory[i][j].order;}
  if(first==DOC_LIMIT){xideTokenDataRelease(data);return;}
  old=s->tokenHistory[first][second].data;s->tokenHistoryBytes-=xideTokenDataBytes(old);xideTokenDataRelease(old);memset(&s->tokenHistory[first][second],0,sizeof(s->tokenHistory[first][second]));
 }
 if(s->tokenHistoryClock==ULLONG_MAX){for(int i=0;i<DOC_LIMIT;++i)clearTokenHistory(s,i);s->tokenHistoryClock=0;}
 s->tokenHistory[document][0].data=data;s->tokenHistory[document][0].order=++s->tokenHistoryClock;s->tokenHistoryBytes+=bytes;
}
static void collectTokenJobs(server* s)
{
 for(size_t i=0;i<TOKEN_JOB_LIMIT;++i)if(s->tokenJobs[i] && xideSemanticTokenJobCancelled(s->tokenJobs[i]) && xideSemanticTokenJobFinished(s->tokenJobs[i])){xideSemanticTokenJobDestroy(s->tokenJobs[i]);s->tokenJobs[i]=NULL;}
}
static xvalue* tokenResult(server* s,semantic_request* request)
{
 if(request->tokenJob){
  if(!xideLanguageCheckMatchesTokenOwner(s->guard,request->tokenJob)){request->navigationFailed=1;request->failureCode=-32801;return NULL;}
  enum xide_token_job_state state=xideSemanticTokenJobState(request->tokenJob);
  if(state==XIDE_TOKEN_RUNNING){request->waitingNavigation=1;return NULL;}
  if(state==XIDE_TOKEN_READY){
   xvalue* result=xideSemanticTokenJobResult(request->tokenJob);
   if(result && request->token.mode!=XIDE_TOKENS_RANGE)rememberTokenData(s,request->document,xideSemanticTokenJobData(request->tokenJob));
   if(!result)request->navigationFailed=1;
   return result;
  }
  request->navigationFailed=1;request->failureCode=state==XIDE_TOKEN_STALE?-32801:state==XIDE_TOKEN_CANCELLED?-32800:-32603;return NULL;
 }
 size_t slot=TOKEN_JOB_LIMIT;
 for(size_t i=0;i<TOKEN_JOB_LIMIT;++i)if(!s->tokenJobs[i]){slot=i;break;}
 if(slot==TOKEN_JOB_LIMIT){request->waitingNavigation=1;return NULL;}
 if(s->tokenSerial==ULLONG_MAX){request->navigationFailed=1;return NULL;}
 document* d=&s->docs[request->document];
 snprintf(request->token.resultId,sizeof(request->token.resultId),"%llu.%llu",d->identity,++s->tokenSerial);
 int current=0;xide_model_error error={0};
 xide_semantic_token_job* job=xideLanguageCheckStartTokens(s->guard,d->path,request->version,&s->tokenLegend,&request->token,request->tokenPrevious,&current,&error);
 if(!job){if(error.code)request->navigationFailed=1;return current && !error.code?xrtValueNull():NULL;}
 request->tokenJob=job;s->tokenJobs[slot]=job;request->waitingNavigation=1;return NULL;
}
static int tokenOptions(server* s,int document,int kind,const xvalue* params,xide_token_request* options)
{
 options->mode=kind-4;
 if(options->mode==XIDE_TOKENS_RANGE){
  const xvalue *range=get(params,"range"),*start=get(range,"start"),*end=get(range,"end");xide_token_range* r=&options->range;r->enabled=1;
  if(!integer(get(start,"line"),&r->startLine) || !integer(get(start,"character"),&r->startCharacter) || !integer(get(end,"line"),&r->endLine) || !integer(get(end,"character"),&r->endCharacter) || !validPosition(&s->docs[document],r->startLine,r->startCharacter) || !validPosition(&s->docs[document],r->endLine,r->endCharacter) || r->startLine>r->endLine || (r->startLine==r->endLine && r->startCharacter>r->endCharacter))return 0;
 }
 return options->mode!=XIDE_TOKENS_DELTA || string(get(params,"previousResultId"))!=NULL;
}
