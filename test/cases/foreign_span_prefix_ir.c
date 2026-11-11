#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static unsigned mutations;
static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t count=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_SPAN_PREFIX",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned prefixes=0;
    assert(sizeof(XL6_ForeignCallFacts)==88);
    for(size_t i=0;i<module->foreignParameterCount;++i){
        XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        if(!row->spanLimitIndexPlusOne)continue;
        ++prefixes;XL6_ForeignParameterContract saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
        row->spanLimitIndexPlusOne=0;refused(module);*row=saved;
        row->spanLimitIndexPlusOne=65;refused(module);*row=saved;
        row->spanLimitIndexPlusOne=row->parameter+1;refused(module);*row=saved;
        row->readOnly=false;refused(module);*row=saved;
        row->countAbi=0;refused(module);*row=saved;
        row->kind=XL6_FOREIGN_PARAMETER_SYNC_CALLBACK;refused(module);*row=saved;
        memset(row->elementCName,'x',sizeof(row->elementCName));refused(module);*row=saved;
        strcpy(row->elementCName,"uint32_t");refused(module);*row=saved;
        node->foreignCall.spanPrefixCount=0;refused(module);node->foreignCall=facts;
        node->foreignCall.inputMappingFacts&=~XL6_FOREIGN_INPUT_SPAN_PREFIXES_PRESENT;refused(module);node->foreignCall=facts;
        node->foreignCall.inputMappingFacts|=XL6_FOREIGN_INPUT_REGIONS_PRESENT;refused(module);node->foreignCall=facts;
        for(size_t t=0;t<module->callTargetCount;++t){
            XL6_IrCallTarget* target=&module->callTargets[t];if(target->declaration!=row->declaration)continue;
            size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC)) ? 1:0;
            size_t p=row->spanLimitIndexPlusOne-1+receiver;assert(p<target->parameterCount);
            XL6_IrCallParameterPhysicalAbi abi=target->parameterPhysicalAbis[p];
            target->parameterPhysicalAbis[p]=XL6_IR_CALL_PARAMETER_PHYSICAL_TEXT_VIEW_BORROW;refused(module);target->parameterPhysicalAbis[p]=abi;
            XL6_IrTypeId type=target->parameterTypes[p];target->parameterTypes[p]=0;refused(module);target->parameterTypes[p]=type;
        }
    }
    assert(prefixes>=8 && xl6IrVerifyModule(module,NULL));
    printf("Span prefixes: %u rows, %u mutations rejected before C/header bytes; facts %zu bytes, parameter row %zu bytes\n",
        prefixes,mutations,sizeof(XL6_ForeignCallFacts),sizeof(XL6_ForeignParameterContract));
    xl6CompileUnitFree(&unit);return 0;
}
