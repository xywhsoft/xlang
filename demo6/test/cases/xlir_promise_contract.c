#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/abi/xl6_native_generic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static size_t refusals;
static void refuse(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(int header=0;header<2;++header) {
        XL6_Output output;XL6_DiagList errors;size_t size=SIZE_MAX;
        xl6DiagInit(&errors);assert(xl6OutputOpenMemory(&output));
        assert(!(header?xl6IrCgenEmitHeader(&output,module,"BAD_PROMISE",&errors):
            xl6IrCgenEmitC(&output,module,&errors)));
        char* value=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(value);xl6OutputDiscard(&output);xl6DiagFree(&errors);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==4);size_t targets=0,contexts=0,addresses=0;unsigned roles=0;
    for(int input=1;input<argc;++input) {
        XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
        assert(xl6CompileUnitLoadFile(&unit,argv[input],1));
        assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
        XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
        for(size_t i=0;i<module->callTargetCount;++i) {
            XL6_IrCallTarget* target=&module->callTargets[i];
            XL6_NativeGenericRole role=xl6NativeGenericRole(target->cName);
            if(!role)continue;
            roles|=1u<<role;++targets;XL6_IrCallTarget saved=*target;
            assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
            assert(xl6ArtifactRuntimeSymbolOwner(target->cName)==XL6_ARTIFACT_PROMISE);
            target->nativeContextType=XL6_IR_TYPE_INVALID;target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_FUTURE_VALUE_V1;
            refuse(module);*target=saved;
            target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refuse(module);*target=saved;
            target->cName="xl6PromiseResolveV1Almost";refuse(module);*target=saved;
            if(saved.nativeContextAbi) {
                ++contexts;target->nativeContextAbi=XL6_IR_CALL_NATIVE_CONTEXT_NONE;refuse(module);*target=saved;
                target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_INVALID;refuse(module);*target=saved;
            }
            if(role==XL6_NATIVE_GENERIC_RESOLVE) {
                ++addresses;XL6_IrCallParameterPhysicalAbi physical=target->parameterPhysicalAbis[1];
                target->parameterPhysicalAbis[1]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
                refuse(module);target->parameterPhysicalAbis[1]=physical;
            }
        }
        for(size_t i=0;i<module->genericSpecializationCount;++i) {
            XL6_IrGenericSpecialization* row=&module->genericSpecializations[i];
            XL6_SymbolId saved=row->symbol;row->symbol=0;refuse(module);row->symbol=saved;
        }
        assert(xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    }
    unsigned expected=0;
#define XL6_NATIVE_GENERIC_BRIDGE(Name,Owner,Role) if(!strcmp(Owner,"xl6PromiseDestroyV1"))expected|=1u<<XL6_NATIVE_GENERIC_##Role;
#include "src/abi/xl6_native_generic_contracts.inc"
#undef XL6_NATIVE_GENERIC_BRIDGE
    assert(roles==expected && contexts && addresses);
    printf("Promise: %zu exact targets, %zu contexts, %zu slot addresses, %zu IR refusals\n",targets,contexts,addresses,refusals);
    return 0;
}
