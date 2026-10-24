#include "analysis/dependency_graph.h"
#include <xrt.h>
#include <string.h>

typedef struct dependency_node {
    xavlnode tree;
    uint64_t owners,staged;
    size_t size;
    struct dependency_node* retired;
    unsigned char data[];
} dependency_node;
typedef struct dependency_owner { dependency_node** nodes;size_t count,capacity; } dependency_owner;
struct xls_dependency_graph {
    xavl tree;
    dependency_owner owners[64];
    xls_dependency_limits limits;
    xls_dependency_compare compare;
    void* user;
    xls_dependency_stats stats;
    int busy;
};
static int failure(enum xls_dependency_error* error,enum xls_dependency_error value)
{if(error)*error=value;return 0;}
static void increment(uint64_t* value){if(*value!=UINT64_MAX)++*value;}
static xls_dependency_key nodeKey(const dependency_node* node)
{return (xls_dependency_key){node->data,node->size};}
static int compareNode(const void* key,const xavlnode* raw,void* user)
{
    xls_dependency_graph* g=user;increment(&g->stats.comparisons);
    return g->compare(*(const xls_dependency_key*)key,nodeKey((const dependency_node*)raw),g->user);
}
static int validKey(const xls_dependency_graph* g,xls_dependency_key key)
{return key.data && key.size && key.size<=g->limits.keyBytes;}
static int charge(xls_dependency_graph* g,size_t* bytes,size_t add)
{
    if(add>g->limits.bytes-*bytes)return 0;
    *bytes+=add;if(*bytes>g->stats.peakBytes)g->stats.peakBytes=*bytes;return 1;
}
static size_t nodeBytes(const dependency_node* n){return sizeof(*n)+n->size;}
static void removeNode(xls_dependency_graph* g,dependency_node* n)
{xls_dependency_key key=nodeKey(n);xrtAVLRemove(&g->tree,&key,compareNode,g);}
xls_dependency_graph* xlsDependencyGraphCreate(const xls_dependency_limits* limits,
    xls_dependency_compare compare,void* user,enum xls_dependency_error* error)
{
    xls_dependency_limits chosen=limits ? *limits : (xls_dependency_limits){64,163840,163840,262144,64u*1024u*1024u};
    if(!compare || !chosen.roots || chosen.roots>64 || !chosen.paths || !chosen.edges ||
       !chosen.keyBytes || chosen.keyBytes>SIZE_MAX-sizeof(dependency_node) || chosen.bytes<sizeof(xls_dependency_graph)) {
        failure(error,XLS_DEPENDENCY_ARGUMENT);return NULL;
    }
    xls_dependency_graph* g=xrtCalloc(1,sizeof(*g));if(!g){failure(error,XLS_DEPENDENCY_MEMORY);return NULL;}
    if(!xrtAVLInit(&g->tree)){xrtFree(g);failure(error,XLS_DEPENDENCY_MEMORY);return NULL;}
    g->limits=chosen;g->compare=compare;g->user=user;g->stats.bytes=g->stats.peakBytes=sizeof(*g);
    if(error)*error=XLS_DEPENDENCY_OK;
    return g;
}
void xlsDependencyGraphDestroy(xls_dependency_graph* g)
{
    if(!g || g->busy)return;
    while(g->tree.Root){dependency_node* n=(dependency_node*)g->tree.Root;removeNode(g,n);xrtFree(n);}
    for(size_t i=0;i<g->limits.roots;++i)xrtFree(g->owners[i].nodes);
    xrtFree(g);
}
void xlsDependencyGraphStats(const xls_dependency_graph* g,xls_dependency_stats* stats)
{if(stats)*stats=g ? g->stats : (xls_dependency_stats){0};}
int xlsDependencyGraphFind(xls_dependency_graph* g,xls_dependency_key key,uint64_t* owners)
{
    if(owners)*owners=0;
    if(!g || g->busy || !owners || !validKey(g,key))return 0;
    increment(&g->stats.lookups);
    const dependency_node* n=(const dependency_node*)xrtAVLFind(&g->tree,&key,compareNode,g);
    if(n)*owners=n->owners;
    return 1;
}
int xlsDependencyGraphUpdate(xls_dependency_graph* g,const xls_dependency_update* updates,size_t count,
    int (*cancelled)(void*),void* user,enum xls_dependency_error* error)
{
    if(error)*error=XLS_DEPENDENCY_ARGUMENT;
    if(!g || g->busy || count>g->limits.roots || (count && !updates))return 0;
    if(cancelled && cancelled(user))return failure(error,XLS_DEPENDENCY_CANCELLED);
    if(!count){if(error)*error=XLS_DEPENDENCY_OK;return 1;}
    if(g->stats.generation==UINT64_MAX)return failure(error,XLS_DEPENDENCY_LIMIT);
    dependency_owner staged[64]={0};dependency_node** touched=NULL;size_t touchedCount=0,total=0,bytes=g->stats.bytes;
    uint64_t roots=0;enum xls_dependency_error reason=XLS_DEPENDENCY_ARGUMENT;g->busy=1;
    for(size_t i=0;i<count;++i){
        const xls_dependency_update* in=&updates[i];
        if(in->root>=g->limits.roots || (roots&(UINT64_C(1)<<in->root)) || (in->count && !in->keys))goto rollback;
        roots|=UINT64_C(1)<<in->root;
        if(in->count>g->limits.edges-total || in->count>SIZE_MAX/sizeof(dependency_node*)){reason=XLS_DEPENDENCY_LIMIT;goto rollback;}
        total+=in->count;
    }
    if(total>SIZE_MAX/sizeof(*touched) || !charge(g,&bytes,total*sizeof(*touched))){reason=XLS_DEPENDENCY_LIMIT;goto rollback;}
    if(total && !(touched=xrtMalloc(total*sizeof(*touched)))){reason=XLS_DEPENDENCY_MEMORY;goto rollback;}
    for(size_t i=0;i<count;++i){
        const xls_dependency_update* in=&updates[i];dependency_owner* target=&staged[in->root];
        if(!charge(g,&bytes,in->count*sizeof(*target->nodes))){reason=XLS_DEPENDENCY_LIMIT;goto rollback;}
        if(in->count && !(target->nodes=xrtMalloc(in->count*sizeof(*target->nodes)))){reason=XLS_DEPENDENCY_MEMORY;goto rollback;}
        target->capacity=in->count;uint64_t bit=UINT64_C(1)<<in->root;
        for(size_t j=0;j<in->count;++j){
            if(cancelled && cancelled(user)){reason=XLS_DEPENDENCY_CANCELLED;goto rollback;}
            xls_dependency_key key=in->keys[j];if(!validKey(g,key))goto rollback;
            dependency_node* n=(dependency_node*)xrtAVLFind(&g->tree,&key,compareNode,g);
            if(!n){
                if(g->tree.Count>=g->limits.paths || !charge(g,&bytes,sizeof(*n)+key.size)){reason=XLS_DEPENDENCY_LIMIT;goto rollback;}
                n=xrtCalloc(1,sizeof(*n)+key.size);if(!n){reason=XLS_DEPENDENCY_MEMORY;goto rollback;}
                n->size=key.size;memcpy(n->data,key.data,key.size);xrtAVLNodeInit(&n->tree);
                bool added=false;xavlnode* actual=xrtAVLInsert(&g->tree,&n->tree,&key,compareNode,g,&added);
                if(actual!=&n->tree || !added){xrtFree(n);reason=XLS_DEPENDENCY_ARGUMENT;goto rollback;}
            }
            if(!(n->staged&bit)){
                if(!n->staged)touched[touchedCount++]=n;
                n->staged|=bit;target->nodes[target->count++]=n;
            }
        }
    }
    size_t edges=g->stats.edges;
    for(size_t i=0;i<count;++i){size_t root=updates[i].root;edges-=g->owners[root].count;}
    for(size_t i=0;i<count;++i){size_t root=updates[i].root;if(staged[root].count>g->limits.edges-edges){reason=XLS_DEPENDENCY_LIMIT;goto rollback;}edges+=staged[root].count;}
    if(cancelled && cancelled(user)){reason=XLS_DEPENDENCY_CANCELLED;goto rollback;}
    /* All allocations and cancellation gates are above this point. Clear old
     * edges first,then add all staged owners before retiring shared nodes. */
    for(size_t i=0;i<count;++i){size_t root=updates[i].root;uint64_t bit=UINT64_C(1)<<root;
        for(size_t j=0;j<g->owners[root].count;++j)g->owners[root].nodes[j]->owners&=~bit;
    }
    for(size_t i=0;i<touchedCount;++i){dependency_node* n=touched[i];n->owners|=n->staged;n->staged=0;}
    dependency_node* retired=NULL;
    for(size_t i=0;i<count;++i){size_t root=updates[i].root;dependency_owner* old=&g->owners[root];
        for(size_t j=0;j<old->count;++j){dependency_node* n=old->nodes[j];
            if(!n->owners && !n->retired){removeNode(g,n);n->retired=retired ? retired : n;retired=n;bytes-=nodeBytes(n);}
        }
        bytes-=old->capacity*sizeof(*old->nodes);xrtFree(old->nodes);*old=staged[root];staged[root]=(dependency_owner){0};
    }
    while(retired){dependency_node* n=retired;retired=n->retired==n ? NULL : n->retired;xrtFree(n);}
    bytes-=total*sizeof(*touched);xrtFree(touched);
    g->stats.bytes=bytes;g->stats.paths=g->tree.Count;g->stats.edges=edges;g->stats.roots=0;
    for(size_t i=0;i<g->limits.roots;++i)g->stats.roots+=g->owners[i].count!=0;
    ++g->stats.generation;increment(&g->stats.updates);g->busy=0;if(error)*error=XLS_DEPENDENCY_OK;return 1;
rollback:
    for(size_t i=0;i<touchedCount;++i){dependency_node* n=touched[i];n->staged=0;if(!n->owners){removeNode(g,n);xrtFree(n);}}
    for(size_t i=0;i<64;++i)xrtFree(staged[i].nodes);
    xrtFree(touched);g->busy=0;return failure(error,reason);
}
