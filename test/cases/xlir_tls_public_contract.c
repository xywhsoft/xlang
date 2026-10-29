#include "src/compile/xl6_compile_unit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Public built-in names must resolve to the same closed physical contracts
 * as user-declared native classes, including unused overload declarations. */
int main(int argc,char** argv)
{
    assert(argc==2);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));
    size_t targets=0,refusals=0,services=0,contexts=0,policies=0;
    XL6_IrTypeId integer=0,boolean=0;
    for(size_t i=0;i<module->types.count;i++){
        const XL6_IrType* type=&module->types.items[i];
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_BOOL)boolean=(XL6_IrTypeId)i+1;
        if(type->kind==XL6_IR_TYPE_BUILTIN&&type->cAbi==XL6_IR_C_ABI_I64&&
            type->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64)integer=(XL6_IrTypeId)i+1;
    }
    assert(integer&&boolean);
    for(size_t i=0;i<module->callTargetCount;i++){
        XL6_IrCallTarget* target=&module->callTargets[i];const char* name=target->cName;
        if(!name)continue;
        assert(strncmp(name,"xl6_xrt_tls_",12)!=0);
        if(strstr(name,"Options"))continue;
        if(strncmp(name,"xl6TlsStream",12)&&strncmp(name,"xl6TlsListener",14)&&
            strncmp(name,"xl6TlsDial",10)&&strncmp(name,"xl6TlsProxyDial",15))continue;
        targets++;assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        XL6_IrCallTarget saved=*target;
#define REFUSE(Field,Value) do { target->Field=(Value); \
    assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));refusals++;*target=saved; } while(0)
        REFUSE(effectAbi,saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE);
        REFUSE(parameterCount,saved.parameterCount+1);
        REFUSE(returnType,saved.returnType==integer?boolean:integer);
        if(saved.nativeServicesRequired){services++;REFUSE(nativeServicesRequired,0);}
        if(saved.nativeContextAbi!=XL6_IR_CALL_NATIVE_CONTEXT_NONE){contexts++;REFUSE(nativeContextAbi,XL6_IR_CALL_NATIVE_CONTEXT_NONE);}
        if(saved.nativeHandlePolicyCName){
            policies++;target->nativeHandlePolicyCName="notATlsPolicy";
            assert(!xl6IrVerifyModule(module,NULL));refusals++;*target=saved;
        }
        for(size_t j=0;j<saved.parameterCount;j++){
            XL6_IrTypeId type=target->parameterTypes[j];
            target->parameterTypes[j]=type==integer?boolean:integer;
            assert(!xl6IrRuntimeDirectBridgeTargetMatches(module,target));refusals++;target->parameterTypes[j]=type;
        }
#undef REFUSE
    }
    assert(targets==167&&policies==2&&services>60&&contexts>30&&refusals>900);
    assert(xl6IrVerifyModule(module,NULL));
    printf("public TLS XLIR: %zu targets; %zu services; %zu contexts; %zu policies; %zu signature refusals\n",
        targets,services,contexts,policies,refusals);
    xl6CompileUnitFree(&unit);return 0;
}
