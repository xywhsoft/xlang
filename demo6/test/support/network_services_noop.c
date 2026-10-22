/* Standalone/JIT language counterpart. Host retention tests register permanent
 * callbacks instead; no generated/unloading code ever owns the external root. */
#include <stdint.h>
struct xvalue;
int64_t network_services_mode(void){return 0;}
void network_services_hold(struct xvalue* value){(void)value;}
void network_services_finalized(void){}
