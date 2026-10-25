/* Immutable declaration chunks around the shared lexer/parser. This module
 * owns data/range transformations, never a second grammar or lexer. */
#include "analysis/frontend_internal.h"
#include "analysis/ast_clone.h"
#include "analysis/ast_materialize.h"
#include "src/sem/xl6_completion.h"
#include <limits.h>
#include <string.h>

#define UNIT_LIMIT 65536u
#define TREE_DEPTH 512u
typedef struct syntax_unit {
    xatomic32 references;
    XL6_AstNode* node;
    size_t offset,size;
    int line,column;
    size_t storageBytes;
    int storageKnown;
} syntax_unit;
typedef struct placement { syntax_unit* unit; size_t offset; int line,column; } placement;
typedef struct syntax_view { xmutex lock; XL6_AstNode* root; } syntax_view;
struct xls_syntax {
    xatomic32 references;
    xls_text* text;
    char* path;
    XL6_ParseResult parse;
    xarray units;
    syntax_view* view;
    int accepted,incremental,cursor,cursorParsed,cursorPrefix;
    char* cursorError;
    size_t cursorOffset;
    XL6_AstRange cursorRange;
};
#include "analysis/syntax_storage.inc"
static syntax_unit* retainUnit(syntax_unit* unit)
{ if(unit) xrtAtomic32FetchAdd(&unit->references,1,XMEMORY_RELAXED); return unit; }
static void releaseUnit(syntax_unit* unit)
{ if(unit && xrtAtomic32FetchSub(&unit->references,1,XMEMORY_ACQ_REL)==1) { xl6AstFree(unit->node); xrtFree(unit); } }
xls_syntax* xlsSyntaxRetain(xls_syntax* syntax)
{ if(syntax) xrtAtomic32FetchAdd(&syntax->references,1,XMEMORY_RELAXED); return syntax; }
void xlsSyntaxRelease(xls_syntax* syntax)
{
    if(!syntax || xrtAtomic32FetchSub(&syntax->references,1,XMEMORY_ACQ_REL)!=1) return;
    for(size_t i=0;i<syntax->units.Count;++i) releaseUnit(((placement*)xrtArrayGet(&syntax->units,i))->unit);
    xrtArrayUnit(&syntax->units); xl6ParseResultFree(&syntax->parse);
    if(syntax->view) { xl6AstFree(syntax->view->root); xrtMutexUnit(&syntax->view->lock); xrtFree(syntax->view); }
    xlsTextRelease(syntax->text); xrtFree(syntax->cursorError); xrtFree(syntax->path); xrtFree(syntax);
}
static size_t bom(const xls_text* text)
{ return xlsTextSize(text)>=3 && !memcmp(xlsTextData(text),"\xef\xbb\xbf",3) ? 3 : 0; }
/* Compiler offsets/columns exclude BOM and count Unicode scalars. The text
 * index locates the line; only its prefix is decoded for a scalar column. */
