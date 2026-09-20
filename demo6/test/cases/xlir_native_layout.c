#include <stdio.h>
#include <xrt.h>
#include "internal/xrt_tcp.h"

int main(void)
{
    size_t limit = 576u;
    unsigned dial = 0, future = 0;
#if defined(XRT_FEATURE_NET_TCP_DIAL)
    limit += 40u; dial = 1;
#endif
#if defined(XRT_FEATURE_NET_TCP_FUTURE)
    limit += 64u; future = 1;
#endif
    printf("{\"PointerBytes\":%zu,\"Stream\":%zu,\"StreamLimit\":%zu,"
        "\"Buffer\":%zu,\"Command\":%zu,\"Post\":%zu,\"Dial\":%u,\"Future\":%u}\n",
        sizeof(void*), sizeof(xnetstream), limit, sizeof(xnetbuf),
        sizeof(__xrt_net_engine_internal), sizeof(xnetpost), dial, future);
    return sizeof(xnetstream) <= limit && sizeof(xnetbuf) <= 6u*sizeof(void*)+8u &&
        sizeof(xrt_net_post_impl) <= XNET_POST_STORAGE_SIZE ? 0 : 1;
}
