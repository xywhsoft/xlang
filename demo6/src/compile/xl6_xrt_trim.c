#include "src/compile/xl6_xrt_trim.h"

#include "src/cgen/xl6_cgen_needs_basic.h"

#include <string.h>

static int xl6XrtTrimTextStartsWith(const char* text, const char* prefix)
{
    size_t prefixLen;

    if ( text == NULL || prefix == NULL ) {
        return 0;
    }
    prefixLen = strlen(prefix);
    return strncmp(text, prefix, prefixLen) == 0;
}

static int xl6XrtTrimTextContains(const char* text, const char* needle)
{
    return text != NULL && needle != NULL && strstr(text, needle) != NULL;
}

static void xl6XrtTrimKeepValueStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepValue = 1;
}

static void xl6XrtTrimKeepTypedContainerStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepValueStack(cfg);
    cfg->keepTypedContainer = 1;
}

static void xl6XrtTrimKeepJsonStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepJson = 1;
}

static void xl6XrtTrimKeepNetworkStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepNetwork = 1;
}



static void xl6XrtTrimKeepNetInterface(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepNetInterface = 1;
}



static void xl6XrtTrimKeepNetInterfaceText(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetInterface(cfg);
    cfg->keepNetInterfaceText = 1;
}



static void xl6XrtTrimKeepNetDns(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepNetDns = 1;
}



static void xl6XrtTrimKeepNetResolver(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetDns(cfg);
    cfg->keepNetResolver = 1;
}



static void xl6XrtTrimKeepNetEngine(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepNetEngine = 1;
}



static void xl6XrtTrimKeepNetBuffer(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepNetBuffer = 1;
}



static void xl6XrtTrimKeepNetProxy(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepNetProxy = 1;
}



static void xl6XrtTrimKeepNetTcp(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetEngine(cfg);
    xl6XrtTrimKeepNetBuffer(cfg);
    cfg->keepNetTcp = 1;
}



static void xl6XrtTrimKeepNetTcpFile(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetTcp(cfg);
    cfg->keepFile = 1;
    cfg->keepNetTcpFile = 1;
}



static void xl6XrtTrimKeepNetTcpDial(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetTcp(cfg);
    xl6XrtTrimKeepNetResolver(cfg);
    cfg->keepNetTcpDial = 1;
}



static void xl6XrtTrimKeepNetTcpServer(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetTcp(cfg);
    cfg->keepNetTcpServer = 1;
}



static void xl6XrtTrimKeepNetUdp(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetEngine(cfg);
    xl6XrtTrimKeepNetBuffer(cfg);
    cfg->keepNetUdp = 1;
}



static void xl6XrtTrimKeepNetProxyDial(XL6_XrtTrimConfig* cfg)
{
    xl6XrtTrimKeepNetProxy(cfg);
    xl6XrtTrimKeepNetTcpDial(cfg);
    cfg->keepNetProxyDial = 1;
}

static void xl6XrtTrimKeepTlsStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepTls = 1;
}

static void xl6XrtTrimKeepTlsStreamStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepTlsStack(cfg);
    xl6XrtTrimKeepNetworkStack(cfg);
    xl6XrtTrimKeepNetBuffer(cfg);
    cfg->keepTlsStream = 1;
}

static void xl6XrtTrimKeepTlsDialStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepTlsStreamStack(cfg);
    xl6XrtTrimKeepNetTcpDial(cfg);
    cfg->keepTlsDial = 1;
}

static void xl6XrtTrimKeepTlsServerStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepTlsStreamStack(cfg);
    xl6XrtTrimKeepNetEngine(cfg);
    cfg->keepTlsServer = 1;
}



/* WebSocket 通信流复用 TCP、HTTP Upgrade 和事件等待能力。 */
static void xl6XrtTrimKeepWebSocketStreamStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetTcp(cfg);
    cfg->keepWebSocketUpgrade = 1;
    cfg->keepWebSocketStream = 1;
}



/* WSS 仅在显式使用 TLS 接管入口时增加 TLS 流依赖。 */
static void xl6XrtTrimKeepWebSocketStreamTlsStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepWebSocketStreamStack(cfg);
    xl6XrtTrimKeepTlsStreamStack(cfg);
    cfg->keepWebSocketStreamTls = 1;
}

static void xl6XrtTrimKeepHttpUtilStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepHttpUtil = 1;
}

static void xl6XrtTrimKeepCodecHex(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepCodecHex = 1;
}



static void xl6XrtTrimKeepCodecBase64(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepCodecBase64 = 1;
}



static void xl6XrtTrimKeepCodecPercent(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepCodecPercent = 1;
}



static void xl6XrtTrimKeepFormUrl(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepCodecPercent(cfg);
    cfg->keepFormUrl = 1;
}

static void xl6XrtTrimKeepCoroutineStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepCoroutine = 1;
}

