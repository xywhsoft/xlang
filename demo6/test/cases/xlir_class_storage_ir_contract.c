#include "xlir_continuation_owner_test.h"
int main(int argc, char** argv)
{
    XL6_ParseResult parse; XL6_IrModule ir; XL6_Output out;
    size_t allocations = 0, commits = 0, lifecycleFields = 0;
    assert(argc == 3); xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_class_storage.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "class_storage"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    verify(&ir); assert(ir.abi.classStorageAbi == XL6_IR_CLASS_STORAGE_NONE);
    for (size_t i = 0; i < ir.nodeCount; ++i) {
        XL6_IrNode* node = &ir.nodes[i];
        XL6_IrNode saved = *node;
        if (node->implicitNameAbi != XL6_IR_IMPLICIT_NAME_CLASS_FIELD) continue;
        ++lifecycleFields;
        node->implicitNameAbi = XL6_IR_IMPLICIT_NAME_NONE;
        reject(&ir, NULL); *node = saved;
        node->implicitReceiverType = XL6_IR_TYPE_INVALID;
        reject(&ir, NULL); *node = saved;
        node->implicitFieldDeclaration = XL6_IR_NODE_INVALID;
        reject(&ir, NULL); *node = saved;
    }
    assert(lifecycleFields == 2);
    ir.abi.classStorageAbi = XL6_IR_CLASS_STORAGE_RESIDENT_PREPARED_LEASE_V1;
    reject(&ir, "class storage/prepared code ownership ABI");
    ir.abi.classStorageAbi = XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2;
    reject(&ir, "class storage/prepared code ownership ABI");
    ir.abi.classStorageAbi = (XL6_IrClassStorageAbi)999;
    reject(&ir, "class storage/prepared code ownership ABI");
    ir.abi.classStorageAbi = XL6_IR_CLASS_STORAGE_NONE;
    if (!xl6IrLowerModule(&ir, &parse.diags)) {
        xl6DiagPrintText(&parse.diags, "test/cases/xlir_class_storage.xl");
        return 2;
    }
    verify(&ir);
    assert(ir.abi.classStorageAbi == XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2);
    ir.abi.classStorageAbi = XL6_IR_CLASS_STORAGE_RESIDENT_PREPARED_LEASE_V1;
    reject(&ir, "class storage/prepared code ownership ABI");
    ir.abi.classStorageAbi = XL6_IR_CLASS_STORAGE_NONE;
    reject(&ir, "class storage/prepared code ownership ABI");
    ir.abi.classStorageAbi = (XL6_IrClassStorageAbi)999;
    reject(&ir, "class storage/prepared code ownership ABI");
    ir.abi.classStorageAbi = XL6_IR_CLASS_STORAGE_RESIDENT_GRAPH_V2;
    for (size_t i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        XL6_IrInstruction saved = *instruction;
        if (instruction->kind != XL6_IR_INSTR_RUNTIME_CALL) continue;
        if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_NEW) {
            ++allocations;
            instruction->runtimeOp = XL6_IR_RUNTIME_XVALUE_OBJECT_NEW;
            reject(&ir, "class construction must pair"); *instruction = saved;
        } else if (instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_CONSTRUCTION_COMMIT) {
            ++commits;
            instruction->payloadType = XL6_IR_TYPE_INVALID;
            reject(&ir, NULL); *instruction = saved;
            /* Another consuming identity operation cannot stand in for the
             * semantic commit even with the original result/owner preserved. */
            instruction->runtimeOp = XL6_IR_RUNTIME_XVALUE_CLASS_IDENTITY_BIND;
            reject(&ir, NULL); *instruction = saved;
        }
    }
    /* Two source classes and both expanded DynamicFields creation sites must
     * obey the same prepared-storage contract, including unreachable bodies. */
    if (allocations != 4 || commits != 4 || mutations != 24) {
        fprintf(stderr, "Class storage population: allocations=%lu commits=%lu mutations=%lu\n",
            (unsigned long)allocations, (unsigned long)commits, (unsigned long)mutations);
        assert(xl6OutputOpenFile(&out, argv[1])); xl6IrDumpModuleJson(&out, &ir); xl6OutputClose(&out);
    }
    assert(allocations == 4 && commits == 4 && mutations == 24); verify(&ir);
    assert(xl6OutputOpenFile(&out, argv[1]) && xl6IrCgenEmitC(&out, &ir, &parse.diags)); xl6OutputClose(&out);
    assert(xl6OutputOpenFile(&out, argv[2]) && xl6IrCgenEmitHeader(&out, &ir, "CLASS_STORAGE", &parse.diags)); xl6OutputClose(&out);
    puts("Class storage IR: 4 prepared allocations, 4 exact commits, 2 lifecycle field bindings, 24 rejected Typed/Lowered/C/header mutations");
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse); return 0;
}
