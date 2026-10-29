#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
static size_t mutations;
static void reject(XL6_IrModule* module)
{
    XL6_DiagList diagnostics={0}; assert(!xl6IrVerifyModule(module,&diagnostics) && diagnostics.count);
    for(unsigned header=0;header<2;++header) {
        XL6_Output output;size_t length;assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"INVALID_SCALAR",&diagnostics) :
            xl6IrCgenEmitC(&output,module,&diagnostics)));
        char* text=xl6OutputTakeMemoryText(&output,&length);assert(!length);xrtFree(text);
    }
    xl6DiagFree(&diagnostics);++mutations;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    if(!xl6CompileUnitLoadFile(&unit,argv[1],1) || !xl6IrLowerModule(&unit.ir,&unit.parse.diags)) {
        xl6DiagPrintText(&unit.parse.diags,argv[1]);return 1;
    }
    size_t scalars=0,nullable=0,native=0,bound=0;
    for(size_t i=0;i<unit.ir.dynamicMethodScalarCodecCount;++i) {
        XL6_IrCallableBridgeResult* slot=&unit.ir.dynamicMethodScalarCodecs[i];
        if(slot->decodeAbi==XL6_IR_CALLABLE_BRIDGE_INVALID)continue;
        ++scalars;XL6_IrCallableBridgeResult saved=*slot;
        slot->decodeAbi=XL6_IR_CALLABLE_BRIDGE_INVALID;reject(&unit.ir);*slot=saved;
        slot->encodeAbi=XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN;reject(&unit.ir);*slot=saved;
        slot->valueType=0;reject(&unit.ir);*slot=saved;
        slot->destroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE;reject(&unit.ir);*slot=saved;
        if(slot->presenceAbi!=XL6_IR_CALLABLE_PRESENCE_NONE) {
            ++nullable;slot->presenceAbi=XL6_IR_CALLABLE_PRESENCE_NONE;reject(&unit.ir);*slot=saved;
        }
        if(slot->nativeScalarProfile!=XL6_NATIVE_SCALAR_NONE) {
            ++native;slot->nativeScalarProfile=XL6_NATIVE_SCALAR_NONE;reject(&unit.ir);*slot=saved;
        }
        if(slot->integerDomain.rule!=XL6_VALUE_ADMIT_NONE) {
            slot->integerDomain.identity=0;reject(&unit.ir);*slot=saved;
        }
    }
    for(size_t i=0;i<unit.ir.dynamicMethodParameterCount;++i) {
        XL6_IrDynamicMethodParameter* parameter=&unit.ir.dynamicMethodParameters[i];
        if(!parameter->scalarCodec)continue;
        size_t codec=parameter->scalarCodec;parameter->scalarCodec=0;reject(&unit.ir);parameter->scalarCodec=codec;
        XL6_IrDynamicMethodMatchAbi match=parameter->matchAbi;
        parameter->matchAbi=XL6_IR_DYNAMIC_METHOD_MATCH_ANY_XVALUE;reject(&unit.ir);parameter->matchAbi=match;
    }
    for(size_t i=0;i<unit.ir.dynamicMethodCount;++i) {
        XL6_IrDynamicMethod* method=&unit.ir.dynamicMethods[i];
        if(!method->boundSignatureType)continue;
        ++bound;XL6_IrTypeId type=method->boundSignatureType;method->boundSignatureType=0;reject(&unit.ir);method->boundSignatureType=type;
        if(method->scalarResultCodecCount) {
            size_t count=method->scalarResultCodecCount;method->scalarResultCodecCount=0;reject(&unit.ir);method->scalarResultCodecCount=count;
        }
    }
    assert(scalars && bound && mutations);
    printf("method scalar slots=%zu nullable=%zu native=%zu bound=%zu mutations=%zu rejected before output\n",scalars,nullable,native,bound,mutations);
    xl6CompileUnitFree(&unit);return 0;
}
