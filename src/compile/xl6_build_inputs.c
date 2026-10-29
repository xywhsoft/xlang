#include "src/compile/xl6_build_inputs.h"
#include "src/common/xl6_output.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#endif
/* The compiler API lock serializes this binding, the process work directory
 * and TCC's disk-reader callback as one scope. Activation is nested/restored
 * under that same lock. A separate TLS slot is neither needed nor sufficient
 * to make the other two global transports concurrent; Windows PE generated
 * by embedded TCC cannot use the ELF-style __thread access sequence either. */
static XL6_BuildInputs* active;
static int fail(XL6_BuildInputs* r,const char* path,const char* message)
{
    if(r && r->complete) { r->complete=0; snprintf(r->error,sizeof(r->error),"%s: %s",message,path?path:""); }
    return 0;
}
static int equalPath(const char* a,const char* b)
{
#ifdef _WIN32
    wchar_t wa[4096],wb[4096];
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,a,-1,wa,4096) &&
        MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b,-1,wb,4096) &&
        CompareStringOrdinal(wa,-1,wb,-1,TRUE)==CSTR_EQUAL;
#else
    return !strcmp(a,b);
#endif
}
static int infoEqual(const xfileinfo* a,const xfileinfo* b)
{
    uint32 required=XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME|XFILE_INFO_IDENTITY;
    return a->Type==XFILE_TYPE_FILE && b->Type==XFILE_TYPE_FILE &&
        (a->Available&required)==required && (b->Available&required)==required &&
        a->Available==b->Available && a->Size==b->Size && a->Device==b->Device && a->Identity==b->Identity &&
        a->Modified==b->Modified && a->Attributes==b->Attributes &&
        (!(a->Available&XFILE_INFO_CHANGE_TIME)||a->Changed==b->Changed);
}
/* An unreadable or non-regular existing path cannot be treated as missing. */
static int diskState(const char* path)
{
#ifdef _WIN32
    wchar_t name[4096]; DWORD flags,code;
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,name,4096)) return -1;
    flags=GetFileAttributesW(name); code=GetLastError();
    if(flags==INVALID_FILE_ATTRIBUTES) return code==ERROR_FILE_NOT_FOUND||code==ERROR_PATH_NOT_FOUND ? 0 : -1;
    return flags&FILE_ATTRIBUTE_DIRECTORY ? -1 : 1;
#else
    struct stat st;
    if(stat(path,&st)) return errno==ENOENT||errno==ENOTDIR ? 0 : -1;
    return S_ISREG(st.st_mode) ? 1 : -1;
