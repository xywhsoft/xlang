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
    cfg->keepArray = 1;
    cfg->keepList = 1;
    cfg->keepDict = 1;
    cfg->keepAvlTree = 1;
    cfg->keepMemPool = 1;
    cfg->keepMemPoolFs = 1;
    cfg->keepMemUnit = 1;
    cfg->keepBsmn = 1;
    cfg->keepJnum = 1;
    cfg->keepJson = 1;
    cfg->keepXson = 1;
    cfg->keepStack = 1;
    cfg->keepBuffer = 1;
    cfg->keepFile = 1;
    cfg->keepTime = 1;
}

static void xl6XrtTrimKeepJsonStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepJson = 1;
    cfg->keepJnum = 1;
    cfg->keepStack = 1;
    cfg->keepFile = 1;
    xl6XrtTrimKeepValueStack(cfg);
}

static void xl6XrtTrimKeepNetworkStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepNetwork = 1;
    cfg->keepCoroutine = 1;
    cfg->keepQueue = 1;
    cfg->keepFile = 1;
    cfg->keepFileAsync = 1;
}

static void xl6XrtTrimKeepNetTlsStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepNetTls = 1;
}

static void xl6XrtTrimKeepUrlStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepXUrl = 1;
}

static void xl6XrtTrimKeepHttpUtilStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepXUrl = 1;
    cfg->keepHttpUtil = 1;
}

static void xl6XrtTrimKeepCodecStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepXCodec = 1;
}

static void xl6XrtTrimKeepWebStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepXUrl = 1;
    cfg->keepHttpUtil = 1;
    cfg->keepXCodec = 1;
    cfg->keepXHttpd = 1;
    cfg->keepXWeb = 1;
    cfg->keepNetTls = 1;
}

static void xl6XrtTrimKeepHttpClientStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimKeepNetworkStack(cfg);
    cfg->keepXUrl = 1;
    cfg->keepHttpUtil = 1;
    cfg->keepXCodec = 1;
    cfg->keepXHttp = 1;
    cfg->keepNetTls = 1;
}

