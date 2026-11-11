#include "workspace_edit_spool_fixture.h"
#define main journalFixtureMain
#include "workspace_edit_journal_test.c"
#undef main
#ifndef XLS_COMPILER_XRT
#error "Journal allocation qualification requires native XRT"
#endif
void* __real_xrtMalloc(size_t);
void* __wrap_xrtMalloc(size_t n){return refused()?NULL:__real_xrtMalloc(n);}
int main(int argc,char** argv)
{
 CHECK((argc==3||argc==4)&&xrtDirCreateAll(argv[1]));xrtMemStatsEnable(true);uint64_t initial=editBlocks();void* p=xrtMalloc(64);CHECK(p&&editBlocks()>initial);xrtFree(p);CHECK(editBlocks()==initial);warmServices(argv[1]);group g;createGroup(&g,argv[1]);xide_model_error e={0};int appendStage=argc==4;if(appendStage){g.journal=xideEditJournalCreate(g.spool,&e);CHECK(g.journal&&xideEditJournalSetPhase(g.journal,XIDE_EDIT_TX_APPLYING,0,&e));}xrtClearError();uint64_t blocks=editBlocks();DWORD handles=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));tracking=1;failAllocation=(size_t)strtoull(argv[2],NULL,10);allocationCalls=0;int okay=0;if(appendStage)okay=xideEditJournalFileIntent(g.journal,0,XIDE_EDIT_FILE_APPLY,&e);else{g.journal=xideEditJournalCreate(g.spool,&e);okay=g.journal!=NULL;}tracking=0;CHECK(!failAllocation||allocationCalls>=failAllocation);if(okay)CHECK(!e.code);else CHECK(failAllocation&&e.code);if(!appendStage){xideEditJournalDestroy(g.journal);g.journal=NULL;}xrtClearError();CHECK(editBlocks()==blocks);DWORD after=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);CHECK(xideWorkspaceEditFilesValidate(xideEditSpoolSources(g.spool),NULL,NULL,&e));printf("workspace_edit_journal_fault_test passed: %u checks; allocation gate%zu/%zu, appendStage%d, native memory statistics active, steady blocks%llu handles%lu\n",checks,failAllocation,allocationCalls,appendStage,(unsigned long long)blocks,(unsigned long)after);clearGroup(&g);return 0;
}
