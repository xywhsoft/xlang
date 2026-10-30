/* Current-cohort only. Compare final typed outputs against the independently
 * owned pre-finalization facts,after all original native passes have run.
 * Receipts are exact local deltas,not hashes,AST ownership or freshness. */
struct xls_sem_ir_receipt {
    xatomic32 references;
    xstrbuf data;
    xls_typed_input_identity* identity;
    size_t bytes,nodes,types;
};
typedef struct xls_sem_ir_scope {
    struct xls_sem_ir_scope* previous;
    const XL6_AstNode* root;
    size_t bytes;
} xls_sem_ir_scope;
static _Thread_local xls_sem_ir_scope* xlsSemIrFinalScope;
static xls_sem_ir_receipt* xlsSemIrReceiptRetain(xls_sem_ir_receipt* receipt)
{if(receipt)xrtAtomic32FetchAdd(&receipt->references,1,XMEMORY_RELAXED);return receipt;}
static void xlsSemIrReceiptRelease(xls_sem_ir_receipt* receipt)
{
    if(!receipt || xrtAtomic32FetchSub(&receipt->references,1,XMEMORY_ACQ_REL)!=1)return;
    xrtStrBufFree(&receipt->data);xlsTypedInputIdentityRelease(receipt->identity);xrtFree(receipt);
}
typedef struct xls_sem_ir_writer {
    const xls_sem_ir_receipt* old;
    xstrbuf data;
    size_t at,limit,nodes,types,visits;
    int changed;
} xls_sem_ir_writer;
static int xlsSemIrMaterialize(xls_sem_ir_writer* writer)
{
    if(writer->changed)return 1;
    writer->changed=1;
    if(writer->at && (!writer->old || writer->at>writer->old->data.Size ||
       !xlsSemKeyBytes(&writer->data,writer->old->data.Data,writer->at)))return 0;
    return writer->data.Capacity<=writer->limit;
}
static int xlsSemIrWrite(xls_sem_ir_writer* writer,const void* data,size_t size)
{
    if(writer->at>writer->limit || size>writer->limit-writer->at)return 0;
    if(!writer->changed && writer->old && writer->at<=writer->old->data.Size &&
       size<=writer->old->data.Size-writer->at &&
       (!size || !memcmp(writer->old->data.Data+writer->at,data,size))){
        writer->at+=size;return 1;
    }
    if(!xlsSemIrMaterialize(writer) || !xlsSemKeyBytes(&writer->data,data,size) ||
       writer->data.Capacity>writer->limit)return 0;
    writer->at+=size;return 1;
}
/* Field order is private,process-local and padding-free. ABI is the sole64-bit
 * scalar;32-bit bit patterns exactly retain signed enum/package values too. */
