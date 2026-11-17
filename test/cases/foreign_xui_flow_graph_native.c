#ifndef XRT_MODULE_ALL
#define XRT_MODULE_ALL
#endif
#include "foreign_xui_flow_graph.h"
#include <assert.h>
#include <string.h>
static xmemdebugsnapshot warm;
size_t flowModelReferences(xui_flow_graph graph)
{
    xrtownershipref ref=xuiFlowGraphOwnership(graph);xrtownershipscope freeze={0};size_t count=0;
    assert(ref.Data && xrtOwnershipFreezeTryBegin(&freeze));
    assert(ref.Ops->Count(ref.Data,&count));assert(xrtOwnershipScopeEnd(&freeze));return count;
}
void flowModelExpectId(const char* text,size_t size,int kind)
{
    static const char* prefixes[]={"node_","edge_","port_"};assert(kind>=0 && kind<3 && size==64);
    size_t prefix=strlen(prefixes[kind]);assert(!memcmp(text,prefixes[kind],prefix));
    assert(text[prefix]>='1' && text[prefix]<='9' && memchr(text,0,size));
}
void flowModelWarmBegin(void)
{assert(xrtMemDebugEnabled());xrtMemDebugSnapshot(&warm);}
void flowModelWarmEnd(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.AllocCount==warm.AllocCount && after.ReallocCount==warm.ReallocCount);}
void flowModelWarmNative(xui_flow_graph graph)
{for(int i=0;i<1000;++i)assert(xuiFlowGraphGetNodeCount(graph)==2);}
void flowModelBudget(int budget)
{assert(budget>=0 && xrtMemDebugFailAfter((uint64)budget));}
void flowModelResetBudget(void)
{xrtMemDebugFailClear();xrtClearError();}
int flowModelBudgetTriggered(void)
{return xrtMemDebugFailTriggered() ? 1:0;}
size_t flowModelLiveCount(void)
{xmemdebugsnapshot snapshot;xrtMemDebugSnapshot(&snapshot);return snapshot.LiveCount;}

typedef struct FlowModelPayload {FlowModelProbe probe;void* user;int copy;} FlowModelPayload;
static bool flowModelClone(void* raw,void** output,void* user)
{
    FlowModelPayload* source=raw;FlowModelPayload* copy;(void)user;*output=NULL;
    assert(source->probe(1,source->user)==0);
    copy=xrtMalloc(sizeof(*copy));if(!copy)return false;
    *copy=*source;copy->copy=1;*output=copy;return true;
}
static void flowModelDrop(void* raw,void* user)
{
    FlowModelPayload* value=raw;(void)user;
    if(value->copy){assert(value->probe(2,value->user)==0);xrtFree(value);}
}
int flowModelConfigReentry(xui_flow_graph graph,FlowModelProbe probe,void* user)
{
    static const xvaluehandleops ops={flowModelClone,flowModelDrop,NULL,NULL};
    FlowModelPayload source={probe,user,0};void* raw=&source;
    xvalue* value=xrtValueHandleTake(&raw,&ops,NULL);assert(value && !raw);
    int status=xuiFlowGraphSetNodeConfig(graph,"a",value);
    xrtValueRelease(value);
    if(status==XUI_OK)status=xuiFlowGraphSetNodeConfig(graph,"a",NULL);
    assert(!xrtGetError());return status;
}
