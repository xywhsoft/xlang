#include "analysis/ast_clone.h"
#include <xrt.h>
#include <string.h>

typedef struct clone_tree {XL6_AstNode* node;size_t begin,count;uint64_t nodes;} clone_tree;
typedef struct clone_context {
    xls_ast_clone_options options;
    xmap sources;
    size_t bytes;
    xls_ast_cursor_source_policy cursorPolicy;
    void* policyUser;
    xls_ast_storage* storage;
    int logicalBudget;
    size_t poolStart,poolAllowance;
    xmap leaves;
    int leafSharing;
    xls_ast_leaf_work leafWork;
    xmap trees;int treeSharing;
    xls_ast_tree_work treeWork;
    const xls_ast_tree_projection* projection;
} clone_context;
static int bareType(const XL6_AstNode* n)
{
    if(!n || n->kind!=XL6_AST_TYPE || n->doc || n->literalData || n->inferredType || n->semanticDeclaredType || n->childCount ||
       (n->source && n->source->completionQuery))return 0;
    uint64_t fields=n->flags | n->literalSize | n->literalScalar | n->literalDecoded |
        n->semanticSymbolId | n->semanticDeclaredSymbolId |
        n->semanticTargetSymbolId | n->semanticCompoundOperatorSymbolId | n->semanticCallKind | n->semanticLvalueKind |
        n->semanticOwnership | n->semanticExitKind | n->semanticScopeDepth | n->semanticTargetScopeDepth | n->semanticRegionId | n->semanticTargetRegionId |
        n->line | n->column | n->range.known | n->range.startOffset | n->range.endOffset | n->range.startLine | n->range.startColumn | n->range.endLine | n->range.endColumn |
        n->tokenRange.known | n->tokenRange.startOffset | n->tokenRange.endOffset | n->tokenRange.startLine | n->tokenRange.startColumn | n->tokenRange.endLine | n->tokenRange.endColumn;
    return fields==0;
}
static int noOwnedFields(const XL6_AstNode* n)
{return !n->text && !n->doc && !n->literalData && !n->source && !n->children && !n->inferredType && !n->semanticDeclaredType;}
static size_t leafKey(const XL6_AstNode* in,unsigned char* key)
{
    size_t size=in->text ? strlen(in->text) : 0;if(size>256)return 0;
    /* Pointer identity only selects a source already immutable for this call.
     * It is stricter than source equality,never a persisted semantic identity.
     * The full label bytes and NULL/empty distinction remain exact map keys. */
    size_t at=0;
    memcpy(key+at,&in->source,sizeof(in->source));at+=sizeof(in->source);
    memcpy(key+at,&in->semanticTypeId,sizeof(in->semanticTypeId));at+=sizeof(in->semanticTypeId);
    memcpy(key+at,&in->semanticTypeAbiId,sizeof(in->semanticTypeAbiId));at+=sizeof(in->semanticTypeAbiId);
    key[at++]=(unsigned char)(in->text!=NULL);
    if(size)memcpy(key+at,in->text,size);
    return at+size;
}
static int charge(clone_context* c,size_t bytes)
{
    if(bytes>SIZE_MAX-c->bytes || (c->options.bytes && bytes>c->options.bytes-c->bytes)) return 0;
    c->bytes+=bytes; return 1;
}
static char* duplicate(clone_context* c,const char* text)
{ return text && charge(c,strlen(text)+1) ? xrtStrDup(text) : NULL; }
static int source(clone_context* c,const XL6_AstSource* in,XL6_AstNode* out)
{
    if(!in || c->options.dropSources) return 1;
    xbytesview key={(const unsigned char*)&in,sizeof(in)};
    XL6_AstSource** found=xrtMapGet(&c->sources,key); XL6_AstSource* value=found ? *found : NULL;
    if(value) { out->source=value; ++value->references; return 1; }
    if(c->options.targetSource) {
        XL6_AstSource* target=c->options.targetSource(in,c->options.user);
        if(target) {
            if(!xlsAstCloneSourceTargetValid(in,target,c->cursorPolicy,c->policyUser))return 0;
            if(!xrtMapSet(&c->sources,key,&target)) return 0;
            out->source=target; ++target->references; return 1;
        }
    }
    if(!value) {
        if(!charge(c,sizeof(*value))) return 0;
        value=xrtCalloc(1,sizeof(*value)); if(!value) return 0;
        *value=*in; value->references=0; value->path=duplicate(c,in->path);
        if((in->path && !value->path) || !xrtMapSet(&c->sources,key,&value)) { xrtFree(value->path); xrtFree(value); return 0; }
    }
    out->source=value; ++value->references; return 1;
}

