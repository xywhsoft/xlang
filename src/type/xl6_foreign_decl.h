#ifndef XL6_FOREIGN_DECL_H
#define XL6_FOREIGN_DECL_H

#include "src/ast/xl6_ast.h"
#include "src/type/xl6_foreign_call_contract.h"
#include <string.h>

/* Declaration facts only. Backends consume the corresponding closed XLIR
 * contract, never these AST nodes. A caller lifetime is explicit: this view
 * does not retain, destroy, or extend the lifetime of its C object. */
typedef struct XL6_ForeignClassDecl {
    char cName[64];
    char header[256];
    XL6_ForeignLifetimeAbi lifetime;
    char policy[64],scope[64],watch[64],owner[256];
    uint32 ownerSymbol;
} XL6_ForeignClassDecl;

static inline const XL6_AstNode* xl6ForeignFunctionAttributes(const XL6_AstNode* node)
{
    if (!node || !node->childCount) return NULL;
    const XL6_AstNode* last = node->children[node->childCount - 1];
    return last && last->kind == XL6_AST_FIELD_ATTR && last->text &&
        !strcmp(last->text, "c_call") ? last : NULL;
}

static inline int xl6ForeignDecimalInt(const XL6_AstNode* node, int32* output)
{
    int negative = node && node->kind == XL6_AST_UNARY && node->text &&
        !strcmp(node->text, "-") && node->childCount == 1;
    if (negative) node = node->children[0];
    if (!node || node->kind != XL6_AST_INT || !node->text || !*node->text) return 0;
    uint64 value = 0, limit = negative ? UINT64_C(2147483648) : INT32_MAX;
    for (const unsigned char* p = (const unsigned char*)node->text; *p; ++p) {
        if (*p < '0' || *p > '9' || value > (limit - (*p - '0')) / 10u) return 0;
        value = value * 10u + (*p - '0');
    }
    *output = negative ? (int32)(-(int64)value) : (int32)value;
    return 1;
}

static inline int xl6ForeignQuotedText(const XL6_AstNode*,char*,size_t);
static inline int xl6ForeignIdentifier(const char*);
static inline int xl6ForeignPointerSpelling(const char*);
static inline int xl6ForeignHeaderName(const char*);

static inline int xl6ForeignCallbackSpanRead(const XL6_AstNode* attr,XL6_ForeignCallbackSpanContract* output)
{
    XL6_ForeignCallbackSpanContract row={0};int32 parameter,argument;char count[32];
    if(!attr || !attr->text || strcmp(attr->text,"c_callback_span") || attr->childCount!=4 ||
        !xl6ForeignDecimalInt(attr->children[0],&parameter) || parameter<0 || parameter>=64 ||
        !xl6ForeignDecimalInt(attr->children[1],&argument) || argument<0 || argument>=64 ||
        !xl6ForeignQuotedText(attr->children[2],row.pointerCName,sizeof(row.pointerCName)) ||
        !xl6ForeignQuotedText(attr->children[3],count,sizeof(count)))return 0;
    if(strcmp(row.pointerCName,"const void*") && strcmp(row.pointerCName,"const uint8_t*") &&
        strcmp(row.pointerCName,"const unsigned char*") && strcmp(row.pointerCName,"const char*"))return 0;
    if(!strcmp(count,"int"))row.countAbi=XL6_FOREIGN_SPAN_COUNT_INT;
    else if(!strcmp(count,"size_t"))row.countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;
    else return 0;
    row.parameter=(uint32)parameter;row.argument=(uint32)argument;
    if(output)*output=row;
    return 1;
}

