#ifndef XL_MEMCACHED_NATIVE_H
#define XL_MEMCACHED_NATIVE_H

#include "xrt.h"

/* Owned command lines use xrtFree. A zero frame length means incomplete. */
char *xmc_build_get(xvalue *keys);
char *xmc_build_set(xstrview key, int64 flags, int64 exptime, int64 bytes);
char *xmc_build_add(xstrview key, int64 flags, int64 exptime, int64 bytes);
char *xmc_build_delete(xstrview key);
char *xmc_build_touch(xstrview key, int64 exptime);
const char *xmc_build_stats(void);
const char *xmc_build_flush_all(void);
const char *xmc_build_version(void);
bool xmc_key_valid_abi(xstrview key);
void *xmc_build_set_abi(xstrview key, int64 flags, int64 exptime, int64 bytes);
void *xmc_build_add_abi(xstrview key, int64 flags, int64 exptime, int64 bytes);
void *xmc_build_delete_abi(xstrview key);
void *xmc_build_touch_abi(xstrview key, int64 exptime);
int64 xmc_frame_size(xbytesview data);
xvalue *xmc_parse_response_span(xbytesview data);
int64 xmc_frame_size_abi(void *data, size_t size);
xvalue *xmc_parse_response_abi(void *data, size_t size);

#endif
