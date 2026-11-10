/* Complete per-provider candidate checking, on the existing project actor.
 * Never submit synchronous work back to this actor's own task queue. */
#include "services/semantic_binding_check.h"
#include "analysis/frozen_inputs.h"
typedef struct recheck_document {xide_rename_plan_document view;wchar_t* key;} recheck_document;
static int recheckDocumentOrder(const void* a,const void* b)
{return xidePathNativeKeyCompare(((const recheck_document*)a)->key,((const recheck_document*)b)->key);}
static const recheck_document* recheckDocument(const recheck_document* rows,size_t count,const wchar_t* key)
{
 size_t low=0,high=count;while(low<high){size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(rows[mid].key,key)<0)low=mid+1;else high=mid;}
 return low<count && !xidePathNativeKeyCompare(rows[low].key,key)?rows+low:NULL;
}
static int workspaceRecheckOwner(reference_output* out,const xls_context* context,
 xide_reference_shard* before,const recheck_document* documents,size_t documentCount,xcancel* cancel)
{
 int okay=0;size_t count=xideSemanticReferenceShardSources(before),inputCount=0,changed=0;
 xls_semantic_input* inputs=NULL;xide_semantic_source* sources=NULL;
 xls_semantic_check* check=NULL;xide_reference_shard* after=NULL;
 if(!context || !count || count>XLS_FROZEN_INPUT_FILES)return fail(out,"Rename provider lacks an exact bounded native context/source bank.");
 inputs=xrtCalloc(count,sizeof(*inputs));sources=xrtCalloc(count,sizeof(*sources));if(!inputs || !sources)goto done;
 for(size_t i=0;i<count;++i){
  if(stopped(cancel))goto done;
  xide_semantic_source source;if(!xideSemanticReferenceShardSource(before,i,&source))goto done;
  if(!(source.kind&1))continue;
  wchar_t* key=xidePathNativeWide(source.path);if(!key)goto done;
  const recheck_document* edited=recheckDocument(documents,documentCount,key);free(key);
  const char* text=source.text;size_t bytes=strlen(text);
  if(edited){text=edited->view.after.Data;bytes=edited->view.after.Size;++changed;}
  inputs[inputCount++]=(xls_semantic_input){source.path,text,bytes};
 }
 xide_binding_check_info info={0};enum xide_binding_check_result binding;
 if(!changed){
  /* Exact source/configuration/resolution guards remain mandatory outside.
   * The independent comparer also proves no plan file was missed by this
   * prepared index, rather than trusting only its changed counter. */
  binding=xideSemanticBindingsPreserved(before,before,out->plan,XIDE_REFERENCE_BYTES,stopped,cancel,&info);
  if(binding!=XIDE_BINDING_PRESERVED)goto rejected;
  ++out->stats.renameUnchanged;
 }else{
  char error[512]={0};const xls_context_spec* spec=xlsContextSpec(context);
  check=xlsSemanticCheckCreateFrozenInContext((xls_context*)context,spec->entry,inputs,inputCount,1,stopped,cancel,error,sizeof(error));
  if(!check || !xlsSemanticCheckInfo(check).accepted){
   if(!stopped(cancel)){char message[512];xls_check_diagnostic diagnostic={0};
    if(check && xlsSemanticCheckDiagnostic(check,0,&diagnostic))snprintf(message,sizeof(message),"Rename candidate fails native checking: %.380s",diagnostic.message);
    else snprintf(message,sizeof(message),"Rename candidate native recheck failed: %.380s",error);
    fail(out,message);}
   goto done;
  }
  xls_check_inputs captured;if(!xlsSemanticCheckInputs(check,&captured) || captured.sources>XLS_FROZEN_INPUT_FILES)goto done;
  if(captured.sources>count){xide_semantic_source* larger=xrtCalloc(captured.sources,sizeof(*larger));if(!larger)goto done;xrtFree(sources);sources=larger;}
  for(size_t i=0;i<captured.sources;++i){xls_input_source source;if(stopped(cancel) || !xlsSemanticCheckSource(check,i,&source))goto done;sources[i]=(xide_semantic_source){source.path,source.text,source.kind,0};}
  after=xideSemanticReferenceShardCreate(NULL,check,sources,captured.sources,XIDE_REFERENCE_BYTES,NULL);if(!after)goto done;
  binding=xideSemanticBindingsPreserved(before,after,out->plan,XIDE_REFERENCE_BYTES,stopped,cancel,&info);
  if(binding!=XIDE_BINDING_PRESERVED)goto rejected;
  if(!xlsSemanticCheckValidate(check))goto done;
  ++out->stats.renameRechecks;
 }
 out->stats.renameSources+=info.sources;out->stats.renameReferences+=info.comparedReferences;okay=1;goto done;
rejected:
 if(!stopped(cancel)){char message[128];snprintf(message,sizeof(message),"Rename candidate does not preserve complete native source bindings (%d).",(int)binding);fail(out,message);}
done:
 xideSemanticReferenceShardRelease(after);xlsSemanticCheckDestroy(check);xrtFree(inputs);xrtFree(sources);
 return okay?1:stopped(cancel)?0:*out->error?0:fail(out,"Rename candidate native source capture/allocation failed.");
}
static int workspaceRecheckPlan(reference_output* out,xcancel* cancel)
{
 xide_rename_plan_info info;recheck_document* documents=NULL;size_t prepared=0,bytes=0;int okay=0;
 if(!out->plan || !xideRenamePlanInfo(out->plan,&info) || out->graphs.Count!=out->guards.Count || !out->graphs.Count ||
    info.documents>RESULT_BYTES/sizeof(*documents))goto done;
 bytes=info.documents*sizeof(*documents);documents=xrtCalloc(info.documents,sizeof(*documents));if(!documents)goto done;
 for(size_t i=0;i<info.documents;++i){
  if(stopped(cancel))goto done;
  recheck_document* row=documents+prepared++;if(!xideRenamePlanDocumentAt(out->plan,i,&row->view))goto done;
  row->key=xidePathNativeWide(row->view.path);if(!row->key || !charge(&bytes,(wcslen(row->key)+1)*sizeof(wchar_t),RESULT_BYTES))goto done;
 }
 qsort(documents,info.documents,sizeof(*documents),recheckDocumentOrder);
 for(size_t i=0;i<out->graphs.Count;++i){
  if(stopped(cancel))goto done;
  xide_language_input_guard* guard=((xide_language_input_guard**)out->guards.Data)[i];xide_model_error error={0};
  if(!xideLanguageInputGuardValidate(guard,stopped,cancel,&error)){if(!stopped(cancel))fail(out,"Rename provider source/resolution inputs changed before candidate checking.");goto done;}
  xide_reference_shard* before=xideSemanticReferenceGraphRetainShard(((xide_reference_graph**)out->graphs.Data)[i]);
  int accepted=workspaceRecheckOwner(out,xideLanguageInputGuardContext(guard),before,documents,info.documents,cancel);xideSemanticReferenceShardRelease(before);if(!accepted)goto done;
  XIDE_REFERENCE_WORKSPACE_FENCE(SIZE_MAX-1);
  if(!xideLanguageInputGuardValidate(guard,stopped,cancel,&error)){if(!stopped(cancel))fail(out,"Rename provider source/resolution inputs changed during candidate checking.");goto done;}
 }
 okay=1;
done:
 for(size_t i=0;i<prepared;++i)free(documents[i].key);
 xrtFree(documents);
 return okay?1:stopped(cancel)?0:*out->error?0:fail(out,"Rename candidate recheck index/quota failed.");
}
