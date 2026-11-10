/* An implicit field remains receiver-relative even after native semantics
 * records its exact declaration. Other positive targets keep their old reject
 * behavior. The original layout/type/name rules below must also agree with
 * this field identity; this slot check alone does not authorize an ABI. */
static int xlsIrImplicitTargetAllowed(const XL6_IrModule* module,const XL6_IrNode* name)
{
 if(!module || !name)return 0;
 if(!name->targetSymbol)return 1;
 if(name->symbol || !module->symbolNodes || name->targetSymbol>=module->symbolNodeCapacity)return 0;
 const XL6_IrNode* declaration=xl6IrNodeGet(module,module->symbolNodes[name->targetSymbol]);
 return declaration && declaration->kind==XL6_IR_NODE_FIELD_DECL &&
        declaration->symbol==name->targetSymbol;
}
