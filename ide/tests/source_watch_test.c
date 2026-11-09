#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int refuseAllocation;
static void* watchMalloc(size_t size){if(refuseAllocation){refuseAllocation=0;return NULL;}return malloc(size);}
#define malloc watchMalloc
#include "../src/platform/file_watch.c"
#undef malloc
static int checks,failed,wakes;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"source_watch_test:%d:%s\n",__LINE__,#v);failed=1;goto cleanup;}}while(0)
static xide_file_watch* advanceOnWake;
static void woke(void* unused){(void)unused;++wakes;if(advanceOnWake){++advanceOnWake->stats.epoch;advanceOnWake=NULL;}}
static size_t packet(unsigned char* data,const wchar_t* name,DWORD action)
{DWORD next=0,size=(DWORD)(wcslen(name)*sizeof(wchar_t));memcpy(data,&next,4);memcpy(data+4,&action,4);memcpy(data+8,&size,4);memcpy(data+12,name,size);return 12+size;}
static int take(xide_file_watch* w,int state,int type,const char* path)
{xide_watch_event e={0};if(!xideFileWatchTake(w,&e))return 0;int okay=e.epoch==1 && !e.token && e.state==state && e.changeType==type && !e.length && !e.bytes && (!path ? !e.path : e.path && xidePathLexicalEqual(e.path,path));xideFileWatchEventClear(&e);return okay;}
static void drain(xide_file_watch* w){xide_watch_event e={0};while(xideFileWatchTake(w,&e))xideFileWatchEventClear(&e);}
int main(int argc,char** argv)
{
    xide_file_watch w={0};batch b={.epoch=1,.sources=1};directory d={0};char *root=NULL,*candidate=NULL;_Alignas(4) unsigned char data[1024]={0};size_t bytes=0;
    InitializeCriticalSection(&w.mutex);w.stats.epoch=1;w.wake=woke;
    CHECK(argc==2);root=xidePathResolveLexical(NULL,argv[1]);candidate=root?xidePathResolveLexical(root,"候选😀.xl"):NULL;CHECK(root && candidate);d.path=root;
    b.count=2;b.items=calloc(b.count,sizeof(*b.items));CHECK(b.items);b.items[0].path=xidePathResolveLexical(root,"known.xl");b.items[1].path=xidePathResolveLexical(root,"命令.xl");CHECK(b.items[0].path && b.items[1].path);prepareSourceIndex(&w,&b);CHECK(b.sourceIndex && !w.count);
    bytes=packet(data,L"KNOWN.XL",FILE_ACTION_MODIFIED);sourceNotifications(&w,&b,&d,data,bytes);CHECK(!w.count);
    bytes=packet(data,L"命令.xl",FILE_ACTION_ADDED);sourceNotifications(&w,&b,&d,data,bytes);CHECK(!w.count);
    bytes=packet(data,L"ignored.tmp",FILE_ACTION_RENAMED_OLD_NAME);sourceNotifications(&w,&b,&d,data,bytes);CHECK(!w.count);
    bytes=packet(data,L"CANDID~1.XL",FILE_ACTION_REMOVED);sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    bytes=packet(data,L"候选😀.xl",FILE_ACTION_ADDED);sourceNotifications(&w,&b,&d,data,bytes);CHECK(w.count==1);
    bytes=packet(data,L"候选😀.xl",FILE_ACTION_MODIFIED);sourceNotifications(&w,&b,&d,data,bytes);CHECK(w.count==1 && take(&w,XIDE_WATCH_SOURCE_CHANGE,1,candidate));
    const DWORD actions[]={FILE_ACTION_MODIFIED,FILE_ACTION_REMOVED,FILE_ACTION_RENAMED_OLD_NAME,FILE_ACTION_RENAMED_NEW_NAME};const int types[]={2,3,3,1};
    for(size_t i=0;i<4;++i){bytes=packet(data,L"候选😀.xl",actions[i]);sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_CHANGE,types[i],candidate));}
    const wchar_t* invalidNames[]={L"../escape.xl",L"sub/../escape.xl",L"/absolute.xl",L"C:drive.xl",L"sub\\\\bad.xl"};
    for(size_t i=0;i<sizeof(invalidNames)/sizeof(*invalidNames);++i){bytes=packet(data,invalidNames[i],FILE_ACTION_ADDED);sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));}
    bytes=packet(data,L"a.xl",FILE_ACTION_ADDED);sourceNotifications(&w,&b,&d,data,11);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    sourceNotifications(&w,&b,&d,data,bytes-1);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    DWORD bad=1;memcpy(data+8,&bad,4);sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    bytes=packet(data,L"a.xl",FILE_ACTION_ADDED);bad=(DWORD)bytes;memcpy(data,&bad,4);sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    bytes=packet(data,L"a.xl",0);sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    bytes=packet(data,L"a.xl",FILE_ACTION_ADDED);data[12]=0;data[13]=0;sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    bytes=packet(data,L"a.xl",FILE_ACTION_ADDED);data[12]=0;data[13]=0xd8;sourceNotifications(&w,&b,&d,data,bytes);CHECK(take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    bytes=packet(data,L"a.xl",FILE_ACTION_ADDED);refuseAllocation=1;sourceNotifications(&w,&b,&d,data,bytes);CHECK(!refuseAllocation && take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));
    b.sourceIndex=0;sourceNotifications(&w,&b,&d,data,bytes);sourceNotifications(&w,&b,&d,data,bytes);CHECK(w.count==1 && take(&w,XIDE_WATCH_SOURCE_RESCAN,0,NULL));b.sourceIndex=1;
    ++w.stats.epoch;sourceNotifications(&w,&b,&d,data,bytes);CHECK(!w.count && w.sourceGap);w.stats.epoch=1;w.sourceGap=0;
    bytes=packet(data,L"候选😀.xl",FILE_ACTION_MODIFIED);advanceOnWake=&w;sourceNotifications(&w,&b,&d,data,bytes);CHECK(w.stats.epoch==2 && w.sourceGap && !advanceOnWake);drain(&w);w.stats.epoch=1;w.sourceGap=0;
    /* Path metadata has its own bound and coalesces independently of body
       snapshots. Overflow must wake the owner even if no event was admitted. */
    for(size_t i=0;i<140;++i){char* path=malloc(32769);CHECK(path);memset(path,'a',32768);snprintf(path,20,"C:/%08u/",(unsigned)i);path[strlen(path)]='a';path[32768]=0;xide_watch_event e={.epoch=1,.state=XIDE_WATCH_SOURCE_CHANGE,.changeType=2,.path=path};enqueue(&w,&e);xideFileWatchEventClear(&e);}
    CHECK(w.sourceBytes<=SOURCE_NAMES_LIMIT && w.stats.dropped && wakes>=140);drain(&w);CHECK(!w.sourceBytes && !w.bytes);
    uint64_t dropped=w.stats.dropped;
    for(size_t i=0;i<MAX_EVENTS+1;++i){char name[64];snprintf(name,sizeof(name),"C:/bounded/%u.xl",(unsigned)i);xide_watch_event e={.epoch=1,.state=XIDE_WATCH_SOURCE_CHANGE,.changeType=2,.path=copy(name)};CHECK(e.path);enqueue(&w,&e);xideFileWatchEventClear(&e);}
    CHECK(w.count==MAX_EVENTS && w.stats.dropped==dropped+1);drain(&w);
    printf("source_watch_test passed: %d checks;real parser,indexed known-path exclusion,UTF16/Unicode,action/coalescing,invalid/truncated records,allocation refusal,epoch rejection,4MiB names and4096-event overflow wakes.\n",checks);
cleanup:
    drain(&w);clearBatch(&b);free(w.events);DeleteCriticalSection(&w.mutex);free(root);free(candidate);return failed;
}
