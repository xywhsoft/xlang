typedef struct imported_overlay {wchar_t* key;const xls_text* text;} imported_overlay;
static int importedOverlayOrder(const void* a,const void* b)
{return xidePathNativeKeyCompare(((const imported_overlay*)a)->key,((const imported_overlay*)b)->key);}
static wchar_t* importedProofKey(const char* path)
{
 if(!path || strlen(path)>262144 || !absolute(path))return NULL;
 char* resolved=xidePathResolveLexical(NULL,path);wchar_t* key=resolved&&xidePathLexicalEqual(resolved,path)?xidePathLexicalWide(path):NULL;free(resolved);return key;
}
int xideLanguageSnapshotImport(xide_language_check* q,const xide_language_proof_overlay* input,size_t count,xide_model_error* error)
{
 const xls_context_spec* spec=q->context?xlsContextSpec(q->context):NULL;size_t files=xrtValueCount(q->guardFiles),bytes=0;int okay=0,entry=0;imported_overlay overlays[64]={0};wchar_t **keys=NULL,*entryKey=NULL,*directoryKey=NULL;
 if(!spec || count>64 || (count&&!input) || !xrtValueIs(q->guardFiles,XVALUE_ARRAY) || !files || files>512 || cancelled(q))goto done;
 entryKey=importedProofKey(spec->entry);directoryKey=importedProofKey(spec->directory);if(!entryKey || !directoryKey)goto done;
 for(size_t i=0;i<count;++i){if(cancelled(q) || !input[i].text || xlsTextSize(input[i].text)>FILE_LIMIT || xlsTextSize(input[i].text)>32u*1024u*1024u-bytes)goto done;bytes+=xlsTextSize(input[i].text);overlays[i]=(imported_overlay){importedProofKey(input[i].path),input[i].text};if(!overlays[i].key)goto done;}
 qsort(overlays,count,sizeof(*overlays),importedOverlayOrder);for(size_t i=1;i<count;++i)if(!importedOverlayOrder(overlays+i-1,overlays+i))goto done;
 keys=xrtCalloc(files,sizeof(*keys));if(!keys)goto done;bytes=0;
 /* Reject duplicate/invalid names before any source read. Reports retain their
  * wire order; only the prepared lexical keys are sorted for duplicate checks. */
 for(size_t i=0;i<files;++i){const xvalue* item=xrtValueArrayGet(q->guardFiles,i);int64 size=0,raw=0,kind=0;const char* origin=string(get(item,"origin"));
  if(cancelled(q) || !origin || (strcmp(origin,"overlay")&&strcmp(origin,"disk")) || !integer(get(item,"kind"),&kind,1,3) || !integer(get(item,"textSize"),&size,0,FILE_LIMIT) || !integer(get(item,"rawSize"),&raw,size,FILE_LIMIT) || (raw-size!=0 && raw-size!=3) || (size_t)size>GRAPH_LIMIT-bytes || !xideReferenceDocumentHex(get(item,"rawSha256")) || !xideReferenceDocumentHex(get(item,"textSha256")))goto done;
  bytes+=(size_t)size;keys[i]=importedProofKey(string(get(item,"path")));if(!keys[i])goto done;
 }
 qsort(keys,files,sizeof(*keys),resolutionKeyOrder);for(size_t i=1;i<files;++i)if(!xidePathNativeKeyCompare(keys[i-1],keys[i]))goto done;
 for(size_t i=0;i<files;++i){
  if(cancelled(q))goto done;
  const xvalue* item=xrtValueArrayGet(q->guardFiles,i);const char* path=string(get(item,"path"));wchar_t* key=importedProofKey(path);if(!key)goto done;
  size_t lo=0,hi=count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(xidePathNativeKeyCompare(overlays[mid].key,key)<0)lo=mid+1;else hi=mid;}
  int found=lo<count&&!xidePathNativeKeyCompare(overlays[lo].key,key);int64 kind=0,size=0,raw=0;xrtValueGetInt(get(item,"kind"),&kind);xrtValueGetInt(get(item,"textSize"),&size);xrtValueGetInt(get(item,"rawSize"),&raw);
  if(!xidePathNativeKeyCompare(key,entryKey)&&kind==1)entry=1;
  free(key);
  if(formatted(get(item,"origin"),"overlay")){
   if(!found || !(kind&1) || raw!=size)goto done;
   const char* text=xlsTextData(overlays[lo].text);size_t length=xlsTextSize(overlays[lo].text),bom=length>=3&&!memcmp(text,"\xef\xbb\xbf",3)?3:0;
   if((size_t)size!=length-bom || !overlayHash(text+bom,length-bom,get(item,"textSha256"),get(item,"rawSha256")))goto done;
  }else if(found || !diskProofValid(q,path,item,error))goto done;
 }
 okay=entry && resolution(q,q->guardResolution,error) && !cancelled(q);
done:
 for(size_t i=0;i<count&&i<64;++i)free(overlays[i].key);
 if(keys)for(size_t i=0;i<files;++i)free(keys[i]);
 xrtFree(keys);free(entryKey);free(directoryKey);
 return okay?1:fail(error,"Imported source/resolution proof is malformed, changed, shadowed or cancelled.");
}
