#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
struct xvalue;
static unsigned counts[3];
static bool registered;
static void complete(void)
{
    for (unsigned i=0;i<3;++i) assert(counts[i]==1);
    puts("AOT Channel graph: three distinct finalizers completed before process exit");
}
bool message_graph_fail(void)
{
    if (!registered) { assert(atexit(complete)==0);registered=true; }
    return false;
}
void message_graph_drop(int64_t kind)
{
    assert(kind>=0 && kind<3 && !counts[kind]);++counts[kind];
}
void message_graph_capture(struct xvalue* value) { assert(value); }
