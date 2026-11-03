#include "foreign_xui_choice.h"
int declarative_xui_choice_fire(xui_widget widget,int kind,int serial)
{
    if(kind<0 || kind>7)return XUI_ERROR_INVALID_ARGUMENT;
    if(kind==3){int status=xuiRadioSetChecked(widget,0);if(status!=XUI_OK)return status;}
    if(kind<5)return xuiWidgetPerformAccessibleAction(widget,1,XUI_ACCESSIBLE_ACTION_ACTIVATE,NULL);
    xui_accessible_value_t value={0};value.iSize=sizeof(value);value.iType=XUI_ACCESSIBLE_VALUE_NUMBER;value.fNumber=serial%2 ? 0.75:0.25;
    xui_accessible_node_t node={0};node.iSize=sizeof(node);
    int status=xuiWidgetGetAccessibleNode(widget,kind==6 ? 1:0,&node);
    return status==XUI_OK ? xuiWidgetPerformAccessibleAction(widget,node.iId,XUI_ACCESSIBLE_ACTION_SET_VALUE,&value):status;
}
