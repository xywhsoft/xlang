/* Included only in the qualification TU after the real semantic adapter. */
#include "analysis_ast_same.h"
static unsigned probeChecks;
static int probeDupEnabled;
static size_t probeDupCalls,probeDupFail;
char* __real_xrtStrDupN(const char*,size_t);
char* __wrap_xrtStrDupN(const char* value,size_t size)
{if(probeDupEnabled && ++probeDupCalls==probeDupFail)return NULL;return __real_xrtStrDupN(value,size);}
xls_entity_view* xlsProbeEntityBinding(uint32_t,uint64_t);
void xlsProbeEntityBindingDrop(xls_entity_view*);
#define PROBE(v) do{++probeChecks;if(!(v)){fprintf(stderr,"analysis_sem_metadata_probe:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int probeKeys(void)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    XL6_AstNode* root=xl6AstNewText(XL6_AST_TYPE,"Root",4,NULL);PROBE(root);
    xl6AstAddChild(root,xl6AstNewText(XL6_AST_TYPE,"Child",5,NULL));PROBE(root->childCount==1);
    root->inferredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);root->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"string",6,NULL);PROBE(root->inferredType && root->semanticDeclaredType);
    XL6_AstNode* nodes[]={root,root->children[0],root->inferredType,root->semanticDeclaredType};
    for(unsigned mode=0;mode<4;++mode){
        if(mode==XLS_IR_KEY_FUNCTION)continue;
        xstrbuf plain={0},packed={0};PROBE(xlsSemKeyNode(&plain,root,0,0));PROBE(xlsFrontendIrAstKey(&packed,root,(xls_ir_key_mode)mode));
        for(size_t i=0;i<4;++i){
#define PROBE_FIELD(field,value) do{ xstrbuf a={0},b={0};nodes[i]->field=(value);PROBE(xlsSemKeyNode(&a,root,0,0) && !xlsSemEqualKey(&plain,&a));PROBE(xlsFrontendIrAstKey(&b,root,(xls_ir_key_mode)mode) && !xlsSemEqualKey(&packed,&b));xrtStrBufFree(&a);xrtStrBufFree(&b);nodes[i]->field=0;}while(0)
            PROBE_FIELD(semanticReceiverSymbolId,104);PROBE_FIELD(compiledPackageError,2);PROBE_FIELD(compiledPackageRoot,1);
#undef PROBE_FIELD
            xstrbuf a={0},b={0};nodes[i]->compiledPackageLink=xrtStrDup("");PROBE(nodes[i]->compiledPackageLink);
            PROBE(xlsSemKeyNode(&a,root,0,0) && !xlsSemEqualKey(&plain,&a));PROBE(xlsFrontendIrAstKey(&b,root,(xls_ir_key_mode)mode) && !xlsSemEqualKey(&packed,&b));
            xrtStrBufFree(&a);xrtStrBufFree(&b);xrtFree(nodes[i]->compiledPackageLink);nodes[i]->compiledPackageLink=NULL;
        }
        xrtStrBufFree(&plain);xrtStrBufFree(&packed);
    }
    xl6AstFree(root);
