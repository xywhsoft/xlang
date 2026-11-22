#include "foreign_xui_code_edit_providers.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static xmemdebugsnapshot before;

void fixtureEditorProvidersBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureEditorProvidersEnd(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("CodeEdit providers command heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

static int nativeHover(xui_widget editor,int offset,xui_code_hover_reply_t* reply,void* user)
{
    assert(editor==user);
    xui_code_hover_t value={.iSize=sizeof(value),.tRange={offset,offset+1},.sText="print",.sContentType=""};
    return xuiCodeHoverReplySet(reply,&value);
}

static int borrowedHover(xui_widget editor,int offset,xui_code_hover_t* reply,void* user)
{
    (void)editor;
    (void)offset;
    (void)reply;
    int* calls=user;
    ++*calls;
    return XUI_OK;
}

static int nativeCommand(xui_widget editor,int command,int* handled,void* user)
{
    assert(editor==user);
    *handled=command==XUI_CODE_COMMAND_MOVE_RIGHT;
    return XUI_OK;
}

void fixtureEditorProvidersNative(xui_widget editor)
{
    xui_code_provider_set providers=xuiCodeEditGetProviders(editor);
    assert(providers);
    bool consumed=true;
    assert(xuiCodeProviderSetDestroyOwnerChecked(providers,&consumed)==XUI_ERROR_INVALID_STATE && !consumed);
    int rawCalls=0;
    xui_code_hover_snapshot captured=NULL;
    assert(xuiCodeProviderSetHover(providers,borrowedHover,&rawCalls)==XUI_OK);
    assert(xuiCodeProviderCaptureHover(providers,editor,0,&captured)==XUI_ERROR_UNSUPPORTED && !captured && !rawCalls);
    assert(xuiCodeEditSetHoverProviderOwned(editor,nativeHover,editor,NULL)==XUI_OK);
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    for (int allowance=0;allowance<2;++allowance) {
        assert(xrtMemDebugFailAfter(allowance));
        int status=xuiCodeProviderCaptureHover(providers,editor,0,&captured);
        xrtMemDebugFailClear();
        assert(status==XUI_ERROR_OUT_OF_MEMORY && !captured);
        xrtErrorFree(xrtTakeError());
    }
    assert(xuiCodeProviderCaptureHover(providers,editor,0,&captured)==XUI_OK);
    assert(xuiCodeProviderSetClearChecked(providers)==XUI_OK);
    xui_code_hover_t info={0};
    assert(xuiCodeHoverSnapshotGetInfo(captured,&info)==XUI_OK);
    assert(info.tRange.iStart==0 && info.tRange.iEnd==1);
    assert(strcmp(info.sText,"print")==0 && info.sContentType && !*info.sContentType);
    xuiCodeHoverSnapshotDestroy(captured);

    assert(xuiCodeEditSetCommandProviderOwned(editor,nativeCommand,editor,NULL)==XUI_OK);
    fixtureEditorProvidersBegin();
    for (int i=0;i<1000;++i) {
        int handled=0;
        assert(xuiCodeProviderExecuteCommand(providers,editor,XUI_CODE_COMMAND_MOVE_RIGHT,NULL,&handled)==XUI_OK && handled);
    }
    fixtureEditorProvidersEnd(1);
    assert(xuiCodeProviderSetClearChecked(providers)==XUI_OK);
}
