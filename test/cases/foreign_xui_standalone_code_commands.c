#include "foreign_xui_standalone_code_commands.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static double started;

void fixtureCodeCommandArmOom(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(0));
}

bool fixtureCodeCommandClearOom(void)
{
    bool triggered=xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    return triggered;
}

void fixtureCodeCommandBeginMeasure(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    started=xrtTimer();
}

void fixtureCodeCommandEndMeasure(int language)
{
    double elapsed=xrtTimer()-started;
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount);
    assert(after.ReallocCount==before.ReallocCount && after.LiveCount==before.LiveCount);
    printf("XUI_COMMAND_SAMPLE language=%d count=1000 alloc=0 free=0 realloc=0 wallMs=%.6f\n",
        language,elapsed*1000.0);
}

void fixtureCodeCommandMeasureNative(xui_code_document document,xui_code_selection_model selection)
{
    const xui_code_command_context_t commandContext={
        .iSize=sizeof(commandContext),.pDocument=document,.pSelection=selection,.iIndentColumns=4
    };
    int handled=0;
    fixtureCodeCommandBeginMeasure();
    for(int i=0;i<500;++i){
        assert(xuiCodeCommandExecute(&commandContext,XUI_CODE_COMMAND_MOVE_DOCUMENT_START,&handled)==XUI_OK && handled==1);
        assert(xuiCodeCommandExecute(&commandContext,XUI_CODE_COMMAND_MOVE_DOCUMENT_END,&handled)==XUI_OK && handled==1);
    }
    fixtureCodeCommandEndMeasure(0);
}
