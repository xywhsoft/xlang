/* Current native IR TU only. Pure output assets share neither mutable nodes
 * nor a previous result/AST. Existing raw-unit proofs still precede this layer.
 * The two audited phases read raw local tree facts and the complete context
 * below. Earlier phases do not write those facts; the one CALL-only callKind
 * normalization cannot change operator/INDEX inputs. No language/type whitelist.
 * All native rules execute on misses and the complete verifier always runs. */
enum {XLS_IR_DERIVED_OPERATOR=0,XLS_IR_DERIVED_INDEX=1,XLS_IR_DERIVED_PHASES=2};
typedef struct xls_ir_derived_context {xatomic32 references;xstrbuf key;size_t bytes;} xls_ir_derived_context;
typedef struct xls_ir_derived_input {uint32_t at;XL6_IrTypeId type,storage;} xls_ir_derived_input;
typedef struct xls_ir_derived_row {
    uint32_t at;
    union {
        struct {XL6_IrErrorAbi error;XL6_IrTypeId execution;XL6_IrOperatorOperandAbi operands[2];} op;
        struct {XL6_IrIndexReceiverGate gate;XL6_IrIndexWriteAbi write;XL6_IrErrorAbi error;} index;
    } value;
} xls_ir_derived_row;
typedef struct xls_ir_derived_unit {
    xatomic32 references;xls_ir_unit* raw;xls_ir_derived_context* context;
    xarray rows,inputs;size_t nodes,bytes;unsigned phase;
} xls_ir_derived_unit;
typedef struct xls_ir_derived_range {size_t start,nodes,previous,inputBegin,inputCount;int clean;} xls_ir_derived_range;
struct xls_ir_derived_cache {
    xarray ranges,inputs,phases[XLS_IR_DERIVED_PHASES];size_t inputBytes,inputCursor;
    xls_ir_derived_context* contexts[XLS_IR_DERIVED_PHASES];
};
typedef struct xls_ir_derived_scope {
    struct xls_ir_derived_scope* previous;
    xls_ir_memo_build* build;const XL6_IrModule* module;const XL6_IrNode* nodes;
    size_t count,cursor;unsigned phase;int ready;
} xls_ir_derived_scope;
static _Thread_local xls_ir_derived_scope* xlsActiveIrDerived;
static int xlsIrBuildOperatorAbisOriginal(XL6_IrModule*);
static int xlsIrBuildIndexWriteAbisOriginal(XL6_IrModule*);
static xls_ir_derived_context* xlsIrDerivedContextRetain(xls_ir_derived_context* context)
{if(context)xrtAtomic32FetchAdd(&context->references,1,XMEMORY_RELAXED);return context;}
static void xlsIrDerivedContextRelease(xls_ir_derived_context* context)
{if(context && xrtAtomic32FetchSub(&context->references,1,XMEMORY_ACQ_REL)==1){xrtStrBufFree(&context->key);xrtFree(context);}}
static xls_ir_derived_unit* xlsIrDerivedUnitRetain(xls_ir_derived_unit* unit)
{if(unit)xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED);return unit;}
static void xlsIrDerivedUnitRelease(xls_ir_derived_unit* unit)
{
    if(!unit || xrtAtomic32FetchSub(&unit->references,1,XMEMORY_ACQ_REL)!=1)return;
    xlsIrUnitRelease(unit->raw);xlsIrDerivedContextRelease(unit->context);xrtArrayUnit(&unit->rows);xrtArrayUnit(&unit->inputs);xrtFree(unit);
}
static void xlsIrDerivedDestroy(xls_ir_derived_cache* cache)
{
    if(!cache)return;
    for(unsigned phase=0;phase<XLS_IR_DERIVED_PHASES;++phase){
        for(size_t i=0;i<cache->phases[phase].Count;++i)xlsIrDerivedUnitRelease(*(xls_ir_derived_unit**)xrtArrayGet(&cache->phases[phase],i));
        xrtArrayUnit(&cache->phases[phase]);xlsIrDerivedContextRelease(cache->contexts[phase]);
    }
    xrtArrayUnit(&cache->ranges);xrtArrayUnit(&cache->inputs);xrtFree(cache);
}
static void xlsIrDerivedInit(xls_ir_memo_build* build)
{
    size_t bytes=build->limit-build->memo->bytes,count=build->memo->entries.Count;
    if(!count)return;
    if(bytes<sizeof(xls_ir_derived_cache) || count>(bytes-sizeof(xls_ir_derived_cache))/(sizeof(xls_ir_derived_range)+2*sizeof(void*))){++build->memo->derivedWork.declined;return;}
    xls_ir_derived_cache* cache=xrtCalloc(1,sizeof(*cache));if(!cache){++build->memo->derivedWork.declined;return;}
    size_t charged=sizeof(*cache);
    if(!xrtArrayInit(&cache->inputs,sizeof(xls_ir_derived_input)) ||
       !xrtArrayInit(&cache->ranges,sizeof(xls_ir_derived_range)) || !xrtArrayResize(&cache->ranges,count) ||
       cache->ranges.Capacity>(bytes-charged)/sizeof(xls_ir_derived_range))goto failed;
    charged+=cache->ranges.Capacity*sizeof(xls_ir_derived_range);
    for(unsigned phase=0;phase<XLS_IR_DERIVED_PHASES;++phase){
        if(!xrtArrayInit(&cache->phases[phase],sizeof(xls_ir_derived_unit*)) || !xrtArrayResize(&cache->phases[phase],count) ||
           cache->phases[phase].Capacity>(bytes-charged)/sizeof(xls_ir_derived_unit*))goto failed;
        charged+=cache->phases[phase].Capacity*sizeof(xls_ir_derived_unit*);
    }
    build->memo->derived=cache;build->memo->bytes+=charged;build->memo->derivedWork.bytes+=charged;return;
failed:xlsIrDerivedDestroy(cache);++build->memo->derivedWork.declined;
}
static int xlsIrDerivedRawClean(const XL6_IrNode* node)
{
    return node->memberAbi==XL6_IR_MEMBER_ABI_NONE &&
        node->operatorErrorAbi==XL6_IR_ERROR_ABI_INVALID && node->operatorExecutionType==XL6_IR_TYPE_INVALID &&
        node->operatorOperandAbis[0]==XL6_IR_OPERATOR_OPERAND_INVALID && node->operatorOperandAbis[1]==XL6_IR_OPERATOR_OPERAND_INVALID &&
        node->indexReceiverGate==XL6_IR_INDEX_RECEIVER_UNCONDITIONAL && node->indexWriteAbi==XL6_IR_INDEX_WRITE_NONE &&
        node->indexWriteErrorAbi==XL6_IR_ERROR_ABI_INVALID;
}
static void xlsIrDerivedObserveRaw(const XL6_IrModule* module,const XL6_IrNode* node,const xls_ir_unit* unit)
{
    xls_ir_memo_build* build=xlsActiveIrMemo;
    if(!build || build->module!=module)return;
    if(!xlsIrDerivedRawClean(node) || node->firstChild<unit->startChild || node->firstChild-unit->startChild>unit->children ||
       node->childCount>unit->children-(node->firstChild-unit->startChild) ||
       (node->sourceKind==XL6_AST_BINARY && node->kind!=XL6_IR_NODE_BINARY) ||
       (node->sourceKind==XL6_AST_UNARY && node->kind!=XL6_IR_NODE_UNARY) ||
       ((node->sourceKind==XL6_AST_ASSIGN || node->sourceKind==XL6_AST_MULTI_ASSIGN) && node->kind!=XL6_IR_NODE_ASSIGN) ||
       ((node->sourceKind==XL6_AST_INDEX || node->sourceKind==XL6_AST_SAFE_INDEX) && node->kind!=XL6_IR_NODE_INDEX))build->derivedRawClean=0;
}
static void xlsIrDerivedObserveChildren(const XL6_IrModule* module,const xls_ir_unit* unit)
{
    xls_ir_memo_build* build=xlsActiveIrMemo;
    if(!build || build->module!=module || !build->derivedRawClean)return;
    for(size_t i=unit->startChild;i<module->childCount;++i)
        if(module->children[i]<=unit->startNode || module->children[i]>module->nodeCount){build->derivedRawClean=0;break;}
}
static void xlsIrDerivedRecord(xls_ir_memo_build* build,size_t slot,size_t start,size_t previous)
{
    /* Keep the common one-function/no-raw-unit path allocation-free. A
     * failed metadata attempt is optional and is not repeated per function. */
    if(!build->derivedAttempted){build->derivedAttempted=1;xlsIrDerivedInit(build);}
    xls_ir_derived_cache* cache=build->memo->derived;if(!cache)return;
    xls_ir_unit* const* raw=xrtArrayConstGet(&build->memo->entries,slot);
    xls_ir_derived_range* range=xrtArrayGet(&cache->ranges,slot);
    if(!range || !raw || !*raw || (*raw)->nodes>UINT32_MAX || start>build->module->nodeCount || (*raw)->nodes!=build->module->nodeCount-start)return;
    *range=(xls_ir_derived_range){.start=start,.nodes=(*raw)->nodes,.previous=previous,.clean=build->derivedRawClean};
}
static void xlsIrDerivedActivate(const XL6_IrModule* module)
{
    xls_ir_memo_build* build=xlsActiveIrMemo;
    if(!build || build->module!=module || !build->memo->derived)return;
    size_t end=0;int eligible=0;
    for(size_t i=0;i<build->memo->derived->ranges.Count;++i){
        const xls_ir_derived_range* range=xrtArrayConstGet(&build->memo->derived->ranges,i);
        if(!range->nodes)continue;
        if(range->start<end || range->start>module->nodeCount || range->nodes>module->nodeCount-range->start)return;
        end=range->start+range->nodes;
        if(range->clean)eligible=1;
    }
    build->derivedReady=eligible;
}
static void xlsIrDerivedInputPatch(const XL6_IrModule* module,size_t at)
{
    xls_ir_memo_build* build=xlsActiveIrMemo;
    if(!build || build->module!=module || !build->derivedReady || !build->memo->derived)return;
    xls_ir_derived_cache* cache=build->memo->derived;
    while(cache->inputCursor<cache->ranges.Count){
        xls_ir_derived_range* range=xrtArrayGet(&cache->ranges,cache->inputCursor);
        if(!range->nodes || at>=range->start+range->nodes){++cache->inputCursor;continue;}
        if(at<range->start || !range->clean)return;
        xls_ir_derived_input input={(uint32_t)(at-range->start),module->nodes[at].type,module->nodes[at].storageType};
        size_t limit=build->limit-build->memo->bytes;
        if(cache->inputs.Count>=(limit+cache->inputBytes)/sizeof(input) || !xrtArrayPush(&cache->inputs,&input) ||
           cache->inputs.Capacity>(limit+cache->inputBytes)/sizeof(input)){
            /* The native write already completed. Discard the optional proof
             * transaction;all subsequent phases run their original bodies. */
            xrtArrayUnit(&cache->inputs);build->memo->bytes-=cache->inputBytes;
            build->memo->derivedWork.bytes-=cache->inputBytes;cache->inputBytes=0;build->derivedReady=0;++build->memo->derivedWork.declined;return;
        }
        size_t bytes=cache->inputs.Capacity*sizeof(input),added=bytes-cache->inputBytes;
        build->memo->bytes+=added;build->memo->derivedWork.bytes+=added;cache->inputBytes=bytes;
        if(!range->inputCount)range->inputBegin=cache->inputs.Count-1;
        ++range->inputCount;return;
    }
}
static xls_ir_derived_context* xlsIrDerivedContextBuild(const XL6_IrModule* module,size_t limit)
{
    if(limit<sizeof(xls_ir_derived_context) || module->types.count>module->types.capacity ||
       (module->types.count && !module->types.items) || module->unionLayoutCount>module->unionLayoutCapacity ||
       (module->unionLayoutCount && !module->unionLayouts))return NULL;
    xls_ir_derived_context* context=xrtCalloc(1,sizeof(*context));if(!context)return NULL;
    xrtAtomic32Init(&context->references,1);size_t bytes=limit-sizeof(*context);
    /* The native runtime map is pointer-free; include its complete current
     * representation. All type scalar/text/parameter/result fields retain
     * the existing exact interner-prefix encoder,not a content hash. */
    if(!xlsIrMemoTypes(&module->types,&context->key,bytes) ||
       !xlsIrMemoData(&context->key,&module->runtimeTypeMap,sizeof(module->runtimeTypeMap),bytes) ||
       !xlsIrMemoNumber(&context->key,module->unionLayoutCount,bytes))goto failed;
    for(size_t i=0;i<module->unionLayoutCount;++i){
        const XL6_IrUnionLayout* layout=&module->unionLayouts[i];
        uint64_t fields[]={layout->type,layout->declaration,layout->firstField,layout->fieldCount};
        if(!xlsIrMemoData(&context->key,fields,sizeof(fields),bytes))goto failed;
    }
    context->bytes=sizeof(*context)+context->key.Capacity;return context;
failed:xlsIrDerivedContextRelease(context);return NULL;
}
static int xlsIrDerivedRowsValid(const xls_ir_derived_unit* unit,const xls_ir_derived_range* range,unsigned phase)
{
    if(!unit || unit->phase!=phase || unit->nodes!=range->nodes || !unit->raw || unit->raw->nodes!=range->nodes ||
       unit->rows.Count>unit->rows.Capacity || (unit->rows.Count && !unit->rows.Data))return 0;
    for(size_t i=0;i<unit->rows.Count;++i){
        const xls_ir_derived_row* row=xrtArrayConstGet(&unit->rows,i);
        if(row->at>=range->nodes || (i && ((const xls_ir_derived_row*)unit->rows.Data)[i-1].at>=row->at))return 0;
    }
    return 1;
}
static int xlsIrDerivedInputsMatch(const xls_ir_derived_unit* unit,const xls_ir_derived_cache* cache,const xls_ir_derived_range* range)
{
    _Static_assert(sizeof(xls_ir_derived_input)==3*sizeof(uint32_t),"audit padding-free native input projection");
    if(unit->inputs.Count>unit->inputs.Capacity || (unit->inputs.Count && !unit->inputs.Data) ||
       unit->inputs.Count!=range->inputCount || range->inputBegin>cache->inputs.Count ||
       range->inputCount>cache->inputs.Count-range->inputBegin)return 0;
    return !range->inputCount || !memcmp(unit->inputs.Data,cache->inputs.Data+range->inputBegin*sizeof(xls_ir_derived_input),range->inputCount*sizeof(xls_ir_derived_input));
}
static void xlsIrDerivedBegin(xls_ir_derived_scope* scope,XL6_IrModule* module,unsigned phase)
{
    memset(scope,0,sizeof(*scope));scope->previous=xlsActiveIrDerived;xlsActiveIrDerived=scope;
    xls_ir_memo_build* build=xlsActiveIrMemo;
    if(!build || build->module!=module || !build->derivedReady || !build->memo->derived || xlsFrontendCancelObserved())return;
    xls_ir_derived_cache* cache=build->memo->derived;
    if(cache->contexts[phase])return; /* one construction pass per phase */
    size_t limit=build->limit-build->memo->bytes;
    xls_ir_derived_context* context=xlsIrDerivedContextBuild(module,limit);if(!context){++build->memo->derivedWork.declined;return;}
    const xls_ir_derived_cache* previous=build->baseline ? build->baseline->derived : NULL;
    if(previous && previous->contexts[phase] && xlsIrMemoEqual(&context->key,&previous->contexts[phase]->key)){
        xlsIrDerivedContextRelease(context);context=xlsIrDerivedContextRetain(previous->contexts[phase]);
    }
    if(context->bytes>limit){xlsIrDerivedContextRelease(context);++build->memo->derivedWork.declined;return;}
    cache->contexts[phase]=context;build->memo->bytes+=context->bytes;build->memo->derivedWork.bytes+=context->bytes;
    ++build->memo->derivedWork.contexts;
    for(size_t i=0;i<cache->ranges.Count;++i){
        const xls_ir_derived_range* range=xrtArrayConstGet(&cache->ranges,i);if(!range->nodes || !range->clean)continue;
        xls_ir_unit* const* raw=xrtArrayConstGet(&build->memo->entries,i);
        xls_ir_derived_unit* const* prior=previous ? xrtArrayConstGet(&previous->phases[phase],range->previous) : NULL;
        xls_ir_derived_unit* unit=prior ? *prior : NULL;
        if(raw && unit && unit->raw==*raw && unit->context==context && xlsIrDerivedRowsValid(unit,range,phase) &&
           xlsIrDerivedInputsMatch(unit,cache,range) && unit->bytes<=build->limit-build->memo->bytes){
            *(xls_ir_derived_unit**)xrtArrayGet(&cache->phases[phase],i)=xlsIrDerivedUnitRetain(unit);
            build->memo->bytes+=unit->bytes;build->memo->derivedWork.bytes+=unit->bytes;++build->memo->derivedWork.reused;
        }else ++build->memo->derivedWork.checked;
    }
    scope->build=build;scope->module=module;scope->nodes=module->nodes;scope->count=module->nodeCount;scope->phase=phase;scope->ready=1;
}
static void xlsIrDerivedApply(XL6_IrModule* module,const xls_ir_derived_range* range,const xls_ir_derived_unit* unit)
{
    for(size_t i=0;i<unit->rows.Count;++i){
        const xls_ir_derived_row* row=xrtArrayConstGet(&unit->rows,i);XL6_IrNode* node=&module->nodes[range->start+row->at];
        if(unit->phase==XLS_IR_DERIVED_OPERATOR){
            node->operatorErrorAbi=row->value.op.error;node->operatorExecutionType=row->value.op.execution;
            node->operatorOperandAbis[0]=row->value.op.operands[0];node->operatorOperandAbis[1]=row->value.op.operands[1];
        }else{
            node->indexReceiverGate=row->value.index.gate;node->indexWriteAbi=row->value.index.write;node->indexWriteErrorAbi=row->value.index.error;
        }
    }
}
static size_t xlsIrDerivedNext(XL6_IrModule* module,unsigned phase,size_t next)
{
    xls_ir_derived_scope* scope=xlsActiveIrDerived;
    if(!scope || !scope->ready || scope->phase!=phase || scope->module!=module || scope->nodes!=module->nodes || scope->count!=module->nodeCount)return next;
    xls_ir_derived_cache* cache=scope->build->memo->derived;
    if(!next)scope->cursor=0;
    while(scope->cursor<cache->ranges.Count){
        size_t slot=scope->cursor;const xls_ir_derived_range* range=xrtArrayConstGet(&cache->ranges,slot);
        if(!range->nodes || range->start+range->nodes<=next){++scope->cursor;continue;}
        if(next<range->start)return next;
        xls_ir_derived_unit* unit=*(xls_ir_derived_unit**)xrtArrayGet(&cache->phases[phase],slot);
        if(!unit)return next;
        /* INDEX has two loops. Reapplying the same complete result before
         * either loop is harmless: its first loop reads no index-error field,
         * and its second reads only already-computed target write ABIs. */
        xlsIrDerivedApply(module,range,unit);
        scope->build->memo->derivedWork.nodesSkipped+=range->nodes;
        scope->build->memo->derivedWork.rowsApplied+=unit->rows.Count;
        next=range->start+range->nodes;++scope->cursor;
    }
    return next;
}
static int xlsIrDerivedReadRow(xls_ir_derived_row* row,const XL6_IrNode* node,unsigned phase,uint32_t at)
{
    memset(row,0,sizeof(*row));row->at=at;
    if(phase==XLS_IR_DERIVED_OPERATOR){
        row->value.op.error=node->operatorErrorAbi;row->value.op.execution=node->operatorExecutionType;
        row->value.op.operands[0]=node->operatorOperandAbis[0];row->value.op.operands[1]=node->operatorOperandAbis[1];
        return node->operatorErrorAbi!=XL6_IR_ERROR_ABI_INVALID || node->operatorExecutionType!=XL6_IR_TYPE_INVALID ||
            node->operatorOperandAbis[0]!=XL6_IR_OPERATOR_OPERAND_INVALID || node->operatorOperandAbis[1]!=XL6_IR_OPERATOR_OPERAND_INVALID;
    }
    row->value.index.gate=node->indexReceiverGate;row->value.index.write=node->indexWriteAbi;row->value.index.error=node->indexWriteErrorAbi;
    return node->indexReceiverGate!=XL6_IR_INDEX_RECEIVER_UNCONDITIONAL || node->indexWriteAbi!=XL6_IR_INDEX_WRITE_NONE || node->indexWriteErrorAbi!=XL6_IR_ERROR_ABI_INVALID;
}
static void xlsIrDerivedEnd(xls_ir_derived_scope* scope,int accepted)
{
    if(scope->ready && accepted && scope->nodes==scope->module->nodes && scope->count==scope->module->nodeCount && !xlsFrontendCancelObserved()){
        xls_ir_memo_build* build=scope->build;xls_ir_derived_cache* cache=build->memo->derived;unsigned phase=scope->phase;
        for(size_t slot=0;slot<cache->ranges.Count;++slot){
            const xls_ir_derived_range* range=xrtArrayConstGet(&cache->ranges,slot);
            if(!range->nodes || !range->clean || *(xls_ir_derived_unit**)xrtArrayGet(&cache->phases[phase],slot))continue;
            size_t limit=build->limit-build->memo->bytes;
            xls_ir_derived_unit* unit=limit>=sizeof(*unit) ? xrtCalloc(1,sizeof(*unit)) : NULL;
            if(!unit){++build->memo->derivedWork.declined;continue;}
            xrtAtomic32Init(&unit->references,1);unit->nodes=range->nodes;unit->phase=phase;
            int okay=xrtArrayInit(&unit->rows,sizeof(xls_ir_derived_row)) && xrtArrayInit(&unit->inputs,sizeof(xls_ir_derived_input)) &&
                range->inputCount<=(limit-sizeof(*unit))/sizeof(xls_ir_derived_input) && xrtArrayResize(&unit->inputs,range->inputCount) &&
                unit->inputs.Capacity<=(limit-sizeof(*unit))/sizeof(xls_ir_derived_input);
            size_t inputBytes=unit->inputs.Capacity*sizeof(xls_ir_derived_input);
            if(okay && range->inputCount)memcpy(unit->inputs.Data,cache->inputs.Data+range->inputBegin*sizeof(xls_ir_derived_input),range->inputCount*sizeof(xls_ir_derived_input));
            for(size_t i=0;okay && i<range->nodes;++i){
                xls_ir_derived_row row;
                if(xlsIrDerivedReadRow(&row,&scope->module->nodes[range->start+i],phase,(uint32_t)i))
                    okay=unit->rows.Count<(limit-sizeof(*unit)-inputBytes)/sizeof(row) && xrtArrayPush(&unit->rows,&row) &&
                        unit->rows.Capacity<=(limit-sizeof(*unit)-inputBytes)/sizeof(row);
            }
            unit->bytes=sizeof(*unit)+unit->rows.Capacity*sizeof(xls_ir_derived_row)+inputBytes;
            if(okay && unit->bytes<=limit){
                unit->raw=xlsIrUnitRetain(*(xls_ir_unit**)xrtArrayGet(&build->memo->entries,slot));
                unit->context=xlsIrDerivedContextRetain(cache->contexts[phase]);
                *(xls_ir_derived_unit**)xrtArrayGet(&cache->phases[phase],slot)=unit;
                build->memo->bytes+=unit->bytes;build->memo->derivedWork.bytes+=unit->bytes;++build->memo->derivedWork.captured;
            }else{xlsIrDerivedUnitRelease(unit);++build->memo->derivedWork.declined;}
        }
    }
    xlsActiveIrDerived=scope->previous;
}
static int xl6IrBuildOperatorErrorAbis(XL6_IrModule* module)
{
    xls_ir_derived_scope scope;xlsIrDerivedBegin(&scope,module,XLS_IR_DERIVED_OPERATOR);
    int accepted=xlsIrBuildOperatorAbisOriginal(module);xlsIrDerivedEnd(&scope,accepted);return accepted;
}
static int xl6IrBuildIndexWriteAbis(XL6_IrModule* module)
{
    xls_ir_derived_scope scope;xlsIrDerivedBegin(&scope,module,XLS_IR_DERIVED_INDEX);
    int accepted=xlsIrBuildIndexWriteAbisOriginal(module);xlsIrDerivedEnd(&scope,accepted);return accepted;
}
