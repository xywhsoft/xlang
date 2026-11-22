#include "foreign_xui_code_provider_set.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot before;

void fixtureCodeProviderBudget(int allowance)
{
    if (allowance < 0) xrtMemDebugFailClear();
    else {
        assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
        assert(xrtMemDebugFailAfter(allowance));
    }
}

void fixtureCodeProviderBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureCodeProviderEnd(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("CodeProviderSet command heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

static int nativeCommand(xui_widget editor,int command,int* handled,void* user)
{
    (void)editor;
    *handled=command==*(const int*)user;
    return XUI_OK;
}

static int nativeCompletion(xui_widget editor,int offset,xui_code_completion_reply_t* reply,void* user)
{
    (void)editor;
    (void)offset;
    if (user) assert(xuiCodeProviderSetClearChecked(user)==XUI_OK);
    xui_code_completion_item_t item={.iSize=sizeof(item),.sLabel="print",.sDetail=""};
    return xuiCodeCompletionReplyAppendItem(reply,&item);
}

void fixtureCodeProviderNative(xui_widget editor)
{
    xui_code_provider_set providers=NULL;
    assert(xuiCodeProviderSetCreate(&providers)==XUI_OK);
    int command=XUI_CODE_COMMAND_MOVE_RIGHT;
    assert(xuiCodeProviderSetCommandHandlerOwned(providers,nativeCommand,&command,NULL)==XUI_OK);
    fixtureCodeProviderBegin();
    for (int i=0;i<1000;++i) {
        int handled=0;
        assert(xuiCodeProviderExecuteCommand(providers,editor,command,NULL,&handled)==XUI_OK && handled);
    }
    fixtureCodeProviderEnd(1);

    xui_code_completion_snapshot result=NULL;
    assert(xuiCodeProviderCaptureCompletion(providers,editor,0,"",1,&result)==XUI_ERROR_UNSUPPORTED && !result);
    assert(xuiCodeProviderSetCompletionReplyOwned(providers,nativeCompletion,NULL,NULL)==XUI_OK);
    for (int allowance=0;allowance<2;++allowance) {
        fixtureCodeProviderBudget(allowance);
        int status=xuiCodeProviderCaptureCompletion(providers,editor,0,"",1,&result);
        fixtureCodeProviderBudget(-1);
        assert(status==XUI_ERROR_OUT_OF_MEMORY && !result);
        xrtErrorFree(xrtTakeError());
    }
    assert(xuiCodeProviderSetCompletionReplyOwned(providers,nativeCompletion,providers,NULL)==XUI_OK);
    assert(xuiCodeProviderCaptureCompletion(providers,editor,0,"",1,&result)==XUI_OK);
    assert(xuiCodeProviderSetDestroyChecked(providers)==XUI_OK);
    xui_code_completion_item_t item={0};
    assert(xuiCodeCompletionSnapshotGetCount(result)==1);
    assert(xuiCodeCompletionSnapshotGetItem(result,0,&item)==XUI_OK);
    assert(strcmp(item.sLabel,"print")==0 && item.sDetail && item.sDetail[0]=='\0' && !item.sDocumentation);
    xuiCodeCompletionSnapshotDestroy(result);
}
