#include "xlir_continuation_owner_test.h"
#include "xlir_activation_capture_assert.h"
static void unused_generator(void)
{
    XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char* text;
    xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    int built=xl6ParseFile("test/cases/xlir_generator_unreachable_yield.xl",&parse)&&
        xl6StdlibExpandModule(parse.root,&parse.diags)&&xl6SemCheckModule(parse.root,&parse.diags,"unreachable_yield")&&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags)&&xl6IrLowerModule(&ir,&parse.diags);
    if(!built)xl6DiagPrintText(&parse.diags,"unused generator fixture");
    assert(built);verify(&ir);assert(ir.generatorMachineCount==1);
    const XL6_IrGeneratorMachine* machine=&ir.generatorMachines[0];
    assert(ir.functions[machine->function].reachability==XL6_IR_REACHABILITY_UNREACHABLE&&machine->suspensionCount==3);
    size_t yields=0;for(size_t i=0;i<ir.instructionCount;++i)yields+=ir.instructions[i].kind==XL6_IR_INSTR_YIELD;assert(yields==4);
    assert(xl6OutputOpenMemory(&out)&&xl6IrCgenEmitC(&out,&ir,&parse.diags));text=xl6OutputTakeMemoryText(&out,NULL);
    assert(text&&!strstr(text,"xlir_generator_start_raw_"));xrtFree(text);xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
}
int main(int argc,char** argv)
{
    XL6_ParseResult parse;XL6_IrModule ir;XL6_Output out;char* text;
    assert(argc==4);unused_generator();xl6ParseResultInit(&parse);xl6IrModuleInit(&ir);
    int built=xl6ParseFile("test/cases/xlir_generator_managed.xl",&parse)&&
        xl6StdlibExpandModule(parse.root,&parse.diags)&&xl6SemCheckModule(parse.root,&parse.diags,"generator_managed")&&
        xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags)&&xl6IrLowerModule(&ir,&parse.diags);
    if(!built)xl6DiagPrintText(&parse.diags,"managed generator fixture");
    assert(built);
    verify(&ir);assert(ir.abi.generatorStorageAbi==XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V3&&ir.generatorMachineCount==1);
    const XL6_IrGeneratorMachine* machine=&ir.generatorMachines[0];
    assert(machine->yieldAbi==XL6_IR_GENERATOR_YIELD_MOVE_XVALUE&&machine->frameSlotCount&&machine->destroyActionCount);
    size_t deadYield=SIZE_MAX,deadBlock=SIZE_MAX,totalYields=0;
    const XL6_IrFunction* function=&ir.functions[machine->function];
    for(size_t b=0;b<function->blockCount;++b){const XL6_IrBlock* block=&ir.blocks[function->firstBlock+b];
        for(size_t i=block->firstInstruction;i<block->firstInstruction+block->instructionCount;++i){
            if(ir.instructions[i].kind!=XL6_IR_INSTR_YIELD)continue;
            ++totalYields;bool present=false;
            for(size_t s=0;s<machine->suspensionCount;++s)present|=ir.generatorSuspensions[machine->firstSuspension+s].yieldInstruction==i;
            if(!present){deadYield=i;deadBlock=function->firstBlock+b;}
        }
    }
    assert(totalYields==4&&machine->suspensionCount==3&&deadYield!=SIZE_MAX&&deadBlock!=SIZE_MAX);
    XL6_IrGeneratorMachineBlock savedBlock=ir.generatorMachineBlocks[machine->firstBlock];
    ir.generatorMachineBlocks[machine->firstBlock].block=(XL6_IrBlockId)(deadBlock+1);
    ir.generatorMachineBlocks[machine->firstBlock].firstInstruction=ir.blocks[deadBlock].firstInstruction;
    reject(&ir,NULL);ir.generatorMachineBlocks[machine->firstBlock]=savedBlock;
    XL6_IrGeneratorSuspension savedState=ir.generatorSuspensions[machine->firstSuspension];
    ir.generatorSuspensions[machine->firstSuspension].yieldInstruction=deadYield;
    ir.generatorSuspensions[machine->firstSuspension].block=(XL6_IrBlockId)(deadBlock+1);
    ir.generatorSuspensions[machine->firstSuspension].itemValue=ir.operands[ir.instructions[deadYield].firstOperand];
    reject(&ir,NULL);ir.generatorSuspensions[machine->firstSuspension]=savedState;
    ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_SCOPED_FRAME_V1;reject(&ir,"generator storage ABI");
    ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V3;
    assert(xl6OutputOpenMemory(&out)&&xl6IrCgenEmitC(&out,&ir,&parse.diags));text=xl6OutputTakeMemoryText(&out,NULL);
    assert_activation_layout(text,"xlir_generator",machine->targetSymbol,ir.generatorFrameSlots+machine->firstFrameSlot,
        machine->frameSlotCount,machine->firstFrameSlot,ir.generatorFrameActions+machine->firstDestroyAction,machine->destroyActionCount);
    char expected[256];snprintf(expected,sizeof(expected),"xlir_generator_item_slot_%u = {{XL6_CAPTURE_XVALUE, 0, sizeof(xvalue*)}, NULL}",machine->targetSymbol);
    assert(strstr(text,expected));xrtFree(text);
    assert(xl6OutputOpenFile(&out,argv[1])&&xl6IrCgenEmitC(&out,&ir,&parse.diags));xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[2]));xl6OutputPrintf(&out,"#define MANAGED_GENERATOR_START xlir_generator_start_raw_%u\n",machine->targetSymbol);xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out,argv[3]));xl6IrDumpModuleJson(&out,&ir);xl6OutputClose(&out);
    xl6IrModuleFree(&ir);xl6ParseResultFree(&parse);
    assert(mutations==3);
    puts("Managed generator IR: closed V2 frame/item descriptors, dynamic owning capture and yield in finally; unused and reachable callable roots retain 4 CFG yields but exactly 3 executable suspensions; opaque V1, unreachable program and suspension identities rejected by C/header gates");return 0;
}
