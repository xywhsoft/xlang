#ifndef XIDE_DIALOG_TEST_HELPERS_H
#define XIDE_DIALOG_TEST_HELPERS_H
#include "ui/dialog.h"
/* A fixed action is outside the scroll content by design. Never scroll the
 * body or clip a footer click against the body viewport. */
static inline int xideTestInScroll(xui_widget scroll,xui_widget child)
{
 for(xui_widget p=child;p;p=xuiWidgetGetParent(p))if(p==scroll)return 1;
 return 0;
}
static inline xui_widget xideTestViewport(xui_widget scroll,xui_widget child)
{return xideTestInScroll(scroll,child) ? scroll:xuiWidgetGetParent(scroll);}
static inline int xideTestReveal(xui_widget scroll,xui_widget child)
{
 if(xideTestInScroll(scroll,child))return xuiScrollViewEnsureChildVisible(scroll,child);
 for(xui_widget p=child;p;p=xuiWidgetGetParent(p))
  if(p==xuiWidgetGetParent(scroll))return XUI_OK;
 return XUI_ERROR_INVALID_ARGUMENT;
}
static inline xui_rect_t xideTestMenuItem(xui_widget menu,int index)
{
 xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu);
 r.fX+=world.fX;r.fY+=world.fY;
 xui_widget popup=xuiMenuGetPopupWidget(menu);
 if(popup) {
  xui_rect_t view=xuiWidgetGetWorldRect(xuiPopupGetViewportWidget(popup));
  int right=r.fX+r.fW,bottom=r.fY+r.fH;
  if(r.fX<view.fX)r.fX=view.fX;
  if(r.fY<view.fY)r.fY=view.fY;
  if(right>view.fX+view.fW)right=view.fX+view.fW;
  if(bottom>view.fY+view.fH)bottom=view.fY+view.fH;
  r.fW=right-r.fX;r.fH=bottom-r.fY;
 }
 return r;
}
#endif
