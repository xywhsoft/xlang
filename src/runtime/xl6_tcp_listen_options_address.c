#include "xl6_tcp_options_internal.h"
void* xl6TcpListenOptionsAddressV1(const void* raw)
{void* result=xl6NetAddressCopyV1(&((const xnetlistenconfig*)raw)->Address);if(!result)xl6TcpOptionsFailedV1();return result;}
