#define main positionsFixtureMain
#include "workspace_edit_positions_test.c"
#undef main
static size_t allocationCalls,allocationBytes,failAt;static int probing;
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);
#ifdef XIDE_GUI_DLL_ALLOCATION_PROBE
/* The historical GUI SDK imports XRT through IAT slots, rather than plain
 * function references. Wrap those actual slots; preserve the real slots for
 * the forwarding call. Product Document/map objects remain unchanged. */
extern void* (*__real___imp_xrtMalloc)(size_t);
extern void* (*__real___imp_xrtCalloc)(size_t,size_t);
#define REAL_MALLOC __real___imp_xrtMalloc
#define REAL_CALLOC __real___imp_xrtCalloc
#else
#define REAL_MALLOC __real_xrtMalloc
#define REAL_CALLOC __real_xrtCalloc
#endif
void* __wrap_xrtMalloc(size_t size){if(probing){allocationBytes+=size;if(++allocationCalls==failAt)return NULL;}return REAL_MALLOC(size);}
void* __wrap_xrtCalloc(size_t count,size_t size){if(probing){allocationBytes+=count*size;if(++allocationCalls==failAt)return NULL;}return REAL_CALLOC(count,size);}
#ifdef XIDE_GUI_DLL_ALLOCATION_PROBE
void* (*__wrap___imp_xrtMalloc)(size_t)=__wrap_xrtMalloc;
void* (*__wrap___imp_xrtCalloc)(size_t,size_t)=__wrap_xrtCalloc;
#endif
typedef struct stop_at{size_t calls,at;} stop_at;
static int stopMap(void* user){stop_at* s=user;return ++s->calls==s->at;}
int main(int argc,char** argv)
{
 /* Keep the shared fixture functions compiled, and mark their deliberate non-use. */
 (void)positionsFixtureMain;(void)at;(void)literals;(void)dense;(void)lifetime;
 xstrbuf before={0},after={0};for(unsigned i=0;i<100000;++i)CHECK(xrtStrBufAppend(&before,xrtStrView("a")) && xrtStrBufAppend(&after,xrtStrView("a")));CHECK(xrtStrBufAppend(&before,xrtStrView("OLD")) && xrtStrBufAppend(&after,xrtStrView("NEW")));for(unsigned i=0;i<100000;++i)CHECK(xrtStrBufAppend(&before,xrtStrView("z")) && xrtStrBufAppend(&after,xrtStrView("z")));
 xvalue* edits=xrtValueArray();CHECK(edits && xrtValueArrayAppendNew(edits,textEdit(0,0,(int)before.Size,after.Data)));xide_workspace_edit* plan=prepare(before.Data,edits);uint64_t blocks=editBlocks();enum xide_workspace_edit_error error;
 if(argc==2){failAt=(size_t)strtoull(argv[1],NULL,10);CHECK(failAt>=1 && failAt<=3);probing=1;xide_edit_positions* p=xideEditPositionsPrepare(plan,0,NULL,NULL,&error);probing=0;CHECK(!p && error==XIDE_WORKSPACE_EDIT_MEMORY && allocationCalls==failAt && editBlocks()==blocks);}
 else{
  probing=1;xide_edit_positions* p=xideEditPositionsPrepare(plan,0,NULL,NULL,&error);probing=0;CHECK(p && !error && allocationCalls==3);size_t knownMapBytes=allocationBytes;allocationCalls=0;probing=1;size_t value;CHECK(xideEditPositionMap(p,0,100001,XIDE_EDIT_RIGHT,&value) && value==100003 && !allocationCalls);probing=0;xideEditPositionsDestroy(p);CHECK(editBlocks()==blocks);
  stop_at observed={0,SIZE_MAX};p=xideEditPositionsPrepare(plan,0,stopMap,&observed,&error);CHECK(p && !error && observed.calls==8);xideEditPositionsDestroy(p);for(size_t i=1;i<=observed.calls;++i){stop_at stop={0,i};p=xideEditPositionsPrepare(plan,0,stopMap,&stop,&error);CHECK(!p && error==XIDE_WORKSPACE_EDIT_CANCELLED && stop.calls==i && editBlocks()==blocks);}
  xide_workspace_edit_info info;CHECK(xideWorkspaceEditInfo(plan,&info));size_t low=info.bytes,high=info.bytes+4096;while(low<high){size_t middle=low+(high-low)/2;p=xideEditPositionsPrepare(plan,middle,NULL,NULL,&error);if(p){xideEditPositionsDestroy(p);high=middle;}else{CHECK(error==XIDE_WORKSPACE_EDIT_LIMIT);low=middle+1;}}
  CHECK(low==info.bytes+knownMapBytes);p=xideEditPositionsPrepare(plan,low,NULL,NULL,&error);CHECK(p && !error && xideEditPositionsStorage(p)==low && !xideEditPositionsStorage(NULL));xideEditPositionsDestroy(p);CHECK(!xideEditPositionsPrepare(plan,low-1,NULL,NULL,&error) && error==XIDE_WORKSPACE_EDIT_LIMIT && editBlocks()==blocks);printf("map allocation/cancellation:3 cold gates;%zu observed cancel points;known quota%zu bytes\n",observed.calls,low);
 }
 xideWorkspaceEditRelease(plan);xrtStrBufFree(&before);xrtStrBufFree(&after);printf("workspace_edit_positions_fault_test passed: %u checks; refusal/cancel/budget and no-allocation lookup\n",checks);return 0;
}
