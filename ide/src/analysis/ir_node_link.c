#include "analysis/ir_node_link.h"
#include <xrt.h>
#include <stdint.h>
#include <string.h>
_Static_assert(sizeof(XL6_IrNodeId)==sizeof(uint32_t),"Audit node ID width");
_Static_assert(sizeof(XL6_IrTypeId)==sizeof(uint32_t),"Audit type ID width");
_Static_assert(sizeof(XL6_SymbolId)==sizeof(uint32_t),"Audit symbol ID width");

typedef struct link_slot {uint32_t binding,local;} link_slot;
struct xls_ir_node_link {
    size_t nodeCount,childCount,bindingCount,bytes,peak;XL6_IrNodeId root;
    XL6_IrNode* nodes;XL6_IrNodeId* children;xls_ir_node_unit** units;
};
typedef struct link_state {
    const xls_ir_node_layout* layout;const xls_ir_node_binding* binding;
    size_t index;XL6_IrNodeId source,destination;xls_ir_link_error* error;
} link_state;
static int add(size_t* value,size_t n)
{if(n>SIZE_MAX-*value)return 0;*value+=n;return 1;}
static int region(size_t* bytes,size_t count,size_t width,size_t align,size_t* at)
{
    if(!add(bytes,(align-*bytes%align)%align) || count>SIZE_MAX/width)return 0;
    *at=*bytes;return add(bytes,count*width);
}
static int fail(link_state* s,xls_ir_link_reason reason,const char* field,xls_ir_reference_domain domain,uint64_t value)
{
    if(s->error)*s->error=(xls_ir_link_error){reason,s->index,s->source,s->destination,domain,value,field};
    return 0;
}
static int reference(link_state* s,xls_ir_reference_domain domain,const char* field,uint64_t value,uint64_t* mapped)
{
    uint64_t sentinel=domain==XLS_IR_REF_AGGREGATE_FIELD ? (uint64_t)SIZE_MAX : 0;
    if(value==sentinel){*mapped=value;return 1;}
    if(!s->binding->map(s->binding->user,domain,value,mapped))return fail(s,XLS_IR_LINK_UNMAPPED,field,domain,value);
    uint64_t limit=domain==XLS_IR_REF_NODE ? s->layout->nodeCount :
                   domain==XLS_IR_REF_TYPE ? s->layout->typeCount :
                   domain==XLS_IR_REF_AGGREGATE_FIELD ? s->layout->aggregateFieldCount : UINT32_MAX;
    if(domain==XLS_IR_REF_AGGREGATE_FIELD){
        if(*mapped>=limit || *mapped>SIZE_MAX)return fail(s,XLS_IR_LINK_RANGE,field,domain,value);
    }else if(!*mapped || *mapped>limit || *mapped>UINT32_MAX)return fail(s,XLS_IR_LINK_RANGE,field,domain,value);
    return 1;
}
static int relocate(link_state* s,const XL6_IrNode* src,XL6_IrNode* dst)
{
    uint64_t mapped;
#define LINK_REFERENCE(field,domain) do{ \
    if(!reference(s,XLS_IR_REF_##domain,#field,(uint64_t)src->field,&mapped))return 0; \
    dst->field=mapped; \
}while(0)
#include "ir-node-reference-schema.inc"
#undef LINK_REFERENCE
    return 1;
}
static int textEqual(const char* a,const char* b)
{return (!a && !b) || (a && b && !strcmp(a,b));}
static int sourceEqual(const XL6_IrNode* a,const XL6_IrNode* b)
{
    if(a->kind!=b->kind || a->sourceKind!=b->sourceKind || a->flags!=b->flags ||
       !textEqual(a->text,b->text) || !textEqual(a->compiledPackageLink,b->compiledPackageLink) ||
       a->compiledPackageError!=b->compiledPackageError || a->compiledPackageRoot!=b->compiledPackageRoot ||
       a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded ||
       !!a->literalData!=!!b->literalData)return 0;
    return !a->literalData || (a->literalSize!=SIZE_MAX && !memcmp(a->literalData,b->literalData,a->literalSize+1));
}
void xlsIrNodeLinkDestroy(xls_ir_node_link* image)
{
    if(!image)return;
    for(size_t i=0;i<image->bindingCount;++i)xlsIrNodeUnitRelease(image->units[i]);
    xrtFree(image);
}
xls_ir_node_link* xlsIrNodeLink(const xls_ir_node_layout* layout,
    const xls_ir_node_binding* bindings,size_t count,size_t limit,
    int(*cancelled)(void*),void* user,xls_ir_graph_status* status,xls_ir_link_error* error)
{
    xls_ir_node_link* image=NULL;void* scratch=NULL;xls_ir_graph_status result=XLS_IR_GRAPH_INVALID;
    link_state state={.layout=layout,.error=error};if(error)*error=(xls_ir_link_error){0};
    if(!layout || !layout->nodes || !layout->nodeCount || layout->nodeCount>UINT32_MAX ||
       !layout->root || layout->root>layout->nodeCount || (layout->childCount && !layout->children) ||
       layout->typeCount>UINT32_MAX || !bindings || !count || count>layout->nodeCount){
        fail(&state,XLS_IR_LINK_INPUT,"layout",XLS_IR_REF_NODE,0);goto failed;
    }
    if(cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
    size_t nodes=0;
    for(size_t i=0;i<count;++i){
        if(!(i%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        state.index=i;const xls_ir_node_binding* b=&bindings[i];
        if(!b->unit || !b->map || !xlsIrNodeUnitCount(b->unit)){fail(&state,XLS_IR_LINK_INPUT,"unit",XLS_IR_REF_NODE,0);goto failed;}
        if(!b->producer || b->producer!=xlsIrNodeUnitProducer(b->unit)){fail(&state,XLS_IR_LINK_NAMESPACE,"producer",XLS_IR_REF_NODE,0);goto failed;}
        if(!add(&nodes,xlsIrNodeUnitCount(b->unit))){result=XLS_IR_GRAPH_BUDGET;goto failed;}
    }
    if(nodes!=layout->nodeCount){fail(&state,XLS_IR_LINK_UNCOVERED,"nodeCount",XLS_IR_REF_NODE,nodes);goto failed;}
    size_t bytes=sizeof(*image),nodeOffset,childOffset,unitOffset,scratchBytes=0,slotOffset,stackOffset,peak;
    result=XLS_IR_GRAPH_BUDGET;
    if(!region(&bytes,layout->nodeCount,sizeof(XL6_IrNode),_Alignof(XL6_IrNode),&nodeOffset) ||
       !region(&bytes,layout->childCount,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&childOffset) ||
       !region(&bytes,count,sizeof(xls_ir_node_unit*),_Alignof(xls_ir_node_unit*),&unitOffset) ||
       !region(&scratchBytes,layout->nodeCount,sizeof(link_slot),_Alignof(link_slot),&slotOffset) ||
       !region(&scratchBytes,layout->nodeCount,sizeof(XL6_IrNodeId),_Alignof(XL6_IrNodeId),&stackOffset))goto failed;
    peak=bytes;if(!add(&peak,scratchBytes) || peak>limit)goto failed;
    result=XLS_IR_GRAPH_MEMORY;image=xrtMalloc(bytes);if(!image)goto failed;
    memset(image,0,sizeof(*image));image->nodes=(XL6_IrNode*)((unsigned char*)image+nodeOffset);
    image->children=(XL6_IrNodeId*)((unsigned char*)image+childOffset);image->units=(xls_ir_node_unit**)((unsigned char*)image+unitOffset);
    image->bytes=bytes;image->peak=peak;image->nodeCount=layout->nodeCount;image->childCount=layout->childCount;image->root=layout->root;
    scratch=xrtCalloc(1,scratchBytes);if(!scratch)goto failed;
    link_slot* slots=(link_slot*)((unsigned char*)scratch+slotOffset);XL6_IrNodeId* stack=(XL6_IrNodeId*)((unsigned char*)scratch+stackOffset);
    result=XLS_IR_GRAPH_INVALID;
    if(layout->nodes[layout->root-1].kind!=XL6_IR_NODE_MODULE || layout->nodes[layout->root-1].parent){
        fail(&state,XLS_IR_LINK_TOPOLOGY,"root",XLS_IR_REF_NODE,layout->root);goto failed;
    }
    size_t pending=1,seen=1,visits=0;stack[0]=layout->root;slots[layout->root-1].local=1;
    while(pending){
        if(!(visits++%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        XL6_IrNodeId id=stack[--pending];const XL6_IrNode* node=&layout->nodes[id-1];
        if(node->firstChild>layout->childCount || node->childCount>layout->childCount-node->firstChild){
            fail(&state,XLS_IR_LINK_TOPOLOGY,"childSpan",XLS_IR_REF_NODE,id);goto failed;
        }
        for(size_t j=0;j<node->childCount;++j){
            if(!(j%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
            XL6_IrNodeId child=layout->children[node->firstChild+j];
            if(!child || child>layout->nodeCount || slots[child-1].local || layout->nodes[child-1].parent!=id || seen>=layout->nodeCount){
                fail(&state,XLS_IR_LINK_TOPOLOGY,"children",XLS_IR_REF_NODE,child);goto failed;
            }
            slots[child-1].local=1;stack[pending++]=child;++seen;
        }
    }
    if(seen!=layout->nodeCount){fail(&state,XLS_IR_LINK_TOPOLOGY,"unreachable",XLS_IR_REF_NODE,seen);goto failed;}
    visits=0;
    for(size_t i=0;i<count;++i){
        state.binding=&bindings[i];state.index=i;const xls_ir_node_unit* u=bindings[i].unit;
        for(size_t j=0;j<xlsIrNodeUnitCount(u);++j){
            if(!(visits++%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
            state.source=xlsIrNodeUnitNativeId(u,j);state.destination=0;uint64_t id;
            if(!reference(&state,XLS_IR_REF_NODE,"nodeId",state.source,&id))goto failed;
            if(!id){fail(&state,XLS_IR_LINK_RANGE,"nodeId",XLS_IR_REF_NODE,state.source);goto failed;}
            state.destination=(XL6_IrNodeId)id;
            if(slots[id-1].binding){fail(&state,XLS_IR_LINK_DUPLICATE,"nodeId",XLS_IR_REF_NODE,state.source);goto failed;}
            slots[id-1]=(link_slot){(uint32_t)i+1,(uint32_t)j};
        }
    }
    for(size_t i=0;i<layout->nodeCount;++i){
        if(!(i%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
        if(!slots[i].binding){fail(&state,XLS_IR_LINK_UNCOVERED,"nodeId",XLS_IR_REF_NODE,i+1);goto failed;}
        size_t binding=slots[i].binding-1;const xls_ir_node_unit* unit=bindings[binding].unit;size_t local=slots[i].local;
        state.binding=&bindings[binding];state.index=binding;state.source=xlsIrNodeUnitNativeId(unit,local);state.destination=(XL6_IrNodeId)(i+1);
        const XL6_IrNode* src=xlsIrNodeUnitNode(unit,local);const XL6_IrNode* current=&layout->nodes[i];XL6_IrNode* dst=&image->nodes[i];
        if(!src || !sourceEqual(src,current)){fail(&state,XLS_IR_LINK_SOURCE,"source",XLS_IR_REF_NODE,state.source);goto failed;}
        memcpy(dst,src,sizeof(*dst));if(!relocate(&state,src,dst))goto failed;
        size_t children;const XL6_IrNodeId* list=xlsIrNodeUnitChildren(unit,local,&children);
        if(dst->parent!=current->parent || children!=current->childCount){fail(&state,XLS_IR_LINK_TOPOLOGY,"parent/arity",XLS_IR_REF_NODE,state.source);goto failed;}
        for(size_t j=0;j<children;++j){
            if(!(j%64) && cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
            uint64_t child;if(!reference(&state,XLS_IR_REF_NODE,"child",list[j],&child))goto failed;
            if(child!=layout->children[current->firstChild+j]){fail(&state,XLS_IR_LINK_TOPOLOGY,"child",XLS_IR_REF_NODE,list[j]);goto failed;}
        }
        dst->firstChild=current->firstChild;dst->childCount=current->childCount;dst->span=current->span;
    }
    if(layout->childCount)memcpy(image->children,layout->children,layout->childCount*sizeof(*image->children));
    if(cancelled && cancelled(user)){result=XLS_IR_GRAPH_CANCELLED;goto failed;}
    for(size_t i=0;i<count;++i)image->units[i]=xlsIrNodeUnitRetain(bindings[i].unit);
    image->bindingCount=count;xrtFree(scratch);if(status){*status=XLS_IR_GRAPH_OK;}return image;
failed:
    xrtFree(scratch);xlsIrNodeLinkDestroy(image);if(status)*status=result;return NULL;
}
const XL6_IrNode* xlsIrNodeLinkNodes(const xls_ir_node_link* image,size_t* count)
{if(count)*count=image ? image->nodeCount : 0;return image ? image->nodes : NULL;}
const XL6_IrNodeId* xlsIrNodeLinkChildren(const xls_ir_node_link* image,size_t* count)
{if(count)*count=image ? image->childCount : 0;return image ? image->children : NULL;}
XL6_IrNodeId xlsIrNodeLinkRoot(const xls_ir_node_link* image){return image ? image->root : 0;}
size_t xlsIrNodeLinkBytes(const xls_ir_node_link* image){return image ? image->bytes : 0;}
size_t xlsIrNodeLinkPeakBytes(const xls_ir_node_link* image){return image ? image->peak : 0;}
