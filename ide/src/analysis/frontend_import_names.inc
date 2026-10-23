/* Included after the audited shared name-map scans. Add/append order,
 * rewriting, allocation of output strings and language rules remain original. */
#include "analysis/frontend_import_names.h"
#define XLS_IMPORT_NAME_BYTES (8u*1024u*1024u)
#define XLS_IMPORT_NAME_COUNT 65536u
struct xls_import_names_index {xmap rows;size_t indexed,bytes,limit;int enabled;};
static _Thread_local xls_import_names_scope* xlsImportNamesScope;
void xlsFrontendImportNamesBegin(xls_import_names_scope* scope,size_t bytes)
{*scope=(xls_import_names_scope){.previous=xlsImportNamesScope,.bytes=bytes>XLS_IMPORT_NAME_BYTES ? XLS_IMPORT_NAME_BYTES : bytes};xlsImportNamesScope=scope;}
void xlsFrontendImportNamesEnd(xls_import_names_scope* scope,xls_import_names_work* work)
{if(xlsImportNamesScope!=scope) {if(work) *work=(xls_import_names_work){0};return;}if(work) *work=scope->work;xlsImportNamesScope=scope->previous;}
static void xlsImportNamesDisable(xls_import_names_index* index)
{if(index) {xrtMapUnit(&index->rows);index->enabled=0;}}
static void xl6ImportsNameMapInit(XL6_ImportNameMap* map)
{
    xlsImportsNameMapInitOriginal(map);if(!map) return;
    size_t limit=xlsImportNamesScope ? xlsImportNamesScope->bytes : XLS_IMPORT_NAME_BYTES;
    if(limit<sizeof(xls_import_names_index)) return;
    xls_import_names_index* index=xrtCalloc(1,sizeof(*index));if(!index) return;
    if(!xrtMapInit(&index->rows,sizeof(size_t))) {xrtFree(index);return;}
    index->enabled=1;index->limit=limit;index->bytes=sizeof(*index);map->xlsIndex=index;
}
static void xl6ImportsNameMapFree(XL6_ImportNameMap* map)
{
    if(map && map->xlsIndex) {xrtMapUnit(&map->xlsIndex->rows);xrtFree(map->xlsIndex);map->xlsIndex=NULL;}
    xlsImportsNameMapFreeOriginal(map);
}
static xls_import_names_index* xlsImportNamesReady(const XL6_ImportNameMap* map)
{
    xls_import_names_index* index=map ? map->xlsIndex : NULL;
    if(!index || !index->enabled || map->count<16 || (xlsImportNamesScope && !xlsImportNamesScope->bytes)) return NULL;
    if(map->count>XLS_IMPORT_NAME_COUNT || index->indexed>map->count) {xlsImportNamesDisable(index);return NULL;}
    /* Original maps are append-only until Free. Store stable array indices,
     * never pointers into the reallocating original items array. */
    for(size_t i=index->indexed;i<map->count;++i) {
        const char* name=map->items[i].name;
        if(!name) {xlsImportNamesDisable(index);return NULL;}
        xbytesview key={(const unsigned char*)name,strlen(name)};
        if(xrtMapHas(&index->rows,key)) continue; /* first insertion wins */
        size_t fixed=index->rows.KeyOffset;
        if(fixed>=index->limit-index->bytes || key.Size>=index->limit-index->bytes-fixed || !xrtMapSet(&index->rows,key,&i)) {xlsImportNamesDisable(index);return NULL;}
        index->bytes+=fixed+key.Size+1;
        if(xlsImportNamesScope) {++xlsImportNamesScope->work.indexedRows;if(index->bytes>xlsImportNamesScope->work.maxBytes) xlsImportNamesScope->work.maxBytes=index->bytes;}
    }
    index->indexed=map->count;return index;
}
static const char* xl6ImportsNameMapFind(const XL6_ImportNameMap* map,const char* name)
{
    if(!map || !name) return NULL;
    xls_import_names_index* index=xlsImportNamesReady(map);
    if(!index) {if(xlsImportNamesScope) ++xlsImportNamesScope->work.fallbacks;return xlsImportsNameMapFindOriginal(map,name);}
    if(xlsImportNamesScope) ++xlsImportNamesScope->work.lookups;
    const size_t* row=xrtMapConstGet(&index->rows,(xbytesview){(const unsigned char*)name,strlen(name)});
    return row ? map->items[*row].internalName : NULL;
}
static char* xl6ImportsNameMapRewriteDotted(const XL6_ImportNameMap* map,const char* name)
{
    if(!map || !name) return NULL;
    /* No dotted prefix can match without an input dot. The explicit zero
     * budget override keeps the exact original path as an independent oracle. */
    if((!xlsImportNamesScope || xlsImportNamesScope->bytes) && !strchr(name,'.')) {
        if(xlsImportNamesScope) ++xlsImportNamesScope->work.noDotSkips;
        return NULL;
    }
    xls_import_names_index* index=xlsImportNamesReady(map);
    if(!index) {if(xlsImportNamesScope) ++xlsImportNamesScope->work.fallbacks;return xlsImportsNameMapRewriteDottedOriginal(map,name);}
    size_t best=SIZE_MAX,length=0;
    for(const char* dot=name;*dot;++dot) if(*dot=='.') {
        if(xlsImportNamesScope) ++xlsImportNamesScope->work.lookups;
        const size_t* row=xrtMapConstGet(&index->rows,(xbytesview){(const unsigned char*)name,(size_t)(dot-name)});
        if(row && *row<best) {best=*row;length=(size_t)(dot-name);}
    }
    /* Preserve the first map entry whose complete key is followed by a dot,
     * even when a later/longer prefix also matches. */
    return best==SIZE_MAX ? NULL : (char*)xrtFormat("%s%s",map->items[best].internalName,name+length);
}
