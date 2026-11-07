#include "app/xide_app.h"
#include "ui/settings_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Observe the actual automatic-diagnostic handshake, rather than substituting
 * a fake language service or disabling the feature that exposed this failure. */
#include "../src/ui/language_view.c"

typedef struct menu_fixture {
    xide_app app;
    const char* directory;
    uint64_t deadline;
    int phase, page, completed, failed;
} menu_fixture;

static int failure(menu_fixture* f,const char* detail)
{
    fprintf(stderr,"settings_menu_frame_test: page=%d phase=%d: %s; XRT=%s\n",
            f->page,f->phase,detail,xrtErrorMessage(xrtGetError()));
    f->failed=1;
    xgeQuit();
    return XGE_ERROR;
}

static int postPointer(menu_fixture* f,xui_rect_t rect,int part)
{
    xge_input_event_t event={0};
    event.iSize=sizeof(event);
    event.iFlags=XGE_INPUT_EVENT_FLAG_SYNTHETIC;
    event.iType=part==0 ? XGE_EVENT_MOUSE_MOVE : part==1 ? XGE_EVENT_MOUSE_DOWN : XGE_EVENT_MOUSE_UP;
    event.fX=(rect.fX+rect.fW/2)*f->app.dpiScale;
    event.fY=(rect.fY+rect.fH/2)*f->app.dpiScale;
    event.iButton=XUI_POINTER_BUTTON_LEFT;
    event.iButtons=part==1 ? XUI_POINTER_BUTTON_LEFT : 0;
    return xgeInputEventPost(&event)==XGE_OK;
}

static int frame(void* user)
{
    menu_fixture* f=user;
    xide_app* app=&f->app;
    if(GetTickCount64()>f->deadline)return failure(f,"automatic diagnostics/menu deadline");
    if(f->phase) {
        int index=-1;
        for(int i=0;i<xuiMenuBarGetItemCount(app->menuBar);++i)
            if(!strcmp(xuiMenuBarGetItem(app->menuBar,i)->sText,"设置"))index=i;
        if(index<0)return failure(f,"missing Settings menu");
        xui_widget menu=xuiMenuBarGetItem(app->menuBar,index)->pMenu;
        if(f->phase<=3) {
            xui_rect_t rect=xuiMenuBarGetItemRect(app->menuBar,index),world=xuiWidgetGetWorldRect(app->menuBar);
            rect.fX+=world.fX;rect.fY+=world.fY;
            if(!postPointer(f,rect,f->phase-1))return failure(f,"posting menu-bar click");
        } else if(f->phase>=5 && f->phase<=7) {
            const int rows[]={0,2,3,4,5,6};
            xui_rect_t rect=xuiMenuGetItemRect(menu,rows[f->page]),world=xuiWidgetGetWorldRect(menu);
            rect.fX+=world.fX;rect.fY+=world.fY;
            if(!postPointer(f,rect,f->phase-5))return failure(f,"posting category click");
        } else if(f->phase>=9 && f->phase<=11) {
            xui_rect_t rect=xuiWidgetGetWorldRect(xideSettingsViewControl(app,XIDE_PREF_CANCEL));
            if(!postPointer(f,rect,f->phase-9))return failure(f,"posting Cancel click");
        }
    }
    int result=xideAppFrame(app);
    if(result!=XGE_OK || app->frameError || xrtGetError())return failure(f,"frame left a failure");
    if(!f->phase) {
        xide_language_view* v=app->languageView;
        if(v && v->initialized && v->autoEnabled && !v->busy && current(v)) {
            if(get(v->snapshotResult,"semanticDefinitions"))return failure(f,"expected current automatic protocol without optional snapshot flag");
            f->phase=1;
        } else Sleep(5);
    } else {
        if(f->phase==4 && !xuiMenuBarIsOpen(app->menuBar))return failure(f,"menu did not remain open");
        if(f->phase==8) {
            if(!xideSettingsViewIsOpen(app))return failure(f,"settings category did not open");
            if(f->page==0) {
                char* image=xidePathResolve(f->directory,"settings-menu.png");
                int okay=image && xideAppCapture(app,image)==XGE_OK;
                free(image);if(!okay)return failure(f,"settings screenshot");
            }
        }
        if(f->phase==12) {
            if(xideSettingsViewIsOpen(app))return failure(f,"Cancel did not close settings");
            if(++f->page==6) {f->completed=1;xgeQuit();return XGE_OK;}
            f->phase=1;
        } else ++f->phase;
    }
    xgeRenderRequest();
    return XGE_OK;
}

int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    menu_fixture f={0};f.directory=argv[1];f.deadline=GetTickCount64()+30000;
    char* entry=xidePathResolve(argv[1],"menu.xl");
    const char source[]="func main() as int\n    return 0\nend\n";
    if(!entry || !xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)source,sizeof(source)-1})) {free(entry);return 2;}
    f.app.hidden=1;f.app.noSession=1;f.app.initialFile=entry;f.app.compilerPath=argv[2];f.app.highDpi=1;
    xge_desc_t desc={0};desc.iWidth=1400;desc.iHeight=1000;desc.sTitle="IDE automatic diagnostics and settings menus";
    desc.iFlags=XGE_INIT_WINDOW | XGE_INIT_VSYNC | XGE_INIT_HIGHDPI | XGE_INIT_ON_DEMAND;
    int result=xgeInit(&desc);
    if(result==XGE_OK)result=xgeRun(frame,&f);
    if(result!=XGE_OK || xrtGetError())f.failed=1;
    xideAppDestroy(&f.app);xgeUnit();free(entry);
    if(!f.failed && f.completed)puts("settings_menu_frame_test passed: real default automatic diagnostics, optional snapshot flag absent, cross-frame pointer input, all six Settings entries and Cancel, no ambient XRT errors.");
    return !f.failed && f.completed ? 0 : 1;
}
