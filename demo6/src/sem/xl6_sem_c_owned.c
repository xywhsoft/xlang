/* Included by xl6_sem.c after callable/expression helpers. */
static const XL6_AstNode* xl6SemCOwnedElement(const XL6_AstNode* type)
{ const XL6_AstNode* inner=xl6SemNullableInnerType(type);return inner ? inner : type; }
static const char* xl6SemCOwnedTypeBase(const XL6_AstNode* type)
{ const XL6_AstNode* element=xl6SemCOwnedElement(type);return element ? xl6COwnedBaseName(element->text) : NULL; }
static const XL6_AstNode* xl6SemCOwnedResolvedElement(XL6_SemContext* ctx,const XL6_AstNode* type)
{ return xl6SemResolveAliasType(ctx,xl6SemCOwnedElement(xl6SemResolveAliasType(ctx,type))); }
/* Global storage has the ordinary value ABI, including inferred declarations.
 * Change only the receiving type; the initializer keeps its C owner until the
 * normal copy succeeds (or unwinds on failure). */
static int xl6SemCOwnedGlobalType(XL6_SemContext* ctx,XL6_AstNode* type)
{
    XL6_AstNode* element=(XL6_AstNode*)xl6SemCOwnedElement(type);
    const char* base=element ? xl6COwnedBaseName(element->text) : NULL;
    if(!base)return 1;
    char* text=xrtStrDup(base);
    if(!text)return 0;
    xrtFree(element->text);element->text=text;
    element->flags &= ~XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
    xl6SemBindInferredTypeIdentity(ctx,type);
    return 1;
}
/* Result covariance extends through returned callables. Their input signature
 * is unchanged; each returned closure gets its own ordinary-result adapter. */
static int xl6SemCOwnedResultHasOwnerDepth(XL6_SemContext* ctx,const XL6_AstNode* type,unsigned depth)
{
    if(depth>32)return 0;
    type=xl6SemCOwnedResolvedElement(ctx,type);
    if(!type)return 0;
    if(xl6COwnedBaseName(type->text))return 1;
    if(xl6TextEquals(type->text,"function") && type->childCount==2)
        for(size_t r=0;r<type->children[1]->childCount;++r)
            if(xl6SemCOwnedResultHasOwnerDepth(ctx,type->children[1]->children[r],depth+1))return 1;
    return 0;
}
static int xl6SemCOwnedResultHasOwner(XL6_SemContext* ctx,const XL6_AstNode* type)
{ return xl6SemCOwnedResultHasOwnerDepth(ctx,type,0); }
static int xl6SemCOwnedOrdinaryResultTypeDepth(XL6_SemContext* ctx,XL6_AstNode** place,unsigned depth)
{
    if(depth>32){xl6SemErrorNode(ctx,*place,"C-owned result adaptation exceeds callable nesting limit");return 0;}
    const XL6_AstNode* resolved=xl6SemResolveAliasType(ctx,*place);
    if(resolved!=*place) {
        XL6_AstNode* copy=xl6SemCloneTypeNode(resolved);
        if(!copy)return 0;
        xl6AstFree(*place);*place=copy;
    }
    XL6_AstNode* type=*place;
    const XL6_AstNode* inner=xl6SemNullableInnerType(type);
    if(inner) {
        if(!xl6SemCOwnedOrdinaryResultTypeDepth(ctx,&type->children[0]->children[0],depth+1))return 0;
    } else if(xl6TextEquals(type->text,"function") && type->childCount==2) {
        for(size_t r=0;r<type->children[1]->childCount;++r)
            if(!xl6SemCOwnedOrdinaryResultTypeDepth(ctx,&type->children[1]->children[r],depth+1))return 0;
    } else return xl6SemCOwnedGlobalType(ctx,type);
    xl6SemBindInferredTypeIdentity(ctx,type);
    return 1;
}
static int xl6SemCOwnedOrdinaryResultType(XL6_SemContext* ctx,XL6_AstNode** place)
{ return xl6SemCOwnedOrdinaryResultTypeDepth(ctx,place,0); }
static int xl6SemCOwnedTypeMatchCost(const XL6_AstNode* actual,const XL6_AstNode* target)
{
    if(!actual || !target)return -1;
    const XL6_AstNode* a=xl6SemCOwnedElement(actual),*b=xl6SemCOwnedElement(target);
    int sourceOptional=xl6SemNullableInnerType(actual)!=NULL,targetOptional=xl6SemNullableInnerType(target)!=NULL;
    if(sourceOptional && !targetOptional)return -1;
    const char* from=xl6COwnedBaseName(a->text),*to=xl6COwnedBaseName(b->text);
    if(!from && !to)return -1;
    if(xl6TextEquals(a->text,b->text))return sourceOptional==targetOptional ? 0 : 1;
    if(to && xl6TextEquals(a->text,to))return 1;
    if(from && xl6TextEquals(from,b->text))return 10;
    return -1;
}
static int xl6SemCheckCOwnedTake(XL6_SemContext*,XL6_AstNode*);
/* The C owner ABI takes signed 64-bit lengths. Keep the checked conversion
 * in the typed graph, so overflow follows the already anchored take cleanup. */
