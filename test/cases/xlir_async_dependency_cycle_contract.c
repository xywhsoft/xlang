#include "xlir_continuation_owner_test.h"
int main(int argc,char** argv)
{
    assert(argc==4);XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    if(!xl6ParseFile("test/cases/xlir_async_dependency_cycle.xl",&parse)||!xl6StdlibExpandModule(parse.root,&parse.diags)||
        !xl6SemCheckModule(parse.root,&parse.diags,"dependency_cycle")||!xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags)||!xl6IrLowerModule(&ir,&parse.diags)){
        for(size_t i=0;i<parse.diags.count;++i)fprintf(stderr,"%s\n",parse.diags.items[i].message);
        return 1;
    }
    verify(&ir);assert(ir.asyncMachineCount==1&&ir.asyncMachines[0].suspensionCount>=3);
    XL6_IrAsyncMachine* machine=&ir.asyncMachines[0];
    machine->ownershipAbi=XL6_IR_ASYNC_OWNERSHIP_INVALID;reject(&ir,"producer/Watch ownership ABI");
    machine->ownershipAbi=XL6_IR_ASYNC_OWNERSHIP_STABLE_PRODUCER_WATCH_V1;
    machine->cancellationAbi=XL6_IR_ASYNC_CANCELLATION_AWAIT_ONCE;reject(&ir,"cancellation delivery transport");
    machine->cancellationAbi=XL6_IR_ASYNC_CANCELLATION_RESIDENT_AWAIT_ONCE_V2;verify(&ir);
    assert(xl6OutputOpenFile(&out,argv[1])&&xl6IrCgenEmitC(&out,&ir,&parse.diags));xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[2]));
    xl6OutputPrintf(&out,"#define DEPENDENCY_START xlir_async_start_%u\n",machine->targetSymbol);xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3]));xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);
    assert(mutations==2);puts("Async dependency IR: verified producer and resident cancellation, 2 C/header-gated mutations, original catch/finally resume CFG");
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);return 0;
}
