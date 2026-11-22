#include "foreign_xui_terminal_session.h"
#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;

void fixtureTerminalSessionBegin(void)
{
 assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
 xrtMemDebugSnapshot(&before);
}

void fixtureTerminalSessionBudget(int allowance)
{
 fixtureTerminalSessionBegin();
 assert(xrtMemDebugFailAfter(allowance));
}

void fixtureTerminalSessionRollback(void)
{
 xrtMemDebugFailClear();
 xmemdebugsnapshot after;
 xrtMemDebugSnapshot(&after);
 assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes);
 assert(after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount);
}

void fixtureTerminalSessionEnd(int native)
{
 xmemdebugsnapshot after;
 xrtMemDebugSnapshot(&after);
 printf("TerminalSession resize/poll %s alloc=%llu free=%llu realloc=%llu\n", native ? "C" : "xlang",
  (unsigned long long)(after.AllocCount-before.AllocCount),
  (unsigned long long)(after.FreeCount-before.FreeCount),
  (unsigned long long)(after.ReallocCount-before.ReallocCount));
 assert(after.AllocCount==before.AllocCount && after.FreeCount==before.FreeCount &&
  after.ReallocCount==before.ReallocCount);
}

void fixtureTerminalSessionNativeHot(xui_terminal_session_t* session)
{
 for(int i=0;i<1000;++i){
  assert(xuiTerminalSessionResize(session,24,5)==XUI_OK);
  assert(xuiTerminalSessionPoll(session)==0);
 }
}

static bool countEdge(xrtownershipref resource, void* raw)
{
 int* count = raw;
 assert(resource.Data && resource.Ops);
 ++*count;
 return true;
}

int fixtureTerminalSessionEdges(xui_terminal_session_t* session)
{
 xrtownershipref reference = xuiTerminalSessionOwnership(session);
 xrtownershipscope freeze = {0};
 int count = 0;
 assert(reference.Data && reference.Ops && xrtOwnershipFreezeTryBegin(&freeze));
 assert(reference.Ops->Trace(reference.Data, countEdge, &count));
 assert(xrtOwnershipScopeEnd(&freeze));
 return count;
}
