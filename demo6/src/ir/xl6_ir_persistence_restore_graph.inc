/* Included by xl6_ir.c: canonical lowered restore graph construction.
 * Breadth-first interning makes recursive schemas finite without using the
 * compiler's C stack. A node is a static recipe domain, not a runtime alias or
 * proof that two private initialized children are the same object. */
typedef struct XL6_IrRestoreGraphWalk {
    const XL6_IrModule* source;
    const XL6_IrModule* recipes;
    XL6_IrModule* output;
    XL6_IrPersistenceRestoreRoot root;
} XL6_IrRestoreGraphWalk;

static int xl6IrRestoreGraphTablesValid(const XL6_IrModule* source,
    const XL6_IrModule* recipes)
{
    if (!source || !recipes || source->types.count > source->types.capacity ||
        (source->types.count && !source->types.items)) return 0;
#define XL6_RESTORE_TABLE(M, Name) \
    if ((M)->Name##Count > (M)->Name##Capacity || \
        ((M)->Name##Count && !(M)->Name##s)) return 0
    XL6_RESTORE_TABLE(source, function);
    XL6_RESTORE_TABLE(source, instruction);
    XL6_RESTORE_TABLE(source, classLayout);
    XL6_RESTORE_TABLE(source, classField);
    XL6_RESTORE_TABLE(source, callTarget);
    XL6_RESTORE_TABLE(recipes, persistenceRestorePlan);
    XL6_RESTORE_TABLE(recipes, persistenceRestoreSchema);
    XL6_RESTORE_TABLE(recipes, persistenceRestoreField);
    XL6_RESTORE_TABLE(recipes, persistenceRestoreClaim);
#undef XL6_RESTORE_TABLE
    return 1;
}

/* Select an existing language contract, never infer cloneability from fields.
 * Shared defaults cannot use NONCOPYABLE_NULL as a successful isolation. The
 * ordinary language fallback from clone to an explicitly declared copy stays
 * part of ClassLayout; a shallow result may require further child isolation. */
static int xl6IrRestoreGraphIsolation(const XL6_IrModule* source,
    const XL6_IrClassLayout* layout, XL6_IrPersistenceRestoreNode* row)
{
    const XL6_IrCallTarget* target;
    const XL6_IrFunction* function = NULL;
    size_t i;
    row->cloneErrorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
    switch (layout->cloneAbi) {
    case XL6_IR_CLASS_COPY_ABI_NONE:
    case XL6_IR_CLASS_COPY_ABI_NONCOPYABLE_NULL:
        if (layout->cloneTarget) return 0;
        row->isolationAbi = XL6_IR_PERSIST_ISOLATION_CHECKED_REJECT; return 1;
    case XL6_IR_CLASS_COPY_ABI_RUNTIME_SHALLOW:
    case XL6_IR_CLASS_COPY_ABI_RUNTIME_DEEP:
        if (layout->cloneTarget) return 0;
        row->isolationAbi = layout->cloneAbi == XL6_IR_CLASS_COPY_ABI_RUNTIME_SHALLOW
            ? XL6_IR_PERSIST_ISOLATION_RUNTIME_SHALLOW : XL6_IR_PERSIST_ISOLATION_RUNTIME_DEEP;
        return 1;
    case XL6_IR_CLASS_COPY_ABI_INTERNAL_CALL:
        target = xl6IrCallTargetGet(source, layout->cloneTarget);
        if (!target || target->kind != XL6_IR_CALL_TARGET_INTERNAL || !target->symbol ||
            target->returnType != row->payloadType || target->parameterCount != 1 ||
            !target->parameterTypes || !target->parameterAbis ||
            !target->parameterPhysicalAbis ||
            target->parameterTypes[0] != row->payloadType ||
            target->parameterAbis[0] != XL6_IR_PARAMETER_ABI_BORROW ||
            target->parameterPhysicalAbis[0] != XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ||
            target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
            target->resultConversionAbi != XL6_IR_CALL_RESULT_CONVERSION_DIRECT ||
            target->resultCAbi != XL6_IR_CALL_RESULT_C_ABI_STORAGE ||
            target->errorPhysicalAbi != (target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
                ? XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER : XL6_IR_CALL_ERROR_PHYSICAL_NONE) ||
            (target->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
             target->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE)) return 0;
        for (i = 0; i < source->functionCount; ++i) {
            const XL6_IrFunction* candidate = &source->functions[i];
            if (candidate->symbol != target->symbol) continue;
            if (function || i >= UINT32_MAX || candidate->kind != XL6_IR_FUNCTION_USER ||
                candidate->declaration != target->declaration ||
                candidate->returnType != row->payloadType || candidate->parameterCount != 1 ||
                candidate->errorAbi != target->errorAbi) return 0;
            function = candidate; row->cloneFunction = (uint32)(i + 1);
        }
        if (!function) return 0;
        row->isolationAbi = XL6_IR_PERSIST_ISOLATION_INTERNAL_CALL;
        row->cloneSymbol = target->symbol; row->cloneErrorAbi = target->errorAbi; return 1;
    default: return 0;
    }
}

static int xl6IrRestoreGraphIntern(XL6_IrRestoreGraphWalk* walk,
    XL6_IrTypeId type, size_t contextField, size_t* local)
{
    XL6_IrModule* output = walk->output;
    XL6_IrPersistenceRestoreNode row;
    size_t i;
    if (!xl6IrTypeGet(&walk->source->types, type) ||
        (contextField != SIZE_MAX && contextField >= walk->source->classFieldCount)) return 0;
    for (i = walk->root.firstNode; i < output->persistenceRestoreNodeCount; ++i) {
        const XL6_IrPersistenceRestoreNode* node = &output->persistenceRestoreNodes[i];
        if (node->type == type && node->contextField == contextField) {
            *local = i - walk->root.firstNode; return 1;
        }
    }
    i = output->persistenceRestoreNodeCount;
    if (i == SIZE_MAX || !xl6IrGrow((void**)&output->persistenceRestoreNodes,
        &output->persistenceRestoreNodeCapacity, i, sizeof(row))) return 0;
    memset(&row, 0, sizeof(row)); row.type = type; row.contextField = contextField;
    row.seed = row.element = row.schema = row.firstEdge = SIZE_MAX;
    row.openClassLayout=SIZE_MAX;
    row.defaultErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
    row.cloneErrorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
    output->persistenceRestoreNodes[i] = row;
    output->persistenceRestoreNodeCount = i + 1;
    *local = i - walk->root.firstNode; return 1;
}

static int xl6IrRestoreGraphSchema(const XL6_IrRestoreGraphWalk* walk,
    XL6_IrTypeId type, size_t* index)
{
    const XL6_IrModule* recipes = walk->recipes;
    size_t p, s;
    for (p = 0; p < recipes->persistenceRestorePlanCount; ++p) {
        const XL6_IrPersistenceRestorePlan* plan = &recipes->persistenceRestorePlans[p];
        if (plan->format != walk->root.format) continue;
        if (plan->firstSchema > recipes->persistenceRestoreSchemaCount ||
            plan->schemaCount > recipes->persistenceRestoreSchemaCount - plan->firstSchema) return 0;
        for (s = 0; s < plan->schemaCount; ++s) {
            size_t at = plan->firstSchema + s;
            const XL6_IrPersistenceRestoreSchema* schema = &recipes->persistenceRestoreSchemas[at];
            if (schema->classLayout >= walk->source->classLayoutCount) return 0;
            if (walk->source->classLayouts[schema->classLayout].type == type) {
                *index = at; return 1;
            }
        }
    }
    return 0;
}

static int xl6IrRestoreGraphCompleteNode(XL6_IrRestoreGraphWalk* walk, size_t at)
{
    const XL6_IrModule* source = walk->source;
    XL6_IrModule* output = walk->output;
    XL6_IrPersistenceRestoreNode row = output->persistenceRestoreNodes[at];
    const XL6_IrType* type = xl6IrTypeGet(&source->types, row.type);
    XL6_IrTypeId payload = row.type;
    size_t depth = 0;
    while (type && type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_NULLABLE) {
        if (++depth > source->types.count) return 0;
        row.flags |= XL6_IR_RESTORE_NODE_NULLABLE;
        payload = type->persistencePayloadType;
        type = xl6IrTypeGet(&source->types, payload);
    }
    if (!type) return 0;
    row.payloadType = payload; row.identity = type->typeValueId;
    switch (type->persistenceValueAbi) {
    case XL6_IR_PERSISTENCE_VALUE_XRT_CLONE:
        row.abi = XL6_IR_RESTORE_NODE_SCALAR;
        row.scalarRule = type->persistenceRestoreScalarAbi;
        if (row.scalarRule == XL6_VALUE_ADMIT_NONE) return 0;
        if (row.scalarRule == XL6_VALUE_ADMIT_ANY) row.identity = 0;
        if (walk->root.format == XL6_IR_PERSIST_FORMAT_JSON &&
            (row.scalarRule == XL6_VALUE_ADMIT_BYTES || row.scalarRule == XL6_VALUE_ADMIT_TIME))
            walk->root.pending |= XL6_IR_RESTORE_PENDING_JSON_DOMAIN;
        break;
    case XL6_IR_PERSISTENCE_VALUE_ARRAY:
    case XL6_IR_PERSISTENCE_VALUE_LIST_SEQUENCE:
    case XL6_IR_PERSISTENCE_VALUE_SET_SEQUENCE:
    case XL6_IR_PERSISTENCE_VALUE_DICT_OBJECT:
        if (!xl6IrRestoreGraphIntern(walk, type->persistencePayloadType, SIZE_MAX, &row.element)) return 0;
        row.carrierAbi = type->collectionCarrierAbi;
        if (type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_ARRAY) {
            row.abi = XL6_IR_RESTORE_NODE_ARRAY;
            row.carrierAbi = type->persistenceRestoreCarrierAbi;
            if (row.carrierAbi == XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1) {
                if (!xl6IrSequenceConstructionRecipe(source, payload, XL6_IR_SEQUENCE_ROLE_ARRAY, NULL) ||
                    walk->root.seedCount == SIZE_MAX) return 0;
                row.seed = walk->root.seedCount++;
            } else walk->root.pending |= XL6_IR_RESTORE_PENDING_ARRAY_VALUE;
        } else if (type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_LIST_SEQUENCE) {
            row.abi = XL6_IR_RESTORE_NODE_LIST; walk->root.pending |= XL6_IR_RESTORE_PENDING_LIST;
        } else if (type->persistenceValueAbi == XL6_IR_PERSISTENCE_VALUE_SET_SEQUENCE) {
            row.abi = XL6_IR_RESTORE_NODE_SET; walk->root.pending |= XL6_IR_RESTORE_PENDING_SET;
        } else {
            row.abi = XL6_IR_RESTORE_NODE_DICT; walk->root.pending |= XL6_IR_RESTORE_PENDING_DICT;
        }
        break;
    case XL6_IR_PERSISTENCE_VALUE_CLASS_SCHEMA: {
        const XL6_IrPersistenceRestoreSchema* schema;
        const XL6_IrClassLayout* layout;
        size_t i, first;
        row.abi = XL6_IR_RESTORE_NODE_CLASS;
        if (!xl6IrRestoreGraphSchema(walk, payload, &row.schema)) return 0;
        schema = &walk->recipes->persistenceRestoreSchemas[row.schema];
        layout = &source->classLayouts[schema->classLayout];
        if (!xl6IrRestoreGraphIsolation(source, layout, &row)) return 0;
        if (schema->firstField > walk->recipes->persistenceRestoreFieldCount ||
            schema->fieldCount > walk->recipes->persistenceRestoreFieldCount - schema->firstField) return 0;
        if (schema->dynamicField != XL6_IR_CLASS_FIELD_INDEX_INVALID) {
            if (schema->dynamicField>=source->classFieldCount || schema->dynamicHandleField>=source->classFieldCount)
                return 0;
            const XL6_IrCallTarget* destroy=xl6IrCallTargetGet(source,schema->dynamicDestroySymbol);
            if (!destroy || destroy->nativeHandlePolicyAbi!=XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1 ||
                !destroy->nativeHandlePolicyCName ||
                strcmp(destroy->nativeHandlePolicyCName,"xl6DynamicFieldsHandlePolicyV1Get")) return 0;
            XL6_IrTypeId storage=source->classFields[schema->dynamicField].type;
            size_t open=0;
            while (open<source->classLayoutCount && source->classLayouts[open].type!=storage) ++open;
            if (open==source->classLayoutCount) return 0;
            XL6_IrPersistenceRestoreNode isolation={0}; isolation.payloadType=storage;
            if (!xl6IrRestoreGraphIsolation(source,&source->classLayouts[open],&isolation)) return 0;
            row.openClassLayout=open; row.openIsolationAbi=isolation.isolationAbi;
            row.openCloneFunction=isolation.cloneFunction; row.openCloneSymbol=isolation.cloneSymbol;
            row.openCloneErrorAbi=isolation.cloneErrorAbi;
        }
        if (row.contextField != SIZE_MAX) row.flags |= XL6_IR_RESTORE_NODE_FIELD_INSTANCE;
        else {
            if (layout->persistenceConstructionAbi != XL6_IR_PERSISTENCE_CONSTRUCTION_DECLARATION_DEFAULT)
                walk->root.pending |= XL6_IR_RESTORE_PENDING_FACTORY;
            else {
                const XL6_IrFunction* factory;
                if (!layout->persistenceDefaultFunction || layout->persistenceDefaultFunction > source->functionCount)
                    return 0;
                factory = &source->functions[layout->persistenceDefaultFunction - 1];
                if (factory->kind != XL6_IR_FUNCTION_CLASS_DEFAULT || !factory->symbol ||
                    factory->returnType != payload || factory->declaration != layout->declaration ||
                    factory->parameterCount || factory->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) return 0;
                row.defaultFunction = layout->persistenceDefaultFunction;
                row.defaultSymbol = factory->symbol; row.defaultErrorAbi = factory->errorAbi;
            }
        }
        first = output->persistenceRestoreEdgeCount;
        if (schema->fieldCount > SIZE_MAX - first) return 0;
        row.firstEdge = first - walk->root.firstEdge; row.edgeCount = schema->fieldCount;
        for (i = 0; i < schema->fieldCount; ++i)
            if (!xl6IrGrow((void**)&output->persistenceRestoreEdges,
                &output->persistenceRestoreEdgeCapacity, first + i,
                sizeof(XL6_IrPersistenceRestoreEdge))) return 0;
        output->persistenceRestoreEdgeCount = first + schema->fieldCount;
        for (i = 0; i < schema->fieldCount; ++i) {
            size_t index = schema->firstField + i, node;
            const XL6_IrPersistenceRestoreField* field = &walk->recipes->persistenceRestoreFields[index];
            size_t context = field->assignAbi == XL6_IR_PERSIST_RESTORE_CLASS_IN_PLACE
                ? field->classField : SIZE_MAX;
            if (field->classField >= source->classFieldCount ||
                !xl6IrRestoreGraphIntern(walk, field->payloadType, context, &node)) return 0;
            output->persistenceRestoreEdges[first + i].field = index;
            output->persistenceRestoreEdges[first + i].node = node;
        }
        break;
    }
    default: return 0;
    }
    output->persistenceRestoreNodes[at] = row; return 1;
}

/* Whole-module admission is not a property of the wire target alone. A class
 * lease traces its module's actual globals, including unrelated Open/native
 * objects. An opaque generated handle cannot safely expose those edges under
 * Freeze. Derive this conservative cutover barrier from reachable physical
 * ownership operations, and independently recompute it during verification.
 * This does not certify every resident family: runtime admission must still
 * reject any unknown transitive policy before invoking Count/Trace. */
static int xl6IrRestoreModuleOwnerPending(const XL6_IrModule* source,uint32* pending)
{
    *pending = 0;
    for (size_t f=0;f<source->functionCount;++f) {
        const XL6_IrFunction* function=&source->functions[f];
        if (function->firstInstruction>source->instructionCount ||
            function->instructionCount>source->instructionCount-function->firstInstruction) return 0;
        if (!xl6IrPersistenceProjectFunctionUsed(source,function)) continue;
        for (size_t i=0;i<function->instructionCount;++i) {
            const XL6_IrInstruction* instruction=&source->instructions[function->firstInstruction+i];
            if (instruction->kind!=XL6_IR_INSTR_RUNTIME_CALL ||
                instruction->runtimeOp!=XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE) continue;
            const XL6_IrCallTarget* destroy=xl6IrCallTargetGet(source,instruction->targetSymbol);
            if (!destroy || destroy->kind!=XL6_IR_CALL_TARGET_EXTERN_C) return 0;
            if (destroy->nativeHandlePolicyAbi==XL6_IR_NATIVE_HANDLE_POLICY_NONE)
                *pending |= XL6_IR_RESTORE_PENDING_OPAQUE_MODULE_OWNER;
        }
    }
    return 1;
}

static int xl6IrBuildPersistenceRestoreGraphs(const XL6_IrModule* source,
    const XL6_IrModule* recipes, XL6_IrModule* output)
{
    uint32 format, modulePending;
    size_t f, i, r;
    if (!output || !xl6IrRestoreGraphTablesValid(source, recipes)) return 0;
    if (!xl6IrRestoreModuleOwnerPending(source,&modulePending)) return 0;
    for (format = XL6_IR_PERSIST_FORMAT_JSON; format <= XL6_IR_PERSIST_FORMAT_XLON; format <<= 1) {
        for (f = 0; f < source->functionCount; ++f) {
            const XL6_IrFunction* function = &source->functions[f];
            if (function->firstInstruction > source->instructionCount ||
                function->instructionCount > source->instructionCount - function->firstInstruction) return 0;
            if (!xl6IrPersistenceProjectFunctionUsed(source, function)) continue;
            for (i = 0; i < function->instructionCount; ++i) {
                const XL6_IrInstruction* instruction = &source->instructions[function->firstInstruction + i];
                XL6_IrRestoreGraphWalk walk;
                size_t at;
                if (xl6IrPersistenceRestoreOperationFormat(instruction) != format) continue;
                for (r = 0; r < output->persistenceRestoreRootCount; ++r)
                    if (output->persistenceRestoreRoots[r].type == instruction->payloadType &&
                        output->persistenceRestoreRoots[r].format == format) break;
                if (r < output->persistenceRestoreRootCount) continue;
                memset(&walk, 0, sizeof(walk)); walk.source = source; walk.recipes = recipes; walk.output = output;
                walk.root.pending = modulePending;
                walk.root.type = instruction->payloadType; walk.root.format = format;
                walk.root.firstNode = output->persistenceRestoreNodeCount;
                walk.root.firstEdge = output->persistenceRestoreEdgeCount;
                if (!xl6IrRestoreGraphIntern(&walk, instruction->payloadType, SIZE_MAX, &walk.root.root)) return 0;
                for (at = walk.root.firstNode; at < output->persistenceRestoreNodeCount; ++at)
                    if (!xl6IrRestoreGraphCompleteNode(&walk, at)) return 0;
                walk.root.nodeCount = output->persistenceRestoreNodeCount - walk.root.firstNode;
                walk.root.edgeCount = output->persistenceRestoreEdgeCount - walk.root.firstEdge;
                r = output->persistenceRestoreRootCount;
                if (r == SIZE_MAX || !xl6IrGrow((void**)&output->persistenceRestoreRoots,
                    &output->persistenceRestoreRootCapacity, r, sizeof(walk.root))) return 0;
                output->persistenceRestoreRoots[r] = walk.root; output->persistenceRestoreRootCount = r + 1;
            }
        }
    }
    return 1;
}

const XL6_IrPersistenceRestoreRoot* xl6IrPersistenceRestoreRootGet(
    const XL6_IrModule* module, XL6_IrTypeId type, uint32 format)
{
    size_t i;
    if (!module || module->persistenceRestoreRootCount > module->persistenceRestoreRootCapacity ||
        (module->persistenceRestoreRootCount && !module->persistenceRestoreRoots)) return NULL;
    for (i = 0; i < module->persistenceRestoreRootCount; ++i) {
        const XL6_IrPersistenceRestoreRoot* row = &module->persistenceRestoreRoots[i];
        if (row->type == type && row->format == format) {
            if (row->firstNode > module->persistenceRestoreNodeCount ||
                row->nodeCount > module->persistenceRestoreNodeCount - row->firstNode || row->root >= row->nodeCount ||
                row->firstEdge > module->persistenceRestoreEdgeCount ||
                row->edgeCount > module->persistenceRestoreEdgeCount - row->firstEdge) return NULL;
            return row;
        }
    }
    return NULL;
}

int xl6IrVerifyPersistenceRestoreGraphs(const XL6_IrModule* module)
{
    XL6_IrModule arena;
    XL6_IrPersistenceRestoreWalk recipes;
    size_t i;
    int ok;
    if (!xl6IrRestoreGraphTablesValid(module, module)) return 0;
#define XL6_RESTORE_GRAPH_TABLE(Name) \
    if (module->Name##Count > module->Name##Capacity || \
        (module->Name##Count && !module->Name##s)) return 0
    XL6_RESTORE_GRAPH_TABLE(persistenceRestoreRoot);
    XL6_RESTORE_GRAPH_TABLE(persistenceRestoreNode);
    XL6_RESTORE_GRAPH_TABLE(persistenceRestoreEdge);
#undef XL6_RESTORE_GRAPH_TABLE
    if (module->stage != XL6_IR_STAGE_LOWERED)
        return !module->persistenceRestoreRootCount && !module->persistenceRestoreNodeCount &&
            !module->persistenceRestoreEdgeCount;
    memset(&recipes, 0, sizeof(recipes)); recipes.source = module; recipes.verify = 1;
    ok = xl6IrPersistenceRestoreWalkArena(&recipes) && recipes.plans == module->persistenceRestorePlanCount &&
        recipes.schemas == module->persistenceRestoreSchemaCount && recipes.fields == module->persistenceRestoreFieldCount &&
        recipes.claims == module->persistenceRestoreClaimCount;
    xrtFree(recipes.marked);
    if (!ok) return 0;
    memset(&arena, 0, sizeof(arena));
    ok = xl6IrBuildPersistenceRestoreGraphs(module, module, &arena) &&
        arena.persistenceRestoreRootCount == module->persistenceRestoreRootCount &&
        arena.persistenceRestoreNodeCount == module->persistenceRestoreNodeCount &&
        arena.persistenceRestoreEdgeCount == module->persistenceRestoreEdgeCount;
#define XL6_RESTORE_EQUAL(Member) if (a->Member != b->Member) { ok = 0; break; }
    for (i = 0; ok && i < arena.persistenceRestoreRootCount; ++i) {
        const XL6_IrPersistenceRestoreRoot *a = &arena.persistenceRestoreRoots[i], *b = &module->persistenceRestoreRoots[i];
        XL6_RESTORE_EQUAL(type); XL6_RESTORE_EQUAL(format); XL6_RESTORE_EQUAL(pending);
        XL6_RESTORE_EQUAL(firstNode); XL6_RESTORE_EQUAL(nodeCount); XL6_RESTORE_EQUAL(root);
        XL6_RESTORE_EQUAL(firstEdge); XL6_RESTORE_EQUAL(edgeCount); XL6_RESTORE_EQUAL(seedCount);
    }
    for (i = 0; ok && i < arena.persistenceRestoreNodeCount; ++i) {
        const XL6_IrPersistenceRestoreNode *a = &arena.persistenceRestoreNodes[i], *b = &module->persistenceRestoreNodes[i];
        XL6_RESTORE_EQUAL(type); XL6_RESTORE_EQUAL(payloadType); XL6_RESTORE_EQUAL(abi); XL6_RESTORE_EQUAL(flags);
        XL6_RESTORE_EQUAL(identity); XL6_RESTORE_EQUAL(contextField); XL6_RESTORE_EQUAL(scalarRule);
        XL6_RESTORE_EQUAL(carrierAbi); XL6_RESTORE_EQUAL(seed); XL6_RESTORE_EQUAL(element);
        XL6_RESTORE_EQUAL(schema); XL6_RESTORE_EQUAL(firstEdge); XL6_RESTORE_EQUAL(edgeCount);
        XL6_RESTORE_EQUAL(defaultFunction); XL6_RESTORE_EQUAL(defaultSymbol); XL6_RESTORE_EQUAL(defaultErrorAbi);
        XL6_RESTORE_EQUAL(isolationAbi); XL6_RESTORE_EQUAL(cloneFunction);
        XL6_RESTORE_EQUAL(cloneSymbol); XL6_RESTORE_EQUAL(cloneErrorAbi);
        XL6_RESTORE_EQUAL(openClassLayout); XL6_RESTORE_EQUAL(openIsolationAbi);
        XL6_RESTORE_EQUAL(openCloneFunction); XL6_RESTORE_EQUAL(openCloneSymbol); XL6_RESTORE_EQUAL(openCloneErrorAbi);
    }
    for (i = 0; ok && i < arena.persistenceRestoreEdgeCount; ++i) {
        const XL6_IrPersistenceRestoreEdge *a = &arena.persistenceRestoreEdges[i], *b = &module->persistenceRestoreEdges[i];
        XL6_RESTORE_EQUAL(field); XL6_RESTORE_EQUAL(node);
    }
#undef XL6_RESTORE_EQUAL
    xrtFree(arena.persistenceRestoreRoots); xrtFree(arena.persistenceRestoreNodes); xrtFree(arena.persistenceRestoreEdges);
    return ok;
}
