#include "foreign_xui_code_editing.h"
#include <assert.h>
#include <stdio.h>
static xmemdebugsnapshot before;
static size_t references=1;
static bool failDrop;
static bool count(const void* raw,size_t* out){assert(raw==&references);*out=references;return true;}
static bool trace(const void* raw,xrtownershipvisitor visit,void* user){(void)raw;(void)visit;(void)user;return true;}
static const xrtownershipops ops={count,trace};
static bool hold(const void* raw){assert(raw==&references);++references;return true;}
static bool drop(const void* raw){assert(raw==&references && references>1);--references;return !failDrop;}
static void changed(xui_code_document document,xui_code_range_t range,uint32_t version,void* user)
{(void)document;(void)range;(void)version;(void)user;}
int fixtureEditingFailEnd(xui_code_document document)
{
    assert(references==1);xui_callback_owner_t owner={{&references,&ops},hold,drop};
    int status=xuiCodeDocumentSetChangeCallbackOwned(document,changed,NULL,&owner);
    if(status==XUI_OK)failDrop=true;
    return status;
}
int fixtureEditingClearFailure(xui_code_document document)
{failDrop=false;return xuiCodeDocumentSetChangeCallbackOwned(document,NULL,NULL,NULL);}
int fixtureEditingReferences(void){return (int)references;}
void fixtureEditingBudget(int value)
{if(value<0)xrtMemDebugFailClear();else{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));assert(xrtMemDebugFailAfter(value));}}
void fixtureEditingBegin(void)
{assert(xrtMemDebugEnabled()||xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
void fixtureEditingReport(int native)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    printf("CodeEditing heap %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}
void fixtureEditingHot(xui_code_document document,xui_code_selection_model selection)
{
    for(int i=0;i<1000;++i){
        assert(xuiCodeEditingDeleteBackward(document,selection,0)==XUI_OK);
        assert(xuiCodeEditingDeleteForward(document,selection,0)==XUI_OK);
        assert(xuiCodeEditingDeleteWordBackward(document,selection,0)==XUI_OK);
        assert(xuiCodeEditingDeleteWordForward(document,selection,0)==XUI_OK);
        assert(xuiCodeEditingOutdentSelection(document,selection,4,0)==XUI_OK);
    }
}
