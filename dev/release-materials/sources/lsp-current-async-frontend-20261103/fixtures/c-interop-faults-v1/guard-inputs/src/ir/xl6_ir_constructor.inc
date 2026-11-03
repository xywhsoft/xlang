/* Typed-IR adapter. It consumes canonical field/receiver/call identities and
 * closed effects, never reconstructs AST or asks the C backend for semantics. */
typedef struct XL6_IrConstructorView {
    const XL6_IrModule* module;
    const XL6_IrClassField* field;
    XL6_SymbolId receiver;
    XL6_DiagList* diags;
    XL6_IrTypeId ownerType;
} XL6_IrConstructorView;
static XL6_AstKind xl6IrConstructorKind(void* context,XL6_ConstructorNode node)
{ (void)context;return ((const XL6_IrNode*)node)->sourceKind; }
static size_t xl6IrConstructorCount(void* context,XL6_ConstructorNode node)
{ (void)context;return ((const XL6_IrNode*)node)->childCount; }
static XL6_ConstructorNode xl6IrConstructorChild(void* context,XL6_ConstructorNode node,size_t i)
{
    const XL6_IrModule* m=((XL6_IrConstructorView*)context)->module;
    const XL6_IrNode* n=(const XL6_IrNode*)node;
    return (XL6_ConstructorNode)xl6IrNodeGet(m,m->children[n->firstChild+i]);
}
static const char* xl6IrConstructorText(void* context,XL6_ConstructorNode node)
{ (void)context;return ((const XL6_IrNode*)node)->text; }
static int xl6IrConstructorSelf(XL6_IrConstructorView* v,const XL6_IrNode* n)
{
    return n&&n->sourceKind==XL6_AST_NAME&&
        ((v->receiver&&n->targetSymbol==v->receiver)||
         n->implicitNameAbi==XL6_IR_IMPLICIT_NAME_BASE_RECEIVER_VIEW);
}
static const XL6_IrClassField* xl6IrConstructorField(XL6_IrConstructorView* v,const XL6_IrNode* n)
{
    if(n->implicitNameAbi==XL6_IR_IMPLICIT_NAME_CLASS_FIELD)
        return xl6IrImplicitNameFieldByIdentity(v->module,n);
    if(n->sourceKind==XL6_AST_MEMBER&&n->childCount==1&&
        xl6IrConstructorSelf(v,xl6IrNodeGet(v->module,v->module->children[n->firstChild]))){
        const XL6_IrClassField* field=xl6IrClassFieldForMemberAbi(v->module,n);
        if(field)return field;
        /* A generic body's receiver still carries its formal TypeId. Verify
         * it against each exact closed layout whose obligation is being
         * proved; no open template layout or dummy resource is fabricated. */
        XL6_IrTypeId owner=v->ownerType;
        for(size_t d=0;owner&&d<=v->module->classLayoutCount;++d){
            const XL6_IrClassLayout* layout=xl6IrBuildClassLayoutByTypeLocal(v->module,owner);
            if(!layout)break;
            for(size_t i=0;i<layout->fieldCount;++i){
                field=&v->module->classFields[layout->firstField+i];
                if(n->text&&field->name&&!strcmp(n->text,field->name))return field;
            }
            owner=layout->baseType;
        }
    }
    return NULL;
}
static int xl6IrConstructorContainsField(const XL6_IrModule* m,XL6_IrTypeId owner,XL6_IrNodeId field)
{
    for(size_t d=0;owner&&d<=m->classLayoutCount;++d){
        const XL6_IrClassLayout* layout=xl6IrBuildClassLayoutByTypeLocal(m,owner);if(!layout)return 0;
        for(size_t i=0;i<layout->fieldCount;++i)if(m->classFields[layout->firstField+i].declaration==field)return 1;
        owner=layout->baseType;
    }
    return 0;
}
static int xl6IrConstructorExposes(XL6_IrConstructorView* v,const XL6_IrNode* n,size_t depth)
{
    if(!n)return 0;
    if(depth>256)return 1;
    if(xl6IrConstructorField(v,n))return 0;
    if(xl6IrConstructorSelf(v,n))return 1;
    if(n->sourceKind==XL6_AST_CALL&&n->callKind==XL6_SEM_CALL_METHOD&&n->childCount){
        const XL6_IrNode* callee=xl6IrNodeGet(v->module,v->module->children[n->firstChild]);
        if(callee&&callee->sourceKind==XL6_AST_NAME)return 1;
    }
    if(n->callKind==XL6_SEM_CALL_BASE_CONSTRUCTOR){
        const XL6_IrNode* target=xl6IrSymbolNodeGet(v->module,n->targetSymbol);
        if(!target)return n->targetSymbol!=0;
        XL6_IrConstructorView base=*v;base.receiver=target->constructorReceiverSymbol;base.ownerType=target->declarationOwnerType;
        if(target->childCount>1&&xl6IrConstructorExposes(&base,
            xl6IrNodeGet(v->module,v->module->children[target->firstChild+1]),depth+1))return 1;
        for(size_t i=1;i<n->childCount;++i)if(xl6IrConstructorExposes(v,
            xl6IrNodeGet(v->module,v->module->children[n->firstChild+i]),depth+1))return 1;
        return 0;
    }
    for(size_t i=0;i<n->childCount;++i)if(xl6IrConstructorExposes(v,
        xl6IrNodeGet(v->module,v->module->children[n->firstChild+i]),depth+1))return 1;
    return 0;
}
static unsigned xl6IrConstructorFacts(void* context,XL6_ConstructorNode node)
{
    XL6_IrConstructorView* v=(XL6_IrConstructorView*)context;const XL6_IrNode* n=(const XL6_IrNode*)node;
    const XL6_IrClassField* field=xl6IrConstructorField(v,n);unsigned f=0;
    if(field)f|=XL6_CONSTRUCTOR_DIRECT_FIELD|(field->declaration==v->field->declaration?XL6_CONSTRUCTOR_FIELD:0);
    else if(xl6IrConstructorSelf(v,n))f|=XL6_CONSTRUCTOR_SELF;
    if(n->sourceKind==XL6_AST_CALL&&n->callKind==XL6_SEM_CALL_METHOD&&n->childCount){
        const XL6_IrNode* callee=xl6IrNodeGet(v->module,v->module->children[n->firstChild]);
        if(callee&&callee->sourceKind==XL6_AST_NAME)f|=XL6_CONSTRUCTOR_SELF;
    }
    if(n->operatorErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||n->castErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||
       n->implicitConversionErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||n->projectionErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||
       n->evaluationErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||n->constructionErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||
       n->indexWriteErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE||n->loopErrorAbi==XL6_IR_ERROR_ABI_MAY_RAISE)
        f|=XL6_CONSTRUCTOR_MAY_RAISE;
    /* Insertions, copies and dynamic calls can allocate even when their
     * callee's own effect is no-raise. Keep these effects independent. */
    if(n->sourceKind==XL6_AST_ASSIGN||n->sourceKind==XL6_AST_MULTI_ASSIGN||n->sourceKind==XL6_AST_CALL||
       n->sourceKind==XL6_AST_AWAIT||n->sourceKind==XL6_AST_ANON_FUNC)f|=XL6_CONSTRUCTOR_MAY_RAISE;
    if(n->sourceKind==XL6_AST_VAR_DECL||n->sourceKind==XL6_AST_CONST_DECL){
        const XL6_IrValueInitializer* initializer=xl6IrValueInitializerGet(v->module,
            (XL6_IrNodeId)(n-v->module->nodes+1));
        if(initializer&&initializer->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE)f|=XL6_CONSTRUCTOR_MAY_RAISE;
    }
    if(n->callKind==XL6_SEM_CALL_BASE_CONSTRUCTOR){
        f|=XL6_CONSTRUCTOR_BASE_CALL;
        const XL6_IrNode* target=xl6IrSymbolNodeGet(v->module,n->targetSymbol);
        if(!target){if(n->targetSymbol)f|=XL6_CONSTRUCTOR_BASE_EXPOSES;}
        else if(xl6IrConstructorContainsField(v->module,target->declarationOwnerType,v->field->declaration))
            f|=XL6_CONSTRUCTOR_BASE_INITIALIZES;
        else {XL6_IrConstructorView base=*v;base.receiver=target->constructorReceiverSymbol;base.ownerType=target->declarationOwnerType;
            if(target->childCount>1&&xl6IrConstructorExposes(&base,
                xl6IrNodeGet(v->module,v->module->children[target->firstChild+1]),0))f|=XL6_CONSTRUCTOR_BASE_EXPOSES;
        }
    }
    return f;
}
static void xl6IrConstructorError(void* context,XL6_ConstructorNode node,const char* message)
{
    XL6_IrConstructorView* v=(XL6_IrConstructorView*)context;
    xl6IrDiag(v->diags,(const XL6_IrNode*)node,message);
}
static int xl6IrVerifyConstructorAssignments(const XL6_IrModule* m,XL6_DiagList* diags)
{
    int pending=0;
    for(size_t i=0;i<m->classFieldCount;++i)
        pending|=m->classFields[i].initializerAbi==XL6_IR_INITIALIZER_CONSTRUCTOR_REQUIRED;
    if(!pending)return 1;
    for(size_t i=0;i<m->nodeCount;++i){
        const XL6_IrNode* n=&m->nodes[i];
        if(n->firstChild>m->childCount||n->childCount>m->childCount-n->firstChild)
            return xl6IrDiag(diags,n,"constructor flow received a malformed node child range");
    }
    for(size_t i=0;i<m->childCount;++i)if(m->children[i]>m->nodeCount)
        return xl6IrDiag(diags,NULL,"constructor flow received an invalid child identity");
    XL6_IrConstructorView context={m,NULL,0,diags};
    XL6_ConstructorView view={&context,xl6IrConstructorKind,xl6IrConstructorCount,
        xl6IrConstructorChild,xl6IrConstructorText,xl6IrConstructorFacts,xl6IrConstructorError,1048576};
    for(size_t l=0;l<m->classLayoutCount;++l){
        const XL6_IrClassLayout* layout=&m->classLayouts[l];
        for(size_t f=0;f<m->classFieldCount;++f){
            const XL6_IrClassField* field=&m->classFields[f];
            if(field->initializerAbi!=XL6_IR_INITIALIZER_CONSTRUCTOR_REQUIRED||
               !xl6IrConstructorContainsField(m,layout->type,field->declaration))continue;
            context.field=field;context.ownerType=layout->type;
            size_t constructors=0;
            for(size_t i=0;i<m->nodeCount;++i){
                const XL6_IrNode* ctor=&m->nodes[i];
                if(ctor->sourceKind!=XL6_AST_CREATE||!ctor->text||strcmp(ctor->text,"create")||
                   ctor->childCount<2)continue;
                int templateBody=xl6IrTypeContainsTypeParam(&m->types,ctor->declarationOwnerType,0)&&
                    xl6IrBuildEnclosingClass(m,(XL6_IrNodeId)(i+1))==layout->declaration;
                if(ctor->declarationOwnerType!=layout->type&&!templateBody)continue;
                ++constructors;
                if(!ctor->constructorReceiverSymbol)return xl6IrDiag(diags,ctor,"constructor has no canonical receiver symbol");
                if(ctor->compiledPackageLink)continue;
                context.receiver=ctor->constructorReceiverSymbol;
                if(!xl6ConstructorFlowCheck(&view,(XL6_ConstructorNode)
                    xl6IrNodeGet(m,m->children[ctor->firstChild+1]),0,1))return 0;
            }
            /* A declaration-only derived class runs its existing verified
             * default base chain. Its own pending slots still require create. */
            if(!constructors&&field->ownerType==layout->type&&
                xl6IrBuildEnclosingClass(m,field->declaration)==layout->declaration)
                return xl6IrDiag(diags,xl6IrNodeGet(m,layout->declaration),"required field has no constructor body proof");
        }
    }
    return 1;
}
