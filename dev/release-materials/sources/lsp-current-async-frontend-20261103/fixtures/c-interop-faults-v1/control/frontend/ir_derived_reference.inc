static int xlsDerivedReferenceOPERATOR(XL6_IrModule* module)
{
    size_t i;
    if ( module == NULL ) return 0;
    for ( i = 0; i < module->nodeCount; ++i ) {
        module->nodes[i].operatorErrorAbi =
            xl6IrClassifyOperatorErrorAbi(module, &module->nodes[i]);
        module->nodes[i].operatorExecutionType =
            xl6IrClassifyOperatorExecutionType(module, &module->nodes[i]);
        module->nodes[i].operatorOperandAbis[0] =
            xl6IrClassifyOperatorOperandAbi(module, &module->nodes[i], 0);
        module->nodes[i].operatorOperandAbis[1] =
            xl6IrClassifyOperatorOperandAbi(module, &module->nodes[i], 1);
    }
    return 1;
}
static int xlsDerivedReferenceINDEX(XL6_IrModule* module)
{
    size_t i;
    if ( module == NULL ) return 0;
    for ( i = 0; i < module->nodeCount; ++i ) {
        module->nodes[i].indexReceiverGate = xl6IrClassifyIndexReceiverGate(
            module, &module->nodes[i]);
        module->nodes[i].indexWriteAbi = xl6IrClassifyIndexWriteAbi(
            module, &module->nodes[i]);
    }
    for ( i = 0; i < module->nodeCount; ++i )
        module->nodes[i].indexWriteErrorAbi =
            xl6IrClassifyIndexWriteErrorAbi(module, &module->nodes[i]);
    return 1;
}
static int xlsDerivedReferenceNAMED(XL6_IrModule* module)
{
    size_t i;
    if ( module == NULL ) return 0;
    for ( i = 0; i < module->nodeCount; ++i ) {
        XL6_IrNode* node = &module->nodes[i];
        XL6_IrNodeId declarationId = xl6IrNamedCallableDeclaration(
            module, (XL6_IrNodeId)(i + 1));
        const XL6_IrNode* declaration = xl6IrNodeGet(module, declarationId);
        const XL6_IrNode* parameters;
        const XL6_IrNode* returns;
        XL6_IrCallableErasureAbi erasure;
        XL6_IrTypeId signature = XL6_IR_TYPE_INVALID;
        node->namedCallableAbi = XL6_IR_NAMED_CALLABLE_NONE;
        node->namedCallableTargetSymbol = 0;
        node->namedCallableSignatureType = XL6_IR_TYPE_INVALID;
        if ( declaration == NULL ) continue;
        if ( !xl6IrNamedCallableSignatureParts(
                 module, declarationId, &parameters, &returns, &erasure) )
            return 0;
        if ( xl6IrNamedCallableSignatureMatches(
                 module, node->storageType, parameters, returns, erasure, node) )
            signature = node->storageType;
        else if ( xl6IrNamedCallableSignatureMatches(
                      module, node->type, parameters, returns, erasure, node) )
            signature = node->type;
        else if (node->sourceKind != XL6_AST_GENERIC_INSTANT)
            signature = xl6IrInternNamedCallableSignature(
                module, parameters, returns, erasure);
        node = &module->nodes[i];
        if ( signature == XL6_IR_TYPE_INVALID ) return 0;
        node->namedCallableAbi = XL6_IR_NAMED_CALLABLE_CAPTURE_FREE_ADAPTER;
        node->namedCallableTargetSymbol = declaration->symbol;
        node->namedCallableSignatureType = signature;
        /* A discarded function name may carry no frontend expression type.
         * Its resolved callable signature still defines a complete owned value,
         * just as it does at an assignment or return boundary. */
        if ( node->type == XL6_IR_TYPE_INVALID ) node->type = signature;
        if ( node->storageType == XL6_IR_TYPE_INVALID ) node->storageType = signature;
    }
    return 1;
}