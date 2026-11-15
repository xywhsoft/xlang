#include "foreign_xui_flow_busy_probe.h"
#include <assert.h>
static int clones,drops;
static xmemdebugsnapshot before;
static bool cloneConfig(void* raw,void** output,void* user)
{
 (void)raw;++clones;*output=NULL;
 uint32_t revision=123;
 assert(xuiFlowGraphRemoveNode((xui_flow_graph)user,"one")==XUI_ERROR_BUSY);
 assert(xuiFlowGraphDestroyChecked((xui_flow_graph)user)==XUI_ERROR_BUSY);
 assert(xuiFlowGraphGetRevisionChecked((xui_flow_graph)user,&revision)==XUI_ERROR_BUSY && revision==123);
 assert(xuiFlowGraphGetNodeCount((xui_flow_graph)user)==XUI_ERROR_BUSY);
 *output=(void*)2;return true;
}
static void dropConfig(void* raw,void* user)
{if(raw==(void*)2){assert(xuiFlowGraphRemoveNode((xui_flow_graph)user,"one")==XUI_ERROR_BUSY);++drops;}}
int fixtureFlowBusyAdversarialValue(xui_flow_graph graph,xvalue** output)
{
 /* Deliberately adversarial, test-only C value. Its raw graph userdata is
  * borrowed while the outer ProbeBusyGraph owner remains alive. Clone/Drop never
  * escape this synchronous fixture or its loaded module. Not a safe public
  * value constructor or a proof of arbitrary retained code/resource trees. */
 static const xvaluehandleops ops={cloneConfig,dropConfig,NULL,NULL};
 void* raw=(void*)1;*output=xrtValueHandleTake(&raw,&ops,graph);
 return *output?XUI_OK:XUI_ERROR_OUT_OF_MEMORY;
}
int fixtureFlowBusyCloneCount(void){return clones;}
int fixtureFlowBusyDropCount(void){return drops;}
void fixtureFlowBusyResetCounts(void){clones=drops=0;}
int fixtureFlowBusyErrorCode(xerror* error){return xrtErrorCode(error);}
void fixtureFlowBusyHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureFlowBusyHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}
