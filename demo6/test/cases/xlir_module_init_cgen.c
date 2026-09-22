#include "src/cgen/xl6_ir_cgen.h"

#include <stdio.h>

static XL6_AstNode* testIntType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "int", 3, NULL);
    if ( type != NULL ) {
        type->semanticTypeId = 1;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.int64");
    }
    return type;
}

static XL6_AstNode* testStringType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "string", 6, NULL);
    if (type != NULL) {
        type->semanticTypeId = 2;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.str");
    }
    return type;
}

static int rejectFinalizerMutation(XL6_IrModule* ir, const char* name)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* generated;
    int ok = !xl6IrVerifyModule(ir, &diags) && diags.count != 0;
    if (!xl6OutputOpenMemory(&out)) return 0;
    ok = !xl6IrCgenEmitC(&out, ir, &diags) && ok;
    generated = xl6OutputTakeMemoryText(&out, NULL);
    if (!ok) fprintf(stderr, "module finalizer mutation accepted: %s\n", name);
    xrtFree(generated);
    xrtFree(diags.items);
    return ok;
}

static int testModuleFinalizerContract(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags = {0};
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_IrFunction* fini;
    XL6_IrInstruction saved, second;
    uint32 index;
    size_t first;
    int i, ok = root != NULL;
    xl6IrModuleInit(&ir);
    for (i = 0; ok && i < 2; ++i) {
        XL6_AstNode* decl = xl6AstNewText(XL6_AST_VAR_DECL, i ? "second" : "first", i ? 6 : 5, NULL);
        XL6_AstNode* value = xl6AstNewText(XL6_AST_STRING, "\"owned\"", 7, NULL);
        if (decl == NULL || value == NULL) return 0;
        decl->semanticSymbolId = 7 + i;
        decl->inferredType = testStringType();
        value->inferredType = testStringType();
        value->semanticOwnership = XL6_SEM_OWNERSHIP_MOVE;
        xl6AstAddChild(decl, testStringType());
        xl6AstAddChild(decl, value);
        xl6AstAddChild(root, decl);
    }
    ok = ok && xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         ir.abi.finalizerFunction != 0 && ir.abi.finalizerCleanupCount == 2;
    if (!ok) goto done;
    index = ir.abi.finalizerFunction;
    fini = &ir.functions[index - 1];
    first = fini->firstInstruction;
    ok = fini->kind == XL6_IR_FUNCTION_MODULE_FINI && fini->instructionCount == 9 &&
         fini->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
         ir.instructions[first].kind == XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN &&
         ir.instructions[first + 1].targetSymbol == 8 &&
         ir.instructions[first + 3].targetSymbol == 7;
    if (!ok) goto done;
    ir.abi.finalizerFunction = 0;
    ok = rejectFinalizerMutation(&ir, "missing finalizer index") && ok;
    ir.abi.finalizerFunction = index;
    --ir.abi.finalizerCleanupCount;
    ok = rejectFinalizerMutation(&ir, "incomplete owner count") && ok;
    ++ir.abi.finalizerCleanupCount;
    saved = ir.instructions[first + 1];
    second = ir.instructions[first + 3];
    ir.instructions[first + 1] = ir.instructions[first + 3];
    ok = rejectFinalizerMutation(&ir, "duplicate owner/missing owner") && ok;
    ir.instructions[first + 3] = saved;
    ok = rejectFinalizerMutation(&ir, "forward instead of reverse order") && ok;
    ir.instructions[first + 1] = saved;
    ir.instructions[first + 3] = second;
    saved = ir.instructions[first + 5];
    ir.instructions[first + 5].kind = XL6_IR_INSTR_DISCARD;
    ok = rejectFinalizerMutation(&ir, "lost aggregate cleanup outcome") && ok;
    ir.instructions[first + 5] = saved;
    fini->errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
    ok = rejectFinalizerMutation(&ir, "missing finalizer failure carrier") && ok;
    fini->errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    saved = ir.instructions[first];
    ir.instructions[first].kind = XL6_IR_INSTR_CLEANUP_BEGIN;
    ok = rejectFinalizerMutation(&ir, "lost incoming failure inheritance") && ok;
    ir.instructions[first] = saved;
    saved = ir.instructions[first + 2];
    ir.instructions[first + 2].kind = XL6_IR_INSTR_ERROR_CLEAR;
    ok = rejectFinalizerMutation(&ir, "lost per-owner diagnostic checkpoint") && ok;
    ir.instructions[first + 2] = saved;
    saved = ir.instructions[first + 6];
    ir.instructions[first + 6].trueTarget = ir.instructions[first + 6].falseTarget;
    ok = rejectFinalizerMutation(&ir, "cleanup success and failure merged") && ok;
    ir.instructions[first + 6] = saved;
    ok = xl6IrVerifyModule(&ir, &diags) && ok;
done:
    if (!ok) fprintf(stderr, "module finalizer verifier contract failed: %s\n",
        diags.count ? diags.items[diags.count - 1].message : "none");
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    return ok;
}

