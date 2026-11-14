/* CPU provider only. Measurement/layout/terminal logic comes from the SDK. */
#include "foreign_xui_widget_queries.h"
#include "foreign_xui_text_layout.c"
_Static_assert(sizeof(xui_terminal_cell_t)==96,"exact full terminal cell");
_Static_assert(offsetof(xui_terminal_cell_t,arrCombining)==32,"combining offset");
void fixtureQueryBudget(int n){fixtureLayoutBudget(n);}
void fixtureQueryCleanup(void)
{assert(references==1&&fonts==1&&paints==shapes&&surfaces==0&&draws==0);}
void fixtureQueryHeapBegin(void){fixtureLayoutHeapBegin();}
void fixtureQueryHeapReport(int kind){fixtureLayoutHeapReport(kind);}
void fixtureQueryWarm(xui_widget widget)
{xui_vec2_t size={0};assert(xuiWidgetMeasure(widget,(xui_vec2_t){500,300},&size)==XUI_OK);}
void fixtureQueryNativeHot(xui_widget widget,xui_widget terminal)
{
 for(int i=0;i<1000;++i){xui_vec2_t content={0},size={0};xui_terminal_cell_t cell={0};
  assert(xuiWidgetMeasureContent(widget,(xui_vec2_t){500,300},&content)==XUI_OK);
  assert(xuiWidgetMeasure(widget,(xui_vec2_t){500,300},&size)==XUI_OK);
  assert(xuiTerminalGetCell(terminal,0,0,&cell)==XUI_OK&&cell.iSize==sizeof(cell)&&cell.iCodepoint==65);
 }
}
