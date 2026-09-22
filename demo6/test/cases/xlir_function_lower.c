#include "src/ir/xl6_ir.h"

#include <stdio.h>
#include <string.h>

static XL6_AstNode* testIntType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "int", 3, NULL);
    if ( type != NULL ) {
        type->semanticTypeId = 1;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.int64");
    }
    return type;
}

static XL6_AstNode* testManagedType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "string", 6, NULL);
    if ( type != NULL ) {
        type->semanticTypeId = 2;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.str");
    }
    return type;
}

static XL6_AstNode* testBoolType(void)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, "bool", 4, NULL);
    if ( type != NULL ) {
        type->semanticTypeId = 3;
        type->semanticTypeAbiId = xl6TypeAbiIdText("xrt.bool");
    }
    return type;
}

/* A normal block exit and an abrupt return have independent plans. Select
 * by their explicit point, not the incidental order in the plan arena. */
static XL6_IrExitPlan* testUniqueExitPlan(
    XL6_IrModule* ir, XL6_IrExitPoint point, XL6_SemExitKind kind)
{
    XL6_IrExitPlan* found = NULL;
    size_t i;
    for (i = 0; i < ir->exitPlanCount; ++i) {
        XL6_IrExitPlan* plan = &ir->exitPlans[i];
        if (plan->point != point || plan->kind != kind) continue;
        if (found != NULL) return NULL;
        found = plan;
    }
    return found;
}

static int testReleasePlan(
    XL6_IrModule* ir, XL6_IrExitPoint point,
    const XL6_SymbolId* symbols, size_t count)
{
    XL6_IrExitPlan* plan = testUniqueExitPlan(ir, point,
        point == XL6_IR_EXIT_POINT_AFTER_SCOPE ? XL6_SEM_EXIT_NONE : XL6_SEM_EXIT_RETURN);
    const XL6_IrNode* origin = plan != NULL ? xl6IrNodeGet(ir, plan->origin) : NULL;
    size_t i;
    if (origin == NULL || plan->cleanupCount != count ||
        (point == XL6_IR_EXIT_POINT_AFTER_SCOPE ? origin->kind != XL6_IR_NODE_BLOCK
                                              : origin->exitKind != XL6_SEM_EXIT_RETURN)) return 0;
    for (i = 0; i < count; ++i) {
        const XL6_IrCleanupAction* action = &ir->cleanupActions[plan->firstCleanup + i];
        if (action->kind != XL6_IR_CLEANUP_RELEASE || action->targetSymbol != symbols[i]) return 0;
    }
    return 1;
}

static int testManagedOwnershipIsNotErased(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    size_t i;
    int sawDup = 0;
    int sawCopy = 0;
    int sawReturn = 0;
    int sawForeignOwnershipCheck = 0;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    if ( root == NULL || func == NULL ) return 0;
    func->semanticSymbolId = 12;
    {
        XL6_AstNode* name = xl6AstNewText(XL6_AST_NAME, "owned", 5, NULL);
        name->inferredType = testManagedType();
        xl6AstAddChild(func, name);
    }
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        xl6AstAddChild(returns, testManagedType());
        xl6AstAddChild(func, returns);
    }
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        XL6_AstNode* value = xl6AstNewText(XL6_AST_STRING, "\"x\"", 3, NULL);
        value->inferredType = testManagedType();
        value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        xl6AstAddChild(ret, value);
        xl6AstAddChild(body, ret);
        xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags);
    for ( i = 0; ok && i < ir.instructionCount; ++i ) {
        const XL6_IrInstruction* instruction = &ir.instructions[i];
        if ( instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
             instruction->runtimeOp == XL6_IR_RUNTIME_STRING_DUP ) sawDup = 1;
        if ( instruction->kind == XL6_IR_INSTR_RUNTIME_CALL &&
             instruction->runtimeOp == XL6_IR_RUNTIME_STRING_COPY ) sawCopy = 1;
        if ( instruction->kind == XL6_IR_INSTR_RETURN ) sawReturn = 1;
    }
    /* The literal producer already owns its allocation.  Returning it is a
     * direct move; a second STRING_COPY would be a redundant allocation. */
    ok = ok && sawDup && !sawCopy && sawReturn;
    /* 验证器必须拒绝把内部拥有返回误标为借用，也拒绝遗漏外部字符串复制。 */
    for ( i = 0; ok && i < ir.callTargetCount; ++i ) {
        XL6_IrCallTarget* target = &ir.callTargets[i];
        if ( target->symbol != 12 ) continue;
        sawForeignOwnershipCheck = 1;
        ok = target->resultConversionAbi == XL6_IR_CALL_RESULT_CONVERSION_DIRECT;
        target->resultConversionAbi = XL6_IR_CALL_RESULT_CONVERSION_BORROWED_STRING_COPY;
        ok = ok && !xl6IrVerifyModule(&ir, &diags) && diags.count > 0 &&
             strstr(diags.items[diags.count - 1].message,
                    "foreign result ownership conversion ABI") != NULL;
        target->resultConversionAbi = XL6_IR_CALL_RESULT_CONVERSION_DIRECT;
        target->kind = XL6_IR_CALL_TARGET_EXTERN_C;
        target->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_FOREIGN_MAY_BLOCK;
        ok = ok && !xl6IrVerifyModule(&ir, &diags) && diags.count > 0 &&
             strstr(diags.items[diags.count - 1].message,
                    "foreign result ownership conversion ABI") != NULL;
        target->kind = XL6_IR_CALL_TARGET_INTERNAL;
        target->executionBoundaryAbi = XL6_IR_CALL_EXECUTION_LANGUAGE_CFG;
        ok = ok && xl6IrVerifyModule(&ir, &diags);
    }
    ok = ok && sawForeignOwnershipCheck;
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    return ok;
}

