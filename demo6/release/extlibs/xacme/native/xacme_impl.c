/* Upstream implementation only. XRT itself remains the shared host runtime. */
#define XRT_MODULE_ALL
#include "lib/xrt/xrt.h"
#include "../upstream/host-features.h"
#include "host_compat.h"
#define XACME_MODULE_ALL
#define XACME_IMPLEMENTATION
#include "../upstream/xacme.h"
#undef xrtFutureWaitFor
#undef xrtNetDialAsync
#undef xrtTlsDialAsync
#undef xrtNetUdpReceiveWait
