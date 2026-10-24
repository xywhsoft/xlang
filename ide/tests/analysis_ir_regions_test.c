#include "analysis/frontend_ir_regions.h"
#include <stdio.h>
#include <string.h>
static size_t operations,failAt,arrays;
static int badLifetime;
static int allowed(void) { return ++operations!=failAt; }
static bool init(xarray* a,size_t bytes)
{ if(!allowed()) return false; bool okay=xrtArrayInit(a,bytes); if(okay) ++arrays; return okay; }
static bool reserve(xarray* a,size_t count) { return allowed() && xrtArrayReserve(a,count); }
static bool push(xarray* a,const void* value) { return allowed() && xrtArrayPush(a,value); }
static bool sort(xarray* a,xarraycompare compare) { return allowed() && xrtArraySort(a,compare); }
static void unit(xarray* a)
{ if(a->ItemSize) {if(!arrays)badLifetime=1;else --arrays;} xrtArrayUnit(a); }
#define xrtArrayInit init
#define xrtArrayReserve reserve
#define xrtArrayPush push
#define xrtArraySort sort
#define xrtArrayUnit unit
#define xlsIrRegionBuild testBuild
#define xlsIrRegionClear testClear
#define xlsIrRegionFirst testFirst
#define xlsIrRegionNext testNext
#define xlsFrontendIrRegionReset testReset
#define xlsFrontendIrRegionWork testWork
#include "analysis/frontend_ir_regions.c"
#undef xrtArrayInit
#undef xrtArrayReserve
#undef xrtArrayPush
#undef xrtArraySort
#undef xrtArrayUnit
#undef xlsIrRegionBuild
#undef xlsIrRegionClear
#undef xlsIrRegionFirst
#undef xlsIrRegionNext
#undef xlsFrontendIrRegionReset
#undef xlsFrontendIrRegionWork
static unsigned checks;
#define CHECK(value) do {++checks;if(!(value)){fprintf(stderr,"analysis_ir_regions_test:%d: %s\n",__LINE__,#value);return 0;}}while(0)
static int compare(xls_ir_region_index* index,const XL6_IrModule* module,uint32 region)
{
    size_t expected=0;
    for(size_t at=testFirst(index,module,region);at<module->nodeCount;at=testNext(index,module,region,at)) {
        if(module->nodes[at].kind!=XL6_IR_NODE_BLOCK || module->nodes[at].region!=region) continue;
        while(expected<module->nodeCount && (module->nodes[expected].kind!=XL6_IR_NODE_BLOCK || module->nodes[expected].region!=region)) ++expected;
        CHECK(at==expected); ++expected;
    }
    while(expected<module->nodeCount && (module->nodes[expected].kind!=XL6_IR_NODE_BLOCK || module->nodes[expected].region!=region)) ++expected;
    CHECK(expected==module->nodeCount); return 1;
}
static int run(void)
{
    XL6_IrNode nodes[100]={0},original[100];
    for(size_t i=0;i<100;++i) {nodes[i].kind=i%3 ? XL6_IR_NODE_NAME : XL6_IR_NODE_BLOCK;nodes[i].region=(uint32)((i*17)%7);}
    nodes[99].region=UINT32_MAX; memcpy(original,nodes,sizeof(nodes));
    XL6_IrModule module={.nodes=nodes,.nodeCount=100,.nodeCapacity=100};xls_ir_region_index index={0};
    testReset(); CHECK(testBuild(&index,&module,XLS_IR_REGION_BYTES) && arrays==1);
    size_t calls=operations; xls_ir_region_work built=testWork(); CHECK(built.indexes==1 && built.records==34 && built.rootVisits==200 && built.peakBytes<=XLS_IR_REGION_BYTES);
    for(uint32 region=0;region<9;++region) { CHECK(compare(&index,&module,region)); }
    CHECK(compare(&index,&module,UINT32_MAX));
    CHECK(testNext(&index,&module,0,SIZE_MAX)==100);
    XL6_IrModule foreign=module;CHECK(compare(&index,&foreign,3));
    --module.nodeCount; CHECK(compare(&index,&module,3)); ++module.nodeCount;
    XL6_IrNode* saved=module.nodes;module.nodes=original;CHECK(compare(&index,&module,3));module.nodes=saved;
    testClear(&index);CHECK(!arrays && !badLifetime);
    for(size_t fail=1;fail<=calls;++fail) {
        operations=0;failAt=fail;CHECK(!testBuild(&index,&module,XLS_IR_REGION_BYTES) && !arrays && !index.ready && !badLifetime);
        CHECK(compare(&index,&module,3));operations=failAt=0;CHECK(testBuild(&index,&module,XLS_IR_REGION_BYTES));testClear(&index);CHECK(!arrays);
    }
    for(size_t bytes=0;bytes<2;++bytes) {CHECK(!testBuild(&index,&module,bytes) && !arrays);CHECK(compare(&index,&module,3));}
    size_t preflight=2*34*sizeof(region_row);CHECK(testBuild(&index,&module,preflight));testClear(&index);
    CHECK(!testBuild(&index,&module,preflight-1) && !arrays);CHECK(compare(&index,&module,UINT32_MAX));
    module.nodeCapacity=99;CHECK(!testBuild(&index,&module,XLS_IR_REGION_BYTES) && !arrays);module.nodeCapacity=100;
    CHECK(!testBuild(&index,NULL,XLS_IR_REGION_BYTES) && !testBuild(NULL,&module,XLS_IR_REGION_BYTES));
    CHECK(testFirst(NULL,NULL,0)==0 && testNext(NULL,NULL,0,SIZE_MAX)==0);
    XL6_IrModule empty={0};CHECK(testBuild(&index,&empty,XLS_IR_REGION_BYTES) && testFirst(&index,&empty,0)==0);testClear(&index);
    CHECK(!arrays && !badLifetime && !memcmp(original,nodes,sizeof(nodes)));
    printf("IR region selector allocation/container seams=%zu,all independently failed/recovered\n",calls);return 1;
}
int main(void)
{if(!run())return 1;printf("analysis_ir_regions_test passed: %u checks; actual ordered selector,same-region blocks,missing/zero/max keys,faults,budgets,identity and immutable input.\n",checks);return 0;}
