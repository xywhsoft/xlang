typedef struct rename_result_charge {size_t bytes,limit;int (*stopped)(void*);void* user;} rename_result_charge;
static bool renameResultCharge(xbytesview bytes,void* user)
{rename_result_charge* q=user;if((q->stopped && q->stopped(q->user)) || !charge(&q->bytes,bytes.Size,q->limit))return false;return true;}
xvalue* xideProjectReferencesRenameResult(xide_reference_query* q,size_t budget,int (*cancelled)(void*),void* user,xide_model_error* error)
{
 if(!budget)budget=RESULT_WIRE_BYTES;
 const reference_output* source=xideProjectReferencesResult(q,NULL,NULL,NULL,0)==XIDE_REFERENCES_READY?xrtFutureValue(q->future):NULL;
 xvalue *out=NULL,*guards=NULL;const char* message="No complete native rename candidate is available.";
 if(budget>RESULT_WIRE_BYTES || !source || !source->plan || !source->guards.Count || !source->stats.workspaceScopes || source->stats.workspaceCompleted!=source->stats.workspaceScopes || (cancelled && cancelled(user)))goto failed;
 out=xrtValueObject();guards=xrtValueArray();rename_result_charge used={0,budget,cancelled,user};xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=budget;
 message="Rename proof encoding was cancelled or exceeded aggregate encoding/allocation limits.";
 if(!out || !guards || !put(out,"schema",xrtValueInt(1)) || !put(out,"scopeCount",xrtValueInt((int64)source->stats.workspaceScopes)) || !put(out,"workspaceEdit",xideRenamePlanWorkspaceEdit(source->plan,1,cancelled,user)) || !xrtJsonWrite(out,&config,renameResultCharge,&used))goto failed;
 for(size_t i=0;i<source->guards.Count;++i){
  if((cancelled && cancelled(user)) || used.bytes>=budget)goto failed;
  xvalue* proof=xideLanguageInputGuardExport(((xide_language_input_guard**)source->guards.Data)[i],budget-used.bytes,cancelled,user,error);
  if(!proof)goto failed;
  int okay=xrtJsonWrite(proof,&config,renameResultCharge,&used) && xrtValueArrayAppend(guards,proof);xrtValueRelease(proof);if(!okay)goto failed;
 }
 if(!put(out,"inputGuards",guards)){guards=NULL;goto failed;}guards=NULL;used.bytes=0;
 if(!xrtJsonWrite(out,&config,renameResultCharge,&used))goto failed;
 /* Encoding can outlast the query's original publication sweep. Retain the
  * complete guard list and validate it after all result construction work. */
 for(size_t i=0;i<source->guards.Count;++i)
  if(!xideLanguageInputGuardValidate(((xide_language_input_guard**)source->guards.Data)[i],cancelled,user,error)){message="Rename source/resolution inputs changed after result encoding.";goto failed;}
 if(cancelled && cancelled(user))goto failed;
 return out;
failed:
 xrtValueRelease(guards);xrtValueRelease(out);if(error){error->code=XIDE_MODEL_INVALID;snprintf(error->message,sizeof(error->message),"%s",message);}return NULL;
}
