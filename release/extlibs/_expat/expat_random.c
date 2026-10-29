/*
	Expat 的随机源按目标平台选择；包清单始终只编译这一入口，避免把
	不适用于目标平台的系统调用源码交给编译器。
*/
#if defined(_WIN32)
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#include "xrt.h"

/* Expat 需要密码安全熵；复用 XRT 的跨 CRT 实现，不依赖 rand_s。 */
bool writeRandomBytes_rand_s(void* target, size_t count)
{
	return xrtSecureRandom(target, count);
}
#elif defined(__linux__)
#include "vendor/expat/lib/random_getrandom.c"
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
	defined(__NetBSD__)
#include "vendor/expat/lib/random_arc4random_buf.c"
#else
#include "vendor/expat/lib/random_dev_urandom.c"
#endif
