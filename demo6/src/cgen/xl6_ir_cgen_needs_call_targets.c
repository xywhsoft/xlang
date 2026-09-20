/* Derive call-target runtime dependencies from reachable verified XLIR. */
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
    else if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) needs->needsResultString = 1;
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
            } else if ( target->parameterPhysicalAbis[parameter] ==
                            XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK ) {
                needs->needsRawCallbackRuntime = 1;
                needs->needsClosureRuntime = 1;
            }
        }
        if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_PRINT ) {
            needs->needsPrintRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueCountRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_WRITE ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_WRITE;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_TEXT_WRITELN ) {
            needs->textOutputBridgeOperations |= XL6_CGEN_TEXT_OUTPUT_WRITELN;
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
        } else if ( target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_PROCESS_EXEC &&
                    target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_PROCESS_EXIT_CODE ) {
            switch (target->runtimeBridge) {
            case XL6_IR_RUNTIME_BRIDGE_PROCESS_EXEC: needs->processRunBridgeOperations |= XL6_CGEN_PROCESS_EXEC; break;
            case XL6_IR_RUNTIME_BRIDGE_PROCESS_SUCCESS: needs->processRunBridgeOperations |= XL6_CGEN_PROCESS_SUCCESS; break;
            case XL6_IR_RUNTIME_BRIDGE_PROCESS_EXIT_CODE: needs->processRunBridgeOperations |= XL6_CGEN_PROCESS_EXIT_CODE; break;
            default: break;
            }
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_STRING_CONTAINS ) {
            needs->needsStringContainsRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_STRING_SPLIT_LINES ) {
            needs->needsStringSplitLinesRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueArrayRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_DYNAMIC_FIELDS ) {
            needs->needsDynamicFieldsBridgeRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_XVALUE_TABLE_REMOVE ) {
            needs->needsXValueTableRemoveBridgeRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
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
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_JSON_WRITER ) {
            needs->needsJsonWriterBridgeRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge >= XL6_IR_RUNTIME_BRIDGE_XSON_PARSE &&
                    target->runtimeBridge <= XL6_IR_RUNTIME_BRIDGE_XSON_WRITER ) {
            switch ( target->runtimeBridge ) {
            case XL6_IR_RUNTIME_BRIDGE_XSON_PARSE:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_PARSE; break;
            case XL6_IR_RUNTIME_BRIDGE_XSON_VALID:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_VALID; break;
            case XL6_IR_RUNTIME_BRIDGE_XSON_READ:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_READ; break;
            case XL6_IR_RUNTIME_BRIDGE_XSON_STRINGIFY:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_STRINGIFY; break;
            case XL6_IR_RUNTIME_BRIDGE_XSON_WRITE:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_WRITE; break;
            case XL6_IR_RUNTIME_BRIDGE_XSON_VISIT:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_VISIT;
                needs->needsLanguageFunctionRuntime = 1;
                needs->needsClosureRuntime = 1;
                break;
            case XL6_IR_RUNTIME_BRIDGE_XSON_WRITER:
                needs->xsonBridgeOperations |= XL6_CGEN_XSON_WRITER; break;
            default: break;
            }
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_STRING_INT_FORMAT ) {
            needs->needsStringIntFormatBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_STRING_UINT_FORMAT ) {
            needs->needsStringUIntFormatBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_STRING_NUM_FORMAT ) {
            needs->needsStringNumFormatBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_TAKE_CSTR ) {
            needs->needsTakeCstrBridgeRuntime = 1;
            needs->needsStringRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_TEMPLATE ) {
            needs->needsTemplateBridgeRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
            needs->needsErrorRuntime = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_BUFFER ) {
            needs->needsBufferBridgeRuntime = 1;
            needs->needsErrorRuntime = 1;
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
            default: return 0;
            }
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_TEXT_CODEC ) {
            needs->needsTextCodecBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge ==
                        XL6_IR_RUNTIME_BRIDGE_BASE64_TEXT_CODEC ) {
            needs->needsBase64TextCodecBridgeRuntime = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_HEX_BYTES_CODEC ) {
            needs->needsHexBytesCodecBridgeRuntime = 1;
            needs->needsStringViews = 1;
        } else if ( target->runtimeBridge == XL6_IR_RUNTIME_BRIDGE_BASE64_BYTES_CODEC ) {
            needs->needsBase64BytesCodecBridgeRuntime = 1;
            needs->needsStringViews = 1;
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
