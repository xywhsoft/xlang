#ifndef XL6_TEST_FLOW_BUSY_PROBE_H
#define XL6_TEST_FLOW_BUSY_PROBE_H
#include <xge3_runtime.h>
/* Test-only unique-root prototype. No safe Graph API is published by this
 * fixture; same-model Busy is tested; safe long-lived aliases remain separate work. */
static inline bool fixtureFlowBusyDestroy(void* raw)
{return xuiFlowGraphDestroyChecked((xui_flow_graph)raw)==XUI_OK;}
static inline bool fixtureFlowBusyTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return xuiFlowGraphTraceOwnedValues((xui_flow_graph)raw,visit,user);}
static inline const XL6_ForeignViewDescriptorV1* fixtureFlowBusyPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,"test.flowConfig",0,1,NULL,NULL,fixtureFlowBusyDestroy,0,1,NULL,NULL,fixtureFlowBusyTrace,NULL,0,NULL,NULL};
 return &policy;
}
#define fixtureFlowBusyPolicy (*fixtureFlowBusyPolicyGet())
int fixtureFlowBusyAdversarialValue(xui_flow_graph graph,xvalue** output);
int fixtureFlowBusyCloneCount(void);
int fixtureFlowBusyDropCount(void);
void fixtureFlowBusyResetCounts(void);
int fixtureFlowBusyErrorCode(xerror* error);
void fixtureFlowBusyHeapBegin(void);
void fixtureFlowBusyHeapEqual(void);
#endif
