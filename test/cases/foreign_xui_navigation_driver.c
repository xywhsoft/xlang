/* Fixture publishers/setup only; generated library methods call XUI directly. */
#include "foreign_xui_navigation.h"
int declarative_xui_navigation_setup(xui_widget w,int k)
{
    int result=xuiWidgetSetRect(w,(xui_rect_t){0,0,240,48});if(result!=XUI_OK)return result;
    if(k==1)return xuiPageSetPageCount(w,2);
    if(k==2)return xuiCarouselSetPageCount(w,2);
    if(k>=3){const xui_breadcrumb_item_t item={"root",1,71};return xuiBreadcrumbSetItems(w,&item,1);}
    return XUI_OK;
}
int declarative_xui_navigation_fire(xui_widget w,int k,int serial)
{
    if(k==0)return xuiWidgetPerformAccessibleAction(w,1,XUI_ACCESSIBLE_ACTION_ACTIVATE,NULL);
    if(k==1)return xuiPageSetCurrent(w,serial%2 ? 1:2,1);
    if(k==2)return xuiCarouselSetCurrent(w,serial%2 ? 0:1,1);
    if(k==3)return xuiWidgetPerformAccessibleAction(w,2,XUI_ACCESSIBLE_ACTION_ACTIVATE,NULL);
    if(k!=4)return XUI_ERROR_INVALID_ARGUMENT;
    xui_event_t event={0};event.iSize=sizeof(event);event.iType=XUI_EVENT_CONTEXT_MENU;event.iKey=XUI_KEY_CONTEXT_MENU;event.pTarget=w;
    return xuiDispatchEvent(xuiWidgetGetContext(w),&event);
}
