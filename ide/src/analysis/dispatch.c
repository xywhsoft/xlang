#include "analysis/dispatch.h"
#include "analysis/frontend_internal.h"
int xlsSemanticCheckDispatchInfo(const xls_semantic_check* check,xls_dispatch_info* out)
{
 if(out)*out=(xls_dispatch_info){0};
 const XL6_IrModule* ir=xlsSemanticCheckIr(check);
 if(!ir || !out || ir->classDispatchImplCount>SIZE_MAX-ir->protocolDispatchImplCount ||
    (ir->classDispatchImplCount && !ir->classDispatchImpls) || (ir->protocolDispatchImplCount && !ir->protocolDispatchImpls))return 0;
 *out=(xls_dispatch_info){ir->classDispatchImplCount,ir->protocolDispatchImplCount};return 1;
}
static int dispatchSymbols(const XL6_IrModule* ir,XL6_IrNodeId a,uint32_t as,XL6_IrNodeId b,uint32_t bs)
{return a && a<=ir->nodeCount && b && b<=ir->nodeCount && ir->nodes && as && bs && ir->nodes[a-1].symbol==as && ir->nodes[b-1].symbol==bs;}
int xlsSemanticCheckDispatchRelation(const xls_semantic_check* check,size_t at,xls_dispatch_relation* out)
{
 if(out)*out=(xls_dispatch_relation){0};
 xls_dispatch_info info;if(!out || !xlsSemanticCheckDispatchInfo(check,&info) || at>=info.classes+info.protocols)return 0;
 const XL6_IrModule* ir=xlsSemanticCheckIr(check);if(!ir)return 0;
 if(at<info.classes){const XL6_IrClassDispatchImpl* row=ir->classDispatchImpls+at;
  if(row->slot>=ir->classDispatchSlotCount || !ir->classDispatchSlots)return 0;
  const XL6_IrClassDispatchSlot* slot=ir->classDispatchSlots+row->slot;
  if(!dispatchSymbols(ir,slot->declaration,slot->declarationSymbol,row->implementationDeclaration,row->implementationSymbol))return 0;
  *out=(xls_dispatch_relation){slot->declarationSymbol,row->implementationSymbol,row->ownerType,slot->ownerType,row->slot,XLS_DISPATCH_CLASS};
 }else{const XL6_IrProtocolDispatchImpl* row=ir->protocolDispatchImpls+at-info.classes;
  if(row->slot>=ir->protocolDispatchSlotCount || !ir->protocolDispatchSlots)return 0;
  const XL6_IrProtocolDispatchSlot* slot=ir->protocolDispatchSlots+row->slot;
  if(!dispatchSymbols(ir,slot->declaration,slot->declarationSymbol,row->implementationDeclaration,row->implementationSymbol))return 0;
  *out=(xls_dispatch_relation){slot->declarationSymbol,row->implementationSymbol,row->ownerType,slot->protocolType,row->slot,XLS_DISPATCH_PROTOCOL};
 }
 return 1;
}