static int xl6SemCOwnedSignedLength(XL6_SemContext* ctx,XL6_AstNode** place)
{
    XL6_AstNode* value=*place;
    const char* type=xl6SemResolveAliasName(ctx,xl6SemExprStaticTypeName(ctx,value));
    XL6_SemIntTypeInfo info;
    if(!xl6SemIntTypeInfo(type,&info) || info.bits<64 || info.isSigned)return 1;
    XL6_AstNode* name=xl6AstNewText(XL6_AST_NAME,"int",3,NULL);
    XL6_AstNode* args=xl6AstNew(XL6_AST_LIST,NULL);
    XL6_AstNode* arg=xl6AstNew(XL6_AST_ARG,NULL);
    if(!name || !args || !arg){xl6AstFree(name);xl6AstFree(args);xl6AstFree(arg);return 0;}
    XL6_AstNode* call=xl6AstNewCall(name,args,NULL);
    if(!call){xl6AstFree(name);xl6AstFree(args);xl6AstFree(arg);return 0;}
    call->line=value->line;call->column=value->column;xl6AstCopySource(call,value);
    xl6AstAddChild(arg,value);xl6AstAddChild(args,arg);*place=call;
    return xl6SemCheckExpr(ctx,call);
}
static int xl6SemCOwnedResult(XL6_SemContext*,XL6_AstNode*,const char*);
static int xl6SemCOwnedCompare(XL6_SemContext* ctx,XL6_AstNode* node)
{
    if(node->childCount!=2 || !node->text ||
        (!xl6TextEquals(node->text,"+") && !xl6TextEquals(node->text,"==") && !xl6TextEquals(node->text,"!=") &&
         !xl6TextEquals(node->text,"<") && !xl6TextEquals(node->text,"<=") &&
         !xl6TextEquals(node->text,">") && !xl6TextEquals(node->text,">=")))return -1;
    if(node->children[0]->kind==XL6_AST_NULL || node->children[1]->kind==XL6_AST_NULL)return -1;
    XL6_AstNode* types[2]={xl6SemInferAutoTypeNode(ctx,node->children[0]),xl6SemInferAutoTypeNode(ctx,node->children[1])};
    const XL6_AstNode* elements[2]={xl6SemCOwnedElement(types[0]),xl6SemCOwnedElement(types[1])};
    if((elements[0] && xl6TextEquals(elements[0]->text,"null")) ||
       (elements[1] && xl6TextEquals(elements[1]->text,"null"))) {xl6AstFree(types[0]);xl6AstFree(types[1]);return -1;}
    int nullable=(types[0] && xl6SemNullableInnerType(types[0])) || (types[1] && xl6SemNullableInnerType(types[1]));
    const char* a=xl6SemCOwnedTypeBase(types[0]);
    const char* b=xl6SemCOwnedTypeBase(types[1]);
    int owned=a || b;
    int concat=xl6TextEquals(node->text,"+");
    int same=owned && types[0] && types[1] &&
        xl6TextEquals(a ? a : xl6SemResolveAliasName(ctx,elements[0]->text),
                     b ? b : xl6SemResolveAliasName(ctx,elements[1]->text));
    int sameOptional=(xl6SemNullableInnerType(types[0])!=NULL)==(xl6SemNullableInnerType(types[1])!=NULL);
    xl6AstFree(types[0]);xl6AstFree(types[1]);
    if(nullable && !sameOptional)return -1;
    if(nullable && !xl6TextEquals(node->text,"==") && !xl6TextEquals(node->text,"!="))return -1;
    if(!owned || (concat && ((a && strcmp(a,"bytes")) || (b && strcmp(b,"bytes")))))return -1;
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
    xl6SemSetCallPlan(node,concat ? XL6_SEM_CALL_C_OWNED_CONCAT : XL6_SEM_CALL_C_OWNED_COMPARE,NULL);
    return xl6SemCOwnedResult(ctx,node,concat ? "cbytes" : "bool");
}
static int xl6SemCOwnedResult(XL6_SemContext* ctx,XL6_AstNode* node,const char* name)
{
    XL6_AstNode fake={0}; fake.kind=XL6_AST_NAME; fake.text=(char*)name;
    xl6AstFree(node->inferredType); node->inferredType=xl6SemNewTypeFromName(&fake);
    if(!node->inferredType)return 0;
    xl6SemBindInferredTypeIdentity(ctx,node->inferredType); return 1;
}
static int xl6SemCOwnedNormalizedResult(XL6_SemContext* ctx,XL6_AstNode* call,const char* base)
{
    XL6_AstNode* argument=xl6SemCallArgExprAt(call,0);
    XL6_AstNode* actual=argument ? xl6SemInferAutoTypeNode(ctx,argument) : NULL;
    int optional=xl6SemNullableInnerType(actual)!=NULL;
    xl6AstFree(actual);
    if(!xl6SemCOwnedResult(ctx,call,base))return 0;
    if(optional) {
        call->inferredType=xl6SemLiftNullableType(call->inferredType,call);
        if(!call->inferredType)return 0;
        xl6SemBindInferredTypeIdentity(ctx,call->inferredType);
    }
    return 1;
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
    if(!base && (node->flags & XL6_AST_FLAG_C_OWNED_SPECIALIZATION)) {
        XL6_AstNode* actual=xl6SemInferAutoTypeNode(ctx,arg);
        base=xl6SemCOwnedTypeBase(actual);xl6AstFree(actual);
    }
    if(!base)return -1;
    if(!xl6TextEquals(base,callee->text)) {
        xl6SemErrorNode(ctx,node,"C text and binary carriers only convert to their matching base type");return 0;
    }
    if(!xl6SemCheckExpr(ctx,arg))return 0;
    xl6SemSetCallPlan(node,XL6_SEM_CALL_C_OWNED_COPY,NULL);
    return xl6SemCOwnedNormalizedResult(ctx,node,base);
}
static int xl6SemNormalizeCOwnedExpr(XL6_SemContext* ctx,XL6_AstNode** place)
{
    XL6_AstNode* expression=*place;
    const char* base=NULL;
    if(!xl6SemCOwnedBaseForExpr(ctx,expression,&base))return 0;
    if(!base) {
        XL6_AstNode* actual=xl6SemInferAutoTypeNode(ctx,expression);
        base=xl6SemCOwnedTypeBase(actual);xl6AstFree(actual);
    }
    if(!base)return 1;
    XL6_AstNode* name=xl6AstNewText(XL6_AST_NAME,base,strlen(base),NULL);
    XL6_AstNode* args=xl6AstNew(XL6_AST_LIST,NULL);
    XL6_AstNode* arg=xl6AstNew(XL6_AST_ARG,NULL);
    XL6_AstNode* call=name && args ? xl6AstNewCall(name,args,NULL) : NULL;
    if(!call || !arg) {
        if(call)xl6AstFree(call);else{xl6AstFree(name);xl6AstFree(args);}
        xl6AstFree(arg);return 0;
    }
    call->line=expression->line;call->column=expression->column;call->flags|=XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
    xl6AstAddChild(arg,expression);xl6AstAddChild(args,arg);*place=call;
    if(!strcmp(base,"string")) {
        if(!xl6SemCheckExpr(ctx,expression))return 0;
        xl6SemSetCallPlan(call,XL6_SEM_CALL_C_OWNED_VIEW,NULL);
        return xl6SemCOwnedNormalizedResult(ctx,call,base);
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
    size_t count=(size_t)xl6SemCallArgCount(call);
    if ((binary && count!=3) || (!binary && count!=2 && count!=3)) {
        xl6SemErrorNode(ctx,call,"C owner take requires pointer, optional byte length, and named C releaser");
        return 0;
    }
    XL6_AstNode* pointer = xl6SemCallArgExprAt(call,0);
    XL6_AstNode* length = count==3 ? xl6SemCallArgExprAt(call,1) : NULL;
    XL6_AstNode* releaser = xl6SemCallArgExprAt(call,count-1);
    int ok = xl6SemCheckExpr(ctx,pointer);
    if (!xl6TextEquals(xl6SemExprStaticTypeName(ctx,pointer),"point") && pointer->kind!=XL6_AST_NULL) {
        xl6SemErrorNode(ctx,pointer,"C owner take requires a raw point"); ok=0;
    }
    if (length && (!xl6SemCheckExpr(ctx,length) ||
        !xl6SemIsIntegerTypeName(xl6SemExprStaticTypeName(ctx,length)))) {
        xl6SemErrorNode(ctx,length,"C owner byte length must be an integer"); ok=0;
    }
    if(length && ok && !xl6SemCOwnedSignedLength(ctx,&call->children[1]->children[1]->children[0]))ok=0;
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
    if(node->kind==XL6_AST_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL) {
        XL6_AstNode* params=(XL6_AstNode*)xl6SemCallableParams(node);
        for(size_t i=0;params && i<params->childCount;++i) {
            XL6_AstNode* param=params->children[i];
            const XL6_AstNode* type=param && param->childCount ? param->children[0] : NULL;
            if(type && xl6COwnedBaseName(type->text)) {
                if(param->flags & (XL6_AST_FLAG_BYVAL|XL6_AST_FLAG_BYREF|XL6_AST_FLAG_OUT)) {
                    xl6SemErrorNode(ctx,param,"C span parameters require the default read-only borrow mode");ok=0;
                } else param->flags|=XL6_AST_FLAG_C_SPAN_PARAM;
            }
        }
    }
    if(node->kind==XL6_AST_RETURN_TYPES) {
        for(size_t r=0;r<node->childCount;++r) {
            XL6_AstNode* inner=(XL6_AstNode*)xl6SemNullableInnerType(node->children[r]);
            if(inner && xl6COwnedBaseName(inner->text))inner->flags|=XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
        }
    }
    if(node->kind==XL6_AST_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_FUNC ||
        node->kind==XL6_AST_NATIVE_EXTERN_SPECIAL || node->kind==XL6_AST_ASYNC_FUNC ||
        (node->flags & XL6_AST_FLAG_ASYNC)) {
        const XL6_AstNode* returns=xl6SemCallableReturns(node);
        for(size_t r=0;returns && r<returns->childCount;++r)
            if(xl6SemCOwnedTypeBase(returns->children[r])) {
                xl6SemErrorNode(ctx,node,"C result markers require a synchronous language function with an explicit take adapter");ok=0;
            }
    }
    if (node->kind==XL6_AST_TYPE && xl6COwnedBaseName(node->text) && !(node->flags & XL6_AST_FLAG_C_OWNED_SPECIALIZATION) &&
        (!parent || (parent->kind!=XL6_AST_VAR_DECL && parent->kind!=XL6_AST_CONST_DECL && parent->kind!=XL6_AST_RETURN_TYPES &&
          !(parent->kind==XL6_AST_PARAM && (parent->flags & (XL6_AST_FLAG_C_OWNED_SPECIALIZATION|XL6_AST_FLAG_C_SPAN_PARAM)))) ||
         (parent->kind!=XL6_AST_RETURN_TYPES && parent->kind!=XL6_AST_PARAM && xl6SemVarDeclTypeNode(parent)!=node))) {
        xl6SemErrorNode(ctx,node,"C markers are limited to initialized locals and synchronous results; use base types at storage boundaries");
        ok=0;
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xl6SemValidateCOwnedTypes(ctx,node->children[i],node))ok=0;
    return ok;
}

/* Clone a body for an exact static owner signature. Type identities and binding
 * symbols are rechecked in its own scope; decoded literals retain their bytes. */
static XL6_AstNode* xl6SemCOwnedClone(const XL6_AstNode* source)
{
    XL6_AstNode* copy=source->text ? xl6AstNewText(source->kind,source->text,strlen(source->text),NULL)
        : xl6AstNew(source->kind,NULL);
    if(!copy)return NULL;
    copy->flags=source->flags;copy->line=source->line;copy->column=source->column;
    copy->range=source->range;copy->tokenRange=source->tokenRange;xl6AstCopySource(copy,source);
    if(source->literalDecoded) {
        copy->literalDecoded=1;copy->literalScalar=source->literalScalar;
        if(source->literalData && !xl6AstSetLiteralData(copy,source->literalData,source->literalSize)) {
            xl6AstFree(copy);return NULL;
        }
    }
    if(source->semanticCallKind==XL6_SEM_CALL_C_OWNED_COMPARE || source->semanticCallKind==XL6_SEM_CALL_C_OWNED_CONCAT || source->semanticCallKind==XL6_SEM_CALL_C_OWNED_VIEW)
        copy->semanticCallKind=source->semanticCallKind;
    for(size_t i=0;i<source->childCount;++i) {
        XL6_AstNode* child=xl6SemCOwnedClone(source->children[i]);
        if(!child){xl6AstFree(copy);return NULL;}
        xl6AstAddChild(copy,child);
    }
    return copy;
}
/* The signature key includes every concrete parameter and result, avoiding a
 * fixed-width argument mask and distinguishing concrete generic instances. */
static uint64 xl6SemCOwnedSignatureHash(const XL6_AstNode* node,uint64 hash)
{
    if(!node)return hash;
    hash^=(uint64)node->kind;hash*=UINT64_C(1099511628211);
    for(const unsigned char* p=(const unsigned char*)(node->text ? node->text : "");*p;++p) {
        hash^=*p;hash*=UINT64_C(1099511628211);
    }
    hash^=node->flags & (XL6_AST_FLAG_BYVAL|XL6_AST_FLAG_BYREF|XL6_AST_FLAG_OUT);
    hash*=UINT64_C(1099511628211);
    for(size_t i=0;i<node->childCount;++i)hash=xl6SemCOwnedSignatureHash(node->children[i],hash);
    return hash;
}
enum {
    XL6_C_OWNED_CONCRETE_GENERIC=1,
    XL6_C_OWNED_ORDINARY_RESULT=2,
    XL6_C_OWNED_RESULT_ONLY=4
};
static XL6_AstNode* xl6SemCOwnedSpecialize(XL6_SemContext* ctx,XL6_AstNode* call,XL6_AstNode* function,int options,const XL6_AstNode* resultTypes)
{
    if((function->kind!=XL6_AST_FUNC && function->kind!=XL6_AST_EXPORT_FUNC && function->kind!=XL6_AST_ASYNC_FUNC) || xl6SemFuncIsGeneric(function) ||
        call->childCount!=2 || (call->children[0]->kind!=XL6_AST_NAME && call->children[0]->kind!=XL6_AST_MEMBER))return function;
    XL6_AstNode* owner=NULL;
    XL6_AstNode* declarations=ctx->root;
    if(call->children[0]->kind==XL6_AST_MEMBER) {
        if(function->flags & (XL6_AST_FLAG_VIRTUAL|XL6_AST_FLAG_OVERRIDE))return function;
        for(size_t c=0;c<ctx->root->childCount && !owner;++c) {
            XL6_AstNode* candidate=ctx->root->children[c];
            XL6_AstNode* body=candidate->kind==XL6_AST_CLASS && candidate->childCount>1 ? candidate->children[1] : NULL;
            for(size_t m=0;body && m<body->childCount;++m)
                if(body->children[m]==function){owner=candidate;declarations=body;break;}
        }
        if(!owner)return function;
    }
    XL6_AstNode* params=function->childCount>1 ? function->children[1] : NULL;
    XL6_AstNode* args=call->children[1];
    if(!params)return function;
    size_t fixedCount=(size_t)xl6SemFixedParamCount(params);
    unsigned char* promote=params->childCount ? xrtCalloc(params->childCount,2) : NULL;
    if(params->childCount && !promote)return NULL;
    unsigned char* provided=promote ? promote+params->childCount : NULL;
    int any=0;size_t positional=0;
    for(size_t i=0;!(options & XL6_C_OWNED_RESULT_ONLY) && i<args->childCount;++i) {
        XL6_AstNode* arg=args->children[i];
        int position=arg->kind==XL6_AST_NAMED_ARG ? xl6SemFindParamIndex(params,(int)fixedCount,arg->text) : (int)positional++;
        if((arg->kind!=XL6_AST_ARG && arg->kind!=XL6_AST_NAMED_ARG) || arg->childCount!=1) {xrtFree(promote);return function;}
        /* Variadic elements keep their ordinary boxed boundary. Only the
         * fixed prefix participates in the C-owner specialization key. */
        if(position<0 || (size_t)position>=fixedCount) {
            if(fixedCount<params->childCount)continue;
            xrtFree(promote);return function;
        }
        provided[position]=1;
        XL6_AstNode* parameter=params->children[position];
        if(parameter->flags & (XL6_AST_FLAG_BYREF|XL6_AST_FLAG_OUT))continue;
        XL6_AstNode* actual=xl6SemInferAutoTypeNode(ctx,arg->children[0]);
        const char* base=xl6SemCOwnedTypeBase(actual);
        const XL6_AstNode* formal=parameter->childCount ? xl6SemCOwnedResolvedElement(ctx,parameter->children[0]) : NULL;
        const XL6_AstNode* argument=xl6SemCOwnedElement(actual);
        if(base && formal &&
            (xl6TextEquals(base,formal->text) ||
             ((options & XL6_C_OWNED_CONCRETE_GENERIC) && argument && xl6TextEquals(argument->text,formal->text))))
            any=promote[position]=1;
        xl6AstFree(actual);
    }
    for(size_t p=0;!(options & XL6_C_OWNED_RESULT_ONLY) && p<fixedCount;++p)if(!provided[p]) {
        XL6_AstNode* parameter=params->children[p];
        if(parameter->childCount<2 || (parameter->flags & (XL6_AST_FLAG_BYREF|XL6_AST_FLAG_OUT)))continue;
        XL6_AstNode* expression=parameter->children[1];
        XL6_AstNode* actual=expression->inferredType ? xl6SemCloneTypeNode(expression->inferredType) :
            xl6SemInferAutoTypeNode(ctx,expression);
        const char* base=xl6SemCOwnedTypeBase(actual);
        const XL6_AstNode* formal=xl6SemCOwnedResolvedElement(ctx,parameter->children[0]);
        if(base && formal && xl6TextEquals(base,formal->text))any=promote[p]=1;
        xl6AstFree(actual);
    }
    const XL6_AstNode* originalReturns=xl6SemCallableReturns(function);
    if(options & XL6_C_OWNED_ORDINARY_RESULT)
        for(size_t r=0;originalReturns && r<originalReturns->childCount;++r)
            if(resultTypes ? !xl6SemTypeEquals(ctx,originalReturns->children[r],resultTypes->children[r]) :
                xl6SemCOwnedResultHasOwner(ctx,originalReturns->children[r]))any=1;
    if(!any){xrtFree(promote);return function;}
    XL6_AstNode* signature=xl6SemCOwnedClone(function);
    if(!signature){xrtFree(promote);return NULL;}
    /* An exported language wrapper still has a private static specialization.
     * Keep its public ABI ordinary; only direct calls use the C owner ABI. */
    if(signature->kind==XL6_AST_EXPORT_FUNC)signature->kind=XL6_AST_FUNC;
    signature->flags|=XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
    for(size_t p=0;p<params->childCount;++p)if(promote[p]) {
        XL6_AstNode* parameter=signature->children[1]->children[p];
        XL6_AstNode* concrete=xl6SemCloneTypeNode(xl6SemResolveAliasType(ctx,parameter->children[0]));
        if(!concrete){xrtFree(promote);xl6AstFree(signature);return NULL;}
        xl6AstFree(parameter->children[0]);parameter->children[0]=concrete;
        XL6_AstNode* type=(XL6_AstNode*)xl6SemCOwnedElement(concrete);
        type->flags|=XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
        if(!xl6COwnedBaseName(type->text)) {
            const char* marker=xl6TextEquals(xl6SemCOwnedResolvedElement(ctx,parameter->children[0])->text,"string") ? "cstring" : "cbytes";
            xrtFree(type->text);type->text=xrtStrDup(marker);
        }
        parameter->flags|=XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
    }
    xrtFree(promote);
    XL6_AstNode* returns=signature->children[2];
    if(resultTypes) {
        returns=xl6SemCloneTypeNode(resultTypes);
        if(!returns){xl6AstFree(signature);return NULL;}
        returns->kind=XL6_AST_RETURN_TYPES;
        xl6AstFree(signature->children[2]);signature->children[2]=returns;
    } else if(options & XL6_C_OWNED_ORDINARY_RESULT)
        for(size_t r=0;returns && r<returns->childCount;++r)
            if(!xl6SemCOwnedOrdinaryResultType(ctx,&returns->children[r])){xl6AstFree(signature);return NULL;}
    for(size_t r=0;!(options & XL6_C_OWNED_ORDINARY_RESULT) && function->kind!=XL6_AST_ASYNC_FUNC &&
        !(function->flags & XL6_AST_FLAG_ASYNC) && returns && r<returns->childCount;++r) {
        const XL6_AstNode* resolved=xl6SemResolveAliasType(ctx,returns->children[r]);
        const XL6_AstNode* element=xl6SemCOwnedResolvedElement(ctx,resolved);
        if(!element || (!xl6TextEquals(element->text,"string") && !xl6TextEquals(element->text,"bytes")))continue;
        const char* marker=xl6TextEquals(element->text,"string") ? "cstring" : "cbytes";
        XL6_AstNode* concrete=xl6SemCloneTypeNode(resolved);
        if(!concrete){xl6AstFree(signature);return NULL;}
        xl6AstFree(returns->children[r]);returns->children[r]=concrete;
        XL6_AstNode* type=(XL6_AstNode*)xl6SemCOwnedElement(concrete);
        type->flags|=XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
        xrtFree(type->text);type->text=xrtStrDup(marker);
    }
    uint64 hash=xl6SemCOwnedSignatureHash(signature->children[1],UINT64_C(14695981039346656037));
    hash=xl6SemCOwnedSignatureHash(returns,hash);
    char name[96];snprintf(name,sizeof(name),"__xl_c_owned_%u_%llx",function->semanticSymbolId,(unsigned long long)hash);
    XL6_AstNode* specialized=NULL;
    for(size_t i=0;i<declarations->childCount;++i) {
        XL6_AstNode* candidate=declarations->children[i];
        if((candidate->kind==XL6_AST_FUNC || candidate->kind==XL6_AST_ASYNC_FUNC) && xl6TextEquals(xl6SemFuncName(candidate),name)){specialized=candidate;break;}
    }
    if(!specialized) {
        specialized=signature;signature=NULL;
        xrtFree(specialized->children[0]->text);specialized->children[0]->text=xrtStrDup(name);
        if(!xl6SemAssignEntitySymbolIds(ctx,specialized)){xl6AstFree(specialized);return NULL;}
        XL6_Scope* saved=ctx->scope;
        while(ctx->scope->parent)ctx->scope=ctx->scope->parent;
        int declared=owner || xl6SemDeclareGlobalFuncName(ctx,specialized->children[0]);ctx->scope=saved;
        if(!declared){xl6AstFree(specialized);return NULL;}
        xl6AstAddChild(declarations,specialized);
        if(owner) {
            /* Recheck in the declaration's class/global scope, never the
             * caller's local scope. Register first for recursive calls. */
            XL6_AstNode* savedClass=ctx->currentClass;
            const char* savedSelf=ctx->implicitSelfName;
            XL6_TypeParamScope* savedTypes=ctx->typeParams;
            int savedCreate=ctx->inClassCreate,savedLoop=ctx->loopDepth,
                savedCatch=ctx->catchDepth,savedGenerator=ctx->generatorDepth;
            XL6_TypeParamScope classTypes={owner->childCount>2 ? owner->children[2] : NULL,NULL};
            while(ctx->scope->parent)ctx->scope=ctx->scope->parent;
            ctx->currentClass=owner;ctx->implicitSelfName=(specialized->flags & XL6_AST_FLAG_STATIC) ? NULL : "self";
            ctx->typeParams=&classTypes;ctx->inClassCreate=ctx->loopDepth=ctx->catchDepth=ctx->generatorDepth=0;
            int checked=xl6SemCheckFunc(ctx,specialized);
            ctx->scope=saved;ctx->currentClass=savedClass;ctx->implicitSelfName=savedSelf;
            ctx->typeParams=savedTypes;ctx->inClassCreate=savedCreate;ctx->loopDepth=savedLoop;
            ctx->catchDepth=savedCatch;ctx->generatorDepth=savedGenerator;
            if(!checked){xl6AstFree(signature);return NULL;}
        }
    }
    xl6AstFree(signature);
    xrtFree(call->children[0]->text);call->children[0]->text=xrtStrDup(name);
    xl6AstFree(call->inferredType);call->inferredType=NULL;
    return specialized;
}
static XL6_AstNode* xl6SemCOwnedSpecializeMethod(XL6_SemContext* ctx,XL6_AstNode* call,XL6_AstNode* method)
{ return xl6SemCOwnedSpecialize(ctx,call,method,0,NULL); }
/* Callable and Future boundaries expose ordinary result ABIs. A distinct
 * language body lets normal return lowering copy and unwind transactionally;
 * never cast between the owner descriptor and the ordinary string pair. */
/* Result covariance is pairwise, including inside a returned closure. Keep
 * unchanged owner slots and invariant input signatures in their original ABI.
 * Return -1 for an incompatible leaf, 0 for equality, 1 for an owner copy. */
static int xl6SemCOwnedResultMatchDepth(XL6_SemContext* ctx,
    const XL6_AstNode* target,const XL6_AstNode* source,unsigned depth)
{
    if(xl6SemTypeEquals(ctx,target,source))return 0;
    if(!target || !source || depth>32)return -1;
    target=xl6SemResolveAliasType(ctx,target);source=xl6SemResolveAliasType(ctx,source);
    const XL6_AstNode* targetInner=xl6SemNullableInnerType(target);
    const XL6_AstNode* sourceInner=xl6SemNullableInnerType(source);
    if(targetInner || sourceInner)
        return targetInner && sourceInner ? xl6SemCOwnedResultMatchDepth(ctx,targetInner,sourceInner,depth+1) : -1;
    if(xl6TextEquals(target->text,"function") && xl6TextEquals(source->text,"function") &&
       target->childCount==2 && source->childCount==2 &&
       xl6SemCallableTypeListEquals(ctx,target->children[0],source->children[0])) {
        const XL6_AstNode* targetResults=target->children[1],*sourceResults=source->children[1];
        if(targetResults->childCount!=sourceResults->childCount)return -1;
        int changed=0;
        for(size_t r=0;r<sourceResults->childCount;++r) {
            int match=xl6SemCOwnedResultMatchDepth(ctx,targetResults->children[r],sourceResults->children[r],depth+1);
            if(match<0)return -1;
            changed|=match;
        }
        return changed;
    }
    const char* base=xl6COwnedBaseName(source->text);
    return base && xl6TextEquals(base,target->text) ? 1 : -1;
}
static int xl6SemCOwnedResultListMatches(XL6_SemContext* ctx,const XL6_AstNode* target,const XL6_AstNode* source)
{
    if(!target || !source || target->childCount!=source->childCount)return 0;
    int changed=0;
    for(size_t r=0;r<source->childCount;++r) {
        int match=xl6SemCOwnedResultMatchDepth(ctx,target->children[r],source->children[r],0);
        if(match<0)return 0;
        changed|=match;
    }
    return changed;
}
static XL6_AstNode* xl6SemCOwnedFunctionWithResults(XL6_SemContext* ctx,XL6_AstNode* function,const XL6_AstNode* returns)
{
    XL6_AstNode* name=xl6SemCOwnedClone(function->children[0]);
    if(!name)return NULL;
    XL6_AstNode args={0},call={0};XL6_AstNode* children[2]={name,&args};
    args.kind=XL6_AST_LIST;call.kind=XL6_AST_CALL;call.children=children;call.childCount=2;
    XL6_AstNode* selected=xl6SemCOwnedSpecialize(ctx,&call,function,XL6_C_OWNED_ORDINARY_RESULT|XL6_C_OWNED_RESULT_ONLY,returns);
    xl6AstFree(name);return selected;
}
static XL6_AstNode* xl6SemCOwnedOrdinaryFunction(XL6_SemContext* ctx,XL6_AstNode* function)
{ return xl6SemCOwnedFunctionWithResults(ctx,function,NULL); }
static XL6_AstNode* xl6SemCOwnedAppendNode(XL6_AstNode* parent,XL6_AstNode* child)
{
    if(!parent || !child){xl6AstFree(child);return NULL;}
    size_t count=parent->childCount;xl6AstAddChild(parent,child);
    if(parent->childCount==count){xl6AstFree(child);return NULL;}
    return child;
}
/* Overload probing must not rewrite a callable for a candidate that may lose.
 * Rank exact owner signatures ahead of materializing result conversions. */
static int xl6SemCOwnedCallableResultCost(XL6_SemContext* ctx,const XL6_AstNode* expr,const XL6_AstNode* target)
{
    target=xl6SemResolveAliasType(ctx,target);
    int targetOptional=xl6SemNullableInnerType(target)!=NULL;
    target=xl6SemCOwnedResolvedElement(ctx,target);
    if(!target || !xl6TextEquals(target->text,"function") || target->childCount!=2)return 0;
    XL6_AstNode* source=xl6SemInferAutoTypeNode(ctx,(XL6_AstNode*)expr);
    const XL6_AstNode* callable=xl6SemCOwnedResolvedElement(ctx,source);
    int matches=source && (!xl6SemNullableInnerType(xl6SemResolveAliasType(ctx,source)) || targetOptional) &&
        callable && xl6TextEquals(callable->text,"function") && callable->childCount==2 &&
        xl6SemCallableTypeListEquals(ctx,callable->children[0],target->children[0]) &&
        xl6SemCOwnedResultListMatches(ctx,target->children[1],callable->children[1]);
    xl6AstFree(source);return matches ? 10 : 0;
}
static int xl6SemCOwnedAdaptCallArguments(XL6_SemContext* ctx,XL6_AstNode* call,const XL6_AstNode* function)
{
    const XL6_AstNode* params=xl6SemCallableParams(function);
    XL6_AstNode* args=call && call->childCount>1 ? call->children[1] : NULL;
    int fixed=xl6SemFixedParamCount(params),positional=0;
    for(size_t a=0;args && a<args->childCount;++a) {
        XL6_AstNode* argument=args->children[a];
        int position=argument->kind==XL6_AST_NAMED_ARG ? xl6SemFindParamIndex(params,fixed,argument->text) : positional++;
        if(position<0 || position>=fixed || argument->childCount!=1)continue;
        const XL6_AstNode* parameter=params->children[position];
        const XL6_AstNode* type=parameter->childCount ? xl6SemResolveAliasType(ctx,parameter->children[0]) : NULL;
        const XL6_AstNode* element=xl6SemCOwnedResolvedElement(ctx,type);
        if(element && xl6TextEquals(element->text,"function") &&
           !xl6SemCOwnedAdaptCallableExpr(ctx,argument->children[0],type))return 0;
    }
    return 1;
}
/* Evaluate the nullable source once. Only its present branch creates the
 * ordinary-result closure; normal flow narrowing feeds the existing adapter.
 * The absent branch returns null without constructing an invocation target. */
static int xl6SemCOwnedAdaptNullableCallableExpr(XL6_SemContext* ctx,XL6_AstNode* expr,
    const XL6_AstNode* source,const XL6_AstNode* target)
{
    XL6_AstNode* replacement=xl6AstNew(XL6_AST_CALL,NULL);
    XL6_AstNode* factory=xl6SemCOwnedAppendNode(replacement,xl6AstNew(XL6_AST_ANON_FUNC,NULL));
    XL6_AstNode* arguments=xl6SemCOwnedAppendNode(replacement,xl6AstNew(XL6_AST_LIST,NULL));
    XL6_AstNode* params=xl6SemCOwnedAppendNode(factory,xl6AstNew(XL6_AST_LIST,NULL));
    XL6_AstNode* returns=xl6SemCOwnedAppendNode(factory,xl6AstNew(XL6_AST_RETURN_TYPES,NULL));
    XL6_AstNode* body=xl6SemCOwnedAppendNode(factory,xl6AstNew(XL6_AST_BLOCK,NULL));
    XL6_AstNode* parameter=xl6SemCOwnedAppendNode(params,xl6AstNewText(XL6_AST_PARAM,"__xl_c_owned_optional",21,NULL));
    XL6_AstNode* branch=xl6SemCOwnedAppendNode(body,xl6AstNew(XL6_AST_IF,NULL));
    XL6_AstNode* condition=xl6SemCOwnedAppendNode(branch,xl6AstNewText(XL6_AST_BINARY,"!=",2,NULL));
    XL6_AstNode* present=xl6SemCOwnedAppendNode(branch,xl6AstNew(XL6_AST_BLOCK,NULL));
    XL6_AstNode* alternatives=xl6SemCOwnedAppendNode(branch,xl6AstNew(XL6_AST_LIST,NULL));
    XL6_AstNode* absent=xl6SemCOwnedAppendNode(branch,xl6AstNew(XL6_AST_BLOCK,NULL));
    XL6_AstNode* value=xl6SemCOwnedAppendNode(present,xl6AstNew(XL6_AST_RETURN,NULL));
    XL6_AstNode* missing=xl6SemCOwnedAppendNode(absent,xl6AstNew(XL6_AST_RETURN,NULL));
    XL6_AstNode* argument=xl6SemCOwnedAppendNode(arguments,xl6AstNew(XL6_AST_ARG,NULL));
    int ok=alternatives && value && missing &&
        xl6SemCOwnedAppendNode(parameter,xl6SemCloneTypeNode(source)) &&
        xl6SemCOwnedAppendNode(returns,xl6SemCloneTypeNode(target)) &&
        xl6SemCOwnedAppendNode(condition,xl6AstNewText(XL6_AST_NAME,"__xl_c_owned_optional",21,NULL)) &&
        xl6SemCOwnedAppendNode(condition,xl6AstNew(XL6_AST_NULL,NULL)) &&
        xl6SemCOwnedAppendNode(value,xl6AstNewText(XL6_AST_NAME,"__xl_c_owned_optional",21,NULL)) &&
        xl6SemCOwnedAppendNode(missing,xl6AstNew(XL6_AST_NULL,NULL)) &&
        xl6SemCOwnedAppendNode(argument,xl6SemCOwnedClone(expr));
    XL6_AstNode* old=ok ? xrtMalloc(sizeof(*old)) : NULL;
    if(ok && !old)ok=0;
    if(ok) {
        replacement->line=expr->line;replacement->column=expr->column;xl6AstCopySource(replacement,expr);
        ok=xl6SemAssignEntitySymbolIds(ctx,replacement) && xl6SemCheckExpr(ctx,replacement);
    }
    if(ok){*old=*expr;*expr=*replacement;xrtFree(replacement);xl6AstFree(old);}
    else{xl6AstFree(replacement);xrtFree(old);}
    return ok;
}
/* Convert a first-class callable only at an ordinary signature/erasure/Future
 * boundary. A factory snapshots the original expression exactly once, then
 * its returned closure performs the normal result conversion on invocation.
 * Explicit owner-returning callable types remain available for static reads. */
static int xl6SemCOwnedAdaptCallableExpr(XL6_SemContext* ctx,XL6_AstNode* expr,const XL6_AstNode* expected)
{
    XL6_AstNode* source=xl6SemInferAutoTypeNode(ctx,expr);
    const XL6_AstNode* callable=xl6SemCOwnedResolvedElement(ctx,source);
    if(!callable || !xl6TextEquals(callable->text,"function") || callable->childCount!=2){xl6AstFree(source);return 1;}
    int sourceOptional=xl6SemNullableInnerType(xl6SemResolveAliasType(ctx,source))!=NULL;
    expected=xl6SemResolveAliasType(ctx,expected);
    const XL6_AstNode* expectedElement=xl6SemCOwnedResolvedElement(ctx,expected);
    XL6_AstNode* target=xl6SemCloneTypeNode(expectedElement && expectedElement->childCount==2 ? expected : source);
    if(!target){xl6AstFree(source);return 0;}
    XL6_AstNode* targetElement=(XL6_AstNode*)xl6SemCOwnedResolvedElement(ctx,target);
    if(!expected || expected->childCount==0)
        for(size_t r=0;r<targetElement->children[1]->childCount;++r)
            if(!xl6SemCOwnedOrdinaryResultType(ctx,&targetElement->children[1]->children[r])){xl6AstFree(source);xl6AstFree(target);return 0;}
    if((sourceOptional && !xl6SemNullableInnerType(target)) ||
       !xl6SemCallableTypeListEquals(ctx,callable->children[0],targetElement->children[0]) ||
       !xl6SemCOwnedResultListMatches(ctx,targetElement->children[1],callable->children[1])) {
        xl6AstFree(source);xl6AstFree(target);return 1;
    }
    if(sourceOptional) {
        int ok=xl6SemCOwnedAdaptNullableCallableExpr(ctx,expr,source,target);
        xl6AstFree(source);xl6AstFree(target);return ok;
    }
    /* A present source adapts to a present closure; the receiving optional
     * declaration/return retains its existing nullable lift. */
    if(targetElement!=target) {
        XL6_AstNode* present=xl6SemCloneTypeNode(targetElement);
        xl6AstFree(target);target=present;
        if(!target){xl6AstFree(source);return 0;}
    }
    if(expr->kind==XL6_AST_NAME) {
        XL6_Symbol* binding=xl6ScopeFind(ctx->scope,expr->text);
        if(binding && binding->kind==XL6_SYM_FUNC) {
            XL6_AstNode* function=xl6SemFindFunctionValueFunc(ctx,expr->text,target,expr,0);
            int ok=function && xl6SemBindResolvedFunctionValue(ctx,expr,target,function,1);
            xl6AstFree(source);xl6AstFree(target);return ok;
        }
    }
    if(expr->kind==XL6_AST_ANON_FUNC) {
        XL6_AstNode* returns=xl6SemCloneTypeNode(target->children[1]);
        if(!returns){xl6AstFree(source);xl6AstFree(target);return 0;}
        xl6AstFree(expr->children[1]);expr->children[1]=returns;
        xl6AstFree(expr->inferredType);expr->inferredType=NULL;
        xl6AstFree(source);xl6AstFree(target);return xl6SemCheckExpr(ctx,expr);
    }
    XL6_AstNode* replacement=xl6AstNew(XL6_AST_CALL,NULL);
    XL6_AstNode* factory=xl6SemCOwnedAppendNode(replacement,xl6AstNew(XL6_AST_ANON_FUNC,NULL));
    XL6_AstNode* arguments=xl6SemCOwnedAppendNode(replacement,xl6AstNew(XL6_AST_LIST,NULL));
    XL6_AstNode* params=xl6SemCOwnedAppendNode(factory,xl6AstNew(XL6_AST_LIST,NULL));
    XL6_AstNode* returns=xl6SemCOwnedAppendNode(factory,xl6AstNew(XL6_AST_RETURN_TYPES,NULL));
    XL6_AstNode* body=xl6SemCOwnedAppendNode(factory,xl6AstNew(XL6_AST_BLOCK,NULL));
    XL6_AstNode* parameter=xl6SemCOwnedAppendNode(params,xl6AstNewText(XL6_AST_PARAM,"__xl_c_owned_source",19,NULL));
    XL6_AstNode* returned=xl6SemCOwnedAppendNode(body,xl6AstNew(XL6_AST_RETURN,NULL));
    XL6_AstNode* adapter=xl6SemCOwnedAppendNode(returned,xl6AstNew(XL6_AST_ANON_FUNC,NULL));
    XL6_AstNode* adapterParams=xl6SemCOwnedAppendNode(adapter,xl6AstNew(XL6_AST_LIST,NULL));
    XL6_AstNode* adapterReturns=xl6SemCOwnedAppendNode(adapter,xl6SemCloneTypeNode(target->children[1]));
    XL6_AstNode* adapterBody=xl6SemCOwnedAppendNode(adapter,xl6AstNew(XL6_AST_BLOCK,NULL));
    size_t resultCount=source->children[1]->childCount;
    XL6_AstNode* result=xl6SemCOwnedAppendNode(adapterBody,xl6AstNew(
        resultCount>1 ? XL6_AST_MULTI_VAR_DECL : XL6_AST_RETURN,NULL));
    XL6_AstNode* resultNames=resultCount>1 ? xl6SemCOwnedAppendNode(result,xl6AstNew(XL6_AST_LIST,NULL)) : NULL;
    XL6_AstNode* invoke=xl6SemCOwnedAppendNode(result,xl6AstNew(XL6_AST_CALL,NULL));
    XL6_AstNode* name=xl6SemCOwnedAppendNode(invoke,xl6AstNewText(XL6_AST_NAME,"__xl_c_owned_source",19,NULL));
    XL6_AstNode* invokeArgs=xl6SemCOwnedAppendNode(invoke,xl6AstNew(XL6_AST_LIST,NULL));
    int ok=arguments && parameter && adapterParams && adapterReturns && name && invokeArgs &&
        xl6SemCOwnedAppendNode(parameter,xl6SemCloneTypeNode(source)) &&
        xl6SemCOwnedAppendNode(returns,xl6SemCloneTypeNode(target));
    if(adapter)adapter->flags|=XL6_AST_FLAG_C_OWNED_CALLABLE_ADAPTER;
    /* Destructure once so each owner result crosses its own normal return
     * conversion; a product value itself has no implicit owner-copy ABI. */
    if(ok && resultCount>1) {
        XL6_AstNode* elements=xl6SemCOwnedAppendNode(adapterBody,xl6AstNew(XL6_AST_RETURN,NULL));
        ok=resultNames && elements;
        for(size_t r=0;ok && r<resultCount;++r) {
            char resultName[48];snprintf(resultName,sizeof(resultName),"__xl_c_owned_result_%zu",r);
            ok=xl6SemCOwnedAppendNode(resultNames,xl6AstNewText(XL6_AST_NAME,resultName,strlen(resultName),NULL)) &&
                xl6SemCOwnedAppendNode(elements,xl6AstNewText(XL6_AST_NAME,resultName,strlen(resultName),NULL));
        }
    }
    for(size_t p=0;ok && p<source->children[0]->childCount;++p) {
        const XL6_AstNode* type=source->children[0]->children[p];
        char parameterName[48];snprintf(parameterName,sizeof(parameterName),"__xl_c_owned_arg_%zu",p);
        XL6_AstNode* item=xl6SemCOwnedAppendNode(adapterParams,xl6AstNewText(XL6_AST_PARAM,parameterName,strlen(parameterName),NULL));
        XL6_AstNode* argument=xl6SemCOwnedAppendNode(invokeArgs,xl6AstNew(XL6_AST_ARG,NULL));
        if(!item || !argument){ok=0;break;}
        item->flags=type->flags|XL6_AST_FLAG_C_OWNED_SPECIALIZATION;
        if(xl6SemTypeIsVararg(type))argument->flags=XL6_AST_FLAG_FORWARD_VARIADIC_PACK;
        ok=xl6SemCOwnedAppendNode(item,xl6SemCloneTypeNode(type)) &&
            xl6SemCOwnedAppendNode(argument,xl6AstNewText(XL6_AST_NAME,parameterName,strlen(parameterName),NULL));
    }
    XL6_AstNode* argument=ok ? xl6SemCOwnedAppendNode(arguments,xl6AstNew(XL6_AST_ARG,NULL)) : NULL;
    if(ok)ok=argument && xl6SemCOwnedAppendNode(argument,xl6SemCOwnedClone(expr));
    XL6_AstNode* old=ok ? xrtMalloc(sizeof(*old)) : NULL;
    if(ok && !old)ok=0;
    if(ok) {
        replacement->line=expr->line;replacement->column=expr->column;xl6AstCopySource(replacement,expr);
        ok=xl6SemAssignEntitySymbolIds(ctx,replacement) && xl6SemCheckExpr(ctx,replacement);
    }
    if(ok){*old=*expr;*expr=*replacement;xrtFree(replacement);xl6AstFree(old);}
    else{xl6AstFree(replacement);xrtFree(old);}
    xl6AstFree(source);xl6AstFree(target);return ok;
}
/* Only a resolved function declaration may be specialized. A mutable callable
 * value keeps its signature and uses the ordinary worker argument boundary. */
static int xl6SemCOwnedSpawn(XL6_SemContext* ctx,XL6_AstNode* call,XL6_AstNode* callable)
{
    if(!callable || callable->kind!=XL6_AST_NAME || !callable->semanticTargetSymbolId)return xl6SemCOwnedAdaptCallableExpr(ctx,callable,NULL);
    XL6_Symbol* binding=xl6ScopeFind(ctx->scope,callable->text);
    if(!binding || binding->kind!=XL6_SYM_FUNC)return xl6SemCOwnedAdaptCallableExpr(ctx,callable,NULL);
    XL6_AstNode* function=NULL;
    for(size_t i=0;i<ctx->root->childCount;++i)
        if(ctx->root->children[i]->semanticSymbolId==callable->semanticTargetSymbolId){function=ctx->root->children[i];break;}
    if(!function || (function->kind!=XL6_AST_FUNC && function->kind!=XL6_AST_EXPORT_FUNC))return 1;
    XL6_AstNode* args=call->children[1];
    XL6_AstNode arguments={0},probe={0};XL6_AstNode* children[2]={callable,&arguments};
    arguments.kind=XL6_AST_LIST;arguments.children=args->children+1;arguments.childCount=args->childCount-1;
    probe.kind=XL6_AST_CALL;probe.children=children;probe.childCount=2;
    /* Future payloads keep the public result representation. */
    XL6_AstNode* selected=xl6SemCOwnedSpecialize(ctx,&probe,function,XL6_C_OWNED_ORDINARY_RESULT,NULL);
    if(!selected)return 0;
    return selected==function || xl6SemBindUniqueNamedFunctionValue(ctx,callable,1);
}
static int xl6SemCOwnedSubstituteTypes(XL6_AstNode** place,const XL6_AstNode* params,const XL6_AstNode* args)
{
    XL6_AstNode* node=*place;
    if(node->kind==XL6_AST_TYPE) {
        XL6_AstNode* type=xl6SemSubstituteGenericTypeArgs(params,args,node);
        if(!type)return 0;
        xl6AstFree(node);*place=type;return 1;
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!xl6SemCOwnedSubstituteTypes(&node->children[i],params,args))return 0;
    return 1;
}
/* Resolve only the C-bearing instance here. Ordinary instances keep the
 * canonical XLIR generic-specialization pipeline and its existing ABI. */
static int xl6SemCOwnedGenericCall(XL6_SemContext* ctx,XL6_AstNode* call,XL6_AstNode* function,const XL6_AstNode* typeArgs)
{
    XL6_AstNode* concrete=xl6SemCOwnedClone(function);
    if(!concrete)return -1;
    concrete->semanticSymbolId=function->semanticSymbolId;
    if(!xl6SemCOwnedSubstituteTypes(&concrete,xl6SemFuncTypeParams(function),typeArgs)) {xl6AstFree(concrete);return -1;}
    XL6_AstNode* typeParams=xl6SemFuncTypeParams(concrete);
    for(size_t i=0;typeParams && i<typeParams->childCount;++i)xl6AstFree(typeParams->children[i]);
    if(typeParams)typeParams->childCount=0;
    XL6_AstNode* original=call->children[0];
    call->children[0]=xl6SemCOwnedClone(function->children[0]);
    XL6_AstNode* selected=xl6SemCOwnedSpecialize(ctx,call,concrete,XL6_C_OWNED_CONCRETE_GENERIC,NULL);
    if(!selected || selected==concrete) {
        xl6AstFree(call->children[0]);call->children[0]=original;xl6AstFree(concrete);
        return selected ? 0 : -1;
    }
    xl6AstFree(original);xl6AstFree(concrete);
    xl6SemSetCallPlan(call,XL6_SEM_CALL_DIRECT,selected);
    return xl6SemCheckKnownCall(ctx,call,selected) ? 1 : -1;
}

static int xl6SemCOwnedPrint(XL6_SemContext* ctx,XL6_AstNode* call)
{
    XL6_AstNode* name=call->childCount ? call->children[0] : NULL;
    if(!name || name->kind!=XL6_AST_NAME ||
       (!xl6TextEquals(name->text,"__xl_std_print") && !xl6TextEquals(name->text,"__xl_std_println")))return -1;
    XL6_AstNode* args=call->childCount>1 ? call->children[1] : NULL;
    int owned=0;
    for(size_t i=0;args && i<args->childCount;++i) {
        XL6_AstNode* arg=args->children[i];
        if(arg->kind!=XL6_AST_ARG || arg->childCount!=1)return -1;
        XL6_AstNode* type=xl6SemInferAutoTypeNode(ctx,arg->children[0]);
        owned|=xl6SemCOwnedTypeBase(type)!=NULL;
        xl6AstFree(type);
    }
    if(!owned)return -1;
    for(size_t i=0;i<args->childCount;++i)
        if(!xl6SemCheckExpr(ctx,args->children[i]->children[0]))return 0;
    xl6SemSetCallPlan(call,XL6_SEM_CALL_C_OWNED_PRINT,NULL);
    xl6AstFree(call->inferredType);call->inferredType=NULL;
    return 1;
}
