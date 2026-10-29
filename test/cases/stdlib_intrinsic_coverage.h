/* Test-only exact AST-to-Typed-IR identity map. Injected declarations and
 * textual name occurrences are not source bindings. Integer addresses are
 * observation keys only: no retired AST pointer is dereferenced or retained. */
typedef struct CoverageSourceMember {
    uintptr_t root, node, receiver;
    XL6_AstKind kind;
    int line, column;
    char* spelling;
    size_t argumentCount;
} CoverageSourceMember;
typedef struct CoverageIntrinsicSources {
    CoverageSourceMember* entries;
    size_t count;
    const CoverageSourceMember** origins;
    uintptr_t* nodeAddresses;
    size_t originCount;
} CoverageIntrinsicSources;

static void coverage_capture_members(CoverageIntrinsicSources* sources,
    const XL6_AstNode* node, uintptr_t root)
{
    if (!node) return;
    const char* spelling = NULL;
    uintptr_t receiver = 0;
    size_t arguments = 0;
    if ((node->kind == XL6_AST_MEMBER || node->kind == XL6_AST_SAFE_MEMBER) && node->text) {
        spelling = node->text;
        if (node->childCount) receiver = (uintptr_t)node->children[0];
    } else if (node->kind == XL6_AST_CALL && node->childCount) {
        const XL6_AstNode* callee = node->children[0];
        if (callee && callee->kind == XL6_AST_GENERIC_INSTANT && callee->childCount)
            callee = callee->children[0];
        if (callee) spelling = callee->text;
        if (callee && (callee->kind == XL6_AST_MEMBER || callee->kind == XL6_AST_SAFE_MEMBER) && callee->childCount)
            receiver = (uintptr_t)callee->children[0];
        if (node->childCount > 1 && node->children[1]) arguments = node->children[1]->childCount;
    }
    if (spelling) {
        assert(sources->count < SIZE_MAX / sizeof(*sources->entries));
        sources->entries = xrtRealloc(sources->entries, (sources->count + 1) * sizeof(*sources->entries));
        assert(sources->entries);
        CoverageSourceMember* entry = &sources->entries[sources->count++];
        *entry = (CoverageSourceMember){root, (uintptr_t)node, receiver, node->kind, node->line, node->column,
            xrtStrDupN(spelling, strlen(spelling)), arguments};
        assert(entry->spelling);
    }
    for (size_t i = 0; i < node->childCount; ++i)
        coverage_capture_members(sources, node->children[i], root);
}

static void coverage_bind_members(CoverageIntrinsicSources* sources, const XL6_IrModule* ir,
    const XL6_AstNode* ast, XL6_IrNodeId id, uintptr_t root)
{
    assert(ast && id && id <= ir->nodeCount);
    const XL6_IrNode* node = &ir->nodes[id - 1];
    /* A checked structural correspondence, not a line/name search. Synthetic
     * IR added after the frontend tree never gains a source observation. */
    assert(node->sourceKind == ast->kind && node->childCount == ast->childCount);
    assert(node->span.line == ast->line && node->span.column == ast->column);
    sources->nodeAddresses[id - 1] = (uintptr_t)ast;
    for (size_t i = 0; i < sources->count; ++i) {
        const CoverageSourceMember* entry = &sources->entries[i];
        if (entry->root == root && entry->node == (uintptr_t)ast && entry->kind == ast->kind &&
            entry->line == ast->line && entry->column == ast->column) {
            sources->origins[id - 1] = entry; break;
        }
    }
    for (size_t i = 0; i < ast->childCount; ++i)
        coverage_bind_members(sources, ir, ast->children[i], ir->children[node->firstChild + i], root);
}

static void coverage_bind_module(CoverageIntrinsicSources* sources, const XL6_IrModule* ir,
    const XL6_AstNode* root)
{
    assert(ir->root && ir->root <= ir->nodeCount);
    const XL6_IrNode* node = &ir->nodes[ir->root - 1];
    assert(node->sourceKind == root->kind && node->childCount == root->childCount);
    sources->originCount = ir->nodeCount;
    sources->origins = xrtCalloc(sources->originCount, sizeof(*sources->origins));
    sources->nodeAddresses = xrtCalloc(sources->originCount, sizeof(*sources->nodeAddresses));
    assert(sources->origins && sources->nodeAddresses);
    for (size_t i = 0; i < root->childCount; ++i)
        coverage_bind_members(sources, ir, root->children[i], ir->children[node->firstChild + i],
            (uintptr_t)root->children[i]);
}

static bool coverage_reachable(const XL6_IrModule* ir, XL6_IrNodeId origin)
{
    for (size_t i = 0; i < ir->functionCount; ++i) {
        const XL6_IrFunction* function = &ir->functions[i];
        if (function->reachability == XL6_IR_REACHABILITY_UNREACHABLE) continue;
        assert(function->firstInstruction <= ir->instructionCount &&
            function->instructionCount <= ir->instructionCount - function->firstInstruction);
        for (size_t j = 0; j < function->instructionCount; ++j)
            if (ir->instructions[function->firstInstruction + j].origin == origin) return true;
    }
    return false;
}