static int sameTypeScalars(const XL6_AstNode* a,const XL6_AstNode* b)
{
#define SAME(field) if(a->field!=b->field)return 0
    SAME(kind);SAME(flags);SAME(literalSize);SAME(literalScalar);SAME(literalDecoded);
    SAME(semanticTypeId);SAME(semanticTypeAbiId);SAME(semanticSymbolId);SAME(semanticDeclaredSymbolId);
    SAME(semanticTargetSymbolId);SAME(semanticCompoundOperatorSymbolId);SAME(semanticCallKind);SAME(semanticLvalueKind);
    SAME(semanticOwnership);SAME(semanticExitKind);SAME(semanticScopeDepth);SAME(semanticTargetScopeDepth);
    SAME(semanticRegionId);SAME(semanticTargetRegionId);SAME(line);SAME(column);
    SAME(range.known);SAME(range.startOffset);SAME(range.endOffset);SAME(range.startLine);SAME(range.startColumn);SAME(range.endLine);SAME(range.endColumn);
    SAME(tokenRange.known);SAME(tokenRange.startOffset);SAME(tokenRange.endOffset);SAME(tokenRange.startLine);SAME(tokenRange.startColumn);SAME(tokenRange.endLine);SAME(tokenRange.endColumn);
#undef SAME
    return 1;
}
static int replayType(clone_context* c,const XL6_AstNode* in,const XL6_AstNode* owned,size_t depth)
{
    if(!in || !owned || depth>512 || in->childCount!=owned->childCount || (in->source && in->source->completionQuery))return 0;
    XL6_AstNode prepared=*in;
    prepared.text=prepared.doc=NULL;prepared.literalData=NULL;prepared.source=NULL;
    prepared.children=NULL;prepared.childCount=prepared.childCapacity=0;prepared.inferredType=prepared.semanticDeclaredType=NULL;
    prepared.xlsStorage=c->storage;prepared.xlsLeafReferences=0;
    ++c->treeWork.validatedNodes;
    if((c->options.transform && !c->options.transform(in,&prepared,c->options.user)) || !noOwnedFields(&prepared) ||
       prepared.childCount || prepared.childCapacity || !sameTypeScalars(&prepared,owned))return 0;
    /* A repeated input source uses the very same clone-local memoized target
     * in the original copier. Never call targetSource/policy a second time. */
    XL6_AstSource* expected=NULL;
    if(in->source && !c->options.dropSources){
        const XL6_AstSource* source=in->source;XL6_AstSource** found=xrtMapGet(&c->sources,(xbytesview){(const unsigned char*)&source,sizeof(source)});
        if(!found)return 0;
        expected=*found;
    }
    if(owned->source!=expected || !!in->inferredType!=!!owned->inferredType || !!in->semanticDeclaredType!=!!owned->semanticDeclaredType)return 0;
    for(size_t i=0;i<in->childCount;++i)if(!replayType(c,in->children[i],owned->children[i],depth+1))return 0;
    if(in->inferredType && !replayType(c,in->inferredType,owned->inferredType,depth+1))return 0;
    if(in->semanticDeclaredType && !replayType(c,in->semanticDeclaredType,owned->semanticDeclaredType,depth+1))return 0;
    return 1;
}