static void xl6XrtTrimScanText(XL6_XrtTrimConfig* cfg, const char* text)
{
    if ( cfg == NULL || text == NULL || text[0] == '\0' ) {
        return;
    }

    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_tcc_") ||
         xl6XrtTrimTextStartsWith(text, "xl6_tcc_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xlang_") ||
         xl6XrtTrimTextStartsWith(text, "xl6_xlang_") ||
         strcmp(text, "TccContext") == 0 ||
         strcmp(text, "XLangContext") == 0 ) {
        /*
         * 脚本嵌入环境会把 XRT 暴露给内嵌 C/xlang 代码。第一期先使用完整 XRT，
         * 避免动态脚本调用静态分析不到的 XRT 函数时产生不可诊断的链接问题。
         */
        cfg->full = 1;
        return;
    }

    if ( strcmp(text, "future") == 0 ||
         strcmp(text, "task") == 0 ||
         strcmp(text, "generator") == 0 ||
         strcmp(text, "channel") == 0 ||
         strcmp(text, "thread") == 0 ||
         strcmp(text, "mutex") == 0 ||
         strcmp(text, "sem") == 0 ||
         strcmp(text, "cond") == 0 ||
         strcmp(text, "rwlock") == 0 ||
         xl6XrtTrimTextStartsWith(text, "xl6_task_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_task_") ) {
        xl6XrtTrimKeepCoroutineStack(cfg);
    }
    if ( strcmp(text, "rwlock") == 0 ) {
        cfg->keepRWLock = 1;
    }
    if ( strcmp(text, "thread") == 0 ) {
        cfg->keepThread = 1;
    }
    if ( strcmp(text, "mutex") == 0 ) {
        cfg->keepMutex = 1;
    }
    if ( strcmp(text, "sem") == 0 ) {
        cfg->keepSem = 1;
    }
    if ( strcmp(text, "cond") == 0 ) {
        cfg->keepCond = 1;
    }

    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_date_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_time_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_now") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_timer") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_log_") ) {
        cfg->keepTime = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_file_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_dir_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_path_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_dir_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_path_") ) {
        cfg->keepFile = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_file_") &&
         xl6XrtTrimTextContains(text, "_async") ) {
        cfg->keepFile = 1;
        cfg->keepFileAsync = 1;
        cfg->keepQueue = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_json_") ) {
        xl6XrtTrimKeepJsonStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_xson_") ) {
        cfg->keepXson = 1;
        xl6XrtTrimKeepJsonStack(cfg);
    }
    if ( strcmp(text, "__xl_std_print") == 0 ||
         strcmp(text, "__xl_std_println") == 0 ) {
        cfg->keepConsole = 1;
        cfg->keepXson = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_template_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_template_") ||
         strcmp(text, "Template") == 0 ) {
        cfg->keepTemplate = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_process_") ||
         strcmp(text, "Process") == 0 ||
         strcmp(text, "ProcessCommand") == 0 ||
         strcmp(text, "ProcessResult") == 0 ||
         strcmp(text, "PipelineResult") == 0 ) {
        cfg->keepSubprocess = 1;
        cfg->keepFile = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_log_") ||
         strcmp(text, "Logger") == 0 ||
         strcmp(text, "LogSink") == 0 ) {
        cfg->keepLogger = 1;
        cfg->keepFile = 1;
        cfg->keepTime = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_get_local_") ) {
        xl6XrtTrimKeepNetInterfaceText(cfg);
    }
    if ( strcmp(text, "NetInterfaceList") == 0 ||
         strcmp(text, "NetInterface") == 0 ||
         strcmp(text, "NetInterfaceAddress") == 0 ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_net_interface") ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_net_interfaces") ) {
        xl6XrtTrimKeepNetInterface(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_tcp_connect") ) {
        xl6XrtTrimKeepNetTcpDial(cfg);
    } else if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_tcp_listen") ) {
        xl6XrtTrimKeepNetTcpServer(cfg);
    } else if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_tcp_") ) {
        xl6XrtTrimKeepNetTcp(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_udp_") ) {
        xl6XrtTrimKeepNetUdp(cfg);
    }
    if ( strcmp(text, "TlsServer") == 0 ||
         strcmp(text, "TlsServerStats") == 0 ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_tls_server_") ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_tls_listen") ||
         xl6XrtTrimTextStartsWith(text, "xrtTlsListener") ) {
        xl6XrtTrimKeepTlsServerStack(cfg);
    } else if ( xl6XrtTrimTextStartsWith(text, "xl6_xrt_tls_connect") ||
         xl6XrtTrimTextStartsWith(text, "xrtTlsDial") ) {
        xl6XrtTrimKeepTlsDialStack(cfg);
    } else if ( strcmp(text, "TlsStream") == 0 ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_tls_stream_") ||
         xl6XrtTrimTextStartsWith(text, "xrtTlsStream") ) {
        xl6XrtTrimKeepTlsStreamStack(cfg);
    } else if ( strcmp(text, "TlsConfig") == 0 ||
                strcmp(text, "TlsSession") == 0 ||
                strcmp(text, "TlsResume") == 0 ||
                xl6XrtTrimTextStartsWith(text, "xl6_xrt_tls_") ||
                xl6XrtTrimTextStartsWith(text, "xrtTls") ) {
        xl6XrtTrimKeepTlsStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "xl6_xrt_ws_stream_attach_tls") ) {
        xl6XrtTrimKeepWebSocketStreamTlsStack(cfg);
    } else if ( strcmp(text, "WebSocket") == 0 ||
                (xl6XrtTrimTextStartsWith(text, "xl6_xrt_ws_stream_") &&
                 !xl6XrtTrimTextStartsWith(text, "xl6_xrt_ws_stream_config_")) ) {
        xl6XrtTrimKeepWebSocketStreamStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_percent_") ) {
        xl6XrtTrimKeepCodecPercent(cfg);
    }
    if ( strcmp(text, "hexEncode") == 0 ||
         strcmp(text, "hexDecode") == 0 ||
         strcmp(text, "hexEncodeBytes") == 0 ||
         strcmp(text, "hexDecodeBytes") == 0 ) {
        xl6XrtTrimKeepCodecHex(cfg);
    }
    if ( strcmp(text, "base64Encode") == 0 ||
         strcmp(text, "base64Decode") == 0 ||
         strcmp(text, "base64EncodeBytes") == 0 ||
         strcmp(text, "base64DecodeBytes") == 0 ) {
        xl6XrtTrimKeepCodecBase64(cfg);
    }
    if ( strcmp(text, "percentEncode") == 0 ||
         strcmp(text, "percentDecode") == 0 ) {
        xl6XrtTrimKeepCodecPercent(cfg);
    }
    if ( strcmp(text, "hexDecodeBytes") == 0 ) {
        cfg->keepBuffer = 1;
        cfg->keepBufferHex = 1;
    }
    if ( strcmp(text, "base64DecodeBytes") == 0 ) {
        cfg->keepBuffer = 1;
        cfg->keepBufferBase64 = 1;
    }
    if ( strcmp(text, "__xl_std_xrt_buffer_from_hex") == 0 ) {
        cfg->keepBuffer = 1;
        cfg->keepBufferHex = 1;
        xl6XrtTrimKeepCodecHex(cfg);
    }
    if ( strcmp(text, "__xl_std_xrt_buffer_from_base64") == 0 ) {
        cfg->keepBuffer = 1;
        cfg->keepBufferBase64 = 1;
        xl6XrtTrimKeepCodecBase64(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_encoding_") ) {
        cfg->keepEncoding = 1;
        /* charset/encoding 暂无独立裁剪宏，保持核心实现可见即可。 */
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_sha") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_hmac") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_chacha") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_aes") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_hkdf") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_random_hex") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_ed25519") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_x25519") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_x448") ||
        xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_rsa_") ) {
        cfg->keepCrypto = 1;
        xl6XrtTrimKeepCodecHex(cfg);
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_xid_make_string") ) {
        cfg->keepXid = 1;
        cfg->keepTime = 1;
    }
    if ( strcmp(text, "Buffer") == 0 ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_buffer_") ) {
        cfg->keepBuffer = 1;
    }
    if ( strcmp(text, "var") == 0 ||
         strcmp(text, "array") == 0 ||
         strcmp(text, "list") == 0 ||
         strcmp(text, "set") == 0 ||
         strcmp(text, "dict") == 0 ||
         xl6XrtTrimTextStartsWith(text, "array<") ||
         xl6XrtTrimTextStartsWith(text, "list<") ||
         xl6XrtTrimTextStartsWith(text, "set<") ||
         xl6XrtTrimTextStartsWith(text, "dict<") ) {
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( strcmp(text, "array") == 0 ||
         strcmp(text, "list") == 0 ||
         strcmp(text, "set") == 0 ||
         strcmp(text, "dict") == 0 ||
         xl6XrtTrimTextStartsWith(text, "array<") ||
         xl6XrtTrimTextStartsWith(text, "list<") ||
         xl6XrtTrimTextStartsWith(text, "set<") ||
         xl6XrtTrimTextStartsWith(text, "dict<") ) {
        xl6XrtTrimKeepTypedContainerStack(cfg);
    }
}

static void xl6XrtTrimScanNode(XL6_XrtTrimConfig* cfg, const XL6_AstNode* node)
{
    size_t i;

    if ( cfg == NULL || node == NULL || cfg->full ) {
        return;
    }
    if ( node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_AWAIT ||
         node->kind == XL6_AST_YIELD ) {
        xl6XrtTrimKeepCoroutineStack(cfg);
    }
    if ( node->kind == XL6_AST_MEMBER &&
         node->text != NULL &&
         (strcmp(node->text, "attachTls") == 0 ||
          strcmp(node->text, "attachTlsWithConfig") == 0) &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         node->children[0]->text != NULL &&
         strcmp(node->children[0]->text, "WebSocket") == 0 ) {
        xl6XrtTrimKeepWebSocketStreamTlsStack(cfg);
    }
    xl6XrtTrimScanText(cfg, node->text);
    if ( cfg->full ) {
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6XrtTrimScanNode(cfg, node->children[i]);
        if ( cfg->full ) {
            return;
        }
    }
}

