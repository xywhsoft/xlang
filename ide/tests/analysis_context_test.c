#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t allocations,live;
static int failAllocation;
static void* contextAllocate(size_t bytes)
{ ++allocations; if(failAllocation) return NULL; void* value=xrtMalloc(bytes); if(value) ++live; return value; }
static void contextFree(void* value) { if(value) --live; xrtFree(value); }
/* Exercise the actual single-allocation failure seam and forced hash collision.
 * This fixture owns its private implementation; no second runtime is linked. */
#define xrtMalloc contextAllocate
#define xrtFree contextFree
#include "analysis/context.c"
#undef xrtMalloc
#undef xrtFree
static int checks;
#define CHECK(call) do { ++checks; if(!(call)) { fprintf(stderr,"analysis_context_test:%d: %s\n",__LINE__,#call); return 1; } } while(0)
#define CAP (16u*1024u*1024u)
static xls_context_spec example(void)
{
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project="/work/app.xlp",.configuration="debug",
        .entry="/work/中文 main.xl",.directory="/work",.analysisProfile="frontend-sha:abi:options",
        .targetKind="exe",.packagePath="/packages;/secondary"};
    return spec;
}
int main(void)
{
    enum xls_context_error error;
    xls_context_spec spec=example();
    char member[]="/work/中文 main.xl",inc[]="/work/inc";
    const char* members[]={member,"/work/helper.xl"}; const char* includes[]={inc,"/work/inc2",inc};
    spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};
    spec.lists[XLS_CONTEXT_INCLUDES]=(xls_context_strings){includes,3};
    xls_context* a=xlsContextCreate(&spec,CAP,&error);
    CHECK(a && error==XLS_CONTEXT_OK && live==1 && allocations==1);
    xls_context* b=xlsContextCreate(&spec,CAP,&error);
    CHECK(b && xlsContextEqual(a,b) && xlsContextHash(a)==xlsContextHash(b));
    CHECK(xlsContextEqual(a,a) && !xlsContextEqual(NULL,NULL) && !xlsContextEqual(a,NULL));
    CHECK(!xlsContextSpec(NULL) && !xlsContextBytes(NULL) && !xlsContextHash(NULL));
    CHECK(!xlsContextRetain(NULL)); xlsContextRelease(NULL);
    size_t before=allocations; const xls_context_spec* v=xlsContextSpec(a);
    CHECK(v && v!=&spec && v->entry!=spec.entry && !strcmp(v->entry,spec.entry));
    CHECK(v->lists[XLS_CONTEXT_INCLUDES].count==3 && v->lists[XLS_CONTEXT_INCLUDES].items!=includes);
    CHECK(!strcmp(v->lists[XLS_CONTEXT_INCLUDES].items[0],inc) && !strcmp(v->lists[XLS_CONTEXT_INCLUDES].items[2],inc));
    member[0]='!'; inc[0]='!'; members[1]="changed";
    CHECK(!strcmp(v->lists[XLS_CONTEXT_MEMBERS].items[0],"/work/中文 main.xl") && !strcmp(v->lists[XLS_CONTEXT_MEMBERS].items[1],"/work/helper.xl"));
    CHECK(!strcmp(v->lists[XLS_CONTEXT_INCLUDES].items[0],"/work/inc") && xlsContextEqual(a,b) && allocations==before);
    xls_context* retained=xlsContextRetain(a); xlsContextRelease(a); a=retained;
    CHECK(xlsContextEqual(a,b) && live==2 && allocations==before);
    size_t exact=xlsContextBytes(a); CHECK(exact>sizeof(*a) && exact<CAP);
    xls_context* c=xlsContextCreate(v,exact,&error); CHECK(c && xlsContextEqual(a,c)); xlsContextRelease(c);
    CHECK(!xlsContextCreate(v,exact-1,&error) && error==XLS_CONTEXT_LIMIT);
    CHECK(!xlsContextCreate(v,0,&error) && error==XLS_CONTEXT_LIMIT);
    for(size_t field=0;field<SCALARS;++field) {
        xls_context_spec changed=*v;
        const char** values[]={&changed.project,&changed.configuration,&changed.entry,&changed.directory,
            &changed.analysisProfile,&changed.targetKind,&changed.packagePath};
        *values[field]="different"; c=xlsContextCreate(&changed,CAP,&error);
        CHECK(c && !xlsContextEqual(a,c));
        c->hash=a->hash; CHECK(!xlsContextEqual(a,c)); xlsContextRelease(c);
    }
    for(size_t field=0;field<XLS_CONTEXT_LIST_COUNT;++field) {
        xls_context_spec changed=*v; const char* value[]={"different"};
        changed.lists[field]=(xls_context_strings){value,1}; c=xlsContextCreate(&changed,CAP,&error);
        CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c);
    }
    xls_context_spec changed=*v; const char* reversed[]={"/work/inc2","/work/inc","/work/inc"};
    changed.lists[XLS_CONTEXT_INCLUDES]=(xls_context_strings){reversed,3};
    c=xlsContextCreate(&changed,CAP,&error); CHECK(c && !xlsContextEqual(a,c)); xlsContextRelease(c);
    /* Equal string concatenation must not merge fields or list boundaries. */
    changed=example(); const char* split[]={"a","b"}; changed.lists[0]=(xls_context_strings){split,2};
    c=xlsContextCreate(&changed,CAP,&error); CHECK(c);
    changed.lists[0].count=1; changed.lists[1]=(xls_context_strings){split+1,1};
    xls_context* d=xlsContextCreate(&changed,CAP,&error); CHECK(d);
    d->hash=c->hash; CHECK(!xlsContextEqual(c,d)); xlsContextRelease(c); xlsContextRelease(d);
    changed=example(); changed.packagePath=NULL; c=xlsContextCreate(&changed,CAP,&error); CHECK(c && !xlsContextSpec(c)->packagePath);
    changed.packagePath=""; d=xlsContextCreate(&changed,CAP,&error); CHECK(d && xlsContextSpec(d)->packagePath && !*xlsContextSpec(d)->packagePath);
    d->hash=c->hash; CHECK(!xlsContextEqual(c,d)); xlsContextRelease(c); xlsContextRelease(d);
    changed=example(); changed.configuration=NULL; c=xlsContextCreate(&changed,CAP,&error); CHECK(c);
    changed.configuration=""; d=xlsContextCreate(&changed,CAP,&error); CHECK(d && xlsContextEqual(c,d));
    xlsContextRelease(c); xlsContextRelease(d);
    failAllocation=1; CHECK(!xlsContextCreate(v,CAP,&error) && error==XLS_CONTEXT_MEMORY && live==2); failAllocation=0;
    before=allocations;
    CHECK(!xlsContextCreate(NULL,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    changed=example(); changed.kind=0; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    changed=example(); changed.project=""; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    changed=example(); changed.analysisProfile=""; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    changed=example(); changed.entry="\xc0\x80"; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ENCODING);
    changed=example(); changed.lists[0]=(xls_context_strings){NULL,1}; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    const char* nulls[]={NULL}; changed.lists[0].items=nulls; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    const char* invalid[]={"\xed\xa0\x80"}; changed.lists[0].items=invalid; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ENCODING);
    changed=example(); changed.lists[0].count=(size_t)-1; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_LIMIT);
    CHECK(allocations==before);
    char* large=malloc(CONTEXT_STRING+2); CHECK(large); memset(large,'a',CONTEXT_STRING+1); large[CONTEXT_STRING+1]=0;
    changed=example(); changed.entry=large; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_LIMIT);
    large[CONTEXT_STRING]=0; c=xlsContextCreate(&changed,CAP,&error); CHECK(c && strlen(xlsContextSpec(c)->entry)==CONTEXT_STRING); xlsContextRelease(c);
    const char** many=malloc(CONTEXT_ITEMS*sizeof(*many)); CHECK(many);
    for(size_t i=0;i<CONTEXT_ITEMS;++i) many[i]="";
    changed=example(); changed.lists[0]=(xls_context_strings){many,CONTEXT_ITEMS}; c=xlsContextCreate(&changed,CAP,&error);
    CHECK(c && xlsContextSpec(c)->lists[0].count==CONTEXT_ITEMS); xlsContextRelease(c);
    changed.lists[1]=(xls_context_strings){many,1}; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_LIMIT);
    changed.lists[1].count=0; changed.lists[0].count=64; for(size_t i=0;i<64;++i) many[i]=large;
    CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_LIMIT); free(many); free(large);
    changed=(xls_context_spec){.kind=XLS_CONTEXT_XLANG,.entry="/single.xl",.directory="/",.analysisProfile="native",.targetKind="exe"};
    c=xlsContextCreate(&changed,CAP,&error); CHECK(c && !*xlsContextSpec(c)->project && !*xlsContextSpec(c)->configuration);
    changed.kind=XLS_CONTEXT_C; d=xlsContextCreate(&changed,CAP,&error); CHECK(d && !xlsContextEqual(c,d)); xlsContextRelease(d);
    changed.project="/app.xlp"; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    changed.project=NULL; changed.configuration="dev"; CHECK(!xlsContextCreate(&changed,CAP,&error) && error==XLS_CONTEXT_ARGUMENT);
    xlsContextRelease(c); xlsContextRelease(a); xlsContextRelease(b); CHECK(!live);
    printf("analysis_context_test passed: %d checks; single allocation, collision, capacity and ownership verified\n",checks); return 0;
}
