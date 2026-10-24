/* Included after the original member-completion enumerator. Its visibility,
 * inheritance and concrete class substitution remain the shared compiler's. */
static int xlsSignatureText(xstrbuf* buffer,const char* text)
{return text && buffer->Size<=32768 && strlen(text)<=32768-buffer->Size && xrtStrBufAppend(buffer,(xstrview){text,strlen(text)});}
static int xlsSignatureType(XL6_SemContext* ctx,xstrbuf* buffer,const XL6_AstNode* type)
{
    char* text=xl6TypeToCanonicalStringLimited(ctx->root,type,8192,128,65536);
    int okay=text && xlsSignatureText(buffer,text);xrtFree(text);return okay;
}
static int xlsSignatureNameIndex(const xls_signature_parameter* params,size_t count,const char* name,size_t size)
{
    for(size_t i=0;i<count;++i)if(params[i].name && strlen(params[i].name)==size && !memcmp(params[i].name,name,size))return (int)i;
    return -1;
}
static void xlsSignatureRank(XL6_SemContext* ctx,xls_signature_scope* scope,const XL6_AstNode* decl,
    const XL6_AstNode* types,xls_signature_record* record)
{
    const XL6_AstNode* args=scope->call && scope->call->childCount>1 ? scope->call->children[1] : NULL;
    size_t previous=scope->data->site.argument;unsigned char provided[128]={0};size_t positional=0;int named=0;
    const XL6_AstNode* typeParams=decl && xl6SemIsFuncDecl(decl) ? xl6SemFuncTypeParams(decl) : NULL;
    record->ranked=!typeParams || !typeParams->childCount || scope->typeArgs;
    XL6_DiagList scratch;xl6DiagInit(&scratch);XL6_DiagList* old=ctx->diags;ctx->diags=&scratch;
    if(!args || args->childCount!=previous+1)record->ranked=0;
    for(size_t i=0;record->ranked && i<previous;++i){
        const XL6_AstNode* arg=args->children[i];const XL6_AstNode* expression=arg && arg->childCount ? arg->children[0] : NULL;
        int index=-1;if(arg && arg->kind==XL6_AST_NAMED_ARG){named=1;if(arg->text)index=xlsSignatureNameIndex(record->parameters,record->count,arg->text,strlen(arg->text));if(index<0 && record->count && record->parameters[record->count-1].variadic==2)index=(int)record->count-1;}
        else if(!named){index=(int)positional++;if((size_t)index>=record->count && record->count && (record->parameters[record->count-1].variadic==1 || record->parameters[record->count-1].variadic==3))index=(int)record->count-1;if(index>=0 && (size_t)index<record->count && record->parameters[index].variadic==2)index=-1;}
        if(index<0 || (size_t)index>=record->count || (!record->parameters[index].variadic && provided[index])){record->ranked=0;break;}
        provided[index]=1;
        int cost=record->parameters[index].variadic ? 200 : expression ? xl6SemExprParamMatchCost(ctx,expression,types->children[index]) : -1;
        uint64 rank=(uint64)record->rank;
        if(cost<0 || !xl6SemCallRankAdd(&rank,(uint32)cost,0))record->ranked=0;
        else record->rank=(uint64_t)rank;
    }
    if(scratch.count)record->ranked=0;
    ctx->diags=old;xl6DiagFree(&scratch);
}
static int xlsSemSignatureOffer(XL6_SemContext* ctx,const XL6_AstNode* decl,const XL6_AstNode* rawType,const char* name)
{
    xls_signature_scope* scope=xlsSignatureScopeCurrent(ctx->completionQuery);
    if(!scope || (scope->targetName && (!name || strcmp(scope->targetName,name))))return 1;
    if(xlsSignatureScopeCancelled(scope)){ctx->completionQuery->error="signature query cancelled";return 0;}
    const XL6_AstNode* type=xl6SemResolveAliasType(ctx,rawType);
    if(!type || !xl6TextEquals(type->text,"function") || type->childCount!=2)return 1;
    const XL6_AstNode* types=type->children[0];const XL6_AstNode* returns=type->children[1];size_t count=types ? types->childCount : 0;
    const XL6_AstNode* declarations=decl && (xl6SemIsFuncDecl(decl) || decl->kind==XL6_AST_CREATE || decl->kind==XL6_AST_NATIVE_EXTERN_SPECIAL) ? xl6SemCallableParams(decl) : NULL;
    if(count>128){ctx->completionQuery->error="signature parameter limit";return 0;}
    xls_signature_record record={.documentation=decl ? decl->doc : NULL,.count=count,.active=SIZE_MAX,.symbol=decl ? decl->semanticSymbolId : 0};
    record.parameters=count ? xrtCalloc(count,sizeof(*record.parameters)) : NULL;xstrbuf label={0};
    const xls_call_site* site=&scope->data->site;
    int okay=(!count || record.parameters) && site->calleeEnd>=site->calleeStart && site->calleeEnd<=scope->data->size && site->calleeEnd-site->calleeStart<=32768 &&
        xrtStrBufAppend(&label,(xstrview){scope->data->source+site->calleeStart,site->calleeEnd-site->calleeStart}) && xlsSignatureText(&label,"(");
    for(size_t i=0;okay && i<count;++i){
        const XL6_AstNode* param=declarations && i<declarations->childCount ? declarations->children[i] : NULL;
        const XL6_AstNode* paramType=types->children[i];
        if(i)okay=xlsSignatureText(&label,", ");
        record.parameters[i].labelStart=label.Size;
        const char* mode=paramType && (paramType->flags&XL6_AST_FLAG_BYVAL) ? "byval " : paramType && (paramType->flags&XL6_AST_FLAG_BYREF) ? "byref " : paramType && (paramType->flags&XL6_AST_FLAG_OUT) ? "out " : "";
        record.parameters[i].name=param ? param->text : NULL;record.parameters[i].optional=param && param->childCount>1;
        const char* typeName=paramType ? paramType->text : NULL;
        record.parameters[i].variadic=xl6TypeTextIsArrayVararg(typeName) ? 1 : xl6TypeTextIsDictVararg(typeName) ? 2 : xl6TypeTextIsCVararg(typeName) ? 3 : 0;
        okay=okay && xlsSignatureText(&label,mode) && xlsSignatureType(ctx,&label,paramType);
        if(okay && record.parameters[i].name)okay=xlsSignatureText(&label," ") && xlsSignatureText(&label,record.parameters[i].name);
        if(okay && record.parameters[i].optional)okay=xlsSignatureText(&label," = ...");
        record.parameters[i].labelEnd=label.Size;
    }
    okay=okay && xlsSignatureText(&label,")");
    if(okay && returns && returns->childCount){okay=xlsSignatureText(&label," as ") && (returns->childCount==1 || xlsSignatureText(&label,"("));
        for(size_t i=0;okay && i<returns->childCount;++i)okay=(!i || xlsSignatureText(&label,", ")) && xlsSignatureType(ctx,&label,returns->children[i]);
        if(okay && returns->childCount>1)okay=xlsSignatureText(&label,")");
    }
    if(!okay){xrtStrBufFree(&label);xrtFree(record.parameters);ctx->completionQuery->error="cannot retain bounded structured signature";return 0;}
    if(site->argumentKnown && count){
        if(site->namedEnd>site->namedStart){int found=xlsSignatureNameIndex(record.parameters,count,scope->data->source+site->namedStart,site->namedEnd-site->namedStart);if(found>=0)record.active=(size_t)found;else if(record.parameters[count-1].variadic==2)record.active=count-1;}
        else if(site->argument<count)record.active=site->argument;
        else if(record.parameters[count-1].variadic)record.active=count-1;
    }
    xlsSignatureRank(ctx,scope,decl,types,&record);record.label=label.Data;record.labelCapacity=label.Capacity;
    if(!xlsSignatureAppend(scope,record)){ctx->completionQuery->error=scope->data->error;return 0;}return 1;
}
static int xlsSignatureFunctions(XL6_SemContext* ctx,xls_signature_scope* scope,XL6_AstNode* name)
{
    const XL6_Symbol* symbol=xl6ScopeFind(ctx->scope,name->text);
    if(!symbol || symbol->kind!=XL6_SYM_FUNC)return 0;
    for(size_t i=xlsSemCallFirst(ctx,name->text);i<ctx->root->childCount && !ctx->completionQuery->error;i=xlsSemCallNext(ctx,i)){
        XL6_AstNode* func=ctx->root->children[i];if(!xl6SemIsFuncDecl(func) || !xl6TextEquals(xl6SemFuncName(func),name->text))continue;
        const XL6_AstNode* parameters=xl6SemFuncTypeParams(func);
        if(scope->typeArgs && (!parameters || parameters->childCount!=scope->typeArgs->childCount || !xl6SemCheckGenericConstraints(ctx,parameters,scope->typeArgs,scope->call,0)))continue;
        XL6_AstNode* type=scope->typeArgs ? xl6SemFunctionTypeFromGenericCallable(func,scope->typeArgs,func) : xl6SemFunctionTypeFromCallableParts(xl6SemCallableParams(func),xl6SemCallableReturns(func),func);
        if(!type){ctx->completionQuery->error="cannot resolve function signature";break;}xlsSemSignatureOffer(ctx,func,type,name->text);xl6AstFree(type);
    }
    return 1;
}
static int xlsSignatureConstructor(XL6_SemContext* ctx,xls_signature_scope* scope,XL6_AstNode* callee)
{
    XL6_AstNode* type=xl6SemTypeNodeFromConstructorCallee(ctx,callee);const XL6_AstNode* concrete=xl6SemResolveAliasType(ctx,type);
    XL6_AstNode* decl=concrete ? xl6SemFindTypeDecl(ctx,concrete->text) : NULL;
    if(!decl || decl->kind!=XL6_AST_CLASS){xl6AstFree(type);return 0;}
    const XL6_AstNode* body=decl->childCount>1 ? decl->children[1] : NULL;const XL6_AstNode* args=xl6SemTypeNodeGenericArgs(concrete);size_t offers=0;
    for(size_t i=0;body && i<body->childCount && !ctx->completionQuery->error;++i){
        const XL6_AstNode* member=body->children[i];if(!member || (member->kind!=XL6_AST_CREATE && member->kind!=XL6_AST_NATIVE_EXTERN_SPECIAL) || !xl6TextEquals(xl6SemMemberName(member),"create") || !xl6SemCanAccessClassMember(ctx,decl,member))continue;
        ++offers;XL6_AstNode* function=xl6SemFunctionTypeFromCallableParts(xl6SemCallableParams(member),NULL,member);
        if(function && args && xl6SemClassHasTypeParams(decl)){XL6_AstNode* replaced=xl6SemSubstituteClassType(decl,args,function);xl6AstFree(function);function=replaced;}
        if(!function){ctx->completionQuery->error="cannot resolve constructor signature";break;}xlsSemSignatureOffer(ctx,member,function,NULL);xl6AstFree(function);
    }
    if(!offers && !xl6SemClassHasCreate(decl)){
        XL6_AstNode* function=xl6SemFunctionTypeFromCallableParts(NULL,NULL,decl);
        if(!function)ctx->completionQuery->error="cannot resolve default constructor signature";
        else{xlsSemSignatureOffer(ctx,decl,function,NULL);
            if(xl6SemClassDeclHasNativeHandle(ctx,decl)){XL6_AstNode* point=xl6AstNewText(XL6_AST_TYPE,"point",5,NULL);if(!point)ctx->completionQuery->error="cannot retain native handle signature";else{xl6AstAddChild(function->children[0],point);xlsSemSignatureOffer(ctx,decl,function,NULL);}}
            xl6AstFree(function);
        }
    }
    xl6AstFree(type);return 1;
}
static int xlsSemSignatureCaptureCall(XL6_SemContext* ctx,XL6_AstNode* node)
{
    xls_signature_scope* scope=xlsSignatureScopeCurrent(ctx->completionQuery);XL6_CompletionQuery* query=ctx->completionQuery;
    if(!scope || !node || node->kind!=XL6_AST_CALL || !node->tokenRange.known || node->tokenRange.startOffset!=scope->data->site.argumentsStart-1 || !node->source || !node->source->completionQuery ||
       node->source->completionStart!=query->replacement.startOffset || node->source->completionEnd!=query->replacement.endOffset || !xl6CompletionPathEqual(xl6AstSourcePath(node),scope->data->path))return 0;
    if(++query->reached!=1){query->error="signature call checked more than once";return 1;}
    if(query->error || (ctx->diags && ctx->diags->count)){if(!query->error)query->error="signature prerequisites contain errors";return 1;}
    scope->call=node;XL6_AstNode* callee=node->childCount ? node->children[0] : NULL;
    if(!callee){query->error="signature call has no callee";return 1;}
    XL6_AstNode* base=callee;if(callee->kind==XL6_AST_GENERIC_INSTANT){base=callee->childCount ? callee->children[0] : NULL;scope->typeArgs=callee->childCount>1 ? callee->children[1] : NULL;}
    if(base && base->kind==XL6_AST_NAME && xlsSignatureFunctions(ctx,scope,base)){}
    else if(xlsSignatureConstructor(ctx,scope,callee)){}
    else if(callee->kind==XL6_AST_MEMBER || callee->kind==XL6_AST_SAFE_MEMBER){scope->targetName=callee->text;xl6SemCompletionMembers(ctx,callee);scope->targetName=NULL;}
    else if(xl6SemCheckExpr(ctx,callee)){
        XL6_AstNode* owned=xl6SemInferAutoTypeNode(ctx,callee);const XL6_AstNode* type=xl6SemResolveAliasType(ctx,owned);
        if(type && xl6TextEquals(type->text,"function"))xlsSemSignatureOffer(ctx,NULL,type,NULL);
        else{XL6_AstNode member={.kind=XL6_AST_MEMBER,.text="__call__",.children=&callee,.childCount=1,.childCapacity=1,.source=callee->source};scope->targetName="__call__";xl6SemCompletionMembers(ctx,&member);scope->targetName=NULL;}
        xl6AstFree(owned);
    }
    query->available=!query->error && !scope->data->error;return 1;
}
