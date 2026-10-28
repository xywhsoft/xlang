#include "analysis/frontend_completion_pool.h"
#include "analysis/frontend_context.h"
static int xlsCompletionAppendPool(xls_completion_pool_scope*,const char*,const char*,XL6_SymbolId,const XL6_AstNode*,char*);
void xlsCompletionPoolFree(xls_completion_pool* pool)
{
    if(!pool)return;
    xrtFree(pool->query.items);xrtStrBufFree(&pool->strings);*pool=(xls_completion_pool){0};
}
static void xlsCompletionPoolClear(xls_completion_pool_scope* s)
{
    xrtFree(s->build.items);s->build=(xls_completion_pool_build){0};xrtStrBufFree(&s->strings);
}
/* Admission charges record-array capacity,actual XRT buffer capacity+its NUL,
 * and the final native item array while both arrays coexist. Allocator/header
 * and realloc transient overhead are not a process/global peak guarantee. */
static int xlsCompletionPoolFits(const xls_completion_pool_scope* s,size_t capacity,size_t strings,size_t count)
{
    if(capacity>SIZE_MAX/sizeof(*s->build.items) || count>SIZE_MAX/sizeof(XL6_CompletionItem))return 0;
    size_t records=capacity*sizeof(*s->build.items),items=count*sizeof(XL6_CompletionItem);
    return records<=s->limit && strings<=s->limit-records && items<=s->limit-records-strings;
}
static int xlsCompletionPoolRecordFits(const xls_completion_pool_scope* s,size_t capacity)
{
    size_t strings=s->strings.Data ? s->strings.Capacity+1 : 0;
    return xlsCompletionPoolFits(s,capacity,strings,s->build.count+1);
}
static size_t xlsCompletionPoolAppend(xls_completion_pool_scope* s,const char* text,size_t size)
{
    if(size==SIZE_MAX || size+1>SIZE_MAX-s->strings.Size)return 0;
    size_t start=s->strings.Size,required=start+size+1;
    if(required==SIZE_MAX || !xlsCompletionPoolFits(s,s->build.capacity,required+1,s->build.count+1) ||
       !xrtStrBufAppend(&s->strings,(xstrview){text,size+1}) ||
       !xlsCompletionPoolRecordFits(s,s->build.capacity))return 0;
    return start+1;
}
void xlsCompletionPoolBegin(xls_completion_pool_scope* s,XL6_CompletionQuery* query,size_t limit)
{
    xls_completion_pool_scope** slot=xlsFrontendCompletionPoolSlot();
    *s=(xls_completion_pool_scope){.previous=slot ? *slot : NULL,.slot=slot,.target=query,.limit=limit};
    if(slot)*slot=s;
}
int xlsCompletionPoolEnd(xls_completion_pool_scope* s,xls_completion_pool* out)
{
    if(!s || !s->slot || *s->slot!=s)return 0;
    *s->slot=s->previous;s->slot=NULL;
    int okay=out && s->limit && !s->failed && !s->build.error && s->target &&
        !s->target->error && s->target->reached==1 && s->target->available;
    size_t strings=s->strings.Data ? s->strings.Capacity+1 : 0;
    if(okay && !xlsCompletionPoolFits(s,s->build.capacity,strings,s->build.count))okay=0;
    XL6_CompletionItem* items=okay && s->build.count ? xrtMalloc(s->build.count*sizeof(*items)) : NULL;
    if(okay && s->build.count && !items)okay=0;
    if(okay){
        for(size_t i=0;i<s->build.count;++i){
            const xls_completion_pool_record* from=&s->build.items[i];
            items[i]=(XL6_CompletionItem){.name=s->strings.Data+from->name-1,.kind=from->kind,
                .canonical=from->canonical ? s->strings.Data+from->canonical-1 : NULL,
                .symbolId=from->symbolId,.selection=from->selection};
        }
        *out=(xls_completion_pool){.query={.items=items,.count=s->build.count,.capacity=s->build.count,.bytes=s->build.bytes},
            .strings=s->strings,.admittedBytes=s->build.capacity*sizeof(*s->build.items)+strings+s->build.count*sizeof(*items)};
        s->strings=(xstrbuf){0};
    }
    xlsCompletionPoolClear(s);return okay;
}
void xlsCompletionPoolAbort(xls_completion_pool_scope** slot)
{
    while(slot && *slot){
        xls_completion_pool_scope* s=*slot;*slot=s->previous;s->slot=NULL;s->failed=1;
        xlsCompletionPoolClear(s);
    }
}
/* The original namespace/private filters precede this observer. Prefix-rejected
 * canonical buffers are freed immediately;matching native items keep theirs.
 * Pool records hold only offsets while XRT grows its shared string buffer. */
static void xlsCompletionPoolObserve(XL6_CompletionQuery* q,const char* name,const char* kind,
    XL6_SymbolId symbolId,const XL6_AstNode* selection,char* canonical,int consume)
{
    xls_completion_pool_scope** slot=xlsFrontendCompletionPoolSlot();
    xls_completion_pool_scope* s=slot ? *slot : NULL;
    if(s && s->target==q && s->limit && !s->failed &&
       !xlsCompletionAppendPool(s,name,kind,symbolId,selection,canonical))s->failed=1;
    if(consume)xrtFree(canonical);
}