static inline int xl6ForeignRetainRead(const XL6_AstNode* attr,XL6_ForeignRetainContract* output)
{
    XL6_ForeignRetainContract row={0};char success[32];
    if(!attr || !attr->text || strcmp(attr->text,"c_retain") || attr->childCount!=3 ||
        !attr->children[0] || attr->children[0]->kind!=XL6_AST_NAME || !attr->children[0]->text ||
        !xl6ForeignQuotedText(attr->children[1],success,sizeof(success)) ||
        !attr->children[2] || attr->children[2]->kind!=XL6_AST_NAME || !attr->children[2]->text)return 0;
    if(!strcmp(success,"zero"))row.success=XL6_FOREIGN_RETAIN_ZERO;
    else if(!strcmp(success,"positive"))row.success=XL6_FOREIGN_RETAIN_POSITIVE;
    else return 0;
    row.retainSymbol=attr->children[0]->semanticSymbolId;
    row.rollbackSymbol=attr->children[2]->semanticSymbolId;
    if(output)*output=row;
    return 1;
}

static inline int xl6ForeignStorageRead(const XL6_AstNode* attr,XL6_ForeignStorageContract* output)
{
    XL6_ForeignStorageContract row={0};
    if(!attr || !attr->text || strcmp(attr->text,"c_inplace") || (attr->childCount!=1 && attr->childCount!=3) ||
        !xl6ForeignQuotedText(attr->children[0],row.recordCName,sizeof(row.recordCName)) ||
        !xl6ForeignIdentifier(!strncmp(row.recordCName,"struct ",7) ? row.recordCName+7 : row.recordCName))return 0;
    if(attr->childCount==1){row.kind=XL6_FOREIGN_STORAGE_POLICY;if(output)*output=row;return 1;}
    if(
        !attr->children[1] || attr->children[1]->kind!=XL6_AST_NAME || !attr->children[1]->text ||
        !xl6ForeignDecimalInt(attr->children[2],&row.successCode))return 0;
    row.kind=XL6_FOREIGN_STORAGE_NATIVE_ADOPTION;
    row.adoptSymbol=attr->children[1]->semanticSymbolId;
    if(output)*output=row;
    return 1;
}

static inline int xl6ForeignFieldLengthRead(const XL6_AstNode* attr,XL6_ForeignFieldLengthContract* output)
{
    XL6_ForeignFieldLengthContract row={0};char count[32];
    if(!attr || !attr->text || strcmp(attr->text,"c_length") || attr->childCount!=2 ||
        !xl6ForeignQuotedText(attr->children[0],row.field,sizeof(row.field)) || !xl6ForeignIdentifier(row.field) ||
        !xl6ForeignQuotedText(attr->children[1],count,sizeof(count)))return 0;
    if(!strcmp(count,"int"))row.countAbi=XL6_FOREIGN_SPAN_COUNT_INT;
    else if(!strcmp(count,"size_t"))row.countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;
    else return 0;
    if(output)*output=row;
    return 1;
}

static inline int xl6ForeignValueRead(const XL6_AstNode* attr,XL6_ForeignValueContract* output)
{
    XL6_ForeignValueContract row={0};int32 p;
    if(!attr || !attr->text || strcmp(attr->text,"c_value") || attr->childCount!=2 ||
        !xl6ForeignDecimalInt(attr->children[0],&p) || p<0 || p>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.cName,sizeof(row.cName)) ||
        !xl6ForeignIdentifier(!strncmp(row.cName,"struct ",7) ? row.cName+7 : row.cName))return 0;
    row.parameter=(uint32)p;if(output)*output=row;return 1;
}

