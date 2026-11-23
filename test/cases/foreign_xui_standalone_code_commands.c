#include "foreign_xui_standalone_code_commands.h"
#include <xui3_code_annotations.h>
#include <xui3_code_token_buffer.h>

#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static double started;

static void expectOwnedReleaseRejected(const XL6_ForeignViewDescriptorV1* policy, void* raw)
{
    assert(raw && (policy->flags & XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE));
    xl6_error_scope scope = {0};
    assert(xl6ErrorScopeBegin(&scope));
    assert(!policy->Destroy(raw));
    xl6_error_result result = xl6ErrorScopeEnd(&scope);
    assert(!result.ok && result.error && xrtErrorCode(result.error) == XUI_ERROR_INVALID_STATE);
    xrtErrorFree(result.error);
}

void fixtureCodeCommandCheckLeafRetirement(xui_widget editor)
{
    xui_code_annotation_store annotations = xuiCodeEditGetAnnotations(editor);
    xui_code_token_buffer tokens = xuiCodeEditGetTokenBuffer(editor);
    xui_code_selection_model selection = xuiCodeEditGetSelection(editor);
    expectOwnedReleaseRejected(&xl6Xui3CodeAnnotationStorePolicy, annotations);
    expectOwnedReleaseRejected(&xl6Xui3CodeTokenBufferPolicy, tokens);
    expectOwnedReleaseRejected(&xl6Xui3CodeSelectionPolicy, selection);
    assert(xuiCodeAnnotationGetDiagnosticCount(annotations) == 0);
    assert(xuiCodeTokenBufferGetCount(tokens) == 0);
    assert(xuiCodeSelectionGetCount(selection) >= 0);
    puts("XUI_CHECKED_LEAF_RETIREMENT_OK");
}

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
