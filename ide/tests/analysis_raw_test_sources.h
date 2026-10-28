#ifndef XLS_RAW_TEST_SOURCES_H
#define XLS_RAW_TEST_SOURCES_H
/* Independent language input shared by the raw ownership and native semantic
 * differential tests. Original parser/checker are the only language oracle. */
static int rawTestSources(const char* label,int cursor,char** beforePath,char** afterPath)
{
    xstrbuf a={0},b={0},directory={0};int okay=0;char* folder=NULL;
    if(!xrtStrBufAppendFormat(&directory,"build/%s-%llu-%u",label,(unsigned long long)xrtClock(),(unsigned)cursor) ||
       !(folder=xrtPathAbs(directory.Data)) || !xrtDirCreateAll(folder))goto done;
    *beforePath=xrtPathJoin(folder,"before.xl");*afterPath=xrtPathJoin(folder,"after.xl");
    if(!*beforePath || !*afterPath || !xrtStrBufAppend(&a,xrtStrView("\xef\xbb\xbf// 中文😀\r\n\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n")))goto done;
    for(unsigned i=0;i<32;++i){
        if(!xrtStrBufAppendFormat(&a,"func work_%u(int value) as int\r\n int local=value+%u\r\n if local>0 then\r\n",i,i))goto done;
        for(unsigned j=0;j<12;++j)if(!xrtStrBufAppend(&a,xrtStrView(i%2 ? " local=identity(local)+1\r\n" : " local=local+1\r\n")))goto done;
        if(!xrtStrBufAppend(&a,xrtStrView(" else\r\n")))goto done;
        for(unsigned j=0;j<12;++j)if(!xrtStrBufAppend(&a,xrtStrView(i%2 ? " local=identity(local)-1\r\n" : " local=local-1\r\n")))goto done;
        if(!xrtStrBufAppend(&a,xrtStrView(" end\r\n return local\r\nend\r\n")))goto done;
    }
    if(!xrtStrBufAppend(&a,xrtStrView("export func compute(int value) as int\r\n return identity(value)\r\nend\r\nfunc main(...) as int\r\n int prefixValue=7\r\n return pref\r\nend\r\n")))goto done;
    const char* at=strstr(a.Data,cursor ? " return pref\r\n" : " if local>0 then\r\n");
    if(!at || !xrtStrBufAppend(&b,(xstrview){a.Data,(size_t)(at-a.Data)}) ||
       !xrtStrBufAppend(&b,xrtStrView(cursor ? " int inserted=prefixValue+9\r\n" : " int inserted=local+9\r\n")) ||
       !xrtStrBufAppend(&b,xrtStrView(at)))goto done;
    okay=xrtFileWriteAtomic(*beforePath,(xbytesview){(const unsigned char*)a.Data,a.Size}) &&
        xrtFileWriteAtomic(*afterPath,(xbytesview){(const unsigned char*)b.Data,b.Size});
done:
    if(!okay){xrtFree(*beforePath);xrtFree(*afterPath);*beforePath=*afterPath=NULL;}
    xrtFree(folder);xrtStrBufFree(&directory);xrtStrBufFree(&a);xrtStrBufFree(&b);return okay;
}
#endif
