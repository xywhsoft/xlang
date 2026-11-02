#include "analysis/frontend_internal.h"
#include "xl6_sem.c"
#include "xl6_sem_index.c"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
/* Constructed producer states test the private receipt boundary,not language
 * semantics. Real checked units and complete original AST/index/IR comparisons
 * are exercised separately by analysis_index_canonical_test. */
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"analysis_index_canonical_guards_test:%d:%s\n",__LINE__,#x);return 0;}}while(0)
static void setup(xls_sem_memo* memo,XL6_AstNode* root,xls_sem_unit* unit,
    xls_sem_ir_receipt* receipt,xls_sem_final_context* context)
{
    *memo=(xls_sem_memo){.trackInputs=1,.finalizedRoot=root,.finalContext=context};
    xrtArrayInit(&memo->entries,sizeof(xls_sem_entry));xrtArrayResize(&memo->entries,1);
    *(xls_sem_entry*)xrtArrayGet(&memo->entries,0)=(xls_sem_entry){.unit=unit,.finalInput=receipt,.finalCandidate=1,.finalReplayed=1,.start=7,.end=11,.typeStart=3,.typeEnd=3};
}
static int run(void)
{
    const char* text="func sample(int value) as int\n return value\nend\n";
    char* path=xrtPathAbs("canonical-guard-origin.xl");CHECK(path);
    XL6_ParseResult first,current;xl6ParseResultInit(&first);xl6ParseResultInit(&current);
    CHECK(xl6ParseSource(text,path,&first) && xl6ParseSource(text,path,&current));
    XL6_SourceReport report;xl6SourceReportInit(&report);CHECK(xl6SourceReportText(&report,path,text,strlen(text),"overlay"));
    char error[256];xls_semantic_index* plain=xlsFrontendIndexCreate(first.root,&report,error,sizeof(error));CHECK(plain);
    size_t start=0,count=0;CHECK(xlsFrontendIndexFunctionSpan(plain,first.root->children[0],&start,&count) && count>0);
    xls_sem_unit unit={0},other={0};CHECK(xrtArrayInit(&unit.facts,sizeof(xls_sem_fact)) && xrtArrayResize(&unit.facts,count));
    xls_sem_ir_receipt receipt={.nodes=count},otherReceipt={.nodes=count};xls_sem_final_context context={0},different={0};
    xls_sem_memo before,after;setup(&before,first.root,&unit,&receipt,&context);setup(&after,current.root,&unit,&receipt,&context);
    size_t actual=0;CHECK(xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual) && actual==count);
    before.finalContext=&different;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));before.finalContext=&context;
    xls_sem_entry* old=xrtArrayGet(&before.entries,0);old->unit=&other;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));old->unit=&unit;
    old->finalInput=&otherReceipt;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));old->finalInput=&receipt;
    ++old->start;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));--old->start;
    after.trackInputs=0;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));after.trackInputs=1;
    CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,first.root,&before,first.root,0,&actual));
    before.finalizedRoot=current.root;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));before.finalizedRoot=first.root;
    xls_sem_entry* entry=xrtArrayGet(&after.entries,0);entry->finalReplayed=0;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));entry->finalReplayed=1;
    unit.cursor.path="cursor";CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));unit.cursor.path=NULL;
    --receipt.nodes;CHECK(!xlsFrontendSemMemoIndexEquivalent(&after,current.root,&before,first.root,0,&actual));++receipt.nodes;
    xls_semantic_index* original=xlsFrontendIndexCreateCached(first.root,&report,&before,NULL,NULL,error,sizeof(error));CHECK(original);
    xls_index_canonical_work work=xlsFrontendIndexCanonicalWork(original);CHECK(work.captured==1 && work.retainedBytes>0);
    size_t known=work.retainedBytes;
    for(unsigned mode=0;mode<3;++mode){
        size_t bytes=mode==0 ? 0 : mode==1 ? known-1 : known;xls_index_canonical_control outer,inner;
        xlsFrontendIndexCanonicalBegin(&outer,bytes);xlsFrontendIndexCanonicalBegin(&inner,SIZE_MAX);CHECK(xlsIndexCanonicalBudget()==bytes);
        xls_semantic_index* bounded=xlsFrontendIndexCreateCached(current.root,&report,&after,original,&before,error,sizeof(error));CHECK(bounded);
        work=xlsFrontendIndexCanonicalWork(bounded);CHECK(mode==2 ? work.reused==1 && work.retainedBytes==known : !work.reused && !work.captured && !work.retainedBytes);
        CHECK(xlsSemanticIndexCount(bounded)==xlsSemanticIndexCount(plain));
        xlsFrontendIndexDestroy(bounded);xlsFrontendIndexCanonicalEnd(&inner);xlsFrontendIndexCanonicalEnd(&outer);
    }
    xls_semantic_index* reused=xlsFrontendIndexCreateCached(current.root,&report,&after,original,&before,error,sizeof(error));CHECK(reused && xlsFrontendIndexCanonicalWork(reused).reused==1);
    xls_index_canonical* arena=original->build.storage->chunks[1].canonical;CHECK(arena && reused->build.storage->chunks[1].canonical==arena);
    xlsFrontendIndexDestroy(reused);
    xls_index_canonical_row* row=xrtArrayGet(&arena->rows,0);XL6_AstKind saved=row->kind;row->kind=XL6_AST_CLASS;
    reused=xlsFrontendIndexCreateCached(current.root,&report,&after,original,&before,error,sizeof(error));CHECK(reused);
    work=xlsFrontendIndexCanonicalWork(reused);CHECK(work.fallbacks==1 && !work.retainedBytes && work.calls>0 && !*error);
    CHECK(xlsSemanticIndexCount(reused)==xlsSemanticIndexCount(plain));row->kind=saved;
    for(size_t i=0;i<xlsSemanticIndexCount(reused);++i){xls_index_fact a,b;CHECK(xlsSemanticIndexFact(reused,i,&a) && xlsSemanticIndexFact(plain,i,&b) &&
        !!a.canonical==!!b.canonical && (!a.canonical || !strcmp(a.canonical,b.canonical)));}
    xlsFrontendIndexDestroy(reused);CHECK(!xlsIndexCanonicalScope && !xlsTypedIndexBuild);
    xlsFrontendIndexDestroy(original);xlsFrontendIndexDestroy(plain);xrtArrayUnit(&unit.facts);xrtArrayUnit(&before.entries);xrtArrayUnit(&after.entries);
    xl6ParseResultFree(&first);xl6ParseResultFree(&current);xl6SourceReportFree(&report);xrtFree(path);return 1;
}
int main(void){if(!run())return 1;printf("analysis_index_canonical_guards_test passed:%u checks;final context/unit/output/root/cursor/ID rejection,bounded independent strings,real cache corruption fallback\n",checks);return 0;}
#else
int main(void){puts("analysis_index_canonical_guards_test skipped:old native representation");return 0;}
#endif
