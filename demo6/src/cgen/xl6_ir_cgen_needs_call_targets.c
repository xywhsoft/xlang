/* Derive call-target runtime dependencies from reachable verified XLIR. */
#include "src/compile/xl6_artifact_runtime.h"
static int xl6IrCgenRequireErrorResult(
    const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_IrTypeId returnType, XL6_IrNodeId declaration, XL6_DiagList* diags)
{
    const XL6_IrType* type;
    if ( returnType == XL6_IR_TYPE_INVALID ) needs->needsResultVoid = 1;
    else if ( (type = xl6IrTypeGet(&module->types, returnType)) == NULL )
        return xl6IrCgenDiag(diags, module, declaration,
            "XLIR C emitter error-result ABI is unresolved");
    else if ( type->cAbi == XL6_IR_C_ABI_BOOL ) needs->needsResultBool = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_I64 || type->cAbi == XL6_IR_C_ABI_TIME_I64 )
        needs->needsResultI64 = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_U64 ) needs->needsResultU64 = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_TYPE_ID ) needs->needsResultTypeId = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_F64 ) needs->needsResultF64 = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_F32 ) needs->needsResultF32 = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_CHAR_U32 ) needs->needsResultChar = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) needs->needsResultString = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES ) needs->needsResultXValue = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) needs->needsResultXValue = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ) needs->needsResultFuture = 1;
    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR ) needs->needsResultGenerator = 1;
    else if ( xl6IrCgenErrorResultType(module, returnType) != NULL ) needs->needsExtendedResults = 1;
    else return xl6IrCgenDiag(diags, module, declaration,
        "XLIR C emitter does not support this error-result ABI");
    return 1;
}


