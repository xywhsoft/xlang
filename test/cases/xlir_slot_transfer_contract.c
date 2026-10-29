#include "xlir_continuation_owner_test.h"
int main(int argc,char** argv)
{
    const char* fixtures[]={"test/cases/xlir_slot_transfers.xl","test/cases/xlir_frame_ownership.xl",
        "test/cases/xlir_generator_ownership.xl","test/cases/xlir_cell_allocation_outcome.xl",
        "test/cases/xlir_module_ownership.xl"};
    assert(argc==4);
    for(size_t n=0;n<5;++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"slot_transfer") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        verify(&ir); assert(ir.abi.slotTransferAbi==XL6_IR_SLOT_TRANSFER_NONE);
        ir.abi.slotTransferAbi=XL6_IR_SLOT_TRANSFER_RESIDENT_ORDERED_WRITES_V1; reject(&ir,"physical slot transfer ABI");
        ir.abi.slotTransferAbi=(XL6_IrSlotTransferAbi)99; reject(&ir,"physical slot transfer ABI");
        ir.abi.slotTransferAbi=XL6_IR_SLOT_TRANSFER_NONE; assert(xl6IrLowerModule(&ir,&parse.diags)); verify(&ir);
        ir.abi.slotTransferAbi=XL6_IR_SLOT_TRANSFER_NONE; reject(&ir,"physical slot transfer ABI");
        ir.abi.slotTransferAbi=(XL6_IrSlotTransferAbi)99; reject(&ir,"physical slot transfer ABI");
        ir.abi.slotTransferAbi=XL6_IR_SLOT_TRANSFER_RESIDENT_ORDERED_WRITES_V1; verify(&ir);
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags)); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"__xl_runtime/xl6_slot.h") && strstr(text,"xl6SlotRuntimeV1Get()->Apply")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"slot-transfer: 1")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"slotTransferAbi\": 1")); xrtFree(text);
        if(n==0) {
            assert(xl6OutputOpenFile(&out,argv[1]) && xl6IrCgenEmitC(&out,&ir,&parse.diags)); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[2]));
            size_t bindings=0;
            for(size_t i=0;i<ir.functionCount;++i) {
                const XL6_IrFunction* function=&ir.functions[i];
                const XL6_IrNode* decl=xl6IrNodeGet(&ir,function->declaration);
                const XL6_IrNode* name=decl && decl->childCount?xl6IrNodeGet(&ir,ir.children[decl->firstChild]):NULL;
                if(name && name->text && (!strcmp(name->text,"writeSlot") || !strcmp(name->text,"readSlot"))) {
                    xl6OutputPrintf(&out,"#define SLOT_%s xlir_fn_%u\n",!strcmp(name->text,"writeSlot")?"WRITE":"READ",function->symbol); ++bindings;
                }
            }
            assert(bindings==2); xl6OutputClose(&out);
            assert(xl6OutputOpenFile(&out,argv[3])); xl6IrDumpModuleJson(&out,&ir); xl6OutputClose(&out);
        }
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations==20);
    puts("Slot transfer IR: 5 fixtures, 20 rejected C/header-gated mutations, explicit text/JSON ABI; ordinary globals, async/generator frames, cells and module owners");
    return 0;
}
