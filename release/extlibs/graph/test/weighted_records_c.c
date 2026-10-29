/* 浮点距离以位模式输出，独立 Python Bellman-Ford 不复用本包的堆。 */
#include "graph_test.h"
#include <stdint.h>

typedef struct { unsigned kind; uint64_t value; } weight_spec;
/* kind: 0 为 IEEE 位模式，1/2 为有符号/无符号整数，进入算法时转 float64。 */
static const weight_spec weights[] = {
    {0,UINT64_C(0)}, {0,UINT64_C(0x8000000000000000)}, {0,UINT64_C(1)},
    {0,UINT64_C(0x0010000000000000)}, {0,UINT64_C(0x0010000000000001)},
    {0,UINT64_C(0x3cb0000000000000)}, {0,UINT64_C(0x3ff0000000000000)},
    {0,UINT64_C(0x4000000000000000)}, {0,UINT64_C(0x4340000000000000)},
    {0,UINT64_C(0x7fdfffffffffffff)}, {0,UINT64_C(0x7feffffffffffffe)},
    {0,UINT64_C(0x7fefffffffffffff)}, {1,INT64_MAX},
    {1,UINT64_C(0x0020000000000001)}, {2,UINT64_MAX}, {2,UINT64_C(0x8000000000000000)}
};

static bool add_edge(xvalue* graph,unsigned from,unsigned to,unsigned choice)
{
    char a[]={'n',(char)('0'+from),0}, b[]={'n',(char)('0'+to),0};
    weight_spec spec=weights[choice];
    xvalue *row=xrtValueArray(), *value=NULL;
    if (spec.kind==0) { double number; memcpy(&number,&spec.value,sizeof(number)); value=xrtValueFloat(number); }
    else if (spec.kind==1) value=xrtValueInt((int64)spec.value);
    else value=xrtValueUInt(spec.value);
    if (!row || !value || !xrtValueArrayAppendNew(row,text(a,2)) ||
        !xrtValueArrayAppendNew(row,text(b,2)) || !xrtValueArrayAppendTake(row,&value)) {
        xrtValueRelease(row); xrtValueRelease(value); return false;
    }
    return xrtValueArrayAppendNew(graph,row);
}

static xvalue* make_graph(unsigned sample)
{
    xvalue* graph=xrtValueArray();
    if (!graph) return NULL;
    for (unsigned i=0; i<6; ++i) {
        char name[]={'n',(char)('0'+i),0};
        xvalue* row=xrtValueArray();
        if (!row || !xrtValueArrayAppendNew(row,text(name,2)) || !xrtValueArrayAppendTake(graph,&row)) {
            xrtValueRelease(row); xrtValueRelease(graph); return NULL;
        }
    }
    if (sample==0) return graph;
    if (sample==1 || sample==2) {
        /* 第二组先松弛出溢出候选，随后才找到接近上界的有限替代路径。 */
        if (!add_edge(graph,0,1,sample==1 ? 11 : 9) || !add_edge(graph,1,2,11)) goto fail;
        if (sample==2 && (!add_edge(graph,0,3,10) || !add_edge(graph,3,2,0))) goto fail;
    } else if (sample==3) {
        if (!add_edge(graph,0,1,11) || !add_edge(graph,0,1,2) ||
            !add_edge(graph,1,2,3) || !add_edge(graph,2,3,4)) goto fail;
    } else if (sample==4) {
        for (unsigned i=0; i<6; ++i) if (!add_edge(graph,i,(i+1)%6,i%2)) goto fail;
    } else if (sample==5) {
        if (!add_edge(graph,0,1,12) || !add_edge(graph,1,2,14) || !add_edge(graph,0,3,13) ||
            !add_edge(graph,3,2,15)) goto fail;
    } else {
        uint32_t state=UINT32_C(0x6902a531) ^ (sample*UINT32_C(2654435761));
        for (unsigned from=0; from<6; ++from) for (unsigned to=0; to<6; ++to) {
            state=state*UINT32_C(1664525)+UINT32_C(1013904223);
            if ((state>>24)%3!=0) continue;
            if (!add_edge(graph,from,to,(state>>16)%16)) goto fail;
            if ((state&15)==0 && !add_edge(graph,from,to,(state>>8)%16)) goto fail;
        }
    }
    return graph;
fail:
    xrtValueRelease(graph);
    return NULL;
}

int main(int argc,char** argv)
{
    bool records=argc==2 && strcmp(argv[1],"--records")==0;
    if (argc!=1 && !records) return 2;
    assert(sizeof(double)==sizeof(uint64_t) && DBL_MANT_DIG==53 && FLT_RADIX==2);
    assert(xrtMemDebugEnabled());
    xrtSetErrorInfo(XERR_STATE,"test.graph",0,"warm XRT slot"); xrtClearError();
    unsigned checked=0;
    for (unsigned sample=0; sample<512; ++sample) {
        xmemdebugsnapshot initial;
        xrtMemDebugSnapshot(&initial);
        xvalue* graph=make_graph(sample);
        assert(graph);
        for (unsigned start=0; start<6; ++start) {
            char name[]={'n',(char)('0'+start),0};
            xmemdebugsnapshot before;
            xrtClearError(); xrtMemDebugSnapshot(&before);
            xvalue* result=xg_dijkstra(graph,name,2);
            xerrkind error=xrtErrorKind(xrtGetError());
            assert((result && error==0) || (!result && error==XERR_RANGE));
            if (records) printf("%u\t%u\t%s",sample,start,result ? "ok" : "range");
            unsigned found=0;
            for (unsigned target=0; target<6; ++target) {
                name[1]=(char)('0'+target);
                xvalue* value=result ? xrtValueObjectGet(result,(xstrview){name,2}) : NULL;
                if (value) {
                    double number; uint64_t bits;
                    assert(xrtValueGetFloat(value,&number) && isfinite(number) && number>=0);
                    memcpy(&bits,&number,sizeof(bits)); ++found;
                    if (records) printf("\t%016llx",(unsigned long long)bits);
                } else if (records) fputs("\t-",stdout);
            }
            if (result) assert(xrtValueCount(result)==found);
            if (records) putchar('\n');
            xrtValueRelease(result); balanced(&before); ++checked;
        }
        xrtValueRelease(graph); balanced(&initial);
    }
    if (!records) printf("graph weighted records: %u cases, PASS\n",checked);
    return ferror(stdout) ? 2 : 0;
}

#include "../graph_native.c"
#include "../graph_traversal.c"
#include "../graph_components.c"
#include "../graph_distance.c"
