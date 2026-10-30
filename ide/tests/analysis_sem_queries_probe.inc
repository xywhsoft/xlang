/* Real native emitter/pool and real signature producer; no capture fences. */
#define XLS_SEM_EFFECTS_MAIN xlsEffectsProbeMain
#include "analysis_sem_effects_probe.inc"
#include "analysis/frontend_completion_pool.h"
int xlsReferenceSemCheckModuleWithQuery(XL6_AstNode*,XL6_DiagList*,const char*,int,XL6_CompletionQuery*);
int xlsReferenceCompletionAdd(XL6_CompletionQuery*,const char*,const char*,XL6_SymbolId,const XL6_AstNode*,char*);
char* xlsReferenceCompletionJson(XL6_CompletionQuery*,const XL6_SourceReport*);
void xlsReferenceCompletionFree(XL6_CompletionQuery*);
static int queryText(const char* marked,xstrbuf* text,size_t* offset)
{
    const char* at=strstr(marked,"@caret@");PROBE(at);
    PROBE(xrtStrBufAppend(text,(xstrview){marked,(size_t)(at-marked)}) && xrtStrBufAppend(text,xrtStrView(at+7)));
    *offset=(size_t)(at-marked)-(text->Size>=3 && !memcmp(text->Data,"\xef\xbb\xbf",3) ? 3 : 0);return 1;
}
static int queryItems(const XL6_CompletionQuery* a,const XL6_CompletionQuery* b)
{
    PROBE(a->count==b->count && a->bytes==b->bytes);
    for(size_t i=0;i<a->count;++i){
        const XL6_CompletionItem *x=&a->items[i],*y=&b->items[i];
        PROBE(sameText(x->name,y->name) && sameText(x->kind,y->kind) && sameText(x->canonical,y->canonical) && x->symbolId==y->symbolId);
        PROBE((x->selection!=NULL)==(y->selection!=NULL));
        if(x->selection)PROBE(sameText(xl6AstSourcePath(x->selection),xl6AstSourcePath(y->selection)) && sameRange(x->selection->range,y->selection->range) && sameRange(x->selection->tokenRange,y->selection->tokenRange));
    }
    return 1;
}
static int queryCompletion(const char* marked,const char* wanted,size_t poolBudget)
{
    char* path=xrtPathAbs("native-queries-中文.xl");PROBE(path);xstrbuf text={0};size_t offset=0;PROBE(queryText(marked,&text,&offset));
    XL6_ParseResult parsed[3];XL6_CompletionQuery queries[3]={{0}};
    xls_frontend_context_scope context;xlsFrontendContextBegin(&context,NULL);
    xls_completion_pool_scope collecting;xls_completion_pool pool={0};
    for(size_t i=0;i<3;++i){
        xl6ParseResultInit(&parsed[i]);queries[i]=(XL6_CompletionQuery){.path=path,.offset=offset};
        PROBE(xl6ParseSourceWithCompletion(text.Data,path,&parsed[i],&queries[i]));
        if(i==0){xlsCompletionPoolBegin(&collecting,&queries[i],poolBudget);xl6SemCheckModuleWithQuery(parsed[i].root,&parsed[i].diags,"queries",0,&queries[i]);}
        else{if(i==2){xrtFree(queries[i].prefix);queries[i].prefix=NULL;}xlsReferenceSemCheckModuleWithQuery(parsed[i].root,&parsed[i].diags,"queries",0,&queries[i]);}
        if(wanted && (!queries[i].available || queries[i].error)){
            fprintf(stderr,"query unavailable %zu: %s; source=[%s]\n",i,queries[i].error ? queries[i].error : "",marked);
            for(size_t n=0;n<parsed[i].diags.count;++n)fprintf(stderr,"%s\n",parsed[i].diags.items[n].message);
        }
        PROBE(queries[i].parsed==1 && queries[i].reached==1);
        PROBE(wanted ? queries[i].available && !queries[i].error : !queries[i].available && queries[i].error);
    }
    int pooled=xlsCompletionPoolEnd(&collecting,&pool);PROBE(pooled==(wanted && poolBudget>4096));
    PROBE(queryItems(&queries[0],&queries[1]));if(pooled)PROBE(queryItems(&pool.query,&queries[2]));
    if(wanted){int found=0;for(size_t i=0;i<queries[0].count;++i)if(!strcmp(queries[0].items[i].name,wanted))found=1;PROBE(found);}
    XL6_SourceReportItem item={.path=path,.text=text.Data,.size=text.Size,.kind=XL6_SOURCE_REPORT_SOURCE};XL6_SourceReport report={.items=&item,.count=1,.complete=1};
    char *a=xl6CompletionJson(&queries[0],&report),*b=xlsReferenceCompletionJson(&queries[1],&report);PROBE(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);
    xlsCompletionPoolFree(&pool);
    for(size_t i=0;i<3;++i){xl6CompletionFree(&queries[i]);xl6ParseResultFree(&parsed[i]);}
    PROBE(xlsFrontendContextEnd(&context));xrtStrBufFree(&text);xrtFree(path);return 1;
}
static int queryCursorVersions(void)
{
    char* path=xrtPathAbs("native-cursor-versions.xl");PROBE(path);
    xstrbuf text[3]={{0}};XL6_SourceReportItem files[3]={{0}};XL6_SourceReport reports[3]={{0}};xls_sem_memo* memo[3]={0};
    for(size_t v=0;v<3;++v){
        for(unsigned f=0;f<2;++f){PROBE(xrtStrBufAppendFormat(&text[v],"func stable_%u() as int\n",f));for(unsigned n=0;n<20;++n)PROBE(xrtStrBufAppendFormat(&text[v]," int value_%u = %u\n",n,n));PROBE(xrtStrBufAppend(&text[v],xrtStrView(" return value_19\nend\n")));}
        PROBE(xrtStrBufAppendFormat(&text[v],"func edited() as int\n int result = 7\n return %s\nend\n",v==1 ? "resu" : "res"));
        size_t offset=(size_t)(strrchr(text[v].Data,'\n')-text[v].Data)-4;
        const char* marker=strstr(text[v].Data," return res");PROBE(marker);offset=(size_t)(marker-text[v].Data)+strlen(v==1 ? " return resu" : " return res");
        XL6_ParseResult a,b;xl6ParseResultInit(&a);xl6ParseResultInit(&b);XL6_CompletionQuery qa={.path=path,.offset=offset},qb=qa;
        PROBE(xl6ParseSourceWithCompletion(text[v].Data,path,&a,&qa) && xl6ParseSourceWithCompletion(text[v].Data,path,&b,&qb));
        files[v]=(XL6_SourceReportItem){.path=path,.text=text[v].Data,.size=text[v].Size,.kind=XL6_SOURCE_REPORT_SOURCE};reports[v]=(XL6_SourceReport){.items=&files[v],.count=1,.complete=1};xls_sem_work work={0};
        int cached=xlsFrontendSemCheckCursorMemo(a.root,&a.diags,"cursor",&qa,path,&reports[v],v ? &reports[v-1] : NULL,v ? memo[v-1] : NULL,&memo[v],&work);
        int fresh=xl6SemCheckModuleWithQuery(b.root,&b.diags,"cursor",0,&qb);
        PROBE(cached==fresh && memo[v] && qa.available && qb.available && queryItems(&qa,&qb) && sameAst(a.root,b.root));
        if(v)PROBE(work.reused>=2);
        xl6CompletionFree(&qa);xl6CompletionFree(&qb);xl6ParseResultFree(&a);xl6ParseResultFree(&b);
    }
    for(size_t v=0;v<3;++v){xlsFrontendSemMemoDestroy(memo[v]);xrtStrBufFree(&text[v]);}xrtFree(path);return 1;
}
static const XL6_AstNode* queryFindCall(const XL6_AstNode* node,size_t open)
{
    if(!node)return NULL;
    if(node->kind==XL6_AST_CALL && node->tokenRange.known && node->tokenRange.startOffset==open)return node;
    for(size_t i=0;i<node->childCount;++i){const XL6_AstNode* found=queryFindCall(node->children[i],open);if(found)return found;}return NULL;
}
static int querySignature(const char* marked,const char* wanted,size_t parameter)
{
    char* path=xrtPathAbs("native-signature-中文.xl");PROBE(path);xstrbuf source={0};size_t offset=0;PROBE(queryText(marked,&source,&offset));
    xls_text* text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);PROBE(text);
    xls_signature_data* data=xrtCalloc(1,sizeof(*data));PROBE(data);data->index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);xlsTextRelease(text);PROBE(data->index);
    size_t bom=source.Size>=3 && !memcmp(source.Data,"\xef\xbb\xbf",3) ? 3 : 0;
    data->source=source.Data+bom;data->size=source.Size-bom;data->offset=offset;data->path=xrtStrDup(path);data->active=SIZE_MAX;
    PROBE(data->path && xrtArrayInit(&data->records,sizeof(xls_signature_record)) && xrtArrayReserve(&data->records,256));data->bytes=sizeof(*data)+data->records.Capacity*sizeof(xls_signature_record);
    PROBE(xlsCallContextLookupRecovering(data->index,offset,&data->site)==1);data->hasSite=1;
    XL6_CompletionQuery q={.path=path,.offset=offset};xls_signature_scope scope;xlsSignatureScopeBegin(&scope,data,NULL,NULL);scope.query=&q;
    XL6_ParseResult first,parsed;xl6ParseResultInit(&first);xl6ParseResultInit(&parsed);
    PROBE(xlsSignatureParseCached(source.Data,path,&first,&q));xl6CompletionFree(&q);q=(XL6_CompletionQuery){.path=path,.offset=offset};scope.query=&q;
    PROBE(xlsSignatureParseCached(source.Data,path,&parsed,&q) && data->parseWork.exactHits>=1 && data->projected && sameAst(first.root,parsed.root));xl6ParseResultFree(&first);
    xl6SemCheckModuleWithQuery(parsed.root,&parsed.diags,"signature",0,&q);xlsSignatureScopeEnd(&scope);
    if(q.error || data->error)fprintf(stderr,"signature capture: %s %s\n",q.error ? q.error : "",data->error ? data->error : "");
    PROBE(q.available && q.reached==1 && !q.error && !data->error && data->records.Count && data->active<data->records.Count);
    const xls_signature_record* record=xrtArrayConstGet(&data->records,data->active);
    if(strcmp(record->label,wanted))fprintf(stderr,"signature actual=[%s] expected=[%s]\n",record->label,wanted);
    PROBE(!strcmp(record->label,wanted) && record->active==parameter);
    for(size_t i=0;i<record->count;++i)PROBE(record->parameters[i].labelStart<record->parameters[i].labelEnd && record->parameters[i].labelEnd<=strlen(record->label));
    XL6_ParseResult oracle;xl6ParseResultInit(&oracle);PROBE(xl6ParseSource(source.Data,path,&oracle));
    PROBE(xlsReferenceSemCheckModuleWithQuery(oracle.root,&oracle.diags,"signature",1,NULL));
    const XL6_AstNode* call=queryFindCall(oracle.root,data->site.argumentsStart-1);PROBE(call && record->symbol && record->symbol==call->semanticTargetSymbolId);xl6ParseResultFree(&oracle);
    xlsSignatureDataDestroy(data);xl6CompletionFree(&q);xl6ParseResultFree(&parsed);xrtStrBufFree(&source);xrtFree(path);return 1;
}
static int queryDefaultVersions(void)
{
    char* path=xrtPathAbs("native-default-versions.xl");PROBE(path);xstrbuf text[4]={{0}};XL6_SourceReportItem items[4]={{0}};XL6_SourceReport reports[4]={{0}};xls_sem_memo* memos[4]={0};
    for(size_t v=0;v<4;++v){
        PROBE(xrtStrBufAppendFormat(&text[v],"func dependency(int value%s) as int\n return value\nend\n",v==2 ? "" : v ? "=8" : "=7"));
        for(unsigned f=0;f<2;++f){PROBE(xrtStrBufAppendFormat(&text[v],"func default_%u() as int\n",f));for(unsigned n=0;n<20;++n)PROBE(xrtStrBufAppendFormat(&text[v]," int value_%u = %u\n",n,n));PROBE(xrtStrBufAppend(&text[v],xrtStrView(f ? " return value_19\nend\n" : " return dependency()\nend\n")));}
        XL6_ParseResult a,b;xl6ParseResultInit(&a);xl6ParseResultInit(&b);PROBE(xl6ParseSource(text[v].Data,path,&a) && xl6ParseSource(text[v].Data,path,&b));
        items[v]=(XL6_SourceReportItem){.path=path,.text=text[v].Data,.size=text[v].Size,.kind=XL6_SOURCE_REPORT_SOURCE};reports[v]=(XL6_SourceReport){.items=&items[v],.count=1,.complete=1};xls_sem_work work={0};
        int cached=xlsFrontendSemCheckMemo(a.root,&a.diags,"defaults",1,path,&reports[v],v ? &reports[v-1] : NULL,v ? memos[v-1] : NULL,&memos[v],&work);
        int fresh=xl6SemCheckModuleWithIndex(b.root,&b.diags,"defaults",1);
        PROBE(cached==fresh && cached==(v!=2) && memos[v] && sameAst(a.root,b.root) && a.diags.count==b.diags.count);
        for(size_t i=0;i<a.diags.count;++i){const XL6_Diag *x=&a.diags.items[i],*y=&b.diags.items[i];PROBE(x->line==y->line && x->column==y->column && sameText(x->message,y->message) && sameText(x->file,y->file));}
        const xls_sem_entry *caller=xrtArrayConstGet(&memos[v]->entries,1),*tail=xrtArrayConstGet(&memos[v]->entries,2);
        if(v==1){const xls_sem_entry *oldCaller=xrtArrayConstGet(&memos[0]->entries,1),*oldTail=xrtArrayConstGet(&memos[0]->entries,2);PROBE(caller->unit && caller->unit!=oldCaller->unit && tail->unit==oldTail->unit && work.reused>=1 && memos[v]->readWork.rejected>=1);}
        if(v==2)PROBE(!caller->unit && a.diags.count>0);
        xl6ParseResultFree(&a);xl6ParseResultFree(&b);
    }
    for(size_t v=0;v<4;++v){xlsFrontendSemMemoDestroy(memos[v]);xrtStrBufFree(&text[v]);}xrtFree(path);return 1;
}
static int queryAlwaysCancel(void* value){(void)value;return 1;}
static int querySignatureFailures(void)
{
    char* path=xrtPathAbs("native-signature-failure.xl");PROBE(path);xstrbuf source={0};size_t offset=0;PROBE(queryText("func edited()\n target(@caret@7)\nend\n",&source,&offset));
    for(unsigned mode=0;mode<2;++mode){
        xls_text* text=xlsTextCreate(source.Data,source.Size,1024,NULL);PROBE(text);xls_signature_data* data=xrtCalloc(1,sizeof(*data));PROBE(data);
        data->index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);xlsTextRelease(text);PROBE(data->index);
        data->source=source.Data;data->size=source.Size;data->offset=offset;data->path=xrtStrDup(mode ? "invalid-\xff.xl" : path);data->hasSite=1;PROBE(data->path && xlsCallContextLookupRecovering(data->index,offset,&data->site)==1);
        XL6_CompletionQuery q={.path=path,.offset=offset};xls_signature_scope scope;xlsSignatureScopeBegin(&scope,data,mode ? NULL : queryAlwaysCancel,NULL);scope.query=&q;
        XL6_ParseResult parsed;xl6ParseResultInit(&parsed);PROBE(!xlsSignatureParseCached(source.Data,path,&parsed,&q));xlsSignatureScopeEnd(&scope);
        PROBE(q.error && !q.available && !data->records.Count && !data->projected);if(!mode)PROBE(scope.cancelObserved);
        xl6CompletionFree(&q);xl6ParseResultFree(&parsed);xlsSignatureDataDestroy(data);
    }
    xrtStrBufFree(&source);xrtFree(path);return 1;
}
static int queryAliasesAndPathErrors(void)
{
    char* path=xrtPathAbs("native-emission.xl");PROBE(path);XL6_AstNode* selection=xl6AstNewText(XL6_AST_NAME,"selected",8,NULL);PROBE(selection && xl6AstSetSourceTree(selection,path));
    xls_frontend_context_scope context;xlsFrontendContextBegin(&context,NULL);PROBE(xlsFrontendNamespaceStart() && xlsFrontendNamespaceAdd("__xl6_hidden","alias") && xlsFrontendNamespaceAdd("__xl6_hidden","another"));
    XL6_CompletionQuery actual={.path=path,.parsed=1,.reached=1,.available=1,.prefix=xrtStrDup("alias")},filtered=actual,all={0};filtered.prefix=xrtStrDup("alias");PROBE(actual.prefix && filtered.prefix);
    xls_completion_pool_scope collecting;xls_completion_pool pool={0};xlsCompletionPoolBegin(&collecting,&actual,8u*1024u*1024u);
    PROBE(xl6CompletionAdd(&actual,"__xl6_hidden","variable",7,selection,xrtStrDup("int")));
    PROBE(xlsReferenceCompletionAdd(&filtered,"alias","variable",7,selection,xrtStrDup("int")) && xlsReferenceCompletionAdd(&all,"alias","variable",7,selection,xrtStrDup("int")) && xlsReferenceCompletionAdd(&all,"another","variable",7,selection,xrtStrDup("int")));
    PROBE(xlsCompletionPoolEnd(&collecting,&pool) && queryItems(&actual,&filtered) && queryItems(&pool.query,&all));xlsCompletionPoolFree(&pool);
    PROBE(xlsFrontendContextEnd(&context));
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    PROBE(xl6AstSetSourceTree(selection,"invalid-\xff.xl"));
    XL6_SourceReportItem item={.path=path,.text="selected",.size=8,.kind=XL6_SOURCE_REPORT_SOURCE};XL6_SourceReport report={.items=&item,.count=1,.complete=1};
    char *a=xl6CompletionJson(&actual,&report),*b=xlsReferenceCompletionJson(&filtered,&report);
    PROBE(a && b && !strcmp(a,b) && strstr(a,"\"complete\":false") && strstr(a,"\"items\":[]") && actual.error && !actual.available);xrtFree(a);xrtFree(b);
#endif
    xl6CompletionFree(&actual);xl6CompletionFree(&filtered);xl6CompletionFree(&all);xl6AstFree(selection);xrtFree(path);return 1;
}
static int querySignatureVersions(void)
{
    char* path=xrtPathAbs("native-signature-versions.xl");PROBE(path);xstrbuf sources[3]={{0}};XL6_SourceReportItem items[3]={{0}};XL6_SourceReport reports[3]={{0}};xls_sem_memo* memos[3]={0};
    xls_signature_data* previous=NULL;XL6_ParseResult held;xl6ParseResultInit(&held);
    for(size_t v=0;v<3;++v){
        for(unsigned f=0;f<2;++f){PROBE(xrtStrBufAppendFormat(&sources[v],"func stable_%u() as int\n",f));for(unsigned n=0;n<20;++n)PROBE(xrtStrBufAppendFormat(&sources[v]," int value_%u=%u\n",n,n));PROBE(xrtStrBufAppend(&sources[v],xrtStrView(" return value_19\nend\n")));}
        PROBE(xrtStrBufAppendFormat(&sources[v],"func plus(int left,string right=\"\") as int\n return left\nend\nfunc edited() as int\n return plus(%u,\"x\")\nend\n",v ? 77 : 7));
        const char* call=strstr(sources[v].Data," return plus(");PROBE(call);size_t offset=(size_t)(strchr(call,',')-sources[v].Data)+1;
        xls_text* text=xlsTextCreate(sources[v].Data,sources[v].Size,1024*1024,NULL);PROBE(text);xls_signature_data* data=xrtCalloc(1,sizeof(*data));PROBE(data);
        data->index=xlsCallContextCreate(text,64u*1024u*1024u,NULL,NULL,NULL,0);xlsTextRelease(text);PROBE(data->index);
        data->source=sources[v].Data;data->size=sources[v].Size;data->offset=offset;data->path=xrtStrDup(path);data->active=SIZE_MAX;data->hasSite=1;
        PROBE(data->path && xlsCallContextLookupRecovering(data->index,offset,&data->site)==1 && xrtArrayInit(&data->records,sizeof(xls_signature_record)) && xrtArrayReserve(&data->records,256));data->bytes=sizeof(*data)+data->records.Capacity*sizeof(xls_signature_record);
        if(previous){data->syntax=xlsSyntaxRetain(previous->syntax);data->parseWork.cacheBytes=previous->parseWork.cacheBytes;xlsSignatureDataDestroy(previous);xl6ParseResultFree(&held);}
        XL6_ParseResult parsed;xl6ParseResultInit(&parsed);XL6_CompletionQuery q={.path=path,.offset=offset};xls_signature_scope scope;xlsSignatureScopeBegin(&scope,data,NULL,NULL);scope.query=&q;
        PROBE(xlsSignatureParseCached(sources[v].Data,path,&parsed,&q));
        if(v==1)PROBE(data->parseWork.fragments==1 && data->parseWork.reused>=2 && !data->parseWork.full);
        if(v==2)PROBE(data->parseWork.exactHits==1 && !data->parseWork.full);
        items[v]=(XL6_SourceReportItem){.path=path,.text=sources[v].Data,.size=sources[v].Size,.kind=XL6_SOURCE_REPORT_SOURCE};reports[v]=(XL6_SourceReport){.items=&items[v],.count=1,.complete=1};xls_sem_work work={0};
        int cached=xlsFrontendSemCheckCursorMemo(parsed.root,&parsed.diags,"signature-versions",&q,path,&reports[v],v ? &reports[v-1] : NULL,v ? memos[v-1] : NULL,&memos[v],&work);xlsSignatureScopeEnd(&scope);
        if(v)PROBE(work.reused>=2);
        xls_signature_data fresh={.source=data->source,.size=data->size,.offset=offset,.path=data->path,.site=data->site,.hasSite=1,.active=SIZE_MAX};PROBE(xrtArrayInit(&fresh.records,sizeof(xls_signature_record)) && xrtArrayReserve(&fresh.records,256));fresh.bytes=sizeof(fresh)+fresh.records.Capacity*sizeof(xls_signature_record);
        XL6_ParseResult oracle;xl6ParseResultInit(&oracle);XL6_CompletionQuery oq={.path=path,.offset=offset};xls_signature_scope os;xlsSignatureScopeBegin(&os,&fresh,NULL,NULL);os.query=&oq;
        PROBE(xl6ParseSourceWithCompletion(sources[v].Data,path,&oracle,&oq));int ordinary=xl6SemCheckModuleWithQuery(oracle.root,&oracle.diags,"signature-versions",0,&oq);xlsSignatureScopeEnd(&os);
        PROBE(cached==ordinary && q.available && oq.available && memos[v] && sameAst(parsed.root,oracle.root) && data->records.Count==fresh.records.Count && data->active==fresh.active);
        for(size_t i=0;i<data->records.Count;++i){const xls_signature_record *a=xrtArrayConstGet(&data->records,i),*b=xrtArrayConstGet(&fresh.records,i);PROBE(!strcmp(a->label,b->label) && a->count==b->count && a->active==b->active && a->symbol==b->symbol && a->rank==b->rank && a->ranked==b->ranked);}
        xlsSignatureDataResetCaptured(&fresh);xrtArrayUnit(&fresh.records);xl6ParseResultFree(&oracle);xl6CompletionFree(&oq);xl6CompletionFree(&q);previous=data;held=parsed;
    }
    xlsSignatureDataDestroy(previous);xl6ParseResultFree(&held);for(size_t v=0;v<3;++v){xlsFrontendSemMemoDestroy(memos[v]);xrtStrBufFree(&sources[v]);}xrtFree(path);return 1;
}
#ifndef XLS_SEM_QUERIES_MAIN
#define XLS_SEM_QUERIES_MAIN main
#endif
int XLS_SEM_QUERIES_MAIN(void)
{
    if(xlsEffectsProbeMain())return 1;
    if(!queryCompletion("func edited() as int\n int result = 7\n return res@caret@\nend\n","result",8u*1024u*1024u) ||
       !queryCompletion("func edited() as int\n int result = 7\n return res@caret@\nend\n","result",1) ||
       !queryCompletion("class Box<T>\n func get(T value) as T\n return value\n end\nend\nfunc edited()\n auto box=Box::<int>()\n box.ge@caret@\nend\n","get",8u*1024u*1024u) || !queryCursorVersions() || !queryDefaultVersions() || !querySignatureFailures() || !queryAliasesAndPathErrors())return 1;
    if(!querySignature("func plus(int left,string right=\"\") as int\n return left\nend\nfunc edited() as int\n return plus(1,@caret@\"x\")\nend\n","plus(int left, string right = ...) as int",1) ||
       !querySignature("func identity<T>(T value) as T\n return value\nend\nfunc edited() as int\n return identity::<int>(@caret@7)\nend\n","identity::<int>(int value) as int",0) ||
       !querySignature("class Box<T>\n func apply(T value) as T\n return value\n end\nend\nfunc edited() as int\n auto box=Box::<int>()\n return box.apply(@caret@7)\nend\n","box.apply(int value) as int",0) ||
       !querySignature("func choose(int value,int endValue) as int\n return value\nend\nfunc choose(string value,int endValue) as int\n return endValue\nend\nfunc edited() as int\n return choose(\"x\",@caret@0)\nend\n","choose(string value, int endValue) as int",1) ||
       !querySignature("func higher(func(int,string) as int callback,int count) as int\n return count\nend\nfunc work(int x,string y) as int\n return x\nend\nfunc edited() as int\n return higher(@caret@work,1)\nend\n","higher(func(int,string) as int callback, int count) as int",0))return 1;
    if(!querySignatureVersions())return 1;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
    if(!queryCompletion("typedef Callback=func(c_vararg) as int\nfunc edited()\n Call@caret@\nend\n",NULL,8u*1024u*1024u) ||
       !queryCompletion("typedef Callback=func(int,c_vararg) as int\nfunc edited()\n Call@caret@\nend\n","Callback",8u*1024u*1024u) ||
       !queryCompletion("typedef Callback=func(int?) as int\nfunc edited()\n Call@caret@\nend\n","Callback",8u*1024u*1024u))return 1;
#endif
    printf("analysis_sem_queries_probe passed: %u checks; original completion oracle,actual pool and signature capture\n",probeChecks);return 0;
}
