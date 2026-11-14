/* Reuse only the existing CPU backend; input/owner implementation is SDK code. */
#include "foreign_xui_input_extended.c"
#include "foreign_xui_input_owned.h"
static unsigned ownedNativeKeys;
static int ownedNativeKey(xui_widget widget,const xui_event_t* event,void* user)
{(void)widget;(void)user;assert(event->iType==XUI_EVENT_HOTKEY);++ownedNativeKeys;return XUI_EVENT_DISPATCH_STOP;}
int fixtureOwnedNativeInstall(xui_context context,xui_widget target)
{
 const xui_callback_owner_t owner={{&references,&operations},hold,drop};ownedNativeKeys=0;
 return xuiHotKeyRegisterOwned(context,target,XUI_KEY_F6,0,ownedNativeKey,NULL,&owner);
}
void fixtureOwnedNativeHot(xui_context context)
{
 for(unsigned i=0;i<1000;++i){uint32_t flags=37;
  assert(xuiInputKeyDownEx(context,XUI_KEY_F6,0,&flags)==XUI_OK && flags==5);
 }
}
int fixtureOwnedNativeCount(void){return (int)ownedNativeKeys;}
