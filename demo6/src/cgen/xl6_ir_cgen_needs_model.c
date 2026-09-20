/* Closed feature model computed from verified XLIR before C emission. */
typedef struct XL6_IrCgenNeeds {
#define XL6_IR_CGEN_NEED(Type, Field, Group) Type Field;
#include "src/cgen/xl6_ir_cgen_needs.inc"
#undef XL6_IR_CGEN_NEED
} XL6_IrCgenNeeds;

static int xl6IrCgenNeedsWebSocketRuntime(
    const XL6_IrCgenNeeds* needs)
{
    return needs != NULL &&
        (needs->needsWebSocketFrameRuntime ||
         needs->needsWebSocketHandshakeRuntime ||
         needs->needsWebSocketUpgradeRuntime ||
         needs->needsWebSocketStreamConfigRuntime ||
         needs->needsWebSocketStreamRuntime ||
         needs->needsWebSocketStreamTlsRuntime);
}

static int xl6IrCgenNeedsXrtDeclarations(const XL6_IrCgenNeeds* needs)
{
    return needs != NULL &&
        (needs->needsModuleOwnershipViews || needs->needsXValueBinaryRuntime || needs->needsXValueCompareRuntime ||
         needs->needsXCallRuntime || needs->needsClosureRuntime || needs->needsCellRuntime ||
         /* Even scalar-only machines emit physical Future/Promise frame
          * traces. Consume the canonical DECLARATION header, not a second
          * handwritten ownership ABI or an implementation amalgamation. */
         needs->needsAsyncMachineRuntime || needs->needsAsyncTaskRuntime || needs->needsFutureContinuationRuntime || needs->needsGeneratorRuntime || needs->needsXValueIteratorRuntime ||
         needs->needsDynamicFieldsBridgeRuntime || needs->needsPersistenceDynamicFields ||
         needs->needsXValueDynamicFieldRuntime ||
         needs->needsXValueUnionRuntime || needs->needsTemplateBridgeRuntime ||
         needs->needsCollectionAlgebraRuntime ||
         needs->needsNetTcpRuntime || needs->needsNetTlsRuntime || needs->needsNetDatagramRuntime ||
         needs->dirBridgeOperations != 0 || needs->processRunBridgeOperations != 0 ||
         needs->needsStringRepeatBridgeRuntime || needs->needsStringSplitBridgeRuntime || needs->needsStringSplitLinesRuntime ||
         needs->needsHttp1Runtime || needs->needsJsonVisitBridgeRuntime ||
         needs->needsJsonWriterBridgeRuntime || needs->xsonBridgeOperations != 0 ||
         xl6IrCgenNeedsWebSocketRuntime(needs));
}