static int xl6IrCgenAnalyzeCallTargetNeeds(
    const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) continue;
        XL6_IrNativeCallValuePlan value={0};
        if (!xl6IrNativeCallValuePlanBuild(module,target,&value)) return 0;
        if (xl6IrCgenNativeCallValueEncoded(&value) && (value.inputParameter || value.output)) {
            needs->needsXValueRuntime=1;
            if (value.value.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1) {
                if(value.inputParameter)needs->needsXValueClosureBridgeRuntime=1;
                if(value.output)needs->needsXValueClosureProjectionRuntime=1;
                needs->needsClosureRuntime=1;needs->needsXCallRuntime=1;
            } else if(value.value.codecAbi==XL6_IR_SEQUENCE_CODEC_UNION_VALUE_V1) {
                needs->needsXValueUnionRuntime=1;needs->needsRuntimeTrap=1;
            }
        }
        if(target->runtimeBridge==XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE && target->cName &&
            strcmp(target->cName,"xl6DictValueRemoveV1")==0)needs->needsDictMethodsRuntime=1;
        if(target->asyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)
            needs->needsNativeFuturePayloadRuntime=1;
        if(target->kind==XL6_IR_CALL_TARGET_INTERNAL_ASYNC && target->asyncCaptureDestroyAbis)
            for(size_t p=0;p<target->parameterCount;++p)
                if(target->asyncCaptureDestroyAbis[p]==XL6_IR_ASYNC_FRAME_DESTROY_NATIVE_VALUE)
                    needs->needsNativeCaptureProfileHeaders=1;
        if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_LOG) {
            needs->needsLogAdapterDeclarations = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        /* A reached resident symbol needs the shared physical declarations
         * regardless of its language bridge family. Registry linkage, not a
         * VFS/FS symbol spelling or emitted adapter body, selects this header. */
        if (xl6ArtifactRuntimeSymbolOwner(target->cName) != XL6_ARTIFACT_RUNTIME_COUNT)
            needs->needsTextAdapterDeclarations = 1;
        if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_VALUE) {
            needs->needsTextAdapterDeclarations = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        if (target->runtimeBridgeOperation >= XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_CREATE &&
            target->runtimeBridgeOperation <= XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_INPUT_BYTES) {
            if (target->runtimeBridgeOperation == XL6_IR_RUNTIME_BRIDGE_OPERATION_SIPHASHER_UPDATE_RAW)
                needs->needsSipHasher64RawBridgeRuntime = 1;
            else needs->needsSipHasher64BridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
        {
            unsigned int randomOperation = 0;
            if (target->runtimeBridgeOperation >= XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_CREATE &&
                target->runtimeBridgeOperation <= XL6_IR_RUNTIME_BRIDGE_OPERATION_SECURE_STRING)
                randomOperation = 1u << (target->runtimeBridgeOperation - XL6_IR_RUNTIME_BRIDGE_OPERATION_RNG_CREATE);
            needs->randomBridgeOperations |= randomOperation;
            if (randomOperation & (XL6_CGEN_RNG_TEXT_MASK |
                    XL6_CGEN_SECURE_STRING | XL6_CGEN_SECURE_HEX)) {
                needs->needsStringRuntime = 1;
                needs->needsStringViews = 1;
            }
        }
        if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING &&
            target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_XID_IS_ZERO) {
            needs->xidBridgeOperations |= 1u <<
                (target->runtimeBridge - XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING);
            if (target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING)
                needs->needsXValueRuntime = 1;
            if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XID_MAKE_STRING ||
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XID_PARSE ||
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XID_TO_STRING) {
                needs->needsStringRuntime = 1;
                needs->needsStringViews = 1;
            }
        }
        if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_VFS_CREATE &&
            target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_ID) {
            needs->vfsBridgeOperations |= UINT64_C(1) <<
                (target->runtimeBridge - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE);
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN &&
            target->runtimeBridge < XL6_IR_RUNTIME_BRIDGE_FS_END) {
            unsigned operation = target->runtimeBridge - XL6_IR_RUNTIME_BRIDGE_FS_FILE_OPEN;
            uint64_t bit = UINT64_C(1) << (operation % 64);
            if (operation < 64) needs->fsBridgeOperations0 |= bit;
            else if (operation < 128) needs->fsBridgeOperations1 |= bit;
            else needs->fsBridgeOperations2 |= bit;
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_FS_WALK_VISIT) {
                needs->needsForeignErrorRuntime = 1;
                needs->needsErrorRuntime = 1;
            }
            if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_OPEN &&
                target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_FS_ASYNC_TREE_ENSURE)
                needs->needsFutureRuntime = 1;
        }
        if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_IO_READ_FILE &&
            target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_IO_WRITE_BUFFER_FILE) {
            needs->ioBridgeOperations |= UINT64_C(1) <<
                (target->runtimeBridge - XL6_IR_RUNTIME_BRIDGE_IO_READ_FILE);
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_LINE &&
            target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_DOWN) {
            needs->consoleBridgeOperations |= UINT64_C(1) <<
                (target->runtimeBridge - XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_LINE);
            if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_CONSOLE_READ_LINE ||
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_CONSOLE_STYLED ||
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_CONSOLE_SESSION_WRITE ||
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_CONSOLE_EVENT_TEXT) {
                needs->needsStringRuntime = 1;
                needs->needsStringViews = 1;
            }
        }
        if (needs->randomBridgeOperations &
            (XL6_CGEN_RNG_BYTES_MASK | XL6_CGEN_RNG_ARRAY_MASK))
            needs->needsXValueRuntime = 1;
        if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_FOREIGN_SCOPE ) {
            needs->needsForeignErrorRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( target->errorPhysicalAbi == XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER &&
             !xl6IrCgenRequireErrorResult(module, needs, target->returnType,
                 target->declaration, diags) ) return 0;
        for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
            if ( target->parameterPhysicalAbis[parameter] ==
                     XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE ) {
                const XL6_IrType* signature = xl6IrTypeGet(
                    &module->types, target->parameterTypes[parameter]);
                size_t callableParameter;
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
                needs->needsXValueRuntime = 1;
                if ( signature != NULL ) {
                    for ( callableParameter = 0;
                          callableParameter < signature->paramCount;
                          ++callableParameter ) {
                        const XL6_IrType* item = xl6IrTypeGet(
                            &module->types,
                            signature->params[callableParameter]);
                        if ( item != NULL &&
                             item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
                            needs->needsLanguageFunctionStringRuntime = 1;
                    }
                    if ( signature->callableResultType !=
                             XL6_IR_TYPE_INVALID ) {
                        const XL6_IrType* item = xl6IrTypeGet(
                            &module->types, signature->callableResultType);
                        if ( item != NULL &&
                             item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
                            needs->needsLanguageFunctionStringRuntime = 1;
                    }
                }
                if ( needs->needsLanguageFunctionStringRuntime ) {
                    needs->needsStringRuntime = 1;
                    needs->needsStringViews = 1;
                }
            } else if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_RESULT_CLOSURE_BORROW){
                needs->needsClosureRuntime=1;
            } else if ( target->parameterPhysicalAbis[parameter] ==
                            XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK ) {
                needs->needsRawCallbackRuntime = 1;
                needs->needsClosureRuntime = 1;
            }
        }
        if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_PRINT ) {
            needs->needsPrintRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLANG ) {
            needs->needsXlangRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_WRITE ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_WRITE;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_WRITELN ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_WRITELN;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_ERROR ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_ERROR;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_ERRORLN ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_ERRORLN;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_FLUSH ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_FLUSH;
        } else if ( target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_DIR_LIST &&
                    target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_DIR_SIZE ) {
            switch (target->runtimeBridge) {
            case XL6_IR_RUNTIME_BRIDGE_DIR_LIST: needs->dirBridgeOperations |= XL6_CGEN_DIR_LIST; break;
            case XL6_IR_RUNTIME_BRIDGE_DIR_GLOB: needs->dirBridgeOperations |= XL6_CGEN_DIR_GLOB; break;
            case XL6_IR_RUNTIME_BRIDGE_DIR_ENTRIES: needs->dirBridgeOperations |= XL6_CGEN_DIR_ENTRIES; break;
            case XL6_IR_RUNTIME_BRIDGE_DIR_IS_EMPTY: needs->dirBridgeOperations |= XL6_CGEN_DIR_IS_EMPTY; break;
            case XL6_IR_RUNTIME_BRIDGE_DIR_COUNT: needs->dirBridgeOperations |= XL6_CGEN_DIR_COUNT; break;
            case XL6_IR_RUNTIME_BRIDGE_DIR_SIZE: needs->dirBridgeOperations |= XL6_CGEN_DIR_SIZE; break;
            default: break;
            }
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_HASH_STRING ) {
            needs->needsHashStringBridgeRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_HASH_BYTES ) {
            needs->needsHashBytesBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_HASH_RAW ) {
            needs->needsHashRawBridgeRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_SIPHASH_STRING ) {
            needs->needsSipHashStringBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_SIPHASH_BYTES ) {
            needs->needsSipHashBytesBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_SIPHASH_RAW ) {
            needs->needsSipHashRawBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_STRING_CONTAINS ) {
            needs->needsStringContainsRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT_LINES ) {
            needs->needsStringSplitLinesRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueArrayRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_STRING_INDEX_OF ) {
            needs->needsStringIndexOfBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_JSON_PARSE ) {
            needs->jsonValueBridgeOperations |= XL6_CGEN_JSON_PARSE;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_VALID ) {
            needs->jsonValueBridgeOperations |= XL6_CGEN_JSON_VALID;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_PARSE_FILE ) {
            needs->jsonValueBridgeOperations |= XL6_CGEN_JSON_PARSE_FILE;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_STRINGIFY ) {
            needs->jsonValueBridgeOperations |= XL6_CGEN_JSON_STRINGIFY;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_WRITE_FILE ) {
            needs->jsonValueBridgeOperations |= XL6_CGEN_JSON_WRITE_FILE;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_VISIT ) {
            needs->needsJsonVisitBridgeRuntime = 1;
            needs->needsLanguageFunctionRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_WRITER ||
                    target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_WRITER_RESULT ) {
            needs->needsJsonWriterBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsResultBool = 1;
            needs->needsResultString = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSONL_STRINGIFY ) {
            needs->needsJsonlStringifyBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLONL_STRINGIFY ) {
            needs->needsXlonlStringifyBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSONL_READ_FILE ) {
            needs->needsJsonlReadFileBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSONL_WRITE_FILE ) {
            needs->needsJsonlWriteFileBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLONL_READ_FILE ) {
            needs->needsXlonlReadFileBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLONL_WRITE_FILE ) {
            needs->needsXlonlWriteFileBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSONL_PARSE ) {
            needs->needsJsonlParseBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSONL_VALID ) {
            needs->needsJsonlValidBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLONL_PARSE ) {
            needs->needsXlonlParseBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLONL_VALID ) {
            needs->needsXlonlValidBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if (target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_PARSE &&
                   target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_DECODE) {
            unsigned bit = (unsigned)(target->runtimeBridge - XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_PARSE);
            needs->structuredOptionsBridgeOperations |= 1u << bit;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            if (needs->structuredOptionsBridgeOperations & XL6_CGEN_CONFIG_READ_MASK)
                needs->needsResultXValue = 1;
            if (needs->structuredOptionsBridgeOperations & XL6_CGEN_CONFIG_WRITE_FILE_MASK)
                needs->needsResultBool = 1;
            if (needs->structuredOptionsBridgeOperations &
                (XL6_CGEN_CONFIG_JSON_VALID | XL6_CGEN_CONFIG_XLON_VALID |
                 XL6_CGEN_CONFIG_JSONL_VALID | XL6_CGEN_CONFIG_XLONL_VALID))
                needs->needsResultCBool = 1;
            if (needs->structuredOptionsBridgeOperations & XL6_CGEN_CONFIG_WRITE_TO_MASK) {
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
            }
            if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_CONFIG_VISIT) {
                needs->needsJsonVisitBridgeRuntime = 1;
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
            } else if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_VISIT) {
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_VISIT;
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
            }
            if (target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_XLON_CONFIG_DECODE) {
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
            }
        } else if ( target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_XLON_PARSE &&
                    target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_XLON_WRITER_RESULT ) {
            switch ( target->runtimeBridge ) {
            case XL6_IR_RUNTIME_BRIDGE_XLON_PARSE:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_PARSE; break;
            case XL6_IR_RUNTIME_BRIDGE_XLON_VALID:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_VALID; break;
            case XL6_IR_RUNTIME_BRIDGE_XLON_READ:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_READ; break;
            case XL6_IR_RUNTIME_BRIDGE_XLON_STRINGIFY:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_STRINGIFY; break;
            case XL6_IR_RUNTIME_BRIDGE_XLON_WRITE:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_WRITE; break;
            case XL6_IR_RUNTIME_BRIDGE_XLON_VISIT:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_VISIT;
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
                break;
            case XL6_IR_RUNTIME_BRIDGE_XLON_WRITER:
            case XL6_IR_RUNTIME_BRIDGE_XLON_WRITER_RESULT:
                needs->xlonBridgeOperations |= XL6_CGEN_XLON_WRITER; break;
            default: break;
            }
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            if (needs->xlonBridgeOperations & XL6_CGEN_XLON_WRITER) {
                needs->needsResultBool = 1;
                needs->needsResultString = 1;
            }
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_TAKE_CSTR ) {
            needs->needsTakeCstrBridgeRuntime = 1;
            needs->needsStringRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_STRING_CHAR_LEN ) {
            needs->needsStringCharLenBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( xl6IrRuntimeStringBridgeKindIs(target->runtimeBridge) ) {
            needs->needsStringViews = 1;
            switch ( target->runtimeBridge ) {
            case XL6_IR_RUNTIME_BRIDGE_STRING_BYTE_LEN: needs->needsStringByteLenBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_BYTE_AT: needs->needsStringByteAtBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_SLICE: needs->needsStringSliceBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_REPLACE: needs->needsStringReplaceBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_REPEAT: needs->needsStringRepeatBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_STARTS_WITH: needs->needsStringStartsWithBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_ENDS_WITH: needs->needsStringEndsWithBridgeRuntime = 1; break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT:
                needs->needsStringSplitBridgeRuntime = 1;
                needs->needsXValueRuntime = 1;
                needs->needsXValueArrayRuntime = 1;
                break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_BYTES:
            case XL6_IR_RUNTIME_BRIDGE_STRING_CHARS:
                if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_STRING_BYTES )
                    needs->needsStringBytesBridgeRuntime = 1;
                else
                    needs->needsStringCharsBridgeRuntime = 1;
                needs->needsXValueRuntime = 1;
                needs->needsXValueArrayRuntime = 1;
                break;
            case XL6_IR_RUNTIME_BRIDGE_STRING_JOIN:
                needs->needsStringJoinBridgeRuntime = 1;
                needs->needsXValueRuntime = 1;
                needs->needsXValueArrayRuntime = 1;
                needs->needsXValueToStringRuntime = 1;
                break;
            default: return 0;
            }
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_CRYPTO_TEXT_HASH ) {
            needs->needsCryptoTextHashBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_PATH_JOIN ) {
            needs->needsPathJoinBridgeRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_FILE ) {
            needs->needsFileBridgeRuntime = 1;
            needs->needsErrorRuntime = 1;
        } else if (xl6IrRuntimeNetDatagramBridgeKindIs(target->runtimeBridge)) {
            needs->needsNetDatagramRuntime = 1;
        } else if ( target->runtimeBridge >=
                        XL6_IR_RUNTIME_BRIDGE_NET_TCP_DIAL &&
                    target->runtimeBridge <=
                        XL6_IR_RUNTIME_BRIDGE_NET_TCP_SERVER ) {
            needs->needsNetTcpRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsXValueClassRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsFutureRuntime = 1;
            needs->needsErrorRuntime = 1;
        } else if ( target->runtimeBridge >=
                        XL6_IR_RUNTIME_BRIDGE_NET_TLS_CONFIG &&
                    target->runtimeBridge <=
                        XL6_IR_RUNTIME_BRIDGE_NET_TLS_SERVER ) {
            needs->needsNetTlsRuntime = 1;
            needs->needsNetTlsStreamRuntime |=
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_TLS_STREAM;
            needs->needsNetTlsDialRuntime |=
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_TLS_DIAL;
            needs->needsNetTlsServerRuntime |=
                target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_NET_TLS_SERVER;
            needs->needsXValueRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsXValueClassRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsFutureRuntime = 1;
            needs->needsErrorRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_HTTP1 ) {
            needs->needsHttp1Runtime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_FRAME ) {
            needs->needsWebSocketFrameRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_HANDSHAKE ) {
            needs->needsWebSocketHandshakeRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_UPGRADE ) {
            needs->needsWebSocketUpgradeRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_STREAM_CONFIG ) {
            needs->needsWebSocketStreamConfigRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_STREAM ) {
            needs->needsWebSocketStreamRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_WEBSOCKET_STREAM_TLS ) {
            needs->needsWebSocketStreamTlsRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_LAST_ERROR_TEXT ) {
            needs->needsLastErrorTextBridgeRuntime = 1;
            needs->needsErrorRuntime = 1;
            needs->needsStringRuntime = 1;
        }
        if ( target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR ) {
            const XL6_IrGeneratorMachine* machine =
                xl6IrCgenGeneratorMachine(module, target);
            size_t slot;
            needs->needsGeneratorRuntime = 1;
            needs->needsErrorRuntime = 1;
            if ( machine == NULL )
                return xl6IrCgenDiag(diags, module, target->declaration,
                    "XLIR C emitter generator machine feature set is unresolved");
            for ( slot = 0; slot < machine->frameSlotCount; ++slot ) {
                const XL6_IrContinuationFrameSlot* frame =
                    &module->generatorFrameSlots[
                        machine->firstFrameSlot + slot];
                const XL6_IrType* type =
                    xl6IrTypeGet(&module->types, frame->type);
                if ( type == NULL )
                    return xl6IrCgenDiag(diags, module, target->declaration,
                        "XLIR C emitter generator frame type is unresolved");
                if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
                    needs->needsStringRuntime = 1;
                else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
                    needs->needsXValueRuntime = 1;
                else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR )
                    needs->needsFutureRuntime = 1;
                else if ( type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
                    needs->needsClosureRuntime = 1;
                else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_CELL_PTR )
                    needs->needsCellRuntime = 1;
                else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR )
                    needs->needsErrorRuntime = 1;
                else if ( type->cAbi ==
                          XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR ) {
                    needs->needsXValueIteratorRuntime = 1;
                    needs->needsXValueRuntime = 1;
                    needs->needsRuntimeTrap = 1;
                }
            }
            {
                const XL6_IrType* item =
                    xl6IrTypeGet(&module->types, machine->itemType);
                if ( item == NULL )
                    return xl6IrCgenDiag(diags, module, target->declaration,
                        "XLIR C emitter generator item type is unresolved");
                if ( item->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
                    needs->needsStringRuntime = 1;
                else if ( item->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
                    needs->needsXValueRuntime = 1;
                else if ( item->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
                    needs->needsClosureRuntime = 1;
            }
        }
    }
    return 1;
}
