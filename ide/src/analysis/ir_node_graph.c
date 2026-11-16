#include "analysis/ir_node_graph.h"
#include <xrt.h>
#include <stdint.h>
#include <string.h>

/* The schema guard admits these native aliases as copied integer values.
 * Check their actual compiled types too,so a typedef cannot hide a pointer. */
#define VALUE_ALIAS(T) _Static_assert(_Generic((T){0}, \
    char:1,signed char:1,unsigned char:1,short:1,unsigned short:1, \
    int:1,unsigned int:1,long:1,unsigned long:1, \
    long long:1,unsigned long long:1,default:0),#T " must be an integer value")
VALUE_ALIAS(uint8);VALUE_ALIAS(uint16);VALUE_ALIAS(uint32);VALUE_ALIAS(uint64);
VALUE_ALIAS(int8);VALUE_ALIAS(int16);VALUE_ALIAS(int32);VALUE_ALIAS(int64);
VALUE_ALIAS(XL6_IrNodeId);VALUE_ALIAS(XL6_IrTypeId);VALUE_ALIAS(XL6_SymbolId);
VALUE_ALIAS(XL6_AstKind);VALUE_ALIAS(XL6_SemCallKind);
VALUE_ALIAS(XL6_SemLvalueKind);VALUE_ALIAS(XL6_SemExitKind);
#undef VALUE_ALIAS

typedef struct graph_namespace {xatomic32 references;} graph_namespace;
typedef struct graph_slot {uint32_t owner,local;} graph_slot;
typedef struct graph_counts {size_t nodes,children,payload,usedChildren,usedPayload;} graph_counts;
struct xls_ir_node_unit {
    xatomic32 references;graph_namespace* producer;
    XL6_IrNodeId root;size_t count,childCount,payloadBytes,bytes;
    XL6_IrNode* nodes;XL6_IrNodeId *ids,*children;size_t* first;
    unsigned char* payload;
};
struct xls_ir_node_graph {
    xatomic32 references;graph_namespace* producer;
    size_t unitCount,nodeCount,childCount,bytes,peak;XL6_IrNodeId root;
    xls_ir_node_unit** units;graph_slot* slots;XL6_IrNodeId* children;
};
struct xls_ir_node_image {
    xls_ir_node_graph* graph;size_t nodeCount,childCount,bytes;
    XL6_IrNode *nodes;XL6_IrNodeId* children;
};

