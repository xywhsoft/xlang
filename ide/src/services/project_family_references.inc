static int workspaceFamilyDeclarationOrder(const result_row* row,const wchar_t* key,size_t start,size_t end)
{int n=xidePathNativeKeyCompare(row->key,key);if(!n)n=(row->start.offset>(int)start)-(row->start.offset<(int)start);return n ? n : (row->end.offset>(int)end)-(row->end.offset<(int)end);}
static int workspaceFamilyDeclarations(reference_output* out,xcancel* cancel)
{
 xide_reference_family_info info;if(!xideSemanticReferenceFamilyInfo(out->family,&info))return 0;
 wchar_t* key=NULL;const char* previous=NULL;int okay=1;
 for(size_t i=0;i<info.members;++i){
  if(stopped(cancel)){okay=0;break;}
  xide_semantic_source source;size_t start,end;if(!xideSemanticReferenceFamilyMember(out->family,i,&source,&start,&end)){okay=0;break;}
  if(source.path!=previous){free(key);key=xidePathNativeWide(source.path);previous=source.path;if(!key){okay=0;break;}}
  size_t low=0,high=out->rows.Count;while(low<high){size_t mid=low+(high-low)/2;const result_row* row=(result_row*)out->rows.Data+mid;if(workspaceFamilyDeclarationOrder(row,key,start,end)<0)low=mid+1;else high=mid;}
  if(low>=out->rows.Count || workspaceFamilyDeclarationOrder((result_row*)out->rows.Data+low,key,start,end)){okay=0;break;}
 }
 free(key);return okay ? 1 : stopped(cancel) ? 0 : fail(out,"A workspace family declaration has no exact captured identifier occurrence.");
}
static int workspaceFamilyCompact(reference_output* out,size_t textBytes,xcancel* cancel)
{
 if(!xrtArraySort(&out->rows,compareRow))return fail(out,"Cannot sort workspace family occurrences.");
 result_row* rows=(result_row*)out->rows.Data;size_t used=0;result_row* file=NULL;const xvalue* checked=NULL;
 workspace_charge proof={0,cancel};xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=RESULT_BYTES;
 for(size_t i=0;i<out->rows.Count;++i){
  if(stopped(cancel))return 0;
  result_row* row=rows+i;
  if(!file || xidePathNativeKeyCompare(file->key,row->key)){
   file=row;checked=row->proof;if(!row->proof || !xrtJsonWrite(row->proof,&config,workspaceProofCharge,&proof))return fail(out,"Workspace family source proof budget exceeded.");
  }else if(row->proof!=file->proof && row->proof!=checked){if(!workspaceProofSame(file->proof,row->proof))return fail(out,"Workspace family providers disagree on a reference document proof.");checked=row->proof;}
  if(used && !compareRow(rows+used-1,row)){xrtFree(row->path);free(row->key);xrtValueRelease((xvalue*)row->proof);*row=(result_row){0};++out->stats.workspaceDuplicates;continue;}
  const xvalue* canonical=file->proof;
  if(row->proof!=canonical){xrtValueRetain((xvalue*)canonical);xrtValueRelease((xvalue*)row->proof);row->proof=canonical;}
  if(used!=i){rows[used]=*row;*row=(result_row){0};}
  if(file==row)file=rows+used;
  ++used;
 }
 out->rows.Count=used;
 if(out->rows.Capacity>RESULT_BYTES/sizeof(result_row) || textBytes>RESULT_BYTES-out->rows.Capacity*sizeof(result_row) || proof.bytes>RESULT_BYTES-out->rows.Capacity*sizeof(result_row)-textBytes)return fail(out,"Workspace family row/proof budget exceeded.");
 return workspaceFamilyDeclarations(out,cancel);
}
static int workspaceFamilyReferences(reference_input* in,reference_output* out,xcancel* cancel)
{
 if(out->graphs.Count!=out->guards.Count)return fail(out,"Workspace declaration providers have incomplete input guards.");
 size_t textBytes=0,proofBytes=0;
 for(size_t i=0;i<out->graphs.Count;++i){
  if(stopped(cancel))return 0;
  xide_reference_graph* graph=((xide_reference_graph**)out->graphs.Data)[i];reference_record provider={.shard=xideSemanticReferenceGraphRetainShard(graph),.guard=((xide_language_input_guard**)out->guards.Data)[i]};
  enum xide_reference_family_error why;xide_family_references* refs=xideSemanticReferenceShardFamily(provider.shard,out->family,1,RESULT_BYTES,stopped,cancel,&why);xide_family_reference_info info;
  xmap known={0};xarray proofs={0};int okay=refs && xideSemanticFamilyReferencesInfo(refs,&info) && xrtMapInit(&known,sizeof(size_t)) && xrtArrayInit(&proofs,sizeof(xvalue*));
  for(size_t j=0;okay && j<info.locations;++j){
   if(stopped(cancel)){okay=0;break;}
   xide_semantic_reference at;okay=xideSemanticFamilyReferenceAt(refs,j,&at) && appendLocation(in,&out->rows,&provider,&at,&textBytes,&known,&proofs,&proofBytes,out);
   if(okay){result_row* row=(result_row*)out->rows.Data+out->rows.Count-1;row->proof=xrtValueRetain((xvalue*)row->proof);row->ownedProof=1;}
  }
  for(size_t j=0;j<proofs.Count;++j)xrtValueRelease(((xvalue**)proofs.Data)[j]);
  xrtArrayUnit(&proofs);xrtMapUnit(&known);xideSemanticFamilyReferencesRelease(refs);xideSemanticReferenceShardRelease(provider.shard);
  if(!okay)return stopped(cancel) ? 0 : *out->error ? 0 : fail(out,"Cannot capture complete native family identifier occurrences.");
 }
 if(!workspaceFamilyCompact(out,textBytes,cancel) || !sortedLocations(&out->rows,out))return *out->error ? 0 : fail(out,"Cannot construct bounded workspace family locations.");
 return 1;
}
