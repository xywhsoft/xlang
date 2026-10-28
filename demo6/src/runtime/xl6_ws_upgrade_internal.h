#ifndef XL6_WS_UPGRADE_INTERNAL_H
#define XL6_WS_UPGRADE_INTERNAL_H
#include "xl6_ws_internal.h"
#include "xl6_http_internal.h"
typedef struct XL6_WsClientOptions {xwsupgradeclientconfig value;char data[];} XL6_WsClientOptions;
typedef struct XL6_WsServerOptions {xwsupgradeserverconfig value;xwsdeflate response;bool selected;char data[];} XL6_WsServerOptions;
typedef struct XL6_WsUpgrade {
    xwsupgrade value;
    xwsupgradeclientconfig client;
    xstrview host;
    xwsrole role;
    bool ready;
    char key[XWS_KEY_CAPACITY];
    char data[];
} XL6_WsUpgrade;
void* xl6WsUpgradeServerFromConfigV2(const XL6_HttpHead*,const xwsupgradeserverconfig*);
#endif
