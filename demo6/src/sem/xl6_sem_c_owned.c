/* Included by xl6_sem.c after callable/expression helpers. */
static int xl6SemCheckCOwnedTake(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* callee = call->childCount ? call->children[0] : NULL;
    XL6_AstNode* receiver = callee && callee->childCount ? callee->children[0] : NULL;
    if (!callee || callee->kind != XL6_AST_MEMBER || !xl6TextEquals(callee->text,"take") ||
        !receiver || receiver->kind != XL6_AST_NAME || !xl6COwnedBaseName(receiver->text)) return -1;
    int binary = xl6TextEquals(receiver->text,"cbytes");
    size_t count = binary ? 3u : 2u;
    if ((size_t)xl6SemCallArgCount(call) != count) {
        xl6SemErrorNode(ctx,call,"C owner take requires pointer, optional byte length, and named C releaser");
        return 0;
    }
    XL6_AstNode* pointer = xl6SemCallArgExprAt(call,0);
    XL6_AstNode* length = binary ? xl6SemCallArgExprAt(call,1) : NULL;
    XL6_AstNode* releaser = xl6SemCallArgExprAt(call,count-1);
    int ok = xl6SemCheckExpr(ctx,pointer);
    if (!xl6TextEquals(xl6SemExprStaticTypeName(ctx,pointer),"point")) {
        xl6SemErrorNode(ctx,pointer,"C owner take requires a raw point"); ok=0;
    }
    if (length && (!xl6SemCheckExpr(ctx,length) ||
        !xl6SemIsIntegerTypeName(xl6SemExprStaticTypeName(ctx,length)))) {
        xl6SemErrorNode(ctx,length,"C bytes length must be an integer"); ok=0;
    }
    XL6_AstNode* selected = NULL;
    for (size_t i=0; releaser && releaser->kind==XL6_AST_NAME && i<ctx->root->childCount; ++i) {
        XL6_AstNode* candidate=ctx->root->children[i];
        if (candidate->kind!=XL6_AST_EXTERN_FUNC ||
            !xl6TextEquals(xl6SemFuncName(candidate),releaser->text)) continue;
        const XL6_AstNode* params=xl6SemCallableParams(candidate);
        const XL6_AstNode* returns=xl6SemCallableReturns(candidate);
        const XL6_AstNode* param=params && params->childCount==1 ? params->children[0] : NULL;
        const XL6_AstNode* type=param && param->childCount ? param->children[0] : NULL;
        if (!(candidate->flags & XL6_AST_FLAG_FOREIGN_NO_RAISE) || !type ||
            !xl6TextEquals(type->text,"point") || type->childCount ||
            (returns && returns->childCount) ||
            (param->flags & (XL6_AST_FLAG_BYREF|XL6_AST_FLAG_OUT|XL6_AST_FLAG_C_CALLBACK))) continue;
        if (selected) { selected=NULL; break; }
        selected=candidate;
    }
    if (!selected) {
        xl6SemErrorNode(ctx,releaser,"C owner releaser must be an unambiguous extern @no_raise func(point) with no result");
        return 0;
    }
    /* Releaser is declaration metadata, never an allocated language closure. */
    xl6SemSetCallPlan(call,XL6_SEM_CALL_C_OWNED_TAKE,selected);
    XL6_AstNode fake=*receiver;
    fake.text=receiver->text;
    xl6AstFree(call->inferredType);
    call->inferredType=xl6SemNewTypeFromName(&fake);
    if (!call->inferredType) return 0;
    xl6SemBindInferredTypeIdentity(ctx,call->inferredType);
    return ok;
}

static int xl6SemValidateCOwnedTypes(XL6_SemContext* ctx,XL6_AstNode* node,XL6_AstNode* parent)
{
    int ok=1;
    if (node->kind==XL6_AST_TYPE && xl6COwnedBaseName(node->text) &&
        (!parent || (parent->kind!=XL6_AST_VAR_DECL && parent->kind!=XL6_AST_CONST_DECL) ||
         xl6SemVarDeclTypeNode(parent)!=node)) {
        xl6SemErrorNode(ctx,node,"C owners are local-only; use string/bytes for parameters, results, fields and generic arguments");
        ok=0;
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xl6SemValidateCOwnedTypes(ctx,node->children[i],node))ok=0;
    return ok;
}