static inline int xl6ForeignParameterRead(const XL6_AstNode* attr,
    XL6_ForeignParameterContract* output)
{
    XL6_ForeignParameterContract result={0};
    int32 index;
    if(!attr || !attr->text || !attr->childCount ||
        !xl6ForeignDecimalInt(attr->children[0],&index) || index<0 || index>=64)return 0;
    result.parameter=(uint32)index;
    if(!strcmp(attr->text,"c_span")) {
        char count[32];
        if((attr->childCount!=3 && attr->childCount!=4) || !xl6ForeignQuotedText(attr->children[1],result.pointerCName,sizeof(result.pointerCName)) ||
            !xl6ForeignPointerSpelling(result.pointerCName) ||
            !xl6ForeignQuotedText(attr->children[2],count,sizeof(count)))return 0;
        if(attr->childCount==4 && (!xl6ForeignQuotedText(attr->children[3],result.header,sizeof(result.header)) ||
            !xl6ForeignHeaderName(result.header)))return 0;
        result.kind=XL6_FOREIGN_PARAMETER_SPAN;
        if(!strcmp(count,"size_t"))result.countAbi=XL6_FOREIGN_SPAN_COUNT_SIZE;
        else if(!strcmp(count,"int"))result.countAbi=XL6_FOREIGN_SPAN_COUNT_INT;
        else return 0;
        const char* element=result.pointerCName;
        if(!strncmp(element,"const ",6)){result.readOnly=true;element+=6;}
        const char* end=strchr(element,'*');
        if(!end || end[1])return 0; /* Exactly one explicit pointer level. */
        size_t bytes=(size_t)(end-element);
        while(bytes && element[bytes-1]==' ')--bytes;
        if(!bytes || bytes>=sizeof(result.elementCName))return 0;
        memcpy(result.elementCName,element,bytes);result.elementCName[bytes]=0;
    } else if(!strcmp(attr->text,"c_owned_callback")) {
        int32 owner;
        if((attr->childCount<4 || attr->childCount>6) ||
            !xl6ForeignDecimalInt(attr->children[1],&owner) || owner<0 || owner>=64 ||
            !xl6ForeignQuotedText(attr->children[2],result.callbackOwnerCName,sizeof(result.callbackOwnerCName)) ||
            !xl6ForeignIdentifier(result.callbackOwnerCName) ||
            !xl6ForeignDecimalInt(attr->children[3],&result.callbackFailureCode))return 0;
        result.kind=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK;
        result.callbackOwnerIndexPlusOne=(uint32)owner+1;
        if(attr->childCount>=5) {
            char order[16];
            if(!xl6ForeignQuotedText(attr->children[4],order,sizeof(order)))return 0;
            if(!strcmp(order,"first"))result.callbackUserFirst=true;
            else if(!strcmp(order,"owned_text") && attr->childCount==5)result.callbackResultKind=XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT;
            else if(strcmp(order,"last"))return 0;
        }
        if(attr->childCount==6){
            char kind[32];
            if(!xl6ForeignQuotedText(attr->children[5],kind,sizeof(kind)) || strcmp(kind,"owned_text"))return 0;
            result.callbackResultKind=XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT;
        }
    } else return 0;
    if(output)*output=result;
    return 1;
}