static void xl6XrtTrimScanGeneratedSource(XL6_XrtTrimConfig* cfg, const char* source)
{
    if ( cfg == NULL || source == NULL || source[0] == '\0' || cfg->full ) {
        return;
    }

    /*
     * 标准库声明会在 cgen 阶段注入，原始 AST 看不到这些真实 XRT 符号。
     * 这里按生成后的 C 源再补一轮依赖，只用于裁剪 XRT 实现，不改变语言语义。
     */
    if ( xl6XrtTrimTextContains(source, "__xl_std_tcc_") ||
         xl6XrtTrimTextContains(source, "xl6_tcc_") ||
         xl6XrtTrimTextContains(source, "__xl_std_xlang_") ||
         xl6XrtTrimTextContains(source, "xl6_xlang_") ) {
        cfg->full = 1;
        return;
    }

    if ( xl6XrtTrimTextContains(source, "xvo") ||
         xl6XrtTrimTextContains(source, "xvalue") ) {
        xl6XrtTrimKeepValueStack(cfg);
    }

    /* Value 集合算法保持独立裁剪；继承类的 derive value 会直接生成对象合并调用。 */
    if ( xl6XrtTrimTextContains(source, "xrtValueArrayExtend") ||
         xl6XrtTrimTextContains(source, "xrtValueArrayConcat") ||
         xl6XrtTrimTextContains(source, "xrtValueIntMapMerge") ||
         xl6XrtTrimTextContains(source, "xrtValueObjectMerge") ||
         xl6XrtTrimTextContains(source, "xrtValueSetMerge") ||
         xl6XrtTrimTextContains(source, "xrtValueSetUnion") ||
         xl6XrtTrimTextContains(source, "xrtValueSetIntersection") ||
         xl6XrtTrimTextContains(source, "xrtValueSetDifference") ||
         xl6XrtTrimTextContains(source, "xrtValueSetSymmetricDifference") ||
         xl6XrtTrimTextContains(source, "xrtValueSetIsSubset") ||
         xl6XrtTrimTextContains(source, "xrtValueSetIsSuperset") ||
         xl6XrtTrimTextContains(source, "xrtValueSetIsDisjoint") ||
         xl6XrtTrimTextContains(source, "xrtValueSetEqual") ) {
        xl6XrtTrimKeepValueStack(cfg);
        cfg->keepValueCollection = 1;
    }

	/* 类句柄分类单独依赖运行时对象，普通动态值不应承担这部分闭包。 */
	if ( xl6XrtTrimTextContains(
		source,
		"#define XL_RUNTIME_NEED_CLASS_HANDLE 1"
	) ) {
		cfg->keepRuntimeObject = 1;
	}
	if ( xl6XrtTrimTextContains(
		source,
		"#define XRUNTIME_MODULE_RUNTIME_VALUE_WEAK 1"
	) ) {
		cfg->keepRuntimeWeak = 1;
	}
	if ( xl6XrtTrimTextContains(
		source,
		"#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD 1"
	) || xl6XrtTrimTextContains(source, "xrtDynamicFields") ) {
		cfg->keepDynamicFields = 1;
	}

    if ( xl6XrtTrimTextContains(source, "xrtConsoleWrite") ) {
        cfg->keepConsole = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtIntFormat") ||
         xl6XrtTrimTextContains(source, "xrtUIntFormat") ||
         xl6XrtTrimTextContains(source, "xrtNumFormat") ) {
        cfg->keepJnum = 1;
    }

    if ( xl6XrtTrimTextContains(
        source,
        "#define XL_RUNTIME_NEED_TYPED_CONTAINER 1"
    ) ) {
        xl6XrtTrimKeepTypedContainerStack(cfg);
    }

    if ( xl6XrtTrimTextContains(
        source,
        "#define XL_RUNTIME_NEED_TYPE_STRING_VALUE 1"
    ) ) {
        cfg->keepTypeStringValue = 1;
    }

    /* 特殊集合桥只保留生成后 C 源真正引用的并发拓扑和容器实现。 */
    if ( xl6XrtTrimTextContains(source, "xrtTypedStack") ) {
        cfg->keepTypedStack = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtTypedTree") ) {
        cfg->keepTypedTree = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtTypedSPSCQueue") ) {
        cfg->keepTypedQueueSPSC = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtTypedMPSCQueue") ) {
        cfg->keepTypedQueueMPSC = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtTypedMPMCQueue") ) {
        cfg->keepTypedQueueMPMC = 1;
    }

    /* WaitQueue 桥直接使用同步原语，原始 AST 中没有这些内部依赖。 */
    if ( xl6XrtTrimTextContains(source, "xrtMutex") ) {
        cfg->keepMutex = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtCond") ) {
        cfg->keepCond = 1;
    }

    /* Resident box-drop implementation is a private source dependency: its
     * nested Future/xvalue releases are not present in the generated unit. */
    if (xl6XrtTrimTextContains(source, "xl6Future") ||
        xl6XrtTrimTextContains(source, "xl6PayloadDropGet") ||
        xl6XrtTrimTextContains(source, "xl6PayloadTraceGet") ||
        xl6XrtTrimTextContains(source, "xl6PayloadResolveOwned") ||
        xl6XrtTrimTextContains(source, "xl6PayloadObserveOutcome")) {
        xl6XrtTrimKeepValueStack(cfg);
        xl6XrtTrimKeepCoroutineStack(cfg);
    }
    /* 用户 protocol 会额外编入 registry 私有源；其互斥锁调用不出现在生成单元中。 */
    if (xl6XrtTrimTextContains(source, "#define XLIR_RUNTIME_CONTINUATION 1")) {
        cfg->keepMutex = 1;
        cfg->keepCond = 1;
        cfg->keepThread = 1;
        xl6XrtTrimKeepCoroutineStack(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xl6Protocol") ) {
        cfg->keepMutex = 1;
    }

    /* 动态 xvalue 运算只依赖基础字符串连接，不应拉入完整标准字符串工具组。 */
	if ( xl6XrtTrimTextContains(
		source,
		"#define XL_RUNTIME_NEED_XVALUE_BINARY 1"
	) ) {
		cfg->keepStringCore = 1;
	}

    /* 标准字符串运行时由 cgen 注入，必须从最终 C 源补齐其 XRT 能力。 */
    if ( xl6XrtTrimTextContains(source, "xrtStrSplit") ||
         xl6XrtTrimTextContains(source, "xrtStrGlob") ) {
        cfg->keepStdString = 1;
    }

    /* 近似比较的相似度调用位于公共运行时头，生成单元中只保留桥接函数名。 */
    if ( xl6XrtTrimTextContains(source, "xrtUtf8Similarity") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_basic_approx") ) {
        cfg->keepUnicodeDistance = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xl6_xrt_utf8_char_size") ||
         xl6XrtTrimTextContains(source, "xrtUtf8Decode") ) {
        cfg->keepUnicode = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xl6_xrt_encoding_") ||
         xl6XrtTrimTextContains(source, "xrtEncodingGuess") ||
         xl6XrtTrimTextContains(source, "xrtTranscode") ) {
        cfg->keepEncoding = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtDate") ||
         xl6XrtTrimTextContains(source, "xrtTime") ||
         xl6XrtTrimTextContains(source, "xrtNow") ||
         xl6XrtTrimTextContains(source, "xrtTimer") ) {
        cfg->keepTime = 1;
    }

    /* 扩展数学 helper 集中在独立 XRT math 模块，普通算术和 CRT 数学函数不启用它。 */
    if ( xl6XrtTrimTextContains(source, "xrtMath") ) {
        cfg->keepMath = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtFile") ||
         xl6XrtTrimTextContains(source, "xrtDir") ||
         xl6XrtTrimTextContains(source, "xrtPath") ) {
        cfg->keepFile = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtJson") ) {
        xl6XrtTrimKeepJsonStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtXson") ) {
        cfg->keepXson = 1;
        xl6XrtTrimKeepJsonStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtHex") ) {
        xl6XrtTrimKeepCodecHex(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtBase64") ) {
        xl6XrtTrimKeepCodecBase64(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtBuffer") ) {
        cfg->keepBuffer = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtBufferFromHex") ) {
        cfg->keepBufferHex = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtBufferFromBase64") ) {
        cfg->keepBufferBase64 = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtPercentEncode") ||
         xl6XrtTrimTextContains(source, "xrtPercentDecode") ||
         xl6XrtTrimTextContains(source, "xrtFormEncode") ||
         xl6XrtTrimTextContains(source, "xrtFormDecode") ||
         xl6XrtTrimTextContains(source, "xrtFormParse") ||
         xl6XrtTrimTextContains(source, "xrtFormWrite") ||
         xl6XrtTrimTextContains(source, "xrtFormBuild") ||
         xl6XrtTrimTextContains(source, "xrtFormFind")) {
        xl6XrtTrimKeepFormUrl(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtPercentEncode") ||
         xl6XrtTrimTextContains(source, "xrtPercentDecode") ) {
        xl6XrtTrimKeepCodecPercent(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtHttp1Message") ) {
        cfg->keepHttp1Codec = 1;
    } else if ( xl6XrtTrimTextContains(source, "xrtHttp1") ) {
        cfg->keepHttp1Core = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtHttpDecode") ) {
        cfg->keepHttpDecode = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtWsFrame") ) {
        cfg->keepWebSocketCodec = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtWsClose") ) {
        cfg->keepWebSocketCodec = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtWsKey") ||
         xl6XrtTrimTextContains(source, "xrtWsAccept") ||
         xl6XrtTrimTextContains(source, "xrtWsProtocol") ) {
        cfg->keepWebSocketHandshake = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtWsUpgrade") ) {
        cfg->keepWebSocketUpgrade = 1;
    }
    if ( xl6XrtTrimTextContains(source, "xrtWsStreamAttachTls") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_ws_stream_attach_tls") ) {
        xl6XrtTrimKeepWebSocketStreamTlsStack(cfg);
    } else if ( xl6XrtTrimTextContains(source, "xrtWsStreamAttachTcp") ||
                xl6XrtTrimTextContains(source, "xl6_xrt_ws_stream_attach_tcp") ) {
        xl6XrtTrimKeepWebSocketStreamStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtSha") ||
         xl6XrtTrimTextContains(source, "xrtHmac") ||
         xl6XrtTrimTextContains(source, "xrtHkdf") ||
         xl6XrtTrimTextContains(source, "xrtChaCha") ||
         xl6XrtTrimTextContains(source, "xrtAes") ||
         xl6XrtTrimTextContains(source, "xrtEd25519") ||
         xl6XrtTrimTextContains(source, "xrtX25519") ||
         xl6XrtTrimTextContains(source, "xrtX448") ||
         xl6XrtTrimTextContains(source, "xrtRsa") ) {
        cfg->keepCrypto = 1;
        xl6XrtTrimKeepCodecHex(cfg);
        xl6XrtTrimKeepValueStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtTlsListener") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_tls_server_") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_tls_listen") ) {
        xl6XrtTrimKeepTlsServerStack(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtTlsDial") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_tls_connect") ) {
        xl6XrtTrimKeepTlsDialStack(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtTlsStream") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_tls_stream_") ||
         xl6XrtTrimTextContains(source, "xl6_tls_stream_") ) {
        xl6XrtTrimKeepTlsStreamStack(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtTls") ||
                xl6XrtTrimTextContains(source, "xl6_xrt_tls_") ) {
        xl6XrtTrimKeepTlsStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtNetLocal") ) {
        xl6XrtTrimKeepNetInterfaceText(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetInterface") ||
         xl6XrtTrimTextContains(source, "xrtNetInterfaces") ) {
        xl6XrtTrimKeepNetInterface(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetResolver") ||
         xl6XrtTrimTextContains(source, "xrtNetResolveAsync") ) {
        xl6XrtTrimKeepNetResolver(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetResolve(") ||
         xl6XrtTrimTextContains(source, "xrtNetResolveOne") ) {
        xl6XrtTrimKeepNetDns(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetProxyDial") ) {
        xl6XrtTrimKeepNetProxyDial(cfg);
    } else if ( xl6XrtTrimTextContains(source, "xrtNetProxy") ) {
        xl6XrtTrimKeepNetProxy(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetDial") ||
         xl6XrtTrimTextContains(source, "xrtNetConnect") ) {
        xl6XrtTrimKeepNetTcpDial(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetListener") ||
         xl6XrtTrimTextContains(source, "xrtNetListen") ) {
        xl6XrtTrimKeepNetTcpServer(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetStreamSendFile") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_tcp_stream_send_file_") ) {
        xl6XrtTrimKeepNetTcpFile(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetStream") ) {
        xl6XrtTrimKeepNetTcp(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetUdp") ) {
        xl6XrtTrimKeepNetUdp(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetBytes") ) {
        xl6XrtTrimKeepNetBuffer(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNetEngine") ) {
        xl6XrtTrimKeepNetEngine(cfg);
    }
    if ( xl6XrtTrimTextContains(source, "xrtNet") ) {
        xl6XrtTrimKeepNetworkStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtHttp") &&
         !xl6XrtTrimTextContains(source, "xrtHttp1") &&
         !xl6XrtTrimTextContains(source, "xrtHttpDecode") ) {
        xl6XrtTrimKeepHttpUtilStack(cfg);
    }


    if ( xl6XrtTrimTextContains(source, "xrtProcess") ) {
        cfg->keepSubprocess = 1;
        cfg->keepFile = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtLog") ) {
        cfg->keepLogger = 1;
        cfg->keepFile = 1;
        cfg->keepTime = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtTemplate") ||
         xl6XrtTrimTextContains(source, "xteCreate") ||
         xl6XrtTrimTextContains(source, "xteCompile") ||
         xl6XrtTrimTextContains(source, "xteRender") ) {
        cfg->keepTemplate = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtXid") ) {
        cfg->keepXid = 1;
        cfg->keepTime = 1;
    }

}

static void xl6XrtTrimApplyBasicNeeds(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root)
{
    if ( cfg == NULL || root == NULL ) {
        return;
    }
    if ( xl6CgenNodeNeedsTimeRuntime(root) ) {
        cfg->keepTime = 1;
    }
    if ( xl6CgenNodeNeedsXValueRuntime(root) ) {
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6CgenNodeNeedsClassHandleRuntime(root) ) {
        cfg->keepAtomic = 1;
    }
    if ( xl6CgenNodeNeedsWeakRuntime(root) ) {
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6CgenNodeNeedsStdStringRuntime(root) ) {
        cfg->keepStdString = 1;
    }
    if ( xl6CgenNodeNeedsPrintRuntime(root) ) {
        cfg->keepConsole = 1;
        cfg->keepXson = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6CgenNodeNeedsCryptoRuntime(root) ) {
        cfg->keepCrypto = 1;
        xl6XrtTrimKeepCodecHex(cfg);
    }
    if ( xl6CgenNodeNeedsProcessRuntime(root) ) {
        cfg->keepSubprocess = 1;
        cfg->keepFile = 1;
    }
    if ( xl6CgenNodeNeedsDirRuntime(root) ||
         xl6CgenNodeNeedsPathRuntime(root) ||
         xl6CgenNodeNeedsFileRuntime(root) ) {
        cfg->keepFile = 1;
    }
    if ( xl6CgenNodeNeedsAsyncFileRuntime(root) ) {
        cfg->keepFile = 1;
        cfg->keepFileAsync = 1;
        cfg->keepQueue = 1;
    }
    if ( xl6CgenNodeNeedsLogRuntime(root) ) {
        cfg->keepLogger = 1;
        cfg->keepTime = 1;
    }
    if ( xl6CgenNodeNeedsTemplateRuntime(root) ) {
        cfg->keepTemplate = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6CgenNodeNeedsBufferRuntime(root) ) {
        cfg->keepBuffer = 1;
    }
}

void xl6XrtTrimConfigInit(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
	memset(cfg, 0, sizeof(*cfg));
	/*
		每个生成产物都公开 xlValueRetain/Release/Clone/IsNull/Data。
		这些稳定 C ABI 入口必须保留深复制、语言记录和原生句柄的完整语义，
		因此 Value 桥接栈属于 xlang 的固定最小运行时，而不是源码启发式能力。
	*/
	xl6XrtTrimKeepValueStack(cfg);
}

static int xl6XrtTrimPackageRequiresFull(const XL6_ImportDeps* deps)
{
    /* A native package without a declared module contract is explicitly
     * unbounded. Runtime/compiler embedding also requests the full product. */
    return deps && (deps->hasUndeclaredCSourceModules ||
        (deps->features & (XL6_IMPORT_FEATURE_TCC_RUNTIME |
            XL6_IMPORT_FEATURE_XLANG_RUNTIME | XL6_IMPORT_FEATURE_XLANG_COMPILER)));
}

void xl6XrtTrimAnalyze(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimConfigInit(cfg);
    cfg->packageXrtModules = deps != NULL ? &deps->xrtModules : NULL;
    if ( xl6XrtTrimPackageRequiresFull(deps) ) {
        cfg->full = 1;
        return;
    }
    xl6XrtTrimApplyBasicNeeds(cfg, root);
    xl6XrtTrimScanNode(cfg, root);
}

void xl6XrtTrimAnalyzeWithGeneratedSource(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimAnalyze(cfg, root, deps);
    xl6XrtTrimScanGeneratedSource(cfg, generatedSource);
    if ( generatedSource != NULL &&
         !xl6XrtTrimTextContains(generatedSource, "xrtTypedArray") &&
         !xl6XrtTrimTextContains(generatedSource, "xrtTypedList") &&
         !xl6XrtTrimTextContains(generatedSource, "xrtTypedSet") &&
         !xl6XrtTrimTextContains(generatedSource, "xrtTypedDict") &&
         !xl6XrtTrimTextContains(generatedSource, "xl6TypedArray") &&
         !xl6XrtTrimTextContains(generatedSource, "xl6TypedList") &&
         !xl6XrtTrimTextContains(generatedSource, "xl6TypedSet") &&
         !xl6XrtTrimTextContains(generatedSource, "xl6TypedDict") ) {
        cfg->keepTypedContainer = 0;
    }
}

/* 控制台原始文本输出公开给语言的根模块。 */
static const char* const xl6XrtTrimConsoleModules[] = {
    "XRT_MODULE_CONSOLE"
};

/* 时间能力公开给语言的根模块。 */
static const char* const xl6XrtTrimTimeModules[] = {
    "XRT_MODULE_TIME_TEXT",
    "XRT_MODULE_TIME_LOCAL"
};

/* 文件能力公开给语言的根模块。 */
static const char* const xl6XrtTrimFileModules[] = {
    "XRT_MODULE_FILE_TEXT",
    "XRT_MODULE_FILE_TREE",
    "XRT_MODULE_FILE_ROOT",
    "XRT_MODULE_FILE_FIFO",
    "XRT_MODULE_FILE_LOCK",
    "XRT_MODULE_FILE_MAP",
    "XRT_MODULE_RANDOM_TEXT_DEFAULT"
};

/* 异步文件能力公开给语言的根模块。 */
static const char* const xl6XrtTrimFileAsyncModules[] = {
	"XRT_MODULE_FILE_ASYNC",
    "XRT_MODULE_FILE_ASYNC_WHOLE",
    "XRT_MODULE_FILE_ASYNC_MANAGE",
	"XRT_MODULE_DIR_ASYNC",
    "XRT_MODULE_FILE_TREE_ASYNC"
};

/* 队列能力公开给语言的根模块。 */
static const char* const xl6XrtTrimQueueModules[] = {
    "XRT_MODULE_QUEUE_SPSC",
    "XRT_MODULE_QUEUE_MPSC",
    "XRT_MODULE_QUEUE_MPMC"
};

/* 语言类描述的进程期 xrttype 缓存使用独立原子槽。 */
static const char* const xl6XrtTrimAtomicModules[] = {
    "XRT_MODULE_ATOMIC"
};

/* 协程、Future、任务和 Channel 公开给语言的根模块。 */
/* 协程、任务和私有异步桥接实现依赖的完整根模块。 */
static const char* const xl6XrtTrimCoroutineModules[] = {
    "XRT_MODULE_TASK_GROUP_POOL",
    "XRT_MODULE_TASK_GROUP_COROUTINE",
    "XRT_MODULE_CHANNEL_SELECT_CANCEL",
    "XRT_MODULE_CHANNEL_COROUTINE",
    "XRT_MODULE_FUTURE_CONTINUE",
    "XRT_MODULE_FUTURE_COMBINE",
    "XRT_MODULE_COROUTINE_EVENT",
    "XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE"
};

/* 读写锁只在语言程序实际声明 rwlock 时进入独立产物。 */
static const char* const xl6XrtTrimRWLockModules[] = {
    "XRT_MODULE_RWLOCK"
};

/* 线程句柄只在语言程序显式使用 thread 时进入独立产物。 */
static const char* const xl6XrtTrimThreadModules[] = {
    "XRT_MODULE_THREAD"
};

/* 互斥锁只在语言程序显式使用 mutex 时进入独立产物。 */
static const char* const xl6XrtTrimMutexModules[] = {
    "XRT_MODULE_MUTEX"
};

/* 信号量只在语言程序显式使用 sem 时进入独立产物。 */
static const char* const xl6XrtTrimSemModules[] = {
    "XRT_MODULE_SEM"
};

/* 条件变量只在语言程序显式使用 cond 时进入独立产物。 */
static const char* const xl6XrtTrimCondModules[] = {
    "XRT_MODULE_COND"
};

/* TCP、UDP、DNS、代理和 framing 公开给语言的根模块。 */
static const char* const xl6XrtTrimNetworkModules[] = {
    "XRT_MODULE_NET"
};

static const char* const xl6XrtTrimNetInterfaceModules[] = {
    "XRT_MODULE_NET_INTERFACE"
};

static const char* const xl6XrtTrimNetInterfaceTextModules[] = {
    "XRT_MODULE_NET_INTERFACE_TEXT"
};

static const char* const xl6XrtTrimNetDnsModules[] = {
    "XRT_MODULE_NET_DNS"
};

static const char* const xl6XrtTrimNetResolverModules[] = {
    "XRT_MODULE_NET_RESOLVER_FUTURE"
};

static const char* const xl6XrtTrimNetEngineModules[] = {
    "XRT_MODULE_NET_ENGINE"
};

static const char* const xl6XrtTrimNetBufferModules[] = {
    "XRT_MODULE_NET_BUFFER"
};

static const char* const xl6XrtTrimNetProxyModules[] = {
    "XRT_MODULE_NET_PROXY"
};

static const char* const xl6XrtTrimNetProxyDialModules[] = {
    "XRT_MODULE_NET_PROXY_DIAL"
};

static const char* const xl6XrtTrimNetTcpModules[] = {
    "XRT_MODULE_NET_TCP_SYNC",
    "XRT_MODULE_EVENT"
};

static const char* const xl6XrtTrimNetTcpFileModules[] = {
    "XRT_MODULE_NET_TCP_FILE"
};

static const char* const xl6XrtTrimNetTcpDialModules[] = {
    "XRT_MODULE_NET_TCP_DIAL_SYNC"
};

static const char* const xl6XrtTrimNetTcpServerModules[] = {
    "XRT_MODULE_NET_TCP_SERVER_SYNC"
};

static const char* const xl6XrtTrimNetUdpModules[] = {
    "XRT_MODULE_NET_UDP_SYNC"
};

/* XRT 核心 HTTP 协议解析、传输判定与内容解码根模块。 */
static const char* const xl6XrtTrimHttpProtocolModules[] = {
    "XRT_MODULE_HTTP_TARGET",
    "XRT_MODULE_HTTP_CONNECTION",
    "XRT_MODULE_HTTP_EXPECT",
    "XRT_MODULE_HTTP_TE",
    "XRT_MODULE_HTTP_TRAILER",
    "XRT_MODULE_HTTP_UPGRADE_WRITE",
    "XRT_MODULE_HTTP_ENCODING",
    "XRT_MODULE_HTTP_DECODE"
};

/* 基础字节、表单和协议 codec 彼此独立裁剪。 */
static const char* const xl6XrtTrimCodecHexModules[] = {
    "XRT_MODULE_CODEC_HEX"
};

static const char* const xl6XrtTrimCodecBase64Modules[] = {
    "XRT_MODULE_CODEC_BASE64"
};

static const char* const xl6XrtTrimBufferHexModules[] = {
    "XRT_MODULE_BUFFER_HEX"
};

static const char* const xl6XrtTrimBufferBase64Modules[] = {
    "XRT_MODULE_BUFFER_BASE64"
};

static const char* const xl6XrtTrimCodecPercentModules[] = {
    "XRT_MODULE_CODEC_PERCENT"
};

static const char* const xl6XrtTrimFormUrlModules[] = {
    "XRT_MODULE_FORM_URLENCODED"
};

static const char* const xl6XrtTrimHttp1CodecModules[] = {
    "XRT_MODULE_HTTP1_MESSAGE"
};

static const char* const xl6XrtTrimHttp1CoreModules[] = {
    "XRT_MODULE_HTTP1_BODY"
};

static const char* const xl6XrtTrimHttpDecodeModules[] = {
    "XRT_MODULE_HTTP_DECODE"
};

static const char* const xl6XrtTrimWebSocketCodecModules[] = {
    "XRT_MODULE_WEBSOCKET_FRAME",
    "XRT_MODULE_WEBSOCKET_CLOSE"
};

static const char* const xl6XrtTrimWebSocketHandshakeModules[] = {
    "XRT_MODULE_WEBSOCKET_KEYGEN"
};

static const char* const xl6XrtTrimWebSocketUpgradeModules[] = {
    "XRT_MODULE_WEBSOCKET_UPGRADE_DEFLATE"
};

/* WS 数据通道包含引用、压缩和跨 Worker 同步命令所需的事件对象。 */
static const char* const xl6XrtTrimWebSocketStreamModules[] = {
    "XRT_MODULE_WEBSOCKET_STREAM_REF",
    "XRT_MODULE_WEBSOCKET_STREAM_DEFLATE",
    "XRT_MODULE_EVENT"
};

/* TLS 接管是独立能力，普通 WS 程序不应携带 TLS。 */
static const char* const xl6XrtTrimWebSocketStreamTlsModules[] = {
    "XRT_MODULE_WEBSOCKET_STREAM_TLS"
};

/* 密码学标准库公开的算法根模块。 */
static const char* const xl6XrtTrimCryptoModules[] = {
    "XRT_MODULE_CRYPTO_SHA1",
    "XRT_MODULE_CRYPTO_SHA256",
    "XRT_MODULE_CRYPTO_SHA512",
    "XRT_MODULE_CRYPTO_RSA_PKCS1",
    "XRT_MODULE_CRYPTO_RSA_PSS",
    "XRT_MODULE_CRYPTO_ECDSA_P256_DER",
    "XRT_MODULE_CRYPTO_ECDSA_P384_DER",
    "XRT_MODULE_CRYPTO_ED25519_KEYPAIR",
    "XRT_MODULE_CRYPTO_ED25519_SIGN",
    "XRT_MODULE_CRYPTO_ED25519_VERIFY",
    "XRT_MODULE_CRYPTO_X25519_KEYPAIR",
    "XRT_MODULE_CRYPTO_X448_KEYPAIR",
    "XRT_MODULE_CRYPTO_P256_KEYPAIR",
    "XRT_MODULE_CRYPTO_P384_KEYPAIR",
    "XRT_MODULE_CRYPTO_AES_GCM",
    "XRT_MODULE_CRYPTO_CHACHA20_POLY1305",
    "XRT_MODULE_CRYPTO_HKDF_SHA256",
    "XRT_MODULE_CRYPTO_HKDF_SHA512",
    "XRT_MODULE_CRYPTO_HMAC_SHA256",
    "XRT_MODULE_CRYPTO_HMAC_SHA512"
};

/* TLS 客户端、服务器和流公开给语言的根模块。 */
static const char* const xl6XrtTrimTlsModules[] = {
    "XRT_MODULE_TLS_SERVER_RESUME",
    "XRT_MODULE_TLS_IDENTITY_RSA",
    "XRT_MODULE_TLS_IDENTITY_P256",
    "XRT_MODULE_TLS_IDENTITY_P384",
    "XRT_MODULE_TLS_IDENTITY_ED25519",
    "XRT_MODULE_TLS_CLIENT_RESUME",
    "XRT_MODULE_TLS_RECORD_AES",
    "XRT_MODULE_TLS_RECORD_CHACHA",
    "XRT_MODULE_TLS_SCHEDULE_SHA256",
    "XRT_MODULE_TLS_SCHEDULE_SHA384",
    "XRT_MODULE_TLS_KEY_EXCHANGE_X25519",
    "XRT_MODULE_TLS_KEY_EXCHANGE_X448",
    "XRT_MODULE_TLS_KEY_EXCHANGE_P256",
    "XRT_MODULE_TLS_KEY_EXCHANGE_P384",
    "XRT_MODULE_X509_STORE_FILE",
    "XRT_MODULE_X509_STORE_SYSTEM"
};

/* TLS 网络流在协议核心之上增加 TCP 拨号与有界异步收发。 */
static const char* const xl6XrtTrimTlsStreamModules[] = {
    "XRT_MODULE_TLS_STREAM_FUTURE",
    "XRT_MODULE_EVENT"
};

/* TLS 瀹㈡埛绔嫧鍙峰彧鐢?connect/connectAsync 璺緞寮曞叆銆?*/
static const char* const xl6XrtTrimTlsDialModules[] = {
    "XRT_MODULE_TLS_STREAM_DIAL_FUTURE"
};

/* TLS 服务端只在语言实际使用监听与接受对象时进入裁剪闭包。 */
static const char* const xl6XrtTrimTlsServerModules[] = {
    "XRT_MODULE_TLS_STREAM_LISTENER_SYNC"
};

/* Buffer 文本编解码公开给语言的根模块。 */
static const char* const xl6XrtTrimBufferModules[] = {
    "XRT_MODULE_BUFFER"
};

/* 动态值能力公开给语言的根模块。 */
static const char* const xl6XrtTrimValueModules[] = {
	"XRT_MODULE_VALUE_GRAPH",
	"XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE",
	"XRUNTIME_MODULE_RUNTIME_VALUE_TYPE",
	"XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE",
	"XRUNTIME_MODULE_RUNTIME_CONVERT_STRING",
	"XRT_MODULE_XSON_WRITE",
	"XRT_MODULE_TIME_TEXT"
};

/* Value 容器的批量合并与集合运算不应扩大普通动态值程序。 */
static const char* const xl6XrtTrimValueCollectionModules[] = {
    "XRT_MODULE_VALUE_COLLECTION"
};

/* 只有语言类句柄需要把 XRT 运行时对象包装为动态值。 */
static const char* const xl6XrtTrimRuntimeObjectModules[] = {
	"XRUNTIME_MODULE_RUNTIME_VALUE_OBJECT"
};

/* weak<T> 直接复用 xruntime 弱引用与 Value 桥接。 */
static const char* const xl6XrtTrimRuntimeWeakModules[] = {
	"XRUNTIME_MODULE_RUNTIME_VALUE_WEAK"
};

/* 语言对象动态字段与运行时对象包装保持独立裁剪。 */
static const char* const xl6XrtTrimDynamicFieldsModules[] = {
	"XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD"
};

/* 语言类型值的字符串化以及字符串值装箱、拆箱。 */
static const char* const xl6XrtTrimTypeStringValueModules[] = {
    "XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE",
    "XRUNTIME_MODULE_RUNTIME_CONVERT_STRING"
};

/* XLang 四类泛型容器需要 Value 桥接与基础类型容器。 */
static const char* const xl6XrtTrimTypedContainerModules[] = {
    "XRUNTIME_MODULE_TYPED_ARRAY_VALUE",
    "XRUNTIME_MODULE_TYPED_LIST_VALUE",
    "XRUNTIME_MODULE_TYPED_SET_VALUE",
    "XRUNTIME_MODULE_TYPED_DICT_VALUE"
};

/* 类型栈、类型树和三种有界并发队列保持独立裁剪边界。 */
static const char* const xl6XrtTrimTypedStackModules[] = {
    "XRUNTIME_MODULE_TYPED_STACK"
};

static const char* const xl6XrtTrimTypedTreeModules[] = {
    "XRUNTIME_MODULE_TYPED_TREE"
};

static const char* const xl6XrtTrimTypedQueueSPSCModules[] = {
    "XRUNTIME_MODULE_TYPED_QUEUE_SPSC"
};

static const char* const xl6XrtTrimTypedQueueMPSCModules[] = {
    "XRUNTIME_MODULE_TYPED_QUEUE_MPSC"
};

static const char* const xl6XrtTrimTypedQueueMPMCModules[] = {
    "XRUNTIME_MODULE_TYPED_QUEUE_MPMC"
};

/* 字符串复制、比较、查找和连接等基础能力。 */
static const char* const xl6XrtTrimStringCoreModules[] = {
    "XRT_MODULE_STRING"
};

/* 标准字符串运行时使用的拆分、通配、Unicode 文本和数字格式化能力。 */
static const char* const xl6XrtTrimStdStringModules[] = {
    "XRT_MODULE_STRING_SPLIT",
    "XRT_MODULE_STRING_GLOB",
    "XRT_MODULE_UNICODE_TEXT",
    "XRT_MODULE_UNICODE_DISTANCE",
    "XRT_MODULE_NUMBER_FORMAT"
};

/* UTF-8 标量迭代与校验。 */
static const char* const xl6XrtTrimUnicodeModules[] = {
    "XRT_MODULE_UNICODE"
};

/* Unicode 标量距离与相似度算法。 */
static const char* const xl6XrtTrimUnicodeDistanceModules[] = {
    "XRT_MODULE_UNICODE_DISTANCE"
};

/* Unicode 编码检测、BOM 处理和转码能力。 */
static const char* const xl6XrtTrimEncodingModules[] = {
    "XRT_MODULE_CHARSET_DETECT"
};

/* 数字解析和格式化公开给语言的根模块。 */
static const char* const xl6XrtTrimNumberModules[] = {
    "XRT_MODULE_NUMBER_FORMAT"
};

/* XRT 扩展数学函数保持独立于数字文本格式化。 */
static const char* const xl6XrtTrimMathModules[] = {
    "XRT_MODULE_MATH"
};

/* JSON 能力公开给语言的根模块。 */
static const char* const xl6XrtTrimJsonModules[] = {
    "XRT_MODULE_JSON"
};

/* XSON 能力公开给语言的根模块。 */
static const char* const xl6XrtTrimXsonModules[] = {
    "XRT_MODULE_XSON"
};

/* XID 能力公开给语言的根模块。 */
static const char* const xl6XrtTrimXidModules[] = {
    "XRT_MODULE_XID"
};

/* Template 组合、扩展和文件入口公开给语言的根模块。 */
static const char* const xl6XrtTrimTemplateModules[] = {
    "XRT_MODULE_TEMPLATE_EXTENSION",
    "XRT_MODULE_TEMPLATE_FILE"
};

/* 进程与管道公开给语言的根模块。 */
static const char* const xl6XrtTrimProcessModules[] = {
    "XRT_MODULE_PROCESS_PIPELINE",
    "XRT_MODULE_PROCESS_FUTURE",
    "XRT_MODULE_PROCESS_FILE",
    "XRT_MODULE_PROCESS_TERMINAL"
};

/* 日志格式、目标和异步写入公开给语言的根模块。 */
static const char* const xl6XrtTrimLoggerModules[] = {
    "XRT_MODULE_LOGGER_ASYNC",
    "XRT_MODULE_LOGGER_FILE_JSON",
    "XRT_MODULE_LOGGER_FILE_TEXT",
    "XRT_MODULE_LOGGER_CONSOLE",
    "XRT_MODULE_LOGGER_PRINTF"
};

#define XL6_XRT_TRIM_COUNT(values) (sizeof(values) / sizeof((values)[0]))

typedef struct XL6_XrtTrimSelection {
    int enabled;
    const char* const* modules;
    size_t count;
} XL6_XrtTrimSelection;

/* 输出一个已经由 XRT 清单定义的根模块。 */
static int xl6XrtTrimEmitModule(XL6_Output* out, int enabled, const char* moduleName)
{
    if ( !enabled ) {
        return 1;
    }
    return xl6OutputPrintf(out, "#define %s\n", moduleName) != EOF;
}

/* 输出同一语言能力对应的一组 XRT 根模块。 */
static int xl6XrtTrimEmitModuleSet(
    XL6_Output* out,
    int enabled,
    const char* const* modules,
    size_t count
)
{
    size_t i;

    if ( !enabled ) {
        return 1;
    }
    for ( i = 0; i < count; i += 1 ) {
        if ( !xl6XrtTrimEmitModule(out, 1, modules[i]) ) {
            return 0;
        }
    }
    return 1;
}

/* 把语言能力映射为公开根模块，不在这里展开 XRT 内部依赖。 */
static int xl6XrtTrimEmitConfigModules(XL6_Output* out, const XL6_XrtTrimConfig* cfg)
{
    XL6_XrtTrimSelection selections[] = {
        { cfg->keepConsole, xl6XrtTrimConsoleModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimConsoleModules) },
        { cfg->keepTime, xl6XrtTrimTimeModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimTimeModules) },
        { cfg->keepFile, xl6XrtTrimFileModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimFileModules) },
        { cfg->keepFileAsync, xl6XrtTrimFileAsyncModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimFileAsyncModules) },
        { cfg->keepQueue, xl6XrtTrimQueueModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimQueueModules) },
        { cfg->keepAtomic, xl6XrtTrimAtomicModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimAtomicModules) },
        { cfg->keepCoroutine, xl6XrtTrimCoroutineModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimCoroutineModules) },
        { cfg->keepThread, xl6XrtTrimThreadModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimThreadModules) },
        { cfg->keepMutex, xl6XrtTrimMutexModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimMutexModules) },
        { cfg->keepSem, xl6XrtTrimSemModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimSemModules) },
        { cfg->keepCond, xl6XrtTrimCondModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimCondModules) },
        { cfg->keepRWLock, xl6XrtTrimRWLockModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimRWLockModules) },
        { cfg->keepNetwork, xl6XrtTrimNetworkModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetworkModules) },
        { cfg->keepNetInterface, xl6XrtTrimNetInterfaceModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetInterfaceModules) },
        { cfg->keepNetInterfaceText, xl6XrtTrimNetInterfaceTextModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetInterfaceTextModules) },
        { cfg->keepNetDns, xl6XrtTrimNetDnsModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetDnsModules) },
        { cfg->keepNetResolver, xl6XrtTrimNetResolverModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetResolverModules) },
        { cfg->keepNetEngine, xl6XrtTrimNetEngineModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetEngineModules) },
        { cfg->keepNetBuffer, xl6XrtTrimNetBufferModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetBufferModules) },
        { cfg->keepNetProxy, xl6XrtTrimNetProxyModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetProxyModules) },
        { cfg->keepNetProxyDial, xl6XrtTrimNetProxyDialModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetProxyDialModules) },
        { cfg->keepNetTcp, xl6XrtTrimNetTcpModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetTcpModules) },
        { cfg->keepNetTcpFile, xl6XrtTrimNetTcpFileModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetTcpFileModules) },
        { cfg->keepNetTcpDial, xl6XrtTrimNetTcpDialModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetTcpDialModules) },
        { cfg->keepNetTcpServer, xl6XrtTrimNetTcpServerModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetTcpServerModules) },
        { cfg->keepNetUdp, xl6XrtTrimNetUdpModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNetUdpModules) },
        { cfg->keepHttpUtil, xl6XrtTrimHttpProtocolModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimHttpProtocolModules) },
        { cfg->keepCodecHex, xl6XrtTrimCodecHexModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimCodecHexModules) },
        { cfg->keepCodecBase64, xl6XrtTrimCodecBase64Modules, XL6_XRT_TRIM_COUNT(xl6XrtTrimCodecBase64Modules) },
        { cfg->keepBufferHex, xl6XrtTrimBufferHexModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimBufferHexModules) },
        { cfg->keepBufferBase64, xl6XrtTrimBufferBase64Modules, XL6_XRT_TRIM_COUNT(xl6XrtTrimBufferBase64Modules) },
        { cfg->keepCodecPercent, xl6XrtTrimCodecPercentModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimCodecPercentModules) },
        { cfg->keepFormUrl, xl6XrtTrimFormUrlModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimFormUrlModules) },
        { cfg->keepHttp1Core, xl6XrtTrimHttp1CoreModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimHttp1CoreModules) },
        { cfg->keepHttp1Codec, xl6XrtTrimHttp1CodecModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimHttp1CodecModules) },
        { cfg->keepHttpDecode, xl6XrtTrimHttpDecodeModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimHttpDecodeModules) },
        { cfg->keepWebSocketCodec, xl6XrtTrimWebSocketCodecModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimWebSocketCodecModules) },
        { cfg->keepWebSocketHandshake, xl6XrtTrimWebSocketHandshakeModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimWebSocketHandshakeModules) },
        { cfg->keepWebSocketUpgrade, xl6XrtTrimWebSocketUpgradeModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimWebSocketUpgradeModules) },
        { cfg->keepWebSocketStream, xl6XrtTrimWebSocketStreamModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimWebSocketStreamModules) },
        { cfg->keepWebSocketStreamTls, xl6XrtTrimWebSocketStreamTlsModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimWebSocketStreamTlsModules) },
        { cfg->keepCrypto, xl6XrtTrimCryptoModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimCryptoModules) },
        { cfg->keepTls, xl6XrtTrimTlsModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimTlsModules) },
        {
            cfg->keepTlsStream,
            xl6XrtTrimTlsStreamModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTlsStreamModules)
        },
        {
            cfg->keepTlsDial,
            xl6XrtTrimTlsDialModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTlsDialModules)
        },
        {
            cfg->keepTlsServer,
            xl6XrtTrimTlsServerModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTlsServerModules)
        },
        { cfg->keepXid, xl6XrtTrimXidModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimXidModules) },
        { cfg->keepBuffer, xl6XrtTrimBufferModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimBufferModules) },
        { cfg->keepValue, xl6XrtTrimValueModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimValueModules) },
		{
			cfg->keepValueCollection,
			xl6XrtTrimValueCollectionModules,
			XL6_XRT_TRIM_COUNT(xl6XrtTrimValueCollectionModules)
		},
		{
			cfg->keepRuntimeObject,
			xl6XrtTrimRuntimeObjectModules,
			XL6_XRT_TRIM_COUNT(xl6XrtTrimRuntimeObjectModules)
        },
		{
			cfg->keepRuntimeWeak,
			xl6XrtTrimRuntimeWeakModules,
			XL6_XRT_TRIM_COUNT(xl6XrtTrimRuntimeWeakModules)
		},
		{
			cfg->keepDynamicFields,
			xl6XrtTrimDynamicFieldsModules,
			XL6_XRT_TRIM_COUNT(xl6XrtTrimDynamicFieldsModules)
		},
        {
            cfg->keepTypeStringValue && !cfg->keepValue,
            xl6XrtTrimTypeStringValueModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypeStringValueModules)
        },
        {
            cfg->keepTypedContainer,
            xl6XrtTrimTypedContainerModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypedContainerModules)
        },
        {
            cfg->keepTypedStack,
            xl6XrtTrimTypedStackModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypedStackModules)
        },
        {
            cfg->keepTypedTree,
            xl6XrtTrimTypedTreeModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypedTreeModules)
        },
        {
            cfg->keepTypedQueueSPSC,
            xl6XrtTrimTypedQueueSPSCModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypedQueueSPSCModules)
        },
        {
            cfg->keepTypedQueueMPSC,
            xl6XrtTrimTypedQueueMPSCModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypedQueueMPSCModules)
        },
        {
            cfg->keepTypedQueueMPMC,
            xl6XrtTrimTypedQueueMPMCModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimTypedQueueMPMCModules)
        },
        {
            cfg->keepStringCore,
            xl6XrtTrimStringCoreModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimStringCoreModules)
        },
        {
            cfg->keepStdString,
            xl6XrtTrimStdStringModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimStdStringModules)
        },
        { cfg->keepUnicode, xl6XrtTrimUnicodeModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimUnicodeModules) },
        {
            cfg->keepUnicodeDistance,
            xl6XrtTrimUnicodeDistanceModules,
            XL6_XRT_TRIM_COUNT(xl6XrtTrimUnicodeDistanceModules)
        },
        { cfg->keepEncoding, xl6XrtTrimEncodingModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimEncodingModules) },
        { cfg->keepJnum, xl6XrtTrimNumberModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimNumberModules) },
        { cfg->keepMath, xl6XrtTrimMathModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimMathModules) },
        { cfg->keepJson, xl6XrtTrimJsonModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimJsonModules) },
        { cfg->keepXson, xl6XrtTrimXsonModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimXsonModules) },
        { cfg->keepTemplate, xl6XrtTrimTemplateModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimTemplateModules) },
        { cfg->keepSubprocess, xl6XrtTrimProcessModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimProcessModules) },
        { cfg->keepLogger, xl6XrtTrimLoggerModules, XL6_XRT_TRIM_COUNT(xl6XrtTrimLoggerModules) }
    };
    size_t i;

    if ( out == NULL || cfg == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_XRT_TRIM_COUNT(selections); i += 1 ) {
        if ( !xl6XrtTrimEmitModuleSet(
                 out,
                 selections[i].enabled,
                 selections[i].modules,
                 selections[i].count
             ) ) {
            return 0;
        }
    }
    return 1;
}



