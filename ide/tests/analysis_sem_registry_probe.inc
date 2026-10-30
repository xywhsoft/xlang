/* Whole current declaration/registry data,with the real current checker. */
#define XLS_SEM_QUERIES_MAIN xlsNativeQueriesProbeMain
#include "analysis_sem_queries_probe.inc"
#include "src/stdlib/xl6_stdlib.h"
#include "src/stdlib/xl6_stdlib_data.h"
static const XL6_StdlibFunc* registryFunction(const char* space,const char* name)
{
    for(size_t i=0;i<xl6StdlibFuncCount;++i)if(!strcmp(xl6StdlibFuncs[i].ns,space) && !strcmp(xl6StdlibFuncs[i].name,name))return &xl6StdlibFuncs[i];
    return NULL;
}
static int registryDeclarations(void)
{
    PROBE(xl6StdlibFuncCount==1070 && XL6_STDLIB_FUNC_COUNT==1070 && xl6StdlibConstCount==202);
    PROBE(!registryFunction("__xrt_path","sep") && !registryFunction("__xrt_path","normalize") && !registryFunction("__xl_runtime","osPlatform"));
    const XL6_StdlibFunc* args=registryFunction("__xl_runtime","osArgs");PROBE(args && sameText(args->cSymbol,"xl6_xrt_os_args") && sameText(args->returnType,"var"));
    const char* names[]={"text","bytes","buffer"},*internal[]={"__xl_std_crc32_text","__xl_std_crc32_bytes","__xl_std_crc32_buffer"};
    for(unsigned i=0;i<3;++i){const XL6_StdlibFunc* f=registryFunction("__crc32",names[i]);PROBE(f && sameText(f->internalName,internal[i]));}
    unsigned contracts=0;for(size_t i=0;i<xl6StdlibForeignContractCount;++i){const XL6_StdlibForeignContract* c=&xl6StdlibForeignContracts[i];PROBE(strncmp(c->internalName,"__xl_crc32_",11));if(!strncmp(c->internalName,"__xl_std_crc32_",14))++contracts;}
    PROBE(contracts==4);return 1;
}
static int registryDiagnostics(const XL6_DiagList* a,const XL6_DiagList* b)
{
    PROBE(a->count==b->count);
    for(size_t i=0;i<a->count;++i)PROBE(a->items[i].line==b->items[i].line && a->items[i].column==b->items[i].column && sameText(a->items[i].message,b->items[i].message));
    return 1;
}
static void registryDifference(const XL6_AstNode* a,const XL6_AstNode* b,unsigned depth)
{
    if(depth>512 || sameAst(a,b))return;
    if(!a || !b){fprintf(stderr,"registry AST missing at depth %u\n",depth);return;}
    XL6_AstNode x=*a,y=*b;x.children=y.children=NULL;x.childCount=y.childCount=0;x.inferredType=y.inferredType=x.semanticDeclaredType=y.semanticDeclaredType=NULL;
    if(!sameAst(&x,&y)){fprintf(stderr,"registry AST depth=%u kind=%u/%u text=%s/%s source=%s/%s range=%d/%d symbols=%u/%u types=%u/%u\n",depth,(unsigned)a->kind,(unsigned)b->kind,a->text?a->text:"",b->text?b->text:"",xl6AstSourcePath(a)?xl6AstSourcePath(a):"<none>",xl6AstSourcePath(b)?xl6AstSourcePath(b):"<none>",a->range.known,b->range.known,a->semanticSymbolId,b->semanticSymbolId,a->semanticTypeId,b->semanticTypeId);return;}
    if(!sameAst(a->inferredType,b->inferredType)){registryDifference(a->inferredType,b->inferredType,depth+1);return;}
    if(!sameAst(a->semanticDeclaredType,b->semanticDeclaredType)){registryDifference(a->semanticDeclaredType,b->semanticDeclaredType,depth+1);return;}
    for(size_t i=0;i<a->childCount && i<b->childCount;++i)if(!sameAst(a->children[i],b->children[i])){registryDifference(a->children[i],b->children[i],depth+1);return;}
}
static unsigned registryOriginExtensions;
static const XL6_AstNode* registryGeneric(const XL6_AstNode* node)
{
    if(!node)return NULL;
    if(node->kind==XL6_AST_GENERIC_INSTANT && node->childCount==2 && sameText(node->children[0]->text,"dependency"))return node;
    for(size_t i=0;i<node->childCount;++i){const XL6_AstNode* found=registryGeneric(node->children[i]);if(found)return found;}
    return NULL;
}
static int registryRecordOrigins(const XL6_AstNode* input,xarray* origins)
{
    if(!input)return 1;
    XL6_AstNode* origin=xl6AstNew(XL6_AST_LIST,NULL);PROBE(origin);
    origin->line=input->line;origin->column=input->column;xl6AstCopySource(origin,input);
    if(!xrtArrayPush(origins,&origin)){xl6AstFree(origin);return 0;}
    for(size_t i=0;i<input->childCount;++i)PROBE(registryRecordOrigins(input->children[i],origins));
    PROBE(registryRecordOrigins(input->inferredType,origins) && registryRecordOrigins(input->semanticDeclaredType,origins));
    return 1;
}
static int registryOrigin(const xarray* origins,const XL6_AstNode* value)
{
    for(size_t i=0;i<origins->Count;++i){const XL6_AstNode* p=*(XL6_AstNode* const*)xrtArrayConstGet(origins,i);
        if(p->source==value->source && p->line==value->line && p->column==value->column && sameRange(p->range,value->range) && sameRange(p->tokenRange,value->tokenRange))return 1;
    }
    return 0;
}
static int registryOriginalParity(const XL6_AstNode* extended,const XL6_AstNode* raw,const xarray* pristine)
{
    if(!extended || !raw){PROBE(extended==raw);return 1;}
    PROBE(extended->childCount==raw->childCount);XL6_AstNode x=*extended,y=*raw;
    x.children=y.children=NULL;x.childCount=y.childCount=0;x.inferredType=y.inferredType=x.semanticDeclaredType=y.semanticDeclaredType=NULL;
    if(extended->kind==XL6_AST_TYPE && !raw->source &&
       (extended->source || !sameRange(extended->range,raw->range) || !sameRange(extended->tokenRange,raw->tokenRange))){
        /* Only the documented synthetic type origin extension qualifies.
         * Its exact source object and both ranges must come from syntax
         * saved before checking;semantic IDs,text,flags,line/column remain
         * untouched in this comparison. No coordinate guess establishes it. */
        PROBE(sameRange(raw->range,(XL6_AstRange){0}) && sameRange(raw->tokenRange,(XL6_AstRange){0}) && registryOrigin(pristine,extended));
        y.source=x.source;y.range=x.range;y.tokenRange=x.tokenRange;++registryOriginExtensions;
    }
    if(!sameAst(&x,&y))registryDifference(extended,raw,0);
    PROBE(sameAst(&x,&y));
    PROBE(registryOriginalParity(extended->inferredType,raw->inferredType,pristine) && registryOriginalParity(extended->semanticDeclaredType,raw->semanticDeclaredType,pristine));
    for(size_t i=0;i<extended->childCount;++i)PROBE(registryOriginalParity(extended->children[i],raw->children[i],pristine));
    return 1;
}
static int registryVersions(unsigned kind)
{
    char* path=xrtPathAbs("current-registry-中文.xl");PROBE(path);xstrbuf texts[5]={{0}};XL6_SourceReportItem items[5]={{0}};XL6_SourceReport reports[5]={{0}};xls_sem_memo* memos[5]={0};
    for(unsigned v=0;v<5;++v){
        for(unsigned f=0;f<2;++f){PROBE(xrtStrBufAppendFormat(&texts[v],"func stable_%u() as int\n",f));for(unsigned n=0;n<24;++n)PROBE(xrtStrBufAppendFormat(&texts[v]," int value_%u=%u\n",n,n));PROBE(xrtStrBufAppend(&texts[v],xrtStrView(" return value_23\nend\n")));}
        if(kind==0){
            PROBE(xrtStrBufAppendFormat(&texts[v],"typedef Callback=func(int) as int\nfunc dependency<T>(T value,int extra%s) as T\n return value\nend\nfunc edited() as int\n Callback callback=dependency::<int>\n return callback(7)\nend\n",v==2 ? "" : v ? "=8" : "=7"));
        }else if(kind==1){
            PROBE(xrtStrBufAppendFormat(&texts[v],"class Resource\n %spoint handle\nend\nfunc edited() as int\n Resource value\n return 0\nend\n",v==1 || v==2 ? "native " : ""));
        }else{
            PROBE(xrtStrBufAppendFormat(&texts[v],"func edited() as uint32\n return crc32.checksum(\"%s\")\nend\n",v ? "12345678" : "123456789"));
        }
        XL6_ParseResult a,b,c;xl6ParseResultInit(&a);xl6ParseResultInit(&b);xl6ParseResultInit(&c);PROBE(xl6ParseSource(texts[v].Data,path,&a) && xl6ParseSource(texts[v].Data,path,&b) && xl6ParseSource(texts[v].Data,path,&c));
        PROBE(xl6StdlibExpandModule(a.root,&a.diags) && xl6StdlibExpandModule(b.root,&b.diags) && xl6StdlibExpandModule(c.root,&c.diags));
        xarray pristine={0};PROBE(xrtArrayInit(&pristine,sizeof(XL6_AstNode*)) && registryRecordOrigins(b.root,&pristine));
        items[v]=(XL6_SourceReportItem){.path=path,.text=texts[v].Data,.size=texts[v].Size,.kind=XL6_SOURCE_REPORT_SOURCE};reports[v]=(XL6_SourceReport){.items=&items[v],.count=1,.complete=1};xls_sem_work work={0};
        int cached=xlsFrontendSemCheckMemo(a.root,&a.diags,"registry",1,path,&reports[v],v ? &reports[v-1] : NULL,v ? memos[v-1] : NULL,&memos[v],&work);
        int complete=xl6SemCheckModuleWithQuery(b.root,&b.diags,"registry",1,NULL);
        int original=xlsReferenceSemCheckModuleWithQuery(c.root,&c.diags,"registry",1,NULL);
        if(cached!=original || (!cached && !(kind==0 && v==2) && !(kind==1 && (v==1 || v==2)))){for(size_t i=0;i<a.diags.count;++i)fprintf(stderr,"registry %u/%u: %s\n",kind,v,a.diags.items[i].message);}
        if(!sameAst(a.root,b.root) || !memos[v]){fprintf(stderr,"registry pair %u/%u cached=%d original=%d memo=%p\n",kind,v,cached,original,(void*)memos[v]);registryDifference(a.root,b.root,0);}
        PROBE(cached==complete && cached==original && registryDiagnostics(&a.diags,&b.diags) && registryDiagnostics(&a.diags,&c.diags) && sameAst(a.root,b.root) && memos[v]);
        PROBE(registryOriginalParity(b.root,c.root,&pristine));
        if(kind==0 && v==2){const XL6_AstNode* expr=registryGeneric(b.root);PROBE(expr && expr->inferredType);
            PROBE(expr->source && expr->source==expr->inferredType->source &&
                sameRange(expr->range,expr->inferredType->range) && sameRange(expr->tokenRange,expr->inferredType->tokenRange));}
        PROBE(cached==!((kind==0 && v==2) || (kind==1 && (v==1 || v==2))));
        if(v==4)PROBE(work.reused>=2);
        if(kind==0 && v==1)PROBE(work.reused>=2 && memos[v]->readWork.rejected>=1);
        /* Every old AST is already retired before the next use of its memo. */
        for(size_t i=0;i<pristine.Count;++i)xl6AstFree(*(XL6_AstNode* const*)xrtArrayConstGet(&pristine,i));
        xrtArrayUnit(&pristine);xl6ParseResultFree(&a);xl6ParseResultFree(&b);xl6ParseResultFree(&c);
    }
    for(unsigned v=0;v<5;++v){xlsFrontendSemMemoDestroy(memos[v]);xrtStrBufFree(&texts[v]);}xrtFree(path);return 1;
}
static int registryOriginProofs(void)
{
    PROBE(registryOriginExtensions>0);return 1;
}
int main(void)
{
    if(xlsNativeQueriesProbeMain() || !registryDeclarations())return 1;
    for(unsigned i=0;i<3;++i)if(!registryVersions(i))return 1;
    if(!registryOriginProofs())return 1;
    printf("analysis_sem_registry_probe passed: %u checks; whole current SDK,complete production AST,original semantics/diagnostics,explicit proven origin extensions=%u,generic defaults,native initialization and actual memo reuse\n",probeChecks,registryOriginExtensions);return 0;
}
