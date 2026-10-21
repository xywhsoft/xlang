#include "analysis/frontend_internal.h"
#include <string.h>

typedef struct analysis_document {
    char* path;
    xls_text* text;
    xls_syntax *syntax,*baseline;
    uint64_t identity,revision,generation;
} analysis_document;
struct xls_session {
    xmap documents;
    xls_session_limits limits;
    xls_session_stats stats;
};
static xbytesview keyView(const char* key)
{ return (xbytesview){(const unsigned char*)key,strlen(key)}; }
static int validLabel(const char* value)
{
    size_t length=value ? strlen(value) : 0;
    return length && length<=262144 && xrtUtf8Valid((xstrview){value,length},NULL);
}
static void dropDocument(xbytesview key,void* value,void* user)
{
    (void)key; (void)user; analysis_document* document=value;
    xrtFree(document->path); xlsTextRelease(document->text); xlsSyntaxRelease(document->syntax); xlsSyntaxRelease(document->baseline);
}
static xls_syntax* parse(const analysis_document* document,xls_session_stats* stats)
{
    xls_parse_work work={0};
    xls_syntax* result=xlsSyntaxBuild(document->path,document->text,document->baseline,&work);
    stats->parses+=work.full+work.fragments; stats->fullParses+=work.full; stats->fragmentParses+=work.fragments;
    stats->reusedDeclarations+=work.reused; stats->parsedBytes+=work.bytes; return result;
}
xls_session* xlsSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen=limits ? *limits : (xls_session_limits){4096,16u*1024u*1024u,256u*1024u*1024u};
    if(!chosen.documents || !chosen.documentBytes || chosen.documentBytes>chosen.totalBytes) return NULL;
    xls_session* session=xrtMalloc(sizeof(*session));
    if(!session) return NULL;
    memset(session,0,sizeof(*session)); session->limits=chosen;
    if(!xrtMapInit(&session->documents,sizeof(analysis_document)) ||
        !xrtMapSetDrop(&session->documents,dropDocument,NULL)) { xlsSessionDestroy(session); return NULL; }
    return session;
}
void xlsSessionDestroy(xls_session* session)
{ if(session) { xrtMapUnit(&session->documents); xrtFree(session); } }
int xlsSessionUpdate(xls_session* session,const char* key,const char* path,
                     uint64_t identity,uint64_t revision,xls_text* source)
{
    if(!session || !source || !validLabel(key) || !validLabel(path)) return 0;
    size_t bytes=xlsTextSize(source);
    analysis_document* previous=xrtMapGet(&session->documents,keyView(key));
    size_t retained=session->stats.textBytes-(previous ? xlsTextSize(previous->text) : 0);
    if(bytes>session->limits.documentBytes || bytes>session->limits.totalBytes-retained ||
        (!previous && session->stats.documents>=session->limits.documents)) return 0;
    int sameIdentity=previous && previous->identity==identity;
    int sameText=sameIdentity && (previous->text==source || (xlsTextSize(previous->text)==bytes &&
        !memcmp(xlsTextData(previous->text),xlsTextData(source),bytes)));
    int equal=sameText && !strcmp(previous->path,path);
    if(sameIdentity && (revision<previous->revision || (revision==previous->revision && !sameText))) return 0;
    if(equal && previous->revision==revision) return 1;
    if(session->stats.generation==UINT64_MAX) return 0;
    analysis_document next={0};
    next.path=xrtStrDup(path); next.text=xlsTextRetain(equal ? previous->text : source);
    next.syntax=equal ? xlsSyntaxRetain(previous->syntax) : NULL;
    if(sameIdentity && !strcmp(previous->path,path)) next.baseline=xlsSyntaxRetain(equal ? previous->baseline : previous->syntax ? previous->syntax : previous->baseline);
    next.identity=identity; next.revision=revision; next.generation=session->stats.generation+1;
    if(!next.path || !xrtMapSet(&session->documents,keyView(key),&next)) { dropDocument((xbytesview){0},&next,NULL); return 0; }
    session->stats.generation=next.generation; session->stats.documents=xrtMapCount(&session->documents);
    session->stats.textBytes=retained+bytes; return 1;
}
int xlsSessionClose(xls_session* session,const char* key)
{
    if(!session || !validLabel(key) || session->stats.generation==UINT64_MAX) return 0;
    analysis_document* document=xrtMapGet(&session->documents,keyView(key));
    if(!document) return 0;
    size_t bytes=xlsTextSize(document->text);
    if(!xrtMapRemove(&session->documents,keyView(key))) return 0;
    ++session->stats.generation; --session->stats.documents; session->stats.textBytes-=bytes; return 1;
}
typedef struct prune_context { xls_session_live live; void* user; xarray keys; int okay; } prune_context;
static bool pruneKey(xbytesview key,void* value,void* user)
{
    (void)value; prune_context* context=user;
    if(!context->live((const char*)key.Data,key.Size,context->user)) context->okay=xrtArrayPush(&context->keys,&key);
    return context->okay;
}
int xlsSessionPrune(xls_session* session,xls_session_live live,void* user)
{
    if(!session || !live) return 0;
    prune_context context={.live=live,.user=user,.okay=1};
    if(!xrtArrayInit(&context.keys,sizeof(xbytesview))) return 0;
    xrtMapVisit(&session->documents,pruneKey,&context);
    if(context.keys.Count>UINT64_MAX-session->stats.generation) context.okay=0;
    if(context.okay) for(size_t i=0;i<context.keys.Count;++i) {
        xbytesview key=*(xbytesview*)xrtArrayGet(&context.keys,i);
        analysis_document* document=xrtMapGet(&session->documents,key); size_t bytes=xlsTextSize(document->text);
        xrtMapRemove(&session->documents,key); ++session->stats.generation; --session->stats.documents; session->stats.textBytes-=bytes;
    }
    xrtArrayUnit(&context.keys); return context.okay;
}
int xlsSessionSyntax(xls_session* session,const char* key,xls_analysis_snapshot* result)
{
    if(!result) return 0;
    memset(result,0,sizeof(*result));
    if(!session || !validLabel(key)) return 0;
    analysis_document* document=xrtMapGet(&session->documents,keyView(key));
    if(!document) return 0;
    if(!document->syntax) {
        document->syntax=parse(document,&session->stats);
        if(!document->syntax) return 0;
        xlsSyntaxRelease(document->baseline); document->baseline=NULL;
    } else ++session->stats.cacheHits;
    *result=(xls_analysis_snapshot){xlsSyntaxRetain(document->syntax),document->identity,document->revision,document->generation}; return 1;
}
void xlsSessionStats(const xls_session* session,xls_session_stats* stats)
{ if(stats) *stats=session ? session->stats : (xls_session_stats){0}; }
