#include "app/xide_app.h"
#include "services/execution_target.h"
#include "ui/reading_state.h"
#include "model/metadata_read.h"
#include <stdio.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"startup_values_test:%d: %s\n",__LINE__,#v);return 1; } } while(0)
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
int main(void)
{
    xide_app app={0};xide_project project={0};
    CHECK(xideExecutionMode(&app)==XIDE_TARGET_AUTO);
    CHECK(!*xideExecutionProject(&app));
    CHECK(xideExecutionConfiguration(&app,&project)==NULL);
    CHECK(!xrtGetError());
    xvalue* invalid=xrtValueInt(7);CHECK(invalid);
    CHECK(!xideExecutionValid(invalid));CHECK(!xrtGetError());
    xrtValueRelease(invalid);
    CHECK(xuiCreate(&app.context)==XUI_OK);
    CHECK(xuiWidgetCreate(app.context,&app.root)==XUI_OK);
    CHECK(xuiSetRootWidget(app.context,app.root)==XUI_OK);
    CHECK(xideActionsInit(&app)==XUI_OK);
    for(size_t i=0;i<xideActionCount();++i) {
        const xide_action_info* action=xideActionAt(i);
        (void)xideActionQuery(&app,action->id);CHECK(!xrtGetError());
    }
    xui_text_edit_desc_t edit={0};edit.iSize=sizeof(edit);edit.bWordWrap=1;
    edit.sPlaceholder="Optional execution target must not poison this callback.";
    xui_widget widget=NULL;CHECK(xuiTextEditCreate(app.context,&widget,&edit)==XUI_OK);
    CHECK(!xrtGetError());xideActionsDestroy(&app);xuiDestroy(app.context);
    xvalue* values[]={xrtValueInt(3),xrtValueUInt(4),xrtValueFloat(5.5),xrtValueString(sv("bad")),xrtValueBool(true),NULL};
    const double expected[]={3,4,5.5,9,9,9};
    for(size_t i=0;i<6;++i) {
        CHECK(xideReadingNumber(values[i],9)==expected[i]);CHECK(!xrtGetError());
        xstrview text={0};CHECK(xideMetaString(values[i],&text)==(i==3));
        CHECK(xideMetaField(values[i],sv("missing"))==NULL);CHECK(xideMetaCount(values[i])==0);
        CHECK(!xrtGetError());
    }
    xrtSetErrorKind(XERR_IO);const xerror* prior=xrtGetError();CHECK(prior);
    CHECK(xideExecutionMode(&app)==XIDE_TARGET_AUTO);
    CHECK(xideReadingNumber(NULL,9)==9);CHECK(xideMetaCount(NULL)==0);
    CHECK(xrtGetError()==prior);xrtErrorFree(xrtTakeError());
    for(size_t i=0;i<5;++i)xrtValueRelease(values[i]);
    puts("startup value probes and checked control creation passed");return 0;
}
