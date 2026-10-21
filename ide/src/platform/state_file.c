#include "platform/state_file.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static int fail(xide_model_error* error,int code,const char* message)
{ if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); } return 0; }
static xvalue* read(const char* path)
{
    char* native=xidePathNativeUtf8(path); size_t size=0;
    unsigned char* text=native ? xrtFileReadAllLimit(native,8u*1024u*1024u,&size) : NULL; free(native); if(!text) return NULL;
    size_t offset=size>=3 && text[0]==0xef && text[1]==0xbb && text[2]==0xbf ? 3 : 0;
    xvalue* result=NULL;
    if(size>offset && !memchr(text+offset,0,size-offset) && MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char*)text+offset,(int)(size-offset),NULL,0))
        result=xrtJsonParse((xstrview){(char*)text+offset,size-offset});
    xrtFree(text); if(result && !xrtValueIs(result,XVALUE_OBJECT)) { xrtValueRelease(result); result=NULL; } return result;
}
void xideStateClear(xide_state_file* state)
{ if(state) { free(state->path); xrtValueRelease(state->source); memset(state,0,sizeof(*state)); } }
int xideStateOpen(xide_state_file* state,const char* path,const char* format,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!state || !path || !format) return fail(error,XIDE_MODEL_INVALID,"Invalid state-file arguments.");
    xide_state_file fresh={0}; fresh.path=xidePathResolve(NULL,path); if(!fresh.path) return fail(error,XIDE_MODEL_INVALID,"Invalid state-file path.");
    fresh.existed=xidePathExists(fresh.path); fresh.source=fresh.existed ? read(fresh.path) : xrtValueObject();
    xstrview value={0};
    if(!fresh.source || (fresh.existed && (!xrtValueGetString(xrtValueObjectGet(fresh.source,sv("format")),&value) || value.Size!=strlen(format) || memcmp(value.Data,format,value.Size)))) {
        xideStateClear(&fresh); return fail(error,XIDE_MODEL_INVALID,"State is unreadable or unsupported; the original file is preserved.");
    }
    xideStateClear(state); *state=fresh; return 1;
}
static int writeLocked(xide_state_file* state,xvalue* root,xide_model_error* error)
{
    if(error) memset(error,0,sizeof(*error));
    if(!state || !state->path || !state->source || !root || !xrtValueIs(root,XVALUE_OBJECT)) return fail(error,XIDE_MODEL_INVALID,"Reload or repair state before saving.");
    int exists=xidePathExists(state->path); xvalue* disk=exists ? read(state->path) : NULL;
    int unchanged=exists==state->existed && (!exists || (disk && xrtValueEqual(disk,state->source))); xrtValueRelease(disk);
    if(!unchanged) return fail(error,XIDE_MODEL_CONFLICT,"State changed on disk; the other process's file is preserved.");
    xvalue* baseline=xrtValueDeepClone(root);
    if(!baseline) return fail(error,XIDE_MODEL_MEMORY,"Cannot retain saved-state baseline.");
    size_t size=0; char* text=xrtJsonStringify(root,true,&size); char* directory=xidePathDirectory(state->path);
    char* nativeDir=directory ? xidePathNativeUtf8(directory) : NULL; char* nativePath=xidePathNativeUtf8(state->path);
    int okay=text && size<=8u*1024u*1024u && nativeDir && nativePath && xrtDirCreateAll(nativeDir) && xrtFileWriteAtomic(nativePath,(xbytesview){(unsigned char*)text,size});
    free(nativeDir); free(nativePath);
    xrtFree(text); free(directory); if(!okay) { xrtValueRelease(baseline); return fail(error,XIDE_MODEL_IO,"Cannot atomically save state; the previous file is preserved."); }
    xrtValueRelease(state->source); state->source=baseline; state->existed=1;
    return 1;
}
int xideStateWrite(xide_state_file* state,xvalue* root,xide_model_error* error)
{
    if(!state || !state->path) return fail(error,XIDE_MODEL_INVALID,"No state-file path.");
    char* directory=xidePathDirectory(state->path);
    char* nativeDir=directory ? xidePathNativeUtf8(directory) : NULL;
    int created=nativeDir && xrtDirCreateAll(nativeDir); free(directory); free(nativeDir);
    if(!created) return fail(error,XIDE_MODEL_IO,"Cannot create state directory.");
    size_t length=strlen(state->path); char* path=malloc(length+6);
    if(!path) return fail(error,XIDE_MODEL_MEMORY,"Cannot allocate state lock.");
    memcpy(path,state->path,length); memcpy(path+length,".lock",6);
    wchar_t* wide=xidePathNativeWide(path);
    HANDLE lock=wide ? CreateFileW(wide,GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
    free(path); free(wide);
    if(lock==INVALID_HANDLE_VALUE) return fail(error,XIDE_MODEL_CONFLICT,"Another IDE is saving state; the previous snapshot is preserved.");
    int result=writeLocked(state,root,error); CloseHandle(lock); return result;
}