static XL6_AstNode* clone(clone_context* c,const XL6_AstNode* in,size_t depth)
{
    if(!in || depth>512) return NULL;
    int treeCandidate=c->treeSharing && depth==1 && in->kind==XL6_AST_TYPE;
    xbytesview treeKey={(const unsigned char*)&in,sizeof(in)};
    clone_tree* tree=treeCandidate ? xrtMapGet(&c->trees,treeKey) : NULL;
    if(tree && tree->node && tree->node->xlsLeafReferences<UINT32_MAX){
        if(c->projection && tree->begin!=SIZE_MAX){
            if(!c->projection->replay(c->options.user,tree->begin,tree->count))return NULL;
            c->treeWork.certifiedNodes+=tree->nodes;
        }else if(!replayType(c,in,tree->node,depth))return NULL;
        ++tree->node->xlsLeafReferences;++c->treeWork.hits;return tree->node;
    }
    size_t projectionBegin=treeCandidate && c->projection ? c->projection->mark(c->options.user) : SIZE_MAX;
    uint64_t nodesBefore=c->treeWork.allocatedNodes;
    /* Only direct fact roots are context-independent. A nested class-handle
     * target can receive its parent's identity in the original finalizer,
     * even when that target initially looks like an unannotated bare leaf. */
    unsigned char key[sizeof(in->source)+sizeof(in->semanticTypeId)+sizeof(in->semanticTypeAbiId)+1+256];size_t keySize=c->leafSharing && depth==1 && bareType(in) ? leafKey(in,key) : 0;
    XL6_AstNode prepared;int transformedBefore=0;
    XL6_AstNode** shared=keySize ? xrtMapGet(&c->leaves,(xbytesview){key,keySize}) : NULL;
    if(shared && *shared && (*shared)->xlsLeafReferences<UINT32_MAX){
        prepared=*in;prepared.xlsStorage=c->storage;prepared.xlsLeafReferences=0;prepared.text=prepared.doc=NULL;
        prepared.literalData=NULL;prepared.source=NULL;prepared.children=NULL;prepared.childCount=prepared.childCapacity=0;
        prepared.inferredType=prepared.semanticDeclaredType=NULL;transformedBefore=1;
        if(c->options.transform && !c->options.transform(in,&prepared,c->options.user))return NULL;
        if(bareType(&prepared) && noOwnedFields(&prepared) && prepared.semanticTypeId==in->semanticTypeId && prepared.semanticTypeAbiId==in->semanticTypeAbiId) {++(*shared)->xlsLeafReferences;++c->leafWork.hits;return *shared;}
        keySize=0;
    }
    XL6_AstNode* out;
    if(c->storage && c->logicalBudget){
        if(!charge(c,sizeof(*in)))return NULL;
        size_t used=xlsAstStorageWork(c->storage).bytes-c->poolStart;
        if(used>c->poolAllowance)return NULL;
        out=xlsAstStorageNode(c->storage,c->poolAllowance-used);if(!out)return NULL;
    }else if(c->storage){
        size_t before=xlsAstStorageWork(c->storage).bytes;
        out=xlsAstStorageNode(c->storage,c->options.bytes ? c->options.bytes-c->bytes : SIZE_MAX);
        if(!out)return NULL;
        if(!charge(c,xlsAstStorageWork(c->storage).bytes-before)){xlsAstStorageFreeNode(out);return NULL;}
    }else{
        if(!charge(c,sizeof(*in)))return NULL;
        out=xrtCalloc(1,sizeof(*out));if(!out)return NULL;
    }
    ++c->treeWork.allocatedNodes;
    if(transformedBefore)*out=prepared;
    else{
        *out=*in;out->text=out->doc=NULL;out->literalData=NULL;out->source=NULL;
        out->children=NULL;out->childCount=out->childCapacity=0;out->inferredType=out->semanticDeclaredType=NULL;
    }
    out->xlsStorage=c->storage;out->xlsLeafReferences=0;
    int transformed=transformedBefore || !c->options.transform || c->options.transform(in,out,c->options.user);
    out->xlsStorage=c->storage;out->xlsLeafReferences=0; /* callbacks never transfer private ownership */
    if(!transformed) goto failed;
    if(keySize && !noOwnedFields(out))keySize=0;
    if(in->text && !(out->text=duplicate(c,in->text))) goto failed;
    if(in->doc && !(out->doc=duplicate(c,in->doc))) goto failed;
    if(!source(c,in->source,out)) goto failed;
    if(in->literalData) {
        if(in->literalSize==SIZE_MAX || !charge(c,in->literalSize+1) || !xl6AstSetLiteralData(out,in->literalData,in->literalSize)) goto failed;
        out->literalDecoded=in->literalDecoded;
    }
    if(in->childCount) {
        if(in->childCount>SIZE_MAX/sizeof(*out->children) || !charge(c,in->childCount*sizeof(*out->children))) goto failed;
        out->children=xrtCalloc(in->childCount,sizeof(*out->children)); if(!out->children) goto failed;
        out->childCapacity=in->childCount;
        for(size_t i=0;i<in->childCount;++i) {
            XL6_AstNode* child=clone(c,in->children[i],depth+1); if(!child) goto failed;
            out->children[out->childCount++]=child;
        }
    }
    if(in->inferredType && !(out->inferredType=clone(c,in->inferredType,depth+1))) goto failed;
    if(in->semanticDeclaredType && !(out->semanticDeclaredType=clone(c,in->semanticDeclaredType,depth+1))) goto failed;
    if(keySize && bareType(out) && out->semanticTypeId==in->semanticTypeId && out->semanticTypeAbiId==in->semanticTypeAbiId && c->leafWork.entries<64 && !shared){
        if(xrtMapSet(&c->leaves,(xbytesview){key,keySize},&out)){
            out->xlsLeafReferences=1;++c->leafWork.entries;c->leafWork.keyBytes+=keySize+sizeof(out);
        }else{
            /* A failed optional insertion must not leave a possibly partial
             * entry pointing at an independently owned node. Earlier shared
             * leaves keep their edge references;future nodes clone normally. */
            c->leafSharing=0;xrtMapUnit(&c->leaves);c->leaves=(xmap){0};
        }
    }
    if(treeCandidate && !tree && c->treeWork.entries<256){
        size_t projectionEnd=c->projection ? c->projection->mark(c->options.user) : SIZE_MAX;
        clone_tree entry={out,projectionBegin,0,c->treeWork.allocatedNodes-nodesBefore};
        if(projectionEnd==SIZE_MAX || projectionBegin>projectionEnd)entry.begin=SIZE_MAX;
        else entry.count=projectionEnd-projectionBegin;
        if(xrtMapSet(&c->trees,treeKey,&entry)){
            if(!out->xlsLeafReferences)out->xlsLeafReferences=1;
            ++c->treeWork.entries;
        }else{c->treeSharing=0;xrtMapUnit(&c->trees);c->trees=(xmap){0};}
    }
    return out;
failed: xl6AstFree(out); return NULL;
}
static XL6_AstNode* cloneForest(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,xls_ast_leaf_work* leaves,xls_ast_tree_work* trees,const xls_ast_tree_projection* projection,size_t* bytes)
{
    if(bytes) *bytes=0;
    if(work)*work=(xls_ast_pool_work){0};
    if(leaves)*leaves=(xls_ast_leaf_work){0};
    if(trees)*trees=(xls_ast_tree_work){0};
    clone_context context={.cursorPolicy=policy,.policyUser=user,.projection=projection}; if(options) context.options=*options;
    if(!input || !xrtMapInit(&context.sources,sizeof(XL6_AstSource*))) return NULL;
    if(leaves && xrtMapInit(&context.leaves,sizeof(XL6_AstNode*)))context.leafSharing=1;
    if(trees && xrtMapInit(&context.trees,sizeof(clone_tree)))context.treeSharing=1;
    if(poolBudget){
        context.storage=xlsAstStorageCreate(poolBudget);
        if(!context.storage || !charge(&context,xlsAstStorageWork(context.storage).bytes)){
            xlsAstStorageUnpin(context.storage);xrtMapUnit(&context.sources);xrtMapUnit(&context.leaves);xrtMapUnit(&context.trees);return NULL;
        }
    }
    XL6_AstNode* root=clone(&context,input,0); xrtMapUnit(&context.sources);xrtMapUnit(&context.leaves);xrtMapUnit(&context.trees);
    if(root && bytes) *bytes=context.bytes;
    if(root && work)*work=xlsAstStorageWork(context.storage);
    if(root && leaves)*leaves=context.leafWork;
    if(root && trees)*trees=context.treeWork;
    xlsAstStorageUnpin(context.storage);
    return root;
}
XL6_AstNode* xlsAstClonePooled(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,size_t* bytes)
{return cloneForest(input,options,policy,user,poolBudget,work,NULL,NULL,NULL,bytes);}
XL6_AstNode* xlsAstCloneFactTypes(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,xls_ast_leaf_work* leaves,size_t* bytes)
{xls_ast_leaf_work ignored;return cloneForest(input,options,policy,user,poolBudget,work,leaves ? leaves : &ignored,NULL,NULL,bytes);}
XL6_AstNode* xlsAstCloneFactTrees(const XL6_AstNode* input,const xls_ast_clone_options* options,
    const xls_ast_tree_projection* projection,size_t poolBudget,xls_ast_pool_work* work,xls_ast_tree_work* trees,size_t* bytes)
{xls_ast_leaf_work leaves;xls_ast_tree_work ignored;return cloneForest(input,options,NULL,NULL,poolBudget,work,&leaves,trees ? trees : &ignored,projection,bytes);}
XL6_AstNode* xlsAstCloneWithCursorSource(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t* bytes)
{return xlsAstClonePooled(input,options,policy,user,0,NULL,bytes);}
XL6_AstNode* xlsAstClone(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes)
{return xlsAstCloneWithCursorSource(input,options,NULL,NULL,bytes);}
XL6_AstNode* xlsAstCloneInStorage(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_storage* storage,size_t extraPoolBytes,size_t* bytes)
{
    if(bytes)*bytes=0;
    if(!input || !storage)return NULL;
    clone_context context={.storage=storage,.logicalBudget=1,.poolStart=xlsAstStorageWork(storage).bytes,.poolAllowance=extraPoolBytes};
    if(options)context.options=*options;
    if(!xrtMapInit(&context.sources,sizeof(XL6_AstSource*)))return NULL;
    XL6_AstNode* root=clone(&context,input,0);xrtMapUnit(&context.sources);
    if(root && bytes)*bytes=context.bytes;
    return root;
}
