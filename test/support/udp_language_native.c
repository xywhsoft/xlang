/* The ONLY fake input is an offline fixture value, constructed by the actual
 * native allocator. Never probe an OS error queue or send malformed traffic. */
#include "udp-language-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
xnetudppacket* udpFixturePacket(const xnetaddr* address)
{return __xrtNetUdpPacketCreate(address,NULL,"_p\0q_",5,0);}
xnetudperrorpacket* udpFixtureError(void)
{xnetdgramerror error={0};return __xrtNetUdpErrorPacketCreate(&error,"e\0r",3);}