#endif
    return 1;
}
static void probeRenameLocal(XL6_AstNode* n,XL6_SymbolId before,XL6_SymbolId after)
{
    XL6_SymbolId* fields[]={&n->semanticSymbolId,&n->semanticDeclaredSymbolId,&n->semanticTargetSymbolId,&n->semanticCompoundOperatorSymbolId};
    for(size_t i=0;i<4;++i)if(*fields[i]==before)*fields[i]=after;
    for(size_t i=0;i<n->childCount;++i)probeRenameLocal(n->children[i],before,after);
}
static int probeReceiverDomains(void)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    char* path=xrtPathAbs("semantic-receiver-domain.xl");PROBE(path);
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);PROBE(xl6ParseSource("func key(int value) as int\n int local = value\n return local\nend\n",path,&parsed));
    PROBE(xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"domain",1));
    XL6_AstNode* a=parsed.root->children[0];XL6_AstNode* b=xlsAstClone(a,NULL,NULL);PROBE(b);
    xls_lexical_symbols sa={0},sb={0};PROBE(xlsLexicalSymbolsBuild(&sa,a,XLS_LEXICAL_SYMBOL_BYTES) && sa.order.Count);
    XL6_SymbolId id=((const xls_lexical_symbol*)xrtArrayConstGet(&sa.order,0))->symbol;PROBE(id>1 && id<1000);
    probeRenameLocal(b,id,id+100);PROBE(xlsLexicalSymbolsBuild(&sb,b,XLS_LEXICAL_SYMBOL_BYTES));
    a->semanticReceiverSymbolId=id;b->semanticReceiverSymbolId=id+100;xstrbuf ka={0},kb={0},different={0};
    PROBE(xlsFrontendIrAstKeySymbols(&ka,a,&sa) && xlsFrontendIrAstKeySymbols(&kb,b,&sb) && xlsSemEqualKey(&ka,&kb));
    a->semanticReceiverSymbolId=1;PROBE(xlsFrontendIrAstKeySymbols(&different,a,&sa) && !xlsSemEqualKey(&ka,&different));
    xrtStrBufFree(&ka);xrtStrBufFree(&kb);xrtStrBufFree(&different);
    a->semanticReceiverSymbolId=id;b->semanticReceiverSymbolId=id+100;
    xl6AstFree(a->inferredType);xl6AstFree(b->inferredType);a->inferredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);b->inferredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);PROBE(a->inferredType && b->inferredType);
    a->inferredType->semanticReceiverSymbolId=id;b->inferredType->semanticReceiverSymbolId=id+100;
    PROBE(xlsFrontendIrAstKeySymbols(&ka,a,&sa) && xlsFrontendIrAstKeySymbols(&kb,b,&sb) && !xlsSemEqualKey(&ka,&kb));
    xrtStrBufFree(&ka);xrtStrBufFree(&kb);xl6AstFree(a->inferredType);xl6AstFree(b->inferredType);a->inferredType=b->inferredType=NULL;
    xls_entity_view *ea=xlsProbeEntityBinding(900,7001),*eb=xlsProbeEntityBinding(901,7001),*other=xlsProbeEntityBinding(901,7002);PROBE(ea && eb && other);
    a->semanticReceiverSymbolId=900;b->semanticReceiverSymbolId=901;
    PROBE(xlsFrontendIrAstKeyEntities(&ka,a,&sa,ea) && xlsFrontendIrAstKeyEntities(&kb,b,&sb,eb) && xlsSemEqualKey(&ka,&kb));
    PROBE(xlsFrontendIrAstKeyEntities(&different,b,&sb,other) && !xlsSemEqualKey(&ka,&different));xrtStrBufFree(&different);
    a->semanticReceiverSymbolId=7001;PROBE(xlsFrontendIrAstKeyEntities(&different,a,&sa,ea) && !xlsSemEqualKey(&kb,&different));
    xrtStrBufFree(&ka);xrtStrBufFree(&kb);xrtStrBufFree(&different);xlsProbeEntityBindingDrop(ea);xlsProbeEntityBindingDrop(eb);xlsProbeEntityBindingDrop(other);
    xlsLexicalSymbolsClear(&sa);xlsLexicalSymbolsClear(&sb);xl6AstFree(b);xl6ParseResultFree(&parsed);xrtFree(path);
#endif
    return 1;
}
static XL6_AstNode* probeTree(void)
{
    XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);if(!root)return NULL;
    for(size_t i=0;i<3;++i){XL6_AstNode* n=xl6AstNewText(XL6_AST_NAME,"value",5,NULL);if(!n){xl6AstFree(root);return NULL;}xl6AstAddChild(root,n);}
    root->inferredType=xl6AstNewText(XL6_AST_TYPE,"int",3,NULL);root->semanticDeclaredType=xl6AstNewText(XL6_AST_TYPE,"string",6,NULL);
    if(!root->inferredType || !root->semanticDeclaredType){xl6AstFree(root);return NULL;}
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    root->inferredType->semanticReceiverSymbolId=105;root->inferredType->compiledPackageLink=xrtStrDup("pkg:type");
    root->semanticDeclaredType->compiledPackageLink=xrtStrDup("pkg:declared");
    XL6_AstNode* nodes[]={root,root->children[0],root->children[1],root->children[2]};
    for(size_t i=0;i<4;++i){nodes[i]->semanticReceiverSymbolId=104;nodes[i]->compiledPackageError=(unsigned)i+1;nodes[i]->compiledPackageRoot=(int)i;nodes[i]->compiledPackageLink=i ? xrtStrDup(i==1 ? "" : "pkg:包") : NULL;}
