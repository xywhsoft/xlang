typedef struct plan_source {xide_semantic_source source;wchar_t* key;} plan_source;
static int planSourceOrder(const void* a,const void* b){return xidePathNativeKeyCompare(((const plan_source*)a)->key,((const plan_source*)b)->key);}
static int workspaceRenamePlan(reference_input* in,reference_output* out,xcancel* cancel)
{
 xarray sources={0};xide_rename_plan_input* inputs=NULL;xide_rename_plan_change* edits=NULL;int okay=0;size_t total=0,keys=0,files=0;
 if(!out->rows.Count || out->rows.Count>65536 || !xrtArrayInit(&sources,sizeof(plan_source)))goto done;
 for(size_t i=0;i<out->graphs.Count;++i){xide_reference_graph_info info;if(!xideSemanticReferenceGraphInfo(((xide_reference_graph**)out->graphs.Data)[i],&info) || !charge(&total,info.sources,RESULT_BYTES/sizeof(plan_source)))goto done;}
 if(!xrtArrayReserve(&sources,total) || sources.Capacity>RESULT_BYTES/sizeof(plan_source))goto done;
 for(size_t i=0;i<out->graphs.Count;++i){
  if(stopped(cancel))goto done;
  xide_reference_shard* shard=xideSemanticReferenceGraphRetainShard(((xide_reference_graph**)out->graphs.Data)[i]);size_t n=xideSemanticReferenceShardSources(shard);int complete=1;
  for(size_t j=0;j<n;++j){plan_source row={0};if(stopped(cancel) || !xideSemanticReferenceShardSource(shard,j,&row.source)){complete=0;break;}row.key=xidePathNativeWide(row.source.path);if(!row.key || !charge(&keys,(wcslen(row.key)+1)*sizeof(wchar_t),RESULT_BYTES-sources.Capacity*sizeof(plan_source)) || !xrtArrayPush(&sources,&row)){free(row.key);complete=0;break;}}
  xideSemanticReferenceShardRelease(shard);if(!complete)goto done;
 }
 if(!xrtArraySort(&sources,planSourceOrder))goto done;
 for(size_t i=0;i<out->rows.Count;++i)if(!i || xidePathNativeKeyCompare(((result_row*)out->rows.Data)[i-1].key,((result_row*)out->rows.Data)[i].key))++files;
 if(!charge(&keys,files*sizeof(*inputs)+out->rows.Count*sizeof(*edits),RESULT_BYTES-sources.Capacity*sizeof(plan_source)))goto done;
 inputs=xrtCalloc(files,sizeof(*inputs));edits=xrtMalloc(out->rows.Count*sizeof(*edits));if(!inputs || !edits)goto done;
 size_t document=0;
 for(size_t i=0;i<out->rows.Count;++i){
  if(stopped(cancel))goto done;
  const result_row* row=(result_row*)out->rows.Data+i;
  if(!i || xidePathNativeKeyCompare(row[-1].key,row->key)){
   if(i)++document;
   size_t low=0,high=sources.Count;while(low<high){size_t mid=low+(high-low)/2;if(xidePathNativeKeyCompare(((plan_source*)sources.Data)[mid].key,row->key)<0)low=mid+1;else high=mid;}
   if(low>=sources.Count || xidePathNativeKeyCompare(((plan_source*)sources.Data)[low].key,row->key))goto done;
   inputs[document]=(xide_rename_plan_input){((plan_source*)sources.Data)[low].source,get(row->proof,"document")};inputs[document].source.path=row->path;
  }
  edits[i]=(xide_rename_plan_change){document,row->start,row->end};
 }
 xide_rename_target_info target;enum xide_rename_plan_error why;if(!xideRenameTargetInfo(in->renameTarget,&target))goto done;
 out->plan=xideRenamePlanCreate(inputs,files,edits,out->rows.Count,target.name,sv(in->renameName),PROOF_BYTES,stopped,cancel,&why);
 if(!out->plan){char message[128];snprintf(message,sizeof(message),"Cannot construct complete versioned rename draft (%d).",(int)why);fail(out,message);goto done;}okay=1;
done:
 for(size_t i=0;i<sources.Count;++i)free(((plan_source*)sources.Data)[i].key);
 xrtArrayUnit(&sources);xrtFree(inputs);xrtFree(edits);return okay ? 1 : stopped(cancel) ? 0 : *out->error ? 0 : fail(out,"Rename source indexing/allocation budget failed.");
}
