/* The backend is shared with input acceptance; notifications are actual SDK. */
#include "foreign_xui_input_extended.c"
#include "foreign_xui_accessibility_observer.h"
static unsigned accessNativeCalls;
static void accessNative(xui_context context,xui_widget widget,const xui_accessibility_event_t* event,void* user)
{
 (void)context;(void)widget;(void)user;
 assert(event->iType==XUI_ACCESSIBLE_EVENT_STATE_CHANGED&&event->iNodeId==UINT64_C(4294967297));++accessNativeCalls;
}
int fixtureAccessNativeInstall(xui_context context)
{
 const xui_callback_owner_t owner={{&references,&operations},hold,drop};accessNativeCalls=0;
 return xuiSetAccessibilityEventCallbackOwned(context,accessNative,NULL,&owner);
}
void fixtureAccessNativeHot(xui_widget widget)
{for(unsigned i=0;i<1000;++i)assert(xuiWidgetNotifyAccessibility(widget,XUI_ACCESSIBLE_EVENT_STATE_CHANGED,UINT64_C(4294967297))==XUI_OK);}
int fixtureAccessNativeCount(void){return (int)accessNativeCalls;}