static int testShortCircuitUsesCfg(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    size_t i;
    size_t constants = 0;
    size_t branches = 0;
    size_t jumps = 0;
    size_t stores = 0;
    size_t loads = 0;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    if ( root == NULL || func == NULL ) return 0;
    func->semanticSymbolId = 13;
    {
        XL6_AstNode* name = xl6AstNewText(XL6_AST_NAME, "guard", 5, NULL);
        name->inferredType = testBoolType();
        xl6AstAddChild(func, name);
    }
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        xl6AstAddChild(returns, testBoolType());
        xl6AstAddChild(func, returns);
    }
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        XL6_AstNode* binary = xl6AstNewText(XL6_AST_BINARY, "&&", 2, NULL);
        XL6_AstNode* left = xl6AstNewText(XL6_AST_BOOL, "true", 4, NULL);
        XL6_AstNode* right = xl6AstNewText(XL6_AST_BOOL, "false", 5, NULL);
        binary->inferredType = testBoolType();
        left->inferredType = testBoolType();
        right->inferredType = testBoolType();
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        xl6AstAddChild(binary, left);
        xl6AstAddChild(binary, right);
        xl6AstAddChild(ret, binary);
        xl6AstAddChild(body, ret);
        xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags) &&
         ir.functionCount == 3 && ir.abi.finalizerFunction == 3 &&
         ir.functions[2].kind == XL6_IR_FUNCTION_MODULE_FINI && ir.functions[1].blockCount == 3 &&
         ir.functions[1].instructionCount > 0;
    for ( i = ir.functions[1].firstInstruction;
          ok && i < ir.functions[1].firstInstruction + ir.functions[1].instructionCount; ++i ) {
        if ( ir.instructions[i].kind == XL6_IR_INSTR_CONST ) constants += 1;
        if ( ir.instructions[i].kind == XL6_IR_INSTR_BRANCH ) branches += 1;
        if ( ir.instructions[i].kind == XL6_IR_INSTR_JUMP ) jumps += 1;
        if ( ir.instructions[i].kind == XL6_IR_INSTR_STORE ) stores += 1;
        if ( ir.instructions[i].kind == XL6_IR_INSTR_LOAD ) loads += 1;
    }
    ok = ok && constants == 2 && branches == 1 && jumps == 1 &&
         stores == 2 && loads == 1;
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    return ok;
}

static int testNonVoidPathMustTerminate(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    int lowered;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    if ( root == NULL || func == NULL ) return 0;
    func->semanticSymbolId = 19;
    {
        XL6_AstNode* name = xl6AstNewText(XL6_AST_NAME, "missing", 7, NULL);
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* value = xl6AstNewText(XL6_AST_INT, "1", 1, NULL);
        name->inferredType = testIntType();
        value->inferredType = testIntType();
        xl6AstAddChild(func, name);
        xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
        xl6AstAddChild(returns, testIntType());
        xl6AstAddChild(func, returns);
        xl6AstAddChild(body, value);
        xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags);
    lowered = ok ? xl6IrLowerModule(&ir, &diags) : -1;
    ok = ok && !lowered && diags.count > 0 &&
         strstr(diags.items[diags.count - 1].message,
                "XLIR non-void function has reachable unterminated") != NULL;
    xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    return ok;
}

