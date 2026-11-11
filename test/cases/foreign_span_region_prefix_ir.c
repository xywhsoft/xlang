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
        assert(!(header ? xl6IrCgenEmitHeader(&out,module,"BAD_PREFIX_REGION",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&count);assert(text && !count);
        xrtFree(text);xl6OutputDiscard(&out);
    }
    ++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1) && xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));unsigned regions=0;
    for(size_t i=0;i<module->foreignRegionCount;++i){
        XL6_ForeignRegionContract* row=&module->foreignRegions[i];XL6_ForeignRegionContract saved=*row;
        XL6_IrNode* node=&module->nodes[row->declaration-1];XL6_ForeignCallFacts facts=node->foreignCall;
        if(!xl6ForeignInputHasSpanPrefixes(facts.inputMappingFacts))continue;
        ++regions;uint32 receiver=node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
        row->parameter=receiver+1;refused(module);*row=saved;
        row->physicalIndex=receiver;refused(module);*row=saved;
        row->physicalIndex=receiver+1;refused(module);*row=saved;
        row->width.physicalIndex=receiver+1;refused(module);*row=saved;
        row->height.physicalIndex=receiver;refused(module);*row=saved;
        row->stride.parameter=receiver+1;refused(module);*row=saved;
        row->logicalType=0;refused(module);*row=saved;
        row->unitBytes=0;refused(module);*row=saved;
        node->foreignCall.spanPrefixCount=0;refused(module);node->foreignCall=facts;
        node->foreignCall.inputMappingFacts^=XL6_FOREIGN_INPUT_SPAN_PREFIXES_PRESENT;refused(module);node->foreignCall=facts;
        for(size_t p=0;p<module->foreignParameterCount;++p){
            XL6_ForeignParameterContract* span=&module->foreignParameters[p];
            if(span->declaration!=row->declaration || !span->spanLimitIndexPlusOne)continue;
            uint32 limit=span->spanLimitIndexPlusOne;span->spanLimitIndexPlusOne=0;refused(module);span->spanLimitIndexPlusOne=limit;
        }
    }
    assert(regions>=5 && xl6IrVerifyModule(module,NULL));
    printf("Prefix regions: %u rows, %u mutations rejected before C/header bytes\n",regions,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
