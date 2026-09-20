int xl6CgenEmitC(
    XL6_Output* out,
    XL6_AstNode* root,
    const XL6_HirModule* hir,
    XL6_DiagList* diags
)
{
    return xl6CgenEmitCEx(out, root, hir, diags, NULL);
}

int xl6CgenEmitCEx(
    XL6_Output* out,
    XL6_AstNode* root,
    const XL6_HirModule* hir,
    XL6_DiagList* diags,
    const XL6_CgenOptions* options
)
{
    XL6_CgenContext ctx;
    size_t i;
    int hasTopLevel;
    int hasModuleGlobals;
    int hasEntryRuntime;
    int hasStringRuntime;
    int hasTypeConvertRuntime;
    int hasFunctionRuntime;
    int hasFunctionVarRuntime;
    int hasXValueRuntime;
    int hasXValueBinaryRuntime;
    int hasPrintRuntime;
    int hasClassHandleRuntime;
	int hasRuntimeTypeOps;
    int hasErrorRuntime;
    int hasDynamicForInErrorRuntime;
    int hasAsyncRuntime;
    int hasGeneratorRuntime;
    int hasCryptoRuntime;
    int hasXidRuntime;
    int hasJsonRuntime;
    int hasProcessRuntime;
    int hasProcessWaitAsyncRuntime;
    int hasNetRuntime;
    int hasDirRuntime;
    int hasStringEdgeRuntime;
    int hasStdStringRuntime;
    int hasPathRuntime;
    int hasOsRuntime;
    int hasBufferRuntime;
    int hasTlsRuntime;
    int hasTlsStreamRuntime;
    int hasTlsDialRuntime;
    int hasTlsServerRuntime;
    int hasHttp1Runtime;
    int hasHttpDecodeRuntime;
    int hasWsFrameRuntime;
    int hasWsHandshakeRuntime;
    int hasWsUpgradeRuntime;
    int hasWsStreamConfigRuntime;
    int hasWsStreamRuntime;
    int hasWsStreamTlsRuntime;
    int hasSpecialCollectionRuntime;
    int hasEncodingRuntime;
    int hasLogRuntime;
    int hasSignalRuntime;
    int hasSignalChannelRuntime;
    int hasTemplateRuntime;
    int hasFileRuntime;
    int hasAsyncFileRuntime;
    int hasTccRuntime;
    int hasXlangRuntime;
    int hasTaskSpawn;
    int hasTaskCollectionRuntime;
    int hasPointMove;
    int hasWeakRuntime;
    int hasTypedContainerRuntime;
    int hasTypeStringValueRuntime;
    int hasUserProtocols;
    int needsObjectClass;
    int needsDynamicFieldsClass;
    XL6_CgenNetNeeds netNeeds;
    const XL6_AstNode* mainFunc;

    if ( out == NULL || root == NULL || hir == NULL || !hir->ready ||
         hir->sourceRoot != root ) {
        if ( diags != NULL ) {
            xl6DiagAdd(
                diags,
                root != NULL ? root->line : 0,
                root != NULL ? root->column : 0,
                "代码生成要求与当前 AST 对应的已完成 typed HIR"
            );
        }
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.root = root;
    ctx.hir = hir;
    ctx.diags = diags;
    ctx.ok = 1;
    if ( options != NULL ) {
        ctx.debugMode = options->debugMode;
        ctx.debugSourcePath = options->debugSourcePath;
    }

    if ( !xl6CgenBuildRootTypeIndex(&ctx) ) {
        xl6CgenFreeRootTypeIndex(&ctx);
        return 0;
    }
    xl6CgenCollectAnonFuncs(&ctx, root);
    xl6CgenCollectGenericInstancesToFixedPoint(&ctx);
    xl6CgenDiscoverGenericClassInstancesToFixedPoint(&ctx);
    xl6CgenCollectGenericProtocolInstances(&ctx, root);
    xl6CgenCollectNullableTypes(&ctx);
    xl6CgenCollectRuntimeTypeDescs(&ctx, root);
    xl6CgenFreeVarsSince(&ctx, 0);
    mainFunc = xl6CgenFindMainEntry(root);
    needsObjectClass = xl6CgenNodeNeedsBuiltinObjectClass(root);
    needsDynamicFieldsClass = xl6CgenHasBuiltinDynamicFieldsClass(root);
    hasTopLevel = xl6CgenHasTopLevelRuntimeStmt(root);
    hasModuleGlobals = xl6CgenHasModuleGlobalDecl(root);
    hasUserProtocols = xl6CgenHasUserProtocols(&ctx);
    /*
        protocol witness 具有模块级注册/注销生命周期。
        即使源文件只有 main，也必须经过统一模块入口，不能绕过元数据初始化。
    */
    hasEntryRuntime = mainFunc != NULL || hasTopLevel || hasModuleGlobals ||
                      hasUserProtocols;
    hasStringRuntime = xl6CgenProgramNeedsStringRuntime(&ctx, root) ||
        ctx.nullableTypeCount > 0;
    hasTypeConvertRuntime = xl6CgenProgramNeedsTypeConvertRuntime(&ctx, root);
    hasFunctionRuntime = xl6CgenNodeNeedsFunctionRuntime(root);
    hasFunctionVarRuntime = xl6CgenProgramNeedsFunctionVarRuntime(&ctx, root);
    /* bare function name passed as call argument needs function-var runtime */
    if ( !hasFunctionVarRuntime && xl6CgenProgramHasBareFuncCallArg(&ctx, root) ) {
        hasFunctionVarRuntime = 1;
    }
    hasClassHandleRuntime = xl6CgenNodeNeedsClassHandleRuntime(root);
    hasPrintRuntime = xl6CgenNodeNeedsPrintRuntime(root);
    hasXValueRuntime = xl6CgenNodeNeedsXValueRuntime(root) || hasFunctionVarRuntime ||
                       ctx.runtimeTypeDescCount > 0 || hasClassHandleRuntime || hasPrintRuntime;
    hasXValueBinaryRuntime = xl6CgenNodeNeedsXValueBinaryRuntime(&ctx, root);
    if ( hasXValueBinaryRuntime ) {
        hasXValueRuntime = 1;
    }
    hasAsyncRuntime = xl6CgenNodeNeedsAsyncRuntime(root);
    hasGeneratorRuntime = xl6CgenNodeNeedsGeneratorRuntime(root);
    hasTaskSpawn = xl6CgenNodeNeedsTaskSpawn(root);
    hasTaskCollectionRuntime = xl6CgenNodeNeedsTaskCollectionRuntime(root);
    hasPointMove = xl6CgenNodeNeedsPointMove(&ctx, root);
    hasWeakRuntime = xl6CgenNodeNeedsWeakRuntime(root);
    hasTypedContainerRuntime = xl6CgenContextNeedsTypedContainerRuntime(&ctx);
    if ( hasTaskCollectionRuntime ) {
        hasTypedContainerRuntime = 1;
    }
	hasRuntimeTypeOps = hasXValueRuntime && xl6CgenNodeNeedsRuntimeTypeOps(root);
    hasDynamicForInErrorRuntime = xl6CgenProgramNeedsDynamicForInErrorRuntime(&ctx, root);
    hasErrorRuntime = xl6CgenNodeNeedsErrorRuntime(&ctx, root) || hasAsyncRuntime ||
                      hasGeneratorRuntime ||
                      hasDynamicForInErrorRuntime || hasTypeConvertRuntime ||
					  hasRuntimeTypeOps ||
                      hasClassHandleRuntime || hasUserProtocols ||
                      ctx.nullableTypeCount > 0;
    hasCryptoRuntime = xl6CgenNodeNeedsCryptoRuntime(root);
    hasXidRuntime = xl6CgenNodeNeedsXidRuntime(root);
    hasJsonRuntime = xl6CgenNodeNeedsJsonRuntime(root);
    hasProcessRuntime = xl6CgenNodeNeedsProcessRuntime(root);
    hasProcessWaitAsyncRuntime = xl6CgenNodeNeedsProcessWaitAsyncRuntime(root);
    hasNetRuntime = xl6CgenNodeNeedsNetRuntime(root);
    hasDirRuntime = xl6CgenNodeNeedsDirRuntime(root);
    hasStringEdgeRuntime = xl6CgenNodeNeedsStringEdgeRuntime(root);
    hasStdStringRuntime = xl6CgenNodeNeedsStdStringRuntime(root);
    hasPathRuntime = xl6CgenNodeNeedsPathRuntime(root);
    hasOsRuntime = xl6CgenNodeNeedsOsRuntime(root);
    hasBufferRuntime = xl6CgenNodeNeedsBufferRuntime(root);
    hasTlsRuntime = xl6CgenNodeNeedsTlsRuntime(root);
    hasTlsStreamRuntime = xl6CgenNodeNeedsTlsStreamRuntime(root);
    hasTlsDialRuntime = xl6CgenNodeNeedsTlsDialRuntime(root);
    hasTlsServerRuntime = xl6CgenNodeNeedsTlsServerRuntime(root);
    hasHttp1Runtime = xl6CgenNodeNeedsHttp1Runtime(root);
    hasHttpDecodeRuntime = xl6CgenNodeNeedsHttpDecodeRuntime(root);
    hasWsFrameRuntime = xl6CgenNodeNeedsWsFrameRuntime(root);
    hasWsHandshakeRuntime = xl6CgenNodeNeedsWsHandshakeRuntime(root);
    hasWsUpgradeRuntime = xl6CgenNodeNeedsWsUpgradeRuntime(root);
    hasWsStreamConfigRuntime = xl6CgenNodeNeedsWsStreamConfigRuntime(root);
    hasWsStreamRuntime = xl6CgenNodeNeedsWsStreamRuntime(root);
    hasWsStreamTlsRuntime = xl6CgenNodeNeedsWsStreamTlsRuntime(root);
    if ( hasWsStreamTlsRuntime ) {
        hasWsStreamRuntime = 1;
    }
    if ( hasWsStreamRuntime ) {
        hasWsStreamConfigRuntime = 1;
        hasWsUpgradeRuntime = 1;
    }
    if ( hasWsUpgradeRuntime ) {
        hasHttp1Runtime = 1;
        hasWsHandshakeRuntime = 1;
    }
    hasSpecialCollectionRuntime = xl6CgenNodeNeedsSpecialCollectionRuntime(root);
    xl6CgenNetCollectNeeds(
        &ctx,
        &netNeeds,
        hasTlsDialRuntime,
        hasTlsServerRuntime
    );
    if ( netNeeds.interfaces ) {
        hasBufferRuntime = 1;
    }
    if ( hasTlsRuntime ) {
        hasBufferRuntime = 1;
        hasErrorRuntime = 1;
    }
    if ( hasHttp1Runtime || hasHttpDecodeRuntime || hasWsFrameRuntime ||
         hasWsHandshakeRuntime || hasWsUpgradeRuntime || hasWsStreamRuntime ) {
        hasBufferRuntime = 1;
        hasErrorRuntime = 1;
    }
    if ( hasWsStreamRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    /* 仅 Future 路径依赖完整异步运行时；同步 DNS 不应带入任务和 Channel。 */
    if ( netNeeds.future ) {
        hasAsyncRuntime = 1;
        hasErrorRuntime = 1;
    }
    if ( hasTlsStreamRuntime || hasTlsServerRuntime ) {
        hasAsyncRuntime = 1;
        hasErrorRuntime = 1;
    }
    /* TLS verification callbacks are dispatched through XL6_Function. */
    if ( hasTlsRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    hasEncodingRuntime = xl6CgenNodeNeedsEncodingRuntime(root);
    hasLogRuntime = xl6CgenNodeNeedsLogRuntime(root);
    hasSignalRuntime = xl6CgenNodeNeedsSignalRuntime(root);
    hasSignalChannelRuntime = xl6CgenNodeNeedsSignalChannelRuntime(root);
    hasTemplateRuntime = xl6CgenNodeNeedsTemplateRuntime(root);
    hasFileRuntime = xl6CgenNodeNeedsFileRuntime(root);
    hasAsyncFileRuntime = xl6CgenNodeNeedsAsyncFileRuntime(root);
    hasTccRuntime = xl6CgenNodeNeedsTccRuntime(root);
    hasXlangRuntime = xl6CgenNodeNeedsXlangRuntime(root);
    if ( hasAsyncFileRuntime ) {
        hasAsyncRuntime = 1;
        hasErrorRuntime = 1;
        hasBufferRuntime = 1;
        hasFileRuntime = 1;
    }
    if ( hasFileRuntime ) {
        hasErrorRuntime = 1;
    }
    if ( hasStdStringRuntime ) {
        hasTypeConvertRuntime = 1;
        hasErrorRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasDirRuntime ) {
        hasXValueRuntime = 1;
    }
    if ( hasSpecialCollectionRuntime ) {
        /* Stack/Tree/Queue 共享语言类型到 XRT xrttype 的 typed bridge。 */
        hasTypedContainerRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasTemplateRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasSignalRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasSignalChannelRuntime ) {
        hasAsyncRuntime = 1;
        hasErrorRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasJsonRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasXlangRuntime ) {
        hasStringRuntime = 1;
    }
    if ( needsObjectClass ) {
        hasXValueRuntime = 1;
    }
    if ( needsDynamicFieldsClass ) {
        hasXValueRuntime = 1;
    }
    if ( hasWeakRuntime ) {
        hasXValueRuntime = 1;
    }
    if ( hasTypedContainerRuntime ) {
        hasXValueRuntime = 1;
    }
    hasTypeStringValueRuntime = hasTypedContainerRuntime || ctx.nullableTypeCount > 0;
    if ( hasTaskSpawn ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    if ( hasAsyncRuntime && hasFunctionRuntime ) {
        hasFunctionVarRuntime = 1;
        hasXValueRuntime = 1;
    }
    /*
        动态函数调用通过 xvalue* 返回结果，再转换为静态返回类型。
        必须在所有复合 needs 展开后收闭包，不能依赖运行时头中的重复 helper。
    */
    if ( hasFunctionVarRuntime ) {
        hasTypeConvertRuntime = 1;
        hasErrorRuntime = 1;
    }
    ctx.needsXValueRuntime = hasXValueRuntime;
    ctx.needsFunctionVarRuntime = hasFunctionVarRuntime;
    ctx.needsErrorRuntime = hasErrorRuntime;
    ctx.wrapExplicitMainWithTopLevel = (mainFunc != NULL && hasEntryRuntime);

    xl6OutputPuts("/* Generated by xlang DEMO6 cgen. */\n\n", out);
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <limits.h>\n", out);
    xl6OutputPuts("#include <stdlib.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n\n", out);
    xl6OutputPuts("#ifndef XL6_C_API\n", out);
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("#if defined(XL6_BUILD_SHARED)\n", out);
    xl6OutputPuts("#define XL6_C_API __declspec(dllexport)\n", out);
    xl6OutputPuts("#elif defined(XL6_USE_SHARED)\n", out);
    xl6OutputPuts("#define XL6_C_API __declspec(dllimport)\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("#define XL6_C_API\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#elif defined(__GNUC__) && defined(XL6_BUILD_SHARED)\n", out);
    xl6OutputPuts("#define XL6_C_API __attribute__((visibility(\"default\")))\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("#define XL6_C_API\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#endif\n\n", out);
    if ( ctx.debugMode ) {
        xl6OutputPuts("#include <stdio.h>\n\n", out);
    }
    xl6OutputPuts("typedef intptr_t xl6_ssize_t;\n\n", out);

    /* 生成单元的临时所有权状态必须与语言运行时生命周期严格配对。 */
    xl6OutputPuts("static bool xl6_temp_managed_runtime_init(void);\n", out);
    xl6OutputPuts("static void xl6_temp_managed_runtime_unit(void);\n", out);
    xl6OutputPuts("#define XL_RUNTIME_INIT_HOOK() xl6_temp_managed_runtime_init()\n", out);
    xl6OutputPuts("#define XL_RUNTIME_UNIT_HOOK() xl6_temp_managed_runtime_unit()\n", out);
    xl6OutputPuts("#define XL_RUNTIME_NEED_LIFECYCLE 1\n", out);
    if ( hasPointMove ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_POINT_MOVE 1\n", out);
    }
    if ( hasStringRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_STRING 1\n", out);
    }
    if ( hasXValueRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_XVALUE 1\n", out);
    }
    if ( hasClassHandleRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_CLASS_HANDLE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_ATOMIC 1\n", out);
    }
    if ( hasAsyncRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_FUTURE_TYPE 1\n", out);
    }
    if ( hasXValueBinaryRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_XVALUE_BINARY 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_STRING 1\n", out);
    }
    if ( hasWeakRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_WEAK 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_RUNTIME_VALUE_WEAK 1\n", out);
    }
    if ( hasTypedContainerRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_TYPED_CONTAINER 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_ARRAY_VALUE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_LIST_VALUE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_SET_VALUE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_DICT_VALUE 1\n", out);
    }
    if ( needsObjectClass || needsDynamicFieldsClass ) {
        xl6OutputPuts("#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD 1\n", out);
    }
    if ( hasTypeStringValueRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_TYPE_STRING_VALUE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_RUNTIME_CONVERT_STRING 1\n", out);
    }
    if ( hasPrintRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_PRINT 1\n", out);
    }
    if ( mainFunc != NULL || hasEntryRuntime ) {
        xl6OutputPuts("#define XL_RUNTIME_NEED_ARGV 1\n", out);
    }
    /* XRT declarations are module-gated.  Runtime fragments must request
       their public XRT contract before xl_runtime.h is included. */
    if ( hasNetRuntime ) {
        if ( netNeeds.engine ) {
            xl6OutputPuts("#define XRT_MODULE_NET_ENGINE 1\n", out);
        }
        if ( netNeeds.bytes ) {
            xl6OutputPuts("#define XRT_MODULE_NET_BUFFER 1\n", out);
        }
        if ( netNeeds.address ) {
            xl6OutputPuts("#define XRT_MODULE_NET_DNS 1\n", out);
        }
        if ( netNeeds.interfaces ) {
            xl6OutputPuts("#define XRT_MODULE_NET_INTERFACE 1\n", out);
        }
        if ( netNeeds.resolver ) {
            xl6OutputPuts("#define XRT_MODULE_NET_RESOLVER_FUTURE 1\n", out);
        }
        if ( netNeeds.proxy ) {
            xl6OutputPuts("#define XRT_MODULE_NET_PROXY 1\n", out);
        }
        if ( netNeeds.tcpStream ) {
            xl6OutputPuts("#define XRT_MODULE_NET_TCP_SYNC 1\n", out);
        }
        if ( netNeeds.tcpFile ) {
            xl6OutputPuts("#define XRT_MODULE_NET_TCP_FILE 1\n", out);
        }
        if ( netNeeds.tcpDial ) {
            xl6OutputPuts("#define XRT_MODULE_NET_TCP_DIAL_SYNC 1\n", out);
        }
        if ( netNeeds.tcpServer || netNeeds.tcpListen ) {
            xl6OutputPuts("#define XRT_MODULE_NET_TCP_SERVER_SYNC 1\n", out);
        }
        if ( netNeeds.udp ) {
            xl6OutputPuts("#define XRT_MODULE_NET_UDP_SYNC 1\n", out);
        }
    }
    if ( hasTlsRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_TLS_SERVER_RESUME 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_IDENTITY_RSA 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_IDENTITY_P256 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_IDENTITY_P384 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_IDENTITY_ED25519 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_CLIENT_RESUME 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_RECORD_AES 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_RECORD_CHACHA 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_SCHEDULE_SHA256 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_SCHEDULE_SHA384 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_KEY_EXCHANGE_X448 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_KEY_EXCHANGE_P256 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TLS_KEY_EXCHANGE_P384 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_X509_STORE_FILE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_X509_STORE_SYSTEM 1\n", out);
    }
    if ( hasTlsStreamRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_TLS_STREAM_FUTURE 1\n", out);
    }
    if ( hasTlsDialRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE 1\n", out);
    }
    if ( hasTlsServerRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_TLS_STREAM_LISTENER_SYNC 1\n", out);
    }
    if ( netNeeds.tcpStream || hasTlsStreamRuntime || hasWsStreamRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_EVENT 1\n", out);
    }
    if ( hasHttp1Runtime ) {
        xl6OutputPuts("#define XRT_MODULE_HTTP1_BODY 1\n", out);
    }
    if ( hasHttpDecodeRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_HTTP_DECODE 1\n", out);
    }
    if ( hasWsFrameRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_FRAME 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_CLOSE 1\n", out);
    }
    if ( hasWsHandshakeRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_KEYGEN 1\n", out);
    }
    if ( hasWsUpgradeRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE 1\n", out);
    }
    if ( hasWsStreamRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_STREAM_REF 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_STREAM_DEFLATE 1\n", out);
    }
    if ( hasWsStreamTlsRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_WEBSOCKET_STREAM_TLS 1\n", out);
    }
    if ( hasXidRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_XID 1\n", out);
    }
    if ( hasLogRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_LOGGER_CORE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_LOGGER_CONSOLE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_LOGGER_FILE_TEXT 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_LOGGER_FILE_JSON 1\n", out);
    }
    if ( hasTemplateRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_TEMPLATE_CORE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TEMPLATE_FILE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TEMPLATE_EXTENSION 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TEMPLATE_COMPOSE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_TEMPLATE_CONTROL 1\n", out);
    }
    if ( hasProcessRuntime ) {
        xl6OutputPuts("#define XRT_MODULE_PROCESS_RUN 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_PROCESS_PIPELINE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_PROCESS_OPEN 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_PROCESS_FILE 1\n", out);
        xl6OutputPuts("#define XRT_MODULE_PROCESS_TERMINAL 1\n", out);
        if ( hasProcessWaitAsyncRuntime ) {
            xl6OutputPuts("#define XRT_MODULE_PROCESS_FUTURE 1\n", out);
        }
    }
	if ( hasSpecialCollectionRuntime ) {
		xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_STACK 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_TREE 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_QUEUE_SPSC 1\n", out);
        xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_QUEUE_MPSC 1\n", out);
		xl6OutputPuts("#define XRUNTIME_MODULE_TYPED_QUEUE_MPMC 1\n", out);
	}
	if ( hasErrorRuntime ) {
		/* 运行时头的 typed 边界助手必须在展开时看到异常入口。 */
		xl6OutputPuts(
			"static void xl6_error_raise(const char* kind, const char* message, "
			"const char* file, int line, int column);\n"
			"#define XL6_TYPED_INDEX_RAISE(kind, message, file, line, column) "
			"xl6_error_raise((kind), (message), (file), (line), (column))\n",
			out
		);
	}
	/*
		xl_runtime.h 是 xlang 生成代码的内部运行时头。
        使用专用 VFS 路径，避免 release/include 下的外部残留文件覆盖内置版本。
    */
    xl6OutputPuts("#include <__xl_runtime/xl_runtime.h>\n\n", out);
    /*
        所有生成单元都由 xl_runtime.h 提供 XRT 的公开视图类型。
        旧 cgen 分片使用私有 guard 防止重复输出；在统一入口补齐这些
        guard，避免它们在运行时头之后再次定义 xstrview/xbytesview。
    */
    xl6OutputPuts("#define XL6_XRT_STRVIEW_TYPEDEF 1\n", out);
    xl6OutputPuts("#define XL6_XRT_BYTESVIEW_TYPEDEF 1\n", out);
    xl6OutputPuts("#define XL6_XRT_BYTES_VIEW_TYPEDEF 1\n", out);
    xl6OutputPuts("#define XL6_XRT_HTTP_FIELD_TYPEDEF 1\n", out);
    xl6OutputPuts("#define XL6_XRT_QUERY_PAIR_TYPEDEF 1\n", out);
    xl6OutputPuts("#define XL6_XRT_FORM_DATA_PART_TYPEDEF 1\n", out);
    xl6OutputPuts("#define XL6_XRT_PEM_TYPES 1\n", out);
    xl6OutputPuts("#define XL6_XBUFFER_TYPEDEF 1\n\n", out);
	xl6CgenEmitTempScopeRuntime(
		out,
		hasStringRuntime,
		hasAsyncFileRuntime,
		hasNetRuntime
	);
    if ( ctx.debugMode ) {
        xl6CgenEmitDebugRuntime(out);
    }
    if ( hasFunctionRuntime || hasTaskSpawn || hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionRuntime(out);
    }
    if ( hasTypeConvertRuntime ) {
        xl6CgenEmitTypeConvertRuntime(&ctx);
    }
    if ( hasErrorRuntime ) {
        xl6CgenEmitErrorRuntime(out, hasXValueRuntime);
    }
    if ( hasClassHandleRuntime ) {
        xl6CgenEmitClassHandleRuntime(out);
    }
    if ( hasSpecialCollectionRuntime ) {
        xl6CgenEmitSpecialCollectionRuntime(out);
    }
    if ( hasAsyncRuntime ) {
		xl6CgenEmitAsyncRuntime(
			out,
			hasTaskSpawn,
			hasTypedContainerRuntime,
			hasSpecialCollectionRuntime,
			hasWeakRuntime,
			hasStringRuntime,
			hasFunctionVarRuntime);
	}
    if ( hasGeneratorRuntime ) {
        xl6CgenEmitGeneratorRuntime(out);
    }
    if ( hasAsyncFileRuntime ) {
        xl6CgenEmitAsyncFileRuntime(out);
    }
    if ( hasFileRuntime ) {
        xl6CgenEmitFileRuntime(out, 0, 1);
    }
    /*
        复合运行时类型描述可能引用 protocol TypeDesc。
        原型必须先于 descriptor 定义，而不是等到 class 声明阶段。
    */
    xl6CgenUserProtocolTypeProtos(&ctx);
    xl6CgenEmitNullableTypeDescProtos(&ctx);
    if ( hasXValueRuntime ) {
        xl6CgenEmitRuntimeTypeDescs(&ctx);
    }
    if ( hasCryptoRuntime ) {
        xl6CgenEmitCryptoRuntime(out);
    }
    if ( hasXidRuntime ) {
        xl6CgenEmitXidRuntime(out);
    }
    if ( hasJsonRuntime ) {
        xl6CgenEmitJsonRuntime(out);
    }
    if ( hasProcessRuntime ) {
        xl6CgenEmitProcessRuntime(out, hasProcessWaitAsyncRuntime);
    }
    if ( hasDirRuntime ) {
        xl6CgenEmitDirRuntime(out, XL6_CGEN_DIR_ALL);
    }
    if ( hasStringEdgeRuntime ) {
        xl6CgenEmitStringEdgeRuntime(out);
    }
    if ( hasStdStringRuntime ) {
        xl6CgenEmitStdStringRuntime(out);
    }
    if ( hasPathRuntime ) {
        xl6CgenEmitPathRuntime(out);
    }
    if ( hasOsRuntime ) {
        xl6CgenEmitOsRuntime(out);
    }
    if ( hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionVarRuntime(out, hasTaskSpawn);
    }
    if ( hasBufferRuntime ) {
        xl6CgenEmitBufferRuntime(out, hasFileRuntime, 0, 1);
    }
    if ( hasHttp1Runtime ) {
        xl6CgenEmitHttp1Runtime(out, 0);
    }
    if ( hasHttpDecodeRuntime ) {
        xl6CgenEmitHttpDecodeRuntime(out);
    }
    if ( hasWsFrameRuntime || hasWsHandshakeRuntime || hasWsUpgradeRuntime ||
         hasWsStreamConfigRuntime || hasWsStreamRuntime ) {
        xl6CgenEmitWebSocketProtocolRuntime(
            out,
            hasWsFrameRuntime,
            hasWsHandshakeRuntime,
            hasWsUpgradeRuntime,
            hasWsStreamConfigRuntime,
            hasWsStreamRuntime,
            hasWsStreamTlsRuntime,
            0
        );
    }
    if ( hasNetRuntime ) {
        xl6CgenEmitNetExtendedRuntime(&ctx, &netNeeds);
    }
    if ( hasTlsRuntime ) {
        xl6CgenEmitTlsRuntime(
            out,
            hasTlsStreamRuntime,
            hasTlsDialRuntime,
            hasTlsServerRuntime,
            "xl6ValueHandleData",
            "xl6ValueHandleData",
            XL6_CGEN_TLS_FUTURE_LEGACY
        );
    }
    if ( hasEncodingRuntime ) {
        xl6CgenEmitEncodingRuntime(out);
    }
    if ( hasLogRuntime ) {
        xl6CgenEmitLogRuntime(out);
    }
    if ( hasSignalRuntime ) {
        xl6CgenEmitSignalRuntime(out, hasSignalChannelRuntime);
    }
    if ( hasTemplateRuntime ) {
        xl6CgenEmitTemplateRuntime(out);
    }
    if ( hasTccRuntime ) {
        xl6CgenEmitTccRuntime(out);
    }
    if ( hasXlangRuntime ) {
        xl6EmitXlangRuntimeComponent(out, 0);
    }
    if ( xl6CgenNodeNeedsIntPow(root) ) {
        xl6CgenEmitIntPowHelper(out);
    }
    if ( xl6CgenNodeNeedsRangeContains(root) ) {
        xl6CgenEmitRangeContainsHelper(out);
    }
    xl6CgenClassForwardDecls(&ctx, root);
    xl6CgenEnumDecls(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_UNION ) {
            xl6CgenUnionDecls(&ctx, root->children[i]);
        }
    }
    xl6CgenEmitNullableTypeDecls(&ctx, hasAsyncRuntime);
    xl6CgenEmitNullableTypeDescs(&ctx);
    xl6CgenEmitCAbiCallbackTypeDecls(&ctx);
    xl6CgenRegisterModuleGlobalTypes(&ctx, root);
    xl6CgenEmitModuleGlobalForwardDecls(&ctx, root);
    /* 泛型 class helper 可能读取模块常量，必须晚于模块值前置声明。 */
    xl6CgenCollectGenericClassDecls(&ctx, root);

    /*
        class 默认字段初始化会被写入默认构造 helper，而该 helper 早于普通函数定义。
        单返回值函数不依赖多返回值结构体，可在 class 结构体前向声明之后安全提前声明。
    */
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* child = root->children[i];
        const XL6_AstNode* returns;

        if ( child == NULL ) {
            continue;
        }
        returns = child->childCount > 2 ? child->children[2] : NULL;
        if ( returns != NULL && returns->childCount > 1 ) {
            continue;
        }
        if ( child->kind == XL6_AST_FUNC &&
             !xl6CgenFuncIsGeneric(child) &&
             !xl6CgenIsPrintIntrinsicFunc(child) ) {
            xl6CgenFuncProto(&ctx, child);
            xl6OutputPuts(";\n", out);
        } else if ( child->kind == XL6_AST_ASYNC_FUNC && !xl6CgenFuncIsGeneric(child) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, child) ) {
                xl6CgenFuncProto(&ctx, child);
            } else {
                xl6CgenAsyncFuncProto(&ctx, child);
            }
            xl6OutputPuts(";\n", out);
        } else if ( child->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportInternalProto(&ctx, child);
            xl6OutputPuts(";\n", out);
        } else if ( child->kind == XL6_AST_EXTERN_FUNC && !xl6CgenIsRuntimeHeaderExtern(child) ) {
            xl6CgenExternFuncProto(&ctx, child);
            xl6OutputPuts(";\n", out);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CLASS &&
             (!xl6CgenIsBuiltinObjectClass(root->children[i]) || needsObjectClass) ) {
            xl6CgenClassDecls(&ctx, root->children[i]);
        }
    }
    if ( hasRuntimeTypeOps ) {
        xl6CgenEmitRuntimeTypeHelpers(&ctx);
    }
    if ( hasXValueRuntime ) {
        xl6CgenEmitRecordFieldHelpers(&ctx);
        xl6CgenEmitProgramTypeNameHelper(&ctx);
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( (root->children[i]->kind == XL6_AST_FUNC || root->children[i]->kind == XL6_AST_EXPORT_FUNC) &&
             !xl6CgenFuncIsGeneric(root->children[i]) &&
             !xl6CgenIsPrintIntrinsicFunc(root->children[i]) ) {
            xl6CgenFuncReturnStructDecl(&ctx, root->children[i]);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        xl6CgenFuncReturnStructDecl(&ctx, ctx.genericInstances[i].func);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }
    xl6CgenFunctionReturnStructDecls(&ctx, root);
    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_EXTERN_FUNC ) {
            if ( !xl6CgenIsRuntimeHeaderExtern(root->children[i]) ) {
                xl6CgenExternFuncProto(&ctx, root->children[i]);
                xl6OutputPuts(";\n", out);
            }
            if ( xl6CgenExternCallNeedsWrapper(&ctx, root->children[i]) ) {
                xl6CgenExternCallWrapperProto(&ctx, root->children[i]);
                xl6OutputPuts(";\n", out);
            }
        } else if ( root->children[i]->kind == XL6_AST_EXTERN_VAR ) {
            xl6CgenExternVarDecl(&ctx, root->children[i]);
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportInternalProto(&ctx, root->children[i]);
            xl6OutputPuts(";\n", out);
            if ( root->children[i]->text != NULL && root->children[i]->text[0] != '\0' ) {
                xl6CgenExportWrapperProto(&ctx, root->children[i]);
                xl6OutputPuts(";\n", out);
            }
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_VAR ||
                    root->children[i]->kind == XL6_AST_EXPORT_CONST ) {
            if ( xl6CgenExternalVarHasCSymbol(root->children[i]) ) {
                xl6CgenExportVarDef(&ctx, root->children[i]);
            }
        } else if ( root->children[i]->kind == XL6_AST_FUNC &&
                    !xl6CgenFuncIsGeneric(root->children[i]) &&
                    !xl6CgenIsPrintIntrinsicFunc(root->children[i]) ) {
            xl6CgenFuncProto(&ctx, root->children[i]);
            xl6OutputPuts(";\n", out);
        } else if ( root->children[i]->kind == XL6_AST_ASYNC_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, root->children[i]) ) {
                xl6CgenFuncProto(&ctx, root->children[i]);
            } else {
                xl6CgenAsyncFuncProto(&ctx, root->children[i]);
            }
            xl6OutputPuts(";\n", out);
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        if ( xl6CgenIsAsyncFunc(ctx.genericInstances[i].func) &&
             !xl6CgenFuncReturnsGenerator(&ctx, ctx.genericInstances[i].func) ) {
            xl6CgenAsyncFuncProto(&ctx, ctx.genericInstances[i].func);
        } else {
            xl6CgenFuncProto(&ctx, ctx.genericInstances[i].func);
        }
        xl6OutputPuts(";\n", out);
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }
    xl6CgenGenericClassMethodProtos(&ctx);
    xl6CgenClassProtocolWrapperProtos(&ctx);
    xl6CgenUserProtocolProtos(&ctx);
    xl6CgenEmitCAbiCallbackBridges(&ctx);
    xl6CgenEmitCallableCAbiCallbackAdapters(&ctx);
    xl6CgenEmitAnonCAbiCallbackProtos(&ctx);
    xl6CgenEmitModuleGlobalDecls(&ctx, root);
    if ( root->childCount > 0 ) {
        xl6OutputPutc('\n', out);
    }

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_EXTERN_FUNC ) {
            xl6CgenExternCallWrapper(&ctx, root->children[i]);
        }
    }

    if ( hasFunctionRuntime || hasTaskSpawn || hasFunctionVarRuntime ) {
        xl6CgenEmitFunctionValueWrappers(&ctx);
    }
    xl6CgenEmitAnonFunctions(&ctx);
    xl6CgenEmitAnonCAbiCallbackDefs(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_CLASS &&
             (!xl6CgenIsBuiltinObjectClass(root->children[i]) || needsObjectClass) ) {
            xl6CgenClassMethods(&ctx, root->children[i]);
            xl6CgenEmitDynamicMethodDefinitions(&ctx, root->children[i]);
        }
    }

    xl6CgenGenericClassMethods(&ctx);
    xl6CgenClassProtocolWrappers(&ctx);
    xl6CgenUserProtocolDefinitions(&ctx);

    for ( i = 0; i < root->childCount; ++i ) {
        if ( root->children[i]->kind == XL6_AST_FUNC &&
             !xl6CgenFuncIsGeneric(root->children[i]) &&
             !xl6CgenIsPrintIntrinsicFunc(root->children[i]) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, root->children[i]) ) {
                xl6CgenGeneratorFunc(&ctx, root->children[i]);
            } else {
                xl6CgenFunc(&ctx, root->children[i]);
            }
        } else if ( root->children[i]->kind == XL6_AST_ASYNC_FUNC && !xl6CgenFuncIsGeneric(root->children[i]) ) {
            if ( xl6CgenFuncReturnsGenerator(&ctx, root->children[i]) ) {
                xl6CgenGeneratorFunc(&ctx, root->children[i]);
            } else {
                xl6CgenAsyncFunc(&ctx, root->children[i]);
            }
        } else if ( root->children[i]->kind == XL6_AST_EXPORT_FUNC ) {
            xl6CgenExportFunc(&ctx, root->children[i]);
            if ( root->children[i]->text != NULL && root->children[i]->text[0] != '\0' ) {
                xl6CgenExportWrapper(&ctx, root->children[i]);
            }
        }
    }
    for ( i = 0; i < ctx.genericInstanceCount; ++i ) {
        const XL6_AstNode* prevParams = ctx.currentGenericParams;
        const XL6_AstNode* prevArgs = ctx.currentGenericArgs;

        ctx.currentGenericParams = xl6CgenFuncTypeParams(ctx.genericInstances[i].func);
        ctx.currentGenericArgs = ctx.genericInstances[i].typeArgs;
        if ( xl6CgenFuncReturnsGenerator(&ctx, ctx.genericInstances[i].func) ) {
            xl6CgenGeneratorFunc(&ctx, ctx.genericInstances[i].func);
        } else if ( xl6CgenIsAsyncFunc(ctx.genericInstances[i].func) ) {
            xl6CgenAsyncFunc(&ctx, ctx.genericInstances[i].func);
        } else {
            xl6CgenFunc(&ctx, ctx.genericInstances[i].func);
        }
        ctx.currentGenericParams = prevParams;
        ctx.currentGenericArgs = prevArgs;
    }

    if ( hasEntryRuntime ) {
        xl6CgenEmitTopLevelXlangMain(&ctx, root, mainFunc, hasErrorRuntime, hasAsyncRuntime, hasOsRuntime);
    }
    if ( (mainFunc != NULL || hasEntryRuntime) && (options == NULL || !options->suppressMainWrapper) ) {
        xl6CgenEmitMainWrapper(&ctx);
    }
    if ( (mainFunc != NULL || hasEntryRuntime) && options != NULL && options->suppressMainWrapper ) {
        xl6CgenEmitScriptErrorBridge(&ctx);
    }

    hasTopLevel = ctx.ok && (diags == NULL || diags->count == 0);
    xl6CgenFreeVars(&ctx);
    xl6CgenFreeEmittedClasses(&ctx);
    xl6CgenFreeEmittedUnions(&ctx);
    xl6CgenFreeFunctionReturnStructs(&ctx);
    xrtFree(ctx.emittedCAbiCallbackTypes);
    xrtFree(ctx.emittedCAbiCallbackBridges);
    xl6CgenFreeCleanups(&ctx);
    xl6CgenFreeLoopTargets(&ctx);
    xl6CgenFreeTryTargets(&ctx);
    xl6CgenFreeFinallyTargets(&ctx);
    xl6CgenFreeGenericInstances(&ctx);
    xl6CgenFreeGenericClassInstances(&ctx);
    xl6CgenFreeGenericProtocolInstances(&ctx);
    xl6CgenFreeGenericEnumInstances(&ctx);
    xl6CgenFreeNullableTypes(&ctx);
    xl6CgenFreeRuntimeTypeDescs(&ctx);
    xl6CgenFreeAnonFuncs(&ctx);
    xl6CgenFreeOwnedTypeNodes(&ctx);
    xl6CgenFreeTextBlocks(&ctx);
    xl6CgenTypeCacheFree(&ctx);
    xl6CgenFreeRootTypeIndex(&ctx);
    xrtFree(ctx.coalesceSlots);
    return hasTopLevel;
}
