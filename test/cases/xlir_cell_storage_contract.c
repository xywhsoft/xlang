#include "xlir_continuation_owner_test.h"
int main(void)
{
    const char* fixtures[]={"test/cases/xlir_closure_mutable_xvalue_capture.xl",
        "test/cases/xlir_cell_allocation_suspension.xl","test/cases/xlir_cell_allocation_outcome.xl",
        "test/cases/xlir_cell_thread_lifetime.xl","test/cases/xlir_frame_ownership.xl"};
    size_t newCells=0,exchanges=0,initializations=0;
    for(size_t n=0;n<5;++n) {
        XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out; char* text;
        xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
        assert(xl6ParseFile(fixtures[n],&parse) && xl6StdlibExpandModule(parse.root,&parse.diags) &&
            xl6SemCheckModule(parse.root,&parse.diags,"cell_storage") && xl6IrBuildModuleFromAst(&ir,parse.root,&parse.diags));
        verify(&ir); assert(ir.abi.cellStorageAbi==XL6_IR_CELL_STORAGE_NONE);
        ir.abi.cellStorageAbi=XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1; reject(&ir,"cell storage/resident transfer ABI");
        ir.abi.cellStorageAbi=(XL6_IrCellStorageAbi)99; reject(&ir,"cell storage/resident transfer ABI");
        ir.abi.cellStorageAbi=XL6_IR_CELL_STORAGE_NONE; assert(xl6IrLowerModule(&ir,&parse.diags)); verify(&ir);
        assert(ir.abi.cellStorageAbi==XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1);
        ir.abi.cellStorageAbi=XL6_IR_CELL_STORAGE_NONE; reject(&ir,"cell storage/resident transfer ABI");
        ir.abi.cellStorageAbi=(XL6_IrCellStorageAbi)99; reject(&ir,"cell storage/resident transfer ABI");
        ir.abi.cellStorageAbi=XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1; verify(&ir);
        for(size_t i=0;i<ir.instructionCount;++i) {
            newCells+=ir.instructions[i].kind==XL6_IR_INSTR_CELL_NEW;
            initializations+=ir.instructions[i].kind==XL6_IR_INSTR_CELL_INIT;
            exchanges+=ir.instructions[i].kind==XL6_IR_INSTR_CELL_EXCHANGE;
        }
        assert(xl6OutputOpenMemory(&out) && xl6IrCgenEmitC(&out,&ir,&parse.diags));
        text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"__xl_runtime/xl6_cell.h") && strstr(text,"xl6CellRuntimeV1Get()->Create") &&
            strstr(text,"xl6CellRuntimeV1Get()->Initialize") && !strstr(text,"struct xlir_cell {") &&
            !strstr(text,"xlir_cell_ownership_count") && !strstr(text,"xlir_cell_store_") && !strstr(text,"xlir_cell_load_"));
        xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleText(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"cell-storage: 1")); xrtFree(text);
        assert(xl6OutputOpenMemory(&out)); xl6IrDumpModuleJson(&out,&ir); text=xl6OutputTakeMemoryText(&out,NULL);
        assert(text && strstr(text,"\"cellStorageAbi\": 1")); xrtFree(text);
        xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    }
    assert(mutations==20 && newCells && initializations && exchanges);
    printf("Resident cell IR: 5 original fixtures, 20 rejected C/header-gated mutations; %zu allocations, %zu initializations, %zu owning exchanges; explicit text/JSON ABI and no generated lifecycle replica\n",newCells,initializations,exchanges);
    return 0;
}
