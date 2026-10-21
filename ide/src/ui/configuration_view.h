#ifndef XLANG_IDE_CONFIGURATION_VIEW_H
#define XLANG_IDE_CONFIGURATION_VIEW_H
#include "app/xide_app.h"
enum xide_configuration_control {
    XIDE_CONFIG_PICKER, XIDE_CONFIG_NAME, XIDE_CONFIG_OUTPUT, XIDE_CONFIG_DIRECTORY,
    XIDE_CONFIG_CATEGORY, XIDE_CONFIG_LIST, XIDE_CONFIG_KEY, XIDE_CONFIG_VALUE,
    XIDE_CONFIG_NEW, XIDE_CONFIG_CLONE, XIDE_CONFIG_DELETE, XIDE_CONFIG_ADD,
    XIDE_CONFIG_UPDATE, XIDE_CONFIG_REMOVE, XIDE_CONFIG_UP, XIDE_CONFIG_DOWN,
    XIDE_CONFIG_APPLY, XIDE_CONFIG_CANCEL, XIDE_CONFIG_TEST_PROGRAM, XIDE_CONFIG_TEST_DIRECTORY, XIDE_CONFIG_CONTROL_COUNT
};
int xideConfigurationViewCreate(xide_app* app);
void xideConfigurationViewDestroy(xide_app* app);
int xideConfigurationViewOpen(xide_app* app);
int xideConfigurationViewIsOpen(xide_app* app);
void xideConfigurationViewLayout(xide_app* app);
xui_widget xideConfigurationViewControl(xide_app* app,int control);
xui_widget xideConfigurationViewWindow(xide_app* app);
int xideConfigurationViewApply(xide_app* app,xide_model_error* error);
#endif
