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
        (needs->needsCOwnedRuntime || needs->needsTextAdapterDeclarations || needs->needsLogAdapterDeclarations || needs->needsModuleOwnershipViews ||
         /* Value consumers now share resident providers and their public
          * physical types. Select the canonical DECLARATIONS for the entire
          * family before emitting any fallback typedef, not by whichever
          * optional method happens to accompany a minimal consumer. This
          * header contributes no implementation or native component root. */
         needs->needsXValueRuntime || needs->needsXValueBinaryRuntime || needs->needsXValueCompareRuntime ||
         needs->needsStringScalarRuntime || needs->needsBytesRuntime || needs->needsErrorRuntime ||
         needs->needsXCallRuntime || needs->needsClosureRuntime || needs->needsCellRuntime ||
         /* Even scalar-only machines emit physical Future/Promise frame
          * traces. Consume the canonical DECLARATION header, not a second
          * handwritten ownership ABI or an implementation amalgamation. */
         needs->needsAsyncMachineRuntime || needs->needsAsyncTaskRuntime || needs->needsFutureContinuationRuntime || needs->needsGeneratorRuntime || needs->needsXValueIteratorRuntime ||
         needs->needsPersistenceDynamicFields ||
         needs->needsPrintRuntime || needs->needsDebugConsoleRuntime ||
         needs->textOutputBridgeOperations != 0 ||
         needs->needsXlangRuntime ||
         needs->needsXValueDynamicFieldRuntime ||
         needs->needsXValueUnionRuntime ||
         needs->needsCollectionAlgebraRuntime ||
         needs->needsNetTcpRuntime || needs->needsNetTlsRuntime || needs->needsNetDatagramRuntime ||
         needs->dirBridgeOperations != 0 ||
         needs->needsStringRepeatBridgeRuntime || needs->needsStringSplitBridgeRuntime || needs->needsStringSplitLinesRuntime ||
         needs->needsStringJoinBridgeRuntime ||
         needs->needsHashBytesBridgeRuntime ||
         needs->needsSipHashStringBridgeRuntime ||
         needs->needsSipHashBytesBridgeRuntime ||
         needs->needsSipHashRawBridgeRuntime ||
         needs->needsSipHasher64BridgeRuntime ||
         needs->needsSipHasher64RawBridgeRuntime ||
         needs->randomBridgeOperations != 0 ||
         needs->xidBridgeOperations != 0 ||
         needs->ioBridgeOperations != 0 || needs->consoleBridgeOperations != 0 ||
         needs->vfsBridgeOperations != 0 ||
         needs->fsBridgeOperations0 != 0 || needs->fsBridgeOperations1 != 0 || needs->fsBridgeOperations2 != 0 ||
         needs->needsStringBytesBridgeRuntime || needs->needsStringCharsBridgeRuntime ||
         needs->needsHttp1Runtime || needs->needsJsonVisitBridgeRuntime ||
         needs->needsJsonWriterBridgeRuntime || needs->xlonBridgeOperations != 0 ||
         needs->structuredOptionsBridgeOperations != 0 ||
         needs->needsJsonlStringifyBridgeRuntime ||
         needs->needsXlonlStringifyBridgeRuntime ||
         needs->needsJsonlReadFileBridgeRuntime ||
         needs->needsJsonlWriteFileBridgeRuntime ||
         needs->needsXlonlReadFileBridgeRuntime ||
         needs->needsXlonlWriteFileBridgeRuntime ||
         needs->needsJsonlParseBridgeRuntime ||
         needs->needsJsonlValidBridgeRuntime ||
         needs->needsXlonlParseBridgeRuntime ||
         needs->needsXlonlValidBridgeRuntime ||
         xl6IrCgenNeedsWebSocketRuntime(needs));
}
#define XL6_IO_BIT(Name) (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_IO_##Name - XL6_IR_RUNTIME_BRIDGE_IO_READ_FILE))
#define XL6_VFS_BIT(Name) (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_##Name - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE))
#define XL6_CONSOLE_BIT(Name) (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_CONSOLE_##Name - XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_LINE))
