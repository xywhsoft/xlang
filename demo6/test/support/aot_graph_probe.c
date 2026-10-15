/* Observation callbacks only. The language owns every tested graph and
 * executes its original destructors; this file cannot collect any cycle. */
#include <__xl_runtime/xruntime_decl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned drops, mask;
#ifndef GRAPH_EXPECTED
#error GRAPH_EXPECTED must match the unchanged language fixture
#endif
static void verify(void)
{
    fprintf(stderr,"standalone graph finalizers=%u expected=%u mask=%u\n",drops,(unsigned)GRAPH_EXPECTED,mask);
    if(drops!=GRAPH_EXPECTED || mask!=(1u<<GRAPH_EXPECTED)-1u)_Exit(91);
}
bool graph_probe_begin(void) { return atexit(verify)==0; }
static void mark(unsigned index)
{
    assert(index<GRAPH_EXPECTED && !(mask & (1u<<index)));
    mask|=1u<<index;++drops;
}
void stack_graph_capture(xvalue* value) { (void)value;assert(0); }
void tree_graph_capture(xvalue* value) { (void)value;assert(0); }
void queue_graph_capture(xvalue* value) { (void)value;assert(0); }
void stack_graph_dropped(int64_t id,int64_t count)
{
#ifdef GRAPH_CONTROL
    assert(id==9 && count==0);
#else
    assert(id==7 && count==1);
#endif
    mark(0);
}
void tree_graph_dropped(int64_t id,int64_t values,int64_t arrays,int64_t second)
{
    int64_t expected[3]={0};assert(id>=7 && id<=11);
    unsigned family=(unsigned)(id-7);
    if(family==2)expected[1]=1;
    else {expected[0]=family==1 ? 2 : 1;if(family==3 || family==4)expected[2]=1;}
    assert(values==expected[0] && arrays==expected[1] && second==expected[2]);mark(family);
}
void queue_graph_dropped(int64_t id,int64_t a,int64_t b,int64_t c,int64_t futures,int64_t arrays,int64_t callbacks)
{
    assert(id>=7 && id<=12);int64_t counts[]={a,b,c,futures,arrays,callbacks};
    unsigned family=(unsigned)(id-7);
    for(unsigned at=0;at<6;++at)assert(counts[at]==(at==family ? family==4 ? 2 : 1 : 0));
    mark(family);
}
