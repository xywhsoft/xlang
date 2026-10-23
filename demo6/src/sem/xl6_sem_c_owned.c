/* Included by xl6_sem.c after callable/expression helpers. */
static int xl6SemCheckCOwnedTake(XL6_SemContext*,XL6_AstNode*);
static int xl6SemCOwnedResult(XL6_SemContext*,XL6_AstNode*,const char*);
static int xl6SemCOwnedCompare(XL6_SemContext* ctx,XL6_AstNode* node)
{
    if(node->childCount!=2 || !node->text ||
        (!xl6TextEquals(node->text,"==") && !xl6TextEquals(node->text,"!=") &&
         !xl6TextEquals(node->text,"<") && !xl6TextEquals(node->text,"<=") &&
         !xl6TextEquals(node->text,">") && !xl6TextEquals(node->text,">=")))return -1;
    XL6_AstNode* types[2]={xl6SemInferAutoTypeNode(ctx,node->children[0]),xl6SemInferAutoTypeNode(ctx,node->children[1])};
    const char* a=types[0] ? xl6COwnedBaseName(types[0]->text) : NULL;
    const char* b=types[1] ? xl6COwnedBaseName(types[1]->text) : NULL;
    int owned=a || b;
    int same=owned && types[0] && types[1] &&
        xl6TextEquals(a ? a : xl6SemResolveAliasName(ctx,types[0]->text),
                     b ? b : xl6SemResolveAliasName(ctx,types[1]->text));
    xl6AstFree(types[0]);xl6AstFree(types[1]);
    if(!owned)return -1;
    if(!same) {xl6SemErrorNode(ctx,node,"C owner comparisons require matching text or binary operands");return 0;}
    XL6_AstNode* callee=xl6AstNewText(XL6_AST_NAME,"__xl_c_owned_compare",20,NULL);
    XL6_AstNode* args=xl6AstNew(XL6_AST_LIST,NULL);
    XL6_AstNode* arguments[2]={xl6AstNew(XL6_AST_ARG,NULL),xl6AstNew(XL6_AST_ARG,NULL)};
    if(!callee || !args || !arguments[0] || !arguments[1]) {
        xl6AstFree(callee);xl6AstFree(args);xl6AstFree(arguments[0]);xl6AstFree(arguments[1]);return 0;
    }
    for(size_t i=0;i<2;++i) {
        xl6AstAddChild(arguments[i],node->children[i]);xl6AstAddChild(args,arguments[i]);
    }
    node->kind=XL6_AST_CALL;node->children[0]=callee;node->children[1]=args;
    xl6SemSetCallPlan(node,XL6_SEM_CALL_C_OWNED_COMPARE,NULL);
    return xl6SemCOwnedResult(ctx,node,"bool");
}
static int xl6SemCOwnedResult(XL6_SemContext* ctx,XL6_AstNode* node,const char* name)
{
    XL6_AstNode fake={0}; fake.kind=XL6_AST_NAME; fake.text=(char*)name;
    xl6AstFree(node->inferredType); node->inferredType=xl6SemNewTypeFromName(&fake);
    if(!node->inferredType)return 0;
    xl6SemBindInferredTypeIdentity(ctx,node->inferredType); return 1;
}
static int xl6SemCOwnedBaseForExpr(XL6_SemContext* ctx,XL6_AstNode* expression,const char** base)
{
    XL6_AstNode* actual=xl6SemInferAutoTypeNode(ctx,expression);
    *base=actual ? xl6COwnedBaseName(actual->text) : NULL;
    xl6AstFree(actual);
    if(!*base && expression && expression->kind==XL6_AST_CALL) {
        int take=xl6SemCheckCOwnedTake(ctx,expression);
        if(!take)return 0;
        if(take>0)*base=xl6COwnedBaseName(expression->inferredType->text);
    }
    return 1;
}
static int xl6SemCheckCOwnedCopy(XL6_SemContext* ctx,XL6_AstNode* node)
{
    XL6_AstNode* callee=node->childCount ? node->children[0] : NULL;
    XL6_AstNode* arg=xl6SemCallArgCount(node)==1 ? xl6SemCallArgExprAt(node,0) : NULL;
    if(!callee || callee->kind!=XL6_AST_NAME || !arg ||
        (!xl6TextEquals(callee->text,"string") && !xl6TextEquals(callee->text,"bytes")))return -1;
    const char* base=NULL;
    if(!xl6SemCOwnedBaseForExpr(ctx,arg,&base))return 0;
    if(!base)return -1;
    if(!xl6TextEquals(base,callee->text)) {
        xl6SemErrorNode(ctx,node,"C text and binary carriers only convert to their matching base type");return 0;
    }
    if(!xl6SemCheckExpr(ctx,arg))return 0;
    xl6SemSetCallPlan(node,XL6_SEM_CALL_C_OWNED_COPY,NULL);
    return xl6SemCOwnedResult(ctx,node,base);
}
static int xl6SemNormalizeCOwnedExpr(XL6_SemContext* ctx,XL6_AstNode** place)
{
    XL6_AstNode* expression=*place;
    const char* base=NULL;
    if(!xl6SemCOwnedBaseForExpr(ctx,expression,&base))return 0;
    if(!base)return 1;
    XL6_AstNode* name=xl6AstNewText(XL6_AST_NAME,base,strlen(base),NULL);
    XL6_AstNode* args=xl6AstNew(XL6_AST_LIST,NULL);
    XL6_AstNode* arg=xl6AstNew(XL6_AST_ARG,NULL);
    XL6_AstNode* call=name && args ? xl6AstNewCall(name,args,NULL) : NULL;
    if(!call || !arg) {
        if(call)xl6AstFree(call);else{xl6AstFree(name);xl6AstFree(args);}
        xl6AstFree(arg);return 0;
    }
    call->line=expression->line;call->column=expression->column;
    xl6AstAddChild(arg,expression);xl6AstAddChild(args,arg);*place=call;
    if(!strcmp(base,"string")) {
        if(!xl6SemCheckExpr(ctx,expression))return 0;
        xl6SemSetCallPlan(call,XL6_SEM_CALL_C_OWNED_VIEW,NULL);
        return xl6SemCOwnedResult(ctx,call,base);
    }
    return xl6SemCheckCOwnedCopy(ctx,call)==1;
}
static int xl6SemCheckCOwnedRead(XL6_SemContext* ctx,XL6_AstNode* node)
{
    if(!node->childCount)return -1;
    XL6_AstNode* receiver=node->children[0];
    const char* base=NULL;
    if(!xl6SemCOwnedBaseForExpr(ctx,receiver,&base))return 0;
    if(!base)return -1;
    int index=node->kind==XL6_AST_INDEX;
    if(!index && !xl6TextEquals(node->text,"length")) {
        xl6SemErrorNode(ctx,node,"C owner has no such property; materialize string/bytes for other operations");return 0;
    }
    if(!xl6SemCheckExpr(ctx,receiver))return 0;
    XL6_AstNode* actual=xl6SemInferAutoTypeNode(ctx,receiver);
    int stillOwned=actual && xl6COwnedBaseName(actual->text)!=NULL;
    xl6AstFree(actual);
    if(!stillOwned)return -1;
    if(index && (xl6SemIndexArgCount(node)!=1 ||
        !xl6SemCheckExpr(ctx,xl6SemIndexArgAt(node,0)) ||
        !xl6SemIsIntegerTypeName(xl6SemExprStaticTypeName(ctx,xl6SemIndexArgAt(node,0))))) {
        xl6SemErrorNode(ctx,node,"C owner index requires one integer");return 0;
    }
    xl6SemSetCallPlan(node,XL6_SEM_CALL_C_OWNED_READ,NULL);
    return xl6SemCOwnedResult(ctx,node,!index ? "int" : !strcmp(base,"string") ? "char" : "uint8");
}

