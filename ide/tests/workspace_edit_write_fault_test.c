#include "workspace_edit_spool_fixture.h"
#define main writerFixtureMain
#include "workspace_edit_write_test.c"
#undef main
#ifndef XLS_COMPILER_XRT
#error "Pinned writer allocation qualification requires native XRT"
#endif
void* __real_xrtMalloc(size_t);
void* __wrap_xrtMalloc(size_t n){return refused()?NULL:__real_xrtMalloc(n);}
int main(int argc,char** argv)
{
 CHECK(argc==3&&xrtDirCreateAll(argv[1]));xrtMemStatsEnable(true);uint64_t initial=editBlocks();void* control=xrtMalloc(64);CHECK(control&&editBlocks()>initial);xrtFree(control);CHECK(editBlocks()==initial);warmServices(argv[1]);sample s;makeSample(&s,argv[1],"original","new");xideEditFileWriteDestroy(s.write);s.write=NULL;xrtClearError();uint64_t baseline=editBlocks();DWORD handles=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));allocationCalls=0;failAllocation=(size_t)strtoull(argv[2],NULL,10);tracking=1;xide_model_error e={0};s.write=xideEditFileWritePrepare(s.spool,0,0,&e);tracking=0;CHECK(!failAllocation||allocationCalls>=failAllocation);if(s.write)CHECK(!e.code);else CHECK(failAllocation&&e.code);xideEditFileWriteDestroy(s.write);s.write=NULL;xrtClearError();CHECK(editBlocks()==baseline);DWORD after=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);bytesEqual(s.source,"original");printf("workspace_edit_write_fault_test passed: %u checks; native heap statistics active, allocation gate%zu/%zu, steady blocks%llu and handles%lu\n",checks,failAllocation,allocationCalls,(unsigned long long)baseline,(unsigned long)after);clearSample(&s);return 0;
}