static int testCleanupActionIsMaterialized(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    size_t i;
    int sawReleaseReturn = 0;
    int ok;
    const XL6_SymbolId expected[] = {22};

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    if ( root == NULL || func == NULL ) return 0;
    func->semanticSymbolId = 21;
    func->semanticRegionId = 100;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "clean", 5, NULL));
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(func, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* local = xl6AstNewText(XL6_AST_VAR_DECL, "temp", 4, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        body->semanticScopeDepth = 1;
        body->semanticRegionId = 101;
        local->semanticSymbolId = 22;
        local->semanticScopeDepth = 1;
        local->semanticRegionId = 101;
        local->inferredType = testManagedType();
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        ret->semanticScopeDepth = 1;
        ret->semanticTargetScopeDepth = 0;
        ret->semanticRegionId = 101;
        ret->semanticTargetRegionId = 100;
        xl6AstAddChild(local, testManagedType());
        xl6AstAddChild(body, local);
        xl6AstAddChild(body, ret);
        xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         testReleasePlan(&ir, XL6_IR_EXIT_POINT_AFTER_SCOPE, expected, 1) &&
         testReleasePlan(&ir, XL6_IR_EXIT_POINT_BEFORE_NODE, expected, 1) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrVerifyModule(&ir, &diags);
    for ( i = 0; ok && i + 1 < ir.instructionCount; ++i ) {
        if ( ir.instructions[i].kind == XL6_IR_INSTR_RELEASE &&
             ir.instructions[i + 1].kind == XL6_IR_INSTR_RETURN ) sawReleaseReturn = 1;
    }
    ok = ok && sawReleaseReturn;
    if ( !ok ) {
        const XL6_IrExitPlan* plan = testUniqueExitPlan(
            &ir, XL6_IR_EXIT_POINT_BEFORE_NODE, XL6_SEM_EXIT_RETURN);
        const XL6_IrNode* origin = plan != NULL ? xl6IrNodeGet(&ir, plan->origin) : NULL;
        fprintf(stderr,
                "cleanup materialization failed: stage=%d plans=%lu actions=%lu instructions=%lu "
                "planKind=%d planScope=%u planRegion=%lu originKind=%d originExit=%d "
                "originScope=%u originRegion=%lu first=%lu count=%lu message=%s\n",
                (int)ir.stage, (unsigned long)ir.exitPlanCount,
                (unsigned long)ir.cleanupActionCount,
                (unsigned long)ir.instructionCount,
                plan != NULL ? (int)plan->kind : -1,
                plan != NULL ? (unsigned)plan->targetScopeDepth : 0,
                plan != NULL ? (unsigned long)plan->targetRegion : 0,
                origin != NULL ? (int)origin->kind : -1,
                origin != NULL ? (int)origin->exitKind : -1,
                origin != NULL ? (unsigned)origin->targetScopeDepth : 0,
                origin != NULL ? (unsigned long)origin->targetRegion : 0,
                plan != NULL ? (unsigned long)plan->firstCleanup : 0,
                plan != NULL ? (unsigned long)plan->cleanupCount : 0,
                diags.count != 0 && diags.items[diags.count - 1].message != NULL
                    ? diags.items[diags.count - 1].message : "<none>");
    }
    xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    return ok;
}

static int testSemanticCleanupPlanUsesLexicalLifo(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    XL6_AstNode* body;
    XL6_AstNode* ret;
    int ok;
    const XL6_SymbolId expected[] = {32, 31};

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    body = xl6AstNew(XL6_AST_BLOCK, NULL);
    ret = xl6AstNew(XL6_AST_RETURN, NULL);
    if ( root == NULL || func == NULL || body == NULL || ret == NULL ) return 0;
    func->semanticSymbolId = 30;
    func->semanticRegionId = 100;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "clean_plan", 10, NULL));
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(func, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    body->semanticScopeDepth = 1;
    body->semanticRegionId = 101;
    {
        XL6_AstNode* first = xl6AstNewText(XL6_AST_VAR_DECL, "first", 5, NULL);
        XL6_AstNode* second = xl6AstNewText(XL6_AST_VAR_DECL, "second", 6, NULL);
        first->semanticSymbolId = 31; first->semanticScopeDepth = 1; first->semanticRegionId = 101;
        second->semanticSymbolId = 32; second->semanticScopeDepth = 1; second->semanticRegionId = 101;
        xl6AstAddChild(first, testManagedType());
        xl6AstAddChild(second, testManagedType());
        xl6AstAddChild(body, first);
        xl6AstAddChild(body, second);
    }
    ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
    ret->semanticScopeDepth = 1;
    ret->semanticTargetScopeDepth = 0;
    ret->semanticRegionId = 101;
    ret->semanticTargetRegionId = 100;
    xl6AstAddChild(body, ret);
    xl6AstAddChild(func, body);
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         testReleasePlan(&ir, XL6_IR_EXIT_POINT_AFTER_SCOPE, expected, 2) &&
         testReleasePlan(&ir, XL6_IR_EXIT_POINT_BEFORE_NODE, expected, 2);
    xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    return ok;
}

