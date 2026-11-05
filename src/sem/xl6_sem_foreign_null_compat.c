/* A fixed typed NULL can omit one const POD reference/descriptor input only.
 * This does not equate arbitrary logical signatures or consume an owner. */
static int xl6SemForeignTrivialCValue(XL6_SemContext*,const XL6_AstNode*,size_t);
typedef struct XL6_SemNullPhysical {
    const XL6_AstNode* node;
    const XL6_AstNode* params;
    XL6_ForeignCallFacts facts;
    uint32 outputs[64],map[64];
    size_t receiver,parameters,outputCount,steps;
} XL6_SemNullPhysical;
static int xl6SemNullPhysicalRead(const XL6_AstNode* node,XL6_SemNullPhysical* map)
{
    memset(map,0,sizeof(*map));map->node=node;map->params=xl6SemExternFuncParams(node);
    if(!map->params || !xl6ForeignFunctionRead(node,&map->facts))return 0;
    map->receiver=node->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    map->parameters=map->params->childCount+map->receiver;
    if(map->parameters>64 || !xl6ForeignPhysicalOutputSlots(node,&map->facts,map->outputs,&map->outputCount))return 0;
    map->steps=xl6ForeignPhysicalStepCount(node,map->parameters,map->outputCount);
    if(map->steps>map->parameters+map->outputCount+64)return 0;
    for(size_t i=0;i<64;++i)map->map[i]=UINT32_MAX;
    return 1;
}
static int xl6SemNullStep(const XL6_SemNullPhysical* map,size_t i,XL6_ForeignPhysicalStep* step)
{return xl6ForeignPhysicalStepRead(map->node,&map->facts,map->parameters,map->receiver,map->outputs,map->outputCount,i,step);}
static int xl6SemNullHeader(const XL6_AstNode* node,const XL6_AstNode* owner,char* header,size_t size)
{
    header[0]=0;XL6_ForeignClassDecl type={0};
    if(owner && xl6ForeignClassRead(owner,&type,NULL)==1){if(strlen(type.header)>=size)return 0;strcpy(header,type.header);}
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);
    for(size_t i=0;attrs && i<attrs->childCount;++i)if(xl6ForeignFunctionHeaderRead(attrs->children[i],header,size))break;
    return header[0] && xl6ForeignHeaderName(header);
}
static int xl6SemNullBit(uint64 bits,size_t p)
{return p<64 && (bits&(UINT64_C(1)<<p))!=0;}
static unsigned xl6SemNullFlags(const XL6_SemNullPhysical* m,size_t p)
{
    size_t explicitIndex=p>=m->receiver ? p-m->receiver:SIZE_MAX;const XL6_ForeignCallFacts* f=&m->facts;
    return xl6SemNullBit(f->constAggregateAddresses,explicitIndex)|
        xl6SemNullBit(f->staticCallbackAddresses,explicitIndex)<<1|xl6SemNullBit(f->textViews,explicitIndex)<<2|
        xl6SemNullBit(f->bytesViews,explicitIndex)<<3|xl6SemNullBit(f->terminatedStrings,explicitIndex)<<4|
        xl6SemNullBit(f->physicalPairs,explicitIndex)<<5|xl6SemNullBit(f->retainedCallbacks,explicitIndex)<<6|
        xl6SemNullBit(f->controlViews,p)<<7;
}
static int xl6SemNullOmittedType(XL6_SemContext* ctx,const XL6_SemNullPhysical* full,
    const XL6_ForeignPhysicalStep* step,const char* pointer)
{
    char expected[128];
    if(step->kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){XL6_ForeignPackContract pack;
        if(step->parameter<full->receiver || !xl6ForeignPackForParameter(full->node,step->parameter-full->receiver,&pack) ||
            pack.firstParameter+full->receiver!=step->parameter || pack.physicalIndex!=step->physicalIndex)return 0;
        snprintf(expected,sizeof(expected),"const %s*",pack.recordCName);
        return !strcmp(pointer,expected);
    }
    if(step->kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT || step->parameter<full->receiver ||
        xl6SemNullFlags(full,step->parameter)!=1)return 0;
    const XL6_AstNode* parameter=full->params->children[step->parameter-full->receiver];
    const XL6_AstNode* type=parameter && parameter->childCount ? xl6SemResolveAliasType(ctx,parameter->children[0]):NULL;
    const XL6_AstNode* record=type && type->text && !type->childCount ? xl6SemFindTypeDecl(ctx,type->text):NULL;
    XL6_ForeignClassDecl native={0};
    if(!type || xl6SemTypeIsNullable(type) || !record || xl6ForeignRecordRead(record,&native,NULL)!=1 ||
        !xl6SemForeignTrivialCValue(ctx,type,0) || xl6SemParamMode(parameter))return 0;
    snprintf(expected,sizeof(expected),"const %s*",native.cName);return !strcmp(pointer,expected);
}
static int xl6SemNullInputAttr(const XL6_AstNode* attr)
{
    static const char* const names[]={"c_pack","c_pack_field","c_pack_record","c_pack_size","c_null","c_code_owner",
        "c_ref","c_const","c_control","c_value","c_static_callback","c_cstr","c_text_view","c_bytes_view",
        "c_span","c_owned_callback","c_sync_callback","c_callback_span","c_result"};
    for(size_t i=0;attr && attr->text && i<sizeof(names)/sizeof(*names);++i)if(!strcmp(attr->text,names[i]))return 1;
    return 0;
}
static int xl6SemNullOtherAttrs(const XL6_AstNode* a,const XL6_AstNode* b)
{
    a=xl6ForeignFunctionAttributes(a);b=xl6ForeignFunctionAttributes(b);
    size_t left=0,right=0;
    for(size_t i=0;a && i<a->childCount;++i){if(xl6SemNullInputAttr(a->children[i]))continue;
        ++left;size_t matches=0;for(size_t j=0;b && j<b->childCount;++j)
            if(!xl6SemNullInputAttr(b->children[j]))matches+=xl6SemForeignAttributeEqual(a->children[i],b->children[j]);
        if(matches!=1)return 0;
    }
    for(size_t i=0;b && i<b->childCount;++i)right+=!xl6SemNullInputAttr(b->children[i]);
    return left==right;
}
static int xl6SemNullParamMeta(const XL6_SemNullPhysical* a,size_t p,const XL6_SemNullPhysical* b,size_t q)
{
    if(xl6SemNullFlags(a,p)!=xl6SemNullFlags(b,q))return 0;
    const XL6_AstNode* aa=xl6ForeignFunctionAttributes(a->node);const XL6_AstNode* bb=xl6ForeignFunctionAttributes(b->node);
    XL6_ForeignValueContract x={0},y={0};int xv=0,yv=0,xq=0,yq=0;
    for(size_t i=0;aa && i<aa->childCount;++i){uint32 index;XL6_ForeignValueContract value;
        if(xl6ForeignPointerQualifierRead(aa->children[i],&index) && index==p)++xq;
        if(xl6ForeignValueRead(aa->children[i],&value) && value.parameter==p){x=value;++xv;}}
    for(size_t i=0;bb && i<bb->childCount;++i){uint32 index;XL6_ForeignValueContract value;
        if(xl6ForeignPointerQualifierRead(bb->children[i],&index) && index==q)++yq;
        if(xl6ForeignValueRead(bb->children[i],&value) && value.parameter==q){y=value;++yv;}}
    if(xq!=yq || xv!=yv || (xv && strcmp(x.cName,y.cName)))return 0;
    if(p<a->receiver || q<b->receiver)return p<a->receiver && q<b->receiver;
    XL6_ForeignParameterContract xp={0},yp={0};int xe=xl6SemExternParameterExpansion(a->node,p-a->receiver,&xp),ye=xl6SemExternParameterExpansion(b->node,q-b->receiver,&yp);
    if(xe!=ye)return 0;
    if(xe){
        if(xp.callbackOwnerIndexPlusOne){size_t owner=xp.callbackOwnerIndexPlusOne-1;if(owner>=a->parameters || a->map[owner]==UINT32_MAX)return 0;xp.callbackOwnerIndexPlusOne=a->map[owner]+1;}
        if(yp.callbackOwnerIndexPlusOne){size_t owner=yp.callbackOwnerIndexPlusOne-1;if(owner>=b->parameters || b->map[owner]==UINT32_MAX)return 0;yp.callbackOwnerIndexPlusOne=b->map[owner]+1;}
        xp.parameter=yp.parameter=0;if(memcmp(&xp,&yp,sizeof(xp)))return 0;
    }
    size_t lc=0,rc=0;
    for(size_t i=0;aa && i<aa->childCount;++i){XL6_ForeignCallbackSpanContract span;if(!xl6ForeignCallbackSpanRead(aa->children[i],&span) || span.parameter!=p-a->receiver)continue;
        ++lc;size_t matches=0;span.parameter=0;
        for(size_t j=0;bb && j<bb->childCount;++j){XL6_ForeignCallbackSpanContract other;if(!xl6ForeignCallbackSpanRead(bb->children[j],&other) || other.parameter!=q-b->receiver)continue;
            other.parameter=0;matches+=!memcmp(&span,&other,sizeof(span));}
        if(matches!=1)return 0;
    }
    for(size_t j=0;bb && j<bb->childCount;++j){XL6_ForeignCallbackSpanContract span;if(xl6ForeignCallbackSpanRead(bb->children[j],&span) && span.parameter==q-b->receiver)++rc;}
    return lc==rc;
}
static int xl6SemNullPacksEqual(const XL6_SemNullPhysical* a,const XL6_ForeignPhysicalStep* as,
    const XL6_SemNullPhysical* b,const XL6_ForeignPhysicalStep* bs)
{
    XL6_ForeignPackContract ap,bp;
    if(as->parameter<a->receiver || bs->parameter<b->receiver || !xl6ForeignPackForParameter(a->node,as->parameter-a->receiver,&ap) ||
        !xl6ForeignPackForParameter(b->node,bs->parameter-b->receiver,&bp) || ap.parameterCount!=bp.parameterCount || strcmp(ap.recordCName,bp.recordCName))return 0;
    const XL6_AstNode* aa=xl6ForeignFunctionAttributes(a->node);const XL6_AstNode* bb=xl6ForeignFunctionAttributes(b->node);size_t left=0,right=0;
    for(size_t i=0;aa && i<aa->childCount;++i){XL6_ForeignPackFieldContract row;if(!xl6ForeignPackFieldRead(aa->children[i],&row) || row.packPhysicalIndex!=as->physicalIndex)continue;
        ++left;if(row.parameter!=UINT32_MAX)row.parameter-=ap.firstParameter;size_t matches=0;
        for(size_t j=0;bb && j<bb->childCount;++j){XL6_ForeignPackFieldContract other;if(!xl6ForeignPackFieldRead(bb->children[j],&other) || other.packPhysicalIndex!=bs->physicalIndex)continue;
            if(other.parameter!=UINT32_MAX)other.parameter-=bp.firstParameter;
            matches+=!memcmp(&row,&other,sizeof(row));}
        if(matches!=1)return 0;
    }
    for(size_t j=0;bb && j<bb->childCount;++j){XL6_ForeignPackFieldContract row;if(xl6ForeignPackFieldRead(bb->children[j],&row) && row.packPhysicalIndex==bs->physicalIndex)++right;}
    return left==right;
}
static int xl6SemForeignNullPhysicalCompatible(XL6_SemContext* ctx,const XL6_AstNode* left,
    const XL6_AstNode* right,const XL6_AstNode* owner)
{
    XL6_SemNullPhysical a,b;if(!xl6SemNullPhysicalRead(left,&a) || !xl6SemNullPhysicalRead(right,&b))return 0;
    if(!xl6ForeignInputHasFixedInputs(a.facts.inputMappingFacts) && !xl6ForeignInputHasFixedInputs(b.facts.inputMappingFacts))return -1;
    int changed=0;
    if(a.steps!=b.steps)return 0;
    for(size_t i=0;i<a.steps;++i){XL6_ForeignPhysicalStep x,y;if(!xl6SemNullStep(&a,i,&x) || !xl6SemNullStep(&b,i,&y))return 0;
        if(x.physicalIndex!=y.physicalIndex)return 0;
        if((x.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED)!=(y.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED)){
            const XL6_SemNullPhysical* fixed=x.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED ? &a:&b;
            const XL6_SemNullPhysical* full=fixed==&a ? &b:&a;const XL6_ForeignPhysicalStep* omitted=fixed==&a ? &y:&x;
            XL6_ForeignFixedInputContract row;
            if(!xl6ForeignFixedInputAt(fixed->node,x.physicalIndex,&row) || row.kind!=XL6_FOREIGN_FIXED_TYPED_NULL ||
                !xl6SemNullOmittedType(ctx,full,omitted,row.pointerCName))return 0;
            ++changed;
        }
    }
    if(!changed)return -1;
    const uint64 flags=XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_NO_RAISE|XL6_AST_FLAG_FOREIGN_NONBLOCKING|XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    char ah[256],bh[256];
    if(left->kind!=right->kind || (left->kind!=XL6_AST_EXTERN_FUNC && left->kind!=XL6_AST_NATIVE_EXTERN_FUNC) ||
        !left->text || !right->text || strcmp(left->text,right->text) || ((left->flags^right->flags)&flags) ||
        !(left->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || ((left->flags|right->flags)&XL6_AST_FLAG_FOREIGN_FIELD) || a.receiver!=b.receiver ||
        !xl6SemNullHeader(left,owner,ah,sizeof(ah)) || !xl6SemNullHeader(right,owner,bh,sizeof(bh)) || strcmp(ah,bh) ||
        xl6ForeignInputHasRegions(a.facts.inputMappingFacts) || xl6ForeignInputHasRegions(b.facts.inputMappingFacts) ||
        xl6ForeignInputHasArrayGroups(a.facts.inputMappingFacts) || xl6ForeignInputHasArrayGroups(b.facts.inputMappingFacts) ||
        !xl6SemExternCReturnsCompatible(ctx,xl6SemExternFuncReturns(left),xl6SemExternFuncReturns(right)))return 0;
    for(size_t i=0;i<a.steps;++i){XL6_ForeignPhysicalStep x,y;if(!xl6SemNullStep(&a,i,&x) || !xl6SemNullStep(&b,i,&y))return 0;
        if(x.kind!=y.kind)continue;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_INPUT){a.map[x.parameter]=x.physicalIndex*65;b.map[y.parameter]=y.physicalIndex*65;}
        else if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){XL6_ForeignPackContract ap,bp;
            if(!xl6ForeignPackForParameter(left,x.parameter-a.receiver,&ap) || !xl6ForeignPackForParameter(right,y.parameter-b.receiver,&bp) || ap.parameterCount!=bp.parameterCount)return 0;
            for(size_t n=0;n<ap.parameterCount;++n){a.map[x.parameter+n]=x.physicalIndex*65+(uint32)n;b.map[y.parameter+n]=y.physicalIndex*65+(uint32)n;}}
    }
    for(size_t i=0;i<a.steps;++i){XL6_ForeignPhysicalStep x,y;if(!xl6SemNullStep(&a,i,&x) || !xl6SemNullStep(&b,i,&y))return 0;
        if(x.kind!=y.kind)continue;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_OUTPUT){if(x.ordinal!=y.ordinal)return 0;continue;}
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED){XL6_ForeignFixedInputContract xf,yf;
            if(!xl6ForeignFixedInputAt(left,x.physicalIndex,&xf) || !xl6ForeignFixedInputAt(right,y.physicalIndex,&yf) || memcmp(&xf,&yf,sizeof(xf)))return 0;
            continue;}
        size_t count=1;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){XL6_ForeignPackContract p;if(!xl6SemNullPacksEqual(&a,&x,&b,&y) || !xl6ForeignPackForParameter(left,x.parameter-a.receiver,&p))return 0;count=p.parameterCount;}
        else if(x.kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT)return 0;
        for(size_t n=0;n<count;++n){size_t pa=x.parameter+n,pb=y.parameter+n;
            if(!xl6SemNullParamMeta(&a,pa,&b,pb))return 0;
            if(pa<a.receiver || pb<b.receiver){if(pa>=a.receiver || pb>=b.receiver)return 0;continue;}
            const XL6_AstNode* ap=a.params->children[pa-a.receiver];const XL6_AstNode* bp=b.params->children[pb-b.receiver];
            if(!ap || !bp || !ap->childCount || !bp->childCount || xl6SemParamMode(ap)!=xl6SemParamMode(bp) || !xl6SemTypeEquals(ctx,ap->children[0],bp->children[0]))return 0;
        }
    }
    XL6_ForeignCallFacts af=a.facts,bf=b.facts;
    if(af.resultOwnerIndexPlusOne || bf.resultOwnerIndexPlusOne){
        size_t ap=af.resultOwnerIndexPlusOne-1,bp=bf.resultOwnerIndexPlusOne-1;
        if(!af.resultOwnerIndexPlusOne || !bf.resultOwnerIndexPlusOne || ap>=a.parameters || bp>=b.parameters || a.map[ap]==UINT32_MAX || a.map[ap]!=b.map[bp])return 0;
    }
    af.constAggregateAddresses=bf.constAggregateAddresses=af.staticCallbackAddresses=bf.staticCallbackAddresses=0;
    af.textViews=bf.textViews=af.bytesViews=bf.bytesViews=af.terminatedStrings=bf.terminatedStrings=0;
    af.physicalPairs=bf.physicalPairs=af.retainedCallbacks=bf.retainedCallbacks=af.controlViews=bf.controlViews=0;
    af.resultOwnerIndexPlusOne=bf.resultOwnerIndexPlusOne=0;af.inputMappingFacts=bf.inputMappingFacts=0;
    return xl6ForeignCallFactsEqual(&af,&bf) && xl6SemNullOtherAttrs(left,right);
}
