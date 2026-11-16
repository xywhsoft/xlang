#ifndef XL6_TEST_XGE_HOST_PROBE_H
#define XL6_TEST_XGE_HOST_PROBE_H
#include <xge.h>
/* The fixture owns OS handles only. No XRT/engine/compiler implementation or
 * runtime resolver is linked into the optional precompiled fixture library. */
typedef struct xge_test_host_probe_info {
    unsigned platformInit,graphicsInit,platformUnit,graphicsUnit;
    unsigned begins,ends,polls,presents,releases,makeCurrent;
    bool contextLive,platformLive;
} xge_test_host_probe_info;
int xgeTestHostConfigure(int backend,bool deferRelease);
int xgeTestHostPlatformInit(void);
int xgeTestHostGraphicsInit(void);
void xgeTestHostPlatformUnit(void);
void xgeTestHostGraphicsUnit(void);
int xgeTestHostMakeCurrent(void);
int xgeTestHostIsCurrent(void);
xge_host_release_result_t xgeTestHostRelease(void);
int xgeTestHostBegin(void);
int xgeTestHostEnd(void);
int xgeTestHostPoll(void);
void xgeTestHostPresent(void);
int xgeTestHostInfo(xge_test_host_probe_info*);
#endif