/* 生成只包含实际语言能力根模块的 xruntime 产品实现单元。 */
int xl6XrtTrimEmitImplementationSource(XL6_Output* out, const XL6_XrtTrimConfig* cfg)
{
    int full;

    if ( out == NULL ) {
        return 0;
    }
    if ( xl6OutputPuts("/* Generated XRT implementation for xlang output. */\n", out) == EOF ) {
        return 0;
    }
    full = (cfg == NULL) || cfg->full;
    if ( !xl6XrtTrimEmitModule(out, full, "XRT_MODULE_ALL") ) {
        return 0;
    }
    if ( !xl6XrtTrimEmitModule(out, full, "XRUNTIME_MODULE_ALL") ) {
        return 0;
    }
    /* 生成运行时始终使用临时内存和线程键管理表达式级临时值。 */
    if ( !xl6XrtTrimEmitModule(out, !full && !cfg->explicitModules, "XRT_MODULE_TEMP_MEMORY") ) {
        return 0;
    }
	if ( !xl6XrtTrimEmitModule(out, !full && !cfg->explicitModules, "XRT_MODULE_THREAD_KEY") ) {
		return 0;
	}
	if ( !xl6XrtTrimEmitModule(out, !full && !cfg->explicitModules, "XRT_MODULE_ONCE") ) {
		return 0;
	}
    if ( !full && !cfg->explicitModules && !xl6XrtTrimEmitConfigModules(out, cfg) ) {
        return 0;
    }
    if ( !full && cfg != NULL && cfg->packageXrtModules != NULL ) {
        size_t i;

        for ( i = 0; i < cfg->packageXrtModules->count; i += 1 ) {
            if ( !xl6XrtTrimEmitModule(out, 1, cfg->packageXrtModules->items[i]) ) {
                return 0;
            }
        }
    }
    if (!full && cfg != NULL) {
        if (cfg->artifactModuleCount && !cfg->artifactModules) return 0;
        for (size_t i=0;i<cfg->artifactModuleCount;++i)
            if (!cfg->artifactModules[i] ||
                !xl6XrtTrimEmitModule(out,1,cfg->artifactModules[i])) return 0;
    }
    if ( xl6OutputPuts("#define XRUNTIME_IMPLEMENTATION\n", out) == EOF ) {
        return 0;
    }
    return xl6OutputPuts("#include \"xruntime.h\"\n", out) != EOF;
}

