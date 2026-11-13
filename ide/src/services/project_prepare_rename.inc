/* Local native preparation only. Routing failures and negative importer probes
 * remain freshness inputs even though other workspace scopes are not checked. */
static int prepareGuard(reference_output* out,xide_language_input_guard* guard)
{
 if(!guard || !charge(&out->guardBytes,xideLanguageInputGuardReportBytes(guard),PROOF_BYTES))return fail(out,"Prepare rename guard budget exceeded.");
 guard=xideLanguageInputGuardRetain(guard);
 if(!xrtArrayPush(&out->guards,&guard)){xideLanguageInputGuardRelease(guard);return fail(out,"Cannot retain prepare rename input guard.");}
 return out->guards.Capacity<=PROOF_BYTES/sizeof(guard) && out->guardBytes<=PROOF_BYTES-out->guards.Capacity*sizeof(guard) ? 1 : fail(out,"Prepare rename guard storage exceeded.");
}
static int prepareDependency(reference_output* out,xls_dependency_capture* dependency,xcancel* cancel)
{
 xls_check_inputs inputs;
 if(!dependency || !xlsDependencyCaptureInputs(dependency,&inputs) || inputs.sources>65536 || inputs.probes>65536)return fail(out,"Cannot retain complete rename provider routing inputs.");
 size_t bytes=0;
 for(size_t i=0;i<inputs.sources;++i){
  xls_check_source source;
  if(stopped(cancel) || !xlsDependencyCaptureSource(dependency,i,&source) || !source.path || !source.origin || !charge(&bytes,source.size,PROOF_BYTES) || !charge(&bytes,source.rawSize,PROOF_BYTES) || !charge(&bytes,sizeof(source),PROOF_BYTES) || !charge(&bytes,strlen(source.path)+1,PROOF_BYTES) || !charge(&bytes,strlen(source.origin)+1,PROOF_BYTES))return fail(out,"Prepare rename routing source budget exceeded.");
 }
 for(size_t i=0;i<inputs.probes;++i){xls_check_probe probe;if(stopped(cancel) || !xlsDependencyCaptureProbe(dependency,i,&probe) || !probe.path || !charge(&bytes,sizeof(probe),PROOF_BYTES) || !charge(&bytes,strlen(probe.path)+1,PROOF_BYTES))return fail(out,"Prepare rename routing probe budget exceeded.");}
 if(inputs.packagePath && !charge(&bytes,strlen(inputs.packagePath)+1,PROOF_BYTES))return fail(out,"Prepare rename package metadata exceeded.");
 if(!charge(&out->prepareBytes,bytes,PROOF_BYTES))return fail(out,"Prepare rename routing storage exceeded.");
 dependency=xlsDependencyCaptureRetain(dependency);
 if(!xrtArrayPush(&out->prepareDependencies,&dependency)){xlsDependencyCaptureRelease(dependency);return fail(out,"Cannot retain prepare rename routing capture.");}
 return out->prepareDependencies.Capacity<=PROOF_BYTES/sizeof(dependency) && out->prepareBytes<=PROOF_BYTES-out->prepareDependencies.Capacity*sizeof(dependency) ? 1 : fail(out,"Prepare rename routing reference storage exceeded.");
}
static int prepareInputsValid(const reference_output* out,int (*cancelled)(void*),void* user,xide_model_error* error)
{
 if(!out || !out->prepareTarget || !out->family || !out->guards.Count)return 0;
 for(size_t i=0;i<out->prepareDependencies.Count;++i)if((cancelled && cancelled(user)) || !xlsDependencyCaptureValidate(((xls_dependency_capture**)out->prepareDependencies.Data)[i]))return 0;
 for(size_t i=0;i<out->guards.Count;++i)if((cancelled && cancelled(user)) || !xideLanguageInputGuardValidate(((xide_language_input_guard**)out->guards.Data)[i],cancelled,user,error))return 0;
 return !cancelled || !cancelled(user);
}
static int completePrepare(reference_input* in,reference_output* out,reference_record* record,const xide_reference_key* key,xcancel* cancel)
{
 enum xide_reference_family_error why;out->family=xideSemanticReferenceGraphFamily(record->graph,key,XIDE_REFERENCE_BYTES,stopped,cancel,&why);
 if(!out->family || xideSemanticReferenceFamilyRenamePolicy(out->family))return fail(out,"The selected local declaration component does not support semantic rename.");
 xide_reference_family_info info;
 if(!xideSemanticReferenceFamilyInfo(out->family,&info) || !info.members)return fail(out,"Missing native rename declaration component.");
 for(size_t i=0;i<info.members;++i){
  xide_semantic_source source;size_t start,end;uint64_t owners=0;
  if(stopped(cancel) || !xideSemanticReferenceFamilyMember(out->family,i,&source,&start,&end) || !(source.kind&1) || !xidePathHasExtension(source.path,".xl") || !xideRenameScopeOwners(in->renameScope,source.path,&owners) || !owners)return fail(out,"A local rename declaration is outside explicitly configured source membership.");
 }
 xide_rename_target_info at;
 if(!xideRenameTargetInfo(in->renameTarget,&at) || !prepareGuard(out,record->guard) || !charge(&out->prepareBytes,info.bytes,PROOF_BYTES) || !charge(&out->prepareBytes,at.bytes,PROOF_BYTES) || !charge(&out->prepareBytes,out->guardBytes,PROOF_BYTES) || !charge(&out->prepareBytes,out->guards.Capacity*sizeof(xide_language_input_guard*),PROOF_BYTES) || !charge(&out->prepareBytes,out->prepareDependencies.Capacity*sizeof(xls_dependency_capture*),PROOF_BYTES))return fail(out,"Prepare rename retained-input budget exceeded.");
 out->prepareTarget=xideRenameTargetRetain(in->renameTarget);xide_model_error error={0};
 return prepareInputsValid(out,stopped,cancel,&error) ? 1 : fail(out,"Rename preparation inputs changed before publication.");
}