static void coverage_dump_intrinsics(const CoverageIntrinsicSources* sources, const XL6_IrModule* ir)
{
    fputs(",\"Intrinsics\":[", stdout);
    for (size_t i = 0; i < ir->intrinsicCallCount; ++i) {
        const XL6_IrIntrinsicCall* call = &ir->intrinsicCalls[i];
        const CoverageSourceMember* source = call->origin && call->origin <= sources->originCount
            ? sources->origins[call->origin - 1] : NULL;
        XL6_IrNodeId receiverNode = call->receiver;
        XL6_IrTypeId receiverType = call->receiverType;
        /* Several closed collection intrinsics store the receiver as operand
         * zero, not in the dedicated resource receiver slot. Bind it only by
         * the exact captured source expression identity, never its type/name. */
        if (!receiverNode && source && source->receiver && call->operandCount) {
            const XL6_IrIntrinsicOperand* first = &ir->intrinsicOperands[call->firstOperand];
            if (first->expression && first->expression <= sources->originCount &&
                sources->nodeAddresses[first->expression - 1] == source->receiver) {
                receiverNode = first->expression; receiverType = first->type;
            }
        }
        const XL6_IrType* receiver = xl6IrTypeGet(&ir->types, receiverType);
        const XL6_IrType* result = xl6IrTypeGet(&ir->types, call->resultType);
        bool receiverMatches = source && (source->receiver
            ? receiverNode && receiverNode <= sources->originCount &&
                sources->nodeAddresses[receiverNode - 1] == source->receiver
            : receiverNode == 0);
        if (i) putchar(',');
        printf("{\"Id\":%zu,\"Origin\":%u,\"SourceCall\":%s,\"InReachableFunction\":%s,\"Spelling\":",
            i, call->origin, source ? "true" : "false", coverage_reachable(ir, call->origin) ? "true" : "false");
        quoted(source ? source->spelling : NULL);
        printf(",\"SourceArgumentCount\":%zu,\"SourceMember\":%s,\"SourceReceiverMatches\":%s,\"Admission\":%u,\"SourceKind\":",
            source ? source->argumentCount : 0, source && source->receiver ? "true" : "false",
            receiverMatches ? "true" : "false", call->admission);
        quoted(source ? xl6AstKindName(source->kind) : NULL);
        fputs(",\"Kind\":", stdout); quoted(coverage_intrinsic_name(call->kind));
        fputs(",\"ResourceOp\":", stdout); quoted(coverage_resource_name(call->runtimeResourceOp));
        fputs(",\"TypedOp\":", stdout); quoted(coverage_typed_name(call->typedCollectionOp));
        fputs(",\"DynamicOp\":", stdout); quoted(coverage_dynamic_name(call->dynamicCollectionOp));
        fputs(",\"ReceiverFamily\":", stdout); quoted(receiver ? receiver->name : NULL);
        fputs(",\"ReceiverType\":", stdout); quoted(receiver ? receiver->displayName : NULL);
        fputs(",\"ResultFamily\":", stdout); quoted(result ? result->name : NULL);
        fputs(",\"ResultType\":", stdout); quoted(result ? result->displayName : NULL);
        printf(",\"ErrorAbi\":%u,\"ResultAbi\":%u,\"OperandTypes\":[", call->errorAbi, call->resultAbi);
        assert(call->firstOperand <= ir->intrinsicOperandCount &&
            call->operandCount <= ir->intrinsicOperandCount - call->firstOperand);
        for (size_t p = 0; p < call->operandCount; ++p) {
            const XL6_IrIntrinsicOperand* operand = &ir->intrinsicOperands[call->firstOperand + p];
            if (p) putchar(',');
            quoted(xl6IrTypeGet(&ir->types, operand->type)->displayName);
        }
        fputs("]}", stdout);
    }
    putchar(']');
}

static void coverage_dump_properties(const CoverageIntrinsicSources* sources, const XL6_IrModule* ir)
{
    fputs(",\"Properties\":[", stdout); size_t count = 0;
    for (size_t i = 0; i < sources->originCount; ++i) {
        const CoverageSourceMember* source = sources->origins[i];
        if (!source || !source->receiver || (source->kind != XL6_AST_MEMBER && source->kind != XL6_AST_SAFE_MEMBER)) continue;
        const XL6_IrNode* node = &ir->nodes[i];
        if (!node->childCount) continue;
        if (node->parent && node->parent <= ir->nodeCount) {
            const XL6_IrNode* parent = &ir->nodes[node->parent - 1];
            if (parent->sourceKind == XL6_AST_CALL && parent->childCount &&
                ir->children[parent->firstChild] == i + 1) continue;
        }
        XL6_IrNodeId receiverId = ir->children[node->firstChild];
        if (!receiverId || receiverId > sources->originCount || sources->nodeAddresses[receiverId - 1] != source->receiver) continue;
        const XL6_IrType* receiver = xl6IrTypeGet(&ir->types, ir->nodes[receiverId - 1].storageType);
        if (count++) putchar(',');
        printf("{\"Origin\":%zu,\"SourceCall\":true,\"SourceMember\":true,\"SourceReceiverMatches\":true,"
            "\"VerifiedLoweredModule\":true,\"InReachableFunction\":%s,\"MemberAbi\":%u,\"SourceArgumentCount\":0,\"Spelling\":",
            i + 1, coverage_reachable(ir, (XL6_IrNodeId)(i + 1)) ? "true" : "false", node->memberAbi);
        quoted(source->spelling); fputs(",\"SourceKind\":", stdout); quoted(xl6AstKindName(source->kind));
        fputs(",\"ReceiverFamily\":", stdout); quoted(receiver ? receiver->name : NULL);
        fputs(",\"ReceiverType\":", stdout); quoted(receiver ? receiver->displayName : NULL);
        fputs(",\"Kind\":\"member-projection\"}", stdout);
    }
    putchar(']');
}

static void coverage_free_sources(CoverageIntrinsicSources* sources)
{
    for (size_t i = 0; i < sources->count; ++i) xrtFree(sources->entries[i].spelling);
    xrtFree(sources->entries); xrtFree(sources->origins); xrtFree(sources->nodeAddresses);
    *sources = (CoverageIntrinsicSources){0};
}
