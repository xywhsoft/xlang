#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>

#ifdef XLS_NATIVE_IR_PROVIDER_FIELDS
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_current_provider_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int provider(void)
{
    const char* names[]={"xl6_tcc_context_create","xl6_tcc_context_create_output","xl6_tcc_context_destroy",
        "xl6_tcc_context_run_args","xl6_tcc_context_output_file","xl6_tcc_handler_wrap","xl6_tcc_handler_create",
        "xl6_tcc_handler_destroy","xl6_tcc_handler_run_args","xl6_tcc_vfs_mount_text"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)CHECK(xl6IrRuntimeCallImplementation(names[i])==XL6_IR_CALL_IMPLEMENTATION_GENERATED_TCC_V1);
    CHECK(xl6IrRuntimeCallImplementation(NULL)==XL6_IR_CALL_IMPLEMENTATION_DEFAULT);
    CHECK(xl6IrRuntimeCallImplementation("xl6_xlang_context_create")==XL6_IR_CALL_IMPLEMENTATION_DEFAULT);
    CHECK(xl6IrRuntimeCallImplementation("xl6_tcc_vfs_mount_text_bad")==XL6_IR_CALL_IMPLEMENTATION_DEFAULT);
    CHECK(!xl6IrRuntimeCallImplementationMatches(NULL,NULL));
    XL6_IrType types[]={
        {.kind=XL6_IR_TYPE_BUILTIN,.cAbi=XL6_IR_C_ABI_MANAGED_STRING,.ownershipAbi=XL6_IR_OWNERSHIP_ABI_MANAGED,.nullAbi=XL6_IR_NULL_ABI_POINTER_SENTINEL},
        {.kind=XL6_IR_TYPE_BUILTIN,.cAbi=XL6_IR_C_ABI_C_INT,.ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL,.nullAbi=XL6_IR_NULL_ABI_NONE}};
    XL6_IrTypeId parameters[2]={1,1};
    XL6_IrParameterAbi transfer[2]={XL6_IR_PARAMETER_ABI_BORROW,XL6_IR_PARAMETER_ABI_BORROW};
    XL6_IrCallParameterPhysicalAbi physical[2]={XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT};
    XL6_IrCallParameterConstraintAbi constraints[2]={XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE,XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE};
    XL6_IrNodeId fields[2]={0};
    XL6_IrCallTarget target={.kind=XL6_IR_CALL_TARGET_EXTERN_C,.implementationAbi=XL6_IR_CALL_IMPLEMENTATION_GENERATED_TCC_V1,
        .cName="xl6_tcc_vfs_mount_text",.returnType=2,.resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_DIRECT,
        .errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE,.parameterCount=2,.parameterTypes=parameters,.parameterAbis=transfer,
        .parameterPhysicalAbis=physical,.parameterConstraintAbis=constraints,.parameterPhysicalFieldDeclarations=fields};
    XL6_IrModule module={.types={.items=types,.count=2,.capacity=2},.callTargets=&target,.callTargetCount=1,.callTargetCapacity=1};
    CHECK(xl6IrRuntimeCallImplementationMatches(&module,&target));
    target.errorPhysicalAbi=XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE;
    CHECK(xl6IrRuntimeCallImplementationMatches(&module,&target));
    XL6_IrCallTarget original=target;XL6_IrType string=types[0];
    for(unsigned mode=0;mode<25;++mode){
        target=original;types[0]=string;parameters[0]=1;transfer[0]=XL6_IR_PARAMETER_ABI_BORROW;
        physical[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;constraints[0]=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE;fields[0]=0;
        switch(mode){
        case 0:target.implementationAbi=XL6_IR_CALL_IMPLEMENTATION_DEFAULT;break;
        case 1:target.runtimeBridge=(XL6_IrRuntimeBridgeKind)1;break;
        case 2:target.runtimeBridgeOperation=(XL6_IrRuntimeBridgeOperation)1;break;
        case 3:target.parameterCount=1;break;
        case 4:target.variadicAbi=XL6_IR_CALL_VARIADIC_C;break;
        case 5:target.errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;break;
        case 6:target.errorPhysicalAbi=(XL6_IrCallErrorPhysicalAbi)999;break;
        case 7:target.kind=XL6_IR_CALL_TARGET_NATIVE_FACTORY;break;
        case 8:types[0].ownershipAbi=XL6_IR_OWNERSHIP_ABI_TRIVIAL;break;
        case 9:types[0].nullAbi=XL6_IR_NULL_ABI_NONE;break;
        case 10:transfer[0]=XL6_IR_PARAMETER_ABI_VALUE;break;
        case 11:physical[0]=(XL6_IrCallParameterPhysicalAbi)999;break;
        case 12:constraints[0]=(XL6_IrCallParameterConstraintAbi)999;break;
        case 13:fields[0]=1;break;
        case 14:parameters[0]=0;break;
        case 15:target.returnType=0;break;
        case 16:target.parameterTypes=NULL;break;
        case 17:target.parameterAbis=NULL;break;
        case 18:target.parameterPhysicalAbis=NULL;break;
        case 19:target.parameterConstraintAbis=NULL;break;
        case 20:target.parameterPhysicalFieldDeclarations=NULL;break;
        case 21:target.resultPhysicalFieldDeclaration=1;break;
        case 22:target.resultPhysicalDestroySymbol=1;break;
        case 23:target.resultPhysicalAbi=XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE;break;
        case 24:target.cName="xl6_tcc_vfs_mount_text_bad";break;
        }
        CHECK(!xl6IrRuntimeCallImplementationMatches(&module,&target));
    }
    target=original;types[0]=string;parameters[0]=1;transfer[0]=XL6_IR_PARAMETER_ABI_BORROW;
    physical[0]=XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;constraints[0]=XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE;fields[0]=0;
    CHECK(xl6IrRuntimeCallImplementationMatches(&module,&target));
    return 1;
}
static int lowered(void)
{
    const char* body="TccContext ctx = tcc.Context()\n";char* path=xrtPathAbs("provider-lower-memory.xl");CHECK(path);
    xls_semantic_input input={path,body,strlen(body)};char error[256];
    xls_semantic_check* checked=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    CHECK(checked && xlsSemanticCheckInfo(checked).accepted);
    XL6_IrModule module;xl6IrModuleInit(&module);XL6_DiagList diag;xl6DiagInit(&diag);
    CHECK(xl6IrBuildModuleFromAst(&module,xlsSemanticCheckAst(checked),&diag) && xl6IrLowerModule(&module,&diag));
    CHECK(!diag.count && xl6IrVerifyModule(&module,NULL));size_t generated=0;
    for(size_t i=0;i<module.callTargetCount;++i){XL6_IrCallTarget* target=&module.callTargets[i];
        if(target->implementationAbi!=XL6_IR_CALL_IMPLEMENTATION_GENERATED_TCC_V1)continue;
        ++generated;target->implementationAbi=XL6_IR_CALL_IMPLEMENTATION_DEFAULT;
        CHECK(!xl6IrVerifyModule(&module,NULL));
        target->implementationAbi=XL6_IR_CALL_IMPLEMENTATION_GENERATED_TCC_V1;
        CHECK(xl6IrVerifyModule(&module,NULL));
    }
    CHECK(generated);
    XL6_Output out;CHECK(xl6OutputOpenMemory(&out));xl6IrDumpModuleJson(&out,&module);
    size_t size=0;char* text=xl6OutputTakeMemoryText(&out,&size);xl6OutputDiscard(&out);CHECK(text && size);
    xvalue* json=xrtJsonParse((xstrview){text,size});xrtFree(text);CHECK(json);
    const xvalue* targets=xrtValueObjectGet(json,(xstrview){"callTargets",11});CHECK(xrtValueCount(targets)==module.callTargetCount);
    for(size_t i=0;i<module.callTargetCount;++i){const xvalue* row=xrtValueArrayGet(targets,i);int64 marker=-1;
        CHECK(row && xrtValueGetInt(xrtValueObjectGet(row,(xstrview){"implementationAbi",17}),&marker));
        CHECK(marker==module.callTargets[i].implementationAbi);
    }
    xrtValueRelease(json);xl6IrModuleFree(&module);xl6DiagFree(&diag);xlsSemanticCheckDestroy(checked);xrtFree(path);return 1;
}
static int logDefault(void)
{
    const char* body="auto sink = log.customSink(func(LogRecord record) as LogResult\n return LogResult.written\nend)\n";
    char* path=xrtPathAbs("provider-log-memory.xl");CHECK(path);
    xls_semantic_input input={path,body,strlen(body)};char error[256];
    xls_semantic_check* result=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,error,sizeof(error));
    if(!result)fprintf(stderr,"provider log: %s\n",error);
    CHECK(result);xls_check_info info=xlsSemanticCheckInfo(result);
    if(!info.accepted)for(size_t i=0;i<info.diagnostics;++i){xls_check_diagnostic d;if(xlsSemanticCheckDiagnostic(result,i,&d))fprintf(stderr,"provider log: %s\n",d.message);}
    CHECK(info.accepted && !info.diagnostics && info.irNodes);xlsSemanticCheckDestroy(result);xrtFree(path);return 1;
}
int main(void)
{
    if(!provider() || !lowered() || !logDefault())return 1;
    printf("analysis_current_provider_test passed: %u checks; original provider classifier/signature rejection/JSON marker and real customSink nullable default\n",checks);return 0;
}
#else
int main(void){puts("analysis_current_provider_test passed: earlier cohort has no implementation-provider field");return 0;}
#endif
