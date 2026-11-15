#include "foreign_xui_flow_config_probe.h"
#include <assert.h>
static int clones,drops;
static xmemdebugsnapshot before;
static bool cloneConfig(void* raw,void** output,void* user)
{
 (void)raw;++clones;*output=NULL;
 if(xuiFlowGraphRemoveNode((xui_flow_graph)user,"one")!=XUI_OK)return false;
 *output=(void*)2;return true;
}
static void dropConfig(void* raw,void* user)
{(void)user;if(raw==(void*)2)++drops;}
int fixtureFlowAdversarialValue(xui_flow_graph graph,xvalue** output)
{
 /* Deliberately adversarial, test-only C value. Its raw graph userdata is
  * borrowed while the outer ProbeGraph owner remains alive. Clone/Drop never
  * escape this synchronous fixture or its loaded module. Not a safe public
  * value constructor or a proof of arbitrary retained code/resource trees. */
 static const xvaluehandleops ops={cloneConfig,dropConfig,NULL,NULL};
 void* raw=(void*)1;*output=xrtValueHandleTake(&raw,&ops,graph);
 return *output?XUI_OK:XUI_ERROR_OUT_OF_MEMORY;
}
int fixtureFlowCloneCount(void){return clones;}
int fixtureFlowDropCount(void){return drops;}
void fixtureFlowResetCounts(void){clones=drops=0;}
int fixtureFlowErrorCode(xerror* error){return xrtErrorCode(error);}
void fixtureFlowHeapBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureFlowHeapEqual(void)
{
 xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}
