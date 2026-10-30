/* Closed filesystem asynchronous adapters; native jobs and resource lifetimes
 * belong to the resident task service, not generated callback trampolines. */
static void xl6IrCgenEmitFsAsyncRuntime(XL6_Output* out,const XL6_IrCgenNeeds* needs)
{
    int async=0;
    for(XL6_IrRuntimeBridgeKind k=XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_OPEN;k<=XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_ENSURE;++k)
        async |= xl6IrCgenFsSelected(needs,k);
    if(async) xl6OutputPuts(
        "/* XRT native results never escape: the mapped Future owns a canonical language\n"
        " * payload and a resident destructor. Cancellation forwards to the exclusive\n"
        " * source; already performed filesystem mutations are not rolled back. */\n"
        "static inline void xlir_fs_async_fail(xpromise* output)\n"
        "{ const xerror* error=xrtGetError(); if(error==NULL || !xrtPromiseReject(output,error)) (void)xrtPromiseClose(output); }\n"
        "static inline void xlir_fs_async_publish(xpromise* output, void* owned, XL6_FuturePayloadKind kind)\n"
        "{ if(owned==NULL){xlir_fs_async_fail(output);return;} if(!xl6PayloadResolveOwned(output,owned,kind)){xl6PayloadDropGet(kind)(owned,NULL);(void)xrtPromiseClose(output);} }\n"
        "static inline void xlir_fs_async_bool(const xfutureresult* input,xpromise* output,void* arg)\n"
        "{ bool* box=(bool*)xrtMalloc(sizeof(*box)); (void)input;(void)arg; if(box)*box=true;xlir_fs_async_publish(output,box,XL6_FUTURE_PAYLOAD_VALUE); }\n"
        "static inline void xlir_fs_async_empty(const xfutureresult* input,xpromise* output,void* arg)\n"
        "{ const xdirquery* value=(const xdirquery*)input->Value;bool* box; (void)arg; if(!value){xrtSetErrorKind(XERR_STATE);xlir_fs_async_fail(output);return;} box=xrtMalloc(sizeof(*box));if(box)*box=value->Empty;xlir_fs_async_publish(output,box,XL6_FUTURE_PAYLOAD_VALUE); }\n"
        "static inline void xlir_fs_async_number(const xfutureresult* input,xpromise* output,void* arg)\n"
        "{ uint64_t* box; if(!input->Value){xrtSetErrorKind(XERR_STATE);xlir_fs_async_fail(output);return;}box=xrtMalloc(sizeof(*box)); if(box)*box=(uintptr_t)arg==1 ? ((const xfilechange*)input->Value)->Size : ((const xfilesize*)input->Value)->Size;xlir_fs_async_publish(output,box,XL6_FUTURE_PAYLOAD_VALUE); }\n"
        "static inline void xlir_fs_async_bytes(const xfutureresult* input,xpromise* output,void* arg)\n"
        "{ const xfiledata* value=(const xfiledata*)input->Value; (void)arg;if(!value){xrtSetErrorKind(XERR_STATE);xlir_fs_async_fail(output);return;} xlir_native_future_publish_xvalue(output,xrtValueBytes((xbytesview){value->Data,value->Size})); }\n"
        "static inline xfuture* xlir_fs_async_map(xfuture* source,xfuturecontinueproc mapper,uintptr_t argument)\n"
        "{ if(!source)return NULL;xfuture* result=xlir_native_future_then(source,mapper,argument);if(!result)(void)xrtFutureCancel(source);xrtFutureDestroy(source);return result; }\n", out);
    if (xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TEXT_READ)) xl6OutputPuts(
        "static inline void xlir_fs_async_text(const xfutureresult* input,xpromise* output,void* argument)\n"
        "{ const xfiledata* value=(const xfiledata*)input->Value;uintptr_t flags=(uintptr_t)argument;size_t bom=0,count=0;unsigned char* data;xstrview* box; xencoding source=(xencoding)(flags&7),detected; xutfpolicy policy=(xutfpolicy)((flags>>3)&1);\n"
        "  if(!value){xrtSetErrorKind(XERR_STATE);xlir_fs_async_fail(output);return;}\n"
        "  xbytesview raw={value->Data,value->Size};detected=xrtEncodingBom(raw,&bom);\n"
        "  if(source!=XENCODING_UNKNOWN && detected!=XENCODING_UNKNOWN && source!=detected){xrtSetErrorKind(XERR_VALUE);xlir_fs_async_fail(output);return;}\n"
        "  if(source==XENCODING_UNKNOWN)source=detected!=XENCODING_UNKNOWN ? detected : raw.Size ? xrtEncodingGuess(raw).Encoding : XENCODING_UTF8;\n"
        "  if(source==XENCODING_UNKNOWN){xrtSetErrorKind(XERR_VALUE);xlir_fs_async_fail(output);return;}\n"
        "  data=xrtTranscode((xbytesview){raw.Data ? raw.Data+bom : NULL,raw.Size-bom},source,XENCODING_UTF8,policy,false,&count);\n"
        "  if(!data){xlir_fs_async_fail(output);return;}box=xrtMalloc(sizeof(*box));if(!box){xrtFree(data);xlir_fs_async_fail(output);return;}\n"
        "  *box=(xstrview){(const char*)data,count};xlir_fs_async_publish(output,box,XL6_FUTURE_PAYLOAD_STRING); }\n",out);
    if (xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_COPY) || xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_REMOVE) || xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_STATS)) xl6OutputPuts(
        "static inline void xlir_fs_async_stats(const xfutureresult* input,xpromise* output,void* argument)\n"
        "{ const xwalkstats* stats=(const xwalkstats*)input->Value;xvalue* result; (void)argument;\n"
        "  if(!stats){xrtSetErrorKind(XERR_STATE);xlir_fs_async_fail(output);return;}result=xrtValueArray();\n"
        "  uint64_t values[]={stats->Items,stats->Files,stats->Directories,stats->Links,stats->Others,stats->Bytes};\n"
        "  for(size_t i=0;result!=NULL && i<6;++i)if(!xrtValueArrayAppendNew(result,xrtValueUInt(values[i]))){xrtValueRelease(result);result=NULL;}\n"
        "  if(result && !xrtValueArrayAppendNew(result,xrtValueBool(stats->Stopped))){xrtValueRelease(result);result=NULL;}\n"
        "  xlir_native_future_publish_xvalue(output,result); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_OPEN)) xl6OutputPuts(
        "void* xl6_xrt_fs_async_open(xlir_string path,int64_t flags,int64_t mode,int64_t share)\n"
        "{ xfileoptions options;const char* name=xlir_string_cstr(path); if(!name || !xlir_fs_options(flags,mode,share,&options))return NULL;return xl6AsyncFileOpenV1(xlir_async_pool,name,&options); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_ADOPT)) xl6OutputPuts(
        "void* xl6_xrt_fs_async_adopt(void* handle)\n"
        "{ xlir_fs_file* file=handle; XL6_AsyncFile* result;\n"
        "  if(!xlir_fs_file_get(handle))return NULL;\n"
        "  if(file->deleteOnClose || xrtAtomic32Load(&file->refs,XMEMORY_ACQUIRE)!=1 || xrtAtomic32Load(&file->lock,XMEMORY_ACQUIRE)!=0){xrtSetErrorKind(XERR_STATE);return NULL;}\n"
        "  result=xl6AsyncFileAdoptV1(xlir_async_pool,file->file);if(result)file->file=NULL;return result; }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_CLOSE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_close(void* handle){return xlir_fs_async_map(xl6AsyncFileCloseV1(handle),xlir_fs_async_bool,0);}\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_FLUSH)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_flush(void* handle){return xlir_fs_async_map(xl6AsyncFileFlushV1(handle),xlir_fs_async_bool,0);}\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_SIZE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_size(void* handle){return xlir_fs_async_map(xl6AsyncFileSizeV1(handle),xlir_fs_async_number,0);}\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_READ)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_read(void* handle,uint64_t offset,int64_t size)\n"
        "{ size_t count;if(!xlir_fs_size(size,&count))return NULL;return xlir_fs_async_map(xl6AsyncFileReadV1(handle,offset,count),xlir_fs_async_bytes,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_WRITE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_write(void* handle,uint64_t offset,const xvalue* data,int64_t size)\n"
        "{ xbytesview view;if(!xrtValueGetBytes(data,&view)){xrtSetErrorKind(XERR_TYPE);return NULL;}if(!xlir_fs_count(size,view.Size,&view.Size))return NULL;return xlir_fs_async_map(xl6AsyncFileWriteV1(handle,offset,view.Data,view.Size),xlir_fs_async_number,1); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_RESIZE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_resize(void* handle,uint64_t size){return xlir_fs_async_map(xl6AsyncFileResizeV1(handle,size),xlir_fs_async_number,1);}\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_FLAGS)) xl6OutputPuts(
        "int64_t xl6_xrt_fs_async_flags(void* handle){return xl6AsyncFileFlagsV1(handle);}\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_CLOSED)) xl6OutputPuts(
        "bool xl6_xrt_fs_async_closed(void* handle){return xl6AsyncFileClosedV1(handle);}\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_WHOLE_READ)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_whole_read(xlir_string path,int64_t limit)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);size_t maximum;if(!name || !xlir_fs_size(limit,&maximum))return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileReadAllLimitAsync(scope.native,name,maximum);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bytes,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_WHOLE_WRITE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_whole_write(xlir_string path,const xvalue* data,int64_t size,int64_t operation)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);xbytesview view;if(!name)return NULL;if(!xrtValueGetBytes(data,&view)){xrtSetErrorKind(XERR_TYPE);return NULL;}if(!xlir_fs_count(size,view.Size,&view.Size))return NULL;if(operation<0 || operation>2){xrtSetErrorKind(XERR_RANGE);return NULL;}\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=operation==0 ? xrtFileWriteAllAsync(scope.native,name,view) : operation==1 ? xrtFileAppendAsync(scope.native,name,view) : xrtFileWriteAtomicAsync(scope.native,name,view);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TEXT_READ)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_text_read(xlir_string path,int64_t encoding,int64_t policy,int64_t limit)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);size_t maximum;if(encoding<0 || encoding>5 || policy<0 || policy>1){xrtSetErrorKind(XERR_RANGE);return NULL;}if(!name || !xlir_fs_size(limit,&maximum))return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileReadAllLimitAsync(scope.native,name,maximum);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_text,(uintptr_t)(encoding | (policy<<3))); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TEXT_WRITE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_text_write(xlir_string path,xlir_string text,int64_t encoding,int64_t policy,bool bom,int64_t size,int64_t operation)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);xstrview view={text.Data,text.Size};size_t count=0;unsigned char* data;\n"
        "  if(encoding<1 || encoding>5 || policy<0 || policy>1 || operation<0 || operation>2){xrtSetErrorKind(XERR_RANGE);return NULL;}\n"
        "  if(!name || !xlir_fs_count(size,view.Size,&view.Size))return NULL;\n"
        "  data=xrtTranscode((xbytesview){(const uint8*)view.Data,view.Size},XENCODING_UTF8,(xencoding)encoding,(xutfpolicy)policy,bom,&count);if(!data)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope)){xrtFree(data);return NULL;}\n"
        "  xbytesview raw={data,count};pendingNative=operation==0 ? xrtFileWriteAllAsync(scope.native,name,raw) : operation==1 ? xrtFileAppendAsync(scope.native,name,raw) : xrtFileWriteAtomicAsync(scope.native,name,raw);\n"
        "  xl6TaskNativeEndV1(&scope);xrtFree(data);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_COPY)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_copy(xlir_string source,xlir_string target,bool replace)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* a=xlir_string_cstr(source);const char* b=xlir_string_cstr(target);if(!a || !b)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileCopyAsync(scope.native,a,b,replace);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_MOVE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_move(xlir_string source,xlir_string target,bool replace)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* a=xlir_string_cstr(source);const char* b=xlir_string_cstr(target);if(!a || !b)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileMoveAsync(scope.native,a,b,replace);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_MOVE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_tree_move(xlir_string source,xlir_string target,bool replace)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* a=xlir_string_cstr(source);const char* b=xlir_string_cstr(target);if(!a || !b)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtDirMoveAsync(scope.native,a,b,replace);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_DELETE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_delete(xlir_string path)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileDeleteAsync(scope.native,name);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_DIR_REMOVE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_dir_remove(xlir_string path)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtDirRemoveAsync(scope.native,name);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_DIR_EMPTY)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_dir_empty(xlir_string path)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtDirEmptyAsync(scope.native,name);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_empty,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_ENSURE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_tree_ensure(xlir_string path)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtDirEnsureEmptyAsync(scope.native,name);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_DIR_CREATE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_dir_create(xlir_string path,int64_t mode,bool all)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;if(mode < -1 || mode>4095){xrtSetErrorKind(XERR_RANGE);return NULL;}\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=mode<0 ? (all ? xrtDirCreateAllAsync(scope.native,name) : xrtDirCreateAsync(scope.native,name)) : (all ? xrtDirCreateAllModeAsync(scope.native,name,(uint32_t)mode) : xrtDirCreateModeAsync(scope.native,name,(uint32_t)mode));xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_bool,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_COPY)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_tree_copy(xlir_string source,xlir_string target,int64_t flags)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* a=xlir_string_cstr(source);const char* b=xlir_string_cstr(target);xtreecopyoptions options;if(flags<0 || flags>127 || (flags&12)==12){xrtSetErrorKind(XERR_RANGE);return NULL;}if(!a || !b)return NULL;xrtTreeCopyOptionsInit(&options);options.Flags=(uint32_t)flags;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileTreeCopyAsync(scope.native,a,b,&options);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_stats,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_REMOVE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_tree_remove(xlir_string path,bool keepRoot)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtFileTreeRemoveAsync(scope.native,name,keepRoot);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_stats,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_STATS)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_tree_stats(xlir_string path,bool recursive)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtDirStatsAsync(scope.native,name,recursive);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_stats,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_SIZE)) xl6OutputPuts(
        "xfuture* xl6_xrt_fs_async_tree_size(xlir_string path,bool recursive)\n"
        "{ XL6_TaskNativeScopeV1 scope={0};xfuture* pendingNative;const char* name=xlir_string_cstr(path);if(!name)return NULL;\n"
        "  if(!xl6TaskNativeBeginV1(xlir_async_pool,&scope))return NULL;\n"
        "  pendingNative=xrtDirSizeAsync(scope.native,name,recursive);xl6TaskNativeEndV1(&scope);return xlir_fs_async_map(pendingNative,xlir_fs_async_number,0); }\n",out);
    if(xl6IrCgenFsSelected(needs,XL6_IR_RUNTIME_BRIDGE_FS_STATS_FROM_VALUE)) xl6OutputPuts(
        "void* xl6_xrt_fs_stats_from_value(const xvalue* value)\n"
        "{ uint64 values[6];bool stopped;xlir_fs_stats* result;\n"
        "  if(xrtValueCount(value)!=7){xrtSetErrorKind(XERR_TYPE);return NULL;}\n"
        "  for(size_t i=0;i<6;++i)if(!xrtValueGetUInt(xrtValueArrayGet(value,i),&values[i])){xrtSetErrorKind(XERR_TYPE);return NULL;}\n"
        "  if(!xrtValueGetBool(xrtValueArrayGet(value,6),&stopped)){xrtSetErrorKind(XERR_TYPE);return NULL;}\n"
        "  result=xlir_fs_stats_alloc();if(result)result->value=(xwalkstats){values[0],values[1],values[2],values[3],values[4],values[5],stopped};return result; }\n",out);
}