int main(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_Output out;
    XL6_Output repeatOut;
    XL6_AstNode* root;
    XL6_AstNode* decl;
    XL6_AstNode* type;
    XL6_AstNode* value;
    char* text;
    char* repeat;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    decl = xl6AstNewText(XL6_AST_VAR_DECL, "answer", 6, NULL);
    type = xl6AstNewText(XL6_AST_TYPE, "auto", 4, NULL);
    value = xl6AstNewText(XL6_AST_INT, "42", 2, NULL);
    if ( root == NULL || decl == NULL || type == NULL || value == NULL || !xl6OutputOpenMemory(&out) ) return 1;
    decl->semanticSymbolId = 7;
    decl->inferredType = testIntType();
    value->inferredType = testIntType();
    value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
    xl6AstAddChild(decl, type);
    xl6AstAddChild(decl, value);
    xl6AstAddChild(root, decl);

    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrCgenEmitC(&out, &ir, &diags);
    text = xl6OutputTakeMemoryText(&out, NULL);
    if ( text != NULL ) {
        FILE* generated = fopen("build/test/xlir_module_init_generated.c", "wb");
        if ( generated == NULL || fputs(text, generated) < 0 || fclose(generated) != 0 ) ok = 0;
    }
    ok = ok && ir.storageCount == 1 && ir.storages[0].kind == XL6_IR_STORAGE_GLOBAL &&
         text != NULL && strstr(text, "static int64_t s7;\n") != NULL &&
         strstr(text, "static int xlir_module_init(void)\n") != NULL &&
         strstr(text, "if (xlir_module_initialized == 2) return 1;") != NULL &&
         strstr(text, "if (xlir_module_initialized != 1 && xlir_module_initialized != 2) return false;") != NULL &&
         strstr(text, "v1 = 42LL;") != NULL &&
         strstr(text, "{ int64_t xlir_committed = v1;") != NULL &&
         strstr(text, "{ &(s7), &(xlir_committed), sizeof(s7) },") != NULL &&
         strstr(text, "(xl6SlotRuntimeV1Get()->Apply)(xlir_writes, sizeof(xlir_writes)/sizeof(*xlir_writes));") != NULL &&
         strstr(text, "xlir_module_initialized = 2;") != NULL &&
         ir.abi.finalizerFunction != 0 && ir.abi.finalizerCleanupCount == 0;
    repeat = NULL;
    if ( ok && xl6OutputOpenMemory(&repeatOut) ) {
        ok = xl6IrCgenEmitC(&repeatOut, &ir, &diags);
        repeat = xl6OutputTakeMemoryText(&repeatOut, NULL);
        ok = ok && repeat != NULL && strcmp(text, repeat) == 0;
    } else {
        ok = 0;
    }
    if ( !ok ) fprintf(stderr, "XLIR module init C emitter test failed: %s\n", diags.count > 0 ? diags.items[0].message : "<none>");
    xrtFree(repeat);
    xrtFree(text);
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    if ( !ok || !testModuleFinalizerContract() ) return 1;
    puts("XLIR module init C emitter and nine finalizer verifier/Cgen rejection contracts ok");
    return 0;
}
