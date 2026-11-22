#include "foreign_xui_table_header_owned.h"

#include <assert.h>
#include <stdio.h>

static xmemdebugsnapshot before;
static xui_table_view_cell_output_t *held;

void fixtureTableShareBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void fixtureTableShareEnd(int native)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    printf("Table cell share %s: alloc=%llu free=%llu realloc=%llu\n",native?"native":"language",
        (unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(after.FreeCount-before.FreeCount),
        (unsigned long long)(after.ReallocCount-before.ReallocCount));
}

void fixtureTableShareMeasureNative(xui_table_view_cell_output_t *cell)
{
    xui_table_view_cell_output_t automatic;
    xuiTableViewCellOutputInit(&automatic);
    xui_table_view_cell_output_t *out=cell;
    assert(xuiTableViewCellOutputShareOwned(&automatic,&out)==XUI_ERROR_INVALID_ARGUMENT && !out);
    assert(xuiTableViewCellOutputDisposeChecked(&automatic)==XUI_OK);
    assert(xuiTableViewCellOutputShareOwned(NULL,&out)==XUI_ERROR_INVALID_ARGUMENT && !out);
    assert(xuiTableViewCellOutputShareOwned(cell,NULL)==XUI_ERROR_INVALID_ARGUMENT);
    assert(!held && xuiTableViewCellOutputShareOwned(cell,&held)==XUI_OK && held==cell);
    fixtureTableShareBegin();
    for(int i=0;i<1000;++i){
        assert(xuiTableViewCellOutputShareOwned(cell,&out)==XUI_OK && out==cell);
        assert(xuiTableViewCellOutputReleaseChecked(out)==XUI_OK);
    }
    fixtureTableShareEnd(1);
}

void fixtureTableShareSameCell(xui_table_view_cell_output_t *a,xui_table_view_cell_output_t *b)
{assert(a==b);}

void fixtureTableShareDropLast(void)
{
    xmemdebugsnapshot prior,after;
    assert(held);
    xrtMemDebugSnapshot(&prior);
    assert(xuiTableViewCellOutputReleaseChecked(held)==XUI_OK);
    held=NULL;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount<prior.LiveCount);
}

void fixtureTableHeaderFailAllocation(int allowance)
{
    if(allowance<0)xrtMemDebugFailClear();
    else assert(xrtMemDebugFailAfter(allowance));
}