#define XLS_SEM_IR_FIELDS 18u
#define XLS_SEM_IR_U32(field) _Static_assert(sizeof(((XL6_AstNode*)0)->field)<=sizeof(uint32_t),"audit final scalar width: " #field)
XLS_SEM_IR_U32(flags);XLS_SEM_IR_U32(semanticTypeId);
XLS_SEM_IR_U32(semanticSymbolId);XLS_SEM_IR_U32(semanticDeclaredSymbolId);
XLS_SEM_IR_U32(semanticTargetSymbolId);XLS_SEM_IR_U32(semanticCompoundOperatorSymbolId);
XLS_SEM_IR_U32(semanticCallKind);XLS_SEM_IR_U32(semanticLvalueKind);
XLS_SEM_IR_U32(semanticOwnership);XLS_SEM_IR_U32(semanticExitKind);
XLS_SEM_IR_U32(semanticScopeDepth);XLS_SEM_IR_U32(semanticTargetScopeDepth);
XLS_SEM_IR_U32(semanticRegionId);XLS_SEM_IR_U32(semanticTargetRegionId);
XLS_SEM_IR_U32(semanticReceiverSymbolId);XLS_SEM_IR_U32(compiledPackageError);
XLS_SEM_IR_U32(compiledPackageRoot);
#undef XLS_SEM_IR_U32
_Static_assert(sizeof(((XL6_AstNode*)0)->semanticTypeAbiId)==sizeof(uint64_t),"audit final ABI width");
static void xlsSemIrNodeFields(const XL6_AstNode* n,uint64_t out[XLS_SEM_IR_FIELDS])
{
    uint64_t fields[]={(uint32_t)n->flags,n->semanticTypeId,n->semanticTypeAbiId,
        n->semanticSymbolId,n->semanticDeclaredSymbolId,n->semanticTargetSymbolId,
        n->semanticCompoundOperatorSymbolId,(uint32_t)n->semanticCallKind,
        (uint32_t)n->semanticLvalueKind,(uint32_t)n->semanticOwnership,
        (uint32_t)n->semanticExitKind,n->semanticScopeDepth,n->semanticTargetScopeDepth,
        n->semanticRegionId,n->semanticTargetRegionId,n->semanticReceiverSymbolId,
        n->compiledPackageError,(uint32_t)n->compiledPackageRoot};
    _Static_assert(sizeof(fields)/sizeof(*fields)==XLS_SEM_IR_FIELDS,"audit final typed-output fields");
    memcpy(out,fields,sizeof(fields));
}
static void xlsSemIrFactFields(const xls_sem_fact* f,uint64_t out[XLS_SEM_IR_FIELDS])
{
    uint64_t fields[]={(uint32_t)f->flags,f->type,f->abi,f->symbol,f->declared,
        f->target,f->compound,(uint32_t)f->call,(uint32_t)f->lvalue,
        (uint32_t)f->ownership,(uint32_t)f->exit,f->scope,f->targetScope,
        f->region,f->targetRegion,f->receiver,f->packageError,(uint32_t)f->packageRoot};
    memcpy(out,fields,sizeof(fields));
}
static int xlsSemIrRecord(xls_sem_ir_writer* writer,const XL6_AstNode* node,
    const uint64_t* before,const char* previousLink)
{
    if((++writer->visits&63u)==0 && xlsFrontendCancelPoll())return 0;
    if(node->source && node->source->completionQuery)return 0;
    uint64_t current[XLS_SEM_IR_FIELDS];xlsSemIrNodeFields(node,current);
    unsigned char packed[4+8+4*(XLS_SEM_IR_FIELDS-1)];uint32_t mask=0;size_t used=4;
    for(size_t i=0;i<XLS_SEM_IR_FIELDS;++i)if(current[i]!=before[i]){
        mask|=UINT32_C(1)<<i;
        if(i==2){memcpy(packed+used,&current[i],8);used+=8;}
        else{uint32_t value=(uint32_t)current[i];memcpy(packed+used,&value,4);used+=4;}
    }
    const char* link=node->compiledPackageLink;
    int sameLink=link==previousLink || (link && previousLink && !strcmp(link,previousLink));
    if(!sameLink)mask|=UINT32_C(1)<<XLS_SEM_IR_FIELDS;
    memcpy(packed,&mask,sizeof(mask));
    if(!xlsSemIrWrite(writer,packed,used))return 0;
    if(!sameLink){
        size_t size=link ? strlen(link) : 0;if(size>UINT32_MAX)return 0;
        uint32_t header[2]={link!=NULL,(uint32_t)size};
        if(!xlsSemIrWrite(writer,header,sizeof(header)) ||
           (size && !xlsSemIrWrite(writer,link,size)))return 0;
    }
    return 1;
}
static int xlsSemIrTypes(xls_sem_ir_writer* writer,const XL6_AstNode* node,
    const XL6_AstNode* before,size_t depth)
{
    if(!node || !before)return node==before;
    if(depth>512 || node->kind!=before->kind || node->childCount!=before->childCount ||
       (node->childCount && (!node->children || !before->children)) ||
       (before->source && before->source->completionQuery))return 0;
    uint64_t fields[XLS_SEM_IR_FIELDS];xlsSemIrNodeFields(before,fields);
    if(!xlsSemIrRecord(writer,node,fields,before->compiledPackageLink))return 0;
    ++writer->types;
    if(!xlsSemIrTypes(writer,node->inferredType,before->inferredType,depth+1) ||
       !xlsSemIrTypes(writer,node->semanticDeclaredType,before->semanticDeclaredType,depth+1))return 0;
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsSemIrTypes(writer,node->children[i],before->children[i],depth+1))return 0;
    return 1;
}
static int xlsSemIrNodes(xls_sem_ir_writer* writer,const XL6_AstNode* node,
    const xls_sem_unit* unit,size_t depth)
{
    if(!node)return 1;
    if(depth>512 || writer->nodes>=unit->facts.Count || (node->childCount && !node->children))return 0;
    const xls_sem_fact* fact=xrtArrayConstGet(&unit->facts,writer->nodes++);
    uint64_t fields[XLS_SEM_IR_FIELDS];xlsSemIrFactFields(fact,fields);
    if(!xlsSemIrRecord(writer,node,fields,fact->packageLink) ||
       !xlsSemIrTypes(writer,node->inferredType,fact->inferred,depth+1) ||
       !xlsSemIrTypes(writer,node->semanticDeclaredType,fact->declaredType,depth+1))return 0;
    for(size_t i=0;i<node->childCount;++i)
        if(!xlsSemIrNodes(writer,node->children[i],unit,depth+1))return 0;
    return 1;
}
static xls_sem_ir_receipt* xlsSemIrReceipt(const XL6_AstNode* node,
    const xls_sem_unit* unit,const xls_sem_ir_receipt* old,size_t budget)
{
    if(!unit || unit->cursor.path || !unit->facts.Count ||
       budget<=sizeof(xls_sem_ir_receipt) || xlsFrontendCancelObserved())return NULL;
    size_t limit=budget-sizeof(xls_sem_ir_receipt);
    if(limit>XLS_SEM_KEY_BYTES)limit=XLS_SEM_KEY_BYTES;
    xls_sem_ir_writer writer={.old=old,.limit=limit};
    xls_sem_ir_receipt* result=NULL;
    if(!xlsSemIrNodes(&writer,node,unit,0) || writer.nodes!=unit->facts.Count ||
       xlsFrontendCancelObserved())goto done;
    if(old && !writer.changed && writer.at==old->data.Size && writer.nodes==old->nodes &&
       writer.types==old->types && old->bytes<=budget){
        result=xlsSemIrReceiptRetain((xls_sem_ir_receipt*)old);goto done;
    }
    if(!xlsSemIrMaterialize(&writer))goto done;
    result=xrtCalloc(1,sizeof(*result));if(!result)goto done;
    xrtAtomic32Init(&result->references,1);
    result->identity=xlsTypedInputIdentityCreate();
    if(!result->identity){xlsSemIrReceiptRelease(result);result=NULL;goto done;}
    size_t fixed=sizeof(*result)+xlsTypedInputIdentityOwnedBytes(result->identity);
    if(fixed>budget || writer.data.Capacity>budget-fixed){xlsSemIrReceiptRelease(result);result=NULL;goto done;}
    result->data=writer.data;writer.data=(xstrbuf){0};
    result->bytes=fixed+result->data.Capacity;result->nodes=writer.nodes;result->types=writer.types;
done:xrtStrBufFree(&writer.data);return result;
}
static void xlsSemIrFinalizeMemo(xls_sem_memo* memo,const XL6_AstNode* root,
    const xls_sem_memo* previous,size_t budget)
{
    if(!budget || !root || root->childCount!=memo->entries.Count ||
       (root->childCount && !root->children))return;
    if(budget>XLS_SEM_MEMO_BYTES)budget=XLS_SEM_MEMO_BYTES;
    size_t retained=0;
    for(size_t i=0;i<memo->entries.Count;++i){
        xls_sem_entry* entry=xrtArrayGet(&memo->entries,i);
        if(!entry->unit || !xlsSemFunction(root->children[i]))continue;
        if(xlsFrontendCancelPoll())return;
        const xls_sem_entry* old=previous ? xrtArrayConstGet(&previous->entries,i) : NULL;
        const xls_sem_ir_receipt* before=old && old->unit==entry->unit ? old->finalInput : NULL;
        size_t left=memo->bytes<=XLS_SEM_MEMO_BYTES ? XLS_SEM_MEMO_BYTES-memo->bytes : 0;
        if(left>budget-retained)left=budget-retained;
        entry->finalInput=xlsSemIrReceipt(root->children[i],entry->unit,before,left);
        if(entry->finalInput){memo->bytes+=entry->finalInput->bytes;retained+=entry->finalInput->bytes;}
    }
    if(!xlsFrontendCancelObserved())memo->finalizedRoot=root;
}
int xlsFrontendSemMemoBindIrInput(xls_sem_memo* memo,const xls_typed_module* input)
{
    xls_typed_input_identity* owner=xlsTypedModuleIdentity(input);
    if(!memo || !owner || !memo->finalizedRoot || memo->finalizedRoot!=xlsTypedModuleRoot(input))return 0;
    if(memo->inputOwner)return memo->inputOwner==owner;
    memo->inputOwner=xlsTypedInputIdentityRetain(owner);return 1;
}
xls_typed_input_identity* xlsFrontendSemMemoIrIdentity(
    const xls_sem_memo* memo,const xls_typed_module* input,size_t index)
{
    if(!memo || !memo->inputOwner || memo->inputOwner!=xlsTypedModuleIdentity(input) ||
       memo->finalizedRoot!=xlsTypedModuleRoot(input))return NULL;
    const xls_sem_entry* entry=xrtArrayConstGet(&memo->entries,index);
    return entry && entry->finalInput ? entry->finalInput->identity : NULL;
}
