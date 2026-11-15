/* Read-only global references share the existing cursor/member actors. */
static int referenceCursorPosition(const reference_input* in,const reference_record* record,reference_output* out)
{
 for(size_t i=0;i<xideSemanticReferenceShardSources(record->shard);++i){xide_semantic_source source;
  if(!xideSemanticReferenceShardSource(record->shard,i,&source))break;
  if(!same(source.path,in->path))continue;
  int character=in->character,offset=0;
  if(!in->line)character-=source.utf16Prefix;
  if(character>=0&&xideLspPositionToOffset(source.text,in->line,character,&offset))return 1;
  break;
 }
 out->code=-32602;return fail(out,"Reference cursor is not a valid UTF16 position in the checked source.");
}
static int referenceScopeCurrent(reference_input* in,reference_output* out,xcancel* cancel)
{
 if(stopped(cancel))return 0;
 ++out->stats.scopeValidations;
 if(!xideRenameScopeValidate(in->renameScope,stopped,cancel,out->error,sizeof(out->error))){out->code=-32801;if(!*out->error)fail(out,"Reference scope metadata changed.");return 0;}return 1;
}
static int referenceCursorProofs(reference_output* out,const reference_output* seed,const reference_record* record,xcancel* cancel)
{
 xmap known={0};size_t incoming=seed->guards.Count;int okay=xrtMapInit(&known,1);
 for(size_t i=0;okay&&i<out->guards.Count;++i){void* guard=((xide_language_input_guard**)out->guards.Data)[i];bool added=false;okay=xrtMapGetOrAdd(&known,(xbytesview){(const unsigned char*)&guard,sizeof(guard)},&added)!=NULL;}
 for(size_t i=0;okay&&i<=incoming;++i){xide_language_input_guard* guard=i<incoming?((xide_language_input_guard**)seed->guards.Data)[i]:record->guard;bool added=false;
  if(stopped(cancel)){okay=0;break;}okay=xrtMapGetOrAdd(&known,(xbytesview){(const unsigned char*)&guard,sizeof(guard)},&added)!=NULL;
  if(okay&&added)okay=prepareGuard(out,guard);
 }
 xrtMapUnit(&known);if(!okay)return *out->error?0:fail(out,"Cannot retain unique reference cursor/source guards.");
 if(!out->prepareDependencies.ItemSize&&!xrtArrayInit(&out->prepareDependencies,sizeof(xls_dependency_capture*)))return fail(out,"Cannot retain reference routing evidence.");
 if(out!=seed)for(size_t i=0;i<seed->prepareDependencies.Count;++i)if(!prepareDependency(out,((xls_dependency_capture**)seed->prepareDependencies.Data)[i],cancel))return 0;
 return !stopped(cancel);
}
static int referenceFrame(reference_input* in,reference_output* out,xcancel* cancel)
{
 xvalue* reply=xrtValueObject();int okay=out->locations&&reply&&put(reply,"jsonrpc",xrtValueString(sv("2.0")))&&put(reply,"id",xrtValueRetain(in->requestId))&&put(reply,"result",xrtValueRetain(out->locations));xide_model_error error={0};
 if(okay&&!stopped(cancel))out->frame=xideLspFrame(reply,&out->frameBytes,&error);
 xrtValueRelease(reply);if(!out->frame){out->code=-32603;return fail(out,"Cannot encode bounded reference response.");}out->stats.encodedFrameBytes=out->frameBytes;
 for(size_t i=0;i<out->prepareDependencies.Count;++i){if(stopped(cancel))return 0;if(!xlsDependencyCaptureValidate(((xls_dependency_capture**)out->prepareDependencies.Data)[i])){out->code=-32801;return fail(out,"Reference compilation owner/routing changed after response framing.");}}
 for(size_t i=0;i<out->guards.Count;++i){if(stopped(cancel))return 0;if(!xideLanguageInputGuardValidate(((xide_language_input_guard**)out->guards.Data)[i],stopped,cancel,&error)){out->code=-32801;return fail(out,"Reference source/resolution inputs changed after response framing.");}}
 return referenceScopeCurrent(in,out,cancel);
}
static xide_rename_scope* referenceScope(xide_analysis_catalog* catalog,xls_context* selected,const xls_analysis_document* docs,size_t count,const char* path,const char* trigger)
{
 const xls_context_spec* spec=xlsContextSpec(selected);if(!spec||strcmp(spec->analysisProfile,XIDE_ANALYSIS_PROFILE)||!path||!xrtPathIsAbs(path)||!xidePathHasExtension(path,".xl")||count>64||(count&&!docs))return NULL;
 xide_analysis_catalog* empty=NULL;if(!catalog){if(spec->kind!=XLS_CONTEXT_XLANG||!xidePathLexicalEqual(spec->entry,path))return NULL;catalog=empty=xideAnalysisCatalogEmpty(spec->packagePath,NULL,0);if(!catalog)return NULL;}
 xls_context* files[64]={0};size_t filesCount=0,labels=0;int okay=xideAnalysisCatalogComplete(catalog),found=!trigger;const xls_context* owner=NULL;
 int binding=xideAnalysisCatalogFind(catalog,path,&owner);xls_context* file=NULL;
 if(binding==XIDE_ANALYSIS_UNOWNED){file=empty?xlsContextRetain(selected):xideAnalysisCatalogFile(catalog,path);owner=file;}
 okay=okay&&(binding==XIDE_ANALYSIS_PROJECT||binding==XIDE_ANALYSIS_UNOWNED)&&owner&&xlsContextEqual(owner,selected);
 if(okay&&binding==XIDE_ANALYSIS_UNOWNED)files[filesCount++]=xlsContextRetain(file);
 xlsContextRelease(file);
 for(size_t i=0;okay&&i<count;++i){const xls_analysis_document* d=docs+i;size_t a=d->key?strlen(d->key):0,b=d->path?strlen(d->path):0;
  okay=a&&b&&a<=262144&&b<=32767&&d->text&&d->identity&&d->revision<=UINT32_MAX&&xrtPathIsAbs(d->path)&&xidePathHasExtension(d->path,".xl")&&charge(&labels,a+b+2,2u*1024u*1024u);
  if(!okay)break;
  if(trigger&&!strcmp(d->key,trigger)){if(!xidePathLexicalEqual(d->path,path)||found){okay=0;break;}found=1;}
  if(xidePathLexicalEqual(d->path,path))continue;
  uint64_t owners=0;wchar_t* key=xidePathNativeWide(d->path);okay=key&&xideAnalysisCatalogOwnersNative(catalog,key,&owners);free(key);if(!okay||owners)continue;
  if(filesCount==64){okay=0;break;}file=xideAnalysisCatalogFile(catalog,d->path);if(!file){okay=0;break;}files[filesCount++]=file;
 }
 xide_rename_scope* result=okay&&found?xideRenameScopeCreate(catalog,files,filesCount,0,NULL,NULL,NULL):NULL;
 for(size_t i=0;i<filesCount;++i)xlsContextRelease(files[i]);
 xideAnalysisCatalogDestroy(empty);return result;
}
