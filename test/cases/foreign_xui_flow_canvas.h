#ifndef FOREIGN_XUI_FLOW_CANVAS_H
#define FOREIGN_XUI_FLOW_CANVAS_H
#include "foreign_xui_render_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
void fixtureFlowCanvasBegin(void);
void fixtureFlowCanvasEqual(void);
void fixtureFlowCanvasBudget(int allowance);
void fixtureFlowCanvasReset(void);
int fixtureFlowCanvasDefaults(xui_widget graph,xui_widget workflow);
int fixtureFlowCanvasContextMenu(xui_widget target);
void fixtureFlowCanvasReport(int native);
void fixtureFlowCanvasNativeHot(xui_widget graph);
#ifdef __cplusplus
}
#endif
#endif
