/* Cursor selection and full rename planning share the existing reference
 * coordinator. Never submit a second task to its own single-worker queue. */
static xtaskoutcome executeRenameAt(xcancel* cancel,void* data,xtaskvalue* value)
{
 reference_input* in=data;reference_output* seed=xrtCalloc(1,sizeof(*seed));
 if(!seed)return XTASK_FAILED;
 xarray roots={0};size_t bytes=0,requested=SIZE_MAX;owner_proof proof={0};
 reference_job job={0};reference_record* record=NULL;xide_reference_key* key=NULL;
 seed->stats.thread=xrtThreadCurrentId();seed->stats.memberWorkers=in->store->memberCount;
 if(in->renameAt==2 && (!xrtArrayInit(&seed->guards,sizeof(xide_language_input_guard*)) || !xrtArrayInit(&seed->prepareDependencies,sizeof(xls_dependency_capture*))))goto failed;
 const xls_context_spec* spec=xlsContextSpec(in->scope);
 if(!xrtArrayInit(&roots,sizeof(root)) || !addRoot(&roots,spec->directory,spec->entry,&bytes))goto failed;
 for(size_t i=0;i<spec->lists[XLS_CONTEXT_MEMBERS].count;++i){
  if(stopped(cancel))goto done;
  if(!addRoot(&roots,spec->directory,spec->lists[XLS_CONTEXT_MEMBERS].items[i],&bytes))goto failed;
 }
 if(!xrtArraySort(&roots,compareRoot))goto failed;
 root* all=(root*)roots.Data;
 for(size_t i=0;i<roots.Count;++i)if(same(all[i].path,in->path)){requested=i;break;}
 if(!selectOwner(in,all,roots.Count,&requested,cancel,&proof,seed))goto done;
 if(requested==SIZE_MAX){fail(seed,"No explicitly configured provider owns the rename cursor.");goto done;}
 const char* paths[64];for(size_t i=0;i<in->count;++i)paths[i]=in->docs[i].path;
 pathIndexBuild(&in->paths,paths,in->count);
 if(stopped(cancel))goto done;
 beginAnalysis(in,all[requested].path,1,in->store->keys,&job);
 record=finishAnalysis(in,&job,1,cancel,&key,seed);
 if(!record || !in->renameTarget)goto done;
 if(ownsSource(record,in->path)!=1){fail(seed,"Selected checked provider does not contain the rename cursor source.");goto done;}
 uint64_t owners=0;
 if(!xideRenameScopeTarget(in->renameScope,in->renameTarget,&owners) || !owners){fail(seed,"Rename declaration is outside explicitly configured membership.");goto done;}
 cacheRecord(in->store,record);++seed->stats.roots;
 if(stopped(cancel))goto done;
 if(in->renameAt==2){
  if(completePrepare(in,seed,record,key,cancel)){*value=(xtaskvalue){seed,freeOutput,NULL};seed=NULL;}
  goto done;
 }
 /* Ordinary scope children now consume the exact immutable target. */
 in->renameAt=0;
 xtaskoutcome state=executeWorkspace(cancel,in,value);
 reference_output* out=value->Value;
 if(state==XTASK_SUCCESS && out && !*out->error){
  xide_model_error error={0};
  if(!ownerProofValid(&proof,cancel) || !overlayCurrent(record,in,cancel) || !xideLanguageInputGuardValidate(record->guard,stopped,cancel,&error))fail(out,"Rename cursor provider changed before candidate publication.");
  else if(!charge(&out->guardBytes,xideLanguageInputGuardReportBytes(record->guard),PROOF_BYTES))fail(out,"Rename cursor guard budget exceeded.");
  else{
   xide_language_input_guard* guard=xideLanguageInputGuardRetain(record->guard);
   if(!xrtArrayPush(&out->guards,&guard)){xideLanguageInputGuardRelease(guard);fail(out,"Cannot retain rename cursor guard.");}
   else if(out->guards.Capacity>PROOF_BYTES/sizeof(guard) || out->guardBytes>PROOF_BYTES-out->guards.Capacity*sizeof(guard))fail(out,"Rename cursor guard storage budget exceeded.");
   else out->stats.workspaceGuardBytes=out->guardBytes+out->guards.Capacity*sizeof(guard);
  }
  out->stats.roots+=seed->stats.roots;out->stats.captured+=seed->stats.captured;
  out->stats.ownerProbes+=seed->stats.ownerProbes;out->stats.ownerRouteHits+=seed->stats.ownerRouteHits;
#define CURSOR_TOTAL(field) if(out->stats.field<seed->stats.field)out->stats.field=seed->stats.field
  CURSOR_TOTAL(keyActorBuilds);CURSOR_TOTAL(keyActorHits);CURSOR_TOTAL(keyActorSessions);
  CURSOR_TOTAL(keyNavigationBuilds);CURSOR_TOTAL(keyNavigationHits);
  CURSOR_TOTAL(ownerActorBuilds);CURSOR_TOTAL(ownerActorHits);
#undef CURSOR_TOTAL
 }
 goto done;
failed:fail(seed,"Cannot prepare bounded native rename cursor analysis.");
done:
 xideSemanticReferenceKeyRelease(key);releaseRecord(record);closeJob(&job);
 xlsDependencyCaptureRelease(proof.dependency);xideLanguageInputGuardRelease(proof.guard);
 for(size_t i=0;i<roots.Count;++i){root* r=(root*)roots.Data+i;free(r->path);free(r->key);xrtFree(r->rejection);releaseRecord(r->cached);}
 xrtArrayUnit(&roots);
 if(!value->Value && !stopped(cancel)){if(!*seed->error)fail(seed,"Native rename cursor analysis did not complete.");*value=(xtaskvalue){seed,freeOutput,NULL};seed=NULL;}
 if(seed)freeOutput(seed,NULL);
 if(stopped(cancel)){if(value->Destroy)value->Destroy(value->Value,value->DestroyData);*value=(xtaskvalue){0};return XTASK_CANCELLED;}
 return value->Value ? XTASK_SUCCESS : XTASK_FAILED;
}
