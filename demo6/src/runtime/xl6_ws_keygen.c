#include "xl6_ws_internal.h"
xstrview xl6WsHandshakeKeyV2(void)
{char key[XWS_KEY_CAPACITY];return xrtWsKeyGenerate(key,sizeof(key))?xl6TextCopy((xstrview){key,XWS_KEY_SIZE}):xl6TextFailure;}