/* 构造最小物理类型表，逐项破坏桥接合同，避免仅靠生成文本断言。 */
static int testStringBridgeContracts(void)
{
    const XL6_IrCAbi abis[] = {
        XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_I64, XL6_IR_C_ABI_C_SIZE,
        XL6_IR_C_ABI_C_INT, XL6_IR_C_ABI_RAW_POINTER, XL6_IR_C_ABI_C_BOOL,
        XL6_IR_C_ABI_RUNTIME_XVALUE_PTR
    };
    const struct {
        const char* name;
        XL6_IrTypeId result;
        size_t count;
        XL6_IrTypeId parameters[3];
    } cases[] = {
        { "xl6_xrt_string_byte_len", 3, 1, {1} },
        { "xl6_xrt_string_byte_at", 4, 2, {1, 2} },
        { "xl6_xrt_string_slice", 5, 3, {1, 2, 2} },
        { "xl6_xrt_string_replace", 5, 3, {1, 1, 1} },
        { "xl6_xrt_string_starts_with", 6, 3, {1, 1, 6} },
        { "xl6_xrt_string_ends_with", 6, 3, {1, 1, 6} },
        { "xl6_xrt_string_split", 7, 2, {1, 1} }
    };
    XL6_IrModule module = {0};
    XL6_IrType types[7] = {{0}};
    size_t i;
    module.types.items = types;
    module.types.count = 7;
    for ( i = 0; i < 7; ++i ) {
        types[i].kind = XL6_IR_TYPE_BUILTIN;
        types[i].cAbi = abis[i];
        types[i].ownershipAbi = (i == 0 || i == 6)
            ? XL6_IR_OWNERSHIP_ABI_MANAGED : XL6_IR_OWNERSHIP_ABI_TRIVIAL;
        types[i].nullAbi = i == 6 ? XL6_IR_NULL_ABI_XVALUE_SENTINEL :
            (i == 0 || i == 4) ? XL6_IR_NULL_ABI_POINTER_SENTINEL : XL6_IR_NULL_ABI_NONE;
    }
    for ( i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i ) {
        XL6_IrCallTarget target = {0};
        XL6_IrTypeId parameters[3];
        XL6_IrParameterAbi modes[3];
        XL6_IrCallParameterPhysicalAbi physical[3];
        size_t j;
        for ( j = 0; j < cases[i].count; ++j ) {
            parameters[j] = cases[i].parameters[j];
            modes[j] = parameters[j] == 1 ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE;
            physical[j] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
        }
        target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
        target.cName = (char*)cases[i].name;
        target.runtimeBridge = xl6IrRuntimeStringBridgeKind(target.cName);
        target.returnType = cases[i].result;
        target.parameterCount = cases[i].count;
        target.parameterTypes = parameters;
        target.parameterAbis = modes;
        target.parameterPhysicalAbis = physical;
        target.resultPhysicalAbi = XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
        target.errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        target.variadicAbi = XL6_IR_CALL_VARIADIC_NONE;
        if ( !xl6IrRuntimeStringBridgeTargetMatches(&module, &target) ) return 0;
        --target.parameterCount;
        if ( xl6IrRuntimeStringBridgeTargetMatches(&module, &target) ) return 0;
        ++target.parameterCount;
        modes[0] = XL6_IR_PARAMETER_ABI_VALUE;
        if ( xl6IrRuntimeStringBridgeTargetMatches(&module, &target) ) return 0;
        modes[0] = XL6_IR_PARAMETER_ABI_BORROW;
        target.returnType = 1;
        if ( xl6IrRuntimeStringBridgeTargetMatches(&module, &target) ) return 0;
        target.returnType = cases[i].result;
        target.runtimeBridge = XL6_IR_RUNTIME_BRIDGE_NONE;
        if ( xl6IrRuntimeStringBridgeTargetMatches(&module, &target) ) return 0;
    }
    return xl6IrRuntimeStringBridgeKind("ordinary_foreign_function") == XL6_IR_RUNTIME_BRIDGE_NONE;
}

