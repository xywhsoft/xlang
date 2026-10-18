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
        assert(!(header ? xl6IrCgenEmitHeader(&output,module,"BAD_CHANNEL_CASE",&errors) :
            xl6IrCgenEmitC(&output,module,&errors)));
        char* text=xl6OutputTakeMemoryText(&output,&size);assert(!size);
        xrtFree(text);xl6OutputDiscard(&output);xl6DiagFree(&errors);
    }
    ++refusals;
}
int main(int argc,char** argv)
{
    assert(argc==2);XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    size_t channels=0,arrays=0,slots=0;unsigned roles=0,arrayCoverage=0;
    static const char* const coreArrays[]={"xl6ChannelCasesWaitLanguageV1","xl6ChannelCasesTryLanguageV1",
        "xl6ChannelCasesForLanguageV1","xl6ChannelCasesUntilLanguageV1","xl6ChannelCasesWaitCancelLanguageV1",
        "xl6ChannelCasesForCancelLanguageV1","xl6ChannelCasesUntilCancelLanguageV1"};
    for(size_t i=0;i<module->callTargetCount;++i) {
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(target->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_CHANNEL_SCHEMA_V1 &&
            target->nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1)continue;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        XL6_IrCallTarget saved=*target;
        target->nativeContextAbi=(XL6_IrCallNativeContextAbi)999;refuse(module);*target=saved;
        target->nativeContextType=XL6_IR_TYPE_INVALID;refuse(module);*target=saved;
        target->nativeContextPayloadAbi=XL6_IR_ASYNC_RESULT_VOID;refuse(module);*target=saved;
        target->nativeResultAbi=(XL6_IrCallNativeResultAbi)999;refuse(module);*target=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refuse(module);*target=saved;
        target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;refuse(module);*target=saved;
        target->cName="xl6ChannelCasesUnknownV1";refuse(module);*target=saved;
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_ARRAY_V1) {
            ++arrays;assert(xl6IrNativeClassArrayContextField(module,target));
            for(size_t entry=0;entry<sizeof(coreArrays)/sizeof(*coreArrays);++entry)
                if(!strcmp(target->cName,coreArrays[entry]))arrayCoverage|=1u<<entry;
            XL6_IrTypeId savedType=target->parameterTypes[0];
            target->parameterTypes[0]=target->returnType;refuse(module);target->parameterTypes[0]=savedType;
        } else {
            ++channels;roles|=1u<<xl6NativeGenericRole(target->cName);
            assert(xl6IrChannelConstructionRecipe(module,target->nativeContextType,NULL,NULL));
            if(saved.nativeResultAbi==XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1) {
                ++slots;
                assert(target->returnType==xl6IrNativeContextElementType(module,target));
                target->nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_NONE;refuse(module);*target=saved;
                target->returnType=target->nativeContextType;refuse(module);*target=saved;
            }
        }
        if(saved.resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE) {
            target->resultPhysicalFieldDeclaration=XL6_IR_NODE_INVALID;refuse(module);*target=saved;
        }
    }
    const unsigned requiredRoles=(
        (1u<<XL6_NATIVE_GENERIC_CHANNEL_CASE_RECEIVE)|(1u<<XL6_NATIVE_GENERIC_CHANNEL_CASE_SEND)|
        (1u<<XL6_NATIVE_GENERIC_CHANNEL_SELECTION_VALUE));
    const unsigned permittedRoles=requiredRoles|(1u<<XL6_NATIVE_GENERIC_CHANNEL_SEND_CREATE)|
        (1u<<XL6_NATIVE_GENERIC_CHANNEL_RECEIVE_CREATE);
    /* New canonical async builders must not invalidate coverage of the seven
     * existing selector entries, or make a missing core entry go unnoticed. */
    assert(channels>=3 && arrays>=7 && arrayCoverage==0x7fu && slots &&
        (roles&requiredRoles)==requiredRoles && !(roles&~permittedRoles));
    /* Even an ordinary internal target must not acquire a private native
     * output convention by mutating one otherwise unused metadata field. */
    bool ordinary=false;
    for(size_t i=0;i<module->callTargetCount;++i) {
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(target->kind==XL6_IR_CALL_TARGET_INTERNAL && !target->nativeContextAbi) {
            assert(!target->nativeServicesRequired);
            target->nativeServicesRequired=XL6_IR_CALL_NATIVE_SERVICE_EXECUTOR_V1;refuse(module);
            target->nativeServicesRequired=0;
            target->nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_VALUE_SLOT_V1;refuse(module);
            target->nativeResultAbi=XL6_IR_CALL_NATIVE_RESULT_NONE;ordinary=true;break;
        }
    }
    assert(ordinary && xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
    printf("Channel cases: %zu schema targets, %zu class arrays, %zu private results, %zu pre-emission refusals\n",
        channels,arrays,slots,refusals);
    return 0;
}
