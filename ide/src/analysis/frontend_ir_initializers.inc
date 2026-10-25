/* One borrowed table in an audited read-only phase. No result or table
 * identity survives that phase,and fields are read from the current arena. */
static _Thread_local xls_ir_initializer_scope* xlsActiveInitializers;
static _Thread_local xls_ir_initializer_work xlsInitializerWork;
static int xlsIrValueInitializerMayRaiseOriginal(const XL6_IrModule*,XL6_IrNodeId);
static const XL6_IrValueInitializer* xlsIrValueInitializerGetOriginal(const XL6_IrModule*,XL6_IrNodeId);
void xlsFrontendIrInitializerBegin(xls_ir_initializer_scope* scope,const XL6_IrModule* module)
{
    *scope=(xls_ir_initializer_scope){.previous=xlsActiveInitializers,.module=module,.allowed=1};
    xlsActiveInitializers=scope;
}
void xlsFrontendIrInitializerEnd(xls_ir_initializer_scope* scope)
{if(xlsActiveInitializers==scope)xlsActiveInitializers=scope->previous;}
void xlsFrontendIrInitializerReset(void){memset(&xlsInitializerWork,0,sizeof(xlsInitializerWork));}
xls_ir_initializer_work xlsFrontendIrInitializerWork(void){return xlsInitializerWork;}
int xlsFrontendIrInitializerActive(void){return xlsActiveInitializers!=NULL;}
static void xlsIrInitializerValidated(const XL6_IrModule* module)
{
    /* The verifier calls this only after its original complete initializer
     * validation. Earlier queries must not pre-scan an unvalidated arena. */
    if(xlsActiveInitializers && xlsActiveInitializers->module==module)xlsActiveInitializers->allowed=1;
}
static const XL6_IrValueInitializer* xlsIrInitializerFind(const XL6_IrModule* module,XL6_IrNodeId declaration,int* used)
{
    *used=0;++xlsInitializerWork.queries;
    xls_ir_initializer_scope* scope=xlsActiveInitializers;
    if(!module || !declaration || !scope || scope->module!=module || !scope->allowed)goto linear;
    if(!scope->attempted){
        scope->attempted=1;
        /* Delay the proof until the original query would touch this arena.
         * Malformed/unordered tables retain the original first-match scan. */
        if(module->valueInitializerCount>module->valueInitializerCapacity ||
           (module->valueInitializerCount && !module->valueInitializers))goto linear;
        for(size_t i=0;i<module->valueInitializerCount;++i){
            ++xlsInitializerWork.proofRecords;
            if(i && module->valueInitializers[i-1].declaration>module->valueInitializers[i].declaration)goto linear;
        }
        scope->source=module->valueInitializers;scope->count=module->valueInitializerCount;scope->ready=1;
        ++xlsInitializerWork.indexes;
    }
    if(!scope->ready)goto linear;
    if(scope->source!=module->valueInitializers || scope->count!=module->valueInitializerCount){
        /* A phase contract violation cannot revive an old proof if a later
         * count happens to match again. No rebuilding from a partial table. */
        scope->ready=0;goto linear;
    }
    size_t low=0,high=scope->count;
    while(low<high){
        size_t mid=low+(high-low)/2;++xlsInitializerWork.probes;
        if(scope->source[mid].declaration<declaration)low=mid+1;else high=mid;
    }
    *used=1;++xlsInitializerWork.indexedQueries;
    if(low==scope->count)return NULL;
    ++xlsInitializerWork.probes;
    return scope->source[low].declaration==declaration ? &scope->source[low] : NULL;
linear:++xlsInitializerWork.linearQueries;return NULL;
}
static int xl6IrValueInitializerMayRaise(const XL6_IrModule* module,XL6_IrNodeId declaration)
{
    int used;const XL6_IrValueInitializer* value=xlsIrInitializerFind(module,declaration,&used);
    return used ? value && value->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE : xlsIrValueInitializerMayRaiseOriginal(module,declaration);
}
const XL6_IrValueInitializer* xl6IrValueInitializerGet(const XL6_IrModule* module,XL6_IrNodeId declaration)
{
    int used;const XL6_IrValueInitializer* value=xlsIrInitializerFind(module,declaration,&used);
    return used ? value : xlsIrValueInitializerGetOriginal(module,declaration);
}
