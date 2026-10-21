/* Immutable declaration chunks around the shared lexer/parser. This module
 * owns data/range transformations, never a second grammar or lexer. */
#include "analysis/frontend_internal.h"
#include <limits.h>
#include <string.h>

#define UNIT_LIMIT 65536u
#define TREE_DEPTH 512u
typedef struct syntax_unit {
    xatomic32 references;
    XL6_AstNode* node;
    size_t offset,size;
    int line,column;
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
    int accepted,incremental;
};
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
    xlsTextRelease(syntax->text); xrtFree(syntax->path); xrtFree(syntax);
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
static XL6_AstNode* copyNode(const XL6_AstNode* in,const placement* p,size_t depth)
{
    if(!in || depth>TREE_DEPTH) return NULL;
    XL6_AstNode* out=xrtCalloc(1,sizeof(*out)); if(!out) return NULL;
    *out=*in; out->text=out->doc=NULL; out->literalData=NULL; out->source=NULL;
    out->children=NULL; out->childCount=out->childCapacity=0; out->inferredType=out->semanticDeclaredType=NULL;
    if(p && (!mapRange(in->range,p,&out->range) || !mapRange(in->tokenRange,p,&out->tokenRange) ||
       (in->line && !mapPoint(in->line,in->column,p,&out->line,&out->column)))) goto failed;
    if(in->text && !(out->text=xrtStrDup(in->text))) goto failed;
    if(in->doc && !(out->doc=xrtStrDup(in->doc))) goto failed;
    if(in->literalData) {
        if(!xl6AstSetLiteralData(out,in->literalData,in->literalSize)) goto failed;
        out->literalDecoded=in->literalDecoded;
    }
    if(in->childCount) {
        out->children=xrtCalloc(in->childCount,sizeof(*out->children)); if(!out->children) goto failed;
        out->childCapacity=in->childCount;
        for(size_t i=0;i<in->childCount;++i) {
            XL6_AstNode* child=copyNode(in->children[i],p,depth+1); if(!child) goto failed;
            out->children[out->childCount++]=child;
        }
    }
    if(in->inferredType && !(out->inferredType=copyNode(in->inferredType,p,depth+1))) goto failed;
    if(in->semanticDeclaredType && !(out->semanticDeclaredType=copyNode(in->semanticDeclaredType,p,depth+1))) goto failed;
    return out;
failed: xl6AstFree(out); return NULL;
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
static int functionHeader(const XL6_AstNode* node,size_t* header,size_t* end)
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
static xls_syntax* incremental(const char* path,xls_text* text,const xls_syntax* old,xls_parse_work* work)
{
    if(!old || !old->accepted || !old->incremental || strcmp(path,old->path) || bom(text)!=bom(old->text)) return NULL;
    xls_text_edit edit; if(xlsTextDiff(old->text,text,&edit)!=1) return NULL;
    size_t prefix=(size_t)(edit.text-xlsTextData(text)),newSize=xlsTextSize(text),oldSize=xlsTextSize(old->text),skip=bom(text);
    size_t oldEnd=oldSize-(newSize-prefix-edit.length);
    if(prefix<skip) return NULL;
    prefix-=skip; oldEnd-=skip;
    int64_t delta=(int64_t)newSize-(int64_t)oldSize;
    for(size_t i=0;i<old->units.Count;++i) {
        const placement* p=xrtArrayConstGet(&old->units,i); const syntax_unit* unit=p->unit;
        size_t header=0,end=0;
        if(!functionHeader(unit->node,&header,&end)) continue;
        header=p->offset+header-unit->offset; end=p->offset+end-unit->offset;
        if(prefix<header || oldEnd>end) continue;
        size_t length=(size_t)((int64_t)unit->size+delta);
        if(p->offset>newSize-skip || length>newSize-skip-p->offset) return NULL;
        char* fragment=xrtStrDupN(xlsTextData(text)+skip+p->offset,length); if(!fragment) return NULL;
        XL6_ParseResult parsed; xl6ParseResultInit(&parsed);
        ++work->fragments; work->bytes+=length;
        int okay=xl6ParseSource(fragment,path,&parsed); xrtFree(fragment);
        XL6_AstNode* fresh=okay && parsed.root && parsed.root->childCount==1 ? parsed.root->children[0] : NULL;
        size_t freshHeader=0,freshEnd=0;
        if(!fresh || fresh->kind!=unit->node->kind || !fresh->range.known || fresh->range.startOffset || fresh->range.endOffset!=length ||
           !functionHeader(fresh,&freshHeader,&freshEnd) || freshHeader!=header-p->offset) { xl6ParseResultFree(&parsed); return NULL; }
        xls_syntax* s=allocate(path,text); if(!s) { xl6ParseResultFree(&parsed); return NULL; }
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
                next.unit=changed;
            } else next.unit=retainUnit(prior->unit);
            if(j>i) next.offset=(size_t)((int64_t)next.offset+delta);
            if(!location(text,next.offset,&next.line,&next.column) || !xrtArrayPush(&s->units,&next)) { releaseUnit(next.unit); goto failed; }
        }
        xl6ParseResultFree(&parsed); s->accepted=s->incremental=1; work->reused+=old->units.Count-1; return s;
failed: xl6ParseResultFree(&parsed); xlsSyntaxRelease(s); return NULL;
    }
    return NULL;
}
xls_syntax* xlsSyntaxBuild(const char* path,xls_text* text,const xls_syntax* previous,xls_parse_work* work)
{
    if(!path || !*path || !text || !work) return NULL;
    xls_syntax* s=incremental(path,text,previous,work); if(s) return s;
    s=allocate(path,text); if(!s) return NULL;
    ++work->full; work->bytes+=xlsTextSize(text);
    s->accepted=xl6ParseSource(xlsTextData(text),s->path,&s->parse);
    if(s->accepted && !decompose(s)) { xlsSyntaxRelease(s); return NULL; }
    return s;
}
int xlsSyntaxAccepted(const xls_syntax* syntax) { return syntax && syntax->accepted; }
const char* xlsSyntaxPath(const xls_syntax* syntax) { return syntax ? syntax->path : NULL; }
const xls_text* xlsSyntaxText(const xls_syntax* syntax) { return syntax ? syntax->text : NULL; }
static XL6_AstNode* materialize(const xls_syntax* s)
{
    XL6_AstNode* root=copyNode(s->parse.root,NULL,0); int okay=root!=NULL;
    if(okay && s->units.Count) {
        root->children=xrtCalloc(s->units.Count,sizeof(*root->children)); okay=root->children!=NULL; root->childCapacity=s->units.Count;
    }
    for(size_t i=0;okay && i<s->units.Count;++i) {
        const placement* p=xrtArrayConstGet(&s->units,i); XL6_AstNode* node=copyNode(p->unit->node,p,0);
        if(!node) okay=0; else root->children[root->childCount++]=node;
    }
    if(okay) okay=xl6AstSetSourceTree(root,s->path);
    if(okay) return root;
    xl6AstFree(root); return NULL;
}
int xlsSyntaxCopyParse(const xls_syntax* s,XL6_ParseResult* result)
{
    if(!s || !result || result->root || result->diags.count) return 0;
    XL6_ParseResult copy; xl6ParseResultInit(&copy);
    if(s->parse.root && !(copy.root=materialize(s))) goto failed;
    for(size_t i=0;i<s->parse.diags.count;++i) {
        const XL6_Diag* d=&s->parse.diags.items[i];
        xl6DiagAddFile(&copy.diags,d->file ? d->file : s->path,d->line,d->column,d->message);
        if(copy.diags.count!=i+1) goto failed;
    }
    xl6ParseResultFree(result); *result=copy; return 1;
failed: xl6ParseResultFree(&copy); return 0;
}
const XL6_AstNode* xlsSyntaxAst(const xls_syntax* s)
{
    if(!s || !s->units.Count) return s ? s->parse.root : NULL;
    if(!xrtMutexLock(&s->view->lock)) return NULL;
    if(!s->view->root) s->view->root=materialize(s);
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
