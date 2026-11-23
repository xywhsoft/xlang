#include "foreign_xui_property_editor_config.h"
#include <string.h>

static xui_widget find_child(xui_widget parent, xui_widget_type type)
{
    for ( xui_widget child=xuiWidgetGetFirstChild(parent); child; child=xuiWidgetGetNextSibling(child) ) {
        if ( xuiWidgetGetType(child)==type ) return child;
        xui_widget nested=find_child(child,type);
        if ( nested ) return nested;
    }
    return NULL;
}

int fixturePropertyEditorNumeric(xui_widget grid)
{
    xui_widget number=find_child(grid,xuiNumericInputGetType(xuiWidgetGetContext(grid)));
    float minimum=0,maximum=0;
    return number && xuiNumericInputGetRange(number,&minimum,&maximum)==XUI_OK &&
        minimum==2 && maximum==12 && xuiNumericInputGetStep(number)==2 && xuiNumericInputGetPrecision(number)==0;
}

int fixturePropertyEditorCombo(xui_widget grid)
{
    xui_widget combo=find_child(grid,xuiComboBoxGetType(xuiWidgetGetContext(grid)));
    if ( !combo || !xuiWidgetGetEffectiveVisible(combo) ||
         xuiComboBoxGetItemCount(combo)!=2 || xuiComboBoxGetSelected(combo)!=0 ) return 0;
    const xui_combobox_item_t* first=xuiComboBoxGetItem(combo,0);
    const xui_combobox_item_t* second=xuiComboBoxGetItem(combo,1);
    return first && first->sText && !strcmp(first->sText,"Draft") && first->iValue==10 && first->bEnabled &&
        second && second->sText && !strcmp(second->sText,"Ready") && second->iValue==20 && !second->bEnabled;
}
