#include "src/compile/xl6_component_set.h"
#include <assert.h>
#include <stdio.h>

enum { MAX_BITS=257, MAX_WORDS=5 };
typedef struct Graph {
    size_t bits, edges[MAX_BITS][3];
    int invalid;
} Graph;
static int dependencies(void* context, size_t id, const size_t** ids, size_t* count)
{
    Graph* graph=context;
    if (graph->invalid==1) return 0;
    *ids=graph->invalid==2 ? NULL : graph->edges[id]; *count=3;
    return 1;
}
static void verify_graph(size_t bits)
{
    Graph graph={.bits=bits};
    unsigned char reaches[MAX_BITS][MAX_BITS]={{0}};
    for (size_t i=0;i<bits;++i) {
        /* Disconnected 17-node rings cross word boundaries. A broken close
         * that merely fills every bit must fail this independent oracle. */
        size_t start=(i/17)*17, width=bits-start < 17 ? bits-start : 17;
        graph.edges[i][0]=i; graph.edges[i][1]=start+(i-start+1)%width;
        graph.edges[i][2]=start+((i-start)*5+3)%width;
        reaches[i][i]=reaches[i][graph.edges[i][1]]=reaches[i][graph.edges[i][2]]=1;
    }
    for (size_t k=0;k<bits;++k) for (size_t i=0;i<bits;++i) for (size_t j=0;j<bits;++j)
        reaches[i][j] |= reaches[i][k] && reaches[k][j];
    for (size_t a=0;a<bits;++a) for (size_t b=0;b<bits;++b) {
        struct { uint64_t before, words[MAX_WORDS], after; } storage;
        memset(&storage,0,sizeof(storage)); storage.before=storage.after=UINT64_MAX;
        size_t words=xl6ComponentSetWordCount(bits);
        for (size_t i=words;i<MAX_WORDS;++i) storage.words[i]=UINT64_MAX;
        assert(xl6ComponentSetAdd(storage.words,bits,a) && xl6ComponentSetAdd(storage.words,bits,b));
        assert(xl6ComponentSetClose(storage.words,bits,dependencies,&graph));
        for (size_t i=0;i<bits;++i) assert(xl6ComponentSetHas(storage.words,bits,i)==(reaches[a][i]||reaches[b][i]));
        assert(storage.before==UINT64_MAX && storage.after==UINT64_MAX);
        for (size_t i=words;i<MAX_WORDS;++i) assert(storage.words[i]==UINT64_MAX);
    }
    uint64_t set[MAX_WORDS]={0}, snapshot[MAX_WORDS]={0};
    size_t ids[]={0,bits-1,0};
    assert(xl6ComponentSetAddMany(set,bits,ids,3)); memcpy(snapshot,set,sizeof(set));
    ids[1]=bits;
    assert(!xl6ComponentSetAddMany(set,bits,ids,3) && !memcmp(set,snapshot,sizeof(set)));
    assert(!xl6ComponentSetAdd(set,bits,bits) && !xl6ComponentSetHas(set,bits,bits));
    assert(xl6ComponentSetUnion(set,set,bits) && xl6ComponentSetEqual(set,snapshot,bits));
    graph.edges[0][0]=bits;
    assert(!xl6ComponentSetClose(set,bits,dependencies,&graph) && xl6ComponentSetEmpty(set,bits));
    graph.edges[0][0]=0;
    for (int invalid=1;invalid<=2;++invalid) {
        graph.invalid=invalid; assert(xl6ComponentSetAdd(set,bits,0));
        assert(!xl6ComponentSetClose(set,bits,dependencies,&graph) && xl6ComponentSetEmpty(set,bits));
    }
    graph.invalid=0;
    for (size_t i=0;i<bits;++i) {
        graph.edges[i][0]=i; graph.edges[i][1]=graph.edges[i][2]=i?i-1:0;
    }
    assert(xl6ComponentSetAdd(set,bits,bits-1));
    assert(xl6ComponentSetClose(set,bits,dependencies,&graph));
    for (size_t i=0;i<bits;++i) assert(xl6ComponentSetHas(set,bits,i));
    xl6ComponentSetClear(set,bits);
    if (bits%64) {
        set[xl6ComponentSetWordCount(bits)-1]=UINT64_C(1)<<(bits%64);
        assert(!xl6ComponentSetValid(set,bits));
        assert(!xl6ComponentSetClose(set,bits,dependencies,&graph) && xl6ComponentSetEmpty(set,bits));
    }
}
int main(void)
{
    size_t sizes[]={1,63,64,65,127,128,129,130,256,257};
    assert(xl6ComponentSetWordCount(0)==0 && xl6ComponentSetWordCount(SIZE_MAX)==SIZE_MAX/64+1);
    assert(xl6ComponentSetValid(NULL,0) && xl6ComponentSetEmpty(NULL,0));
    assert(!xl6ComponentSetValid(NULL,1) && !xl6ComponentSetAdd(NULL,64,0));
    assert(!xl6ComponentSetHas(NULL,64,0) && !xl6ComponentSetClose(NULL,0,NULL,NULL));
    assert(!xl6ComponentSetUnion(NULL,NULL,1) && !xl6ComponentSetEqual(NULL,NULL,1));
    for (size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) verify_graph(sizes[i]);
    puts("component set: 10 populations through 257, all root pairs vs Floyd oracle, word/tail boundaries, cycles/duplicates/self edges, bad providers/edges clear, canaries intact");
    return 0;
}
