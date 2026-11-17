#ifndef FOREIGN_XUI_FLOW_GRAPH_TEST_H
#define FOREIGN_XUI_FLOW_GRAPH_TEST_H
#include <stddef.h>
#include <xge3_runtime.h>
size_t flowModelReferences(xui_flow_graph);
void flowModelExpectId(const char*,size_t,int);
void flowModelWarmBegin(void);
void flowModelWarmEnd(void);
void flowModelWarmNative(xui_flow_graph);
void flowModelBudget(int);
void flowModelResetBudget(void);
int flowModelBudgetTriggered(void);
size_t flowModelLiveCount(void);
typedef int (*FlowModelProbe)(int,void*);
int flowModelConfigReentry(xui_flow_graph,FlowModelProbe,void*);
#endif
