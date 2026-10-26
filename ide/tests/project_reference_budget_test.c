#include "services/project_references.c"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_reference_budget_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static void clearRows(xarray* rows)
{for(size_t i=0;i<rows->Count;++i){result_row* r=(result_row*)rows->Data+i;xrtFree(r->path);free(r->key);}xrtArrayUnit(rows);}
static int fill(xarray* rows,const char* path,size_t count)
{
 if(!xrtArrayInit(rows,sizeof(result_row)) || !xrtArrayReserve(rows,count))return 0;
 for(size_t i=0;i<count;++i){result_row r={.path=xrtStrDup(path),.key=xidePathNativeWide(path),.start={(int)i,0,(int)i},.end={(int)i,1,(int)i+1}};if(!r.path || !r.key || !xrtArrayPush(rows,&r)){xrtFree(r.path);free(r.key);return 0;}}return 1;
}
static int receiveBudget(const xvalue* v,void* user)
{int* okay=user;xstrview id={0};*okay=xrtValueGetString(get(v,"id"),&id) && id.Size==65535 && id.Data[0]==1 && id.Data[65534]==1 && xrtValueCount(get(v,"result"))==512;return *okay;}
static int run(void)
{
 char name[181];memset(name,'a',180);name[180]=0;char relative[256];snprintf(relative,sizeof(relative),"build/reference-budget/%s.xl",name);char* path=xrtPathAbs(relative);CHECK(path);wchar_t* native=xidePathNativeWide(path);CHECK(native);size_t per=strlen(path)+1+(wcslen(native)+1)*sizeof(wchar_t)+2*sizeof(result_row);size_t count=(RESULT_BYTES-1024u*1024u)/per;free(native);CHECK(count>10000 && count<100000);
 xarray rows={0};reference_output out={0};CHECK(fill(&rows,path,count));CHECK(!locations(&rows,&out));CHECK(out.locations && xrtValueCount(out.locations)==count);
 /* The original framer independently proves this payload cannot be emitted.
    New query execution must turn locations()==0 into RequestFailed instead. */
 xvalue* rpc=xrtValueObject();CHECK(rpc && put(rpc,"jsonrpc",xrtValueString(sv("2.0"))) && put(rpc,"id",xrtValueInt(1)));CHECK(xrtValueObjectSet(rpc,sv("result"),out.locations));xide_model_error error={0};size_t bytes=0;char* frame=xideLspFrame(rpc,&bytes,&error);CHECK(!frame && error.code);xrtValueRelease(rpc);xrtValueRelease(out.locations);out.locations=NULL;clearRows(&rows);
 CHECK(fill(&rows,path,512) && locations(&rows,&out));char* id=xrtMalloc(65536);CHECK(id);memset(id,1,65535);id[65535]=0;rpc=xrtValueObject();CHECK(rpc && put(rpc,"jsonrpc",xrtValueString(sv("2.0"))) && put(rpc,"id",xrtValueString((xstrview){id,65535})) && xrtValueObjectSet(rpc,sv("result"),out.locations));error=(xide_model_error){0};frame=xideLspFrame(rpc,&bytes,&error);CHECK(frame && bytes<XIDE_LSP_BODY_BYTES && !error.code);xide_lsp_stream stream={0};int received=0;CHECK(xideLspStreamFeed(&stream,frame,bytes,receiveBudget,&received,&error) && received && xideLspStreamEnd(&stream,&error));xideLspStreamClear(&stream);free(frame);xrtFree(id);xrtValueRelease(rpc);xrtValueRelease(out.locations);clearRows(&rows);xrtFree(path);return 1;
}
int main(void){if(!run())return 1;printf("project_reference_budget_test passed: %u checks;over7MiB result rejected before real8MiB framing,positive reference roundtrip with64KiB worst-escaped request ID\n",checks);return 0;}
