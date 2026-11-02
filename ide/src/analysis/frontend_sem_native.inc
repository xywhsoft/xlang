#define XLS_NATIVE_RETAIN_BYTES (32u*1024u*1024u)
static _Thread_local xls_sem_native_control* xlsActiveNativeControl;
size_t xlsFrontendSemNativeBudget(void)
{return xlsActiveNativeControl ? xlsActiveNativeControl->bytes : XLS_NATIVE_RETAIN_BYTES;}
void xlsFrontendSemNativeBegin(xls_sem_native_control* control,size_t bytes)
{if(control){size_t limit=xlsFrontendSemNativeBudget();*control=(xls_sem_native_control){xlsActiveNativeControl,bytes<limit ? bytes : limit};xlsActiveNativeControl=control;}}
void xlsFrontendSemNativeEnd(xls_sem_native_control* control)
{if(control && xlsActiveNativeControl==control)xlsActiveNativeControl=control->previous;}
static int xlsSemNativeRetain(xls_sem_memo* memo,xls_sem_entry* entry,xls_typed_function* function)
{
    if(entry->native)return entry->native==function;
    size_t known=xlsTypedFunctionKnownBytes(function),limit=xlsFrontendSemNativeBudget();
    const void* owner=xlsTypedFunctionMetadataOwner(function);
    if(!known || !owner || memo->nativeWork.bytes>limit)return 0;
    if(!memo->nativeOwnersReady){
        if(!xrtMapInit(&memo->nativeOwners,sizeof(unsigned char)))return 0;
        memo->nativeOwnersReady=1;
    }
    xbytesview key={(const unsigned char*)&owner,sizeof(owner)};
    if(!xrtMapHas(&memo->nativeOwners,key)){
        unsigned char yes=1;
        if(known>limit-memo->nativeWork.bytes || 128>limit-memo->nativeWork.bytes-known ||
           !xrtMapSet(&memo->nativeOwners,key,&yes))return 0;
        memo->nativeWork.bytes+=known+128;++memo->nativeWork.owners;
    }
    entry->native=xlsTypedFunctionRetain(function);++memo->nativeWork.retained;return 1;
}
void xlsFrontendSemMemoDetachNative(xls_sem_memo* memo,XL6_AstNode* root)
{
    if(!memo || !root || root->childCount!=memo->entries.Count)return;
    for(size_t i=0;i<memo->entries.Count;++i){xls_sem_entry* entry=xrtArrayGet(&memo->entries,i);
        if(entry->nativePlaced && root->children[i]==xlsTypedFunctionRoot(entry->native))root->children[i]=NULL;
    }
}
xls_typed_module* xlsFrontendSemMemoTakeModule(xls_sem_memo* memo,XL6_AstNode* root)
{
    if(!memo || !memo->nativeWork.reused)return xlsTypedModuleTake(root);
    if(!root || root->childCount!=memo->entries.Count)return NULL;
    xls_typed_function** owners=xrtCalloc(root->childCount,sizeof(*owners));if(!owners)return NULL;
    for(size_t i=0;i<root->childCount;++i){const xls_sem_entry* entry=xrtArrayConstGet(&memo->entries,i);if(entry->nativePlaced)owners[i]=entry->native;}
    xls_typed_module* result=xlsTypedModuleTakeBorrowed(root,owners,root->childCount);xrtFree(owners);return result;
}
static int xlsSemNativeCancelled(void* unused){(void)unused;return xlsFrontendCancelPoll();}
/* Negative write-footprint certificate for the exact unchanged C-owned
 * validator. It writes extern-parameter and C-marker return flags. Such trees
 * stay eager;the validator still visits EVERY admitted native subtree. */
static int xlsSemNativeCOwnedReadOnly(const XL6_AstNode* node,size_t depth)
{
    if(!node)return 1;
    if(depth>512 || (node->childCount && !node->children) || xlsFrontendCancelPoll() ||
       node->kind==XL6_AST_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_FUNC ||
       node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL || xl6COwnedBaseName(node->text))return 0;
    for(size_t i=0;i<node->childCount;++i)if(!xlsSemNativeCOwnedReadOnly(node->children[i],depth+1))return 0;
    return 1;
}
void xlsFrontendSemMemoBindFunctions(xls_sem_memo* memo,xls_typed_module* module)
{
    if(!memo || !memo->raw || !memo->finalContext || memo->finalizedRoot!=xlsTypedModuleRoot(module))return;
    int candidate=0;
    for(size_t i=0;i<memo->entries.Count && !candidate;++i){const xls_sem_entry* entry=xrtArrayConstGet(&memo->entries,i);
        candidate=entry->unit && entry->unit->proof && entry->finalInput && entry->finalPlanValid;
    }
    if(candidate && xlsTypedModuleMeasureFunctions(module,xlsFrontendSemNativeBudget(),xlsSemNativeCancelled,NULL)){
        for(size_t i=0;i<memo->entries.Count;++i){xls_sem_entry* entry=xrtArrayGet(&memo->entries,i);
            if(entry->native || !entry->unit || !entry->unit->proof || !entry->finalInput || !entry->finalPlanValid ||
               i>=memo->raw->entries.Count || !*(xls_raw_fact* const*)xrtArrayConstGet(&memo->raw->entries,i))continue;
            xls_typed_function* function=xlsTypedModuleFunction(module,i);
            if(function){
                if(xlsSemNativeCOwnedReadOnly(xlsTypedFunctionRoot(function),0))xlsSemNativeRetain(memo,entry,function);
                else ++memo->nativeWork.readonlyDeclined;
                xlsTypedFunctionRelease(function);
            }
        }
    }
    xrtMapUnit(&memo->nativeOwners);memo->nativeOwnersReady=0;
}
xls_sem_native_work xlsFrontendSemMemoNativeWork(const xls_sem_memo* memo)
{return memo ? memo->nativeWork : (xls_sem_native_work){0};}
