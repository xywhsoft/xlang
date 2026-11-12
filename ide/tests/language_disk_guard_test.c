#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "app_xrt.h"
#include "platform/file_io.h"
#include <windows.h>
#include <stdlib.h>
static int observing,readFault,closeFault,allocationFault;static size_t reads,bytesRead,maxRead,hashBytes,allocations,maxAllocation;
static int namedFault,metadataFault,eofFault;static size_t metadataCalls;
static void* observedMalloc(size_t n){if(observing){++allocations;if(n>maxAllocation)maxAllocation=n;if(allocationFault)return NULL;}return malloc(n);}
static bool observedRead(xfile f,void* p,size_t n,size_t* done)
{if(observing){++reads;bytesRead+=n;if(n>maxRead)maxRead=n;if(readFault&&(size_t)(readFault<0?-readFault:readFault)==reads){if(readFault>0){*done=0;return false;}bool okay=xrtReadFull(f,p,n,done);if(*done)--*done;return okay;}}return xrtReadFull(f,p,n,done);}
static bool observedClose(xfile f){bool okay=xrtClose(f);return observing&&closeFault?false:okay;}
static bool observedHash(xsha256* h,const void* p,size_t n){if(observing)hashBytes+=n;return xrtSha256Update(h,p,n);}
static bool observedSha(const void* p,size_t n,void* hash){if(observing)hashBytes+=n;return xrtSha256(p,n,hash);}
static int observedFileStat(xfile f,xfileinfo* info){int okay=xideIoReceiptFileStat(f,info);if(observing&&okay&&++metadataCalls==(size_t)metadataFault)++info->Size;return okay;}
static int observedNameStat(const char* path,xfileinfo* info){int okay=xideIoReceiptStat(path,info);if(observing&&okay&&namedFault)info->Identity^=1;return okay;}
static bool observedEof(xfile f,void* p,size_t n,size_t* done){bool okay=xrtRead(f,p,n,done);if(observing&&eofFault&&okay&&!*done)*done=1;return okay;}
#define malloc observedMalloc
#define xrtReadFull observedRead
#define xrtClose observedClose
#define xrtSha256Update observedHash
#define xrtSha256 observedSha
#define xideIoReceiptFileStat observedFileStat
#define xideIoReceiptStat observedNameStat
#define xrtRead observedEof
#include "services/language_snapshot.c"
#undef malloc
#undef xrtReadFull
#undef xrtClose
#undef xrtSha256Update
#undef xrtSha256
#undef xideIoReceiptFileStat
#undef xideIoReceiptStat
#undef xrtRead
static unsigned checks;static size_t cancelAt,cancelPolls;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"disk-guard:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static int cancel(void* unused){(void)unused;return ++cancelPolls>=cancelAt&&cancelAt;}
static void reset(void){reads=bytesRead=maxRead=hashBytes=allocations=maxAllocation=cancelPolls=metadataCalls=0;readFault=closeFault=allocationFault=namedFault=metadataFault=eofFault=0;cancelAt=0;}
static xvalue* digest(const void* body,size_t n)
{unsigned char hash[32];char out[65];if(!xrtSha256(body,n,hash))return NULL;for(int i=0;i<32;++i)snprintf(out+2*i,3,"%02x",hash[i]);return xrtValueString(xrtStrView(out));}
static xvalue* proof(const char* path,const void* body,size_t n,int schema)
{
 xfileinfo info;CHECK(xideIoReceiptStat(path,&info));xfileinfo wire=xideReceiptInfo(&info,schema);size_t bom=n>=3&&!memcmp(body,"\xef\xbb\xbf",3)?3:0;
 xvalue *out=xrtValueObject(),*meta=xrtValueObject();char text[32];int okay=out&&meta&&put(meta,"available",xrtValueInt(info.Available))&&put(meta,"attributes",xrtValueInt(info.Attributes));
 snprintf(text,sizeof(text),"%016llx",(unsigned long long)info.Device);okay=okay&&put(meta,"device",xrtValueString(xrtStrView(text)));snprintf(text,sizeof(text),"%016llx",(unsigned long long)info.Identity);okay=okay&&put(meta,"identity",xrtValueString(xrtStrView(text)));
 snprintf(text,sizeof(text),"%lld",(long long)wire.Modified);okay=okay&&put(meta,schema==1?"modifiedUs":"modifiedMs",xrtValueString(xrtStrView(text)));snprintf(text,sizeof(text),"%lld",(long long)wire.Changed);okay=okay&&put(meta,schema==1?"changedUs":"changedMs",xrtValueString(xrtStrView(text)));
 okay=okay&&put(out,"path",xrtValueString(xrtStrView(path)))&&put(out,"origin",xrtValueString(xrtStrView("disk")))&&put(out,"kind",xrtValueInt(1))&&put(out,"rawSize",xrtValueInt((int64)n))&&put(out,"textSize",xrtValueInt((int64)(n-bom)))&&put(out,"rawSha256",digest(body,n))&&put(out,"textSha256",digest((const char*)body+bom,n-bom));if(okay){okay=put(out,"metadata",meta);meta=NULL;}xrtValueRelease(meta);if(!okay){xrtValueRelease(out);out=NULL;}return out;
}
static int writeBytes(const char* path,const void* p,size_t n){return xrtFileWriteAtomic(path,(xbytesview){p,n});}
static void compare(const char* path,const void* body,size_t n,int expected,int schema)
{
 CHECK(writeBytes(path,body,n));xvalue* row=proof(path,body,n,schema);CHECK(row);xide_language_check q={.externalCancel=cancel};xide_model_error e={0};reset();char* old=xideLanguageReadDiskProof(path,row,NULL,NULL,NULL,&e);CHECK((old!=NULL)==expected);free(old);
 observing=1;int okay=diskProofValid(&q,path,row,&e);observing=0;CHECK(okay==expected&&maxRead<=65536&&!allocations);
 if(okay){size_t bom=n>=3&&!memcmp(body,"\xef\xbb\xbf",3)?3:0;CHECK(bytesRead==n&&hashBytes==(bom?2*n-bom:n));}xrtValueRelease(row);
}
static int run(const char* root)
{
 char* path=xidePathResolve(root,"source.xl");CHECK(path);const char* tiny[]={"","a","\r\n","\xef\xbb\xbf","\xef\xbb\xbf" "中😀\r\n","\xef\xbb","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xf0\x9f\x98","\x80","\xef\xbf\xbf"};
 for(size_t i=0;i<sizeof(tiny)/sizeof(*tiny);++i)for(int schema=1;schema<=2;++schema)compare(path,tiny[i],strlen(tiny[i]),i<5||i==11,schema);
 char nul[]={'a',0,'b'};compare(path,nul,sizeof(nul),0,1);
 size_t large=8u*1024u*1024u;char* body=malloc(large+1);CHECK(body);memset(body,'x',large);body[large]=0;
 /* Every position of a UTF8 scalar crossing64KiB, including after a physical
  * BOM; a following ASCII byte and EOF independently finish validation. */
 const char* scalars[]={"中","😀","\xc2\xa2"};for(int bom=0;bom<2;++bom)for(size_t s=0;s<3;++s)for(size_t split=1;split<strlen(scalars[s]);++split){size_t at=65536-split,n=65536+16;memset(body,'x',n);if(bom)memcpy(body,"\xef\xbb\xbf",3);memcpy(body+at,scalars[s],strlen(scalars[s]));compare(path,body,n,1,1);compare(path,body,at+split,0,2);body[at+split]=(char)0xff;compare(path,body,n,0,1);}
 memset(body,'x',large);body[large-1]='\n';compare(path,body,large,1,2);CHECK(writeBytes(path,body,large));xvalue* row=proof(path,body,large,1);CHECK(row);xide_language_check q={.externalCancel=cancel};xide_model_error e={0};reset();observing=1;CHECK(diskProofValid(&q,path,row,&e));observing=0;size_t blocks=reads,polls=cancelPolls;CHECK(blocks==128&&polls>=blocks&&!allocations);
 DWORD handles=0,after=0;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handles));
 for(size_t i=1;i<=polls;++i){reset();cancelAt=i;observing=1;int okay=diskProofValid(&q,path,row,&e);observing=0;CHECK(!okay&&cancelPolls==i&&GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);}
 for(size_t i=1;i<=blocks;++i)for(int partial=0;partial<2;++partial){reset();readFault=partial?-(int)i:(int)i;observing=1;int okay=diskProofValid(&q,path,row,&e);observing=0;CHECK(!okay&&reads==i&&GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);}
 reset();closeFault=1;observing=1;int okay=diskProofValid(&q,path,row,&e);observing=0;CHECK(!okay&&GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);
 for(int i=0;i<4;++i){reset();if(i<2)metadataFault=i+1;else if(i==2)namedFault=1;else eofFault=1;observing=1;okay=diskProofValid(&q,path,row,&e);observing=0;CHECK(!okay&&GetProcessHandleCount(GetCurrentProcess(),&after)&&after==handles);}
 reset();allocationFault=1;observing=1;okay=diskProofValid(&q,path,row,&e);observing=0;CHECK(okay&&!allocations&&bytesRead==large&&hashBytes==large);
 for(int field=0;field<5;++field){xvalue* bad=xrtValueDeepClone(row);CHECK(bad);if(field==0)CHECK(put(bad,"rawSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));if(field==1)CHECK(put(bad,"textSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));if(field==2)CHECK(put(bad,"textSize",xrtValueInt((int64)large-1)));if(field==3)CHECK(put(get(bad,"metadata"),"identity",xrtValueString(xrtStrView("0000000000000000"))));if(field==4)CHECK(put(get(bad,"metadata"),"modifiedUs",xrtValueString(xrtStrView("0"))));reset();CHECK(!diskProofValid(&q,path,bad,&e));xrtValueRelease(bad);}
 /* Actual identity replacement with identical bytes must still invalidate. */
 CHECK(writeBytes(path,body,large));reset();CHECK(!diskProofValid(&q,path,row,&e));xrtValueRelease(row);row=proof(path,body,large,1);CHECK(row);
 /* The production guard calls this reader, skips overlays and keeps its
  * original package/candidate guards. No normalization allocation survives. */
 q.guardFiles=xrtValueArray();q.guardResolution=xrtValueObject();q.nativeSemantic=1;CHECK(q.guardFiles&&q.guardResolution&&xrtValueArrayAppend(q.guardFiles,row)&&put(q.guardResolution,"probes",xrtValueArray())&&xideLanguageCheckPackagePath(&q.packagePath,&e));reset();observing=1;okay=xideLanguageSnapshotGuard(&q,&e);observing=0;CHECK(okay&&bytesRead==large&&maxAllocation<=65536&&hashBytes==large);free(q.packagePath);xrtValueRelease(q.guardFiles);xrtValueRelease(q.guardResolution);xrtValueRelease(row);free(body);free(path);
 printf("PASS streaming disk guard: %u checks; 8MiB/128 bounded reads, %zu cancellation points, every failed/short read, schema1/2, UTF8/BOM boundary oracle, exact raw/post-BOM digest, no producer body allocation, one digest without BOM, metadata/identity and real guard consumer\n",checks,polls);return 1;
}
int main(int argc,char** argv){if(argc!=2||!xideIoCreateDirectories(argv[1]))return 2;return run(argv[1])?0:1;}