static inline int xl6ForeignFunctionRead(const XL6_AstNode* node,
    XL6_ForeignCallFacts* output)
{
    XL6_ForeignCallFacts facts = {0};int countedField=0,retainedResult=0,voidFactory=0;uint64 callbackSpans=0;
    const XL6_AstNode* attributes = xl6ForeignFunctionAttributes(node);
    for (size_t i = 0; attributes && i < attributes->childCount; ++i) {
        const XL6_AstNode* attr = attributes->children[i];
        int32 index;
        if(attr && attr->text && !strcmp(attr->text,"c_value")){
            XL6_ForeignValueContract row;
            if(!xl6ForeignValueRead(attr,&row))return 0;
            continue;
        }
        if(attr && attr->text && !strcmp(attr->text,"c_callback_span")){
            XL6_ForeignCallbackSpanContract row;
            if(!xl6ForeignCallbackSpanRead(attr,&row))return 0;
            if(facts.callbackSpanMappings==64)return 0;
            callbackSpans|=UINT64_C(1)<<row.parameter;++facts.callbackSpanMappings;continue;
        }
        if(attr && attr->text && !strcmp(attr->text,"c_retain")){
            if(retainedResult || !xl6ForeignRetainRead(attr,NULL))return 0;
            retainedResult=1;continue;
        }
        if(attr && attr->text && !strcmp(attr->text,"c_inplace")){
            if(facts.inplaceOutput || !xl6ForeignStorageRead(attr,NULL))return 0;
            facts.inplaceOutput=XL6_FOREIGN_INPLACE_STATUS;continue;
        }
        if(attr && attr->text && !strcmp(attr->text,"c_deref")){
            if(attr->childCount || facts.receiverDereference)return 0;
            facts.receiverDereference=1;continue;
        }
        if(attr && attr->text && !strcmp(attr->text,"c_length")){
            if(countedField || !xl6ForeignFieldLengthRead(attr,NULL))return 0;
            countedField=1;continue;
        }
        if (attr && attr->text && !strcmp(attr->text, "c_result")) {
            const XL6_AstNode* value = attr->childCount ? attr->children[0] : NULL;
            if (facts.borrowedViewResult || !value || value->kind != XL6_AST_STRING || !value->text) return 0;
            if (!strcmp(value->text, "\"text_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_TEXT;
            else if (!strcmp(value->text, "\"bytes_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_BYTES;
            else if (!strcmp(value->text, "\"owned_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE;
            else if (!strcmp(value->text, "\"retained_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE;
            else if (!strcmp(value->text, "\"owner_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE;
            else if (!strcmp(value->text, "\"bool_status\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_BOOL_STATUS;
            else if (!strcmp(value->text, "\"scope_view\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_SCOPE_NATIVE;
            else if (!strcmp(value->text, "\"status\"")) facts.borrowedViewResult = XL6_FOREIGN_VIEW_RESULT_INT_STATUS;
            else return 0;
            if(facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNER_NATIVE || facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_SCOPE_NATIVE){
                int32 source;
                if(attr->childCount!=2 || !xl6ForeignDecimalInt(attr->children[1],&source) || source<0 || source>=64)return 0;
                facts.resultOwnerIndexPlusOne=(uint32)source+1;
            }else if(facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INT_STATUS){
                if(attr->childCount!=2 || !xl6ForeignDecimalInt(attr->children[1],&facts.successCode))return 0;
            }else if(attr->childCount!=1)return 0;
            continue;
        }
        if (!attr || !attr->text || !attr->childCount ||
            !xl6ForeignDecimalInt(attr->children[0], &index) || index < 0 || index >= 64) return 0;
        if (!strcmp(attr->text,"c_span") || !strcmp(attr->text,"c_owned_callback")) {
            XL6_ForeignParameterContract contract;
            uint64 bit=UINT64_C(1)<<index;
            if(!xl6ForeignParameterRead(attr,&contract) || ((facts.spanParameters|facts.retainedCallbacks)&bit))return 0;
            if(contract.kind==XL6_FOREIGN_PARAMETER_SPAN)facts.spanParameters|=bit;
            else facts.retainedCallbacks|=bit;
        } else if (!strcmp(attr->text, "c_text_view")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.textViews & bit)) return 0;
            facts.textViews |= bit;
        } else if (!strcmp(attr->text, "c_bytes_view")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.bytesViews & bit)) return 0;
            facts.bytesViews |= bit;
        } else if (!strcmp(attr->text,"c_cstr")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.terminatedStrings & bit)) return 0;
            facts.terminatedStrings |= bit;
        } else if(!strcmp(attr->text,"c_control")){
            uint64 bit=UINT64_C(1)<<index;
            if(attr->childCount!=1 || (facts.controlViews&bit))return 0;
            facts.controlViews|=bit;
        } else if (!strcmp(attr->text, "c_ref")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.constRecordAddresses & bit)) return 0;
            facts.constRecordAddresses |= bit;
        } else if (!strcmp(attr->text, "c_static_callback")) {
            uint64 bit = UINT64_C(1) << index;
            if (attr->childCount != 1 || (facts.staticCallbackAddresses & bit)) return 0;
            facts.staticCallbackAddresses |= bit;
        } else if (!strcmp(attr->text,"c_output")) {
            if(facts.outputIndexPlusOne || attr->childCount!=2 ||
                !xl6ForeignDecimalInt(attr->children[1],&facts.successCode))return 0;
            facts.outputIndexPlusOne=(uint32)index+1;facts.valueOutput=1;
        } else if (!strcmp(attr->text, "factory")) {
            if(facts.outputIndexPlusOne || (attr->childCount!=2 && attr->childCount!=3))return 0;
            voidFactory=attr->childCount==2;
            const XL6_AstNode* rollback=attr->children[attr->childCount-1];
            if((!voidFactory && !xl6ForeignDecimalInt(attr->children[1],&facts.successCode)) ||
                !rollback || rollback->kind!=XL6_AST_NAME || !rollback->text)return 0;
            facts.outputIndexPlusOne=(uint32)index+1;
            facts.rollbackSymbol=rollback->semanticSymbolId;
        } else return 0;
    }
    if(voidFactory){
        if(facts.inplaceOutput!=XL6_FOREIGN_INPLACE_STATUS)return 0;
        facts.inplaceOutput=XL6_FOREIGN_INPLACE_VOID;
    }
    if(callbackSpans&~facts.retainedCallbacks)return 0;
    if(retainedResult!=(facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE) ||
        (retainedResult && (facts.outputIndexPlusOne || facts.inplaceOutput || facts.receiverDereference || facts.valueOutput)))return 0;
    if(facts.receiverDereference && ((node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || facts.valueOutput || facts.outputIndexPlusOne))return 0;
    if(facts.inplaceOutput && (!facts.outputIndexPlusOne || facts.valueOutput ||
        facts.borrowedViewResult!=XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE ||
        (node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || facts.receiverDereference))return 0;
    if(countedField){
        if(!(node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || node->kind!=XL6_AST_GET || facts.outputIndexPlusOne)return 0;
        if(facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_TEXT)facts.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_TEXT_FIELD;
        else if(facts.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_BYTES)facts.borrowedViewResult=XL6_FOREIGN_VIEW_RESULT_BYTES_FIELD;
        else return 0;
    }
    if (((facts.spanParameters|facts.retainedCallbacks) & (facts.terminatedStrings|facts.textViews|facts.bytesViews|facts.constRecordAddresses|facts.staticCallbackAddresses)) ||
        (facts.terminatedStrings & (facts.textViews | facts.bytesViews | facts.constRecordAddresses | facts.staticCallbackAddresses)) ||
        (facts.textViews & (facts.constRecordAddresses | facts.staticCallbackAddresses | facts.bytesViews)) ||
        (facts.bytesViews & (facts.constRecordAddresses | facts.staticCallbackAddresses)) ||
        (facts.borrowedViewResult && facts.borrowedViewResult<=XL6_FOREIGN_VIEW_RESULT_BYTES && facts.outputIndexPlusOne) ||
        (facts.valueOutput && facts.borrowedViewResult)) return 0;
    if (output) *output = facts;
    return 1;
}

static inline int xl6ForeignFunctionDereferences(const XL6_AstNode* node)
{XL6_ForeignCallFacts facts={0};return xl6ForeignFunctionRead(node,&facts) && facts.receiverDereference;}

/* Parser-owned fixed C array vocabulary. '$' is not a source identifier, so
 * the family is introduced only by a checked field declarator, not by an
 * arbitrary user spelling. Extents remain ordinary exact type identities in
 * the frontend; XLIR stores the decoded extent as a closed numeric fact. */
static inline int xl6ForeignArrayExtent(const char* name, uint64* extent)
{
    static const char prefix[] = "c_array$";
    uint64 value = 0;
    if (!name || strncmp(name, prefix, sizeof(prefix)-1)) return 0;
    const unsigned char* p = (const unsigned char*)name + sizeof(prefix)-1;
    if (!*p || *p == '0') return 0;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9' || value > (UINT32_MAX - (*p - '0')) / 10u) return 0;
        value = value * 10u + (*p - '0');
    }
    if (!value) return 0;
    if (extent) *extent = value;
    return 1;
}

static inline int xl6ForeignIdentifier(const char* text)
{
    const unsigned char* p = (const unsigned char*)text;
    if (!p || !((*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') || *p == '_')) return 0;
    while (*++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_')) return 0;
    return 1;
}

/* A typedef naming a pointer, or a bounded ordinary C pointer declarator.
 * No parentheses, arrays, preprocessor tokens or arbitrary C expressions.
 * Pointee qualifications remain part of the exact physical call signature. */
static inline int xl6ForeignPointerSpelling(const char* text)
{
    char identifier[64];
    const char* p = text;
    size_t size = 0, pointers = 0;
    if (xl6ForeignIdentifier(text)) return 1;
    if (!p) return 0;
    if (!strncmp(p, "const ", 6)) p += 6;
    if (!strncmp(p, "struct ", 7)) p += 7;
    else if (!strncmp(p, "union ", 6)) p += 6;
    while (*p && *p != ' ' && *p != '*') {
        if (size + 1 >= sizeof(identifier)) return 0;
        identifier[size++] = *p++;
    }
    identifier[size] = 0;
    if (!xl6ForeignIdentifier(identifier)) return 0;
    while (*p == ' ') ++p;
    while (*p == '*') {
        if (++pointers > 8) return 0;
        ++p;
        while (*p == ' ') ++p;
    }
    return pointers && !*p;
}

static inline int xl6ForeignQuotedText(const XL6_AstNode* node,
    char* output, size_t capacity)
{
    size_t size;
    const char* text = node ? node->text : NULL;
    if (!text || !output || !capacity) return 0;
    size = strlen(text);
    if (size < 2 || text[0] != '"' || text[size-1] != '"' ||
        size - 2 >= capacity) return 0;
    /* Header/type metadata is deliberately a plain portable spelling, not
     * executable C or an escaped language string. */
    for (size_t i = 1; i + 1 < size; ++i)
        if ((unsigned char)text[i] < 32 || text[i] == '\\' || text[i] == '"') return 0;
    memcpy(output, text + 1, size - 2);
    output[size - 2] = 0;
    return 1;
}

static inline int xl6ForeignHeaderName(const char* text)
{
    if (!text || !*text || *text == '/' || strstr(text, "..")) return 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' ||
              *p == '.' || *p == '/')) return 0;
    return 1;
}

/* 0: ordinary declaration; 1: valid external view; -1: malformed contract.
 * Keep parsing/semantic diagnostics and the AST->IR handoff on one schema. */
static inline int xl6ForeignClassRead(const XL6_AstNode* node,
    XL6_ForeignClassDecl* output, const char** error)
{
    XL6_ForeignClassDecl result = {0};
    const XL6_AstNode* declaration;
    unsigned seen = 0;
    if (!node || node->kind != XL6_AST_CLASS ||
        !(node->flags & XL6_AST_FLAG_EXTERN)) return 0;
    if (error) *error = "invalid external class declaration contract";
    if (node->childCount != 4 || !(declaration = node->children[3]) ||
        declaration->kind != XL6_AST_FIELD_ATTR || !declaration->text ||
        strcmp(declaration->text, "c_type") || declaration->childCount < 1 ||
        !xl6ForeignQuotedText(declaration->children[0], result.cName, sizeof(result.cName)) ||
        !xl6ForeignPointerSpelling(result.cName)) return -1;
    for (size_t i = 1; i < declaration->childCount; ++i) {
        const XL6_AstNode* attr = declaration->children[i];
        char value[256];
        if(attr && attr->kind==XL6_AST_FIELD_ATTR && attr->text && !strcmp(attr->text,"owner")){
            const XL6_AstNode* name=attr->childCount==1 ? attr->children[0] : NULL;
            if((seen&16) || !name || name->kind!=XL6_AST_NAME || !name->text ||
                strlen(name->text)>=sizeof(result.owner))return -1;
            strcpy(result.owner,name->text);result.ownerSymbol=name->semanticSymbolId;seen|=16;continue;
        }
        if (!attr || attr->kind != XL6_AST_FIELD_ATTR || !attr->text ||
            attr->childCount != 1 ||
            !xl6ForeignQuotedText(attr->children[0], value, sizeof(value))) return -1;
        if (!strcmp(attr->text, "header")) {
            if ((seen & 1) || !xl6ForeignHeaderName(value)) return -1;
            memcpy(result.header, value, strlen(value) + 1); seen |= 1;
        } else if (!strcmp(attr->text, "lifetime")) {
            if (seen & 2) return -1;
            if(!strcmp(value,"caller"))result.lifetime=XL6_FOREIGN_LIFETIME_CALLER;
            else if(!strcmp(value,"owned"))result.lifetime=XL6_FOREIGN_LIFETIME_OWNED;
            else if(!strcmp(value,"owner"))result.lifetime=XL6_FOREIGN_LIFETIME_OWNER;
            else if(!strcmp(value,"callback"))result.lifetime=XL6_FOREIGN_LIFETIME_CALLBACK;
            else return -1;
            seen |= 2;
        } else if(!strcmp(attr->text,"policy")){
            if((seen&4) || !xl6ForeignIdentifier(value) || strlen(value)>=sizeof(result.policy))return -1;
            strcpy(result.policy,value);seen|=4;
        } else if(!strcmp(attr->text,"scope")){
            if((seen&8) || !xl6ForeignIdentifier(value) || strlen(value)>=sizeof(result.scope))return -1;
            strcpy(result.scope,value);seen|=8;
        } else if(!strcmp(attr->text,"watch")){
            if((seen&32) || !xl6ForeignIdentifier(value) || strlen(value)>=sizeof(result.watch))return -1;
            strcpy(result.watch,value);seen|=32;
        } else return -1;
    }
    unsigned expected=result.lifetime==XL6_FOREIGN_LIFETIME_CALLER ? 3 :
        result.lifetime==XL6_FOREIGN_LIFETIME_OWNED ? ((seen&8)?15:7) :
        result.lifetime==XL6_FOREIGN_LIFETIME_CALLBACK ? 19 : 23;
    if((result.lifetime==XL6_FOREIGN_LIFETIME_OWNED || result.lifetime==XL6_FOREIGN_LIFETIME_OWNER) && (seen&32))expected|=32;
    if (seen != expected) {
        if (error) *error = "external class requires an exact header/lifetime contract; owned views also need policy/scope, owner views need policy/owner";
        return -1;
    }
    if (output) *output = result;
    return 1;
}

/* C records are trivial values. Their field layout belongs to the named C
 * declaration, and is checked against the ordered language field schema. */
static inline int xl6ForeignRecordRead(const XL6_AstNode* node,
    XL6_ForeignClassDecl* output, const char** error)
{
    XL6_ForeignClassDecl result = {0};
    const XL6_AstNode* declaration;
    if (!node || node->kind != XL6_AST_C_RECORD) return 0;
    if (error) *error = "C record requires a C type identifier and one @header(\"...\")";
    if (!(node->flags & XL6_AST_FLAG_EXTERN) || node->childCount != 2 ||
        !(declaration = node->children[1]) ||
        declaration->kind != XL6_AST_FIELD_ATTR || !declaration->text ||
        strcmp(declaration->text, "c_type") || declaration->childCount != 2 ||
        !xl6ForeignQuotedText(declaration->children[0], result.cName, sizeof(result.cName)) ||
        !xl6ForeignIdentifier(result.cName)) return -1;
    const XL6_AstNode* header = declaration->children[1];
    if (!header || header->kind != XL6_AST_FIELD_ATTR || !header->text ||
        strcmp(header->text, "header") || header->childCount != 1 ||
        !xl6ForeignQuotedText(header->children[0], result.header, sizeof(result.header)) ||
        !xl6ForeignHeaderName(result.header)) return -1;
    if (output) *output = result;
    return 1;
}

#endif
