/* Whole-workspace rename reference collection on the original reference actor.
 * Children execute synchronously on this worker, never on its own task queue. */
#ifndef XIDE_REFERENCE_WORKSPACE_FENCE
#define XIDE_REFERENCE_WORKSPACE_FENCE(completed) ((void)(completed))
#endif
static int workspaceKey(xstrview key,const char* word)
{size_t n=strlen(word);return key.Size==n && !memcmp(key.Data,word,n);}
/* Both proofs were constructed from validated native rows. Their row keys
 * already prove the paths lexically equal. Ignore only the two path spellings;
 * preserve every other field, including unknown future source-report fields. */
static int workspaceDocumentSame(const xvalue* a,const xvalue* b,int document)
{
 if(a==b)return 1;
 if(!a || !b || xrtValueType(a)!=XVALUE_OBJECT || xrtValueType(b)!=XVALUE_OBJECT || xrtValueCount(a)!=xrtValueCount(b))return 0;
 for(size_t i=0;i<xrtValueCount(a);++i){xstrview key={0};const xvalue* left=xrtValueObjectAt(a,i,&key),*right=xrtValueObjectGet(b,key);if(!left || !right)return 0;
  if(workspaceKey(key,"path"))continue;
  if(document && workspaceKey(key,"input")){if(!workspaceDocumentSame(left,right,0))return 0;}
  else if(!xrtValueEqual(left,right))return 0;
 }
 return 1;
}
static int workspaceProofSame(const xvalue* a,const xvalue* b)
{return a==b || (a && b && xrtValueCount(a)==2 && xrtValueCount(b)==2 && xrtValueEqual(get(a,"schema"),get(b,"schema")) && workspaceDocumentSame(get(a,"document"),get(b,"document"),1));}
typedef struct workspace_charge {size_t bytes;xcancel* cancel;} workspace_charge;
static bool workspaceProofCharge(xbytesview bytes,void* user)
{workspace_charge* state=user;return !stopped(state->cancel) && charge(&state->bytes,bytes.Size,RESULT_BYTES);}
static result_row* workspaceNext(xarray* left,xarray* right,size_t* a,size_t* b)
{
 result_row *x=(result_row*)left->Data,*y=(result_row*)right->Data;
 return *a<left->Count && (*b>=right->Count || compareRow(x+*a,y+*b)<=0) ? x+(*a)++ : y+(*b)++;
}
static int workspaceMerge(reference_output* out,reference_output* part,xcancel* cancel)
{
 xarray merged={0};size_t count=0,duplicates=0;result_row *previous=NULL,*file=NULL;const xvalue* checked=NULL;
 workspace_charge used={0,cancel};xjsonwriteconfig json;xrtJsonWriteConfigInit(&json);json.MaxOutputBytes=RESULT_BYTES;
 /* Count and verify before moving ownership. Per-file proof pointer fast paths
    avoid repeatedly comparing a shared DOM for every occurrence. */
 for(size_t a=0,b=0;a<out->rows.Count || b<part->rows.Count;){
  if(stopped(cancel))return 0;
  result_row* row=workspaceNext(&out->rows,&part->rows,&a,&b);
  if(!file || xidePathNativeKeyCompare(file->key,row->key)){
   file=row;checked=row->proof;if(!row->proof || !xrtJsonWrite(row->proof,&json,workspaceProofCharge,&used))return fail(out,"Workspace source proof charge exceeded.");
  }else if(row->proof!=file->proof && row->proof!=checked){if(!workspaceProofSame(file->proof,row->proof))return fail(out,"Workspace scopes disagree on a reference document proof.");checked=row->proof;}
  if(previous && !compareRow(previous,row)){++duplicates;continue;}
  if(!charge(&used.bytes,strlen(row->path)+1+(wcslen(row->key)+1)*sizeof(wchar_t),RESULT_BYTES) || count>=RESULT_BYTES/sizeof(result_row))return fail(out,"Workspace reference storage budget exceeded.");
  previous=row;++count;
 }
 if(!xrtArrayInit(&merged,sizeof(result_row)) || !xrtArrayReserve(&merged,count))goto failed;
 if(merged.Capacity>RESULT_BYTES/sizeof(result_row) || used.bytes>RESULT_BYTES-merged.Capacity*sizeof(result_row))goto failed;
 previous=NULL;file=NULL;
 for(size_t a=0,b=0;a<out->rows.Count || b<part->rows.Count;){
  if(stopped(cancel))goto failed;
  result_row* row=workspaceNext(&out->rows,&part->rows,&a,&b);
  /* Compare with the last emitted row: earlier source rows have been moved. */
  result_row* last=merged.Count ? (result_row*)merged.Data+merged.Count-1 : NULL;
  if(last && !compareRow(last,row))continue;
  const xvalue* proof=last && !xidePathNativeKeyCompare(last->key,row->key) ? last->proof : row->proof;
  result_row take=*row;take.proof=xrtValueRetain((xvalue*)proof);take.ownedProof=1;
  if(!take.proof || !xrtArrayPush(&merged,&take)){xrtValueRelease((xvalue*)take.proof);goto failed;}
  if(row->ownedProof)xrtValueRelease((xvalue*)row->proof);
  *row=(result_row){0};
 }
 workspaceRowsFree(&out->rows);out->rows=merged;out->stats.workspaceDuplicates+=duplicates;return 1;
failed:workspaceRowsFree(&merged);return stopped(cancel) ? 0 : fail(out,"Workspace merge allocation or storage budget failed.");
}
static int workspaceGuards(reference_output* out,const reference_output* part)
{
 if(!part->guards.Count)return fail(out,"A workspace scope has no complete input proof.");
 for(size_t i=0;i<part->guards.Count;++i){xide_language_input_guard* guard=((xide_language_input_guard**)part->guards.Data)[i];size_t add=xideLanguageInputGuardReportBytes(guard);
  if(add==SIZE_MAX || !charge(&out->guardBytes,add,PROOF_BYTES))return fail(out,"Workspace input proof budget exceeded.");
  guard=xideLanguageInputGuardRetain(guard);if(!xrtArrayPush(&out->guards,&guard)){xideLanguageInputGuardRelease(guard);return fail(out,"Cannot retain workspace input guards.");}
  if(out->guards.Capacity>PROOF_BYTES/sizeof(guard) || out->guardBytes>PROOF_BYTES-out->guards.Capacity*sizeof(guard))return fail(out,"Workspace input guard storage budget exceeded.");
 }
 return 1;
}
static void workspaceStats(xide_project_reference_stats* all,const xide_project_reference_stats* part)
{
#define ADD(field) all->field+=part->field
 ADD(roots);ADD(captured);ADD(reused);ADD(memberTasks);ADD(ownerProbes);ADD(ownerRouteHits);ADD(coveredRoots);ADD(sourceRejections);ADD(coverageCacheRoots);
#undef ADD
 if(part->memberPeak>all->memberPeak)all->memberPeak=part->memberPeak;
 if(part->memberThreads>all->memberThreads)all->memberThreads=part->memberThreads;
 /* These are original cumulative worker snapshots, not additive work. */
 all->keyActorBuilds=part->keyActorBuilds;all->keyActorHits=part->keyActorHits;all->keyActorSessions=part->keyActorSessions;
 all->keyNavigationBuilds=part->keyNavigationBuilds;all->keyNavigationHits=part->keyNavigationHits;all->ownerActorBuilds=part->ownerActorBuilds;all->ownerActorHits=part->ownerActorHits;
 all->records=part->records;all->chargedBytes=part->chargedBytes;
}
#include "services/project_family_references.inc"
#include "services/project_rename_plan.inc"
#include "services/project_rename_recheck.inc"
static xtaskoutcome executeWorkspace(xcancel* cancel,void* data,xtaskvalue* value)
{
 reference_input* in=data;reference_output* out=xrtCalloc(1,sizeof(*out));if(!out)return XTASK_FAILED;
 out->stats.thread=xrtThreadCurrentId();out->stats.memberWorkers=in->store->memberCount;out->stats.workspaceScopes=xideRenameScopeCount(in->renameScope);
 if(!xrtArrayInit(&out->rows,sizeof(result_row)) || !xrtArrayInit(&out->guards,sizeof(xide_language_input_guard*)) || !xrtArrayInit(&out->graphs,sizeof(xide_reference_graph*)))goto failed;
 for(size_t i=0;i<out->stats.workspaceScopes;++i){
  if(stopped(cancel))goto done;
  reference_input child=*in;child.scope=(xls_context*)xideRenameScopeAt(in->renameScope,i);child.workspacePart=1;child.paths=(path_index){0};xtaskvalue result={0};
  xtaskoutcome state=execute(cancel,&child,&result);pathIndexUnit(&child.paths);reference_output* part=result.Value;
  int okay=state==XTASK_SUCCESS && part && !*part->error;
  if(okay){workspaceStats(&out->stats,&part->stats);
   if(in->familyMode){for(size_t j=0;okay && j<part->graphs.Count;++j)okay=appendGraph(out,((xide_reference_graph**)part->graphs.Data)[j]);}
   else okay=workspaceMerge(out,part,cancel);
   okay=okay && workspaceGuards(out,part);
  }
  else if(!stopped(cancel))fail(out,part && *part->error ? part->error : "A configured workspace scope did not complete.");
  if(result.Destroy)result.Destroy(result.Value,result.DestroyData);
  if(!okay)goto done;
  ++out->stats.workspaceCompleted;XIDE_REFERENCE_WORKSPACE_FENCE(out->stats.workspaceCompleted);
 }
 if(out->stats.workspaceCompleted!=out->stats.workspaceScopes)goto failed;
 XIDE_REFERENCE_WORKSPACE_FENCE(0);
 for(size_t i=0;i<out->guards.Count;++i){xide_model_error error={0};if(!xideLanguageInputGuardValidate(((xide_language_input_guard**)out->guards.Data)[i],stopped,cancel,&error)){if(!stopped(cancel))fail(out,"Workspace source/resolution inputs changed before publication.");goto done;}}
 if(in->familyMode){
  xide_reference_key* key=xideRenameTargetRetainKey(in->renameTarget);enum xide_reference_family_error why;
  out->family=xideSemanticReferenceGraphsFamily((xide_reference_graph**)out->graphs.Data,out->graphs.Count,key,PROOF_BYTES,stopped,cancel,&why);xideSemanticReferenceKeyRelease(key);
  if(!out->family){char message[128];snprintf(message,sizeof(message),"Cannot compose complete workspace declaration components (%d).",(int)why);fail(out,message);goto done;}
  xide_reference_family_info info;if(!xideSemanticReferenceFamilyInfo(out->family,&info))goto failed;
  for(size_t i=0;i<info.members;++i){xide_semantic_source source;size_t start,end;uint64_t owners=0;
   if(stopped(cancel))goto done;
   if(!xideSemanticReferenceFamilyMember(out->family,i,&source,&start,&end) || !(source.kind&1) || !xidePathHasExtension(source.path,".xl") || !xideRenameScopeOwners(in->renameScope,source.path,&owners) || !owners){fail(out,"A workspace family declaration is outside explicitly configured source membership.");goto done;}
  }
  if(in->familyMode==2 && !workspaceFamilyReferences(in,out,cancel))goto done;
  if(in->renameName && !workspaceRenamePlan(in,out,cancel))goto done;
  if(in->renameName && !workspaceRecheckPlan(out,cancel))goto done;
  /* Composition may outlast the earlier validation. Publish only after a
   * final sweep of every scope's guard, including zero-target scopes. */
  XIDE_REFERENCE_WORKSPACE_FENCE(SIZE_MAX);
  for(size_t i=0;i<out->guards.Count;++i){xide_model_error error={0};if(!xideLanguageInputGuardValidate(((xide_language_input_guard**)out->guards.Data)[i],stopped,cancel,&error)){if(!stopped(cancel))fail(out,"Workspace source/resolution inputs changed after declaration composition.");goto done;}}
 }else if(!locations(&out->rows,out))goto failed;
 out->stats.workspaceGuardBytes=out->guardBytes+out->guards.Capacity*sizeof(xide_language_input_guard*);
 goto done;
failed:fail(out,"Workspace reference allocation or query budget failed.");
done:
 if(stopped(cancel)){freeOutput(out,NULL);return XTASK_CANCELLED;}
 if((!out->locations && !out->family) || *out->error){if(!*out->error)fail(out,"Workspace reference collection did not complete.");xrtValueRelease(out->locations);out->locations=NULL;xideSemanticReferenceFamilyRelease(out->family);out->family=NULL;xideRenamePlanRelease(out->plan);out->plan=NULL;}
 *value=(xtaskvalue){out,freeOutput,NULL};return XTASK_SUCCESS;
}
