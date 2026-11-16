#ifndef FOREIGN_XGE3_HOST_VALUE_H
#define FOREIGN_XGE3_HOST_VALUE_H
#include <xge3_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif
xge_host_session_desc_t fixtureHostValueCopy(xge_host_session_desc_t);
int fixtureHostValueRead(const xge_host_session_desc_t*,xge_host_session_desc_t*,int);
xge_host_session_desc_t fixtureHostValueCpp(xge_host_session_desc_t);
size_t fixtureHostValueSize(void);
void fixtureHostValueHeapBegin(void);
bool fixtureHostValueHeapEqual(void);
bool fixtureHostValueNativeHot(void);
#ifdef __cplusplus
}
#endif
#endif
