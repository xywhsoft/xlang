/* Exercise the actual private adapter,including the preceding metadata suite. */
#define main xlsMetadataProbeMain
#include "analysis_sem_metadata_probe.inc"
#undef main
static unsigned effectReserveKind;
static size_t effectReserveCalls,effectReserveFail;
bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __real_xrtArrayReserve(xarray*,size_t);
bool __wrap_xrtStrBufReserve(xstrbuf* p,size_t n)
{if(effectReserveKind==1 && ++effectReserveCalls==effectReserveFail)return false;return __real_xrtStrBufReserve(p,n);}
bool __wrap_xrtArrayReserve(xarray* p,size_t n)
{if(effectReserveKind==2 && ++effectReserveCalls==effectReserveFail)return false;return __real_xrtArrayReserve(p,n);}
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
static int effectHeaderLinks(void)
{
    XL6_AstNode* root=probeTree();PROBE(root);
    XL6_AstNode* copy=xlsAstClone(root,NULL,NULL);PROBE(copy);
    XL6_AstNode* places[]={copy,copy->children[0],copy->inferredType,copy->semanticDeclaredType};
    for(size_t i=0;i<4;++i){
        PROBE(xlsSemHeaderNodeEqual(root,copy,0,0));
        char* saved=places[i]->compiledPackageLink;
        places[i]->compiledPackageLink=xrtStrDup(saved ? "changed:包" : "");PROBE(places[i]->compiledPackageLink);
        PROBE(!xlsSemHeaderNodeEqual(root,copy,0,0));
        xrtFree(places[i]->compiledPackageLink);places[i]->compiledPackageLink=saved;
    }
    xl6AstFree(copy);xl6AstFree(root);return 1;
}
static int effectBuildSource(xstrbuf* text,int variant,unsigned body,unsigned literal)
{
    int member=variant==1,generic=variant==2;
    PROBE(xrtStrBufAppend(text,xrtStrView("extern \"text\" @no_raise func nativeText() as point\nextern \"free\" @no_raise func nativeFree(point p)\n")));
    if(member)PROBE(xrtStrBufAppend(text,xrtStrView("class Reader\n")));
    if(generic)PROBE(xrtStrBufAppendFormat(text,"func read<T>(T value) as T\n int revision = %u\n return value\nend\n",body));
    else PROBE(xrtStrBufAppendFormat(text,"func read(string value) as int\n return value.length + %u\nend\n",body));
    if(member)PROBE(xrtStrBufAppend(text,xrtStrView("end\n")));
    for(unsigned f=0;f<4;++f){
        PROBE(xrtStrBufAppendFormat(text,"func effect_%u() as int\n",f));
        for(unsigned i=0;i<20;++i)PROBE(xrtStrBufAppendFormat(text," int local_%u = %u\n",i,i+(!f && i==4 ? literal : 0)));
        if(f==1 || f==2){
            PROBE(xrtStrBufAppend(text,xrtStrView(" cstring value = cstring.take(nativeText(),nativeFree)\n")));
            if(member)PROBE(xrtStrBufAppend(text,xrtStrView(" auto reader = Reader()\n return reader.read(value)\n")));
            else if(generic)PROBE(xrtStrBufAppendFormat(text," return read::<string>(value).length + %u\n",body));
            else PROBE(xrtStrBufAppend(text,xrtStrView(" return read(value)\n")));
        }else PROBE(xrtStrBufAppend(text,xrtStrView(" return local_19\n")));
        PROBE(xrtStrBufAppend(text,xrtStrView("end\n")));
    }
    return 1;
}
static size_t effectFindSlot(const XL6_AstNode* root,const char* name)
{for(size_t i=0;i<root->childCount;++i)if(xl6SemIsFuncDecl(root->children[i]) && xl6TextEquals(xl6SemFuncName(root->children[i]),name))return i;return SIZE_MAX;}
static int effectVersions(int variant)
{
    int member=variant==1;
    char* path=xrtPathAbs(member ? "semantic-class-effects.xl" : variant==2 ? "semantic-generic-effects.xl" : "semantic-module-effects.xl");PROBE(path);
    xstrbuf sources[4]={{0}};XL6_ParseResult parsed[4];XL6_SourceReportItem items[4]={{0}};XL6_SourceReport reports[4]={{0}};
    xls_sem_memo* memos[4]={0};xls_sem_work work[4]={{0}};
    for(size_t v=0;v<4;++v){
        PROBE(effectBuildSource(&sources[v],variant,v>=2 ? 2 : 1,v ? (unsigned)v : 0));
        xl6ParseResultInit(&parsed[v]);PROBE(xl6ParseSource(sources[v].Data,path,&parsed[v]));
        items[v]=(XL6_SourceReportItem){.path=path,.text=sources[v].Data,.size=sources[v].Size,.kind=XL6_SOURCE_REPORT_SOURCE};
        reports[v]=(XL6_SourceReport){.items=&items[v],.count=1,.complete=1};
        PROBE(xlsFrontendSemCheckMemo(parsed[v].root,&parsed[v].diags,"effects",1,path,&reports[v],v ? &reports[v-1] : NULL,v ? memos[v-1] : NULL,&memos[v],&work[v]));
        PROBE(memos[v] && memos[v]->effectsComplete && memos[v]->effects.Count==1 && !memos[v]->inputSame);
        PROBE(memos[v]->bytes<=XLS_SEM_MEMO_BYTES);
        size_t changed=effectFindSlot(parsed[v].root,"effect_1"),again=effectFindSlot(parsed[v].root,"effect_2"),after=effectFindSlot(parsed[v].root,"effect_3");
        PROBE(changed!=SIZE_MAX && again!=SIZE_MAX && after!=SIZE_MAX);
        const xls_sem_entry* mutator=xrtArrayConstGet(&memos[v]->entries,changed);
        const xls_sem_entry* tail=xrtArrayConstGet(&memos[v]->entries,after);
        PROBE(mutator && !mutator->unit && tail && tail->unit && tail->effectCount==1);
        PROBE(!((const xls_sem_entry*)xrtArrayConstGet(&memos[v]->entries,again))->unit);
        if(v){
            const xls_sem_entry* old=xrtArrayConstGet(&memos[v-1]->entries,after);
            if(v==2){PROBE(!xlsSemEqualKey(xrtArrayConstGet(&memos[v-1]->effects,0),xrtArrayConstGet(&memos[v]->effects,0)));PROBE(tail->unit!=old->unit);}
            else{PROBE(xlsSemEqualKey(xrtArrayConstGet(&memos[v-1]->effects,0),xrtArrayConstGet(&memos[v]->effects,0)));PROBE(work[v].reused>=1 && tail->unit==old->unit);}
        }
        XL6_ParseResult fresh;xl6ParseResultInit(&fresh);PROBE(xl6ParseSource(sources[v].Data,path,&fresh));
        PROBE(xl6SemCheckModuleWithIndex(fresh.root,&fresh.diags,"effects",1));PROBE(sameAst(parsed[v].root,fresh.root));xl6ParseResultFree(&fresh);
        if(member){const XL6_AstNode* body=parsed[v].root->children[2]->children[1];PROBE(body->childCount==2);}
        else PROBE(parsed[v].root->childCount==memos[v]->entries.Count+1);
        xl6ParseResultFree(&parsed[v]); /* next version keeps no original AST */
    }
    for(size_t v=0;v<4;++v){xlsFrontendSemMemoDestroy(memos[v]);xrtStrBufFree(&sources[v]);}
    xrtFree(path);return 1;
}
static int effectScopeAndBudget(void)
{
    uint64_t live=probeLive();XL6_SemContext ctx={0},foreign={0};xls_sem_memo m={.effectsComplete=1};
    xls_sem_build build={.memo=&m,.enabled=1,.effectContext=&ctx,.effectsCompatible=1,.previous=xlsActiveSemBuild};
    xlsActiveSemBuild=&build;
    xls_sem_effect_scope parent,other,child;xlsSemEffectScopeBegin(&parent,&ctx);xlsSemEffectScopeBegin(&other,&foreign);xlsSemEffectScopeBegin(&child,&ctx);
    xlsSemFunctionMutation(&ctx);PROBE(!xlsSemEffectScopeEnd(&child));PROBE(xlsSemEffectScopeEnd(&other));PROBE(!xlsSemEffectScopeEnd(&parent));
    XL6_AstNode* decl=xl6AstNewText(XL6_AST_TYPE,"receipt",7,NULL);PROBE(decl);
    xlsSemDeclarationEffectBegin(&ctx);PROBE(build.effectPending==1 && !xlsSemEffectsCapture(&build));
    m.bytes=XLS_SEM_MEMO_BYTES;xlsSemDeclarationEffectEnd(&ctx,NULL,decl);
    PROBE(!m.effectsComplete && !build.effectsCompatible && !build.effectPending && !m.effects.Count);
    PROBE(m.bytes==XLS_SEM_MEMO_BYTES);
    m.bytes=0;m.effectsComplete=1;build.effectsCompatible=1;
    xlsSemDeclarationEffectBegin(&ctx);xlsSemDeclarationEffectEnd(&ctx,NULL,decl);
    PROBE(m.effectsComplete && m.effects.Count==1 && m.bytes>0 && m.bytes<=XLS_SEM_MEMO_BYTES);
    xls_sem_entry expected={.effectCount=1};build.effectsCompatible=1;PROBE(xlsSemEffectsAdmit(&build,&expected));
    expected.effectCount=0;PROBE(!xlsSemEffectsAdmit(&build,&expected));
    xlsSemDeclarationEffectBegin(&ctx);PROBE(!xlsSemEffectsCapture(&build));
    /* An unfinished write cannot qualify later functions or a next version. */
    PROBE(!xlsSemEffectsAdmit(&build,&expected));
    xlsSemEffectsFree(&m);xl6AstFree(decl);xlsActiveSemBuild=build.previous;
    PROBE(probeLive()==live);return 1;
}
static int effectReserveFaults(void)
{
    uint64_t live=probeLive();XL6_SemContext ctx={0};xls_sem_memo old={.effectsComplete=1},m={0};
    xls_sem_build build={.memo=&old,.enabled=1,.effectContext=&ctx,.effectsCompatible=1,.previous=xlsActiveSemBuild};
    XL6_AstNode* decl=probeTree();PROBE(decl);xlsActiveSemBuild=&build;
    xlsSemDeclarationEffectBegin(&ctx);xlsSemDeclarationEffectEnd(&ctx,NULL,decl);PROBE(old.effectsComplete && old.effects.Count==1);
    const xstrbuf* original=xrtArrayConstGet(&old.effects,0);char* originalData=original->Data;size_t originalSize=original->Size;
    build.baseline=&old;build.memo=&m;
    for(unsigned kind=1;kind<=2;++kind){
        m=(xls_sem_memo){.effectsComplete=1};build.effectsCompatible=1;
        effectReserveKind=kind;effectReserveCalls=effectReserveFail=0;
        xlsSemDeclarationEffectBegin(&ctx);xlsSemDeclarationEffectEnd(&ctx,NULL,decl);effectReserveKind=0;
        size_t calls=effectReserveCalls;PROBE(calls>0 && m.effectsComplete && m.effects.Count==1 && build.effectsCompatible);
        xlsSemEffectsFree(&m);
        for(size_t fail=1;fail<=calls;++fail){
            uint64_t before=probeLive();m=(xls_sem_memo){.effectsComplete=1};build.effectsCompatible=1;
            effectReserveKind=kind;effectReserveCalls=0;effectReserveFail=fail;
            xlsSemDeclarationEffectBegin(&ctx);xlsSemDeclarationEffectEnd(&ctx,NULL,decl);effectReserveKind=0;
            PROBE(effectReserveCalls==fail && !m.effectsComplete && !build.effectsCompatible && !m.effects.Count && !build.effectPending && !m.bytes);
            xlsSemEffectsFree(&m);PROBE(probeLive()==before);
            PROBE(old.effectsComplete && old.effects.Count==1 && original->Data==originalData && original->Size==originalSize);
            m=(xls_sem_memo){.effectsComplete=1};build.effectsCompatible=1;
            xlsSemDeclarationEffectBegin(&ctx);xlsSemDeclarationEffectEnd(&ctx,NULL,decl);
            PROBE(m.effectsComplete && m.effects.Count==1 && build.effectsCompatible && xlsSemEqualKey(original,xrtArrayConstGet(&m.effects,0)));
            xlsSemEffectsFree(&m);PROBE(probeLive()==before);
        }
        printf("semantic effects targeted reserve failures: kind=%u sites=%zu\n",kind,calls);
    }
    xlsSemEffectsFree(&old);xl6AstFree(decl);xlsActiveSemBuild=build.previous;PROBE(probeLive()==live);return 1;
}
#endif
#ifndef XLS_SEM_EFFECTS_MAIN
#define XLS_SEM_EFFECTS_MAIN main
#endif
int XLS_SEM_EFFECTS_MAIN(void)
{
    if(xlsMetadataProbeMain())return 1;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    if(!effectHeaderLinks() || !effectScopeAndBudget() || !effectReserveFaults() || !effectVersions(0) || !effectVersions(1) || !effectVersions(2))return 1;
#endif
    printf("analysis_sem_effects_probe passed: %u checks; exact declaration receipts and original memo/fresh AST comparison\n",probeChecks);return 0;
}
