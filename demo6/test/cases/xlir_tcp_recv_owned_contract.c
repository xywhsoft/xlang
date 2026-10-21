#define main existingTcpContractMain
#include "xlir_tcp_managed_contract.c"
#undef main
static void owned(const char* path)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));size_t seen=0;
    for(size_t i=0;i<module->callTargetCount;++i){
        XL6_IrCallTarget* target=&module->callTargets[i];
        if(!target->cName||strncmp(target->cName,"xl6TcpStreamRecvOwned",20))continue;
        XL6_IrCallTarget saved=*target;++seen;
        assert(xl6IrRuntimeDirectBridgeTargetMatches(module,target));
        if(saved.nativeContextAbi==XL6_IR_CALL_NATIVE_CONTEXT_CLASS_FIELD_V1){
            const XL6_IrClassField* field=xl6IrNativeClassContextField(module,target);assert(field);
            XL6_IrClassField* change=(XL6_IrClassField*)field;XL6_IrClassField original=*change;
            change->nativeHandlePolicyCName="WrongNetBytesPolicy";refused(module,NULL,11);*change=original;
        }else{
            assert(saved.kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY&&saved.resultPhysicalDestroySymbol);
            target->resultPhysicalDestroySymbol=0;refused(module,target,12);*target=saved;
        }
    }
    assert(seen>=4&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
}
int main(int argc,char** argv)
{
    assert(argc==3);owned(argv[1]);owned(argv[2]);check(argv[1]);check(argv[2]);
    assert(contexts>=4&&deadlines>=2&&refusals>100);
    printf("TCP owned receives: exact native owner, class field, Future, services and checked effects; %zu refusals, %zu zero-output guards\n",refusals,emissionGuards);
    return 0;
}
