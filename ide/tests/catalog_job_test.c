#include "services/catalog_job.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(e) do { ++checks;if(!(e)){fprintf(stderr,"catalog_job_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;} }while(0)
static int waitFor(xide_catalog_job* job,xide_catalog_result* result)
{
    ULONGLONG until=GetTickCount64()+30000;
    while(GetTickCount64()<until){if(xideCatalogJobTake(job,result))return 1;Sleep(1);}return 0;
}
int main(int argc,char** argv)
{
    int failed=0;xide_catalog_job* job=NULL;xide_catalog_result result={0};
    CHECK(argc==2);
    job=xideCatalogJobStart(argv[1]);CHECK(job && waitFor(job,&result));
    CHECK(!*result.error && result.catalog.count>19000 && result.visible==result.catalog.count);
    CHECK(result.nodeCount>(int)result.catalog.count && result.lookup && result.details);
    for(size_t i=0;i<result.catalog.count;++i)CHECK(result.details[i] && strstr(result.details[i],result.catalog.entries[i].signature));
    printf("catalog worker: %zu entries, %.0f ms\n",result.catalog.count,result.loadMs);
    xideCatalogJobCancel(job);job=NULL;
    /* Ownership has moved: a released job cannot invalidate strings or the index. */
    CHECK(xideCatalogLookupId(result.lookup,result.catalog.entries[0].id)==&result.catalog.entries[0]);
    xui_tree_view_node_t* nodes=NULL;int count=0;size_t visible=0;
    CHECK(xideCatalogTreePrepare(&result.catalog,"math sqrt",&nodes,&count,&visible)==XUI_OK && visible>0 && visible<20);free(nodes);
    xideCatalogResultClear(&result);
    job=xideCatalogJobStart("Z:/xlang-nonexistent-catalog-20261108.json");CHECK(job && waitFor(job,&result) && *result.error);xideCatalogJobCancel(job);job=NULL;xideCatalogResultClear(&result);
    /* Cancelled jobs carry no UI/app pointer and reclaim their own results. */
    for(int i=0;i<12;++i){job=xideCatalogJobStart(argv[1]);CHECK(job);xideCatalogJobCancel(job);job=NULL;}
    Sleep(200);
cleanup:
    xideCatalogJobCancel(job);xideCatalogResultClear(&result);
    if(!failed)printf("catalog_job_test passed: %d checks, owned handoff, lookup, search, missing-file and cancellation\n",checks);
    return failed;
}
