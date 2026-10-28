/* Private completed-data accounting for ordinary pre-semantic module chunks.
 * Fields are measured before publishing a chunk and never changed afterwards. */
#include "analysis/syntax_node_storage.h"
#if TREE_DEPTH != XLS_SYNTAX_STORAGE_DEPTH
#error Syntax storage depth must retain the original accounting predicate
#endif
static int cacheAdd(size_t* total,size_t bytes,size_t limit)
{return xlsSyntaxStorageAdd(total,bytes,limit);}
static int cacheNodeBytes(const XL6_AstNode* node,size_t depth,xmap* sources,const XL6_AstSource** recent,size_t* bytes,size_t limit)
{
    if(!node) return 1;
    if(!xlsSyntaxNodeStorage(node,depth,bytes,limit))return 0;
    if(node->source) {
        const XL6_AstSource* source=node->source;
        xbytesview key={(const unsigned char*)&source,sizeof(source)};
        /* An immutable walk may reuse the immediately preceding admitted
         * source identity. Other identities still use the original XRT map;
         * equal path text never merges distinct owned source records. */
        if(source!=*recent) {
            if(!xrtMapHas(sources,key)) {
                unsigned char one=1;
                if(!cacheAdd(bytes,sizeof(*source),limit) ||
                   (source->path && !cacheAdd(bytes,strlen(source->path)+1,limit)) ||
                   !xrtMapSet(sources,key,&one)) return 0;
            }
            *recent=source;
        }
    }
    for(size_t i=0;i<node->childCount;++i)
        if(!cacheNodeBytes(node->children[i],depth+1,sources,recent,bytes,limit)) return 0;
    return cacheNodeBytes(node->inferredType,depth+1,sources,recent,bytes,limit) &&
        cacheNodeBytes(node->semanticDeclaredType,depth+1,sources,recent,bytes,limit);
}
static int cacheTreeBytes(const XL6_AstNode* node,size_t limit,size_t* bytes)
{
    xmap sources={0}; const XL6_AstSource* recent=NULL; *bytes=0;
    int okay=xrtMapInit(&sources,sizeof(unsigned char)) && cacheNodeBytes(node,0,&sources,&recent,bytes,limit);
    xrtMapUnit(&sources); if(!okay) *bytes=0; return okay;
}
static int syntaxCacheBytes(const xls_syntax* s,size_t limit,size_t* bytes,int mode)
{
    if(bytes) *bytes=0;
    if(!s || !bytes || !s->accepted || s->cursor!=mode || s->cursorError || s->parse.diags.count ||
       !s->view || s->view->root || s->units.Capacity>SIZE_MAX/sizeof(placement)) return 0;
    size_t total=sizeof(*s),text=xlsTextStorageBytes(s->text),tree=0;
    if(!text || !cacheAdd(&total,strlen(s->path)+1,limit) ||
       !cacheAdd(&total,sizeof(*s->view),limit) || !cacheAdd(&total,text,limit) ||
       !cacheAdd(&total,s->units.Capacity*sizeof(placement),limit) ||
       !cacheTreeBytes(s->parse.root,limit,&tree) || !cacheAdd(&total,tree,limit)) return 0;
    for(size_t i=0;i<s->units.Count;++i) {
        const syntax_unit* unit=((const placement*)xrtArrayConstGet(&s->units,i))->unit;
        if(!unit->storageKnown || !cacheAdd(&total,sizeof(*unit),limit) ||
           !cacheAdd(&total,unit->storageBytes,limit)) return 0;
    }
    *bytes=total; return 1;
}
int xlsSyntaxCacheBytes(const xls_syntax* s,size_t limit,size_t* bytes)
{return syntaxCacheBytes(s,limit,bytes,0);}
int xlsSyntaxSignatureCacheBytes(const xls_syntax* s,size_t limit,size_t* bytes)
{return syntaxCacheBytes(s,limit,bytes,2);}
