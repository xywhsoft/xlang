/* Logical inputs are packed only into synchronous immutable native records. */
static const char* xl6SemForeignPackScalarCName(const XL6_AstNode* type)
{
    if(!type || type->childCount || !type->text)return NULL;
    static const char* const names[][2]={{"c_int","int"},{"c_uint","unsigned int"},{"c_char","char"},
        {"c_schar","signed char"},{"c_uchar","unsigned char"},{"c_short","short"},{"c_ushort","unsigned short"},
        {"c_long","long"},{"c_ulong","unsigned long"},{"c_longlong","long long"},{"c_ulonglong","unsigned long long"},
        {"c_float","float"},{"c_double","double"},{"c_size","size_t"},{"c_ptrdiff","ptrdiff_t"},
        {"c_intptr","intptr_t"},{"c_uintptr","uintptr_t"},{"int64","int64_t"},{"uint64","uint64_t"},{"bool","bool"}};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);++i)if(!strcmp(type->text,names[i][0]))return names[i][1];
    return NULL;
}
/* Exactly the pointer-free value domain used by borrowed output records.
 * Native descriptor packing does not make a managed record or array POD. */
static int xl6SemForeignTrivialCValue(XL6_SemContext* ctx,const XL6_AstNode* type,size_t depth)
{
    type=xl6SemResolveAliasType(ctx,type);
    if(!type || depth>16 || xl6SemTypeIsNullable(type))return 0;
    if(xl6SemForeignPackScalarCName(type))return 1;
    /* An embedded fixed C array is part of its containing record's value.
     * Top-level arrays still decay in C and are not accepted as value slots. */
    uint64 extent;
    if(depth && type->childCount==1 && xl6ForeignArrayExtent(type->text,&extent) && extent && extent<=UINT32_MAX){
        const XL6_AstNode* args=xl6SemTypeNodeGenericArgs(type);
        return args && args->childCount==1 && xl6SemForeignTrivialCValue(ctx,args->children[0],depth+1);
    }
    const XL6_AstNode* record=type->text && !type->childCount ? xl6SemFindTypeDecl(ctx,type->text):NULL;
    if(!record || record->kind!=XL6_AST_C_RECORD || !record->childCount)return 0;
    const XL6_AstNode* fields=record->children[0];
    for(size_t f=0;fields && f<fields->childCount;++f){
        const XL6_AstNode* field=fields->children[f];
        if(!field || !field->childCount || !xl6SemForeignTrivialCValue(ctx,field->children[0],depth+1))return 0;
    }
    return fields!=NULL;
}
static int xl6SemForeignPacks(XL6_SemContext* ctx,XL6_AstNode* node,const XL6_AstNode* ownerClass,
    const XL6_AstNode* attrs,const XL6_ForeignCallFacts* facts)
{
    const XL6_AstNode* params=xl6SemExternFuncParams(node);size_t count=params ? params->childCount:0;
    size_t packCount=0,fieldCount=0;uint64 consumed=0;
    if(!xl6ForeignInputHasPacks(facts->inputMappingFacts) && !xl6ForeignInputHasFixedInputs(facts->inputMappingFacts))return 1;
    char header[256]={0};XL6_ForeignClassDecl owner={0};
    if(ownerClass && xl6ForeignClassRead(ownerClass,&owner,NULL)==1)strcpy(header,owner.header);
    for(size_t i=0;attrs && i<attrs->childCount;++i)if(xl6ForeignFunctionHeaderRead(attrs->children[i],header,sizeof(header)))break;
    if(!header[0] || !params || (node->kind!=XL6_AST_EXTERN_FUNC && node->kind!=XL6_AST_NATIVE_EXTERN_FUNC) || !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
        xl6ForeignInputHasRegions(facts->inputMappingFacts))goto invalid;
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        XL6_ForeignPackContract pack;if(!xl6ForeignPackRead(attrs->children[a],&pack))continue;
        ++packCount;if(pack.firstParameter>count || pack.parameterCount>count-pack.firstParameter)goto invalid;
        for(size_t p=pack.firstParameter;p<pack.firstParameter+pack.parameterCount;++p){
            if(consumed&(UINT64_C(1)<<p))goto invalid;
            if(!params->children[p] || !params->children[p]->childCount ||
                xl6SemTypeIsCVararg(params->children[p]->children[0]) ||
                xl6SemTypeIsArrayVararg(params->children[p]->children[0]) ||
                xl6SemTypeIsDictVararg(params->children[p]->children[0]))goto invalid;
            consumed|=UINT64_C(1)<<p;
        }
        for(size_t b=0;b<a;++b){XL6_ForeignPackContract previous;
            if(xl6ForeignPackRead(attrs->children[b],&previous) && previous.physicalIndex==pack.physicalIndex)goto invalid;}
        uint64 used=0,textData=0,textSize=0,spanData=0,spanCount=0,callbacks=0,callbackUsers=0;
        size_t fields=0;
        for(size_t f=0;f<attrs->childCount;++f){
            XL6_ForeignPackFieldContract field;if(!xl6ForeignPackFieldRead(attrs->children[f],&field) || field.packPhysicalIndex!=pack.physicalIndex)continue;
            ++fields;++fieldCount;
            for(size_t old=0;old<f;++old){XL6_ForeignPackFieldContract previous;
                if(xl6ForeignPackFieldRead(attrs->children[old],&previous) && previous.packPhysicalIndex==field.packPhysicalIndex && !strcmp(previous.field,field.field))goto invalid;}
            size_t parent=xl6ForeignPackParentLength(field.field),parents=0;
            for(size_t nested=0;nested<attrs->childCount;++nested){XL6_ForeignPackFieldContract witness;
                if(xl6ForeignPackFieldRead(attrs->children[nested],&witness) && witness.packPhysicalIndex==field.packPhysicalIndex &&
                    witness.kind==XL6_FOREIGN_PACK_FIELD_RECORD && strlen(witness.field)==parent && !strncmp(witness.field,field.field,parent))++parents;}
            if(parent ? parents!=1:parents!=0)goto invalid;
            if(field.kind==XL6_FOREIGN_PACK_FIELD_SIZEOF || field.kind==XL6_FOREIGN_PACK_FIELD_RECORD)continue;
            size_t p=field.parameter;if(p<pack.firstParameter || p-pack.firstParameter>=pack.parameterCount)goto invalid;
            const XL6_AstNode* type=xl6SemResolveAliasType(ctx,params->children[p]->children[0]);
            const XL6_AstNode* inner=type && xl6SemTypeIsNullable(type) ? xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(type)):type;
            used|=UINT64_C(1)<<p;
            if(field.kind==XL6_FOREIGN_PACK_FIELD_VALUE){if(!xl6SemForeignTrivialCValue(ctx,type,0))goto invalid;}
            else if(field.kind==XL6_FOREIGN_PACK_FIELD_VIEW_RAW){XL6_ForeignClassDecl view;
                if(!xl6SemForeignViewDeclaration(ctx,type,&view) || view.lifetime==XL6_FOREIGN_LIFETIME_CALLER)goto invalid;
            }else if(field.kind==XL6_FOREIGN_PACK_FIELD_SPAN_DATA || field.kind==XL6_FOREIGN_PACK_FIELD_SPAN_COUNT){
                XL6_ForeignParameterContract span;if(!xl6SemExternParameterExpansion(node,p,&span) || span.kind!=XL6_FOREIGN_PARAMETER_SPAN ||
                    !span.readOnly || !xl6SemForeignSpanType(ctx,type,&span))goto invalid;
                uint64* bits=field.kind==XL6_FOREIGN_PACK_FIELD_SPAN_DATA ? &spanData:&spanCount;
                if(*bits&(UINT64_C(1)<<p))goto invalid;
                *bits|=UINT64_C(1)<<p;
            }else if(field.kind==XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK || field.kind==XL6_FOREIGN_PACK_FIELD_CALLBACK_USER){
                XL6_ForeignParameterContract callback;
                if(!xl6SemExternParameterExpansion(node,p,&callback) || callback.kind!=XL6_FOREIGN_PARAMETER_SYNC_CALLBACK)goto invalid;
                uint64* bits=field.kind==XL6_FOREIGN_PACK_FIELD_SYNC_CALLBACK ? &callbacks:&callbackUsers;
                if(*bits&(UINT64_C(1)<<p))goto invalid;
                *bits|=UINT64_C(1)<<p;
            }else{
                if(!inner || inner->childCount || !xl6TextEquals(inner->text,"string"))goto invalid;
                if(field.kind==XL6_FOREIGN_PACK_FIELD_TEXT_DATA || field.kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE){
                    uint64* bits=field.kind==XL6_FOREIGN_PACK_FIELD_TEXT_DATA ? &textData:&textSize;
                    if(*bits&(UINT64_C(1)<<p))goto invalid;
                    *bits|=UINT64_C(1)<<p;
                }else if(field.kind!=XL6_FOREIGN_PACK_FIELD_C_STRING)goto invalid;
            }
        }
        uint64 expected=pack.parameterCount==64 ? UINT64_MAX:((UINT64_C(1)<<pack.parameterCount)-1)<<pack.firstParameter;
        if(!fields || used!=expected || textData!=textSize || spanData!=spanCount || callbacks!=callbackUsers)goto invalid;
    }
    if((packCount!=0)!=(fieldCount!=0))goto invalid;
    {
        size_t receiver=(node->kind==XL6_AST_NATIVE_EXTERN_FUNC || node->kind==XL6_AST_GET || node->kind==XL6_AST_LET) &&
            !(node->flags&XL6_AST_FLAG_STATIC) ? 1u:0u;
        uint32 outputs[64];size_t outputCount;
        if(!xl6ForeignPhysicalOutputSlots(node,facts,outputs,&outputCount))goto invalid;
        size_t steps=xl6ForeignPhysicalStepCount(node,count+receiver,outputCount);
        if(steps>count+receiver+outputCount+64)goto invalid;
        for(size_t i=0;i<steps;++i)
            if(!xl6ForeignPhysicalStepRead(node,facts,count+receiver,receiver,outputs,outputCount,i,NULL))goto invalid;
        for(size_t a=0;attrs && a<attrs->childCount;++a){XL6_ForeignFixedInputContract nullRow;
            if(!xl6ForeignFixedInputRead(attrs->children[a],&nullRow))continue;
            size_t found=0;for(size_t i=0;i<steps;++i){XL6_ForeignPhysicalStep step;
                if(!xl6ForeignPhysicalStepRead(node,facts,count+receiver,receiver,outputs,outputCount,i,&step))goto invalid;
                found+=step.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED && step.physicalIndex==nullRow.physicalIndex;}
            if(found!=1 || (receiver && !nullRow.physicalIndex))goto invalid;
        }
        if(receiver<64 && ((consumed<<receiver)&facts->controlViews))goto invalid;
        for(size_t a=0;attrs && a<attrs->childCount;++a){uint32 qualifier;XL6_ForeignValueContract value;
            if(xl6ForeignPointerQualifierRead(attrs->children[a],&qualifier) && qualifier>=receiver &&
                qualifier-receiver<64 && (consumed&(UINT64_C(1)<<(qualifier-receiver))))goto invalid;
            if(xl6ForeignValueRead(attrs->children[a],&value) && value.parameter>=receiver &&
                value.parameter-receiver<64 && (consumed&(UINT64_C(1)<<(value.parameter-receiver))))goto invalid;
        }
    }
    for(size_t f=0;attrs && f<attrs->childCount;++f){XL6_ForeignPackFieldContract field;
        if(!xl6ForeignPackFieldRead(attrs->children[f],&field))continue;
        size_t found=0;for(size_t p=0;p<attrs->childCount;++p){XL6_ForeignPackContract pack;
            if(xl6ForeignPackRead(attrs->children[p],&pack) && pack.physicalIndex==field.packPhysicalIndex)++found;}
        if(found!=1)goto invalid;
    }
    return 1;
invalid:xl6SemErrorNode(ctx,node,"native descriptor packs require disjoint complete logical input groups, exact paired read-only fields and an authoritative header");return 0;
}
