/* One selected source only. Reuse the exact reference proof reader/schema;
 * never retain source bodies in the declaration index or trust an old row. */
static int navigationPut(xvalue* o,const char* key,xvalue* value)
{return value && xrtValueObjectSetNew(o,view(key),value);}
static bool navigationCount(xbytesview part,void* user)
{(void)part;return !stopped(user);}
static xvalue* navigationProof(symbol_input* in,symbol_output* out,xcancel* cancel)
{
 xide_workspace_symbol_view selected={0};const xvalue* row=NULL;symbol_record* record=NULL;const xvalue* file=NULL;xvalue* document=NULL,*proof=NULL;char *text=NULL,*original=NULL;xide_model_error error={0};
 for(size_t i=0;i<xrtValueCount(out->wire);++i){if(stopped(cancel))goto done;if(xrtValueEqual(xrtValueArrayGet(out->wire,i),in->selection)){if(!xideWorkspaceSymbolResultAt(out->result,i,&selected))goto done;row=xrtValueArrayGet(out->wire,i);break;}}
 if(!row){fail(out,"Workspace symbol selection changed;search again before navigation.");goto done;}
 for(size_t i=0;i<in->store->records.Count;++i){if(stopped(cancel))goto done;symbol_record* candidate=((symbol_record**)in->store->records.Data)[i];if(!strcmp(candidate->owner,selected.owner) && xlsContextEqual(candidate->scope,selected.scope)){record=candidate;break;}}
 if(!record)goto done;
 const xvalue* files=xideLanguageInputGuardFiles(record->guard);
 for(size_t i=0;i<xrtValueCount(files);++i){if(stopped(cancel))goto done;const xvalue* candidate=xrtValueArrayGet(files,i);const char* path=string(field(candidate,"path"));if(path && xidePathLexicalEqual(path,selected.declaration.path)){file=candidate;break;}}
 const char* origin=string(field(file,"origin"));if(!file || !origin)goto done;
 xide_semantic_source source={.path=selected.declaration.path,.kind=1};int version=0;
 if(!strcmp(origin,"overlay")){
  size_t at=0;for(;at<in->count;++at){if(stopped(cancel))goto done;if(xidePathLexicalEqual(in->docs[at].path,source.path))break;}
  if(at==in->count || in->docs[at].revision>UINT32_MAX)goto done;
  source.utf16Prefix=bom(in->docs[at].text);source.text=xlsTextData(in->docs[at].text)+(source.utf16Prefix?3:0);version=(int)((int64_t)in->docs[at].revision+INT_MIN);
 }else if(!strcmp(origin,"disk")){
  text=xideLanguageReadDiskProof(source.path,file,stopped,cancel,&original,&error);if(!text || !original)goto done;source.text=original;
 }else goto done;
 document=xideLanguageReferenceDocument(&source,version,!strcmp(origin,"disk")?file:NULL);if(!document || stopped(cancel))goto done;
 proof=xrtValueObject();if(!proof || !navigationPut(proof,"symbol",xrtValueRetain((xvalue*)row)))goto invalid;
 {int okay=navigationPut(proof,"document",document);document=NULL;if(!okay)goto invalid;}
 {xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);config.MaxOutputBytes=XIDE_LSP_BODY_BYTES-1024u*1024u;if(!xrtJsonWrite(proof,&config,navigationCount,cancel))goto invalid;}
 goto done;
invalid:xrtValueRelease(proof);proof=NULL;
done:free(text);free(original);xrtValueRelease(document);if(!proof && !*out->error)fail(out,"Cannot verify selected symbol source,version or navigation proof budget.");return proof;
}
