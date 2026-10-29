#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t refusals;
static void rejectAt(XL6_IrModule* module, int line)
{
    if (xl6IrVerifyModule(module, NULL))
        fprintf(stderr, "accepted mutation from line %d, refusal number %zu\n", line, refusals + 1);
    assert(!xl6IrVerifyModule(module, NULL));
    for (int header = 0; header < 2; ++header) {
        XL6_Output output;
        XL6_DiagList errors;
        size_t size = SIZE_MAX;
        xl6DiagInit(&errors);
        assert(xl6OutputOpenMemory(&output));
        assert(!(header ? xl6IrCgenEmitHeader(&output, module, "BAD_FIELD_GRAPH", &errors)
                       : xl6IrCgenEmitC(&output, module, &errors)));
        char* text = xl6OutputTakeMemoryText(&output, &size);
        assert(size == 0);
        xrtFree(text);
        xl6OutputDiscard(&output);
        xl6DiagFree(&errors);
    }
    ++refusals;
}
#define reject(module) rejectAt(module, __LINE__)

static void exercise(const char* source)
{
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, source, 1));
    assert(xl6IrVerifyModule(&unit.ir, NULL));
    assert(xl6IrClassInitializerSpecializationCount(&unit.ir) == 0);
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    size_t programs = 0, wrongOwner = 0, changedType = 0;
    for (size_t i = 0; i < module->classFieldCount; ++i) {
        XL6_IrClassField* field = &module->classFields[i];
        XL6_IrNodeId original;
        assert(xl6IrFieldDeclarationInitializer(module, field->declaration, &original));
        if (field->initializer == original) continue;
        ++programs;
        XL6_IrNodeId saved = field->initializer;
        field->initializer = original;
        reject(module);
        field->initializer = 0;
        reject(module);
        field->initializer = saved;
        XL6_IrNode* node = &module->nodes[saved - 1];
        const XL6_IrNode* sourceNode = xl6IrNodeGet(module, original);
        if (node->storageType != sourceNode->storageType) {
            XL6_IrTypeId type = node->storageType;
            node->storageType = sourceNode->storageType;
            reject(module);
            node->storageType = type;
            ++changedType;
        }
        char* text = node->text;
        node->text = "wrong-initializer-origin";
        reject(module);
        node->text = text;
        if (sourceNode->literalData) {
            assert(node->literalData && node->literalData != sourceNode->literalData);
            unsigned char* data = node->literalData;
            node->literalData = sourceNode->literalData;
            reject(module);
            node->literalData = data;
        }
        for (size_t j = 0; j < module->classFieldCount; ++j) {
            const XL6_IrClassField* other = &module->classFields[j];
            if (other->declaration != field->declaration || other->ownerType == field->ownerType) continue;
            field->initializer = other->initializer;
            reject(module);
            field->initializer = saved;
            ++wrongOwner;
            break;
        }
    }
    assert(programs >= 2 && wrongOwner && changedType);
    assert(programs == xl6IrClassInitializerSpecializationCount(module));
    size_t count = module->specializedSymbolCount;
    assert(count > 1);
    module->specializedSymbolCount = module->specializedSymbolCapacity + 1;
    reject(module);
    module->specializedSymbolCount = count;
    for (size_t i = 0; i < count; ++i) {
        XL6_IrSymbolBinding* item = &module->specializedSymbols[i];
        assert(xl6IrSymbolNodeGet(module, item->symbol) == xl6IrNodeGet(module, item->declaration));
        XL6_IrNodeId declaration = item->declaration;
        item->declaration = module->root;
        reject(module);
        item->declaration = declaration;
    }
    XL6_SymbolId symbol = module->specializedSymbols[1].symbol;
    module->specializedSymbols[1].symbol = module->specializedSymbols[0].symbol;
    reject(module);
    module->specializedSymbols[1].symbol = symbol;
    assert(xl6IrVerifyModule(module, NULL));
    printf("generic field: %zu owner programs, %zu wrong-owner refusals, %zu canonical bindings\n",
        programs, wrongOwner, count);
    xl6CompileUnitFree(&unit);
}

int main(int argc, char** argv)
{
    assert(argc > 1);
    for (int i = 1; i < argc; ++i) exercise(argv[i]);
    printf("generic fields: %zu zero-C/header refusals\n", refusals);
    return 0;
}
