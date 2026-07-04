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

    xrtInit();

    requestId = xrtMakeXIDS();
    localIP = xrtGetLocalIP();
    localMAC = xrtGetLocalMAC();
    localName = xrtGetLocalName();

    require_id_net(requestId != NULL && strlen((const char*)requestId) == 32u);
    require_id_net(localIP != NULL);
    require_id_net(localMAC != NULL);
    require_id_net(localName != NULL);

    xrtFree(requestId);
    xrtFree(localIP);
    xrtFree(localMAC);
    xrtFree(localName);

    printf("id_net=ok\n");
    xrtUnit();
    return 0;
}
