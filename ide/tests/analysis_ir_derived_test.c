#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Actual generated TU and independently extracted original pass bodies. */
static uint64_t derivedBodyVisits[2];
#define XLS_IR_DERIVED_BODY_HIT(phase) (++derivedBodyVisits[phase])
#include "xl6_ir.c"
#ifdef XLS_NATIVE_IR_DERIVED_PHASES
#include "ir_derived_reference.inc"
#include "src/sem/xl6_sem.h"
static unsigned derivedChecks;
static xarray* refusedInputs;
static unsigned refusedCount;
static unsigned faultKind,faultDenied;
static size_t faultAt,faultVisits;
static int faultEnabled,faultInit;
static int derivedRefuse(unsigned kind)
{if(faultEnabled && faultKind==kind && (faultInit || xlsActiveIrDerived) && ++faultVisits==faultAt){++faultDenied;return 1;}return 0;}
void* __real_xrtCalloc(size_t,size_t);
void* __wrap_xrtCalloc(size_t n,size_t size){return derivedRefuse(1) ? NULL : __real_xrtCalloc(n,size);}
bool __real_xrtArrayInit(xarray*,size_t),__real_xrtArrayResize(xarray*,size_t);
bool __wrap_xrtArrayInit(xarray* array,size_t size){return derivedRefuse(2) ? false : __real_xrtArrayInit(array,size);}
bool __wrap_xrtArrayResize(xarray* array,size_t count){return derivedRefuse(3) ? false : __real_xrtArrayResize(array,count);}
bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __wrap_xrtStrBufReserve(xstrbuf* text,size_t count){return derivedRefuse(5) ? false : __real_xrtStrBufReserve(text,count);}
bool __real_xrtArrayPush(xarray*,const void*);
bool __wrap_xrtArrayPush(xarray* array,const void* item)
{if(array==refusedInputs){++refusedCount;return false;}return derivedRefuse(4) ? false : __real_xrtArrayPush(array,item);}
#define CHECK(v) do{++derivedChecks;if(!(v)){fprintf(stderr,"analysis_ir_derived_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static char* derivedDump(const XL6_IrModule* module)
{
    XL6_Output output;CHECK(xl6OutputOpenMemory(&output));xl6IrDumpModuleJson(&output,module);
    char* text=xl6OutputHasFailed(&output) ? NULL : xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);return text;
}
static uint64_t derivedLive(void)
{xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static int originalPasses(const XL6_IrModule* module);
static int dirtyRaw(xls_ir_unit* unit,const XL6_AstNode* source,xstrbuf* output)
{
    xls_lexical_symbols symbols={0};CHECK(xlsLexicalSymbolsBuild(&symbols,source,XLS_LEXICAL_SYMBOL_BYTES));
    xls_ir_reader reader={(const unsigned char*)unit->data.Data,0,unit->data.Size};int changed=0;
    for(size_t i=0;i<unit->nodes && !changed;++i){
        size_t start=reader.at;XL6_IrNode node={0};CHECK(xlsIrMemoDecodeNode(&reader,&node,1,&symbols,unit,NULL));
        if(node.sourceKind!=XL6_AST_INT)continue;
        size_t offset=offsetof(XL6_IrNode,indexWriteErrorAbi),word=offset/8;uint64_t mask;memcpy(&mask,unit->data.Data+start,8);
        size_t before=0;for(size_t bit=0;bit<word;++bit)if(mask&(UINT64_C(1)<<bit))++before;
        size_t at=start+8+before*8;uint64_t value=0;
        int present=(mask&(UINT64_C(1)<<word))!=0;if(present)memcpy(&value,unit->data.Data+at,8);
        value&=~(UINT64_C(0xffffffff)<<((offset%8)*8));value|=(uint64_t)XL6_IR_ERROR_ABI_MAY_RAISE<<((offset%8)*8);mask|=UINT64_C(1)<<word;
        CHECK(xlsIrMemoData(output,unit->data.Data,start,32u*1024u*1024u) && xlsIrMemoData(output,&mask,8,32u*1024u*1024u) &&
            xlsIrMemoData(output,unit->data.Data+start+8,before*8,32u*1024u*1024u) && xlsIrMemoData(output,&value,8,32u*1024u*1024u) &&
            xlsIrMemoData(output,unit->data.Data+at+(present ? 8 : 0),unit->data.Size-at-(present ? 8 : 0),32u*1024u*1024u));changed=1;
    }
    xlsLexicalSymbolsClear(&symbols);CHECK(changed);return 1;
}
static int allocationFailures(void)
{
    for(unsigned kind=1;kind<=3;++kind){
        size_t visits=0;
        for(size_t at=0;at<=visits+1;++at){
            uint64_t live=derivedLive();xls_ir_memo memo={.entries={.Count=8}};xls_ir_memo_build build={.memo=&memo,.limit=32u*1024u*1024u};
            faultKind=kind;faultAt=at ? at : SIZE_MAX;faultVisits=faultDenied=0;faultInit=faultEnabled=1;
            xlsIrDerivedInit(&build);faultEnabled=faultInit=0;
            if(!at)visits=faultVisits;
            CHECK(at && at<=visits ? faultDenied==1 && !memo.derived && !memo.bytes : !faultDenied && memo.derived && memo.bytes);
            xlsIrDerivedDestroy(memo.derived);CHECK(derivedLive()==live);
        }
    }
    const char* source="func first(int value) as int\n return value+1\nend\nfunc second(int value) as int\n return value+2\nend\nfunc edited(int value) as int\n return value+3\nend\n";
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);CHECK(xl6ParseSource(source,"derived-faults.xl",&parsed) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"derived-faults",1));
    XL6_DiagList diags;xl6DiagInit(&diags);xls_ir_result* previous=xlsFrontendIrResultBuild(parsed.root,&diags,NULL,32u*1024u*1024u);CHECK(previous && !diags.count);
    XL6_AstNode* edited=parsed.root->children[parsed.root->childCount-1];xarray stack={0};CHECK(xrtArrayInit(&stack,sizeof(XL6_AstNode*)) && xrtArrayPush(&stack,&edited));unsigned changed=0;
    XL6_AstNode* changedNode=NULL;
    while(stack.Count){XL6_AstNode* node=*(XL6_AstNode**)xrtArrayGet(&stack,stack.Count-1);--stack.Count;
        if(node->kind==XL6_AST_INT && node->text && !strcmp(node->text,"3")){char* value=xrtStrDup("4");CHECK(value);xrtFree(node->text);node->text=value;changedNode=node;++changed;}
        for(size_t i=0;i<node->childCount;++i)CHECK(xrtArrayPush(&stack,&node->children[i]));
    }
    xrtArrayUnit(&stack);CHECK(changed==1);
    xls_ir_result* control=xlsFrontendIrResultBuild(parsed.root,&diags,NULL,0);CHECK(control && !diags.count);char* expected=derivedDump(&control->module);CHECK(expected);
    for(unsigned kind=1;kind<=5;++kind){
        size_t visits=0;
        for(size_t at=0;at<=visits;++at){
            uint64_t live=derivedLive();faultKind=kind;faultAt=at ? at : SIZE_MAX;faultVisits=faultDenied=0;faultEnabled=1;
            xls_ir_result* result=xlsFrontendIrResultBuild(parsed.root,&diags,previous,32u*1024u*1024u);faultEnabled=0;
            if(!at)visits=faultVisits;
            CHECK(result && !diags.count && (at ? faultDenied==1 : !faultDenied));
            char* actual=derivedDump(&result->module);CHECK(actual && !strcmp(actual,expected) && originalPasses(&result->module));xrtFree(actual);
            CHECK(result->memo->bytes<=32u*1024u*1024u && !xlsActiveIrDerived && !xlsActiveIrMemo);xlsFrontendIrResultDestroy(result);CHECK(derivedLive()==live);
        }
        CHECK(visits>0);printf("derived phase allocation kind %u: %zu actual failure sites\n",kind,visits);
    }
    const size_t budgets[]={0,1,256,1024,16384,65536};
    for(unsigned i=0;i<sizeof(budgets)/sizeof(*budgets);++i){
        xls_ir_result* result=xlsFrontendIrResultBuild(parsed.root,&diags,previous,budgets[i]);CHECK(result && !diags.count);
        char* actual=derivedDump(&result->module);CHECK(actual && !strcmp(actual,expected));xrtFree(actual);
        CHECK(!result->memo || result->memo->bytes<=budgets[i]);xlsFrontendIrResultDestroy(result);
    }
    char* originalText=xrtStrDup("3");CHECK(originalText && changedNode);xrtFree(changedNode->text);changedNode->text=originalText;
    size_t slot=parsed.root->childCount-1;xls_ir_unit* raw=*(xls_ir_unit**)xrtArrayGet(&previous->memo->entries,slot);CHECK(raw);
    xstrbuf dirty={0};CHECK(dirtyRaw(raw,edited,&dirty));xstrbuf saved=raw->data;raw->data=dirty;
    xls_ir_result* repaired=xlsFrontendIrResultBuild(parsed.root,&diags,previous,32u*1024u*1024u);CHECK(repaired && !diags.count);
    CHECK(xlsFrontendIrResultWork(repaired).reused>=2 && !((const xls_ir_derived_range*)xrtArrayConstGet(&repaired->memo->derived->ranges,slot))->clean);
    char *left=derivedDump(&repaired->module),*right=derivedDump(&previous->module);CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);
    /* Decoded text may borrow the temporary buffer. Keep it alive until the
     * new result dies,then restore the original immutable raw-unit bytes. */
    xlsFrontendIrResultDestroy(repaired);raw->data=saved;xrtStrBufFree(&dirty);
    xls_ir_derived_unit* output=*(xls_ir_derived_unit**)xrtArrayGet(&previous->memo->derived->phases[0],slot);CHECK(output && output->rows.Count);
    xls_ir_derived_row* row=xrtArrayGet(&output->rows,0);uint32_t oldAt=row->at;row->at=(uint32_t)output->nodes;
    repaired=xlsFrontendIrResultBuild(parsed.root,&diags,previous,32u*1024u*1024u);row->at=oldAt;CHECK(repaired && !diags.count);
    left=derivedDump(&repaired->module);right=derivedDump(&previous->module);CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);xlsFrontendIrResultDestroy(repaired);
    xrtFree(expected);xlsFrontendIrResultDestroy(control);xlsFrontendIrResultDestroy(previous);xl6DiagFree(&diags);xl6ParseResultFree(&parsed);return 1;
}
static int originalPasses(const XL6_IrModule* module)
{
    XL6_IrModule original=*module;original.nodes=xrtMalloc(module->nodeCount*sizeof(*original.nodes));CHECK(original.nodes);
    memcpy(original.nodes,module->nodes,module->nodeCount*sizeof(*original.nodes));
    for(size_t i=0;i<module->nodeCount;++i){
        original.nodes[i].operatorErrorAbi=XL6_IR_ERROR_ABI_INVALID;original.nodes[i].operatorExecutionType=XL6_IR_TYPE_INVALID;
        original.nodes[i].operatorOperandAbis[0]=original.nodes[i].operatorOperandAbis[1]=XL6_IR_OPERATOR_OPERAND_INVALID;
        original.nodes[i].indexReceiverGate=XL6_IR_INDEX_RECEIVER_UNCONDITIONAL;original.nodes[i].indexWriteAbi=XL6_IR_INDEX_WRITE_NONE;
        original.nodes[i].indexWriteErrorAbi=XL6_IR_ERROR_ABI_INVALID;
    }
    CHECK(xlsDerivedReferenceOPERATOR(&original) && xlsDerivedReferenceINDEX(&original));
    CHECK(!memcmp(original.nodes,module->nodes,module->nodeCount*sizeof(*module->nodes)));xrtFree(original.nodes);return 1;
}
static int namedWrites(const xls_ir_result* result)
{
    for(unsigned fault=0;fault<2;++fault){
        XL6_IrModule current=result->module,reference=result->module;
        current.nodes=xrtMalloc(current.nodeCount*sizeof(*current.nodes));reference.nodes=xrtMalloc(reference.nodeCount*sizeof(*reference.nodes));CHECK(current.nodes && reference.nodes);
        memcpy(current.nodes,result->module.nodes,current.nodeCount*sizeof(*current.nodes));memcpy(reference.nodes,current.nodes,current.nodeCount*sizeof(*current.nodes));
        size_t selected=0;
        for(size_t i=0;i<current.nodeCount;++i)if(current.nodes[i].namedCallableAbi!=XL6_IR_NAMED_CALLABLE_NONE){
            current.nodes[i].type=current.nodes[i].storageType=0;reference.nodes[i].type=reference.nodes[i].storageType=0;++selected;
            /* The isolated node copies borrow immutable tables. Prove the
             * native miss arm will find an existing signature before calling
             * a builder which could otherwise append/reallocate that pool. */
            XL6_IrNodeId declaration=xl6IrNamedCallableDeclaration(&current,(XL6_IrNodeId)(i+1));
            const XL6_IrNode *parameters=NULL,*returns=NULL;XL6_IrCallableErasureAbi erasure;
            CHECK(declaration && xl6IrNamedCallableSignatureParts(&current,declaration,&parameters,&returns,&erasure));
            CHECK(xl6IrFindNamedCallableSignature(&current,parameters,returns,erasure)!=XL6_IR_TYPE_INVALID);
        }
        CHECK(selected>=2);
        xls_ir_memo memo={.entries=result->memo->entries};xls_ir_memo_build build={.memo=&memo,.module=&current,.limit=32u*1024u*1024u};xlsIrDerivedInit(&build);CHECK(memo.derived);
        memcpy(memo.derived->ranges.Data,result->memo->derived->ranges.Data,memo.derived->ranges.Count*sizeof(xls_ir_derived_range));build.derivedReady=1;
        xls_ir_memo_build* previous=xlsActiveIrMemo;xlsActiveIrMemo=&build;refusedCount=0;refusedInputs=fault ? &memo.derived->inputs : NULL;
        CHECK(xl6IrBuildNamedCallableAbis(&current));refusedInputs=NULL;xlsActiveIrMemo=previous;
        CHECK(xlsDerivedReferenceNAMED(&reference));CHECK(!memcmp(current.nodes,reference.nodes,current.nodeCount*sizeof(*current.nodes)));
        CHECK(current.types.items==result->module.types.items && current.types.count==result->module.types.count);
        if(fault)CHECK(!build.derivedReady && refusedCount==1 && !memo.derived->inputs.Data && memo.derivedWork.declined==1);
        else{
            CHECK(build.derivedReady && memo.derived->inputs.Count>=1);
            build.baseline=result->memo;
            for(size_t i=0;i<memo.derived->ranges.Count;++i)((xls_ir_derived_range*)memo.derived->ranges.Data)[i].previous=i;
            xls_ir_derived_scope scope;xlsActiveIrMemo=&build;xlsIrDerivedBegin(&scope,&current,0);
            CHECK(scope.ready && memo.derivedWork.reused>0);
            unsigned mismatches=0;
            for(size_t i=0;i<memo.derived->ranges.Count;++i){
                const xls_ir_derived_range* range=xrtArrayConstGet(&memo.derived->ranges,i);if(!range->inputCount)continue;
                const xls_ir_derived_unit* unit=*(xls_ir_derived_unit* const*)xrtArrayConstGet(&result->memo->derived->phases[0],i);CHECK(unit);
                CHECK(!xlsIrDerivedInputsMatch(unit,memo.derived,range));++mismatches;
                CHECK(!*(xls_ir_derived_unit**)xrtArrayGet(&memo.derived->phases[0],i));
            }
            xlsIrDerivedEnd(&scope,0);xlsActiveIrMemo=previous;
            CHECK(mismatches>=1);
        }
        xlsIrDerivedDestroy(memo.derived);xrtFree(current.nodes);xrtFree(reference.nodes);
    }
    return 1;
}
static int sequence(unsigned scenario)
{
    char* path=xrtPathAbs("current-derived-ir.xl");CHECK(path);xls_ir_result* previous=NULL;
    for(unsigned generation=0;generation<6;++generation){
        xstrbuf source={0};if(generation==2)CHECK(xrtStrBufAppend(&source,xrtStrView("// shifted coordinates\n")));
        if(scenario==1)CHECK(xrtStrBufAppend(&source,xrtStrView("func seed_string(string value) as char\n return 'a'\nend\n")));
        if(scenario==2)CHECK(xrtStrBufAppend(&source,xrtStrView("func seed_u8(uint8 value) as bool\n return true\nend\n")));
        for(unsigned f=0;f<8;++f){
            CHECK(xrtStrBufAppendFormat(&source,"func stable_%u(int seed) as int\n int total=seed\n",f));
            if(generation>=3 && !f)CHECK(xrtStrBufAppend(&source,xrtStrView(" int added=1\n")));
            for(unsigned n=0;n<16;++n)CHECK(xrtStrBufAppendFormat(&source," total=total+%u\n",n));
            CHECK(xrtStrBufAppend(&source,xrtStrView(" return total\nend\n")));
        }
        if(scenario==1)CHECK(xrtStrBufAppend(&source,xrtStrView("func mutate(string value) as string\n value[0]='x'\n return value\nend\n")));
        if(scenario==2)CHECK(xrtStrBufAppendFormat(&source,"func narrow(uint8 value) as bool\n return value<%u\nend\n",generation%2 ? 256u : 255u));
        if(scenario==3)CHECK(xrtStrBufAppend(&source,xrtStrView("func named(int value) as int\n return value\nend\nfunc seed_firstclass()\n auto first=named\nend\nfunc firstclass()\n auto copy=named\nend\n")));
        if(scenario==4)CHECK(xrtStrBufAppend(&source,xrtStrView(generation%2 ? "async func remote() as bool\n return true\nend\n" : "async func remote() as int\n return 1\nend\n")));
        CHECK(xrtStrBufAppendFormat(&source,"func edited(int value) as int\n return value+%u\nend\n",generation ? generation+3 : 3));
        XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
        int parsedOkay=xl6ParseSource(source.Data,path,&parsed);
        int semantic=parsedOkay && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"derived",1);
        if(!semantic)for(size_t i=0;i<parsed.diags.count;++i)fprintf(stderr,"derived source %u/%u: %s\n",scenario,generation,parsed.diags.items[i].message);
        CHECK(semantic);
        XL6_DiagList a,b;xl6DiagInit(&a);xl6DiagInit(&b);
        xls_ir_result* control=xlsFrontendIrResultBuild(parsed.root,&b,NULL,0);CHECK(control && !b.count);
        derivedBodyVisits[0]=derivedBodyVisits[1]=0;
        xls_ir_result* result=xlsFrontendIrResultBuild(parsed.root,&a,previous,32u*1024u*1024u);
        if(!result)for(size_t i=0;i<a.count;++i)fprintf(stderr,"derived build %u/%u: %s\n",scenario,generation,a.items[i].message);
        CHECK(result && !a.count && originalPasses(xlsFrontendIrResultModule(result)));
        char *left=derivedDump(xlsFrontendIrResultModule(result)),*right=derivedDump(xlsFrontendIrResultModule(control));CHECK(left && right && !strcmp(left,right));xrtFree(left);xrtFree(right);
        xls_ir_derived_work work=xlsFrontendIrResultDerivedWork(result);
        CHECK(derivedBodyVisits[0]+derivedBodyVisits[1]+work.nodesSkipped==3*result->module.nodeCount);
        CHECK(work.contexts==2 && work.bytes && result->memo->bytes<=32u*1024u*1024u);
        if(generation && scenario==4){CHECK(xlsFrontendIrResultWork(result).reused>=6 && !work.reused && !work.nodesSkipped);}
        else if(generation){CHECK(work.reused>=10 && work.nodesSkipped>100);}
        else CHECK(!work.reused && work.captured>=10);
        size_t indexRows=0;
        for(size_t i=0;i<result->memo->derived->phases[XLS_IR_DERIVED_INDEX].Count;++i){const xls_ir_derived_unit* unit=*(xls_ir_derived_unit* const*)xrtArrayConstGet(&result->memo->derived->phases[XLS_IR_DERIVED_INDEX],i);if(unit)indexRows+=unit->rows.Count;}
        if(scenario==1)CHECK(indexRows>=2);
        if(scenario==3 && !generation)CHECK(namedWrites(result));
        printf("derived scenario %u generation %u: checked=%llu reused=%llu captured=%llu skipped=%llu rows=%llu bytes=%llu indexRows=%zu inputs=%zu\n",scenario,generation,
            (unsigned long long)work.checked,(unsigned long long)work.reused,(unsigned long long)work.captured,(unsigned long long)work.nodesSkipped,(unsigned long long)work.rowsApplied,(unsigned long long)work.bytes,indexRows,result->memo->derived->inputs.Count);
        xlsFrontendIrResultDestroy(previous);previous=result;
        /* The new owner remains valid after old result,parse and control die. */
        xlsFrontendIrResultDestroy(control);xl6ParseResultFree(&parsed);xl6DiagFree(&a);xl6DiagFree(&b);xrtStrBufFree(&source);
        CHECK(originalPasses(xlsFrontendIrResultModule(result)));
    }
    xlsFrontendIrResultDestroy(previous);xrtFree(path);return 1;
}
#endif
#endif
int main(void)
{
#ifdef XLS_NATIVE_IR_DERIVED_PHASES
    xrtMemStatsEnable(true);
    for(unsigned scenario=0;scenario<5;++scenario)if(!sequence(scenario))return 1;
    if(!allocationFailures())return 1;
    printf("analysis_ir_derived_test passed: %u checks; actual native passes,whole uncached module,edits and independent ownership\n",derivedChecks);
#else
    puts("analysis_ir_derived_test passed: original native cohort retained");
#endif
    return 0;
}
