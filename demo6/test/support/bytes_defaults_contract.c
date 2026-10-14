#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t rejected;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; XL6_Output out; size_t length;
    int verified=xl6IrVerifyModule(ir,&diags);
    if(verified || !diags.count)fprintf(stderr,"mutant=%zu verified=%d diagnostics=%zu\n",rejected,verified,diags.count);
    assert(!verified && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,ir,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,ir,"BAD_BYTES_DEFAULT",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}
int main(int argc,char** argv)
{
    const unsigned char data[]={0,0x80,0xff,'A'};
    char* literal=xl6IrCanonicalCDataLiteral(data,sizeof(data));
    assert(literal && !strcmp(literal,"\"\\000\\200\\377A\"")); xrtFree(literal);
    assert(!xl6IrCanonicalCDataLiteral(NULL,1) && !xl6IrCanonicalCDataLiteral(data,SIZE_MAX));
    literal=xl6IrCanonicalCDataLiteral(NULL,0); assert(literal && !strcmp(literal,"\"\"")); xrtFree(literal);
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags) && xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t defaults=0;
    for(size_t i=0;i<unit.ir.dynamicMethodParameterCount;++i) {
        XL6_IrDynamicMethodParameter* row=&unit.ir.dynamicMethodParameters[i];
        if(row->defaultAbi!=XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES)continue;
        ++defaults; char* saved=row->defaultLiteral;
        row->defaultLiteral="\"different\""; reject(&unit.ir); row->defaultLiteral=saved;
        XL6_IrDynamicMethodDefaultAbi abi=row->defaultAbi;
        row->defaultAbi=XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING; reject(&unit.ir); row->defaultAbi=abi;
        const XL6_IrNode* expression=xl6IrNodeGet(&unit.ir,row->defaultExpression); assert(expression && expression->literalDecoded);
    }
    for(size_t i=0;i<unit.ir.closureTargetCount;++i) {
        XL6_IrClosureTarget* row=&unit.ir.closureTargets[i];
        for(size_t p=0;p<row->callParameterCount;++p) {
            if(row->callParameterDefaultAbis[p]!=XL6_IR_DYNAMIC_METHOD_DEFAULT_BYTES)continue;
            ++defaults; char* saved=row->callParameterDefaultLiterals[p];
            row->callParameterDefaultLiterals[p]="\"different\""; reject(&unit.ir); row->callParameterDefaultLiterals[p]=saved;
            XL6_IrDynamicMethodDefaultAbi abi=row->callParameterDefaultAbis[p];
            row->callParameterDefaultAbis[p]=XL6_IR_DYNAMIC_METHOD_DEFAULT_STRING; reject(&unit.ir); row->callParameterDefaultAbis[p]=abi;
        }
    }
    assert(defaults && rejected==defaults*2);
    printf("bytes defaults=%zu mutations=%zu rejected before output\n",defaults,rejected);
    xl6CompileUnitFree(&unit);return 0;
}
