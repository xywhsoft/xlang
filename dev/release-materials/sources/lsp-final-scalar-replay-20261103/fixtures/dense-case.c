static char* denseCanonicalSource(unsigned functions,unsigned generation,int imported)
{
    xstrbuf text={0};int okay=xrtStrBufAppend(&text,xrtStrView("\xef\xbb\xbf// 中文😀\r\n\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n"));
    for(unsigned i=0;okay && i<functions;++i){
        const char* expression=i%2 ? "identity(local)" : "local";
        okay=xrtStrBufAppendFormat(&text,"func work_%u(int value) as int\r\n int local = value+%u\r\n",i,i);
        if(!i && generation)okay=okay && xrtStrBufAppendFormat(&text," int inserted = %u\r\n",generation);
        okay=okay && xrtStrBufAppend(&text,xrtStrView(" if local > 0 then\r\n"));
        for(unsigned j=0;okay && j<12;++j)okay=xrtStrBufAppendFormat(&text," local = %s+1\r\n",expression);
        okay=okay && xrtStrBufAppend(&text,xrtStrView(" else\r\n"));
        for(unsigned j=0;okay && j<12;++j)okay=xrtStrBufAppendFormat(&text," local = %s-1\r\n",expression);
        okay=okay && xrtStrBufAppend(&text,xrtStrView(!i && generation ? " end\r\n return local+inserted\r\nend\r\n" : " end\r\n return local\r\nend\r\n"));
    }
    okay=okay && xrtStrBufAppendFormat(&text,"export func compute(int value) as int\r\n return identity(value)+%u\r\nend\r\n",generation);
    if(!imported)okay=okay && xrtStrBufAppend(&text,xrtStrView("func main(...) as int\n return compute(3)\nend\n"));
    char* result=okay ? xrtStrDup(text.Data) : NULL;xrtStrBufFree(&text);return result;
}
static int denseCanonical(int imported)
{
    char* path=xrtPathAbs("canonical-dense-中文😀.xl");char* dependency=imported ? xrtPathAbs("canonical-dense-dependency.xl") : NULL;
    xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(path && session && (!imported || dependency));
    xls_analysis_document docs[2]={{"entry",path,NULL,1,1},{"dependency",dependency,NULL,1,1}};
    if(imported)CHECK(setBody(&docs[0],"import \"canonical-dense-dependency.xl\" as lib\nfunc main(...) as int\n return lib.compute(3)\nend\n"));
    for(unsigned generation=0;generation<3;++generation){
        char* text=denseCanonicalSource(512,generation,imported);CHECK(text && setBody(&docs[imported ? 1 : 0],text));xrtFree(text);
        char error[256];xls_semantic_check* result=xlsCheckSessionCheck(session,docs,imported ? 2 : 1,"entry",1,NULL,NULL,error,sizeof(error));
        if(!result)fprintf(stderr,"dense canonical:%s\n",error);
        CHECK(result && xlsSemanticCheckInfo(result).accepted && completeCanonical(result,docs,imported ? 2 : 1));
        xls_index_canonical_work work=xlsSemanticCheckCanonicalWork(result);xls_sem_final_work final=xlsSemanticCheckFinalWork(result);
        CHECK(!work.fallbacks && work.retainedBytes<=8u*1024u*1024u);
        printf("dense-canonical-import%d-gen%u:reused=%llu values=%llu calls=%llu final=%llu plans=%llu declined=%llu\n",imported,generation,
            (unsigned long long)work.reused,(unsigned long long)work.valuesReused,(unsigned long long)work.calls,
            (unsigned long long)final.typeReused,(unsigned long long)final.planReused,(unsigned long long)final.declined);
        if(generation==2){
            if(expectNoCanonical)CHECK(!work.captured && !work.reused && !work.valuesReused && !work.retainedBytes);
            else CHECK(work.reused>=512 && work.valuesReused>=65000 && work.calls<256);
            CHECK(final.typeReused>=512 && final.planReused==final.typeReused);
        }
        xlsSemanticCheckDestroy(result);
    }
    xlsCheckSessionDestroy(session);xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xrtFree(path);xrtFree(dependency);return 1;
}
