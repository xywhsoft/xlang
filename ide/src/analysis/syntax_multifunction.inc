/* Ordinary multi-function edits. Byte anchors select candidate boundaries;
 * they never establish grammar. Every changed item must be consumed exactly
 * by the original module parser as one complete same-kind function. Every
 * inter-item byte remains exact, so lexer/doc seams are unchanged. */
#define XLS_MULTI_FUNCTIONS 64u
#define XLS_MULTI_ANCHOR_BYTES 8192u
#define XLS_MULTI_SEARCH_BYTES (8u*1024u*1024u)
static int multiAnchor(const xls_syntax* old,size_t at,size_t oldEnd,xstrview* anchor)
{
    const placement* next=xrtArrayConstGet(&old->units,at+1);
    if(!next)return 0;
    size_t header=0,end=0;
    if(!xlsSyntaxFunctionHeader(next->unit->node,&header,&end) || header<next->unit->offset ||
       header-next->unit->offset>next->unit->size || next->offset<oldEnd)return 0;
    size_t stop=next->offset+header-next->unit->offset;
    size_t size=xlsTextSize(old->text)-bom(old->text);
    if(stop>size || stop<=oldEnd || stop-oldEnd>XLS_MULTI_ANCHOR_BYTES)return 0;
    *anchor=(xstrview){xlsTextData(old->text)+bom(old->text)+oldEnd,stop-oldEnd};return 1;
}
static xls_syntax* incrementalMultiple(const char* path,xls_text* text,const xls_syntax* old,xls_parse_work* work)
{
    if(!old || !old->accepted || !old->incremental || old->cursor || strcmp(path,old->path) ||
       bom(text)!=bom(old->text) || old->units.Count<2)return NULL;
    xls_text_edit edit;if(xlsTextDiff(old->text,text,&edit)!=1)return NULL;
    size_t skip=bom(text),newSize=xlsTextSize(text),oldSize=xlsTextSize(old->text);
    size_t prefix=(size_t)(edit.text-xlsTextData(text));
    if(prefix<skip || newSize>SIZE_MAX/4)return NULL;
    size_t oldEnd=oldSize-(newSize-prefix-edit.length)-skip;prefix-=skip;
    size_t first=SIZE_MAX,last=SIZE_MAX;
    for(size_t i=0;i<old->units.Count;++i){
        const placement* p=xrtArrayConstGet(&old->units,i);size_t h=0,end=0;
        if(!xlsSyntaxFunctionHeader(p->unit->node,&h,&end))continue;
        end=p->offset+end-p->unit->offset;
        if(prefix>p->offset && prefix<p->offset+p->unit->size)first=i;
        if(oldEnd>=p->offset && oldEnd<=end)last=i;
    }
    if(first==SIZE_MAX || last==SIZE_MAX || first>=last)return NULL;
    const char* before=xlsTextData(old->text)+skip;const char* after=xlsTextData(text)+skip;
    size_t previous=0,current=0,changed=0,search=0;
    size_t searchLimit=newSize*4<XLS_MULTI_SEARCH_BYTES ? newSize*4 : XLS_MULTI_SEARCH_BYTES;
    newSize-=skip;oldSize-=skip;
    xls_syntax* s=allocate(path,text);if(!s)return NULL;
    s->parse.root=copyNode(old->parse.root,NULL,0);
    int64_t delta=(int64_t)newSize-(int64_t)oldSize;
    if(!s->parse.root || !shiftRange(&s->parse.root->range,text,oldEnd,delta) ||
       !shiftRange(&s->parse.root->tokenRange,text,oldEnd,delta))goto failed;
    for(size_t i=0;i<old->units.Count;++i){
        const placement* p=xrtArrayConstGet(&old->units,i);const syntax_unit* u=p->unit;
        if(p->offset<previous || p->offset>oldSize || u->size>oldSize-p->offset || current>newSize)goto failed;
        size_t gap=p->offset-previous;
        if(gap>newSize-current || memcmp(before+previous,after+current,gap))goto failed;
        current+=gap;size_t length=u->size;syntax_unit* owned=NULL;
        if(length<=newSize-current && !memcmp(before+p->offset,after+current,length))owned=retainUnit(p->unit);
        else{
            size_t h=0,end=0;
            if(changed>=XLS_MULTI_FUNCTIONS || !xlsSyntaxFunctionHeader(u->node,&h,&end))goto failed;
            if(i+1<old->units.Count){
                xstrview anchor={0};
                if(!multiAnchor(old,i,p->offset+u->size,&anchor) || current==newSize ||
                   newSize-current>searchLimit-search)goto failed;
                search+=newSize-current;
                size_t at=xrtStrFind((xstrview){after,newSize},anchor,current+1);
                if(at==XRT_NPOS || at<=current)goto failed;
                if(newSize-at-1>searchLimit-search)goto failed;
                search+=newSize-at-1;
                if(xrtStrFind((xstrview){after,newSize},anchor,at+1)!=XRT_NPOS)goto failed;
                length=at-current;
            }else{
                size_t tail=oldSize-p->offset-u->size;
                if(tail>newSize-current || memcmp(before+oldSize-tail,after+newSize-tail,tail))goto failed;
                length=newSize-current-tail;
            }
            char* fragment=xrtStrDupN(after+current,length);if(!fragment)goto failed;
            XL6_ParseResult parsed;xl6ParseResultInit(&parsed);++work->fragments;work->bytes+=length;
            int okay=xl6ParseSource(fragment,path,&parsed);xrtFree(fragment);
            XL6_AstNode* fresh=okay && !parsed.diags.count && parsed.root && parsed.root->childCount==1 ? parsed.root->children[0] : NULL;
            if(!fresh || fresh->kind!=u->node->kind || !fresh->range.known || fresh->range.startOffset ||
               fresh->range.endOffset!=length || !xlsSyntaxFunctionHeader(fresh,&h,&end)){
                xl6ParseResultFree(&parsed);goto failed;
            }
            owned=xrtCalloc(1,sizeof(*owned));
            if(!owned){xl6ParseResultFree(&parsed);goto failed;}
            xrtAtomic32Init(&owned->references,1);owned->node=fresh;owned->size=length;owned->line=owned->column=1;
            parsed.root->children[0]=NULL;
            /* Native complete ranges include their #doc block. Keep exactly
             * what this parse owns,including NULL after document removal. */
            if(u->storageKnown && !cacheTreeBytes(fresh,SIZE_MAX,&owned->storageBytes)){
                releaseUnit(owned);xl6ParseResultFree(&parsed);goto failed;
            }
            owned->storageKnown=u->storageKnown;xl6ParseResultFree(&parsed);++changed;
        }
        placement placed={owned,current,0,0};
        if(!location(text,current,&placed.line,&placed.column) || !xrtArrayPush(&s->units,&placed)){
            releaseUnit(owned);goto failed;
        }
        previous=p->offset+u->size;current+=length;
    }
    if(previous>oldSize || current>newSize || oldSize-previous!=newSize-current ||
       memcmp(before+previous,after+current,oldSize-previous) || changed<2)goto failed;
    s->accepted=s->incremental=1;work->reused+=old->units.Count-changed;return s;
failed:xlsSyntaxRelease(s);return NULL;
}
