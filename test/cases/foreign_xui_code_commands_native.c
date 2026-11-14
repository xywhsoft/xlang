/* The independent native suite and boundary/OOM checks use the actual SDK. */
#define XRT_MODULE_ALL
#if defined(_WIN32)
#define XRT_USE_SHARED
#endif
#include "foreign_xui_code_commands.h"
#if defined(XGE_TEST_STATIC)
#include <xge_static_xrt_contract.h>
#else
#include <xge_host_xrt_contract.h>
static xge_host_xrt_proc resolve(void* user,const char* name)
{
 (void)user;
#define XGE_HOST_XRT_SLOT(symbol) if(!strcmp(name,#symbol))return (xge_host_xrt_proc)&symbol;
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
 return NULL;
}
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
int main(void)
{
#if defined(XGE_TEST_STATIC)
 const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
 assert(!strcmp(contract.headerSha256,xgeStaticXrtContractV1.headerSha256));
#else
 const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
 assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
#endif
 assert(xrtMemDebugEnable(true));xui_code_command_map map=NULL;assert(xuiCodeCommandMapCreate(&map)==XUI_OK);
 const int defaults=xuiCodeCommandMapGetCount(map);assert(defaults>0);int command=99;
 assert(xuiCodeCommandMapFind(map,'A',XUI_MOD_CTRL,&command)==XUI_OK&&command==XUI_CODE_COMMAND_SELECT_ALL);
 xui_code_key_binding_t row={0};assert(xuiCodeCommandMapGetBinding(map,0,&row)==XUI_OK&&row.iSize==sizeof(row));
 assert(row.iKey==XUI_KEY_LEFT&&row.iCommand==XUI_CODE_COMMAND_MOVE_LEFT&&row.iWhen==0);
 row.iCommand=99;assert(xuiCodeCommandMapGetBinding(map,0,&row)==XUI_OK&&row.iCommand!=99);
 assert(xuiCodeCommandMapBind(map,XUI_KEY_F2,XUI_MOD_CTRL,7777)==XUI_OK);
 assert(xuiCodeCommandMapBind(map,XUI_KEY_F2,XUI_MOD_CTRL,8888)==XUI_OK&&xuiCodeCommandMapGetCount(map)==defaults+1);
 assert(xuiCodeCommandMapFind(map,XUI_KEY_F2,XUI_MOD_CTRL,&command)==XUI_OK&&command==8888);
 assert(xuiCodeCommandMapBind(map,'a',XUI_MOD_CTRL,7777)==XUI_OK);
 assert(xuiCodeCommandMapFind(map,'A',XUI_MOD_CTRL,&command)==XUI_OK&&command==XUI_CODE_COMMAND_SELECT_ALL);
 assert(xuiCodeCommandMapUnbind(map,XUI_KEY_F2,XUI_MOD_CTRL)==XUI_OK);
 command=99;assert(xuiCodeCommandMapFind(map,XUI_KEY_F2,XUI_MOD_CTRL,&command)==XUI_ERROR_UNSUPPORTED&&command==99);
 assert(xuiCodeCommandMapUnbind(map,XUI_KEY_F2,XUI_MOD_CTRL)==XUI_ERROR_UNSUPPORTED);
 assert(xuiCodeCommandMapBind(map,0,0,1)==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiCodeCommandMapBind(map,XUI_KEY_F2,UINT32_C(0x80000000),1)==XUI_ERROR_INVALID_ARGUMENT);
 assert(xuiCodeCommandMapBind(map,XUI_KEY_F2,0,0)==XUI_ERROR_INVALID_ARGUMENT);
 for(int operation=0;operation<3;++operation){unsigned failures=0,successes=0;
  for(int budget=0;budget<8;++budget){fixtureCommandPrefixBegin();int status;
   if(operation==0){xui_code_command_map candidate=NULL;fixtureCommandBudget(budget);status=xuiCodeCommandMapCreate(&candidate);fixtureCommandBudget(-1);if(status==XUI_OK)xuiCodeCommandMapDestroy(candidate);else assert(!candidate);}
   else{
    xui_code_command_map candidate=NULL;assert(xuiCodeCommandMapCreate(&candidate)==XUI_OK);
    if(operation==1)assert(xuiCodeCommandMapBind(candidate,XUI_KEY_F2,0,7777)==XUI_OK);
    fixtureCommandBudget(budget);status=operation==1 ? xuiCodeCommandMapLoadDefaults(candidate):xuiCodeCommandMapBind(candidate,XUI_KEY_F2,0,7777);fixtureCommandBudget(-1);
    if(status!=XUI_OK){if(operation==1)assert(xuiCodeCommandMapFind(candidate,XUI_KEY_F2,0,&command)==XUI_OK&&command==7777);else assert(xuiCodeCommandMapGetCount(candidate)==defaults);}
    else assert(xuiCodeCommandMapGetCount(candidate)==defaults+operation-1);
    xuiCodeCommandMapDestroy(candidate);
   }
   if(status==XUI_OK)++successes;else ++failures;
   fixtureCommandPrefixEnd();fixtureCommandHeapEqual();
  }
  assert(failures&&successes);
 }
 assert(xuiCodeCommandMapLoadDefaults(map)==XUI_OK);
 assert(xuiCodeCommandMapBind(map,XUI_KEY_F2,XUI_MOD_CTRL,7777)==XUI_OK);
 fixtureCommandNativeHot(map);fixtureCommandBudget(0);fixtureCommandNativeHot(map);fixtureCommandBudget(-1);
 xuiCodeCommandMapDestroy(map);xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(!after.LiveCount&&!after.InvalidFreeCount&&!after.DoubleFreeCount);
 puts("CodeCommands native: exact SDK defaults, key identity, records, checked status, OOM and zero-heap queries passed");return 0;
}
