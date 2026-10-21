#include "analysis/frontend_internal.h"
#include <string.h>

struct xls_syntax {
    xatomic32 references;
    xls_text* text;
    char* path;
    XL6_ParseResult parse;
    int accepted;
};
typedef struct analysis_document {
    char* path;
    xls_text* text;
    xls_syntax* syntax;
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
    xrtFree(document->path); xlsTextRelease(document->text); xlsSyntaxRelease(document->syntax);
}
xls_syntax* xlsSyntaxRetain(xls_syntax* syntax)
{ if(syntax) xrtAtomic32FetchAdd(&syntax->references,1,XMEMORY_RELAXED); return syntax; }
void xlsSyntaxRelease(xls_syntax* syntax)
{
    if(syntax && xrtAtomic32FetchSub(&syntax->references,1,XMEMORY_ACQ_REL)==1) {
        xl6ParseResultFree(&syntax->parse); xlsTextRelease(syntax->text); xrtFree(syntax->path); xrtFree(syntax);
    }
}
static xls_syntax* parse(const analysis_document* document)
{
    xls_syntax* syntax=xrtMalloc(sizeof(*syntax));
    if(!syntax) return NULL;
    memset(syntax,0,sizeof(*syntax)); xrtAtomic32Init(&syntax->references,1);
    syntax->text=xlsTextRetain(document->text); syntax->path=xrtStrDup(document->path);
    if(!syntax->path) { xlsSyntaxRelease(syntax); return NULL; }
    xl6ParseResultInit(&syntax->parse);
    syntax->accepted=xl6ParseSource(xlsTextData(syntax->text),syntax->path,&syntax->parse);
    return syntax;
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
int xlsSessionSyntax(xls_session* session,const char* key,xls_analysis_snapshot* result)
{
    if(!result) return 0;
    memset(result,0,sizeof(*result));
    if(!session || !validLabel(key)) return 0;
    analysis_document* document=xrtMapGet(&session->documents,keyView(key));
    if(!document) return 0;
    if(!document->syntax) {
        document->syntax=parse(document);
        if(!document->syntax) return 0;
        ++session->stats.parses;
    } else ++session->stats.cacheHits;
    *result=(xls_analysis_snapshot){xlsSyntaxRetain(document->syntax),document->identity,document->revision,document->generation}; return 1;
}
void xlsSessionStats(const xls_session* session,xls_session_stats* stats)
{ if(stats) *stats=session ? session->stats : (xls_session_stats){0}; }
int xlsSyntaxAccepted(const xls_syntax* syntax) { return syntax && syntax->accepted; }
const char* xlsSyntaxPath(const xls_syntax* syntax) { return syntax ? syntax->path : NULL; }
const xls_text* xlsSyntaxText(const xls_syntax* syntax) { return syntax ? syntax->text : NULL; }
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* syntax) { return syntax ? syntax->parse.root : NULL; }
size_t xlsSyntaxDiagnosticCount(const xls_syntax* syntax) { return syntax ? syntax->parse.diags.count : 0; }
int xlsSyntaxDiagnostic(const xls_syntax* syntax,size_t index,xls_syntax_diagnostic* result)
{
    if(!result) return 0;
    memset(result,0,sizeof(*result));
    if(!syntax || index>=syntax->parse.diags.count) return 0;
    const XL6_Diag* diagnostic=&syntax->parse.diags.items[index];
    *result=(xls_syntax_diagnostic){diagnostic->line,diagnostic->column,diagnostic->file ? diagnostic->file : syntax->path,diagnostic->message}; return 1;
}
