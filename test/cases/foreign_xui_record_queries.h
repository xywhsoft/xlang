#ifndef FOREIGN_XUI_RECORD_QUERIES_H
#define FOREIGN_XUI_RECORD_QUERIES_H
#include "foreign_xui_render_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct QueryAccessibleScalars {
 uint64_t id,parent;
 int role;
 uint32_t state;
 xui_rect_t bounds;
 int start,endOffset,level,row,column,rows,columns;
 uint32_t actions;
 xui_accessible_range_t range;
} QueryAccessibleScalars;
int fixtureQueryAccessible(xui_widget,QueryAccessibleScalars*);
void fixtureQueryAccessibleText(xui_widget,int,const char*,bool);
void fixtureQueryBegin(void);
void fixtureQueryEqual(void);
void fixtureQueryBudget(int);
void fixtureQueryReset(void);
void fixtureQueryReport(int,int);
void fixtureQueryNativeHot(int,xui_context,xui_widget,xui_code_annotation_store);
void fixtureQueryNativeOracle(void);
#ifdef __cplusplus
}
#endif
#endif
