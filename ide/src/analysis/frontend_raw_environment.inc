/* Construction-only complete raw-module header inputs. No AST/report/text
 * borrow survives Record. Duplicate parse/copy paths must agree exactly. */
#define XLS_RAW_ENV_BYTES (8u*1024u*1024u)
typedef struct raw_environment_record {char *path,*requested;xstrbuf key;} raw_environment_record;
static void rawEnvironmentClear(xls_raw_scope* s)
{
 for(size_t i=0;i<s->environments.Count;++i){raw_environment_record* p=xrtArrayGet(&s->environments,i);xrtFree(p->path);xrtFree(p->requested);xrtStrBufFree(&p->key);}
 xrtArrayUnit(&s->environments);s->environmentBytes=0;s->environmentInitialized=0;
}
static void rawEnvironmentInvalidate(xls_raw_scope* s)
{if(s){s->environmentReady=0;++s->declined;if(s->deferred)s->failed=1;rawEnvironmentClear(s);}}
void xlsRawBodySetEncoder(xls_raw_scope* s,xls_raw_encoder encoder)
{
 if(!s || xlsRawBodyScopeCurrent()!=s)return;
 if(s->environmentStarted){rawEnvironmentInvalidate(s);return;}
 s->encoder=encoder;s->environmentReady=s->ready && encoder!=NULL;
}
void xlsRawBodyEnvironmentMissing(void)
{if(xlsRawBodyScopeCurrent() && xlsRawBodyScopeCurrent()->environmentReady)rawEnvironmentInvalidate(xlsRawBodyScopeCurrent());}
static int rawEnvironmentReserve(xls_raw_scope* s,size_t payload)
{
 size_t current=s->environments.Capacity*sizeof(raw_environment_record);
 if(s->environmentBytes>XLS_RAW_ENV_BYTES || payload>XLS_RAW_ENV_BYTES-s->environmentBytes)return 0;
 if(s->environments.Count<s->environments.Capacity)return 1;
 xarray next={0};int okay=0;size_t used=s->environmentBytes-current;
 if(!xrtArrayInit(&next,sizeof(raw_environment_record)) || !xrtArrayReserve(&next,s->environments.Count+1) ||
    next.Capacity>(XLS_RAW_ENV_BYTES-used-payload)/sizeof(raw_environment_record) ||
    !xrtArrayResize(&next,s->environments.Count))goto done;
 if(s->environments.Count)memcpy(next.Data,s->environments.Data,s->environments.Count*sizeof(raw_environment_record));
 xrtArrayUnit(&s->environments);s->environments=next;next=(xarray){0};s->environmentBytes=used+s->environments.Capacity*sizeof(raw_environment_record);okay=1;
done:xrtArrayUnit(&next);return okay;
}
void xlsRawBodyRecordEnvironment(const char* path,const XL6_AstNode* root)
{
 xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!s || !s->environmentReady || !s->encoder)return;
 s->environmentStarted=1;xstrbuf key={0};char *label=NULL,*request=NULL;
 /* Reuse the source report's original XRT absolute-path routine. A closed
  * entry's ordinary AST may still carry the configured forward spelling.
  * The full AST key keeps that exact source spelling;requested separately
  * keeps the exact caller input. No preprocessor input is discarded. */
 const char* actual=root ? xl6AstSourcePath(root) : NULL;
 if(!path || !*path || !actual || !*actual || !xrtPathIsAbs(actual))goto failed;
 label=xrtPathAbs(actual);
 if(!label || !xrtPathIsAbs(label) || !s->encoder(root,&key) || key.Capacity>XLS_RAW_ENV_BYTES)goto failed;
 for(size_t i=0;i<s->environments.Count;++i){const raw_environment_record* p=xrtArrayConstGet(&s->environments,i);
  if(!strcmp(label,p->path)){if(strcmp(path,p->requested) || p->key.Size!=key.Size || (key.Size && memcmp(p->key.Data,key.Data,key.Size)))goto failed;xrtFree(label);xrtStrBufFree(&key);return;}}
 size_t pathBytes=strlen(label)+1,requestBytes=strlen(path)+1;
 if(pathBytes>XLS_RAW_ENV_BYTES || requestBytes>XLS_RAW_ENV_BYTES-pathBytes || key.Capacity>XLS_RAW_ENV_BYTES-pathBytes-requestBytes || s->environments.Count>=512)goto failed;
 if(!s->environmentInitialized){if(!xrtArrayInit(&s->environments,sizeof(raw_environment_record)))goto failed;s->environmentInitialized=1;s->environmentBytes=s->environments.Capacity*sizeof(raw_environment_record);}
 if(!rawEnvironmentReserve(s,pathBytes+requestBytes+key.Capacity))goto failed;
 request=xrtStrDup(path);if(!request)goto failed;
 ((raw_environment_record*)s->environments.Data)[s->environments.Count++]=(raw_environment_record){label,request,key};
 s->environmentBytes+=pathBytes+requestBytes+key.Capacity;return;
failed:xrtFree(label);xrtFree(request);xrtStrBufFree(&key);rawEnvironmentInvalidate(s);
}
int xlsRawBodyEnvironmentHasPath(const char* path)
{
 xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!s || !s->environmentReady || !path)return 0;
 for(size_t i=0;i<s->environments.Count;++i)if(!strcmp(((const raw_environment_record*)xrtArrayConstGet(&s->environments,i))->path,path))return 1;
 return 0;
}
static int rawEnvironmentCompare(const void* a,const void* b)
{return strcmp(((const raw_environment_record*)a)->path,((const raw_environment_record*)b)->path);}
static int rawEnvironmentAppend(xstrbuf* b,const void* data,size_t size)
{
 if(b->Size>XLS_RAW_ENV_BYTES || size>XLS_RAW_ENV_BYTES-b->Size)return 0;
 if(size){size_t end=b->Size+size;if(end>b->Capacity && !xrtStrBufReserve(b,end))return 0;memcpy(b->Data+b->Size,data,size);b->Size=end;b->Data[end]=0;}
 return b->Capacity<=XLS_RAW_ENV_BYTES;
}
int xlsRawBodyEnvironmentCopy(xstrbuf* out)
{
 xls_raw_scope* s=xlsRawBodyScopeCurrent();if(!out || out->Size || !s || !s->environmentReady || !s->environments.Count)return 0;
 if(s->environments.Count>1)qsort(s->environments.Data,s->environments.Count,sizeof(raw_environment_record),rawEnvironmentCompare);
 uint64_t head[]={UINT64_C(0x584c53454e563032),s->environments.Count};if(!rawEnvironmentAppend(out,head,sizeof(head)))return 0;
 for(size_t i=0;i<s->environments.Count;++i){const raw_environment_record* p=xrtArrayConstGet(&s->environments,i);uint64_t sizes[]={strlen(p->path),p->key.Size,strlen(p->requested)};
  if(!rawEnvironmentAppend(out,sizes,sizeof(sizes)) || !rawEnvironmentAppend(out,p->path,(size_t)sizes[0]) || !rawEnvironmentAppend(out,p->key.Data,p->key.Size) || !rawEnvironmentAppend(out,p->requested,(size_t)sizes[2]))return 0;}
 return 1;
}