int main(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_AstNode* root;
    XL6_AstNode* func;
    XL6_AstNode* value;
    int managedOwnershipOk;
    int shortCircuitOk;
    int nonVoidTerminationOk;
    int cleanupMaterializationOk;
    int semanticCleanupOk;
    int stringBridgeOk;
    int ok;

    memset(&diags, 0, sizeof(diags));
    xl6IrModuleInit(&ir);
    root = xl6AstNew(XL6_AST_MODULE, NULL);
    func = xl6AstNew(XL6_AST_FUNC, NULL);
    if ( root == NULL || func == NULL ) return 1;
    func->semanticSymbolId = 11;
    {
        XL6_AstNode* name = xl6AstNewText(XL6_AST_NAME, "answer", 6, NULL);
        name->inferredType = testIntType();
        xl6AstAddChild(func, name);
    }
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        xl6AstAddChild(returns, testIntType());
        xl6AstAddChild(func, returns);
    }
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        value = xl6AstNewText(XL6_AST_INT, "42", 2, NULL);
        value->inferredType = testIntType();
        value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        xl6AstAddChild(ret, value);
        xl6AstAddChild(body, ret);
        xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         testReleasePlan(&ir, XL6_IR_EXIT_POINT_AFTER_SCOPE, NULL, 0) &&
         testReleasePlan(&ir, XL6_IR_EXIT_POINT_BEFORE_NODE, NULL, 0) &&
         xl6IrLowerModule(&ir, &diags) &&
         xl6IrVerifyModule(&ir, &diags) &&
         ir.functionCount == 3 && ir.abi.finalizerFunction == 3 &&
         ir.functions[2].kind == XL6_IR_FUNCTION_MODULE_FINI &&
         ir.functions[1].symbol == 11 &&
         ir.functions[1].parameterCount == 0 &&
         ir.functions[1].blockCount == 1 &&
         ir.functions[1].instructionCount == 2 &&
         ir.instructions[ir.functions[1].firstInstruction].kind == XL6_IR_INSTR_CONST &&
         ir.instructions[ir.functions[1].firstInstruction + 1].kind == XL6_IR_INSTR_RETURN;
    if ( ok ) {
        XL6_IrExitPlan* plan = testUniqueExitPlan(
            &ir, XL6_IR_EXIT_POINT_BEFORE_NODE, XL6_SEM_EXIT_RETURN);
        ok = plan != NULL;
        if (plan != NULL) {
            plan->kind = XL6_SEM_EXIT_THROW;
            ok = !xl6IrVerifyModule(&ir, &diags);
            plan->kind = XL6_SEM_EXIT_RETURN;
            ok = ok && xl6IrVerifyModule(&ir, &diags);
        }
    }
    managedOwnershipOk = testManagedOwnershipIsNotErased();
    shortCircuitOk = testShortCircuitUsesCfg();
    nonVoidTerminationOk = testNonVoidPathMustTerminate();
    cleanupMaterializationOk = testCleanupActionIsMaterialized();
    semanticCleanupOk = testSemanticCleanupPlanUsesLexicalLifo();
    stringBridgeOk = testStringBridgeContracts();
    ok = ok && managedOwnershipOk && shortCircuitOk && nonVoidTerminationOk &&
         cleanupMaterializationOk && semanticCleanupOk && stringBridgeOk;
    if ( !ok ) {
        fprintf(stderr, "XLIR function lowering test failed: diags=%lu funcs=%lu blocks=%lu instructions=%lu "
                "managed=%d short=%d terminate=%d cleanup=%d semanticCleanup=%d stringBridge=%d message=%s\n",
                (unsigned long)diags.count, (unsigned long)ir.functionCount,
                (unsigned long)ir.blockCount, (unsigned long)ir.instructionCount,
                managedOwnershipOk, shortCircuitOk, nonVoidTerminationOk,
                cleanupMaterializationOk, semanticCleanupOk, stringBridgeOk,
                diags.count > 0 && diags.items[0].message != NULL ? diags.items[0].message : "<none>");
        xl6IrModuleFree(&ir);
        xl6AstFree(root);
        xrtFree(diags.items);
        return 1;
    }
    xl6IrModuleFree(&ir);
    xl6AstFree(root);
    xrtFree(diags.items);
    puts("XLIR function lowering test ok");
    return 0;
}
