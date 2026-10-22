/* Private scope names are borrowed from declarations/parameters/targets.
 * Reference rewriting changes callees/member expressions, not those names.
 * Every producer is ScopeAdd; all five scope owners unconditionally Free.
 * This phase ends before the existing spelling-discovery index starts. */
typedef struct xls_stdlib_scope_index {
    xmap names;
    xls_stdlib_scan* owner;
    const char** source;
    size_t count,charged;
} xls_stdlib_scope_index;
static void xlsStdlibScopeFreeOriginal(XL6_StdlibNameScope*);
static int xlsStdlibScopeContainsOriginal(const XL6_StdlibNameScope*,const char*);
static int xlsStdlibScopeAddOriginal(XL6_StdlibNameScope*,const char*);
static void xlsStdlibScopeDrop(XL6_StdlibNameScope* scope)
{
    xls_stdlib_scope_index* index=scope->xlsIndex;
    if(index) {
        index->owner->scopeBytes-=index->charged;
        xrtMapUnit(&index->names); xrtFree(index); scope->xlsIndex=NULL;
    }
    scope->xlsIndexDisabled=1;
}
static int xlsStdlibScopeCharge(xls_stdlib_scope_index* index,size_t bytes)
{
    xls_stdlib_scan* owner=index->owner;
    if(bytes>owner->limit-owner->scopeBytes) return 0;
    owner->scopeBytes+=bytes; index->charged+=bytes;
    if(owner->scopeBytes>owner->work.scopePeakBytes) owner->work.scopePeakBytes=owner->scopeBytes;
    return 1;
}
static int xlsStdlibScopeInsert(xls_stdlib_scope_index* index,const char* name,size_t at)
{
    size_t size=xlsStdlibSpellingSize(name);
    if(!size || size>XLS_STDLIB_NAME_BYTES) return 0;
    xbytesview key={(const unsigned char*)name,size};
    if(xrtMapHas(&index->names,key)) return 1;
    return xlsStdlibScopeCharge(index,size+1+96) && xrtMapSet(&index->names,key,&at);
}
static xls_stdlib_scope_index* xlsStdlibScopeIndex(XL6_StdlibNameScope* scope,xls_stdlib_scan* owner)
{
    if(scope->xlsIndexDisabled) return NULL;
    if(scope->xlsIndex) {
        xls_stdlib_scope_index* index=scope->xlsIndex;
        if(index->owner==owner && index->count==scope->count && index->source==scope->names) return index;
        xlsStdlibScopeDrop(scope); return NULL;
    }
    if(scope->count<64) return NULL;
    size_t initial=sizeof(xls_stdlib_scope_index)+256;
    if(initial>owner->limit-owner->scopeBytes) { scope->xlsIndexDisabled=1; return NULL; }
    xls_stdlib_scope_index* index=xrtCalloc(1,sizeof(*index));
    if(!index) { scope->xlsIndexDisabled=1; return NULL; }
    scope->xlsIndex=index; index->owner=owner;
    if(!xlsStdlibScopeCharge(index,initial) || !xrtMapInit(&index->names,sizeof(size_t))) {
        xlsStdlibScopeDrop(scope); return NULL;
    }
    for(size_t i=0;i<scope->count;++i) if(!xlsStdlibScopeInsert(index,scope->names[i],i)) {
        xlsStdlibScopeDrop(scope); return NULL;
    }
    index->count=scope->count; index->source=scope->names; ++owner->work.scopeIndexes; return index;
}
static int xl6StdlibScopeContains(const XL6_StdlibNameScope* scope,const char* name)
{
    xls_stdlib_scan* owner=xlsActiveStdlibScan;
    if(owner) ++owner->work.scopeQueries;
    if(!owner || !owner->limit || !name || xlsStdlibSpellingSize(name)>XLS_STDLIB_NAME_BYTES)
        return xlsStdlibScopeContainsOriginal(scope,name);
    for(;scope;scope=scope->parent) {
        /* All private scopes are mutable stack owners, including parents;
         * const here describes the logical membership query. */
        xls_stdlib_scope_index* index=xlsStdlibScopeIndex((XL6_StdlibNameScope*)scope,owner);
        XL6_StdlibNameScope one=*scope; one.parent=NULL; one.xlsIndex=NULL;
        if(index) {
            const size_t* at=xrtMapGet(&index->names,(xbytesview){(const unsigned char*)name,strlen(name)});
            if(!at) { ++owner->work.scopePruned; continue; }
            if(*at>=scope->count) { xlsStdlibScopeDrop((XL6_StdlibNameScope*)scope); return xlsStdlibScopeContainsOriginal(scope,name); }
            one.names=scope->names+*at; one.count=one.capacity=1;
            if(xlsStdlibScopeContainsOriginal(&one,name)) { ++owner->work.scopeHits; return 1; }
            xlsStdlibScopeDrop((XL6_StdlibNameScope*)scope); return xlsStdlibScopeContainsOriginal(scope,name);
        }
        if(xlsStdlibScopeContainsOriginal(&one,name)) return 1;
    }
    return 0;
}
static int xl6StdlibScopeAdd(XL6_StdlibNameScope* scope,const char* name)
{
    size_t before=scope ? scope->count : 0;
    int okay=xlsStdlibScopeAddOriginal(scope,name);
    if(okay && scope && scope->xlsIndex && scope->count!=before) {
        xls_stdlib_scope_index* index=scope->xlsIndex;
        if(index->count!=before || scope->count!=before+1 || !xlsStdlibScopeInsert(index,scope->names[before],before))
            xlsStdlibScopeDrop(scope);
        else { index->count=scope->count; index->source=scope->names; }
    }
    return okay;
}
static void xl6StdlibScopeFree(XL6_StdlibNameScope* scope)
{
    if(scope) xlsStdlibScopeDrop(scope);
    xlsStdlibScopeFreeOriginal(scope);
}
