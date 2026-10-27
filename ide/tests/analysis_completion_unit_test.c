#define main cursorOriginalMain
#include "analysis_cursor_syntax_test.c"
#undef main
#include "analysis/completion_unit.h"
#include "analysis/completion_session.h"
#include "analysis/cursor_word_view.h"
#include <limits.h>
#include "analysis/input_batch.h"
#include "analysis/input_topology.h"
static int unitParitySet(xls_semantic_completion* result,const xls_analysis_document* docs,size_t count,size_t at)
{
    xls_semantic_input inputs[3];CHECK(count && count<=3);
    for(size_t i=0;i<count;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_completion* fresh=xlsSemanticCompletionCreate(docs[0].path,docs[0].path,at,inputs,count,NULL,NULL,NULL,0);
    CHECK(result && fresh);
    char* a=xlsSemanticCompletionJson(result,NULL);char* b=xlsSemanticCompletionJson(fresh,NULL);
    if(a && b && strcmp(a,b))fprintf(stderr,"unit JSON differs\nactual=%s\nfresh=%s\n",a,b);
    CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);
    CHECK(sameAst(xlsSemanticCompletionAst(result),xlsSemanticCompletionAst(fresh)));
    xlsSemanticCompletionDestroy(fresh);return 1;
}
static int unitParity(xls_semantic_completion* result,const xls_analysis_document* doc,size_t at)
{return unitParitySet(result,doc,1,at);}
static int wordSequence(const char* prefix,const char* newline,int uriSeparators)
{
    const char* words[]={"p","pr","pre","prefix","prefixO","prefixOne","pr","pre","p","print","int","p","pr"};
    char* path=xrtPathAbs("completion-word-units.xl");CHECK(path);
    if(uriSeparators)for(char* at=path;*at;++at)if((unsigned char)*at==92)*at='/';
    xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_semantic_completion* held=NULL;
    unsigned projected=0;
    for(size_t i=0;i<sizeof(words)/sizeof(*words);++i){
        xstrbuf source={0};CHECK(xrtStrBufAppendFormat(&source,"%sfunc before(int value) as int%s return value%send%sfunc edited() as int%s %s%s return 0%send%sfunc prefixOne(int value) as int%s return value%send%sfunc prefixOther(int value) as int%s return value%send%s",prefix,newline,newline,newline,newline,words[i],newline,newline,newline,newline,newline,newline,newline,newline,newline));
        const char* word=strstr(source.Data,"func edited()");word=strchr(word,'\n');word=strstr(word,words[i]);CHECK(word);
        size_t bom=skipBom(source.Data),at=(size_t)(word-source.Data)+strlen(words[i])-bom;
        xls_text* text=xlsTextCreate(source.Data,source.Size,8u*1024u*1024u,NULL);CHECK(text);
        xls_analysis_document doc={.key="entry",.path=path,.text=text,.identity=1,.revision=i+1};char error[256]={0};
        xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",at,NULL,NULL,error,sizeof(error));
        if(!result)fprintf(stderr,"word query error %s\n",error);
        CHECK(result);
        xls_completion_unit_work unit=xlsSemanticCompletionUnitWork(result);
        fprintf(stderr,"word=%s projected=%llu available=%d proof=%llu\n",words[i],(unsigned long long)unit.projected,xlsSemanticCompletionInfo(result).available,(unsigned long long)unit.proofNodes);
        if(unit.projected){++projected;CHECK(unit.proofNodes && !unit.astMaterialized && !unit.astFailures && unit.knownBytes);
            CHECK(!xlsSemanticCompletionSemWork(result).checked && !xlsSemanticCompletionMaterializationWork(result).nodes);}
        CHECK(unitParity(result,&doc,at));
        if(unit.projected)CHECK(xlsSemanticCompletionUnitWork(result).astMaterialized==1);
        if(!held && unit.projected)held=xlsSemanticCompletionRetain(result);
        xlsSemanticCompletionDestroy(result);xlsTextRelease(text);xrtStrBufFree(&source);
    }
    xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);CHECK(projected>=8 && stats.wordViews==projected && stats.wordProofNodes);
    xlsCompletionSessionDestroy(session);CHECK(held && xlsSemanticCompletionValidate(held));xls_completion_item item;
    CHECK(xlsSemanticCompletionInfo(held).available && xlsSemanticCompletionItem(held,0,&item) && item.label && item.kind);
    xlsSemanticCompletionDestroy(held);xrtFree(path);return 1;
}
static int wordTemplate(const char* source,const char* first,const char* second,const char* marker)
{
    char* before=replace(source,"@word@",first);char* after=replace(source,"@word@",second);CHECK(before && after);
    char* path=xrtPathAbs("completion-word-template.xl");CHECK(path);xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);
    xls_text* a=xlsTextCreate(before,strlen(before),8u*1024u*1024u,NULL);xls_text* b=xlsTextCreate(after,strlen(after),8u*1024u*1024u,NULL);CHECK(a && b);
    xls_analysis_document doc={"entry",path,a,1,1};size_t at=cursorAt(before,marker);
    xls_semantic_completion* old=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);
    CHECK(old && xlsSemanticCompletionInfo(old).available);doc.text=b;doc.revision=2;at=at-strlen(first)+strlen(second);
    xls_semantic_completion* current=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",at,NULL,NULL,NULL,0);CHECK(current);
    CHECK(xlsSemanticCompletionUnitWork(current).projected && unitParity(current,&doc,at));
    xlsSemanticCompletionDestroy(old);xlsCompletionSessionDestroy(session);xlsTextRelease(a);xlsTextRelease(b);xrtFree(path);xrtFree(before);xrtFree(after);
    CHECK(xlsSemanticCompletionValidate(current));xlsSemanticCompletionDestroy(current);return 1;
}
static int proofBoundaries(void)
{
    const char* a="func edited() as int\n p\n return 0\nend\nfunc after(int value) as int\n return value\nend\n";
    const char* b="func edited() as int\n pref\n return 0\nend\nfunc after(int value) as int\n return value\nend\n";
    char* path=xrtPathAbs("completion-word-proof.xl");CHECK(path);xls_parse_work work={0};
    xls_syntax* old=build(path,a,cursorAt(a,"\n p"),NULL,&work);work=(xls_parse_work){0};xls_syntax* next=build(path,b,cursorAt(b,"\n pref"),old,&work);CHECK(old && next);
    xls_cursor_word_view edit;CHECK(xlsSyntaxCursorWordProof(old,next,&edit,NULL,NULL) && edit.nodes && edit.prefixSize==4);
    size_t known=0;CHECK(xlsSyntaxCursorRetainedBytes(old,256u*1024u*1024u,&known) && known);
    CHECK(!xlsSyntaxCursorRetainedBytes(old,known-1,&known) && !known);
    xls_cursor_word_view reverse;CHECK(xlsSyntaxCursorWordProof(next,old,&reverse,NULL,NULL));
    XL6_AstRange inside=reverse.before,mapped;inside.startOffset++;CHECK(!xlsCursorWordMapRange(&reverse,&inside,&mapped));
    const char* changed="func edited() as int\n pref\n return 1\nend\nfunc after(int value) as int\n return value\nend\n";
    work=(xls_parse_work){0};xls_syntax* different=build(path,changed,cursorAt(changed,"\n pref"),old,&work);CHECK(different);
    edit.nodes=99;CHECK(!xlsSyntaxCursorWordProof(old,different,&edit,NULL,NULL) && !edit.nodes && !edit.prefix);
    CHECK(!xlsSyntaxCursorWordProof(NULL,next,&edit,NULL,NULL) && !xlsSyntaxCursorWordProof(old,NULL,&edit,NULL,NULL));
    xlsSyntaxRelease(different);xlsSyntaxRelease(old);xlsSyntaxRelease(next);xrtFree(path);return 1;
}
static int guardedDependency(void)
{
    char relative[160];snprintf(relative,sizeof(relative),"build/completion-unit-%llu",(unsigned long long)xrtClock());
    char* directory=xrtPathAbs(relative);char* path=xrtPathJoin(directory,"main.xl");char* dep=xrtPathJoin(directory,"dep.xl");CHECK(directory && path && dep && xrtDirCreateAll(directory));
    const char* helper="export func prefixValue() as int\n return 7\nend\n";CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)helper,strlen(helper)}));
    const char* a="import dep\nfunc main(...) as int\n return dep.p\nend\n";
    const char* b="import dep\nfunc main(...) as int\n return dep.pre\nend\n";
    xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);
    xls_text* old=xlsTextCreate(a,strlen(a),8u*1024u*1024u,NULL);xls_text* next=xlsTextCreate(b,strlen(b),8u*1024u*1024u,NULL);CHECK(old && next);
    xls_analysis_document doc={"entry",path,old,1,1};xls_semantic_completion* first=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(a,"dep.p"),NULL,NULL,NULL,0);CHECK(first && xlsSemanticCompletionInfo(first).available);
    doc.text=next;doc.revision=2;xls_semantic_completion* current=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(b,"dep.pre"),NULL,NULL,NULL,0);
    CHECK(current && xlsSemanticCompletionUnitWork(current).projected && unitParity(current,&doc,cursorAt(b,"dep.pre")));
    xlsSemanticCompletionDestroy(first);xlsCompletionSessionDestroy(session);xlsTextRelease(old);xlsTextRelease(next);
    CHECK(xlsSemanticCompletionValidate(current));
    const char* changed="export func prefixValue() as string\n return \"different\"\nend\n";CHECK(xrtFileWriteAtomic(dep,(xbytesview){(const unsigned char*)changed,strlen(changed)}));
    CHECK(!xlsSemanticCompletionValidate(current) && !xlsSemanticCompletionInfo(current).available && !xlsSemanticCompletionAst(current));
    xlsSemanticCompletionDestroy(current);xrtFree(path);xrtFree(dep);xrtFree(directory);return 1;
}
typedef struct word_cancel {unsigned seen,at;} word_cancel;
static int wordCancel(void* user){word_cancel* stop=user;return ++stop->seen>=stop->at;}
static int cancellationBoundaries(void)
{
    const char* a="func main(...) as int\n p\n return 0\nend\n";const char* b="func main(...) as int\n pr\n return 0\nend\n";
    char* path=xrtPathAbs("completion-unit-cancel.xl");CHECK(path);xls_text* old=xlsTextCreate(a,strlen(a),8u*1024u*1024u,NULL);xls_text* next=xlsTextCreate(b,strlen(b),8u*1024u*1024u,NULL);CHECK(old && next);
    unsigned failures=0,boundaries=0;
    {
        xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,old,1,1};
        xls_semantic_completion* first=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(a,"\n p"),NULL,NULL,NULL,0);CHECK(first);
        doc.text=next;doc.revision=2;word_cancel stop={0,UINT_MAX};xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(b,"\n pr"),wordCancel,&stop,NULL,0);
        CHECK(result && xlsSemanticCompletionUnitWork(result).projected);boundaries=stop.seen;CHECK(boundaries);
        xlsSemanticCompletionDestroy(result);xlsSemanticCompletionDestroy(first);xlsCompletionSessionDestroy(session);
    }
    for(unsigned i=1;i<=boundaries+2;++i){
        xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,old,1,1};
        xls_semantic_completion* first=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(a,"\n p"),NULL,NULL,NULL,0);CHECK(first && xlsSemanticCompletionInfo(first).available);
        doc.text=next;doc.revision=2;word_cancel stop={0,i};xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(b,"\n pr"),wordCancel,&stop,NULL,0);
        if(result){CHECK(stop.seen<i && xlsSemanticCompletionInfo(result).available);xlsSemanticCompletionDestroy(result);}else{
            ++failures;CHECK(stop.seen>=i && xlsSemanticCompletionValidate(first));
            result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(b,"\n pr"),NULL,NULL,NULL,0);CHECK(result && xlsSemanticCompletionUnitWork(result).projected);xlsSemanticCompletionDestroy(result);
        }
        xlsCompletionSessionDestroy(session);xlsSemanticCompletionDestroy(first);
    }
    CHECK(failures==boundaries);printf("word query cancellation boundaries: %u\n",boundaries);
    xlsTextRelease(old);xlsTextRelease(next);xrtFree(path);return 1;
}
static int memoAfterWordViews(void)
{
    char* path=xrtPathAbs("completion-unit-return-to-checker.xl");CHECK(path);
    xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_semantic_completion* held=NULL;
    char* unused=xrtPathAbs("completion-unit-unused.xl");char* extra=xrtPathAbs("completion-unit-extra.xl");CHECK(unused && extra);
    xls_text* unusedBody=xlsTextCreate("// unused before\n",17,1024,NULL);CHECK(unusedBody);
    xls_analysis_document docs[3]={{0},{"unused",unused,unusedBody,2,1},{0}};size_t count=2;
    for(unsigned generation=0;generation<3;++generation){
        xstrbuf text={0};
        for(unsigned i=0;i<128;++i)CHECK(xrtStrBufAppendFormat(&text,"func stable_%u(int value) as int\n if value>0 then\n return value+%u\n end\n return 0\nend\n",i,i));
        CHECK(xrtStrBufAppendFormat(&text,"func main(...) as int\n int completionLocal=stable_0(%u)\n return %s\nend\n",generation==2 ? 8u : 7u,generation ? "completionLo" : "completionL"));
        xls_text* body=xlsTextCreate(text.Data,text.Size,8u*1024u*1024u,NULL);CHECK(body);docs[0]=(xls_analysis_document){"entry",path,body,1,generation+1};size_t at=cursorAt(text.Data,generation ? "return completionLo" : "return completionL");
        xls_analysis_document reordered[2]={docs[1],docs[0]};
        xls_semantic_completion* result=xlsCompletionSessionQuery(session,generation==1 ? reordered : docs,count,"entry","entry",at,NULL,NULL,NULL,0);CHECK(result && xlsSemanticCompletionInfo(result).available);
        xls_completion_unit_work unit=xlsSemanticCompletionUnitWork(result);xls_sem_work work=xlsSemanticCompletionSemWork(result);
        if(generation==1){
            uint64_t used=0;CHECK(unit.projected && !work.checked && xlsSemanticCompletionInputUsage(result,&used) && used==2 && xlsSemanticCompletionTopology(result));held=xlsSemanticCompletionRetain(result);
            xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate("// unused after\n",16,1024,NULL);++docs[1].revision;CHECK(docs[1].text);
            xls_semantic_completion* same=xlsCompletionSessionQuery(session,docs,2,"entry","entry",at,NULL,NULL,NULL,0);CHECK(same==result && unitParitySet(same,docs,2,at));xlsSemanticCompletionDestroy(same);
            docs[2]=(xls_analysis_document){"extra",extra,xlsTextCreate("// added\n",9,1024,NULL),3,1};CHECK(docs[2].text);count=3;
            same=xlsCompletionSessionQuery(session,docs,3,"entry","entry",at,NULL,NULL,NULL,0);CHECK(same==result && unitParitySet(same,docs,3,at));xlsSemanticCompletionDestroy(same);
        }
        if(generation==2){CHECK(!unit.projected && work.reused>=128 && work.proofHits>=127 && unitParitySet(result,docs,count,at));}
        xlsSemanticCompletionDestroy(result);xlsTextRelease(body);xrtStrBufFree(&text);
    }
    xls_completion_stats stats;xlsCompletionSessionStats(session,&stats);CHECK(stats.builds==2 && stats.wordViews==1 && stats.inputScopeHits==2);
    xlsCompletionSessionDestroy(session);CHECK(held && xlsSemanticCompletionValidate(held));xlsSemanticCompletionDestroy(held);xlsTextRelease(docs[1].text);xlsTextRelease(docs[2].text);xrtFree(unused);xrtFree(extra);xrtFree(path);return 1;
}
typedef struct word_reader {xls_semantic_completion* result;const XL6_AstNode* root;int okay,demand;} word_reader;
static int32 wordRead(void* user)
{
    word_reader* reader=user;reader->okay=1;
    for(unsigned pass=0;pass<512 && reader->okay;++pass){
        xls_completion_info info=xlsSemanticCompletionInfo(reader->result);xls_inputs inputs;
        reader->okay=info.available && info.count && info.prefix && !strcmp(info.prefix,"pr") && xlsSemanticCompletionInputs(reader->result,&inputs) && inputs.sources;
        for(size_t i=0;reader->okay && i<info.count;++i){xls_completion_item item;reader->okay=xlsSemanticCompletionItem(reader->result,i,&item) && item.label && !strncmp(item.label,"pr",2) && item.kind;}
        for(size_t i=0;reader->okay && i<inputs.sources;++i){xls_input_source source;reader->okay=xlsSemanticCompletionSource(reader->result,i,&source) && source.path && source.text;}
    }
    if(reader->okay && reader->demand){reader->root=xlsSemanticCompletionAst(reader->result);reader->okay=reader->root!=NULL;}
    else if(reader->okay)reader->okay=xlsSemanticCompletionUnitWork(reader->result).astMaterialized==0;
    char* json=reader->okay ? xlsSemanticCompletionJson(reader->result,NULL) : NULL;reader->okay=reader->okay && json; xlsSemanticStringFree(json);
    xlsSemanticCompletionDestroy(reader->result);return 0;
}
static int independentReaders(void)
{
    const char* a="func main(...) as int\n p\n return 0\nend\nfunc prefixOne(int value) as int\n return value\nend\n";const char* b="func main(...) as int\n pr\n return 0\nend\nfunc prefixOne(int value) as int\n return value\nend\n";
    char* path=xrtPathAbs("completion-unit-readers.xl");CHECK(path);xls_text* old=xlsTextCreate(a,strlen(a),8u*1024u*1024u,NULL);xls_text* next=xlsTextCreate(b,strlen(b),8u*1024u*1024u,NULL);CHECK(old && next);
    xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,old,1,1};xls_semantic_completion* first=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(a,"\n p"),NULL,NULL,NULL,0);CHECK(first);
    doc.text=next;doc.revision=2;xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",cursorAt(b,"\n pr"),NULL,NULL,NULL,0);CHECK(result && xlsSemanticCompletionUnitWork(result).projected);
    xlsSemanticCompletionDestroy(first);xlsCompletionSessionDestroy(session);xlsTextRelease(old);xlsTextRelease(next);xrtFree(path);
    for(unsigned phase=0;phase<2;++phase){
        word_reader readers[4];xthread* threads[4];
        for(size_t i=0;i<4;++i){readers[i]=(word_reader){.result=xlsSemanticCompletionRetain(result),.demand=(int)phase};threads[i]=xrtThreadCreate(wordRead,&readers[i],0);CHECK(threads[i]);}
        for(size_t i=0;i<4;++i){int wait=xrtThreadWait(threads[i]);if(wait!=XWAIT_OK || !readers[i].okay)fprintf(stderr,"word reader failed phase=%u reader=%zu count=%zu available=%d\n",phase,i,xlsSemanticCompletionInfo(result).count,xlsSemanticCompletionInfo(result).available);CHECK(wait==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);if(phase)CHECK(readers[i].root==readers[0].root);}
        CHECK(xlsSemanticCompletionUnitWork(result).astMaterialized==phase);
    }
    xlsSemanticCompletionDestroy(result);return 1;
}

