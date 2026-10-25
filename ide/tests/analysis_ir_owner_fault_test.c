/* A diagnostic build supplies the exact generated IR TU plus a copy of its
 * memo include with only OwnerPrepare renamed. Production has no fault hook. */
#include <xrt.h>
#include "analysis/frontend_ir_memo.h"
#include <stdio.h>
static size_t ownerCalls,ownerFailAt,ownerHits;
static int ownerArea;
static bool ownerArrayInit(xarray*,size_t);
static bool ownerArrayResize(xarray*,size_t);
static bool ownerArrayReserve(xarray*,size_t);
static int xlsIrOwnerPrepare(XL6_IrModule*,size_t);
#define xrtArrayInit ownerArrayInit
#define xrtArrayResize ownerArrayResize
#define xrtArrayReserve ownerArrayReserve
/* The unchanged compiler TU normally builds with its material warning
 * profile. Keep fixture logic below under the strict test -Werror profile. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include XLS_IR_OWNER_FIXTURE
#pragma GCC diagnostic pop
#undef xrtArrayInit
#undef xrtArrayResize
#undef xrtArrayReserve
static int ownerAllow(void)
{if(!ownerArea)return 1;if(++ownerCalls==ownerFailAt){++ownerHits;return 0;}return 1;}
static bool ownerArrayInit(xarray* array,size_t size)
{return ownerAllow() && xrtArrayInit(array,size);}
static bool ownerArrayResize(xarray* array,size_t size)
{return ownerAllow() && xrtArrayResize(array,size);}
static bool ownerArrayReserve(xarray* array,size_t size)
{return ownerAllow() && xrtArrayReserve(array,size);}
static int xlsIrOwnerPrepare(XL6_IrModule* module,size_t nodes)
{int before=ownerArea;ownerArea=1;int okay=xlsIrOwnerPrepareOriginal(module,nodes);ownerArea=before;return okay;}
#define main unusedNormalFixtureMain
#include "analysis_ir_memo_test.c"
#undef main
static int faultRun(void)
{
    char* path=xrtPathAbs("ir-owner-fault-中文.xl");char* text=body(0);CHECK(path && text);
    xls_semantic_check* source=checked(path,text);CHECK(source && xlsSemanticCheckInfo(source).accepted);
    const XL6_AstNode* root=xlsSemanticCheckAst(source);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);
    XL6_DiagList diags;xl6DiagInit(&diags);
    xls_ir_result* first=xlsFrontendIrResultBuild(root,&diags,NULL,32u*1024u*1024u);CHECK(first);
    xls_ir_result* baseline=xlsFrontendIrResultBuild(root,&diags,first,32u*1024u*1024u);CHECK(baseline && !diags.count);
    xlsFrontendIrResultDestroy(first);
    xarray references={0};CHECK(xrtArrayInit(&references,sizeof(uint32)) && xrtArrayResize(&references,baseline->memo->entries.Count));
    for(size_t i=0;i<references.Count;++i){
        const xls_ir_unit* unit=*(xls_ir_unit* const*)xrtArrayConstGet(&baseline->memo->entries,i);
        *(uint32*)xrtArrayGet(&references,i)=unit ? xrtAtomic32Load(&unit->references,XMEMORY_ACQUIRE) : 0;
    }
    ownerCalls=ownerFailAt=ownerHits=0;
    xls_ir_result* normal=xlsFrontendIrResultBuild(root,&diags,baseline,32u*1024u*1024u);
    CHECK(normal && !diags.count && xlsFrontendIrResultWork(normal).textNodes);
    size_t operations=ownerCalls;CHECK(operations>16);xlsFrontendIrResultDestroy(normal);
    for(size_t fail=1;fail<=operations;++fail){
        ownerCalls=ownerHits=0;ownerFailAt=fail;
        xls_ir_result* current=xlsFrontendIrResultBuild(root,&diags,baseline,32u*1024u*1024u);
        CHECK(current && !diags.count && ownerHits==1);ownerFailAt=0;
        xls_ir_memo_work work=xlsFrontendIrResultWork(current);CHECK(work.reused && work.ownerBytes<=128u*1024u);
        char* actual=dump(xlsFrontendIrResultModule(current));CHECK(actual && !strcmp(actual,expected));xrtFree(actual);xlsFrontendIrResultDestroy(current);
        for(size_t i=0;i<references.Count;++i){
            const xls_ir_unit* unit=*(xls_ir_unit* const*)xrtArrayConstGet(&baseline->memo->entries,i);
            CHECK((unit ? xrtAtomic32Load(&unit->references,XMEMORY_ACQUIRE) : 0)==*(const uint32*)xrtArrayConstGet(&references,i));
        }
    }
    xrtArrayUnit(&references);xlsFrontendIrResultDestroy(baseline);xlsSemanticCheckDestroy(source);xl6DiagFree(&diags);xrtFree(expected);xrtFree(text);xrtFree(path);
    printf("owner index fault gates=%zu; optional init/resize/reserve rollback,complete fresh IR parity andheld unit references\n",operations);return 1;
}
int main(void)
{xrtAtomic32Init(&checks,0);if(!faultRun())return 1;printf("analysis_ir_owner_fault_test passed: %u checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
