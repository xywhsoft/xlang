#include "xlir_continuation_owner_test.h"
#include "xlir_activation_capture_assert.h"
int main(int argc,char** argv)
{
    const char* fixtures[]={"test/cases/xlir_generator_storage.xl","test/cases/xlir_generator_ownership.xl",
        "test/cases/xlir_generator_frame_loan.xl","test/cases/xlir_generator_close_first_outcome.xl",
        "test/cases/xlir_generator_launch_allocation.xl"};
    assert(argc==4);
    for(size_t n=0;n<5;++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"generator_storage") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        verify(&ir); assert(ir.abi.generatorStorageAbi==XL6_IR_GENERATOR_STORAGE_NONE);
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_SCOPED_FRAME_V1; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V2; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=(XL6_IrGeneratorStorageAbi)99; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_NONE; assert(xl6IrLowerModule(&ir,&parse.diags)); verify(&ir);
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_NONE; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=(XL6_IrGeneratorStorageAbi)99; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_SCOPED_FRAME_V1; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V3; reject(&ir,"generator storage ABI");
        ir.abi.generatorStorageAbi=XL6_IR_GENERATOR_STORAGE_RESIDENT_CLOSED_FRAME_V4; verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags)); text=xl6OutputTakeMemoryText(&out,NULL);
        for(size_t m=0;m<ir.generatorMachineCount;++m){const XL6_IrGeneratorMachine* machine=&ir.generatorMachines[m];
            assert_activation_layout(text,"xlir_generator",machine->targetSymbol,
                machine->frameSlotCount?ir.generatorFrameSlots+machine->firstFrameSlot:NULL,machine->frameSlotCount,machine->firstFrameSlot,
                machine->destroyActionCount?ir.generatorFrameActions+machine->firstDestroyAction:NULL,machine->destroyActionCount);}
        assert(text && strstr(text,"__xl_runtime/xl6_generator.h") && strstr(text,"xl6GeneratorRuntimeV3Get()->Create") &&
            !strstr(text,"struct xlir_generator {") && !strstr(text,"generator->done =") &&
            !strstr(text,"static const xrtownershipops xlir_generator_frame_ops_")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"generator-storage: 4")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"generatorStorageAbi\": 4")); xrtFree(text);
        if(n==0) {
            assert(xl6OutputOpenFile(&out,argv[1]) && xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[2])); size_t bindings=0;
            for(size_t i=0;i<ir.functionCount;++i) {
                const XL6_IrFunction* function=&ir.functions[i];
                const XL6_IrNode* decl=xl6IrNodeGet(&ir,function->declaration);
                const XL6_IrNode* name=decl && decl->childCount?xl6IrNodeGet(&ir,ir.children[decl->firstChild]):NULL;
                if(name && name->text && !strcmp(name->text,"generatorStorageProbe")) {
                    xl6OutputPrintf(&out,"#define GENERATOR_STORAGE_START xlir_generator_start_raw_%u\n",function->symbol); ++bindings;
                }
            }
            assert(bindings==1); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[3])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
        }
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations==35);
    puts("Generator storage IR: 5 fixtures, 35 rejected C/header-gated mutations; checked preparation V4, prior V3 refused, closed resident control/frame/item lifecycle, explicit text/JSON ABI");
    return 0;
}