typedef struct word_lease_release {
    xls_semantic_completion* producer;xls_input_batch* batch;xls_syntax *before,*after;unsigned calls;int okay;
} word_lease_release;
static int wordReleaseInputs(void* user)
{
    word_lease_release* leases=user;if(leases->calls++)return 0;
    leases->okay=xlsSemanticCompletionValidate(leases->producer);
    xlsSemanticCompletionDestroy(leases->producer);xlsInputBatchDestroy(leases->batch);xlsSyntaxRelease(leases->before);xlsSyntaxRelease(leases->after);
    leases->producer=NULL;leases->batch=NULL;leases->before=leases->after=NULL;return 0;
}
static int inputBinding(void)
{
    const char* a="func main(...) as int\n int prefixLocal=0\n p\n return 0\nend\n";
    const char* b="func main(...) as int\n int prefixLocal=0\n pr\n return 0\nend\n";
    const char* bad="func main(...) as int\n int prefixOther=0\n pr\n return 0\nend\n";
    const char* wrong="func main(...) as int\n int prefixOther=0\n p\n return 0\nend\n";
    char* path=xrtPathAbs("completion-unit-bound-inputs.xl");CHECK(path);
    xls_text *x=xlsTextCreate(a,strlen(a),8u*1024u*1024u,NULL),*y=xlsTextCreate(b,strlen(b),8u*1024u*1024u,NULL),*z=xlsTextCreate(bad,strlen(bad),8u*1024u*1024u,NULL),*w=xlsTextCreate(wrong,strlen(wrong),8u*1024u*1024u,NULL),*copy=xlsTextCreate(b,strlen(b),8u*1024u*1024u,NULL);CHECK(x && y && z && w && copy);
    xls_analysis_document original={"entry",path,x,1,1},changed={"entry",path,z,1,2},matching={"entry",path,copy,1,2};
    xls_input_batch* before=xlsInputBatchCreate(NULL,NULL,&original,1,"entry",NULL,NULL,NULL,0);xls_input_batch* after=xlsInputBatchCreate(NULL,before,&changed,1,"entry",NULL,NULL,NULL,0);xls_input_batch* good=xlsInputBatchCreate(NULL,before,&matching,1,"entry",NULL,NULL,NULL,0);CHECK(before && after && good);
    size_t at=cursorAt(a,"\n p"),now=at+1;XL6_CompletionQuery aq={.path=path,.offset=at},bq={.path=path,.offset=now},wq={.path=path,.offset=at},zq={.path=path,.offset=now};xls_parse_work aw={0},bw={0},ww={0},zw={0};
    xls_syntax* ap=xlsSyntaxBuildCursor(path,x,NULL,&aq,&aw);xls_syntax* bp=xlsSyntaxBuildCursor(path,y,ap,&bq,&bw);xls_syntax* wp=xlsSyntaxBuildCursor(path,w,NULL,&wq,&ww);xls_syntax* zp=xlsSyntaxBuildCursor(path,z,wp,&zq,&zw);CHECK(ap && bp && wp && zp);
    xls_semantic_completion* previous=xlsSemanticCompletionCreateBatch(NULL,before,"entry",at,ap,NULL,NULL,NULL,NULL,0);CHECK(previous && xlsSemanticCompletionInfo(previous).available);
    CHECK(!xlsSemanticCompletionTryWordView(previous,after,"entry",ap,bp,NULL,NULL));
    CHECK(!xlsSemanticCompletionTryWordView(previous,after,"entry",wp,zp,NULL,NULL));
    CHECK(!xlsSemanticCompletionTryWordView(previous,good,"entry",wp,bp,NULL,NULL));
    xls_semantic_completion* accepted=xlsSemanticCompletionTryWordView(previous,good,"entry",ap,bp,NULL,NULL);CHECK(accepted && xlsSemanticCompletionUnitWork(accepted).projected && unitParity(accepted,&matching,now));
    xlsSemanticCompletionDestroy(accepted);
    word_lease_release leases={.producer=previous,.batch=good,.before=ap,.after=bp};
    accepted=xlsSemanticCompletionTryWordView(previous,good,"entry",ap,bp,wordReleaseInputs,&leases);
    CHECK(accepted && leases.calls && leases.okay && !leases.producer && !leases.batch && !leases.before && !leases.after);
    CHECK(xlsSemanticCompletionUnitWork(accepted).projected && !xlsSemanticCompletionUnitWork(accepted).astMaterialized && unitParity(accepted,&matching,now));
    xlsSemanticCompletionDestroy(accepted);xlsSyntaxRelease(wp);xlsSyntaxRelease(zp);xl6CompletionFree(&aq);xl6CompletionFree(&bq);xl6CompletionFree(&wq);xl6CompletionFree(&zq);xlsInputBatchDestroy(before);xlsInputBatchDestroy(after);xlsTextRelease(x);xlsTextRelease(y);xlsTextRelease(z);xlsTextRelease(w);xlsTextRelease(copy);xrtFree(path);return 1;
}

