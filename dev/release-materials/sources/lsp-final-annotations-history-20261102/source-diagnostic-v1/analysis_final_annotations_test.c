#define main finalAnnotationsReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/frontend_sem.h"

#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static int finalExtras(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b)return a==b;
    if(a->semanticReceiverSymbolId!=b->semanticReceiverSymbolId || a->compiledPackageError!=b->compiledPackageError ||
       a->compiledPackageRoot!=b->compiledPackageRoot || !sameText(a->compiledPackageLink,b->compiledPackageLink))return 0;
    if(!finalExtras(a->inferredType,b->inferredType) || !finalExtras(a->semanticDeclaredType,b->semanticDeclaredType))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!finalExtras(a->children[i],b->children[i]))return 0;
    return 1;
}
static int original(xls_semantic_check* result,const xls_analysis_document* doc)
{
    xls_semantic_input input={doc->path,xlsTextData(doc->text),xlsTextSize(doc->text)};
    xls_sem_final_control control;xlsFrontendSemFinalBegin(&control,0);
    xls_semantic_check* full=xlsSemanticCheckCreate(doc->path,&input,1,1,NULL,NULL,NULL,0);
    xlsFrontendSemFinalEnd(&control);CHECK(full && result);
    CHECK(xlsSemanticCheckInfo(full).accepted==xlsSemanticCheckInfo(result).accepted);
    CHECK(sameAst(xlsSemanticCheckAst(full),xlsSemanticCheckAst(result)) && finalExtras(xlsSemanticCheckAst(full),xlsSemanticCheckAst(result)));
    char *a=xlsSemanticCheckJson(full,NULL),*b=xlsSemanticCheckJson(result,NULL);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);
    if(xlsSemanticCheckInfo(result).accepted){a=irJson(full);b=irJson(result);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);}
    xlsSemanticCheckDestroy(full);return 1;
}
static char* program(unsigned value,int regionChange,int locals,int move)
{
    char* base=source(value,locals,1);if(!base)return NULL;
    const char* at=strstr(base," return changed\r\n");xstrbuf out={0};
    int okay=at && (!move || xrtStrBufAppend(&out,xrtStrView("// 中文😀 moved\r\n"))) &&
        xrtStrBufAppend(&out,(xstrview){base,(size_t)(at-base)});
    if(regionChange)okay=okay && xrtStrBufAppend(&out,xrtStrView(" if changed>0\r\n while changed>1\r\n changed=changed-1\r\n end\r\n end\r\n"));
    okay=okay && xrtStrBufAppend(&out,xrtStrView(at));
    char* result=okay ? xrtStrDup(out.Data) : NULL;xrtStrBufFree(&out);xrtFree(base);return result;
}
static int edits(void)
{
    char* path=xrtPathAbs("final-annotations-中文😀.xl");xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session);
    xls_analysis_document doc={"entry",path,NULL,1,1};xls_semantic_check* held=NULL;uint64_t reused=0,plans=0;
    for(unsigned v=0;v<6;++v){
        char* body=program(v+1,v==2 || v==3,v==4,v==1 || v==3);CHECK(body);
        xls_semantic_check* result=update(session,&doc,1,body);if(result && !xlsSemanticCheckInfo(result).accepted){char* details=xlsSemanticCheckJson(result,NULL);fprintf(stderr,"REJECTED:%s\n",details ? details : "no report");xrtFree(details);}CHECK(result && xlsSemanticCheckInfo(result).accepted);
        xls_sem_final_work work=xlsSemanticCheckFinalWork(result);
        printf("final-v%u contexts=%llu hits=%llu typed=%llu plans=%llu prediction=%llu validated=%llu bytes=%llu\n",v,
            (unsigned long long)work.contexts,(unsigned long long)work.contextHits,(unsigned long long)work.typeReused,
            (unsigned long long)work.planReused,(unsigned long long)work.predictionNodes,(unsigned long long)work.validatedNodes,(unsigned long long)work.contextBytes);
        CHECK(original(result,&doc));
        if(!v){CHECK(work.contexts==1 && !work.typeReused && work.contextBytes);held=xlsSemanticCheckRetain(result);}
        if(v==1 || v==2 || v==3)CHECK(work.contextHits==1 && work.typeReused>=50 && work.planReused==work.typeReused);
        reused+=work.typeReused;plans+=work.planReused;xlsSemanticCheckDestroy(result);xrtFree(body);
    }
    CHECK(reused>=150 && plans==reused);
    xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(held) && xlsSemanticCheckFinalWork(held).contextBytes);xlsSemanticCheckDestroy(held);
    xlsTextRelease(doc.text);xrtFree(path);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!edits())return 1;
    printf("analysis_final_annotations_test passed:%u checks;original full AST/IR/final-field parity across literal,coordinate,region and local-symbol edits\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
#else
int main(void){puts("analysis_final_annotations_test skipped:old native representation");return 0;}
#endif
