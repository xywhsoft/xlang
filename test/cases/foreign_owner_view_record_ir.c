#include "src/compile/xl6_compile_unit.h"
#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static unsigned rejections;

static void refused(XL6_IrModule* module)
{
    assert(!xl6IrVerifyModule(module, NULL));
    for (unsigned header = 0; header < 2; ++header) {
        XL6_Output out;
        size_t size = SIZE_MAX;
        assert(xl6OutputOpenMemory(&out));
        int ok = header ? xl6IrCgenEmitHeader(&out, module, "INVALID_WATCHED_FIELD", NULL) :
            xl6IrCgenEmitC(&out, module, NULL);
        assert(!ok);
        char* bytes = xl6OutputTakeMemoryText(&out, &size);
        assert(bytes && size == 0);
        xrtFree(bytes);
        xl6OutputDiscard(&out);
    }
    ++rejections;
}

int main(int argc, char** argv)
{
    assert(argc == 2 && sizeof(XL6_ForeignCallFacts) == 88);
    XL6_CompileUnit unit;
    xl6CompileUnitInit(&unit);
    assert(xl6CompileUnitLoadFile(&unit, argv[1], 1));
    assert(xl6IrLowerModule(&unit.ir, &unit.parse.diags));
    XL6_IrModule* module = &unit.ir;
    assert(xl6IrVerifyModule(module, NULL));
    unsigned watched = 0;
    for (size_t i = 0; i < module->foreignOutputViewCount; ++i) {
        XL6_ForeignOutputViewContract* row = &module->foreignOutputViews[i];
        if (row->conversion != XL6_FOREIGN_OUTPUT_CONVERSION_OWNER_VIEW) continue;
        ++watched;
        XL6_ForeignOutputViewContract saved = *row;
        row->conversion = XL6_FOREIGN_OUTPUT_CONVERSION_CSTRING; refused(module); *row = saved;
        row->physicalIndex = 63; refused(module); *row = saved;
        row->ordinal = 63; refused(module); *row = saved;
        row->retainSymbol = 1; refused(module); *row = saved;
        row->rollbackSymbol = 1; refused(module); *row = saved;
        row->retainSuccess = XL6_FOREIGN_RETAIN_ZERO; refused(module); *row = saved;

        XL6_IrNode* node = &module->nodes[row->declaration - 1];
        uint64 flags = node->flags;
        node->flags |= XL6_AST_FLAG_STATIC; refused(module); node->flags = flags;
        node->flags |= XL6_AST_FLAG_FOREIGN_PURE; refused(module); node->flags = flags;
        node->flags &= ~XL6_AST_FLAG_FOREIGN_NO_RAISE; refused(module); node->flags = flags;
        XL6_IrTypeId owner = node->declarationOwnerType;
        node->declarationOwnerType = 0; refused(module); node->declarationOwnerType = owner;
        node->foreignCall.controlViews |= UINT64_C(1);
        refused(module);
        node->foreignCall.controlViews &= ~UINT64_C(1);
    }
    assert(watched == 5 && rejections == 55 && xl6IrVerifyModule(module, NULL));
    puts("WATCHED_RECORD_IR_OK 55");
    xl6CompileUnitFree(&unit);
    return 0;
}
