/* Semantic adapter for the shared, immutable constructor flow analysis. */
typedef struct XL6_SemConstructorView {
    XL6_SemContext* sem;
    const XL6_AstNode* owner;
    const XL6_AstNode* field;
    uint32 receiverSymbol;
} XL6_SemConstructorView;

static XL6_AstKind xl6SemConstructorKind(void* context,XL6_ConstructorNode node)
{ (void)context;return ((const XL6_AstNode*)node)->kind; }
static size_t xl6SemConstructorCount(void* context,XL6_ConstructorNode node)
{ (void)context;return ((const XL6_AstNode*)node)->childCount; }
static XL6_ConstructorNode xl6SemConstructorChild(void* context,XL6_ConstructorNode node,size_t i)
{ (void)context;return (XL6_ConstructorNode)((const XL6_AstNode*)node)->children[i]; }
static const char* xl6SemConstructorText(void* context,XL6_ConstructorNode node)
{ (void)context;return ((const XL6_AstNode*)node)->text; }

static int xl6SemConstructorSelf(XL6_SemConstructorView* v,const XL6_AstNode* node)
{
    return node&&node->kind==XL6_AST_NAME&&
        ((v->receiverSymbol&&node->semanticSymbolId==v->receiverSymbol)||
         (!node->semanticSymbolId&&xl6TextEquals(node->text,"base")));
}
static const XL6_AstNode* xl6SemConstructorField(XL6_SemConstructorView* v,const XL6_AstNode* n)
{
    if(!n)return NULL;
    if(n->kind==XL6_AST_MEMBER&&n->childCount==1&&xl6SemConstructorSelf(v,n->children[0])){
        const XL6_AstNode* owner=v->owner;
        if(xl6TextEquals(n->children[0]->text,"base"))owner=xl6SemBaseClassDecl(v->sem,owner);
        return xl6SemFindClassFieldNode(v->sem,owner,n->text,NULL);
    }
    /* The binder leaves implicit fields symbol-less. A shadowing local has
     * its own canonical symbol even after its lexical scope has been popped. */
    if(n->kind==XL6_AST_NAME&&!n->semanticSymbolId)
        return xl6SemFindClassFieldNode(v->sem,v->owner,n->text,NULL);
    return NULL;
}
static const XL6_AstNode* xl6SemConstructorTarget(const XL6_AstNode* root,uint32 symbol,
    const XL6_AstNode** owner,size_t depth)
{
    if(!root||!symbol||depth>256)return NULL;
    for(size_t i=0;i<root->childCount;++i){
        const XL6_AstNode* n=root->children[i];if(!n)continue;
        if(n->kind==XL6_AST_CREATE&&n->semanticSymbolId==symbol){*owner=root;return n;}
        const XL6_AstNode* found=xl6SemConstructorTarget(n,symbol,owner,depth+1);
        if(found){if(root->kind==XL6_AST_CLASS)*owner=root;return found;}
    }
    return NULL;
}
static int xl6SemConstructorContainsField(XL6_SemConstructorView* v,const XL6_AstNode* owner)
{
    for(size_t depth=0;owner&&depth<=v->sem->types.count;++depth){
        const XL6_AstNode* body=owner->childCount>1?owner->children[1]:NULL;
        for(size_t i=0;body&&i<body->childCount;++i)if(body->children[i]==v->field)return 1;
        owner=xl6SemBaseClassDecl(v->sem,owner);
    }
    return 0;
}
static int xl6SemConstructorExposes(XL6_SemConstructorView* v,const XL6_AstNode* n,size_t depth)
{
    if(!n)return 0;
    if(depth>256)return 1;
    if(xl6SemConstructorField(v,n))return 0;
    if(xl6SemConstructorSelf(v,n))return 1;
    if(n->kind==XL6_AST_CALL&&n->semanticCallKind==XL6_SEM_CALL_METHOD&&n->childCount&&
       n->children[0]&&n->children[0]->kind==XL6_AST_NAME)return 1;
    if(n->kind==XL6_AST_NAME&&!n->semanticSymbolId&&
       xl6SemClassHasCallableMember(v->sem,v->owner,n->text))return 1;
    if(n->semanticCallKind==XL6_SEM_CALL_BASE_CONSTRUCTOR){
        const XL6_AstNode* owner=NULL;
        const XL6_AstNode* target=xl6SemConstructorTarget(v->sem->root,n->semanticTargetSymbolId,&owner,0);
        if(target){XL6_SemConstructorView base=*v;base.owner=owner;base.receiverSymbol=target->semanticReceiverSymbolId;
            if(xl6SemConstructorExposes(&base,target->childCount>1?target->children[1]:NULL,depth+1))return 1;
        }else if(n->semanticTargetSymbolId)return 1;
        for(size_t i=1;i<n->childCount;++i)if(xl6SemConstructorExposes(v,n->children[i],depth+1))return 1;
        return 0;
    }
    for(size_t i=0;i<n->childCount;++i)if(xl6SemConstructorExposes(v,n->children[i],depth+1))return 1;
    return 0;
}
static unsigned xl6SemConstructorFacts(void* context,XL6_ConstructorNode node)
{
    XL6_SemConstructorView* v=(XL6_SemConstructorView*)context;
    const XL6_AstNode* n=(const XL6_AstNode*)node;
    const XL6_AstNode* field=xl6SemConstructorField(v,n);unsigned f=0;
    if(field)f|=XL6_CONSTRUCTOR_DIRECT_FIELD|(field==v->field?XL6_CONSTRUCTOR_FIELD:0);
    else if(xl6SemConstructorSelf(v,n)||(n->kind==XL6_AST_NAME&&!n->semanticSymbolId&&
        xl6SemClassHasCallableMember(v->sem,v->owner,n->text)))f|=XL6_CONSTRUCTOR_SELF;
    if(n->kind==XL6_AST_CALL&&n->semanticCallKind==XL6_SEM_CALL_METHOD&&n->childCount&&
       n->children[0]&&n->children[0]->kind==XL6_AST_NAME)f|=XL6_CONSTRUCTOR_SELF;
    /* Semantic effects deliberately overapproximate potentially allocating
     * expressions. Typed XLIR supplies closed operation effects independently. */
    switch(n->kind){
    case XL6_AST_CALL:case XL6_AST_MEMBER:case XL6_AST_SAFE_MEMBER:
    case XL6_AST_VAR_DECL:case XL6_AST_MULTI_VAR_DECL:case XL6_AST_CONST_DECL:
    case XL6_AST_INDEX:case XL6_AST_SAFE_INDEX:case XL6_AST_TYPE_AS:
    case XL6_AST_AWAIT:case XL6_AST_ASSIGN:case XL6_AST_MULTI_ASSIGN:
    case XL6_AST_BINARY:case XL6_AST_UNARY:case XL6_AST_INTERP_STRING:
    case XL6_AST_ARRAY_LITERAL:case XL6_AST_LIST_LITERAL:case XL6_AST_SET_LITERAL:
    case XL6_AST_DICT_LITERAL:case XL6_AST_TYPED_LITERAL:case XL6_AST_ANON_FUNC:
        f|=XL6_CONSTRUCTOR_MAY_RAISE;break;
    default:break;
    }
    if(n->semanticCallKind==XL6_SEM_CALL_BASE_CONSTRUCTOR){
        f|=XL6_CONSTRUCTOR_BASE_CALL;
        const XL6_AstNode* owner=NULL;
        const XL6_AstNode* target=xl6SemConstructorTarget(v->sem->root,n->semanticTargetSymbolId,&owner,0);
        if(target){
            if(xl6SemConstructorContainsField(v,owner))f|=XL6_CONSTRUCTOR_BASE_INITIALIZES;
            else {XL6_SemConstructorView base=*v;base.owner=owner;base.receiverSymbol=target->semanticReceiverSymbolId;
                if(xl6SemConstructorExposes(&base,target->childCount>1?target->children[1]:NULL,0))
                    f|=XL6_CONSTRUCTOR_BASE_EXPOSES;
            }
        }else if(n->semanticTargetSymbolId)f|=XL6_CONSTRUCTOR_BASE_EXPOSES;
    }
    return f;
}
static void xl6SemConstructorError(void* context,XL6_ConstructorNode n,const char* message)
{
    XL6_SemConstructorView* v=(XL6_SemConstructorView*)context;char text[384];
    snprintf(text,sizeof(text),"%s: %s",message,v->field->text?v->field->text:"<field>");
    xl6SemErrorNode(v->sem,(const XL6_AstNode*)n,text);
}
static int xl6SemCheckConstructorAssignment(XL6_SemContext* sem,const XL6_AstNode* owner,
    const XL6_AstNode* constructor)
{
    /* A compiled-package constructor calls the already verified host body.
     * Its declaration/layout and exact binding are checked again in XLIR. */
    if(constructor->compiledPackageLink)return 1;
    XL6_SemConstructorView context={sem,owner,NULL,constructor->semanticReceiverSymbolId};
    XL6_ConstructorView view={&context,xl6SemConstructorKind,xl6SemConstructorCount,
        xl6SemConstructorChild,xl6SemConstructorText,xl6SemConstructorFacts,xl6SemConstructorError,1048576};
    const XL6_AstNode* classNode=owner;
    for(size_t depth=0;classNode&&depth<=sem->types.count;++depth){
        const XL6_AstNode* fields=classNode->childCount>1?classNode->children[1]:NULL;
        for(size_t i=0;fields&&i<fields->childCount;++i){
            const XL6_AstNode* field=fields->children[i];
            if(!field||field->kind!=XL6_AST_FIELD||(field->flags&XL6_AST_FLAG_STATIC)||
                !field->childCount||xl6SemFieldDefaultExpr(field)||
                !xl6SemNativeTypeRequiresInitializer(sem,field->children[0]))continue;
            context.field=field;
            /* All declaration defaults run before the explicit body. They
             * must not expose a receiver with any outstanding obligation. */
            const XL6_AstNode* defaults=owner;
            for(size_t d=0;defaults&&d<=sem->types.count;++d){
                const XL6_AstNode* body=defaults->childCount>1?defaults->children[1]:NULL;
                for(size_t j=0;body&&j<body->childCount;++j){
                    const XL6_AstNode* member=body->children[j];
                    if(member&&member->kind==XL6_AST_FIELD&&!(member->flags&XL6_AST_FLAG_STATIC)&&
                        !xl6ConstructorFlowCheck(&view,(XL6_ConstructorNode)xl6SemFieldDefaultExpr(member),0,0))return 0;
                }
                defaults=xl6SemBaseClassDecl(sem,defaults);
            }
            if(!xl6ConstructorFlowCheck(&view,(XL6_ConstructorNode)(constructor->childCount>1?constructor->children[1]:NULL),0,1))return 0;
        }
        classNode=xl6SemBaseClassDecl(sem,classNode);
    }
    return 1;
}