#endif
}
static unsigned char* readStable(const char* path,xfileinfo* info,size_t* size)
{
    xfile file=xrtOpen(path,XFILE_READ); xfileinfo first,last,named;
    unsigned char *raw=NULL,extra; size_t done=0,n=0;
    int ok=file && xrtFileStat(file,&first) && infoEqual(&first,&first) && first.Size<=XL6_BUILD_INPUT_FILE_BYTES;
    if(ok) { raw=xrtMalloc((size_t)first.Size+1); ok=raw!=NULL; }
    if(ok) ok=xrtReadFull(file,raw,(size_t)first.Size,&done) && done==first.Size &&
        xrtRead(file,&extra,1,&n) && !n && xrtFileStat(file,&last) && infoEqual(&first,&last) &&
        xrtPathStat(path,true,&named) && infoEqual(&first,&named);
    if(file && !xrtClose(file)) ok=0;
    if(!ok) { xrtFree(raw); return NULL; }
    raw[done]=0; *info=first; *size=done; return raw;
}
void xl6BuildInputsInit(XL6_BuildInputs* r) { memset(r,0,sizeof(*r)); r->complete=1; xl6SourceReportInit(&r->sources); }
void xl6BuildInputsFree(XL6_BuildInputs* r)
{
    size_t i;
    if(!r) return;
    for(i=0;i<r->count;++i) { xrtFree(r->files[i].path); xrtFree(r->files[i].data); }
    for(i=0;i<r->missingCount;++i) xrtFree(r->missing[i]);
    xrtFree(r->files); xrtFree(r->missing); xrtFree(r->artifact.path); xl6SourceReportFree(&r->sources); memset(r,0,sizeof(*r));
}
int xl6BuildInputsRead(void* opaque,const char* path,const unsigned char** data,size_t* size)
{
    XL6_BuildInputs* r=opaque; char* absolute; size_t i;
    XL6_BuildInput item; int state;
    if(data) *data=NULL;
    if(size) *size=0;
    if(!r||!data||!size||!r->complete) return -1;
    if(!path||!*path||strlen(path)>=4096||!xrtUtf8Valid(xrtStrView(path),NULL)) { fail(r,path,"invalid build input path"); return -1; }
    absolute=xrtPathAbs(path);
    if(!absolute||strlen(absolute)>=4096) { xrtFree(absolute); fail(r,path,"cannot resolve build input"); return -1; }
    for(i=0;i<r->count;++i) if(equalPath(absolute,r->files[i].path)) {
        *data=r->files[i].data; *size=r->files[i].size; xrtFree(absolute); return 1;
    }
    for(i=0;i<r->missingCount;++i) if(equalPath(absolute,r->missing[i])) { xrtFree(absolute); return 0; }
    state=diskState(absolute);
    if(!state) {
        size_t pathBytes=strlen(absolute)+1;
        if(r->missingCount>=XL6_BUILD_INPUT_PROBES||
           pathBytes>XL6_BUILD_INPUT_PROBE_BYTES-r->missingBytes) {
            fail(r,absolute,"native candidate limit exceeded"); xrtFree(absolute); return -1;
        }
        if(r->missingCount==r->missingCapacity) {
            size_t cap=r->missingCapacity?r->missingCapacity*2:32;
            char** paths=xrtRealloc(r->missing,cap*sizeof(*paths));
            if(!paths) { fail(r,absolute,"cannot retain native candidate"); xrtFree(absolute); return -1; }
            r->missing=paths; r->missingCapacity=cap;
        }
        r->missing[r->missingCount++]=absolute; r->missingBytes+=pathBytes; return 0;
    }
    memset(&item,0,sizeof(item)); item.path=absolute;
    if(state<0||r->count>=XL6_BUILD_INPUT_FILES) { fail(r,absolute,"unreadable build input or file limit exceeded"); xrtFree(absolute); return -1; }
    item.data=readStable(absolute,&item.info,&item.size);
    if(!item.data||item.size>XL6_BUILD_INPUT_TOTAL_BYTES-r->bytes||!xrtSha256(item.data,item.size,item.sha256)) {
        fail(r,absolute,"cannot retain stable bounded build input"); xrtFree(item.data); xrtFree(absolute); return -1;
    }
    if(r->count==r->capacity) {
        size_t cap=r->capacity?r->capacity*2:32;
        XL6_BuildInput* files=xrtRealloc(r->files,cap*sizeof(*files));
        if(!files) { fail(r,absolute,"cannot grow build input report"); xrtFree(item.data); xrtFree(absolute); return -1; }
        r->files=files; r->capacity=cap;
    }
    r->files[r->count++]=item; r->bytes+=item.size; *data=item.data; *size=item.size; return 1;
}
int xl6BuildInputsValidate(XL6_BuildInputs* r)
{
    size_t i;
    if(!r||!r->complete) return 0;
    if(!xl6SourceReportValidate(&r->sources)) return fail(r,r->sources.errorPath,r->sources.error);
    for(i=0;i<r->sources.count;++i) if(strcmp(r->sources.items[i].origin,"disk")) return fail(r,r->sources.items[i].path,"input reports require disk sources");
    for(i=0;i<r->missingCount;++i) if(diskState(r->missing[i])!=0) return fail(r,r->missing[i],"native candidate changed during build");
    for(i=0;i<r->count;++i) {
        XL6_BuildInput* f=&r->files[i]; xfileinfo info; size_t size; unsigned char hash[32];
        unsigned char* raw=readStable(f->path,&info,&size);
        int ok=raw && size==f->size && infoEqual(&f->info,&info) && xrtSha256(raw,size,hash) && !memcmp(hash,f->sha256,32);
        xrtFree(raw); if(!ok) return fail(r,f->path,"build input changed");
    }
    return 1;
}
int xl6BuildInputsOutputValid(XL6_BuildInputs* r,const char* path)
{
    char* absolute; size_t i; xfileinfo target; int named;
    if(!r) return 1;
    if(!r->complete||!path||!*path) return 0;
    absolute=xrtPathAbs(path); if(!absolute) return fail(r,path,"invalid build output");
    named=xrtPathStat(absolute,true,&target);
    for(i=0;i<r->count;++i) if(equalPath(absolute,r->files[i].path)||
        (named && target.Device==r->files[i].info.Device && target.Identity==r->files[i].info.Identity)) break;
    if(i<r->count) { xrtFree(absolute); return fail(r,path,"output overlaps a build input"); }
    for(i=0;i<r->sources.count;++i) if(equalPath(absolute,r->sources.items[i].path)||
        (named && target.Device==r->sources.items[i].info.Device && target.Identity==r->sources.items[i].info.Identity)) break;
    if(i<r->sources.count) { xrtFree(absolute); return fail(r,path,"output overlaps an xlang input"); }
    /* Output can legitimately create one of the failed library candidates only
     * if the final validation then rejects it; prohibit this up front too. */
    for(i=0;i<r->missingCount;++i) if(equalPath(absolute,r->missing[i])) break;
    if(i<r->missingCount) { xrtFree(absolute); return fail(r,path,"output overlaps a failed native candidate"); }
    for(i=0;i<r->sources.probeCount;++i) if(!r->sources.probes[i].exists&&equalPath(absolute,r->sources.probes[i].path)) break;
    xrtFree(absolute);
    return i==r->sources.probeCount ? 1 : fail(r,path,"output overlaps a failed xlang candidate");
}
int xl6BuildInputsBindArtifact(XL6_BuildInputs* r,const char* path)
{
    XL6_BuildInput item; unsigned char* raw;
    if(!r) return 1;
    r->built=0; memset(&item,0,sizeof(item));
    if(!xl6BuildInputsValidate(r)||!xl6BuildInputsOutputValid(r,path)) return 0;
    item.path=xrtPathAbs(path); if(!item.path) return fail(r,path,"cannot resolve artifact");
    raw=readStable(item.path,&item.info,&item.size);
    if(!raw||!xrtSha256(raw,item.size,item.sha256)) { xrtFree(raw); xrtFree(item.path); return fail(r,path,"cannot bind new artifact"); }
    xrtFree(raw); xrtFree(r->artifact.path); r->artifact=item; r->built=1; return 1;
}
XL6_BuildInputs* xl6BuildInputsActivate(XL6_BuildInputs* r) { XL6_BuildInputs* previous=active; active=r; return previous; }
void* xl6BuildInputReadAll(const char* path,size_t* size)
{
    const unsigned char* data; size_t n; unsigned char* copy;
    if(!active) return xrtFileReadAll(path,size);
    if(size) *size=0;
    if(xl6BuildInputsRead(active,path,&data,&n)!=1) { fail(active,path,"required build input is missing"); return NULL; }
    copy=xrtMalloc(n+1); if(!copy) { fail(active,path,"cannot copy build input"); return NULL; }
    memcpy(copy,data,n); copy[n]=0; if(size) *size=n; return copy;
}
char* xl6BuildInputReadText(const char* path,size_t* size)
{
    char *raw,*text; size_t n,bomSize=0; xencoding bom;
    if(!active) return xrtFileReadText(path,XENCODING_UTF8,XUTF_STRICT,size);
    if(size) *size=0;
    raw=xl6BuildInputReadAll(path,&n); if(!raw) return NULL;
    bom=xrtEncodingBom((xbytesview){(bytes)raw,n},&bomSize);
    if((bom!=XENCODING_UNKNOWN&&bom!=XENCODING_UTF8)||memchr(raw+bomSize,0,n-bomSize)||!xrtUtf8Valid(xrtStrViewN(raw+bomSize,n-bomSize),NULL)) {
        xrtFree(raw); fail(active,path,"build text must be strict UTF-8 without NUL"); return NULL;
    }
    text=xrtStrDupN(raw+bomSize,n-bomSize); xrtFree(raw);
    if(!text) fail(active,path,"cannot copy build text"); else if(size) *size=n-bomSize;
    return text;
}
int xl6BuildInputOutputValid(const char* path) { return xl6BuildInputsOutputValid(active,path); }
int xl6BuildInputExists(const char* path)
{
    const unsigned char* data; size_t size;
    if(!active) return xrtFileExists((char*)path);
    return xl6BuildInputsRead(active,path,&data,&size)==1;
}
static void quote(XL6_Output* out,const char* text)
{
    const unsigned char* p=(const unsigned char*)(text?text:""); xl6OutputPutc('"',out);
    for(;*p;++p) { if(*p=='"'||*p=='\\') { xl6OutputPutc('\\',out); xl6OutputPutc(*p,out); }
        else if(*p<32) xl6OutputPrintf(out,"\\u%04x",*p); else xl6OutputPutc(*p,out); }
    xl6OutputPutc('"',out);
}
static void fileJson(XL6_Output* out,const char* path,size_t size,const unsigned char* hash,const xfileinfo* info)
{
    size_t i; xl6OutputPuts("{\"path\":",out); quote(out,path);
    xl6OutputPrintf(out,",\"size\":%llu,\"sha256\":\"",(unsigned long long)size);
    for(i=0;i<32;++i) xl6OutputPrintf(out,"%02x",hash[i]);
    xl6OutputPrintf(out,"\",\"metadata\":{\"available\":%u,\"device\":\"%016llx\",\"identity\":\"%016llx\",\"modifiedMs\":\"%lld\",\"changedMs\":\"%lld\",\"attributes\":%u}}",
        (unsigned)info->Available,(unsigned long long)info->Device,(unsigned long long)info->Identity,
        (long long)info->Modified,(long long)info->Changed,(unsigned)info->Attributes);
}
int xl6BuildInputsWrite(XL6_BuildInputs* r,const char* artifact,const char* receipt)
{
    XL6_Output out; char *absolute,*text; size_t i,j,n,written=0; xfileinfo info; unsigned char hash[32],*raw;
    if(!r||!r->built||!xl6BuildInputsValidate(r)||!xl6BuildInputsOutputValid(r,receipt)) return 0;
    absolute=xrtPathAbs(artifact); if(!absolute) return fail(r,artifact,"invalid artifact path");
    raw=readStable(absolute,&info,&n);
    if(!raw||!equalPath(absolute,r->artifact.path)||n!=r->artifact.size||!infoEqual(&info,&r->artifact.info)||
        !xrtSha256(raw,n,hash)||memcmp(hash,r->artifact.sha256,32)) { xrtFree(raw); xrtFree(absolute); return fail(r,artifact,"built artifact changed before receipt"); }
    xrtFree(raw);
    { char* target=xrtPathAbs(receipt); xfileinfo receiptInfo;
      int overlap=target && (equalPath(target,absolute)||(xrtPathStat(target,true,&receiptInfo)&&receiptInfo.Device==info.Device&&receiptInfo.Identity==info.Identity));
      xrtFree(target); if(overlap) { xrtFree(absolute); return fail(r,receipt,"receipt overlaps artifact"); } }
    if(!xl6OutputOpenMemory(&out)) { xrtFree(absolute); return fail(r,receipt,"cannot allocate receipt"); }
    xl6OutputPuts("{\"schema\":2,\"scope\":\"xlang-native-build-disk-inputs\",\"complete\":true,\"artifact\":",&out);
    fileJson(&out,absolute,n,hash,&info); xrtFree(absolute);
    xl6OutputPuts(",\"files\":[",&out);
    for(i=0;i<r->sources.count;++i) { const XL6_SourceReportItem* f=&r->sources.items[i];
        if(written++) xl6OutputPutc(',',&out);
        fileJson(&out,f->path,f->rawSize,f->rawSha256,&f->info); }
    for(i=0;i<r->count;++i) { XL6_BuildInput* f=&r->files[i];
        for(j=0;j<r->sources.count;++j) if(equalPath(f->path,r->sources.items[j].path)) break;
        if(j<r->sources.count) continue;
        if(written++) xl6OutputPutc(',',&out);
        fileJson(&out,f->path,f->size,f->sha256,&f->info); }
    xl6OutputPuts("],\"nativeMissing\":[",&out);
    for(i=0;i<r->missingCount;++i) { if(i) xl6OutputPutc(',',&out); quote(&out,r->missing[i]); }
    xl6OutputPuts("],\"xlangProbes\":[",&out);
    for(i=0;i<r->sources.probeCount;++i) { if(i) xl6OutputPutc(',',&out); xl6OutputPuts("{\"path\":",&out); quote(&out,r->sources.probes[i].path);
        xl6OutputPrintf(&out,",\"exists\":%s}",r->sources.probes[i].exists?"true":"false"); }
    xl6OutputPrintf(&out,"],\"packagePathObserved\":%s,\"XLANG_PACKAGE_PATH\":",r->sources.packagePathObserved?"true":"false");
    if(r->sources.packagePath) quote(&out,r->sources.packagePath); else xl6OutputPuts("null",&out);
    xl6OutputPuts("}\n",&out); text=xl6OutputHasFailed(&out)?NULL:xl6OutputTakeMemoryText(&out,&n); xl6OutputClose(&out);
    if(!text) return fail(r,receipt,"cannot serialize receipt");
    /* Validate again immediately before publishing a success record. */
    i=xl6BuildInputsValidate(r)&&xrtFileWriteAll(receipt,(xbytesview){(bytes)text,n}); xrtFree(text);
    return i?1:fail(r,receipt,"cannot publish validated receipt");
}
