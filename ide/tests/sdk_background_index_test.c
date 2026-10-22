/* Component oracle for the exact SDK annotation source. XGE remains the sole
 * XRT owner; the local annotation module is not passed to GUI/DLL controls. */
#undef XUI_DLL
#include <xui.h>
#include <stdio.h>
#include <string.h>
static size_t reallocCalls,failCall;
static void* reallocTracked(void* memory,size_t bytes)
{
    ++reallocCalls;if(failCall==reallocCalls) return NULL;return xrtRealloc(memory,bytes);
}
#ifdef xrtRealloc
#undef xrtRealloc
#endif
#define xrtRealloc reallocTracked
#ifndef XIDE_SDK_ANNOTATION_SOURCE
#define XIDE_SDK_ANNOTATION_SOURCE "../third_party/xge_sdk/provenance/compatibility-patches/xui_code_annotation.c"
#endif
#include XIDE_SDK_ANNOTATION_SOURCE
#undef xrtRealloc

static unsigned checks;
#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"background_index_test:%d %s\n",__LINE__,#v);return 0;}}while(0)
typedef struct range {int start,end,id,style;} range;
typedef struct collect {int cells[512],count,lastEnd,error;} collect;
static int visit(void* user,int start,int end)
{
    collect* out=user;++out->count;
    if(out->error) return XUI_ERROR_BACKEND_FAILED;
    if(start<0 || end<=start || end>512 || (out->count>1 && start<=out->lastEnd)) return XUI_ERROR_INVALID_ARGUMENT;
    for(int i=start;i<end;++i) out->cells[i]=1;
    out->lastEnd=end;return XUI_OK;
}
static int compare(xui_code_annotation_store store,const range* model,size_t count,int left,int right)
{
    int expected[512]={0};collect out={0};size_t allocations=reallocCalls;
    for(size_t i=0;i<count;++i) if(model[i].style==XUI_CODE_INDICATOR_BACKGROUND && model[i].id!=XUI_CODE_INDICATOR_SEARCH_RESULT) {
        int start=model[i].start>left ? model[i].start : left,end=model[i].end<right ? model[i].end : right;
        for(int j=start;j<end;++j) expected[j]=1;
    }
    CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,left,right,visit,&out)==XUI_OK);
    CHECK(allocations==reallocCalls && !memcmp(expected,out.cells,sizeof(expected)));return 1;
}
static unsigned randomState=7531;
static unsigned randomValue(void) {randomState=randomState*1664525u+1013904223u;return randomState;}
static int counter(void* user,int start,int end)
{
    int* values=user;if(start<values[1] || end<=start) return XUI_ERROR_INVALID_ARGUMENT;
    ++values[0];values[1]=end;return XUI_OK;
}
static int verify(void)
{
    xui_code_annotation_store store=NULL;CHECK(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);
    range model[4096]={{0}};size_t count=0;
    CHECK(compare(store,model,count,0,512));
    const range initial[]={{10,20,1005,5},{18,30,1005,5},{30,40,1005,5},{2,6,1005,5},{4,8,1005,5},{10,20,1005,5},{0,100,7,5},{0,100,1006,2}};
    for(size_t i=0;i<sizeof(initial)/sizeof(*initial);++i) {
        model[count++]=initial[i];CHECK(xuiCodeAnnotationSetIndicator(store,initial[i].id,initial[i].style,initial[i].start,initial[i].end,0,0)==XUI_OK);
    }
    CHECK(compare(store,model,count,0,512) && compare(store,model,count,5,32) && compare(store,model,count,8,10));
    collect out={0};CHECK(__xuiCodeAnnotationVisitBackgroundRanges(NULL,0,1,visit,&out)==XUI_ERROR_INVALID_ARGUMENT && !out.count);
    CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,1,1,visit,&out)==XUI_ERROR_INVALID_ARGUMENT && !out.count);
    CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,-1,1,visit,&out)==XUI_ERROR_INVALID_ARGUMENT && !out.count);
    CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,0,1,NULL,&out)==XUI_ERROR_INVALID_ARGUMENT && !out.count);
    out.error=1;CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,0,512,visit,&out)==XUI_ERROR_BACKEND_FAILED && out.count==1);
    CHECK(xuiCodeAnnotationTrackEdit(store,0,0,3,0,0,0)==XUI_OK);
    for(size_t i=0;i<count;++i) {model[i].start+=3;model[i].end+=3;}
    CHECK(compare(store,model,count,0,512));
    xuiCodeAnnotationStoreClear(store);count=0;
    for(int operation=0;operation<1800;++operation) {
        unsigned action=randomValue()%5;
        if(action<3) {
            int start=(int)(randomValue()%480),end=start+1+(int)(randomValue()%32),id=(int)(randomValue()%5);id=id==4 ? 7 : 1005+id;
            int style=randomValue()%3 ? XUI_CODE_INDICATOR_BACKGROUND : XUI_CODE_INDICATOR_UNDERLINE;
            CHECK(count<4096 && xuiCodeAnnotationSetIndicator(store,id,style,start,end,0,0)==XUI_OK);model[count++]=(range){start,end,id,style};
        } else {
            int id=1005+(int)(randomValue()%4),start=(int)(randomValue()%480),end=start+1+(int)(randomValue()%32),partial=action==3;
            int removed=0;size_t used=0;
            for(size_t i=0;i<count;++i) {
                if(model[i].id==id && (!partial || (model[i].start<end && start<model[i].end))) ++removed;
                else model[used++]=model[i];
            }
            count=used;int result=partial ? xuiCodeAnnotationClearIndicator(store,id,start,end) : xuiCodeAnnotationClearIndicators(store,id);
            CHECK(result==(removed ? XUI_OK : XUI_ERROR_UNSUPPORTED));
        }
        int left=(int)(randomValue()%511),right=left+1+(int)(randomValue()%(512-left));
        CHECK(compare(store,model,count,left,right));
    }
    xuiCodeAnnotationStoreDestroy(store);
    /* Fail both reserve stages and a later capacity expansion. The accepted
     * model/query remain usable and no partially published indicator appears. */
    for(int fail=1;fail<=2;++fail) {
        CHECK(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);failCall=reallocCalls+(size_t)fail;
        CHECK(xuiCodeAnnotationSetIndicator(store,1005,5,10,11,0,0)==XUI_ERROR_OUT_OF_MEMORY);failCall=0;
        CHECK(compare(store,model,0,0,512));CHECK(xuiCodeAnnotationSetIndicator(store,1005,5,10,11,0,0)==XUI_OK);
        range one={10,11,1005,5};CHECK(compare(store,&one,1,0,512));xuiCodeAnnotationStoreDestroy(store);
    }
    CHECK(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);
    for(int i=0;i<16;++i) CHECK(xuiCodeAnnotationSetIndicator(store,1005,5,i*3,i*3+1,0,0)==XUI_OK);
    failCall=reallocCalls+2;CHECK(xuiCodeAnnotationSetIndicator(store,1005,5,99,100,0,0)==XUI_ERROR_OUT_OF_MEMORY);failCall=0;
    int values[2]={0};CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,0,101,counter,values)==XUI_OK && values[0]==16 && values[1]==46);
    xuiCodeAnnotationStoreDestroy(store);
    for(int reverse=0;reverse<2;++reverse) {
        CHECK(xuiCodeAnnotationStoreCreate(&store)==XUI_OK);
        for(int i=0;i<65536;++i) {int at=(reverse ? 65535-i : i)*2;CHECK(xuiCodeAnnotationSetIndicator(store,1005,5,at,at+1,0,0)==XUI_OK);}
        size_t allocations=reallocCalls;values[0]=values[1]=0;
        CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,0,131072,counter,values)==XUI_OK && values[0]==65536 && values[1]==131071 && allocations==reallocCalls);
        CHECK(xuiCodeAnnotationClearIndicators(store,1005)==XUI_OK);values[0]=values[1]=0;
        CHECK(__xuiCodeAnnotationVisitBackgroundRanges(store,0,131072,counter,values)==XUI_OK && values[0]==0);
        xuiCodeAnnotationStoreDestroy(store);
    }
    return 1;
}
int main(void)
{if(!verify()) return 1;printf("background_index_test passed: %u checks; independent clipped-union oracle, ordering, find/style isolation, edits, reserve failures, 65536 ranges and allocation-free queries.\n",checks);return 0;}
