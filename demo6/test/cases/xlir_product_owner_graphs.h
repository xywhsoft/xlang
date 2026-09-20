/* Included after the shared Graph helpers. These are pass-level CFG tests;
 * productMutations below additionally exercises real module/C/header admission. */
static void productAcquire(Graph* g, XL6_IrTypeId productType)
{
    emit(g, XL6_IR_INSTR_PRODUCT_MAKE, 1, 0)->type = productType;
}

static void productDrop(Graph* g, size_t field)
{
    emit(g, XL6_IR_INSTR_PRODUCT_DROP, 0, 1)->elementIndex = field;
}

static void productBranch(Graph* g)
{
    XL6_IrInstruction* branch = emit(g, XL6_IR_INSTR_BRANCH, 0, 0);
    branch->trueTarget = 2; branch->falseTarget = 3;
}

static void productGraphs(const XL6_IrModule* source, XL6_IrTypeId productType,
    XL6_IrTypeId stringType)
{
    Graph g; XL6_IrInstruction* instruction;
    size_t before = graphChecks;
    /* No use and partial use must not lose the undisposed fields. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType);
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    g.instructions[1].kind = XL6_IR_INSTR_PRODUCT_DROP;
    g.instructions[1].elementIndex = 0; g.instructions[1].operandCount = 1;
    g.operands[g.ir.operandCount++] = 1; check(&g, 0);
    productDrop(&g, 1); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 1);
    g.instructions[2].elementIndex = 0; check(&g, 0);

    /* Two different fields, each consumed on only one branch: the old global
     * field counts were both exactly one, yet every execution leaked. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType); productBranch(&g);
    block(&g); productDrop(&g, 0); jump(&g, 4);
    block(&g); productDrop(&g, 1); jump(&g, 4);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    g.instructions[1].trueTarget = 3; g.instructions[1].falseTarget = 2; check(&g, 0);

    /* Conversely, independent complete decompositions on exclusive branches
     * are valid even though every field has two static consumers. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType); productBranch(&g);
    block(&g); productDrop(&g, 0); productDrop(&g, 1); jump(&g, 4);
    block(&g); productDrop(&g, 1); productDrop(&g, 0); jump(&g, 4);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 1);

    /* Whole transfer on one path, per-field consumption on the other. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType); productBranch(&g);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 1);
    block(&g); productDrop(&g, 0); productDrop(&g, 1);
    emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 1);
    /* Whole transfers in mutually exclusive blocks are also valid. */
    g.instructions[3].kind = XL6_IR_INSTR_RETURN;
    g.blocks[2].instructionCount = 1; g.ir.instructionCount = 4;
    g.function.instructionCount = 4; check(&g, 1);

    /* Partial consumption followed by whole transfer is never intact. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType);
    productDrop(&g, 0); emit(&g, XL6_IR_INSTR_RETURN, 0, 1); check(&g, 0);
    /* Likewise a second whole consume on the same path. */
    g.instructions[1].kind = XL6_IR_INSTR_PRODUCT_STORE_MOVE; check(&g, 0);

    /* Reacquisition checks every remaining field, including infinite loops. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType);
    productDrop(&g, 0); jump(&g, 1); check(&g, 0);
    g.instructions[2].kind = XL6_IR_INSTR_PRODUCT_DROP; g.instructions[2].trueTarget = 0;
    g.instructions[2].elementIndex = 1; g.instructions[2].operandCount = 1;
    g.operands[g.ir.operandCount++] = 1; jump(&g, 1); check(&g, 1);
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType); jump(&g, 2);
    block(&g); productDrop(&g, 0); productDrop(&g, 1); jump(&g, 2); check(&g, 0);

    /* A borrowed product requires the exact aggregate detach before TAKE. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_LOAD, 1, 0)->type = productType;
    productDrop(&g, 0); productDrop(&g, 1); check(&g, 0);
    g.instructions[1].kind = XL6_IR_INSTR_MOVE;
    productDrop(&g, 0); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 1);
    /* Detaching again after only one field is consumed is also invalid. */
    g.instructions[3].kind = XL6_IR_INSTR_MOVE; check(&g, 0);

    /* TAKE creates a separate scalar owner, not a borrow of the carrier. */
    begin(&g, source, stringType); block(&g); productAcquire(&g, productType);
    emit(&g, XL6_IR_INSTR_PRODUCT_TAKE, 2, 1)->elementIndex = 0;
    productDrop(&g, 1); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    emit(&g, XL6_IR_INSTR_STORE, 0, 2); check(&g, 1);

    /* An empty failed result has no successful field to TAKE. It may be
     * ignored or null-safely destroyed, but success still owes every field. */
    begin(&g, source, stringType); block(&g);
    emit(&g, XL6_IR_INSTR_CLOSURE_CALL, 1, 0)->type = productType;
    instruction = emit(&g, XL6_IR_INSTR_ERROR_TEST, 0, 1);
    instruction->trueTarget = 2; instruction->falseTarget = 3;
    block(&g); productDrop(&g, 0); productDrop(&g, 1); jump(&g, 3);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 1);
    g.instructions[1].kind = XL6_IR_INSTR_BRANCH; check(&g, 0);
    g.instructions[1].kind = XL6_IR_INSTR_ERROR_TEST;
    g.instructions[4].trueTarget = 4;
    g.instructions[5].kind = XL6_IR_INSTR_PRODUCT_TAKE;
    g.instructions[5].result = 2; g.instructions[5].elementIndex = 0;
    g.instructions[5].operandCount = 1; g.operands[g.ir.operandCount++] = 1;
    emit(&g, XL6_IR_INSTR_STORE, 0, 2); productDrop(&g, 1); jump(&g, 4);
    block(&g); emit(&g, XL6_IR_INSTR_RETURN, 0, 0); check(&g, 0);
    g.instructions[5].kind = XL6_IR_INSTR_PRODUCT_DROP; g.instructions[5].result = 0;
    g.instructions[6].kind = XL6_IR_INSTR_DISCARD; g.instructions[6].operandCount = 0; check(&g, 1);
    assert(graphChecks - before == 23);
}

