/* A readonly pointer/count pair has the same C ABI for these three exact
 * storage families. Each parameter keeps its own verified preparation ABI. */
static int xl6IrForeignReadonlySpanByteInputCompatible(const XL6_IrModule* m,
    const XL6_IrCallTarget* a,const XL6_IrCallTarget* b,size_t p)
{
    if(p>=a->parameterCount || p>=b->parameterCount ||
        a->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_SPAN_BORROW ||
        b->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_SPAN_BORROW ||
        !xl6IrForeignSpanParameterMatches(m,a,p) || !xl6IrForeignSpanParameterMatches(m,b,p))return 0;
    const XL6_ForeignParameterContract* x=xl6IrForeignParameterGet(m,a,p);const XL6_ForeignParameterContract* y=xl6IrForeignParameterGet(m,b,p);
    const XL6_IrType* left=xl6IrTypeGet(&m->types,a->parameterTypes[p]);const XL6_IrType* right=xl6IrTypeGet(&m->types,b->parameterTypes[p]);
    if(!x || !y || !x->readOnly || !y->readOnly || x->kind!=XL6_FOREIGN_PARAMETER_SPAN || y->kind!=x->kind ||
        strcmp(x->pointerCName,y->pointerCName) || strcmp(x->elementCName,y->elementCName) || strcmp(x->header,y->header) || x->countAbi!=y->countAbi ||
        !left || !right || left->kind==XL6_IR_TYPE_NULLABLE || right->kind==XL6_IR_TYPE_NULLABLE)return 0;
    unsigned lk=left->kind==XL6_IR_TYPE_BUILTIN && left->cAbi==XL6_IR_C_ABI_MANAGED_STRING ? 1:
        left->kind==XL6_IR_TYPE_BUILTIN && left->cAbi==XL6_IR_C_ABI_MANAGED_BYTES ? 2:
        xl6IrForeignSpanBufferField(m,a,p) ? 3:0;
    unsigned rk=right->kind==XL6_IR_TYPE_BUILTIN && right->cAbi==XL6_IR_C_ABI_MANAGED_STRING ? 1:
        right->kind==XL6_IR_TYPE_BUILTIN && right->cAbi==XL6_IR_C_ABI_MANAGED_BYTES ? 2:
        xl6IrForeignSpanBufferField(m,b,p) ? 3:0;
    return lk && rk && lk!=rk;
}
