/* Read immutable parser chunks directly, placing values only on the stack. */
#include "analysis/cursor_word_view.h"
static int cursorWordText(const char* a,const char* b)
{return a && b ? !strcmp(a,b) : a==b;}
static int cursorWordOffset(const xls_cursor_word_view* e,size_t in,size_t* out)
{
    if(in>e->before.startOffset && in<e->before.endOffset)return 0;
    if(in>=e->before.endOffset){
        size_t tail=in-e->before.endOffset;
        if(tail>SIZE_MAX-e->after.endOffset)return 0;
        *out=e->after.endOffset+tail;
    }else *out=in;
    return 1;
}
static int cursorWordPoint(const xls_cursor_word_view* e,int line,int column,int* out)
{
    *out=column;
    if(line!=e->before.startLine)return 1;
    if(column>e->before.startColumn && column<e->before.endColumn)return 0;
    if(column>=e->before.endColumn){
        int64_t moved=(int64_t)column-e->before.endColumn+e->after.endColumn;
        if(moved<1 || moved>INT_MAX)return 0;
        *out=(int)moved;
    }
    return 1;
}
int xlsCursorWordMapRange(const xls_cursor_word_view* e,const XL6_AstRange* in,XL6_AstRange* out)
{
    if(!e || !in || !out || !e->before.known || !e->after.known ||
       e->before.startOffset>=e->before.endOffset || e->after.startOffset>=e->after.endOffset)return 0;
    *out=*in;if(!in->known)return 1;
    return cursorWordOffset(e,in->startOffset,&out->startOffset) && cursorWordOffset(e,in->endOffset,&out->endOffset) &&
        cursorWordPoint(e,in->startLine,in->startColumn,&out->startColumn) &&
        cursorWordPoint(e,in->endLine,in->endColumn,&out->endColumn);
}
static int cursorWordRangeEqual(const XL6_AstRange* a,const XL6_AstRange* b)
{
    return a->known==b->known && a->startOffset==b->startOffset && a->endOffset==b->endOffset &&
        a->startLine==b->startLine && a->startColumn==b->startColumn && a->endLine==b->endLine && a->endColumn==b->endColumn;
}
static int cursorWordNode(const XL6_AstNode* a,const placement* ap,const XL6_AstNode* b,const placement* bp,
    xls_cursor_word_view* e,size_t depth,int (*cancelled)(void*),void* user,int module)
{
    if(!a || !b)return a==b;
    if(depth>512 || e->nodes>=262144 || ((e->nodes++&63u)==0 && cancelled && cancelled(user)))return 0;
    XL6_AstNode left=*a,right=*b;XL6_AstRange range,token;int column;
    if((ap && !placeNode(a,&left,(void*)ap)) || (bp && !placeNode(b,&right,(void*)bp)))return 0;
#define XLS_CURSOR_SAME(field) if(left.field!=right.field)return 0
    XLS_CURSOR_SAME(kind);XLS_CURSOR_SAME(flags);XLS_CURSOR_SAME(literalSize);XLS_CURSOR_SAME(literalScalar);XLS_CURSOR_SAME(literalDecoded);
    XLS_CURSOR_SAME(semanticTypeId);XLS_CURSOR_SAME(semanticTypeAbiId);XLS_CURSOR_SAME(semanticSymbolId);XLS_CURSOR_SAME(semanticDeclaredSymbolId);
    XLS_CURSOR_SAME(semanticTargetSymbolId);XLS_CURSOR_SAME(semanticCompoundOperatorSymbolId);XLS_CURSOR_SAME(semanticCallKind);XLS_CURSOR_SAME(semanticLvalueKind);
    XLS_CURSOR_SAME(semanticOwnership);XLS_CURSOR_SAME(semanticExitKind);XLS_CURSOR_SAME(semanticScopeDepth);XLS_CURSOR_SAME(semanticTargetScopeDepth);
    XLS_CURSOR_SAME(semanticRegionId);XLS_CURSOR_SAME(semanticTargetRegionId);XLS_CURSOR_SAME(line);
#undef XLS_CURSOR_SAME
    if(!cursorWordText(left.text,right.text) || !cursorWordText(left.doc,right.doc) ||
       (left.literalData!=NULL)!=(right.literalData!=NULL) ||
       (left.literalData && left.literalSize && memcmp(left.literalData,right.literalData,left.literalSize)) ||
       !xlsCursorWordMapRange(e,&left.range,&range) || !cursorWordRangeEqual(&range,&right.range) ||
       !xlsCursorWordMapRange(e,&left.tokenRange,&token) || !cursorWordRangeEqual(&token,&right.tokenRange) ||
       !cursorWordPoint(e,left.line,left.column,&column) || column!=right.column)return 0;
    /* CopyParse drops chunk sources and attaches one current path/marker to
     * the complete tree. Those resulting fields are proved by the snapshots'
     * path/cursor contract below,not stale per-chunk source pointer identity. */
    if(!module){
        if(left.childCount!=right.childCount || (left.childCount && (!left.children || !right.children)))return 0;
        for(size_t i=0;i<left.childCount;++i)if(!cursorWordNode(left.children[i],ap,right.children[i],bp,e,depth+1,cancelled,user,0))return 0;
    }
    return cursorWordNode(left.inferredType,ap,right.inferredType,bp,e,depth+1,cancelled,user,0) &&
        cursorWordNode(left.semanticDeclaredType,ap,right.semanticDeclaredType,bp,e,depth+1,cancelled,user,0);
}
int xlsSyntaxCursorWordProof(const xls_syntax* old,const xls_syntax* next,xls_cursor_word_view* out,
    int (*cancelled)(void*),void* user)
{
    if(out)*out=(xls_cursor_word_view){0};
    if(!out || !old || !next || old->cursor!=1 || next->cursor!=1 || !old->accepted || !next->accepted ||
       old->cursorParsed!=1 || next->cursorParsed!=1 || old->cursorError || next->cursorError || !old->cursorPrefix || !next->cursorPrefix ||
       old->parse.diags.count || next->parse.diags.count || strcmp(old->path,next->path) || bom(old->text)!=bom(next->text) ||
       !old->parse.root || !next->parse.root || old->parse.root->kind!=XL6_AST_MODULE || next->parse.root->kind!=XL6_AST_MODULE)return 0;
    xls_cursor_word_view e={.before=old->cursorRange,.after=next->cursorRange,.offset=next->cursorOffset};
    size_t oldSize=xlsTextSize(old->text)-bom(old->text),newSize=xlsTextSize(next->text)-bom(next->text);
    if(!e.before.known || !e.after.known || e.before.startOffset!=e.after.startOffset ||
       e.before.startOffset>=e.before.endOffset || e.after.startOffset>=e.after.endOffset ||
       e.before.endOffset>oldSize || e.after.endOffset>newSize || old->cursorOffset<e.before.startOffset || old->cursorOffset>e.before.endOffset ||
       next->cursorOffset<e.after.startOffset || next->cursorOffset>e.after.endOffset ||
       e.before.startLine<1 || e.before.startColumn<1 || e.before.startLine!=e.before.endLine ||
       e.after.startLine!=e.after.endLine || e.before.startLine!=e.after.startLine || e.before.startColumn!=e.after.startColumn ||
       e.before.endColumn<=e.before.startColumn || e.after.endColumn<=e.after.startColumn ||
       oldSize-e.before.endOffset!=newSize-e.after.endOffset)return 0;
    const char* a=xlsTextData(old->text)+bom(old->text);const char* b=xlsTextData(next->text)+bom(next->text);
    if(memcmp(a,b,e.before.startOffset) || memcmp(a+e.before.endOffset,b+e.after.endOffset,oldSize-e.before.endOffset))return 0;
    size_t oldCount=old->units.Count ? old->units.Count : old->parse.root->childCount;
    size_t newCount=next->units.Count ? next->units.Count : next->parse.root->childCount;
    if(oldCount!=newCount || !cursorWordNode(old->parse.root,NULL,next->parse.root,NULL,&e,0,cancelled,user,1))return 0;
    for(size_t i=0;i<oldCount;++i){
        const placement* ap=old->units.Count ? xrtArrayConstGet(&old->units,i) : NULL;
        const placement* bp=next->units.Count ? xrtArrayConstGet(&next->units,i) : NULL;
        const XL6_AstNode* an=ap ? ap->unit->node : old->parse.root->children[i];
        const XL6_AstNode* bn=bp ? bp->unit->node : next->parse.root->children[i];
        if(!cursorWordNode(an,ap,bn,bp,&e,1,cancelled,user,0))return 0;
    }
    e.prefix=b+e.after.startOffset;e.prefixSize=next->cursorOffset-e.after.startOffset;*out=e;return 1;
}
int xlsSyntaxCursorRetainedBytes(const xls_syntax* s,size_t limit,size_t* bytes)
{
    if(bytes)*bytes=0;
    if(!bytes || !s || s->cursor!=1 || !s->accepted || s->cursorError || s->parse.diags.count || !s->view ||
       s->units.Capacity>limit/sizeof(placement))return 0;
    size_t total=sizeof(*s),tree=0,text=xlsTextStorageBytes(s->text);
    if(!text || !cacheAdd(&total,text,limit) || !cacheAdd(&total,strlen(s->path)+1,limit) ||
       !cacheAdd(&total,sizeof(*s->view),limit) || !cacheAdd(&total,s->units.Capacity*sizeof(placement),limit) ||
       !cacheTreeBytes(s->parse.root,limit,&tree) || !cacheAdd(&total,tree,limit))return 0;
    for(size_t i=0;i<s->units.Count;++i){
        const syntax_unit* u=((const placement*)xrtArrayConstGet(&s->units,i))->unit;
        tree=u->storageBytes;
        if((!u->storageKnown && !cacheTreeBytes(u->node,limit,&tree)) ||
           !cacheAdd(&total,sizeof(*u),limit) || !cacheAdd(&total,tree,limit))return 0;
    }
    if(!xrtMutexLock(&s->view->lock))return 0;
    int okay=!s->view->root || (cacheTreeBytes(s->view->root,limit,&tree) && cacheAdd(&total,tree,limit));
    xrtMutexUnlock(&s->view->lock);if(okay)*bytes=total;return okay;
}