static int xl6SemCheckCOwnedTake(XL6_SemContext* ctx, XL6_AstNode* call)
{
    XL6_AstNode* callee = call->childCount ? call->children[0] : NULL;
    XL6_AstNode* receiver = callee && callee->childCount ? callee->children[0] : NULL;
    if (!callee || callee->kind != XL6_AST_MEMBER || !xl6TextEquals(callee->text,"take") ||
        !receiver || receiver->kind != XL6_AST_NAME || !xl6COwnedBaseName(receiver->text)) return -1;
    int binary = xl6TextEquals(receiver->text,"cbytes");
    if(!xl6SemCallArgsArePositional(ctx,call,"C owner take requires positional arguments"))return 0;
    size_t count = binary ? 3u : 2u;
    if ((size_t)xl6SemCallArgCount(call) != count) {
        xl6SemErrorNode(ctx,call,"C owner take requires pointer, optional byte length, and named C releaser");
        return 0;
    }
    XL6_AstNode* pointer = xl6SemCallArgExprAt(call,0);
    XL6_AstNode* length = binary ? xl6SemCallArgExprAt(call,1) : NULL;
    XL6_AstNode* releaser = xl6SemCallArgExprAt(call,count-1);
    int ok = xl6SemCheckExpr(ctx,pointer);
    if (!xl6TextEquals(xl6SemExprStaticTypeName(ctx,pointer),"point") && pointer->kind!=XL6_AST_NULL) {
        xl6SemErrorNode(ctx,pointer,"C owner take requires a raw point"); ok=0;
    }
    if (length && (!xl6SemCheckExpr(ctx,length) ||
        !xl6SemIsIntegerTypeName(xl6SemExprStaticTypeName(ctx,length)))) {
        xl6SemErrorNode(ctx,length,"C bytes length must be an integer"); ok=0;
    }
    XL6_AstNode* selected = NULL;
    XL6_Symbol* releaseBinding=releaser && releaser->kind==XL6_AST_NAME ? xl6ScopeFind(ctx->scope,releaser->text) : NULL;
    if(!releaseBinding || releaseBinding->kind!=XL6_SYM_FUNC) {
        xl6SemErrorNode(ctx,releaser,"C owner releaser must be a named extern function");return 0;
    }
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
