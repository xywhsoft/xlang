#include "src/cgen/xl6_ir_cgen.h"
#include <assert.h>
#include <stdio.h>

static XL6_AstNode* intType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "int", 3, NULL);
    type->semanticTypeId = 1;
    type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.int64");
    return type;
}

static XL6_AstNode* callbackType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "function", 8, NULL);
    XL6_AstNode* params = xl6AstNew(XL6_AST_LIST, NULL);
    XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
    xl6AstAddChild(params, intType());
    xl6AstAddChild(returns, intType());
    xl6AstAddChild(type, params);
    xl6AstAddChild(type, returns);
    return type;
}

static void reject(XL6_IrModule* ir)
{
    XL6_DiagList diags = {0};
    XL6_Output out;
    char* text;
    assert(!xl6IrVerifyModule(ir, &diags) && diags.count != 0);
    assert(xl6OutputOpenMemory(&out));
    assert(!xl6IrCgenEmitC(&out, ir, &diags));
    text = xl6OutputTakeMemoryText(&out, NULL);
    xrtFree(text);
    xl6DiagFree(&diags);
}

int main(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags = {0};
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    XL6_AstNode* function = xl6AstNewText(XL6_AST_EXPORT_FUNC, "callback_contract", 17, NULL);
    XL6_AstNode* params = xl6AstNew(XL6_AST_LIST, NULL);
    XL6_AstNode* param = xl6AstNewText(XL6_AST_PARAM, "callback", 8, NULL);
    XL6_IrType* signature;
    XL6_IrExportValueAbi saved;
    function->semanticSymbolId = 1;
    param->semanticSymbolId = 2;
    xl6AstAddChild(param, callbackType());
    xl6AstAddChild(params, param);
    {
        XL6_AstNode* nullable = xl6AstNewText(XL6_AST_TYPE, "nullable", 8, NULL);
        XL6_AstNode* payload = xl6AstNew(XL6_AST_LIST, NULL);
        XL6_AstNode* optional = xl6AstNewText(XL6_AST_PARAM, "optional", 8, NULL);
        optional->semanticSymbolId = 3;
        xl6AstAddChild(payload, callbackType());
        xl6AstAddChild(nullable, payload);
        xl6AstAddChild(optional, nullable);
        xl6AstAddChild(params, optional);
    }
    xl6AstAddChild(function, xl6AstNewText(XL6_AST_NAME, "callback_contract", 17, NULL));
    xl6AstAddChild(function, params);
    xl6AstAddChild(function, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    xl6AstAddChild(function, xl6AstNew(XL6_AST_BLOCK, NULL));
    xl6AstAddChild(root, function);
    xl6IrModuleInit(&ir);
    assert(xl6IrBuildModuleFromAst(&ir, root, &diags));
    if (!xl6IrLowerModule(&ir, &diags) || !xl6IrVerifyModule(&ir, &diags)) {
        fprintf(stderr, "callback contract initial lowering failed (%lu diagnostics)\n", (unsigned long)diags.count);
        return 1;
    }
    assert(ir.exportCount == 1 && ir.exports[0].parameterCount == 2);
    saved = ir.exports[0].parameterValueAbis[0];
    assert(saved == XL6_IR_EXPORT_VALUE_RAW_C_CALLBACK_BORROW);
    ir.exports[0].parameterValueAbis[0] = XL6_IR_EXPORT_VALUE_TYPE_REPRESENTATION;
    reject(&ir);
    ir.exports[0].parameterValueAbis[0] = XL6_IR_EXPORT_VALUE_INVALID;
    reject(&ir);
    ir.exports[0].parameterValueAbis[0] = XL6_IR_EXPORT_VALUE_RAW_C_CALLBACK_OWNED_RESULT_BORROW;
    reject(&ir);
    ir.exports[0].parameterValueAbis[0] = saved;
    ir.exports[0].parameterAbis[0] = XL6_IR_PARAMETER_ABI_OWNED;
    reject(&ir);
    ir.exports[0].parameterAbis[0] = XL6_IR_PARAMETER_ABI_BORROW;
    signature = &ir.types.items[ir.exports[0].parameterTypes[0] - 1];
    assert(signature->parameterModes[0] == XL6_IR_PARAMETER_MODE_DEFAULT);
    signature->parameterModes[0] = XL6_IR_PARAMETER_MODE_BYREF;
    assert(xl6IrExportParameterValueAbiForType(&ir, ir.exports[0].parameterTypes[0],
        XL6_IR_PARAMETER_ABI_BORROW) == XL6_IR_EXPORT_VALUE_INVALID);
    reject(&ir);
    signature->parameterModes[0] = XL6_IR_PARAMETER_MODE_DEFAULT;
    {
        XL6_IrTypeId optionalType = ir.exports[0].parameterTypes[1];
        XL6_IrType* optional = &ir.types.items[optionalType - 1];
        XL6_IrTypeId payload = optional->params[0];
        XL6_IrNullAbi nullAbi = optional->nullAbi;
        assert(ir.exports[0].parameterValueAbis[1] == saved);
        assert(xl6IrExportCallbackSignatureType(&ir, optionalType) == payload);
        optional->params[0] = signature->params[0];
        assert(xl6IrExportCallbackSignatureType(&ir, optionalType) == XL6_IR_TYPE_INVALID);
        reject(&ir);
        optional->params[0] = payload;
        optional->nullAbi = XL6_IR_NULL_ABI_NONE;
        assert(xl6IrExportCallbackSignatureType(&ir, optionalType) == XL6_IR_TYPE_INVALID);
        reject(&ir);
        optional->nullAbi = nullAbi;
    }
    assert(xl6IrVerifyModule(&ir, &diags));
    {
        XL6_Output out;
        char* generated;
        FILE* file;
        assert(xl6OutputOpenMemory(&out));
        assert(xl6IrCgenEmitC(&out, &ir, &diags));
        generated = xl6OutputTakeMemoryText(&out, NULL);
        assert(generated != NULL);
        file = fopen("build/test/xlir_export_callback_contract_generated.c", "wb");
        assert(file != NULL && fputs(generated, file) >= 0);
        assert(fclose(file) == 0);
        xrtFree(generated);
    }
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xl6DiagFree(&diags);
    puts("seven callback export verifier/Cgen mutation contracts ok");
    return 0;
}