static void productMutations(void)
{
    XL6_ParseResult parse; XL6_IrModule ir; size_t i, count = 0;
    XL6_IrTypeId productType = 0, stringType = 0;
    xl6ParseResultInit(&parse); xl6IrModuleInit(&ir);
    assert(xl6ParseFile("test/cases/xlir_managed_product_result.xl", &parse));
    assert(xl6StdlibExpandModule(parse.root, &parse.diags));
    assert(xl6SemCheckModule(parse.root, &parse.diags, "product_owner"));
    assert(xl6IrBuildModuleFromAst(&ir, parse.root, &parse.diags));
    assert(xl6IrLowerModule(&ir, &parse.diags) && xl6IrVerifyModule(&ir, &parse.diags));
    for (i = 0; i < ir.instructionCount; ++i) {
        XL6_IrInstruction* instruction = &ir.instructions[i];
        size_t saved;
        if (instruction->kind != XL6_IR_INSTR_PRODUCT_TAKE &&
            instruction->kind != XL6_IR_INSTR_PRODUCT_DROP) continue;
        productType = definition(&ir, ir.operands[instruction->firstOperand])->type;
        stringType = instruction->type;
        assert(xl6IrInstructionOperandOwnership(&ir, instruction, 0) == XL6_IR_OPERAND_OWNERSHIP_CONSUME_FIELD);
        saved = instruction->elementIndex; instruction->elementIndex = 1 - saved;
        reject(&ir, "duplicate product field leaves another field unconsumed", 1);
        instruction->elementIndex = saved; ++count;
    }
    assert(count >= 4 && productType != 0 && stringType != 0);
    productGraphs(&ir, productType, stringType);
    assert(xl6IrVerifyModule(&ir, &parse.diags));
    xl6IrModuleFree(&ir); xl6ParseResultFree(&parse);
    printf("product owner: %llu real field mutations, 23 CFG checks\n", (unsigned long long)count);
}