static int add(size_t* value,size_t amount)
{if(amount>SIZE_MAX-*value)return 0;*value+=amount;return 1;}
static int region(size_t* bytes,size_t count,size_t width,size_t alignment,size_t* offset)
{
    size_t padding=(alignment-*bytes%alignment)%alignment;
    if(!add(bytes,padding) || count>SIZE_MAX/width)return 0;
    *offset=*bytes;return add(bytes,count*width);
}
static int stringBytes(const char* text,size_t limit,size_t* bytes)
{
    *bytes=0;if(!text)return 1;
    for(size_t i=0;i<limit;++i)if(!text[i]){*bytes=i+1;return 1;}
    return 0;
}
static int payloadBytes(const XL6_IrNode* node,size_t limit,size_t* bytes)
{
    size_t text;*bytes=0;
    if(!stringBytes(node->text,limit,&text) || !add(bytes,text))return 0;
    if(node->literalData && (node->literalSize==SIZE_MAX || !add(bytes,node->literalSize+1)))return 0;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    if(!stringBytes(node->compiledPackageLink,limit,&text) || !add(bytes,text))return 0;
#endif
    return *bytes<=limit;
}
static graph_namespace* namespaceRetain(graph_namespace* value)
{if(value)xrtAtomic32FetchAdd(&value->references,1,XMEMORY_RELAXED);return value;}
static void namespaceRelease(graph_namespace* value)
{if(value && xrtAtomic32FetchSub(&value->references,1,XMEMORY_ACQ_REL)==1)xrtFree(value);}
xls_ir_node_unit* xlsIrNodeUnitRetain(xls_ir_node_unit* unit)
{if(unit)xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED);return unit;}
void xlsIrNodeUnitRelease(xls_ir_node_unit* unit)
{if(unit && xrtAtomic32FetchSub(&unit->references,1,XMEMORY_ACQ_REL)==1){namespaceRelease(unit->producer);xrtFree(unit);}}
xls_ir_node_graph* xlsIrNodeGraphRetain(xls_ir_node_graph* graph)
{if(graph)xrtAtomic32FetchAdd(&graph->references,1,XMEMORY_RELAXED);return graph;}
void xlsIrNodeGraphRelease(xls_ir_node_graph* graph)
{
    if(!graph || xrtAtomic32FetchSub(&graph->references,1,XMEMORY_ACQ_REL)!=1)return;
    for(size_t i=0;i<graph->unitCount;++i)xlsIrNodeUnitRelease(graph->units[i]);
    namespaceRelease(graph->producer);xrtFree(graph);
}
static xls_ir_node_unit* unitCreate(const graph_counts* counts,XL6_IrNodeId root,
    graph_namespace* producer,size_t limit,int* budget)
{
    size_t bytes=sizeof(xls_ir_node_unit),nodes,ids,first,children,payload;
    *budget=1;
    if(!region(&bytes,counts->nodes,sizeof(XL6_IrNode),_Alignof(XL6_IrNode),&nodes) ||
       !region(&bytes,counts->nodes,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&ids) ||
       !region(&bytes,counts->nodes,sizeof(size_t),_Alignof(size_t),&first) ||
       !region(&bytes,counts->children,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&children) ||
       !region(&bytes,counts->payload,1,1,&payload) || bytes>limit)return NULL;
    *budget=0;
    xls_ir_node_unit* unit=xrtMalloc(bytes);if(!unit)return NULL;
    memset(unit,0,sizeof(*unit));xrtAtomic32Init(&unit->references,1);
    unit->producer=namespaceRetain(producer);unit->root=root;unit->count=counts->nodes;
    unit->childCount=counts->children;unit->payloadBytes=counts->payload;unit->bytes=bytes;
    unit->nodes=(XL6_IrNode*)((unsigned char*)unit+nodes);unit->ids=(XL6_IrNodeId*)((unsigned char*)unit+ids);
    unit->first=(size_t*)((unsigned char*)unit+first);unit->children=(XL6_IrNodeId*)((unsigned char*)unit+children);
    unit->payload=(unsigned char*)unit+payload;return unit;
}
static int copyText(xls_ir_node_unit* unit,char** target,const char* source,size_t* used)
{
    size_t bytes;if(!stringBytes(source,unit->payloadBytes-*used,&bytes))return 0;
    *target=NULL;if(!source)return 1;
    *target=(char*)unit->payload+*used;memcpy(*target,source,bytes);*used+=bytes;return 1;
}
static int copyValue(xls_ir_node_unit* unit,size_t local,XL6_IrNodeId id,
    const XL6_IrNode* from,const XL6_IrNodeId* children,graph_counts* counts)
{
    XL6_IrNode* to=&unit->nodes[local];
    memcpy(to,from,sizeof(*to));unit->ids[local]=id;unit->first[local]=counts->usedChildren;
    if(from->childCount>unit->childCount-counts->usedChildren)return 0;
    if(from->childCount)memcpy(unit->children+counts->usedChildren,children,from->childCount*sizeof(*unit->children));
    counts->usedChildren+=from->childCount;
    if(!copyText(unit,&to->text,from->text,&counts->usedPayload))return 0;
    to->literalData=NULL;
    if(from->literalData){
        if(from->literalSize==SIZE_MAX || from->literalSize+1>unit->payloadBytes-counts->usedPayload)return 0;
        to->literalData=unit->payload+counts->usedPayload;
        memcpy(to->literalData,from->literalData,from->literalSize+1);counts->usedPayload+=from->literalSize+1;
    }
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    if(!copyText(unit,&to->compiledPackageLink,from->compiledPackageLink,&counts->usedPayload))return 0;
#endif
    return 1;
}
static int copyNode(xls_ir_node_unit* unit,size_t local,XL6_IrNodeId id,
    const XL6_IrModule* source,graph_counts* counts)
{
    const XL6_IrNode* from=&source->nodes[(size_t)id-1];
    return copyValue(unit,local,id,from,from->childCount ? source->children+from->firstChild : NULL,counts);
}
static size_t entryFind(const xls_ir_node_capture_input* input,XL6_IrNodeId id)
{
    size_t a=0,b=input->count;
    while(a<b){size_t m=a+(b-a)/2;if(input->entries[m].id<id)a=m+1;else b=m;}
    return a<input->count && input->entries[a].id==id ? a : SIZE_MAX;
}
/* XRT's generic array sort delegates to CRT qsort,whose scratch allocation
 * and cancellation are not bounded here. Sort these verification-only IDs
 * in place;each sift has at most32steps in the admitted native ID range. */
