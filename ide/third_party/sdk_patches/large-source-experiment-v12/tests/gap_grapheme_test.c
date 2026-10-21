#include <windows.h>
int old_probe_buffer(const char*,int,int,int,int);
int new_probe_buffer(const char*,int,int,int,int);
static unsigned long long gapAssertions,gapCases,guardCases,pairCases;
#define GAP_CHECK(v) do {++gapAssertions;if(!(v)){fprintf(stderr,"gap traversal %d: %s; case=%llu\n",__LINE__,#v,gapCases);exit(1);}} while(0)
int countedPointerNext(const char* text,int length,int offset) {
    ++calls;return xuiInternalTextGraphemeNext(text,length,offset);
}
static void physicalCompare(const unsigned char* physical,int length,int gapStart,int gapEnd,int offset) {
    int old=old_probe_buffer((const char*)physical,length,gapStart,gapEnd,offset);
    int current=new_probe_buffer((const char*)physical,length,gapStart,gapEnd,offset);
    ++gapCases;GAP_CHECK(old==current);
    GAP_CHECK(current>=0 && current<=length);
}
static void gapCompare(const unsigned char* text,int length,int gap) {
    unsigned char physical[8192];GAP_CHECK(length+32<=(int)sizeof(physical));
    memcpy(physical,text,(size_t)gap);memset(physical+gap,0xfd,32);
    memcpy(physical+gap+32,text+gap,(size_t)(length-gap));
    for(int offset=-2;offset<=length+2;++offset)physicalCompare(physical,length,gap,gap+32,offset);
}
static uint32_t gapRandom=0xba578ed1u;
static uint32_t nextGapRandom(void){gapRandom^=gapRandom<<13;gapRandom^=gapRandom>>17;gapRandom^=gapRandom<<5;return gapRandom;}
static void gapTraversalTests(void) {
    const char* corpus[]={"", "ABCDEFG", "中文😀 é 👩🏽‍💻 🇨🇳🇫🇷", "각각\r\n\n",
        "क्ष क्‍ष á́́\t終", "\xff\xf0\x9f\x80\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80", "A\0中😀"};
    int lengths[]={0,7,0,0,0,13,10};
    for(size_t i=0;i<sizeof(corpus)/sizeof(*corpus);++i) {
        int n=lengths[i] ? lengths[i] : (int)strlen(corpus[i]);
        GAP_CHECK(n>=0 && n<=512);
        for(int gap=0;gap<=n;++gap)gapCompare((const unsigned char*)corpus[i],n,gap);
        for(int offset=-2;offset<=n+2;++offset)physicalCompare((const unsigned char*)corpus[i],n,0,0,offset);
    }
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b) {
        unsigned char text[2]={(unsigned char)a,(unsigned char)b};
        for(int gap=0;gap<=2;++gap)gapCompare(text,2,gap);
        ++pairCases;
    }
    unsigned char text[512];
    for(int trial=0;trial<4096;++trial) {
        int n=(int)(nextGapRandom()%257);
        for(int i=0;i<n;++i)text[i]=(unsigned char)nextGapRandom();
        for(int pick=0;pick<8;++pick)gapCompare(text,n,(int)(nextGapRandom()%(unsigned)(n+1)));
    }
    SYSTEM_INFO info;GetSystemInfo(&info);size_t page=info.dwPageSize;
    unsigned char* pages=VirtualAlloc(NULL,page*5,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);GAP_CHECK(pages);
    DWORD saved;
    for(int p=0;p<5;p+=2)GAP_CHECK(VirtualProtect(pages+(size_t)p*page,page,PAGE_NOACCESS,&saved));
    for(size_t i=0;i<sizeof(corpus)/sizeof(*corpus);++i) {
        int n=lengths[i] ? lengths[i] : (int)strlen(corpus[i]);
        GAP_CHECK(n>=0 && n<=512 && (size_t)n<=page);
        for(int gap=0;gap<=n;++gap) {
            unsigned char* physical=pages+page*2-(size_t)gap;
            memcpy(physical,corpus[i],(size_t)gap);
            memcpy(pages+page*3,corpus[i]+gap,(size_t)(n-gap));
            for(int offset=-2;offset<=n+2;++offset) {physicalCompare(physical,n,gap,gap+(int)page,offset);++guardCases;}
        }
        unsigned char* physical=pages+page*4-(size_t)n;memcpy(physical,corpus[i],(size_t)n);
        for(int offset=-2;offset<=n+2;++offset){physicalCompare(physical,n,0,0,offset);++guardCases;}
    }
    GAP_CHECK(VirtualFree(pages,0,MEM_RELEASE));
    printf("gap traversal passed: %llu assertions; %llu next cases; %llu byte pairs; %llu guard-page cases; arbitrary gap byte splits and logical retries\n",gapAssertions,gapCases,pairCases,guardCases);fflush(stdout);
}
static double gapClock(void) {LARGE_INTEGER q,f;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&q);return q.QuadPart*1000.0/f.QuadPart;}
static void gapLongObservation(void) {
    const char* motif="中文😀 é 0123456789 ";size_t total=16777216,n=strlen(motif);
    char* text=malloc(total+1);GAP_CHECK(text);size_t used=0;
    while(used+n<=total){memcpy(text+used,motif,n);used+=n;}memset(text+used,'A',total-used);text[total]=0;
    xui_code_document docs[2]={0};GAP_CHECK(old_xuiCodeDocumentCreate(&docs[0])==XUI_OK && new_xuiCodeDocumentCreate(&docs[1])==XUI_OK);
    GAP_CHECK(old_xuiCodeDocumentSetText(docs[0],text)==XUI_OK && new_xuiCodeDocumentSetText(docs[1],text)==XUI_OK);
    int columns[2]={0},lines[2]={0};double times[2]={0};
    for(int which=0;which<2;++which) {
        double started=gapClock();int code=which ? new_xuiCodeDocumentOffsetToLineColumn(docs[which],(int)total,&lines[which],&columns[which]) : old_xuiCodeDocumentOffsetToLineColumn(docs[which],(int)total,&lines[which],&columns[which]);
        times[which]=gapClock()-started;GAP_CHECK(code==XUI_OK);
    }
    GAP_CHECK(columns[0]==columns[1] && lines[0]==lines[1]);
    printf("{\"event\":\"document-column-observation\",\"bytes\":%llu,\"column\":%d,\"oldMs\":%.3f,\"newMs\":%.3f,\"diagnosticTraversalCounters\":true,\"singleObservation\":true}\n",(unsigned long long)total,columns[0],times[0],times[1]);fflush(stdout);
    old_xuiCodeDocumentDestroy(docs[0]);new_xuiCodeDocumentDestroy(docs[1]);free(text);
}
int main(void) {GAP_CHECK(positionRegression()==0);gapTraversalTests();gapLongObservation();return 0;}