int xl6XrtTrimBuildImplementationSource(const XL6_AstNode* root, const XL6_ImportDeps* deps, char** outText)
{
    XL6_XrtTrimConfig cfg;
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    xl6XrtTrimAnalyze(&cfg, root, deps);
    if ( !xl6OutputOpenMemory(&out) ) {
        return 0;
    }
    ok = xl6XrtTrimEmitImplementationSource(&out, &cfg) && !xl6OutputHasFailed(&out);
    if ( ok ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
        ok = *outText != NULL;
    }
    xl6OutputClose(&out);
    return ok;
}

int xl6XrtTrimBuildImplementationSourceWithGeneratedSource(const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource, char** outText)
{
    return xl6XrtTrimBuildImplementationSourceWithRuntimeModules(root,deps,generatedSource,NULL,0,outText);
}

int xl6XrtTrimBuildImplementationSourceWithRuntimeModules(const XL6_AstNode* root,
    const XL6_ImportDeps* deps, const char* generatedSource,
    const char* const* modules, size_t moduleCount, char** outText)
{
    XL6_XrtTrimConfig cfg;
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    xl6XrtTrimAnalyzeWithGeneratedSource(&cfg, root, deps, generatedSource);
    if (moduleCount && !modules) return 0;
    cfg.artifactModules=modules;
    cfg.artifactModuleCount=moduleCount;
    if ( !xl6OutputOpenMemory(&out) ) {
        return 0;
    }
    ok = xl6XrtTrimEmitImplementationSource(&out, &cfg) && !xl6OutputHasFailed(&out);
    if ( ok ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
        ok = *outText != NULL;
    }
    xl6OutputClose(&out);
    return ok;
}

int xl6XrtTrimBuildImplementationSourceFromArtifact(const XL6_ImportDeps* deps,
    const char* const* modules, size_t moduleCount, char** outText)
{
    XL6_XrtTrimConfig cfg;
    XL6_Output out;
    int ok;
    if (!outText) return 0;
    *outText=NULL;
    if (moduleCount && !modules) return 0;
    for (size_t i=0;i<moduleCount;++i) if (!modules[i]) return 0;
    memset(&cfg,0,sizeof(cfg));
    cfg.explicitModules=1;
    cfg.full=xl6XrtTrimPackageRequiresFull(deps);
    cfg.packageXrtModules=deps ? &deps->xrtModules : NULL;
    cfg.artifactModules=modules;
    cfg.artifactModuleCount=moduleCount;
    if (!xl6OutputOpenMemory(&out)) return 0;
    ok=xl6XrtTrimEmitImplementationSource(&out,&cfg) && !xl6OutputHasFailed(&out);
    if (ok) {
        *outText=xl6OutputTakeMemoryText(&out,NULL);
        ok=*outText!=NULL;
    }
    xl6OutputClose(&out);
    return ok;
}
