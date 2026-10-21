#include "analysis/check_session.h"
#include "analysis/frontend_internal.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

typedef struct binding { char *key,*path; xls_text* text; uint64_t identity,revision; } binding;
struct xls_check_session {
    xmap documents;
    char *target,*directory;
    int captureIndex;
    xls_session_limits limits;
    xls_semantic_check* result;
    xls_syntax* entrySyntax;
    xls_check_stats stats;
};
static xbytesview key(const char* s) { return (xbytesview){(const unsigned char*)s,strlen(s)}; }
static int label(const char* s)
{ size_t n=s ? strlen(s) : 0; return n && n<=262144 && xrtUtf8Valid((xstrview){s,n},NULL); }
static void drop(xbytesview k,void* value,void* user)
{ (void)k; (void)user; binding* b=value; xrtFree(b->key); xrtFree(b->path); xlsTextRelease(b->text); }
static int sameBody(const binding* old,const xls_analysis_document* in)
{ return old->text==in->text || (xlsTextSize(old->text)==xlsTextSize(in->text) && !memcmp(xlsTextData(old->text),xlsTextData(in->text),xlsTextSize(in->text))); }
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
static void fail(char* error,size_t size,const char* message) { if(error && size) snprintf(error,size,"%s",message); }
xls_check_session* xlsCheckSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen=limits ? *limits : (xls_session_limits){64,8u*1024u*1024u,32u*1024u*1024u};
    if(!chosen.documents || chosen.documents>64 || !chosen.documentBytes || chosen.documentBytes>8u*1024u*1024u ||
       chosen.documentBytes>chosen.totalBytes || chosen.totalBytes>32u*1024u*1024u) return NULL;
    xls_check_session* s=xrtCalloc(1,sizeof(*s)); if(!s) return NULL;
    s->limits=chosen;
    if(!xrtMapInit(&s->documents,sizeof(binding)) || !xrtMapSetDrop(&s->documents,drop,NULL)) { xlsCheckSessionDestroy(s); return NULL; }
    return s;
}
void xlsCheckSessionDestroy(xls_check_session* s)
{ if(s) { xrtMapUnit(&s->documents); xlsSyntaxRelease(s->entrySyntax); xlsSemanticCheckDestroy(s->result); xrtFree(s->target); xrtFree(s->directory); xrtFree(s); } }
void xlsCheckSessionStats(const xls_check_session* s,xls_check_stats* stats)
{ if(stats) *stats=s ? s->stats : (xls_check_stats){0}; }
xls_semantic_check* xlsCheckSessionCheck(xls_check_session* s,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xmap staged={0}; char *directory=NULL,*nextTarget=NULL; xls_semantic_check* result=NULL;
    xls_syntax* syntax=NULL;
    if(error && capacity) *error=0;
    if(!s || !documents || !count || count>s->limits.documents || !label(target) || (captureIndex!=0 && captureIndex!=1)) goto invalid;
    if(stopped(cancelled,user)) goto cancel;
    directory=xrtPathAbs("."); nextTarget=xrtStrDup(target);
    if(!directory || !nextTarget || !xrtMapInit(&staged,sizeof(binding)) || !xrtMapSetDrop(&staged,drop,NULL)) goto memory;
    size_t bytes=0; const char* entry=NULL; const xls_analysis_document* entryDocument=NULL;
    int equal=s->result && s->stats.documents==count && s->captureIndex==captureIndex &&
        !strcmp(s->target,target) && !strcmp(s->directory,directory);
    xls_semantic_input inputs[64];
    for(size_t i=0;i<count;++i) {
        const xls_analysis_document* in=&documents[i];
        if(stopped(cancelled,user)) goto cancel;
        if(!label(in->key) || !label(in->path) || !xrtPathIsAbs(in->path) || !in->text ||
           xlsTextSize(in->text)>s->limits.documentBytes || xlsTextSize(in->text)>s->limits.totalBytes-bytes || xrtMapHas(&staged,key(in->key))) goto invalid;
        binding* old=xrtMapGet(&s->documents,key(in->key));
        int body=old && sameBody(old,in),identity=old && old->identity==in->identity;
        if(identity && (in->revision<old->revision || (in->revision==old->revision && !body))) goto invalid;
        equal=equal && identity && body && !strcmp(old->path,in->path);
        binding b={xrtStrDup(in->key),xrtStrDup(in->path),xlsTextRetain(in->text),in->identity,in->revision};
        if(!b.key || !b.path || !xrtMapSet(&staged,key(in->key),&b)) { drop((xbytesview){0},&b,NULL); goto memory; }
        bytes+=xlsTextSize(in->text);
        inputs[i]=(xls_semantic_input){in->path,xlsTextData(in->text),xlsTextSize(in->text)};
        if(!strcmp(in->key,target)) { entry=in->path; entryDocument=in; }
    }
    if(!entry) goto invalid;
    if(equal && !xlsSemanticCheckValidate(s->result)) { equal=0; ++s->stats.invalidations; }
    if(stopped(cancelled,user)) goto cancel;
    if(equal) result=xlsSemanticCheckRetain(s->result);
    else {
        ++s->stats.builds;
        const binding* old=s->target && !strcmp(s->target,target) ? xrtMapConstGet(&s->documents,key(target)) : NULL;
        const xls_syntax* previous=old && old->identity==entryDocument->identity && !strcmp(old->path,entry) ? s->entrySyntax : NULL;
        if(previous && sameBody(old,entryDocument)) { syntax=xlsSyntaxRetain(s->entrySyntax); ++s->stats.entrySyntaxHits; }
        else {
            char* path=xrtPathAbs(entry); if(!path) goto memory;
            xls_parse_work work={0}; syntax=xlsSyntaxBuild(path,entryDocument->text,previous,&work); xrtFree(path);
            s->stats.entryFullParses+=work.full; s->stats.entryFragmentParses+=work.fragments;
            s->stats.entryReusedDeclarations+=work.reused; s->stats.entryParsedBytes+=work.bytes;
            if(!syntax) goto memory;
        }
        if(stopped(cancelled,user)) goto cancel;
        result=xlsSemanticCheckCreateParsed(entry,inputs,count,captureIndex,syntax,cancelled,user,error,capacity);
        if(!result) goto done;
    }
    if(stopped(cancelled,user)) { xlsSemanticCheckDestroy(result); result=NULL; goto cancel; }
    if(equal) ++s->stats.hits;
    xrtMapUnit(&s->documents); s->documents=staged; memset(&staged,0,sizeof(staged));
    xrtFree(s->target); s->target=nextTarget; nextTarget=NULL;
    xrtFree(s->directory); s->directory=directory; directory=NULL;
    xlsSemanticCheckDestroy(s->result); s->result=xlsSemanticCheckRetain(result);
    s->captureIndex=captureIndex; s->stats.documents=count; s->stats.textBytes=bytes;
    if(syntax) { xlsSyntaxRelease(s->entrySyntax); s->entrySyntax=syntax; syntax=NULL; }
    goto done;
invalid: fail(error,capacity,"invalid semantic batch identity, revision, path or capacity"); goto done;
memory: fail(error,capacity,"cannot retain semantic input batch"); goto done;
cancel: fail(error,capacity,"semantic session cancelled");
done: xlsSyntaxRelease(syntax); xrtMapUnit(&staged); xrtFree(directory); xrtFree(nextTarget); return result;
}
