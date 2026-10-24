/* Transient index around the ORIGINAL source-module preparation. The audited
 * setter seam supplies the root/ordinal, so every observed rename is O(1).
 * No language declaration predicate or rewriting rule is duplicated here. */
typedef struct xls_import_decl_index {
    struct xls_import_decl_index* previous;
    const XL6_AstNode* root;
    xmap names;
    size_t bytes,limit;
    int enabled;
} xls_import_decl_index;
static _Thread_local xls_import_decl_index* xlsImportDeclIndex;
static int xlsImportsPrepareSourceModuleOriginal(XL6_AstNode*,const char*,XL6_ImportSymbolTable*,XL6_DiagList*);
static xbytesview xlsImportDeclKey(const char* name) {return (xbytesview){(const unsigned char*)name,strlen(name)};}
static void xlsImportDeclDisable(xls_import_decl_index* index)
{xrtMapUnit(&index->names);index->enabled=0;}
static xls_import_decl_index* xlsImportDeclBound(const XL6_AstNode* root)
{
    for(xls_import_decl_index* index=xlsImportDeclIndex;index;index=index->previous) if(index->root==root) return index;
    return NULL;
}
static int xlsImportDeclAdd(xls_import_decl_index* index,const char* name)
{
    if(!name || !*name) return 1;
    xbytesview key=xlsImportDeclKey(name);size_t* count=xrtMapGet(&index->names,key);
    if(count) {if(*count==SIZE_MAX) return 0;++*count;return 1;}
    size_t fixed=index->names.KeyOffset,one=1;
    if(xrtMapCount(&index->names)>=XLS_IMPORT_NAME_COUNT || fixed>=index->limit-index->bytes || key.Size>=index->limit-index->bytes-fixed || !xrtMapSet(&index->names,key,&one)) return 0;
    index->bytes+=fixed+key.Size+1;
    if(xlsImportNamesScope && index->bytes>xlsImportNamesScope->work.maxDeclBytes) xlsImportNamesScope->work.maxDeclBytes=index->bytes;
    return 1;
}
static void xlsImportDeclBegin(xls_import_decl_index* index,const XL6_AstNode* root)
{
    *index=(xls_import_decl_index){.previous=xlsImportDeclIndex,.root=root};
    /* Reentering preparation of the same mutable root invalidates outer
     * observations; disabled nested calls must never leave stale tables. */
    for(xls_import_decl_index* outer=index->previous;outer;outer=outer->previous) if(outer->root==root) xlsImportDeclDisable(outer);
    int reentered=xlsImportDeclBound(root)!=NULL;xlsImportDeclIndex=index;
    size_t limit=xlsImportNamesScope ? xlsImportNamesScope->bytes : XLS_IMPORT_NAME_BYTES;
    if(reentered || !root || root->childCount<16 || limit<sizeof(*index)) return;
    if(!xrtMapInit(&index->names,sizeof(size_t))) {xrtMapUnit(&index->names);return;}
    index->enabled=1;index->limit=limit;index->bytes=sizeof(*index);
    for(size_t i=0;i<root->childCount;++i) {
        if(xlsImportNamesScope) ++xlsImportNamesScope->work.declRows;
        if(!xlsImportDeclAdd(index,xl6ImportsDeclName(root->children[i]))) {xlsImportDeclDisable(index);return;}
    }
}
static void xlsImportDeclEnd(xls_import_decl_index* index)
{xlsImportDeclIndex=index->previous;xrtMapUnit(&index->names);index->enabled=0;}
static int xl6ImportsModuleHasDeclName(const XL6_AstNode* root,const char* name)
{
    xls_import_decl_index* index=xlsImportDeclBound(root);
    if(index && index->enabled && (!xlsImportNamesScope || xlsImportNamesScope->bytes)) {
        if(xlsImportNamesScope) ++xlsImportNamesScope->work.declLookups;
        return name && *name && xrtMapHas(&index->names,xlsImportDeclKey(name));
    }
    if(xlsImportNamesScope) ++xlsImportNamesScope->work.declFallbacks;
    return xlsImportsModuleHasDeclNameOriginal(root,name);
}
static int xl6ImportsModuleHasNaturalDeclName(const XL6_AstNode* root,const char* name)
{
    xls_import_decl_index* index=xlsImportDeclBound(root);
    if(index && index->enabled && (!xlsImportNamesScope || xlsImportNamesScope->bytes)) {
        if(xlsImportNamesScope) ++xlsImportNamesScope->work.declLookups;
        return name && *name && !xl6ImportsIsInternalImportName(name) && xrtMapHas(&index->names,xlsImportDeclKey(name));
    }
    if(xlsImportNamesScope) ++xlsImportNamesScope->work.declFallbacks;
    return xlsImportsModuleHasNaturalDeclNameOriginal(root,name);
}
static void xlsImportsSetIndexedDeclName(XL6_AstNode* root,size_t ordinal,XL6_AstNode* node,const char* name)
{
    xls_import_decl_index* index=xlsImportDeclBound(root);
    if(index && index->enabled) {
        if(ordinal>=root->childCount || root->children[ordinal]!=node) xlsImportDeclDisable(index);
        else {
            const char* old=xl6ImportsDeclName(node);
            if(old && *old) {
                xbytesview key=xlsImportDeclKey(old);size_t* count=xrtMapGet(&index->names,key);
                if(!count || !*count) xlsImportDeclDisable(index);
                else if(*count>1) --*count;
                else {
                    size_t bytes=index->names.KeyOffset+key.Size+1;
                    if(xrtMapRemove(&index->names,key)) index->bytes-=bytes;else xlsImportDeclDisable(index);
                }
            }
        }
    }
    /* The original setter still owns every AST string/allocation. Observer
     * failure never rolls back or changes the original compiler operation. */
    xl6ImportsSetDeclName(node,name);
    if(index && index->enabled) {
        if(xlsImportNamesScope) ++xlsImportNamesScope->work.declRenames;
        if(!xlsImportDeclAdd(index,xl6ImportsDeclName(node))) xlsImportDeclDisable(index);
    }
}
static int xl6ImportsPrepareSourceModule(XL6_AstNode* root,const char* ns,XL6_ImportSymbolTable* symbols,XL6_DiagList* diags)
{
    if(!root || !symbols || !ns || !*ns) return xlsImportsPrepareSourceModuleOriginal(root,ns,symbols,diags);
    xls_import_decl_index index;xlsImportDeclBegin(&index,root);
    int okay=xlsImportsPrepareSourceModuleOriginal(root,ns,symbols,diags);
    xlsImportDeclEnd(&index);return okay;
}
