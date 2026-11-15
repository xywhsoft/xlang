#ifndef XL6_TEST_FLOW_CONFIG_PROBE_H
#define XL6_TEST_FLOW_CONFIG_PROBE_H
#include <xge3_runtime.h>
/* Test-only unique-root prototype. No safe Graph API is published by this
 * fixture; command-history/reentry/alias contracts remain separate work. */
static inline bool fixtureFlowDestroy(void* raw)
{xuiFlowGraphDestroy((xui_flow_graph)raw);return true;}
static inline bool fixtureFlowTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return xuiFlowGraphTraceOwnedValues((xui_flow_graph)raw,visit,user);}
static inline const XL6_ForeignViewDescriptorV1* fixtureFlowPolicyGet(void)
{
 static const XL6_ForeignViewDescriptorV1 policy={sizeof(policy),XL6_FOREIGN_VIEW_V1_TAG,"test.flowConfig",0,1,NULL,NULL,fixtureFlowDestroy,0,1,NULL,NULL,fixtureFlowTrace,NULL,0,NULL,NULL};
 return &policy;
}
#define fixtureFlowPolicy (*fixtureFlowPolicyGet())
int fixtureFlowAdversarialValue(xui_flow_graph graph,xvalue** output);
int fixtureFlowCloneCount(void);
int fixtureFlowDropCount(void);
void fixtureFlowResetCounts(void);
int fixtureFlowErrorCode(xerror* error);
void fixtureFlowHeapBegin(void);
void fixtureFlowHeapEqual(void);
#endif
