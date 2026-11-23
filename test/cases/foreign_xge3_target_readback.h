#ifndef FOREIGN_XGE3_TARGET_READBACK_H
#define FOREIGN_XGE3_TARGET_READBACK_H
#include <xge3_render_target.h>
#ifdef __cplusplus
extern "C" {
#endif
void fixtureReadbackCheck(xge_render_target target);
void fixtureReadbackHeapStart(void);
void fixtureReadbackHeapReport(int mode);
void fixtureReadbackBudget(int allowance);
#ifdef __cplusplus
}
#endif
#endif
