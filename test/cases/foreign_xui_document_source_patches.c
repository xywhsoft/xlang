#include "foreign_xui_document_source_patches.h"
#include <assert.h>
static xmemdebugsnapshot patchBefore;
void fixtureDocPatchBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&patchBefore);}
void fixtureDocPatchBudget(int count)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));if(count<0)xrtMemDebugFailClear();else assert(xrtMemDebugFailAfter(count));}
void fixtureDocPatchEndBudget(void)
{xrtMemDebugFailClear();xrtClearError();}
void fixtureDocPatchBalanced(void)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==patchBefore.LiveCount && after.LiveBytes==patchBefore.LiveBytes);
    assert(after.InvalidFreeCount==patchBefore.InvalidFreeCount && after.DoubleFreeCount==patchBefore.DoubleFreeCount);
}