static void siftIds(uint32_t* ids,size_t at,size_t count)
{
    uint32_t value=ids[at];
    while(at<count/2){size_t child=at*2+1;if(child+1<count && ids[child]<ids[child+1])++child;
        if(value>=ids[child])break;
        ids[at]=ids[child];at=child;}
    ids[at]=value;
}
static int sortIds(uint32_t* ids,size_t count,int(*cancelled)(void*),void* user)
{
    size_t visits=0;
    for(size_t i=count/2;i;){if(!(visits++%64) && cancelled && cancelled(user))return 0;siftIds(ids,--i,count);}
    for(size_t end=count;end>1;){if(!(visits++%64) && cancelled && cancelled(user))return 0;
        uint32_t value=ids[--end];ids[end]=ids[0];ids[0]=value;siftIds(ids,0,end);}
    return 1;
}
xls_ir_node_unit* xlsIrNodeUnitCapture(const xls_ir_node_capture_input* input,
    const xls_ir_node_unit* cohort,size_t limit,int(*cancelled)(void*),void* user,
    xls_ir_graph_status* status,size_t* peakBytes)
{
    xls_ir_node_unit* unit=NULL;void* scratch=NULL;xls_ir_graph_status error=XLS_IR_GRAPH_INVALID;
    if(peakBytes)*peakBytes=0;
    if(!input || !input->entries || !input->count || !input->nodeLimit || input->nodeLimit>UINT32_MAX || input->count>input->nodeLimit ||
       !input->root || input->root>input->nodeLimit || !input->moduleRoot || input->moduleRoot>input->nodeLimit ||
       (cohort && !cohort->producer))goto failed;
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    if(!limit){error=XLS_IR_GRAPH_BUDGET;goto failed;}
    int module=input->root==input->moduleRoot;graph_counts counts={.nodes=input->count};
    for(size_t i=0;i<input->count;++i){
        if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        const xls_ir_node_capture_entry* e=&input->entries[i];
        if(!e->node || !e->id || e->id>input->nodeLimit || (i && e->id<=input->entries[i-1].id) ||
           (!module && e->id==input->moduleRoot) || (e->node->childCount && !e->children))goto failed;
        error=XLS_IR_GRAPH_BUDGET;size_t payload;
        if(!payloadBytes(e->node,limit,&payload) || !add(&counts.payload,payload) || !add(&counts.children,e->node->childCount))goto failed;
        error=XLS_IR_GRAPH_INVALID;
    }
    size_t root=entryFind(input,input->root);if(root==SIZE_MAX)goto failed;
    const XL6_IrNode* rootNode=input->entries[root].node;
    if(module){if(input->count!=1 || rootNode->kind!=XL6_IR_NODE_MODULE || rootNode->parent || counts.children>=input->nodeLimit)goto failed;}
    else if(rootNode->parent!=input->moduleRoot || counts.children!=input->count-1)goto failed;
    size_t scratchBytes=0,stackOffset=0,marksOffset=0,total,peak;error=XLS_IR_GRAPH_BUDGET;
    if(module){if(!region(&scratchBytes,counts.children,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&stackOffset))goto failed;}
    else if(!region(&scratchBytes,input->count,sizeof(uint32_t),_Alignof(uint32_t),&stackOffset) ||
            !region(&scratchBytes,input->count,1,1,&marksOffset))goto failed;
    total=scratchBytes;if(!add(&total,sizeof(graph_namespace)) || total>limit)goto failed;
    if(scratchBytes){error=XLS_IR_GRAPH_MEMORY;scratch=xrtCalloc(1,scratchBytes);if(!scratch)goto failed;}
    error=XLS_IR_GRAPH_INVALID;
    uint32_t* stack=scratch ? (uint32_t*)((unsigned char*)scratch+stackOffset) : NULL;
    if(module){
        const XL6_IrNodeId* children=input->entries[root].children;
        for(size_t i=0;i<counts.children;++i){
            if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            if(!children[i] || children[i]>input->nodeLimit || children[i]==input->root)goto failed;
            stack[i]=children[i];
        }
        if(!sortIds(stack,counts.children,cancelled,user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        for(size_t i=1;i<counts.children;++i){
            if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            if(stack[i]==stack[i-1])goto failed;
        }
    }else{
        unsigned char* marks=(unsigned char*)scratch+marksOffset;
        for(size_t i=0;i<input->count;++i){
            if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            const xls_ir_node_capture_entry* e=&input->entries[i];
            for(size_t j=0;j<e->node->childCount;++j){
                if(!(j%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
                size_t child=entryFind(input,e->children[j]);
                if(child==SIZE_MAX || child==root || marks[child] || input->entries[child].node->parent!=e->id)goto failed;
                marks[child]=1;
            }
        }
        size_t pending=1,seen=0;stack[0]=(uint32_t)root;
        while(pending){
            if(!(seen%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            size_t local=stack[--pending];if(marks[local]==2 || seen>=input->count)goto failed;marks[local]=2;++seen;
            const xls_ir_node_capture_entry* e=&input->entries[local];
            for(size_t j=0;j<e->node->childCount;++j){
                if(!(j%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
                size_t child=entryFind(input,e->children[j]);if(child==SIZE_MAX || pending>=input->count)goto failed;stack[pending++]=(uint32_t)child;
            }
        }
        if(seen!=input->count)goto failed;
    }
    int budget=0;unit=unitCreate(&counts,input->root,NULL,limit-total,&budget);
    if(!unit){error=budget ? XLS_IR_GRAPH_BUDGET : XLS_IR_GRAPH_MEMORY;goto failed;}
    peak=total;if(!add(&peak,unit->bytes) || peak>limit){error=XLS_IR_GRAPH_BUDGET;goto failed;}
    for(size_t i=0;i<input->count;++i){
        if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        const xls_ir_node_capture_entry* e=&input->entries[i];
        if(!copyValue(unit,i,e->id,e->node,e->children,&counts)){error=XLS_IR_GRAPH_INVALID;goto failed;}
    }
    if(counts.usedChildren!=counts.children || counts.usedPayload!=counts.payload){error=XLS_IR_GRAPH_INVALID;goto failed;}
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    if(cohort)unit->producer=namespaceRetain(cohort->producer);
    else{unit->producer=xrtCalloc(1,sizeof(*unit->producer));if(!unit->producer){error=XLS_IR_GRAPH_MEMORY;goto failed;}xrtAtomic32Init(&unit->producer->references,1);}
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    xrtFree(scratch);if(peakBytes)*peakBytes=peak;if(status)*status=XLS_IR_GRAPH_OK;return unit;
failed:
    xrtFree(scratch);xlsIrNodeUnitRelease(unit);if(status)*status=error;return NULL;
}
xls_ir_node_graph* xlsIrNodeGraphCapture(const XL6_IrModule* source,size_t limit,
    int (*cancelled)(void*),void* user,xls_ir_graph_status* status)
{
    xls_ir_node_graph* graph=NULL;graph_counts* counts=NULL;XL6_IrNodeId* stack=NULL;
    xls_ir_graph_status error=XLS_IR_GRAPH_INVALID;
    if(!source || !source->ready || source->stage<XL6_IR_STAGE_TYPED || !source->nodes ||
       !source->nodeCount || source->nodeCount>UINT32_MAX || source->nodeCount>source->nodeCapacity ||
       !source->root || source->root>source->nodeCount || source->childCount>source->childCapacity ||
       (source->childCount && !source->children))goto failed;
    const XL6_IrNode* root=&source->nodes[(size_t)source->root-1];
    if(root->kind!=XL6_IR_NODE_MODULE || root->parent || root->childCount>=source->nodeCount ||
       root->childCount>=(size_t)INT32_MAX-1 ||
       root->firstChild>source->childCount || root->childCount>source->childCount-root->firstChild)goto failed;
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    size_t units=root->childCount+1,bytes=sizeof(*graph),unitOffset,slotOffset,childOffset;
    size_t scratch=0,ignored,total;
    error=XLS_IR_GRAPH_BUDGET;
    if(!region(&bytes,units,sizeof(xls_ir_node_unit*),_Alignof(xls_ir_node_unit*),&unitOffset) ||
       !region(&bytes,source->nodeCount,sizeof(graph_slot),_Alignof(graph_slot),&slotOffset) ||
       !region(&bytes,source->childCount,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&childOffset) ||
       !region(&scratch,source->nodeCount,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&ignored) ||
       !region(&scratch,units,sizeof(graph_counts),_Alignof(graph_counts),&ignored))goto failed;
    total=bytes;if(!add(&total,scratch) || !add(&total,sizeof(graph_namespace)) || total>limit)goto failed;
    error=XLS_IR_GRAPH_MEMORY;
    graph=xrtCalloc(1,bytes);if(!graph)goto failed;
    xrtAtomic32Init(&graph->references,1);graph->unitCount=units;graph->nodeCount=source->nodeCount;
    graph->childCount=source->childCount;graph->root=source->root;graph->bytes=bytes;
    graph->units=(xls_ir_node_unit**)((unsigned char*)graph+unitOffset);
    graph->slots=(graph_slot*)((unsigned char*)graph+slotOffset);
    graph->children=(XL6_IrNodeId*)((unsigned char*)graph+childOffset);
    if(source->childCount)memcpy(graph->children,source->children,source->childCount*sizeof(*graph->children));
    stack=xrtMalloc(source->nodeCount*sizeof(*stack));counts=xrtCalloc(units,sizeof(*counts));
    graph->producer=xrtCalloc(1,sizeof(*graph->producer));
    if(graph->producer)xrtAtomic32Init(&graph->producer->references,1);
    if(!stack || !counts || !graph->producer)goto failed;
    graph->bytes+=sizeof(*graph->producer);
    size_t pending=1,seen=1,visits=0;stack[0]=source->root;graph->slots[(size_t)source->root-1].owner=1;
    error=XLS_IR_GRAPH_INVALID;
    while(pending){
        if(!(visits++%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        XL6_IrNodeId id=stack[--pending];const XL6_IrNode* node=&source->nodes[(size_t)id-1];
        if(node->firstChild>source->childCount || node->childCount>source->childCount-node->firstChild)goto failed;
        for(size_t i=0;i<node->childCount;++i){
            if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            XL6_IrNodeId child=source->children[node->firstChild+i];
            if(!child || child>source->nodeCount || graph->slots[(size_t)child-1].owner ||
               source->nodes[(size_t)child-1].parent!=id || seen>=source->nodeCount)goto failed;
            graph->slots[(size_t)child-1].owner=id==source->root ? (uint32_t)i+2 : graph->slots[(size_t)id-1].owner;
            stack[pending++]=child;++seen;
        }
    }
    if(seen!=source->nodeCount)goto failed;
    for(size_t i=0;i<source->nodeCount;++i){
        if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        graph_slot* slot=&graph->slots[i];graph_counts* count=&counts[slot->owner-1];size_t payload;
        slot->local=(uint32_t)count->nodes++;
        error=XLS_IR_GRAPH_BUDGET;
        if(!payloadBytes(&source->nodes[i],limit,&payload) || !add(&count->payload,payload) ||
           count->payload>limit || !add(&count->children,source->nodes[i].childCount))goto failed;
    }
    for(size_t i=0;i<units;++i){
        if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        int budget=0;XL6_IrNodeId id=i ? source->children[root->firstChild+i-1] : source->root;
        graph->units[i]=unitCreate(&counts[i],id,graph->producer,limit-total,&budget);
        if(!graph->units[i]){error=budget ? XLS_IR_GRAPH_BUDGET : XLS_IR_GRAPH_MEMORY;goto failed;}
        total+=graph->units[i]->bytes;graph->bytes+=graph->units[i]->bytes;
    }
    for(size_t i=0;i<source->nodeCount;++i){
        if(!(i%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
        const graph_slot* slot=&graph->slots[i];
        if(!copyNode(graph->units[slot->owner-1],slot->local,(XL6_IrNodeId)(i+1),source,&counts[slot->owner-1])){error=XLS_IR_GRAPH_INVALID;goto failed;}
    }
    for(size_t i=0;i<units;++i)if(counts[i].usedChildren!=counts[i].children || counts[i].usedPayload!=counts[i].payload){error=XLS_IR_GRAPH_INVALID;goto failed;}
    graph->peak=total;xrtFree(stack);xrtFree(counts);if(status)*status=XLS_IR_GRAPH_OK;return graph;
failed:
    xrtFree(stack);xrtFree(counts);xlsIrNodeGraphRelease(graph);if(status)*status=error;return NULL;
}
size_t xlsIrNodeGraphBytes(const xls_ir_node_graph* graph){return graph ? graph->bytes : 0;}
size_t xlsIrNodeGraphPeakBytes(const xls_ir_node_graph* graph){return graph ? graph->peak : 0;}
size_t xlsIrNodeGraphUnitCount(const xls_ir_node_graph* graph){return graph ? graph->unitCount : 0;}
size_t xlsIrNodeGraphNodeCount(const xls_ir_node_graph* graph){return graph ? graph->nodeCount : 0;}
XL6_IrNodeId xlsIrNodeGraphRoot(const xls_ir_node_graph* graph){return graph ? graph->root : 0;}
const XL6_IrNode* xlsIrNodeGraphNode(const xls_ir_node_graph* graph,XL6_IrNodeId id)
{
    if(!graph || !id || id>graph->nodeCount)return NULL;
    const graph_slot* slot=&graph->slots[(size_t)id-1];return &graph->units[slot->owner-1]->nodes[slot->local];
}
const XL6_IrNodeId* xlsIrNodeGraphChildren(const xls_ir_node_graph* graph,XL6_IrNodeId id,size_t* count)
{
    if(count)*count=0;
    if(!graph || !id || id>graph->nodeCount)return NULL;
    const graph_slot* slot=&graph->slots[(size_t)id-1];
    return xlsIrNodeUnitChildren(graph->units[slot->owner-1],slot->local,count);
}
xls_ir_node_unit* xlsIrNodeGraphPinUnit(const xls_ir_node_graph* graph,size_t at)
{return graph && at<graph->unitCount ? xlsIrNodeUnitRetain(graph->units[at]) : NULL;}
size_t xlsIrNodeUnitBytes(const xls_ir_node_unit* unit){return unit ? unit->bytes+sizeof(graph_namespace) : 0;}
size_t xlsIrNodeUnitCount(const xls_ir_node_unit* unit){return unit ? unit->count : 0;}
XL6_IrNodeId xlsIrNodeUnitRoot(const xls_ir_node_unit* unit){return unit ? unit->root : 0;}
XL6_IrNodeId xlsIrNodeUnitNativeId(const xls_ir_node_unit* unit,size_t local)
{return unit && local<unit->count ? unit->ids[local] : 0;}
const XL6_IrNode* xlsIrNodeUnitNode(const xls_ir_node_unit* unit,size_t local)
{return unit && local<unit->count ? &unit->nodes[local] : NULL;}
const XL6_IrNodeId* xlsIrNodeUnitChildren(const xls_ir_node_unit* unit,size_t local,size_t* count)
{
    if(count)*count=0;
    if(!unit || local>=unit->count)return NULL;
    if(count)*count=unit->nodes[local].childCount;
    return unit->children+unit->first[local];
}
const void* xlsIrNodeUnitProducer(const xls_ir_node_unit* unit){return unit ? unit->producer : NULL;}
xls_ir_node_image* xlsIrNodeGraphMaterialize(xls_ir_node_graph* graph,size_t limit,
    int (*cancelled)(void*),void* user,xls_ir_graph_status* status)
{
    xls_ir_node_image* image=NULL;xls_ir_graph_status error=XLS_IR_GRAPH_INVALID;
    if(!graph)goto failed;
    if(cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
    size_t bytes=sizeof(*image),nodes,children;error=XLS_IR_GRAPH_BUDGET;
    if(!region(&bytes,graph->nodeCount,sizeof(XL6_IrNode),_Alignof(XL6_IrNode),&nodes) ||
       !region(&bytes,graph->childCount,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&children) || bytes>limit)goto failed;
    error=XLS_IR_GRAPH_MEMORY;image=xrtMalloc(bytes);if(!image)goto failed;
    memset(image,0,sizeof(*image));image->nodes=(XL6_IrNode*)((unsigned char*)image+nodes);
    image->children=(XL6_IrNodeId*)((unsigned char*)image+children);image->bytes=bytes;
    image->nodeCount=graph->nodeCount;image->childCount=graph->childCount;size_t visits=0;
    for(size_t i=0;i<graph->unitCount;++i){
        const xls_ir_node_unit* unit=graph->units[i];
        for(size_t j=0;j<unit->count;++j){
            if(!(visits++%64) && cancelled && cancelled(user)){error=XLS_IR_GRAPH_CANCELLED;goto failed;}
            memcpy(&image->nodes[(size_t)unit->ids[j]-1],&unit->nodes[j],sizeof(*image->nodes));
        }
    }
    if(graph->childCount)memcpy(image->children,graph->children,graph->childCount*sizeof(*image->children));
    image->graph=xlsIrNodeGraphRetain(graph);if(status)*status=XLS_IR_GRAPH_OK;return image;
failed:
    xrtFree(image);if(status)*status=error;return NULL;
}
void xlsIrNodeImageDestroy(xls_ir_node_image* image)
{if(image){xls_ir_node_graph* graph=image->graph;xrtFree(image);xlsIrNodeGraphRelease(graph);}}
const XL6_IrNode* xlsIrNodeImageNodes(const xls_ir_node_image* image,size_t* count)
{if(count)*count=image ? image->nodeCount : 0;return image ? image->nodes : NULL;}
const XL6_IrNodeId* xlsIrNodeImageChildren(const xls_ir_node_image* image,size_t* count)
{if(count)*count=image ? image->childCount : 0;return image ? image->children : NULL;}
size_t xlsIrNodeImageBytes(const xls_ir_node_image* image){return image ? image->bytes : 0;}
