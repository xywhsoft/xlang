#define main existingTcpContractMain
#include "xlir_tcp_managed_contract.c"
#undef main
static void chain(const char* path)
{
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,path,1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    XL6_IrModule* module=&unit.ir;assert(xl6IrVerifyModule(module,NULL));size_t seen=0;
    for(size_t i=0;i<module->callTargetCount;++i){XL6_IrCallTarget* target=&module->callTargets[i];
        if(!target->cName||strcmp(target->cName,"xl6NetBufferAppendRefV1"))continue;
        XL6_IrCallTarget original=*target;++seen;
        const XL6_IrClassField* field=xl6IrResidentNativeResourceField(module,target->parameterTypes[1],"xl6NetBytesDestroyV1");assert(field);
        XL6_IrClassField* change=(XL6_IrClassField*)field;XL6_IrClassField saved=*change;
        change->nativeHandlePolicyCName="wrongChainBytesPolicy";refused(module,NULL,11);*change=saved;
        target->errorAbi=XL6_IR_ERROR_ABI_NO_RAISE;refused(module,target,12);*target=original;
        target->executionBoundaryAbi=XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK;refused(module,target,13);*target=original;
    }
    assert(seen&&xl6IrVerifyModule(module,NULL));xl6CompileUnitFree(&unit);
}
int main(int argc,char** argv)
{
    assert(argc==3);chain(argv[1]);check(argv[1]);check(argv[2]);
    assert(refusals>100&&policies>=2);
    printf("TCP chain take: exact NetBuffer/NetBytes owners and nominal status, foreign checked nonblocking ABI; %zu refusals, %zu zero-output guards\n",refusals,emissionGuards);return 0;
}
