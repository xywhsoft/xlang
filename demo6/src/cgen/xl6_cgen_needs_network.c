int xl6CgenNodeNeedsBufferRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsBufferClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsBufferRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTlsRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsTlsClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTlsRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTlsStreamRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "TlsStream") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTlsStreamRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTlsDialRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_EXTERN_FUNC &&
         (xl6CgenNeedsBasicExternTextEquals(node->text, "xl6_xrt_tls_connect") ||
          xl6CgenNeedsBasicExternTextEquals(node->text, "xl6_xrt_tls_connect_async")) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTlsDialRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTlsServerRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         (xl6CgenNeedsBasicTextEquals(node->text, "TlsServer") ||
          xl6CgenNeedsBasicTextEquals(node->text, "TlsServerStats")) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTlsServerRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* 识别 HTTP/1 报头、正文与快速封包桥。 */
int xl6CgenNodeNeedsHttp1Runtime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         (xl6CgenNeedsBasicTextEquals(node->text, "Http1") ||
          xl6CgenNeedsBasicTextEquals(node->text, "Http1Head") ||
          xl6CgenNeedsBasicTextEquals(node->text, "Http1Body")) ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_http1_") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttp1Runtime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* gzip/identity 解码桥保持独立裁剪。 */
int xl6CgenNodeNeedsHttpDecodeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "HttpDecoder") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_http_decoder_") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttpDecodeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* 帧头与 Close 载荷保持最小 WebSocket 协议闭包。 */
int xl6CgenNodeNeedsWsFrameRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         (xl6CgenNeedsBasicTextEquals(node->text, "WsFrame") ||
          xl6CgenNeedsBasicTextEquals(node->text, "WsClose")) ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         (xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_frame_") ||
          xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_close_") ||
          xl6CgenNeedsBasicTextEquals(node->text, "xl6_xrt_ws_mask")) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsWsFrameRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* 握手工具独立于 HTTP Upgrade 与 Stream。 */
int xl6CgenNodeNeedsWsHandshakeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "WsHandshake") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         (xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_key_") ||
          xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_accept") ||
          xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_protocol")) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsWsHandshakeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* Upgrade 运行时使用严格 HTTP/1 报头和可选 permessage-deflate。 */
int xl6CgenNodeNeedsWsUpgradeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "WsUpgrade") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_upgrade_") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsWsUpgradeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* Stream 配置可独立于网络对象构造和验证。 */
int xl6CgenNodeNeedsWsStreamConfigRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "WebSocketConfig") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_stream_config_") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsWsStreamConfigRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* TCP 接管、事件与发送操作共享同一份无固定缓冲运行时。 */
int xl6CgenNodeNeedsWsStreamRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CLASS && node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "WebSocket") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_stream_") &&
         !xl6CgenNeedsBasicTextStartsWith(node->text, "xl6_xrt_ws_stream_config_") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsWsStreamRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}



/* WSS 适配只由显式 TLS 接管符号触发。 */
int xl6CgenNodeNeedsWsStreamTlsRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_MEMBER &&
         node->text != NULL &&
         (xl6CgenNeedsBasicTextEquals(node->text, "attachTls") ||
          xl6CgenNeedsBasicTextEquals(node->text, "attachTlsWithConfig")) &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         node->children[0]->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "WebSocket") ) {
        return 1;
    }
    if ( (node->kind == XL6_AST_EXTERN_FUNC ||
          node->kind == XL6_AST_NATIVE_EXTERN_FUNC) &&
         node->text != NULL &&
         xl6CgenNeedsBasicTextEquals(node->text, "xl6_xrt_ws_stream_attach_tls") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsWsStreamTlsRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
