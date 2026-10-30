#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned rejected;
static void refuse(const XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module,NULL));
    for(unsigned header=0;header<2;++header){
        XL6_Output out;size_t size=SIZE_MAX;assert(xl6OutputOpenMemory(&out));
        assert(!(header?xl6IrCgenEmitHeader(&out,module,"BAD_TCC_PROVIDER",NULL):xl6IrCgenEmitC(&out,module,NULL)));
        char* text=xl6OutputTakeMemoryText(&out,&size);assert(!size);xrtFree(text);xl6OutputDiscard(&out);
    }
    ++rejected;
}
int main(int argc,char** argv)
{
    const char* names[]={"xl6_tcc_context_create","xl6_tcc_context_create_output","xl6_tcc_context_destroy",
        "xl6_tcc_context_run_args","xl6_tcc_context_output_file","xl6_tcc_handler_wrap",
        "xl6_tcc_handler_create","xl6_tcc_handler_destroy","xl6_tcc_handler_run_args","xl6_tcc_vfs_mount_text"};
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(argc==1||argc==2);
    assert(xl6CompileUnitLoadFile(&unit,argc==2?argv[1]:"test/cases/stdlib_tcc_runtime_contract.xl",1));
    assert(xl6IrVerifyModule(&unit.ir,NULL));
    assert(xl6IrLowerModule(&unit.ir,&unit.parse.diags));assert(xl6IrVerifyModule(&unit.ir,NULL));
    unsigned seen=0;bool forged=false;
    for(size_t i=0;i<unit.ir.callTargetCount;++i){
        XL6_IrCallTarget* target=&unit.ir.callTargets[i];unsigned bit=0;
        for(unsigned n=0;n<10;++n)if(target->cName&&!strcmp(target->cName,names[n]))bit=1u<<n;
        XL6_IrCallTarget original=*target;
        if(!bit){
            if(!forged&&target->kind==XL6_IR_CALL_TARGET_EXTERN_C&&target->implementationAbi==XL6_IR_CALL_IMPLEMENTATION_DEFAULT){
                target->implementationAbi=XL6_IR_CALL_IMPLEMENTATION_GENERATED_TCC_V1;refuse(&unit.ir);*target=original;forged=true;
            }
            continue;
        }
        seen|=bit;assert(target->implementationAbi==XL6_IR_CALL_IMPLEMENTATION_GENERATED_TCC_V1);
        assert(target->runtimeBridge==XL6_IR_RUNTIME_BRIDGE_NONE&&xl6IrRuntimeCallImplementationMatches(&unit.ir,target));
        target->implementationAbi=XL6_IR_CALL_IMPLEMENTATION_DEFAULT;refuse(&unit.ir);*target=original;
        target->cName="xl6_tcc_context_create_almost";refuse(&unit.ir);*target=original;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;target->errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_NONE;
        refuse(&unit.ir);*target=original;
        if(target->parameterCount){target->parameterCount=0;refuse(&unit.ir);*target=original;}
    }
    assert(seen==1023&&forged&&rejected>=35);assert(xl6IrVerifyModule(&unit.ir,NULL));
    xl6CompileUnitFree(&unit);
    printf("TCC provider: 10 exact implementations, %u rejected provider/name/error/arity mutations; C and header emission refuse before output\n",rejected);
    return 0;
}
