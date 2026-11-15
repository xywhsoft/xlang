#ifndef FOREIGN_XUI_STATE_OUTPUTS_H
#define FOREIGN_XUI_STATE_OUTPUTS_H
#include "foreign_xui_render_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
void fixtureStateBegin(void);
void fixtureStateEqual(void);
void fixtureStateBudget(int);
void fixtureStateReset(void);
int fixtureStateBound(void);
void fixtureStateHotReport(int);
void fixtureStateNativeHot(xui_widget);
xui_doc_range_t fixtureStateRange(xui_document_snapshot,uint64_t);
xui_doc_range_t fixtureStateDocRange(xui_document,uint64_t);
void fixtureStateNodeCheck(xui_document_snapshot,uint64_t,const char*,const char*,const char*,const char*,const char*);
void fixtureStateLinkCheck(xui_document_snapshot,const xui_doc_range_t*,int,int,const char*,const char*);
void fixtureStateDockCheck(xui_widget,xvalue*);
void fixtureStateWindowCheck(xui_widget,int,int);
void fixtureStateNativeOracle(void);
#ifdef __cplusplus
}
#endif
#endif
