static xide_language_input_guard* importGuard(const xvalue* wire,const xls_context* expected,const xide_language_proof_overlay* overlays,size_t count,struct xide_language_proof_memo* memo,int (*stopped)(void*),void* user,xide_model_error* error)
{
 xide_model_error fallback={0};if(!error)error=&fallback;*error=(xide_model_error){0};int64 schema=0;
 const xls_context_spec* spec=expected?xlsContextSpec(expected):NULL;
 if(!spec || spec->kind==XLS_CONTEXT_C || strcmp(spec->analysisProfile,XIDE_ANALYSIS_PROFILE) || !xrtValueIs(wire,XVALUE_OBJECT) || xrtValueCount(wire)!=4 || !xrtValueGetInt(get(wire,"schema"),&schema) || schema!=1 || count>64 || (count&&!overlays) || (stopped&&stopped(user))){fail(error,"Invalid native input proof import scope.");return NULL;}
 guard_export_charge size={0,XIDE_LSP_BODY_BYTES-1024u*1024u,stopped,user};xjsonwriteconfig json;xrtJsonWriteConfigInit(&json);json.MaxOutputBytes=size.limit;
 if(!xrtJsonWrite(wire,&json,guardExportCharge,&size)){fail(error,"Input proof encoding budget or cancellation rejected import.");return NULL;}
 xls_context* context=xideAnalysisContextRead(get(wire,"context"),error->message,sizeof(error->message));
 if(!context || !xlsContextEqual(context,expected)){xlsContextRelease(context);fail(error,"Input proof does not match the independently expected provider context.");return NULL;}
 xide_language_input_guard* guard=xrtCalloc(1,sizeof(*guard));if(!guard){xlsContextRelease(context);fail(error,"Cannot allocate imported input proof.");return NULL;}
 xrtAtomic32Init(&guard->references,1);xrtAtomic32Init(&guard->invalid,0);guard->context=context;guard->nativeBackend=1;guard->compiler=xrtStrDup("");guard->directory=xrtStrDup(spec->directory);guard->packagePath=spec->packagePath?xrtStrDup(spec->packagePath):NULL;
 guard->files=xrtValueDeepClone(get(wire,"files"));guard->resolution=xrtValueDeepClone(get(wire,"resolution"));
 int okay=guard->compiler&&guard->directory&&(!spec->packagePath||guard->packagePath)&&guard->files&&guard->resolution;
 if(okay){xide_language_check view={0};view.compiler=guard->compiler;view.directory=guard->directory;view.packagePath=guard->packagePath;view.context=guard->context;view.nativeSemantic=1;view.guardFiles=guard->files;view.guardResolution=guard->resolution;view.externalCancel=stopped;view.externalUser=user;view.proofMemo=memo;okay=xideLanguageSnapshotImport(&view,overlays,count,error);}
 if(!okay){xideLanguageInputGuardRelease(guard);if(!error->code)fail(error,"Imported input proof is malformed, changed or cancelled.");return NULL;}return guard;
}
xide_language_input_guard* xideLanguageInputGuardImport(const xvalue* wire,const xls_context* expected,const xide_language_proof_overlay* overlays,size_t count,int (*stopped)(void*),void* user,xide_model_error* error)
{return importGuard(wire,expected,overlays,count,NULL,stopped,user,error);}
int xideLanguageInputGuardsImport(const xvalue* array,const xls_context* const* expected,size_t count,const xide_language_proof_overlay* overlays,size_t overlayCount,xide_language_input_guard** out,int (*stopped)(void*),void* user,xide_model_error* error)
{
 if(!out || !expected || !count || count>65536 || !xrtValueIs(array,XVALUE_ARRAY) || xrtValueCount(array)!=count)return fail(error,"Invalid aggregate input proof array.");
 memset(out,0,count*sizeof(*out));struct xide_language_proof_memo memo={0};int okay=proofMemoInit(&memo);
 guard_export_charge size={0,XIDE_LSP_BODY_BYTES-1024u*1024u,stopped,user};xjsonwriteconfig json;xrtJsonWriteConfigInit(&json);json.MaxOutputBytes=size.limit;
 okay=okay && xrtJsonWrite(array,&json,guardExportCharge,&size);
 for(size_t i=0;okay && i<count;++i){out[i]=importGuard(xrtValueArrayGet(array,i),expected[i],overlays,overlayCount,&memo,stopped,user,error);okay=out[i]!=NULL;}
 proofMemoUnit(&memo);
 if(!okay || (stopped && stopped(user))){for(size_t i=0;i<count;++i){xideLanguageInputGuardRelease(out[i]);out[i]=NULL;}return fail(error,"Aggregate input proof import failed or was cancelled.");}return 1;
}
