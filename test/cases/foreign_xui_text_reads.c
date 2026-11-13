#include "foreign_xui_text_reads.h"
#include <assert.h>
#include <string.h>
static xmemdebugsnapshot before;
void fixtureTextReadBegin(void)
{assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xrtMemDebugSnapshot(&before);}
int fixtureTextReadAllocations(void)
{xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(after.ReallocCount==before.ReallocCount);return (int)(after.AllocCount-before.AllocCount);}
int fixtureTextReadWarm(xui_widget editor)
{xstrview view={0};return xuiCodeEditGetTextView(editor,&view);}
int fixtureTextReadSnapshot(xui_widget editor,xstrview snapshot)
{
    xstrview source={0};
    return xuiCodeEditGetTextView(editor,&source)==XUI_OK && source.Data && snapshot.Data &&
        source.Data!=snapshot.Data && source.Size==snapshot.Size && !memcmp(source.Data,snapshot.Data,source.Size);
}
