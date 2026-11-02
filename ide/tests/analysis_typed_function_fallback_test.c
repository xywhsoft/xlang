#define main typedFunctionReferenceMain
#include "analysis_semantic_memo_test.c"
#undef main
#include "analysis/function_unit_internal.h"

static const XL6_AstNode* selectFunction(const XL6_AstNode* node,size_t wanted,size_t* count)
{
    if(!node)return NULL;
    if(xlsFrontendFunctionDefinition(node)){if((*count)++==wanted)return node;}
    for(size_t i=0;i<node->childCount;++i){const XL6_AstNode* found=selectFunction(node->children[i],wanted,count);if(found)return found;}
    return NULL;
}
static int mixedFunctions(void)
{
    char* path=xrtPathAbs("typed-function-mixed-中文😀.xl");CHECK(path);
    const char* source="class Box\n string item=\"owned\"\n func value() as string\n return self.item\n end\nend\nfunc closure() as func() as int\n array values=[41]\n return func() as int\n return int(values[0])\n end\nend\nfunc main(...) as int\n return 0\nend\n";
    xls_text* text=xlsTextCreate(source,strlen(source),8u*1024u*1024u,NULL);CHECK(text);
    xls_analysis_document document={"entry",path,text,81,1};xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);char error[256];
    xls_semantic_check* check=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));
    CHECK(check && xlsSemanticCheckInfo(check).accepted && xlsFrontendCheckFunctionModule(check));
    size_t count=0;CHECK(xlsSemanticCheckFunctionCount(check,&count) && count>=4);
    xls_function_set* set=xlsSemanticCheckFunctionsCapture(check,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(set);
    xls_function_set_info info=xlsFunctionSetInfo(set);CHECK(info.functions==count);
    xls_function_set* exact=xlsSemanticCheckFunctionsCapture(check,info.ownedBytes,NULL,NULL,error,sizeof(error));CHECK(exact);xlsFunctionSetRelease(exact);
    CHECK(!xlsSemanticCheckFunctionsCapture(check,info.ownedBytes-1,NULL,NULL,error,sizeof(error)));
    const XL6_AstNode* root=xlsSemanticCheckAst(check);size_t borrowed=0,cloned=0;
    XL6_AstNode** snapshots=xrtCalloc(count,sizeof(*snapshots));CHECK(snapshots);
    for(size_t i=0;i<count;++i){
        size_t seen=0;const XL6_AstNode* expected=selectFunction(root,i,&seen);CHECK(expected);
        xls_function_unit* unit=xlsFunctionSetUnit(set,i);CHECK(unit);
        xls_function_unit* individual=xlsSemanticCheckFunctionCapture(check,i,64u*1024u*1024u,NULL,NULL,error,sizeof(error));CHECK(individual);
        int top=0;for(size_t j=0;j<root->childCount;++j)if(root->children[j]==expected)top=1;
        const XL6_AstNode* actual=xlsFrontendFunctionUnitRoot(unit);
        if(top){CHECK(actual==expected && xlsFrontendFunctionUnitRoot(individual)==expected);++borrowed;}
        else{CHECK(actual!=expected && xlsFrontendFunctionUnitRoot(individual)!=expected);++cloned;}
        CHECK(sameAst(actual,expected) && sameAst(xlsFrontendFunctionUnitRoot(individual),expected));
        xls_function_info unitInfo=xlsFunctionUnitInfo(individual);xls_function_unit* exactUnit=xlsSemanticCheckFunctionCapture(check,i,unitInfo.ownedBytes,NULL,NULL,error,sizeof(error));CHECK(exactUnit);xlsFunctionUnitRelease(exactUnit);
        snapshots[i]=xlsFrontendFunctionUnitMaterialize(unit,64u*1024u*1024u,NULL,NULL);CHECK(snapshots[i] && sameAst(snapshots[i],expected));
        xlsFunctionUnitRelease(individual);xlsFunctionUnitRelease(unit);
    }
    CHECK(borrowed>=2 && cloned>=2);
    xlsCheckSessionDestroy(session);xlsSemanticCheckDestroy(check);xlsTextRelease(text);xrtFree(path);
    for(size_t i=0;i<count;++i){xls_function_unit* unit=xlsFunctionSetUnit(set,i);CHECK(unit && sameAst(xlsFrontendFunctionUnitRoot(unit),snapshots[i]));
        XL6_AstNode* actual=xlsFrontendFunctionUnitMaterialize(unit,64u*1024u*1024u,NULL,NULL);CHECK(actual && sameAst(actual,snapshots[i]));
        xl6AstFree(actual);xl6AstFree(snapshots[i]);xlsFunctionUnitRelease(unit);
    }
    xrtFree(snapshots);xlsFunctionSetRelease(set);
    printf("mixed cached capture:%zu original native functions,%zu independent nested/method clones;exact collection budget;producer destroyed\n",borrowed,cloned);return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!mixedFunctions())return 1;
    printf("analysis_typed_function_fallback_test passed:%u checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
