#include "workspace_edit_spool_fixture.h"
#ifndef XLS_COMPILER_XRT
#error "Spool allocation qualification requires the native XRT/CRT profile"
#endif
int main(int argc,char** argv)
{
 CHECK(argc==3&&xrtDirCreateAll(argv[1]));xrtMemStatsEnable(true);uint64_t initial=editBlocks();void* control=xrtMalloc(128);CHECK(control&&editBlocks()>initial);xrtFree(control);CHECK(editBlocks()==initial);data d;init(&d,argv[1]);warmServices(argv[1]);char* scope=xidePathResolveLexical(argv[1],"scope");CHECK(scope);DWORD handles=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));uint64_t blocks=editBlocks();allocationCalls=0;failAllocation=(size_t)strtoull(argv[2],NULL,10);tracking=1;xide_model_error error={0};xide_edit_spool* p=xideEditSpoolPrepare(d.plan,&d.view,1,scope,NULL,0,NULL,NULL,&error);tracking=0;
 CHECK(!failAllocation||allocationCalls>=failAllocation);if(p){inspect(p,1);xideEditSpoolDestroy(p);}else{CHECK(failAllocation&&error.code);char* marker=xidePathResolveLexical(scope,"edit-journal.json");CHECK(marker);if(xideIoExists(marker)){p=xideEditSpoolOpen(scope,NULL,NULL,&error);CHECK(p);inspect(p,0);xideEditSpoolDestroy(p);}free(marker);}
 for(unsigned i=0;i<3;++i)diskEquals(d.paths[i],savedRaw[i]);
 xrtClearError();DWORD after=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);uint64_t remaining=editBlocks();CHECK(remaining==blocks);printf("workspace_edit_spool_fault_test passed: %u checks; allocation gate%zu/%zu, native memory statistics active, steady heap blocks%llu and handles%lu\n",checks,failAllocation,allocationCalls,(unsigned long long)remaining,(unsigned long)after);freeData(&d);free(scope);return 0;
}
