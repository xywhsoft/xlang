#include <stdio.h>
#include <string.h>

#include "../include/serial.h"
#ifdef __TINYC__
/* TCC 单文件范例复用包内串口实现。 */
#include "../serial_native.c"
#endif

int main(int argc, char** argv)
{
    void* port;
    xbuffer* reply;
    int ok;
    if (argc != 2) return 2;
    port = xserial_open(argv[1], 115200, 8, 1, 0);
    if (port == NULL) return 1;
    if (!xserial_set_timeouts(port, 1500, 1000)) {
        xserial_close(port);
        return 1;
    }
    if (xserial_write_text(port, "PING") != 4) {
        xserial_close(port);
        return 1;
    }
    reply = (xbuffer*)xserial_read(port, 4);
    ok = reply != NULL && reply->Size == 4 && memcmp(reply->Data, "PONG", 4) == 0;
    xrtBufferDestroy(reply);
    xserial_close(port);
    if (!ok) return 1;
    puts("serial device: PASS");
    return 0;
}