static void xl6XrtTrimKeepCoroutineStack(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    cfg->keepCoroutine = 1;
    /*
     * XRT 目前把 future 类型描述和若干异步对象挂在 network 裁剪组下。
     * xlang 的 task/future 只要参与静态编译，就必须保留 network 根模块；
     * 等 XRT 后续将 future 从 network 中拆出后，这里可以再收窄。
     */
    xl6XrtTrimKeepNetworkStack(cfg);
    xl6XrtTrimKeepValueStack(cfg);
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
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_template_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_template_") ||
         strcmp(text, "Template") == 0 ) {
        cfg->keepTemplate = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_regex_") ||
         strcmp(text, "Regex") == 0 ||
         strcmp(text, "RegexMatch") == 0 ||
         strcmp(text, "RegexSet") == 0 ) {
        cfg->keepRegex = 1;
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
         strcmp(text, "Logger") == 0 ) {
        cfg->keepLogger = 1;
        cfg->keepFile = 1;
        cfg->keepTime = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_http_") ||
         strcmp(text, "HttpRequest") == 0 ||
         strcmp(text, "HttpResponse") == 0 ) {
        xl6XrtTrimKeepWebStack(cfg);
        cfg->keepJson = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "xl6_xrt_http_client_") ||
         strcmp(text, "HttpClientRequest") == 0 ||
         strcmp(text, "HttpClientResponse") == 0 ) {
        xl6XrtTrimKeepHttpClientStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_url_") ) {
        xl6XrtTrimKeepUrlStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_query_") ) {
        xl6XrtTrimKeepHttpUtilStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_get_local_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_tcp_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_udp_") ) {
        xl6XrtTrimKeepNetworkStack(cfg);
    }
    if ( strcmp(text, "TlsConfig") == 0 ||
         strcmp(text, "TlsSession") == 0 ||
         strcmp(text, "TlsResume") == 0 ||
         xl6XrtTrimTextStartsWith(text, "xl6_xrt_tls_") ||
         xl6XrtTrimTextStartsWith(text, "xrtNetTls") ) {
        xl6XrtTrimKeepNetTlsStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_percent_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_form_url_") ) {
        xl6XrtTrimKeepHttpUtilStack(cfg);
    }
    if ( strcmp(text, "codec") == 0 ||
         strcmp(text, "hexEncode") == 0 ||
         strcmp(text, "hexDecode") == 0 ||
         strcmp(text, "base64Encode") == 0 ||
         strcmp(text, "base64Decode") == 0 ||
         strcmp(text, "hexEncodeBytes") == 0 ||
         strcmp(text, "hexDecodeBytes") == 0 ||
         strcmp(text, "base64EncodeBytes") == 0 ||
         strcmp(text, "base64DecodeBytes") == 0 ) {
        xl6XrtTrimKeepCodecStack(cfg);
    }
    if ( strcmp(text, "codec") == 0 ||
         strcmp(text, "percentEncode") == 0 ||
         strcmp(text, "percentDecode") == 0 ||
         strcmp(text, "formUrlEncode") == 0 ||
         strcmp(text, "formUrlDecode") == 0 ) {
        xl6XrtTrimKeepHttpUtilStack(cfg);
    }
    if ( strcmp(text, "hexDecodeBytes") == 0 ||
         strcmp(text, "base64DecodeBytes") == 0 ) {
        cfg->keepBuffer = 1;
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_buffer_from_") ) {
        cfg->keepBuffer = 1;
        xl6XrtTrimKeepCodecStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_encoding_") ) {
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
        xl6XrtTrimKeepCodecStack(cfg);
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_make_xids") ) {
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

    if ( xl6XrtTrimTextContains(source, "xrtDate") ||
         xl6XrtTrimTextContains(source, "xrtTime") ||
         xl6XrtTrimTextContains(source, "xrtNow") ||
         xl6XrtTrimTextContains(source, "xrtTimer") ) {
        cfg->keepTime = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtFile") ||
         xl6XrtTrimTextContains(source, "xrtDir") ||
         xl6XrtTrimTextContains(source, "xrtPath") ) {
        cfg->keepFile = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtParseJSON") ||
         xl6XrtTrimTextContains(source, "xrtJson") ||
         xl6XrtTrimTextContains(source, "xrtStringifyJSON") ) {
        xl6XrtTrimKeepJsonStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtParseXSON") ||
         xl6XrtTrimTextContains(source, "xrtXson") ||
         xl6XrtTrimTextContains(source, "xrtStringifyXSON") ) {
        cfg->keepXson = 1;
        xl6XrtTrimKeepJsonStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtHex") ||
         xl6XrtTrimTextContains(source, "xrtBase64") ) {
        xl6XrtTrimKeepCodecStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtBuffer") ) {
        cfg->keepBuffer = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtPercent") ||
         xl6XrtTrimTextContains(source, "xrtFormUrl") ) {
        xl6XrtTrimKeepHttpUtilStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtSHA") ||
         xl6XrtTrimTextContains(source, "xrtHMAC") ||
         xl6XrtTrimTextContains(source, "xrtHKDF") ||
         xl6XrtTrimTextContains(source, "xrtChaCha") ||
         xl6XrtTrimTextContains(source, "xrtAES") ||
         xl6XrtTrimTextContains(source, "xrtEd25519") ||
         xl6XrtTrimTextContains(source, "xrtX25519") ||
         xl6XrtTrimTextContains(source, "xrtX448") ||
         xl6XrtTrimTextContains(source, "xrtRSA") ) {
        cfg->keepCrypto = 1;
        xl6XrtTrimKeepCodecStack(cfg);
        xl6XrtTrimKeepValueStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtNetTls") ||
         xl6XrtTrimTextContains(source, "xl6_xrt_tls_") ) {
        xl6XrtTrimKeepNetTlsStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtNet") ) {
        xl6XrtTrimKeepNetworkStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtNetDgramPacketBytesCopy") ) {
        cfg->keepBuffer = 1;
        xl6XrtTrimKeepNetworkStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtHttp") ||
         xl6XrtTrimTextContains(source, "xrtWeb") ) {
        xl6XrtTrimKeepWebStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtUrl") ) {
        xl6XrtTrimKeepUrlStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtRegex") ||
         xl6XrtTrimTextContains(source, "bbre") ) {
        cfg->keepRegex = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtSubprocess") ||
         xl6XrtTrimTextContains(source, "xrtProcess") ) {
        cfg->keepSubprocess = 1;
        cfg->keepFile = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtLog") ) {
        cfg->keepLogger = 1;
        cfg->keepFile = 1;
        cfg->keepTime = 1;
    }

    if ( xl6XrtTrimTextContains(source, "xrtTemplate") ||
         xl6XrtTrimTextContains(source, "xte") ) {
        cfg->keepTemplate = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }

    if ( xl6XrtTrimTextContains(source, "xrtMakeXIDS") ) {
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
    if ( xl6CgenNodeNeedsCryptoRuntime(root) ) {
        cfg->keepCrypto = 1;
        xl6XrtTrimKeepCodecStack(cfg);
    }
    if ( xl6CgenNodeNeedsRegexRuntime(root) ) {
        cfg->keepRegex = 1;
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
    if ( xl6CgenNodeNeedsHttpServerRuntime(root) ) {
        xl6XrtTrimKeepWebStack(cfg);
        cfg->keepJson = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6CgenNodeNeedsHttpClientRuntime(root) ) {
        xl6XrtTrimKeepHttpClientStack(cfg);
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

static void xl6XrtTrimApplyDeps(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL || cfg->full ) {
        return;
    }
    if ( cfg->keepFileAsync ) {
        cfg->keepFile = 1;
        cfg->keepQueue = 1;
    }
    if ( cfg->keepNetwork ) {
        cfg->keepQueue = 1;
        cfg->keepFile = 1;
        cfg->keepFileAsync = 1;
    }
    if ( cfg->keepXWeb ) {
        cfg->keepXHttpd = 1;
    }
    if ( cfg->keepXHttp || cfg->keepXHttpd || cfg->keepXWeb || cfg->keepXWs ) {
        cfg->keepXUrl = 1;
        cfg->keepHttpUtil = 1;
        cfg->keepXCodec = 1;
    }
    if ( cfg->keepXUrl || cfg->keepHttpUtil || cfg->keepXCodec ||
         cfg->keepXHttp || cfg->keepXHttpd || cfg->keepXWeb || cfg->keepXWs ) {
        cfg->keepNetwork = 1;
    }
    if ( cfg->keepXWs ) {
        cfg->keepCrypto = 1;
    }
    if ( cfg->keepNetTls ) {
        cfg->keepCrypto = 1;
        cfg->keepFile = 1;
    }
    if ( cfg->keepJson ) {
        xl6XrtTrimKeepJsonStack(cfg);
    }
    if ( cfg->keepXson ) {
        xl6XrtTrimKeepJsonStack(cfg);
    }
    if ( cfg->keepTemplate ) {
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( cfg->keepLogger ) {
        cfg->keepTime = 1;
    }
    if ( cfg->keepSubprocess ) {
        cfg->keepFile = 1;
    }
    if ( cfg->keepXid ) {
        cfg->keepTime = 1;
    }
    if ( cfg->keepValue ) {
        xl6XrtTrimKeepValueStack(cfg);
    }
}

void xl6XrtTrimConfigInit(XL6_XrtTrimConfig* cfg)
{
    if ( cfg == NULL ) {
        return;
    }
    memset(cfg, 0, sizeof(*cfg));
}

void xl6XrtTrimAnalyze(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimConfigInit(cfg);
    if ( deps != NULL &&
         (deps->features & (XL6_IMPORT_FEATURE_TCC_RUNTIME |
                            XL6_IMPORT_FEATURE_XLANG_RUNTIME |
                            XL6_IMPORT_FEATURE_XLANG_COMPILER)) != 0 ) {
        cfg->full = 1;
        return;
    }
    if ( deps != NULL && deps->cSources.count > 0 ) {
        /*
            native C package 可以调用任意 XRT 公开函数。
            当前裁剪器只分析 xlang AST 和标准库需求，不能可靠推断 C 源内部的 XRT 依赖。
        */
        cfg->full = 1;
        return;
    }
    xl6XrtTrimApplyBasicNeeds(cfg, root);
    xl6XrtTrimScanNode(cfg, root);
    xl6XrtTrimApplyDeps(cfg);
}

void xl6XrtTrimAnalyzeWithGeneratedSource(XL6_XrtTrimConfig* cfg, const XL6_AstNode* root, const XL6_ImportDeps* deps, const char* generatedSource)
{
    if ( cfg == NULL ) {
        return;
    }
    xl6XrtTrimAnalyze(cfg, root, deps);
    xl6XrtTrimScanGeneratedSource(cfg, generatedSource);
    xl6XrtTrimApplyDeps(cfg);
}

static int xl6XrtTrimEmitNoMacro(XL6_Output* out, int keep, const char* macroName)
{
    if ( keep ) {
        return 1;
    }
    return xl6OutputPrintf(out, "#define %s\n", macroName) != EOF;
}

int xl6XrtTrimEmitImplementationSource(XL6_Output* out, const XL6_XrtTrimConfig* cfg)
{
    if ( out == NULL ) {
        return 0;
    }
    if ( xl6OutputPuts("/* Generated XRT implementation for xlang output. */\n", out) == EOF ||
         xl6OutputPuts("#define XRT_IMPLEMENTATION\n", out) == EOF ) {
        return 0;
    }
    if ( (cfg == NULL || cfg->full || cfg->keepTemplate) &&
         xl6OutputPuts("#define XTE_ENABLE_FILE\n", out) == EOF ) {
        return 0;
    }
    if ( cfg == NULL || cfg->full ) {
        return xl6OutputPuts("#include \"xrt.h\"\n", out) != EOF;
    }
    if ( xl6OutputPuts("/* XRT module-level trim macros. */\n", out) == EOF ) {
        return 0;
    }
    if ( !xl6XrtTrimEmitNoMacro(out, cfg->keepTime, "XRT_NO_TIME") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepFile, "XRT_NO_FILE") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepFileAsync, "XRT_NO_FILE_ASYNC") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepQueue, "XRT_NO_QUEUE") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepCoroutine, "XRT_NO_COROUTINE") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepNetwork, "XRT_NO_NETWORK") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepCrypto, "XRT_NO_CRYPTO") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepNetTls, "XRT_NO_NETTLS") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepXid, "XRT_NO_XID") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepBuffer, "XRT_NO_BUFFER") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepArray, "XRT_NO_ARRAY") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepBsmn, "XRT_NO_BSMN") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepMemUnit, "XRT_NO_MEMUNIT") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepMemPoolFs, "XRT_NO_MEMPOOL_FS") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepStack, "XRT_NO_STACK") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepAvlTree, "XRT_NO_AVLTREE") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepMemPool, "XRT_NO_MEMPOOL") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepDict, "XRT_NO_DICT") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepList, "XRT_NO_LIST") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepValue, "XRT_NO_VALUE") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepJnum, "XRT_NO_JNUM") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepJson, "XRT_NO_JSON") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepXson, "XRT_NO_XSON") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepTemplate, "XRT_NO_TEMPLATE") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepRegex, "XRT_NO_REGEX") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepSubprocess, "XRT_NO_SUBPROCESS") ||
         !xl6XrtTrimEmitNoMacro(out, cfg->keepLogger, "XRT_NO_LOGGER") ) {
        return 0;
    }
    if ( cfg->keepNetwork &&
         (!xl6XrtTrimEmitNoMacro(out, cfg->keepXUrl, "XRT_NO_XURL") ||
          !xl6XrtTrimEmitNoMacro(out, cfg->keepHttpUtil, "XRT_NO_HTTP_UTIL") ||
          !xl6XrtTrimEmitNoMacro(out, cfg->keepXCodec, "XRT_NO_XCODEC") ||
          !xl6XrtTrimEmitNoMacro(out, cfg->keepXHttp, "XRT_NO_XHTTP") ||
          !xl6XrtTrimEmitNoMacro(out, cfg->keepXHttpd, "XRT_NO_XHTTPD") ||
          !xl6XrtTrimEmitNoMacro(out, cfg->keepXWeb, "XRT_NO_XWEB") ||
          !xl6XrtTrimEmitNoMacro(out, cfg->keepXWs, "XRT_NO_XWS")) ) {
        return 0;
    }
    return xl6OutputPuts("#include \"xrt.h\"\n", out) != EOF;
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
    XL6_XrtTrimConfig cfg;
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    xl6XrtTrimAnalyzeWithGeneratedSource(&cfg, root, deps, generatedSource);
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
