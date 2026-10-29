/* Same complete verifier/emission rejection machinery, now with the full
 * nominal aggregate Server surface as well as existing bytes/int Futures. */
#define main existingTcpContractMain
#include "xlir_tcp_managed_contract.c"
#undef main
int main(int argc,char** argv)
{
    assert(argc==3);
    XL6_CompileUnit unit;xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit,argv[1],1)&&xl6IrLowerModule(&unit.ir,&unit.parse.diags));
    size_t serverTargets=0;
    for(size_t i=0;i<unit.ir.callTargetCount;++i){const XL6_IrCallTarget* target=&unit.ir.callTargets[i];
        if(target->cName&&!strncmp(target->cName,"xl6TcpServer",12))++serverTargets;}
    assert(serverTargets>=35&&xl6IrVerifyModule(&unit.ir,NULL));xl6CompileUnitFree(&unit);
    check(argv[1]);check(argv[2]);
    assert(targets>50&&refusals>200&&contexts>=2&&policies>=4&&byteFutures>=1&&deadlines&&intFutures);
    printf("TCP XLIR: Server targets=%zu total=%zu descriptor refusals=%zu, %zu violation kinds guarded before C/header output\n",
        serverTargets,targets,refusals,emissionGuards);return 0;
}