#endif
    return root;
}
static uint64_t probeLive(void)
{xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static int probePackageFaults(void)
{
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    XL6_AstNode* root=probeTree();PROBE(root);xarray raw={0},facts={0};
    PROBE(xrtArrayInit(&raw,sizeof(xls_sem_raw)) && xlsSemRawNodes(root,&raw,0));
    PROBE(xrtArrayInit(&facts,sizeof(xls_sem_fact)) && xrtArrayResize(&facts,raw.Count));memset(facts.Data,0,facts.Count*sizeof(xls_sem_fact));
    for(unsigned mode=0;mode<2;++mode){
        for(size_t fail=0;fail<=3;++fail){
            if(mode)for(size_t i=0;i<facts.Count;++i)((xls_sem_fact*)xrtArrayGet(&facts,i))->packageLink=((xls_sem_raw*)xrtArrayGet(&raw,i))->node->compiledPackageLink;
            uint64_t before=probeLive();size_t bytes=0;probeDupCalls=0;probeDupFail=fail;probeDupEnabled=1;
            int okay=mode ? xlsSemPackageFactsCopy(&facts,4096,&bytes) : xlsSemPackageFactsCapture(&raw,&facts,4096,&bytes);
            probeDupEnabled=0;
            if(!fail){PROBE(okay && probeDupCalls==3 && bytes>0);xlsSemPackageFactsFree(&facts);}
            else{PROBE(!okay && !bytes && probeDupCalls==fail);for(size_t i=0;i<facts.Count;++i)PROBE(!((xls_sem_fact*)xrtArrayGet(&facts,i))->packageLink);}
            PROBE(probeLive()==before);
            /* An unfailed retry must still own independent copies after the
             * source survives partial construction/borrowed staging failure. */
            if(mode)for(size_t i=0;i<facts.Count;++i)((xls_sem_fact*)xrtArrayGet(&facts,i))->packageLink=((xls_sem_raw*)xrtArrayGet(&raw,i))->node->compiledPackageLink;
            PROBE(mode ? xlsSemPackageFactsCopy(&facts,4096,&bytes) : xlsSemPackageFactsCapture(&raw,&facts,4096,&bytes));
            xlsSemPackageFactsFree(&facts);PROBE(probeLive()==before);
        }
    }
    xrtArrayUnit(&facts);xrtArrayUnit(&raw);xl6AstFree(root);
#endif
    return 1;
}
static int probeFacts(int shared)
{
    XL6_AstNode* root=probeTree();PROBE(root && root->inferredType && root->semanticDeclaredType);
    xarray raw={0};PROBE(xrtArrayInit(&raw,sizeof(xls_sem_raw)) && xlsSemRawNodes(root,&raw,0));
    xls_sem_unit* unit=xrtCalloc(1,sizeof(*unit));PROBE(unit);xrtAtomic32Init(&unit->references,1);unit->symbols=(xls_sem_symbol_span){100,120,100,1};
    size_t bytes=0;PROBE(xlsSemCaptureFacts(&raw,unit,XLS_SEM_MEMO_BYTES,&bytes,shared));unit->bytes=bytes+sizeof(*unit);
    XL6_AstNode* target=xlsAstClone(root,NULL,NULL);PROBE(target);xl6AstFree(root);xrtArrayUnit(&raw);
    PROBE(xrtArrayInit(&raw,sizeof(xls_sem_raw)) && xlsSemRawNodes(target,&raw,0));
    xls_sem_entry entry={.unit=unit};xls_sem_moves moves={0};xmap sources={0};xarray positions={0};xls_sem_cursor cursor={0};
    PROBE(xrtMapInit(&moves.paths,sizeof(xls_sem_move)) && xrtMapInit(&sources,sizeof(XL6_AstNode*)));
    PROBE(xlsSemApplyFacts(&entry,&raw,&moves,&sources,&positions,target,&cursor,200,1));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    PROBE(target->semanticReceiverSymbolId==204 && target->inferredType->semanticReceiverSymbolId==205);
    for(size_t i=0;i<raw.Count;++i){XL6_AstNode* n=((xls_sem_raw*)xrtArrayGet(&raw,i))->node;const xls_sem_fact* f=xrtArrayConstGet(&unit->facts,i);PROBE(n->compiledPackageError==f->packageError && n->compiledPackageRoot==f->packageRoot);PROBE(!f->packageLink ? !n->compiledPackageLink : n->compiledPackageLink!=f->packageLink && !strcmp(n->compiledPackageLink,f->packageLink));}
    xls_sem_fact* first=xrtArrayGet(&unit->facts,0);first->receiver=150;
    char* before=target->compiledPackageLink;unsigned receiver=target->semanticReceiverSymbolId;
    xstrbuf beforeKey={0},afterKey={0};PROBE(xlsSemKeyNode(&beforeKey,target,0,0));
    PROBE(!xlsSemApplyFacts(&entry,&raw,&moves,&sources,&positions,target,&cursor,200,1));PROBE(target->compiledPackageLink==before && target->semanticReceiverSymbolId==receiver);
    PROBE(xlsSemKeyNode(&afterKey,target,0,0) && xlsSemEqualKey(&beforeKey,&afterKey));xrtStrBufFree(&beforeKey);xrtStrBufFree(&afterKey);
    first->receiver=104;
    xarray staged={0};PROBE(xrtArrayInit(&staged,sizeof(xls_sem_fact)) && xrtArrayResize(&staged,unit->facts.Count));memcpy(staged.Data,unit->facts.Data,unit->facts.Count*sizeof(xls_sem_fact));
    size_t flat=0;for(size_t i=0;i<unit->facts.Count;++i){const xls_sem_fact* f=xrtArrayConstGet(&unit->facts,i);if(f->packageLink)flat+=strlen(f->packageLink)+1;}
    size_t copied=0;PROBE(!xlsSemCopyFactTypes(&staged,&(xls_ast_clone_options){.bytes=flat},&copied));
    for(size_t i=0;i<staged.Count;++i){const xls_sem_fact* f=xrtArrayConstGet(&staged,i);PROBE(!f->inferred && !f->declaredType && !f->packageLink);}
    xlsSemFactsFree(&staged);
    unit->proof=xlsSemProofCreate(unit,4096);PROBE(unit->proof && unit->proof->shiftSafe);
    xlsSemProofDestroy(unit->proof);unit->proof=NULL;first->receiver=150;unit->proof=xlsSemProofCreate(unit,4096);PROBE(unit->proof && !unit->proof->shiftSafe);first->receiver=104;
#endif
    xrtArrayUnit(&positions);xrtMapUnit(&moves.paths);xrtMapUnit(&sources);xrtArrayUnit(&raw);xl6AstFree(target);xlsSemUnitRelease(unit);return 1;
}
static int probeMemo(void)
{
    char* path=xrtPathAbs("semantic-unit-metadata.xl");PROBE(path);xstrbuf first={0},second={0};
    for(unsigned f=0;f<2;++f){PROBE(xrtStrBufAppendFormat(&first,"func f_%u() as int\n",f));PROBE(xrtStrBufAppendFormat(&second,"func f_%u() as int\n",f));
        for(unsigned i=0;i<20;++i){PROBE(xrtStrBufAppendFormat(&first," int value_%u = %u\n",i,i));PROBE(xrtStrBufAppendFormat(&second," int value_%u = %u\n",i,i+(!f && i==5)));}
        PROBE(xrtStrBufAppend(&first,xrtStrView(" return value_19\nend\n")) && xrtStrBufAppend(&second,xrtStrView(" return value_19\nend\n")));
    }
    XL6_ParseResult a,b,fresh;xl6ParseResultInit(&a);xl6ParseResultInit(&b);xl6ParseResultInit(&fresh);
    PROBE(xl6ParseSource(first.Data,path,&a) && xl6ParseSource(second.Data,path,&b) && xl6ParseSource(second.Data,path,&fresh));
    XL6_SourceReportItem itemA={.path=path,.text=first.Data,.size=first.Size,.kind=XL6_SOURCE_REPORT_SOURCE},itemB={.path=path,.text=second.Data,.size=second.Size,.kind=XL6_SOURCE_REPORT_SOURCE};
    XL6_SourceReport ra={.items=&itemA,.count=1,.complete=1},rb={.items=&itemB,.count=1,.complete=1};
    xls_sem_memo *old=NULL,*next=NULL;xls_sem_work wa={0},wb={0};
    PROBE(xlsFrontendSemCheckMemo(a.root,&a.diags,"probe",1,path,&ra,NULL,NULL,&old,&wa));
    PROBE(xlsFrontendSemCheckMemo(b.root,&b.diags,"probe",1,path,&rb,&ra,old,&next,&wb));
    PROBE(old && next && wb.checked>=1 && wb.reused>=1);
    PROBE(xl6SemCheckModuleWithIndex(fresh.root,&fresh.diags,"probe",1) && sameAst(b.root,fresh.root));
    xlsFrontendSemMemoDestroy(old);xlsFrontendSemMemoDestroy(next);xl6ParseResultFree(&a);xl6ParseResultFree(&b);xl6ParseResultFree(&fresh);
    xrtStrBufFree(&first);xrtStrBufFree(&second);xrtFree(path);return 1;
}
int main(void)
{
    xrtMemStatsEnable(true);
    if(!probeKeys() || !probeReceiverDomains() || !probePackageFaults() || !probeFacts(0) || !probeFacts(1) || !probeMemo())return 1;
    printf("analysis_sem_metadata_probe passed: %u checks; actual keys/facts/rebinding and ordinary memo/fresh comparison\n",probeChecks);return 0;
}
