#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t rejected;
static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags={0}; XL6_Output out; size_t length;
    assert(!xl6IrVerifyModule(ir,&diags) && diags.count);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitC(&out,ir,&diags));
    char* text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    assert(xl6OutputOpenMemory(&out)); assert(!xl6IrCgenEmitHeader(&out,ir,"BAD_NULLABLE",&diags));
    text=xl6OutputTakeMemoryText(&out,&length); assert(!length); xrtFree(text);
    xl6DiagFree(&diags); ++rejected;
}
int main(int argc,char** argv)
{
    assert(argc==2); XL6_CompileUnit unit; xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags) && xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    size_t parameters=0,results=0;
    for(size_t i=0;i<unit.ir.callableBridgeParameterCount;++i) {
        XL6_IrCallableBridgeParameter* slot=&unit.ir.callableBridgeParameters[i],saved=*slot;
        if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_NONE)continue;
        ++parameters;
        slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_NONE; reject(&unit.ir); *slot=saved;
        slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_POINTER; if(slot->presenceAbi==saved.presenceAbi)slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_TAGGED;
        reject(&unit.ir); *slot=saved;
        slot->valueType=saved.type; reject(&unit.ir); *slot=saved;
        slot->valueType=XL6_IR_TYPE_INVALID; reject(&unit.ir); *slot=saved;
        slot->invokeAbi=XL6_IR_CALLBACK_INVOKE_INVALID; reject(&unit.ir); *slot=saved;
    }
    for(size_t i=0;i<unit.ir.callableBridgeResultCount;++i) {
        XL6_IrCallableBridgeResult* slot=&unit.ir.callableBridgeResults[i],saved=*slot;
        if(slot->presenceAbi==XL6_IR_CALLABLE_PRESENCE_NONE)continue;
        ++results;
        slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_NONE; reject(&unit.ir); *slot=saved;
        slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_POINTER; if(slot->presenceAbi==saved.presenceAbi)slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_TAGGED;
        reject(&unit.ir); *slot=saved;
        slot->valueType=saved.type; reject(&unit.ir); *slot=saved;
        slot->valueType=XL6_IR_TYPE_INVALID; reject(&unit.ir); *slot=saved;
        slot->destroyAbi=slot->destroyAbi==XL6_IR_ASYNC_FRAME_DESTROY_NONE?XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE:XL6_IR_ASYNC_FRAME_DESTROY_NONE;
        reject(&unit.ir); *slot=saved;
    }
    assert(parameters && results && rejected==(parameters+results)*5);
    assert(xl6IrVerifyModule(&unit.ir,&unit.parse.diags));
    printf("nullable parameters=%zu results=%zu mutations=%zu rejected before output\n",parameters,results,rejected);
    xl6CompileUnitFree(&unit); return 0;
}
