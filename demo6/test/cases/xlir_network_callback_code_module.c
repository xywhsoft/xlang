#define main xlir_network_generated_main
#include XLIR_NETWORK_GENERATED_C
#undef main
/* Only the fixture adapter is exposed. No source rewriting, substitute
 * mapper, substitute class layout, engine initialization or fake native
 * handle is used. The host supplies a controlled real source Promise. */
XLIR_C_API xfuture* xlir_network_probe_map(xfuture* source)
{
#if defined(XLIR_NETWORK_RESOLVER)
    return xlir_datagram_future_map(xrtFutureRef(source),xlir_datagram_map_addresses,(void*)(uintptr_t)207);
#else
    return xl6_net_future_map(xrtFutureRef(source),xl6_net_map_bool);
#endif
}
