/* Negative spelling filter around unchanged shared stdlib predicates. The
 * build audits that every positive case requires an exact AST text match. */
#include "analysis/frontend_stdlib.h"
#define XLS_STDLIB_INDEX_BYTES (8u*1024u*1024u)
#define XLS_STDLIB_NAME_BYTES 256u
typedef struct xls_stdlib_scan {
    struct xls_stdlib_scan* previous;
    const XL6_AstNode* root;
    xmap names;
    xls_stdlib_work work;
    size_t limit,scopeBytes;
    int enabled;
} xls_stdlib_scan;
static _Thread_local xls_stdlib_scan* xlsActiveStdlibScan;
static int xlsStdlibTypeOriginal(const XL6_AstNode*,const char*);
static int xlsStdlibMemberOriginal(const XL6_AstNode*,const char*);
static int xlsStdlibExpandOriginal(XL6_AstNode*,XL6_DiagList*);
static size_t xlsStdlibSpellingSize(const char* text)
{
    if(!text) return 0;
    size_t size=0; while(size<=XLS_STDLIB_NAME_BYTES && text[size]) ++size;
    return size;
}
static int xlsStdlibCollect(const XL6_AstNode* node,xls_stdlib_scan* scan,size_t depth)
{
    if(!node || depth>512) return 0;
    ++scan->work.nodes;
    size_t size=xlsStdlibSpellingSize(node->text);
    if(size && size<=XLS_STDLIB_NAME_BYTES) {
        xbytesview key={(const unsigned char*)node->text,size};
        if(!xrtMapHas(&scan->names,key)) {
            /* Include conservative per-entry overhead. The bound describes
             * this optional index, not the process's peak memory. */
            size_t charge=size+1+96;
            if(scan->work.names>=65536u || charge>scan->limit-scan->work.bytes) return 0;
            unsigned char present=1; if(!xrtMapSet(&scan->names,key,&present)) return 0;
            scan->work.bytes+=charge; ++scan->work.names;
        }
    }
    for(size_t i=0;i<node->childCount;++i) if(!xlsStdlibCollect(node->children[i],scan,depth+1)) return 0;
    return 1;
}
static void xlsStdlibScanBegin(const XL6_AstNode* root)
{
    xls_stdlib_scan* scan=xlsActiveStdlibScan;
    if(!scan || scan->root!=root || !scan->limit) return;
    scan->enabled=xrtMapInit(&scan->names,sizeof(unsigned char)) && xlsStdlibCollect(root,scan,0);
}
static void xlsStdlibScanEnd(void)
{ if(xlsActiveStdlibScan) xlsActiveStdlibScan->enabled=0; }
static int xlsStdlibAbsent(const XL6_AstNode* node,const char* name)
{
    xls_stdlib_scan* scan=xlsActiveStdlibScan;
    if(!scan) return 0;
    if(node==scan->root) {
        ++scan->work.queries;
        size_t size=xlsStdlibSpellingSize(name);
        if(scan->enabled && size && size<=XLS_STDLIB_NAME_BYTES &&
           !xrtMapHas(&scan->names,(xbytesview){(const unsigned char*)name,size})) {
            ++scan->work.pruned; return 1;
        }
    }
    if(node) ++scan->work.predicateVisits;
    return 0;
}
static int xl6StdlibNodeUsesTypeName(const XL6_AstNode* node,const char* name)
{ return xlsStdlibAbsent(node,name) ? 0 : xlsStdlibTypeOriginal(node,name); }
static int xl6StdlibNodeUsesMemberName(const XL6_AstNode* node,const char* name)
{ return xlsStdlibAbsent(node,name) ? 0 : xlsStdlibMemberOriginal(node,name); }
int xlsFrontendStdlibExpand(XL6_AstNode* root,XL6_DiagList* diags,size_t bytes,xls_stdlib_work* work)
{
    xls_stdlib_scan scan={.previous=xlsActiveStdlibScan,.root=root,.limit=bytes>XLS_STDLIB_INDEX_BYTES ? XLS_STDLIB_INDEX_BYTES : bytes};
    xlsActiveStdlibScan=&scan;
    int okay=xlsStdlibExpandOriginal(root,diags);
    xlsActiveStdlibScan=scan.previous;
    scan.work.scopeLiveBytes=scan.scopeBytes;
    xrtMapUnit(&scan.names); if(work) *work=scan.work;
    return okay;
}
int xlsFrontendStdlibExpandReference(XL6_AstNode* root,XL6_DiagList* diags,xls_stdlib_work* work)
{ return xlsFrontendStdlibExpand(root,diags,0,work); }
int xl6StdlibExpandModule(XL6_AstNode* root,XL6_DiagList* diags)
{ return xlsFrontendStdlibExpand(root,diags,XLS_STDLIB_INDEX_BYTES,NULL); }
