#define main existingTcpContractMain
#include "xlir_tcp_managed_contract.c"
#undef main
static void owned(const char* path)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    assert(xl6IrVerifyModule(&unit.ir,NULL));size_t seen=0;
    for(size_t i=0;i<unit.ir.callTargetCount;++i){XL6_IrCallTarget* target=&unit.ir.callTargets[i];
        if(!target->cName||strncmp(target->cName,"xl6NetBytes",11))continue;
        XL6_IrCallTarget saved=*target;assert(xl6IrRuntimeDirectBridgeTargetMatches(&unit.ir,target));++seen;
        target->errorAbi=saved.errorAbi==XL6_IR_ERROR_ABI_NO_RAISE?XL6_IR_ERROR_ABI_MAY_RAISE:XL6_IR_ERROR_ABI_NO_RAISE;
        refused(&unit.ir,target,11);*target=saved;
        target->effectAbi=saved.effectAbi==XL6_IR_CALL_EFFECT_PURE?XL6_IR_CALL_EFFECT_CONSERVATIVE:XL6_IR_CALL_EFFECT_PURE;
        refused(&unit.ir,target,12);*target=saved;
        if(saved.kind==XL6_IR_CALL_TARGET_NATIVE_FACTORY){
            target->resultPhysicalDestroySymbol=0;refused(&unit.ir,target,13);*target=saved;
        }
    }
    assert(seen>=9&&xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
}
int main(int argc,char** argv)
{
    assert(argc==3);owned(argv[1]);check(argv[1]);check(argv[2]);
    assert(targets>30&&byteFutures&&contexts&&refusals>100);
    printf("TCP owned sends: exact NetBytes owners, class fields, status enum, nonblocking checked effects; %zu descriptor refusals, %zu zero-output guards\n",refusals,emissionGuards);
    return 0;
}
