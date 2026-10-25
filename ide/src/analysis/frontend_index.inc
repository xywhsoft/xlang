/* Included only after the immutable shared sem_index.c by the checked build
 * adapter. Reuses its collector, limits, selection and type-domain rules. */
#include "analysis/frontend_index.h"
#include "analysis/index_modules.h"
#include "analysis/frontend_types.h"
struct xls_semantic_index { XL6_IndexBuild build;xls_module_graph* modules; };

void xlsFrontendIndexDestroy(xls_semantic_index* index)
{
    if(!index) return;
    XL6_IndexBuild* b=&index->build;
    if(b->facts) for(size_t i=0;i<b->count;++i) xrtFree(b->facts[i].canonical);
    xrtFree(b->facts); xrtFree(b->sources); xlsModuleGraphRelease(index->modules);xrtFree(index);
}
void xlsFrontendIndexAttachModules(xls_semantic_index* index,xls_module_graph* graph)
{if(index){xls_module_graph* retained=xlsModuleGraphRetain(graph);xlsModuleGraphRelease(index->modules);index->modules=retained;}}
const xls_module_graph* xlsSemanticIndexModules(const xls_semantic_index* index)
{return index ? index->modules : NULL;}
uint64_t xlsSemanticIndexModuleIdentity(const xls_semantic_index* index,size_t source)
{return xlsModuleGraphSource(index ? index->modules : NULL,source);}
xls_semantic_index* xlsFrontendIndexCreate(const XL6_AstNode* root,
    const XL6_SourceReport* report,char* error,size_t capacity)
{
    if(error && capacity) *error=0;
    const char* failure="cannot allocate semantic index";
    xls_semantic_index* index=xrtCalloc(1,sizeof(*index));
    if(index) {
        XL6_IndexBuild* b=&index->build;
        b->root=root; b->report=report;
        if(!root || !report || !report->complete) failure="semantic index requires a complete checked source report";
        else {
            if(xl6IndexBudget(b,sizeof(*index)+1024)) {
                    xls_type_alias_scope aliases;
                    xlsFrontendTypeAliasBegin(&aliases,root,8u*1024u*1024u);
                    XL6_IndexBuild* previous=xlsTypedIndexBuild; xlsTypedIndexBuild=b;
                    int okay=xl6IndexVisit(b,root,0,1); xlsTypedIndexBuild=previous;
                    xlsFrontendTypeAliasEnd(&aliases);
                    if(okay) return index;
            }
            if(b->error) failure=b->error;
        }
    }
    if(error && capacity) snprintf(error,capacity,"%s",failure);
    xlsFrontendIndexDestroy(index); return NULL;
}
size_t xlsFrontendIndexBudgetUsed(const xls_semantic_index* index)
{ return index ? index->build.budget : 0; }
size_t xlsSemanticIndexCount(const xls_semantic_index* index)
{ return index ? index->build.count : 0; }
static xls_index_range xlsFrontendRange(const XL6_SourceReport* report,
    unsigned source,const XL6_AstRange* range)
{
    xls_index_range out={0};
    /* Same null-range contract as xl6IndexRange in the shared serializer. */
    if(!source || !range || !range->known) return out;
    const XL6_SourceReportItem* file=&report->items[source-1];
    const unsigned char* text=(const unsigned char*)file->text;
    size_t bom=file->size>=3 && text[0]==0xef && text[1]==0xbb && text[2]==0xbf ? 3 : 0;
    text+=bom; size_t size=file->size-bom;
    if(range->endOffset<range->startOffset || range->endOffset>size ||
       (range->startOffset<size && (text[range->startOffset]&0xc0)==0x80) ||
       (range->endOffset<size && (text[range->endOffset]&0xc0)==0x80)) return out;
    return (xls_index_range){range->startOffset,range->endOffset,range->startLine,
        range->startColumn,range->endLine,range->endColumn,1};
}
int xlsSemanticIndexFact(const xls_semantic_index* index,size_t at,xls_index_fact* out)
{
    if(!index || !out || at>=index->build.count) return 0;
    const XL6_IndexBuild* b=&index->build;
    const XL6_IndexFact* f=&b->facts[at]; const XL6_AstNode* n=f->node;
    *out=(xls_index_fact){.kind=xl6AstKindName(n->kind),.canonical=f->canonical,
        .id=(unsigned)at+1,.parent=f->parent,.source=f->source,.selectionSource=f->selectionSource,
        .declaredSymbol=n->semanticDeclaredSymbolId,.symbol=n->semanticSymbolId,
        .targetSymbol=n->semanticTargetSymbolId,.compoundOperatorSymbol=n->semanticCompoundOperatorSymbolId,
        .declaredType=f->declaredType,.referencedType=f->referencedType,
        .typeId=n->semanticTypeId,.typeAbi=n->semanticTypeAbiId,
        .typeObjectId=f->type ? f->type->semanticTypeId : 0,
        .typeObjectAbi=f->type ? f->type->semanticTypeAbiId : 0,
        .typeOrigin=!f->type ? 0 : n->semanticDeclaredType ? 1 : n->inferredType ? 2 : 3,
        .range=xlsFrontendRange(b->report,f->source,&n->range),
        .tokenRange=xlsFrontendRange(b->report,f->source,&n->tokenRange),
        .selectionRange=xlsFrontendRange(b->report,f->selectionSource,f->selection ? &f->selection->tokenRange : NULL)};
    return 1;
}
