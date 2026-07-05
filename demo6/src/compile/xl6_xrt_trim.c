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
    cfg->keepQueue = 1;
    cfg->keepFile = 1;
    cfg->keepFileAsync = 1;
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
        xl6XrtTrimKeepNetworkStack(cfg);
        cfg->keepJson = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_url_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_runtime_query_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_get_local_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_tcp_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_udp_") ) {
        xl6XrtTrimKeepNetworkStack(cfg);
    }
    if ( xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_hex_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_base64_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_percent_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_form_url_") ||
         xl6XrtTrimTextStartsWith(text, "__xl_std_xrt_buffer_from_") ) {
        /*
         * 当前 XRT 中 XCODEC 仍挂在 network 裁剪组下。保留 codec 时必须暂时保留
         * network 根模块；后续 XRT 将 XCODEC 拆出后，这条规则即可收窄。
         */
        xl6XrtTrimKeepNetworkStack(cfg);
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
        xl6XrtTrimKeepNetworkStack(cfg);
        cfg->keepJson = 1;
        cfg->keepJnum = 1;
        xl6XrtTrimKeepValueStack(cfg);
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
    xl6XrtTrimApplyBasicNeeds(cfg, root);
    xl6XrtTrimScanNode(cfg, root);
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
    if ( cfg == NULL || cfg->full ) {
        return xl6OutputPuts("#include \"lib/xrt/xrt.h\"\n", out) != EOF;
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
    return xl6OutputPuts("#include \"lib/xrt/xrt.h\"\n", out) != EOF;
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