static int location(const xls_text* text,size_t offset,int* line,int* column)
{
    size_t prefix=bom(text),raw=offset+prefix,start=0; xls_position pos;
    if(offset>xlsTextSize(text)-prefix || !xlsTextPosition(text,raw,&pos) || pos.line>=INT_MAX ||
       !xlsTextOffset(text,(xls_position){pos.line,0},&start)) return 0;
    if(!pos.line) start=prefix;
    size_t at=start,scalars=0;
    while(at<raw) {
        uint32 scalar; size_t read;
        if(xrtUtf8Decode((xstrview){xlsTextData(text)+at,raw-at},&scalar,&read)!=XUTF_OK) return 0;
        at+=read; ++scalars;
    }
    if(scalars>=INT_MAX) return 0;
    *line=(int)pos.line+1; *column=(int)scalars+1; return 1;
}
static int mapPoint(int sourceLine,int sourceColumn,const placement* p,int* line,int* column)
{
    const syntax_unit* u=p->unit;
    if(sourceLine<u->line || sourceColumn<1 || sourceLine-u->line>INT_MAX-p->line) return 0;
    *line=p->line+sourceLine-u->line;
    int64_t col=sourceLine==u->line ? (int64_t)p->column+sourceColumn-u->column : sourceColumn;
    if(col<1 || col>INT_MAX) return 0;
    *column=(int)col; return 1;
}
static int mapRange(XL6_AstRange r,const placement* p,XL6_AstRange* out)
{
    *out=r; if(!r.known) return 1;
    const syntax_unit* u=p->unit;
    if(r.startOffset<u->offset || r.endOffset<r.startOffset || r.endOffset-u->offset>u->size ||
       !mapPoint(r.startLine,r.startColumn,p,&out->startLine,&out->startColumn) ||
       !mapPoint(r.endLine,r.endColumn,p,&out->endLine,&out->endColumn)) return 0;
    out->startOffset=p->offset+r.startOffset-u->offset;
    out->endOffset=p->offset+r.endOffset-u->offset; return 1;
}
static int placeNode(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{
    const placement* p=user;
    return mapRange(in->range,p,&out->range) && mapRange(in->tokenRange,p,&out->tokenRange) &&
        (!in->line || mapPoint(in->line,in->column,p,&out->line,&out->column));
}
static XL6_AstNode* copyNode(const XL6_AstNode* in,const placement* p,size_t depth)
{
    (void)depth;
    xls_ast_clone_options options={p ? placeNode : NULL,(void*)p,0,1,NULL};
    return xlsAstClone(in,&options,NULL);
}
static xls_syntax* allocate(const char* path,xls_text* text)
{
    xls_syntax* s=xrtCalloc(1,sizeof(*s)); if(!s) return NULL;
    xrtAtomic32Init(&s->references,1); s->path=xrtStrDup(path); s->text=xlsTextRetain(text);
    xl6ParseResultInit(&s->parse);
    if(!s->path || !xrtArrayInit(&s->units,sizeof(placement))) goto failed;
    s->view=xrtCalloc(1,sizeof(*s->view)); if(!s->view) goto failed;
    if(!xrtMutexInit(&s->view->lock)) { xrtFree(s->view); s->view=NULL; goto failed; }
    return s;
failed: xlsSyntaxRelease(s); return NULL;
}
static int decompose(xls_syntax* s)
{
    XL6_AstNode* root=s->parse.root;
    if(!root || root->kind!=XL6_AST_MODULE || !root->childCount || root->childCount>UNIT_LIMIT) return 1;
    size_t previous=0,size=xlsTextSize(s->text)-bom(s->text); s->incremental=1;
    for(size_t i=0;i<root->childCount;++i) {
        XL6_AstNode* node=root->children[i];
        if(!node) return 0;
        syntax_unit* u=xrtCalloc(1,sizeof(*u)); if(!u) return 0;
        xrtAtomic32Init(&u->references,1); u->node=node;
        u->offset=node->range.known ? node->range.startOffset : 0;
        u->size=node->range.known ? node->range.endOffset-node->range.startOffset : size;
        u->line=node->range.known ? node->range.startLine : 1; u->column=node->range.known ? node->range.startColumn : 1;
        placement p={u,u->offset,u->line,u->column};
        if(!xl6AstSetSourceTree(node,s->path) || !xrtArrayPush(&s->units,&p)) { u->node=NULL; releaseUnit(u); return 0; }
        root->children[i]=NULL;
        if(!node->range.known || u->offset<previous || u->size>size || u->offset>size-u->size) s->incremental=0;
        previous=u->offset+u->size;
    }
    xrtFree(root->children); root->children=NULL; root->childCount=root->childCapacity=0;
    return 1;
}
int xlsSyntaxFunctionHeader(const XL6_AstNode* node,size_t* header,size_t* end)
{
    if(node->kind!=XL6_AST_FUNC && node->kind!=XL6_AST_ASYNC_FUNC && node->kind!=XL6_AST_EXPORT_FUNC) return 0;
    const XL6_AstNode* body=NULL; *header=node->range.startOffset;
    for(size_t i=0;i<node->childCount;++i) {
        const XL6_AstNode* child=node->children[i];
        if(child->kind==XL6_AST_BLOCK) { if(body) return 0; body=child; }
        else if(child->range.known && child->range.endOffset>*header) *header=child->range.endOffset;
    }
    if(!body || !body->range.known || *header>body->range.startOffset) return 0;
    *end=body->range.endOffset; return 1;
}
static int shiftRange(XL6_AstRange* r,const xls_text* updated,size_t end,int64_t delta)
{
    if(!r->known) return 1;
    if(r->startOffset>=end) r->startOffset=(size_t)((int64_t)r->startOffset+delta);
    if(r->endOffset>=end) r->endOffset=(size_t)((int64_t)r->endOffset+delta);
    return location(updated,r->startOffset,&r->startLine,&r->startColumn) && location(updated,r->endOffset,&r->endLine,&r->endColumn);
}
static int attachSource(const xls_syntax* s,XL6_AstNode* root)
{
    /* copyNode deliberately drops sources. Each construction gets a fresh
     * source owner, never writes a published chunk's non-atomic metadata. */
    if(!xl6AstSetSourceTree(root,s->path)) return 0;
    if(root && root->source && s->cursor && s->accepted) {
        root->source->completionQuery=1;
        root->source->completionStart=s->cursorRange.startOffset;
        root->source->completionEnd=s->cursorRange.endOffset;
    }
    return 1;
}
static xls_syntax* incremental(const char* path,xls_text* text,const xls_syntax* old,
    XL6_CompletionQuery* query,xls_parse_work* work,int mode,xls_syntax_parse_fn parser,void* user)
{
    if(!old || !old->accepted || !old->incremental || old->cursor!=(query ? mode : 0) ||
       strcmp(path,old->path) || bom(text)!=bom(old->text)) return NULL;
    xls_text_edit edit; if(xlsTextDiff(old->text,text,&edit)!=1) return NULL;
    size_t prefix=(size_t)(edit.text-xlsTextData(text)),newSize=xlsTextSize(text),oldSize=xlsTextSize(old->text),skip=bom(text);
    size_t oldEnd=oldSize-(newSize-prefix-edit.length);
    if(prefix<skip) return NULL;
    prefix-=skip; oldEnd-=skip;
    int64_t delta=(int64_t)newSize-(int64_t)oldSize;
    for(size_t i=0;i<old->units.Count;++i) {
        const placement* p=xrtArrayConstGet(&old->units,i); const syntax_unit* unit=p->unit;
        size_t header=0,end=0;
        if(!xlsSyntaxFunctionHeader(unit->node,&header,&end)) continue;
        header=p->offset+header-unit->offset; end=p->offset+end-unit->offset;
        if(prefix<header || oldEnd>end) continue;
        size_t length=(size_t)((int64_t)unit->size+delta);
        if(p->offset>newSize-skip || length>newSize-skip-p->offset) return NULL;
        /* Only the edited cursor function may contain a marker in retained
         * chunks. Moving to another function or editing elsewhere reparses
         * the whole source, removing the old marker through the real parser. */
        if(query && (!old->cursorRange.known || old->cursorRange.startOffset<header ||
           old->cursorRange.endOffset>end || query->offset<header ||
           query->offset>(size_t)((int64_t)end+delta))) continue;
        char* fragment=xrtStrDupN(xlsTextData(text)+skip+p->offset,length); if(!fragment) return NULL;
        XL6_ParseResult parsed; xl6ParseResultInit(&parsed);
        XL6_CompletionQuery local={0}; local.path=path;
        if(query) local.offset=query->offset-p->offset;
        ++work->fragments; work->bytes+=length;
        int okay=query ? (parser ? parser(fragment,path,&parsed,&local,user) : xl6ParseSourceWithCompletion(fragment,path,&parsed,&local)) : xl6ParseSource(fragment,path,&parsed);
        xrtFree(fragment);
        XL6_AstNode* fresh=okay && parsed.root && parsed.root->childCount==1 ? parsed.root->children[0] : NULL;
        size_t freshHeader=0,freshEnd=0;
        if(!fresh || fresh->kind!=unit->node->kind || !fresh->range.known || fresh->range.startOffset || fresh->range.endOffset!=length ||
           !xlsSyntaxFunctionHeader(fresh,&freshHeader,&freshEnd) || freshHeader!=header-p->offset) {
            xl6CompletionFree(&local); xl6ParseResultFree(&parsed); return NULL;
        }
        xls_syntax* s=allocate(path,text);
        if(!s) { xl6CompletionFree(&local); xl6ParseResultFree(&parsed); return NULL; }
        s->parse.root=copyNode(old->parse.root,NULL,0);
        if(!s->parse.root || !shiftRange(&s->parse.root->range,text,oldEnd,delta) || !shiftRange(&s->parse.root->tokenRange,text,oldEnd,delta)) goto failed;
        for(size_t j=0;j<old->units.Count;++j) {
            const placement* prior=xrtArrayConstGet(&old->units,j); placement next=*prior;
            if(j==i) {
                syntax_unit* changed=xrtCalloc(1,sizeof(*changed)); if(!changed) goto failed;
                xrtAtomic32Init(&changed->references,1); changed->node=fresh; changed->size=length; changed->line=changed->column=1;
                parsed.root->children[0]=NULL;
                xrtFree(fresh->doc); fresh->doc=unit->node->doc ? xrtStrDup(unit->node->doc) : NULL;
                if(unit->node->doc && !fresh->doc) { releaseUnit(changed); goto failed; }
                if(unit->storageKnown) {
                    if(!cacheTreeBytes(changed->node,SIZE_MAX,&changed->storageBytes)) { releaseUnit(changed); goto failed; }
                    changed->storageKnown=1;
                }
                next.unit=changed;
            } else next.unit=retainUnit(prior->unit);
            if(j>i) next.offset=(size_t)((int64_t)next.offset+delta);
            if(!location(text,next.offset,&next.line,&next.column) || !xrtArrayPush(&s->units,&next)) { releaseUnit(next.unit); goto failed; }
        }
        s->accepted=s->incremental=1;
        if(query) {
            syntax_unit localUnit={.size=length,.line=1,.column=1};
            placement current={&localUnit,p->offset,0,0};
            if(local.parsed!=1 || !local.prefix || !location(text,p->offset,&current.line,&current.column) ||
               !mapRange(local.replacement,&current,&s->cursorRange)) goto failed;
            s->cursor=mode; s->cursorOffset=query->offset;
            s->cursorParsed=local.parsed; s->cursorPrefix=1;
            if(!attachSource(s,s->parse.root)) goto failed;
            query->prefix=local.prefix; local.prefix=NULL;
            query->replacement=s->cursorRange; query->parsed=local.parsed;
        }
        xl6CompletionFree(&local); xl6ParseResultFree(&parsed);
        work->reused+=old->units.Count-1; return s;
failed: xl6CompletionFree(&local); xl6ParseResultFree(&parsed); xlsSyntaxRelease(s); return NULL;
    }
    return NULL;
}
static xls_syntax* build(const char* path,xls_text* text,const xls_syntax* previous,
    XL6_CompletionQuery* query,xls_parse_work* work)
{
    if(!path || !*path || !text || !work) return NULL;
    xls_syntax* s=incremental(path,text,previous,query,work,1,NULL,NULL); if(s) return s;
    s=allocate(path,text); if(!s) return NULL;
    ++work->full; work->bytes+=xlsTextSize(text);
    s->accepted=query ? xl6ParseSourceWithCompletion(xlsTextData(text),s->path,&s->parse,query) :
        xl6ParseSource(xlsTextData(text),s->path,&s->parse);
    if(query) {
        s->cursor=1; s->cursorOffset=query->offset; s->cursorRange=query->replacement;
        s->cursorParsed=query->parsed; s->cursorPrefix=query->prefix!=NULL;
        s->cursorError=query->error ? xrtStrDup(query->error) : NULL;
        if(query->error && !s->cursorError) { xlsSyntaxRelease(s); return NULL; }
    }
    if(s->accepted && !decompose(s)) { xlsSyntaxRelease(s); return NULL; }
    return s;
}
xls_syntax* xlsSyntaxBuild(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work)
{ return build(path,text,previous,NULL,work); }
xls_syntax* xlsSyntaxAdoptOrdinary(const char* path,xls_text* text,XL6_AstNode** root)
{
    if(!path || !*path || !text || !root || !*root) return NULL;
    xls_syntax* s=allocate(path,text); if(!s) return NULL;
    s->parse.root=*root; *root=NULL; s->accepted=1;
    if(s->parse.root->xlsStorage || !decompose(s)) goto failed;
    for(size_t i=0;i<s->units.Count;++i) {
        syntax_unit* unit=((placement*)xrtArrayGet(&s->units,i))->unit;
        if(!cacheTreeBytes(unit->node,SIZE_MAX,&unit->storageBytes)) goto failed;
        unit->storageKnown=1;
    }
    return s;
failed: xlsSyntaxRelease(s); return NULL;
}
xls_syntax* xlsSyntaxTryFunction(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work)
{
    if(!path || !*path || !text || !work || !previous || previous->cursor) return NULL;
    return incremental(path,text,previous,NULL,work,0,NULL,NULL);
}
static int emptyQuery(const XL6_CompletionQuery* query)
{
    return query && !query->prefix && !query->parsed && !query->reached && !query->available &&
        !query->items && !query->count && !query->capacity && !query->bytes && !query->error &&
        !query->replacement.known && !query->replacement.startOffset && !query->replacement.endOffset &&
        !query->replacement.startLine && !query->replacement.startColumn &&
        !query->replacement.endLine && !query->replacement.endColumn;
}
xls_syntax* xlsSyntaxBuildCursor(const char* path,xls_text* text,const xls_syntax* previous,
    XL6_CompletionQuery* query,xls_parse_work* work)
{
    if(!emptyQuery(query) || !xl6CompletionTarget(query,path)) return NULL;
    return build(path,text,previous,query,work);
}
int xlsSyntaxAccepted(const xls_syntax* syntax) { return syntax && syntax->accepted; }
int xlsSyntaxIsCursor(const xls_syntax* syntax) { return syntax && syntax->cursor; }
int xlsSyntaxCursorMatches(const xls_syntax* syntax,const char* path,size_t offset)
{return syntax && syntax->cursor==1 && path && !strcmp(syntax->path,path) && syntax->cursorOffset==offset;}
const char* xlsSyntaxPath(const xls_syntax* syntax) { return syntax ? syntax->path : NULL; }
const xls_text* xlsSyntaxText(const xls_syntax* syntax) { return syntax ? syntax->text : NULL; }
static XL6_AstNode* materialize(const xls_syntax* s,int compilerPrivate)
{
    xls_ast_clone_options rootOptions={.dropSources=1};
    XL6_AstNode* root=compilerPrivate ? xlsAstMaterialize(s->parse.root,&rootOptions,NULL) : copyNode(s->parse.root,NULL,0); int okay=root!=NULL;
    if(okay && s->units.Count) {
        root->children=xrtCalloc(s->units.Count,sizeof(*root->children)); okay=root->children!=NULL; root->childCapacity=s->units.Count;
    }
    for(size_t i=0;okay && i<s->units.Count;++i) {
        const placement* p=xrtArrayConstGet(&s->units,i);
        xls_ast_clone_options options={placeNode,(void*)p,0,1,NULL};
        XL6_AstNode* node=compilerPrivate ? xlsAstMaterialize(p->unit->node,&options,NULL) : copyNode(p->unit->node,p,0);
        if(!node) okay=0; else root->children[root->childCount++]=node;
    }
    if(okay) okay=attachSource(s,root);
    if(okay) return root;
    xl6AstFree(root); return NULL;
}
int xlsSyntaxCopyParse(const xls_syntax* s,XL6_ParseResult* result)
{
    if(!s || !result || result->root || result->diags.count) return 0;
    XL6_ParseResult copy; xl6ParseResultInit(&copy);
    if(s->parse.root && !(copy.root=materialize(s,1))) goto failed;
    for(size_t i=0;i<s->parse.diags.count;++i) {
        const XL6_Diag* d=&s->parse.diags.items[i];
        xl6DiagAddFile(&copy.diags,d->file ? d->file : s->path,d->line,d->column,d->message);
        if(copy.diags.count!=i+1) goto failed;
    }
    xl6ParseResultFree(result); *result=copy; return 1;
failed: xl6ParseResultFree(&copy); return 0;
}
static int copyProjected(const xls_syntax* s,XL6_CompletionQuery* query,XL6_ParseResult* result,int mode)
{
    if(!s || s->cursor!=mode || s->cursorParsed!=1 || !emptyQuery(query) ||
       query->offset!=s->cursorOffset || !xl6CompletionTarget(query,s->path) ||
       !result || result->root || result->diags.count) return 0;
    char* prefix=NULL;
    if(s->cursorPrefix && mode==2){prefix=xrtStrDup("");if(!prefix)return 0;}
    else if(s->cursorPrefix) {
        size_t size=xlsTextSize(s->text)-bom(s->text),start=s->cursorRange.startOffset;
        if(!s->cursorRange.known || start>query->offset || query->offset>s->cursorRange.endOffset ||
           s->cursorRange.endOffset>size) return 0;
        prefix=xrtStrDupN(xlsTextData(s->text)+bom(s->text)+start,query->offset-start);
        if(!prefix) return 0;
    }
    XL6_ParseResult copy; xl6ParseResultInit(&copy);
    if(!xlsSyntaxCopyParse(s,&copy)) { xrtFree(prefix); return 0; }
    xl6ParseResultFree(result); *result=copy;
    query->prefix=prefix; query->error=s->cursorError;
    query->parsed=s->cursorParsed; query->replacement=s->cursorRange;
    return 1;
}
int xlsSyntaxCopyCursorParse(const xls_syntax* s,XL6_CompletionQuery* query,XL6_ParseResult* result)
{return copyProjected(s,query,result,1);}
int xlsSyntaxSignatureMatches(const xls_syntax* s,const char* path,size_t offset,size_t callOpen)
{return s && s->cursor==2 && path && !strcmp(s->path,path) && s->cursorOffset==offset && s->cursorRange.known &&
    s->cursorRange.startOffset==callOpen && callOpen<SIZE_MAX && s->cursorRange.endOffset==callOpen+1 && offset>=callOpen+1;}
int xlsSyntaxCopySignatureParse(const xls_syntax* s,XL6_CompletionQuery* query,XL6_ParseResult* result,size_t callOpen)
{return query && xlsSyntaxSignatureMatches(s,query->path,query->offset,callOpen) && copyProjected(s,query,result,2);}
xls_syntax* xlsSyntaxTrySignatureFunction(const char* path,xls_text* text,const xls_syntax* previous,
    XL6_CompletionQuery* query,xls_syntax_parse_fn parser,void* user,xls_parse_work* work)
{
    if(!path || !text || !previous || previous->cursor!=2 || !parser || !work || !emptyQuery(query) || !xl6CompletionTarget(query,path))return NULL;
    return incremental(path,text,previous,query,work,2,parser,user);
}
xls_syntax* xlsSyntaxCaptureSignature(const char* path,xls_text* text,const XL6_AstNode* root,const XL6_CompletionQuery* query,size_t bytes)
{
    if(!path || !text || !root || !query || !xl6CompletionTarget(query,path) || query->parsed!=1 || query->error || !query->prefix || *query->prefix || !query->replacement.known ||
       query->replacement.startOffset==SIZE_MAX || query->replacement.endOffset!=query->replacement.startOffset+1 || query->offset<query->replacement.endOffset ||
       query->offset>xlsTextSize(text)-bom(text) || xlsTextData(text)[bom(text)+query->replacement.startOffset]!='(')return NULL;
    xls_syntax* s=allocate(path,text);if(!s)return NULL;
    xls_ast_clone_options options={.dropSources=1,.bytes=bytes};s->parse.root=xlsAstClone(root,&options,NULL);
    s->accepted=1;s->cursor=2;s->cursorOffset=query->offset;s->cursorRange=query->replacement;s->cursorParsed=1;s->cursorPrefix=1;
    size_t charge=0;
    if(!s->parse.root || !attachSource(s,s->parse.root) || !decompose(s))goto failed;
    for(size_t i=0;i<s->units.Count;++i){
        syntax_unit* unit=((placement*)xrtArrayGet(&s->units,i))->unit;
        if(!cacheTreeBytes(unit->node,bytes,&unit->storageBytes))goto failed;
        unit->storageKnown=1;
    }
    if(!xlsSyntaxSignatureCacheBytes(s,bytes,&charge))goto failed;
    return s;
failed:xlsSyntaxRelease(s);return NULL;
}
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* s)
{
    if(!s || !s->units.Count) return s ? s->parse.root : NULL;
    if(!xrtMutexLock(&s->view->lock)) return NULL;
    if(!s->view->root) s->view->root=materialize(s,0);
    const XL6_AstNode* root=s->view->root; xrtMutexUnlock(&s->view->lock); return root;
}
typedef struct walk_node { const XL6_AstNode* node; const placement* place; } walk_node;
int xlsSyntaxVisit(const xls_syntax* s,xls_syntax_visit visit,void* user,int (*cancelled)(void*),void* cancelUser)
{
    if(!s || !visit) return 0;
    xarray stack={0}; int okay=xrtArrayInit(&stack,sizeof(walk_node)); size_t visited=0;
    if(s->units.Count) {
        for(size_t i=s->units.Count;okay && i>0;--i) { const placement* p=xrtArrayConstGet(&s->units,i-1); walk_node n={p->unit->node,p}; okay=xrtArrayPush(&stack,&n); }
    } else if(s->parse.root) { walk_node n={s->parse.root,NULL}; okay=okay && xrtArrayPush(&stack,&n); }
    while(okay && stack.Count) {
        if((visited++&63u)==0 && cancelled && cancelled(cancelUser)) { okay=0; break; }
        walk_node item=*(walk_node*)xrtArrayGet(&stack,stack.Count-1); --stack.Count;
        XL6_AstRange range=item.node->range;
        if(item.place && !mapRange(range,item.place,&range)) { okay=0; break; }
        int action=visit(item.node,&range,user);
        if(action<=0) { okay=action==0; break; }
        for(size_t i=item.node->childCount;okay && i>0;--i) { walk_node n={item.node->children[i-1],item.place}; if(n.node) okay=xrtArrayPush(&stack,&n); }
    }
    xrtArrayUnit(&stack); return okay && !(cancelled && cancelled(cancelUser));
}
size_t xlsSyntaxDiagnosticCount(const xls_syntax* syntax) { return syntax ? syntax->parse.diags.count : 0; }
int xlsSyntaxDiagnostic(const xls_syntax* syntax,size_t index,xls_syntax_diagnostic* result)
{
    if(!result) return 0;
    memset(result,0,sizeof(*result));
    if(!syntax || index>=syntax->parse.diags.count) return 0;
    const XL6_Diag* d=&syntax->parse.diags.items[index];
    *result=(xls_syntax_diagnostic){d->line,d->column,d->file ? d->file : syntax->path,d->message}; return 1;
}
