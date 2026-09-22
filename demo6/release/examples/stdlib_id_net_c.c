#define XRT_MODULE_XID
#define XRT_MODULE_NET_INTERFACE_TEXT
#include "xrt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_id_net(int ok)
{
    if ( !ok ) {
        fprintf(stderr, "id_net=fail\n");
        exit(1);
    }
}

int main(void)
{
    str requestId;
    str localIP;
    str localMAC;
    str localName;
    xid original;
    xid decoded;
    str encoded;

    requestId = xrtXidMakeString();
    localIP = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
    localMAC = xrtNetLocalHardwareString();
    localName = xrtNetHostNameString();

    require_id_net(requestId != NULL && strlen((const char*)requestId) == 32u);
    require_id_net(localIP != NULL);
    require_id_net(localMAC != NULL);
    require_id_net(localName != NULL);

    require_id_net(xrtXidMake(&original));
    encoded = xrtXidFormat(&original);
    require_id_net(encoded != NULL);
    require_id_net(xrtXidParse((xstrview){encoded, strlen(encoded)}, &decoded));
    require_id_net(xrtXidEqual(&original, &decoded));
    require_id_net(!xrtXidParse(XRT_STR_LITERAL("invalid"), &decoded));

    xrtFree(requestId);
    xrtFree(localIP);
    xrtFree(localMAC);
    xrtFree(localName);
    xrtFree(encoded);

    printf("id_net=ok\n");
    fflush(stdout);
    return 0;
}