int main(void)
{
    if(cursorOriginalMain())return 1;
    if(!inputBinding())return 1;
    if(!wordSequence("","\n",0) || !wordSequence("\xef\xbb\xbf// 中文 😀\r\n","\r\n",0) ||
       !wordSequence("","\n",1) || !wordSequence("\xef\xbb\xbf// 中文 😀\r\n","\r\n",1))return 1;
    if(!wordTemplate("class Box\n int prefixField\n func prefixMethod(int value) as int\n return value\n end\nend\nfunc main(...) as int\n auto box=Box()\n return box.@word@\nend\n","p","pre","box.p") ||
       !wordTemplate("func main(...) as int\n int 中文变量=1\n return @word@\nend\n","中","中文","return 中") ||
       !wordTemplate("typedef Count = int\nfunc prefixChoose(Count value) as Count\n return value\nend\nfunc main(...) as int\n Count prefixValue=prefixChoose(7)\n return @word@\nend\n","p","pre","return p") ||
       !wordTemplate("func identity<T>(T value) as T\n return value\nend\nfunc main(...) as int\n int prefixLocal=identity::<int>(7)\n return @word@\nend\n","p","pre","return p") ||
       !wordTemplate("func closure() as func() as int\n array values=[41]\n return func() as int\n return int(values[0])\n end\nend\nfunc main(...) as int\n auto prefixClosure=closure()\n @word@\n return 0\nend\n","p","pre","\n p") ||
       !proofBoundaries() || !guardedDependency() || !cancellationBoundaries() || !memoAfterWordViews() || !independentReaders())return 1;
    printf("analysis_completion_unit_test passed: %u checks\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
