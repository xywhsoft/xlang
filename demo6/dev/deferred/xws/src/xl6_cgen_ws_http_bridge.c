static const char xl6WebSocketHttpBridgeSource[] = XL6_WS_SOURCE(
int xl6_xrt_http_response_upgrade_websocket(void* raw, void* serverRaw)
{
    XL6_Http_Response* response = (XL6_Http_Response*)raw;
    XL6_WsServer* server = (XL6_WsServer*)serverRaw;
    if ( response == 0 || response->Committed || response->Connection == 0 || response->Request == 0 || response->Request->Request == 0 || server == 0 ) { return false; }
    if ( !xl6_ws_server_upgrade(server, response->Connection, response->Request->Request) ) { return false; }
    response->Committed = true;
    return true;
}
) "\n";



/* WebSocket 运行时按依赖顺序输出，HTTP bridge 由模块生成器单独控制。 */
