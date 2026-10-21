#include "platform/text_file.h"
#include "platform/path.h"
#include <windows.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xui.h"
static int failure(xide_model_error* error,int code,const char* message)
{
    if(error) { error->code=code; snprintf(error->message,sizeof(error->message),"%s",message); }
    return 0;
}
void xideTextFileClear(xide_text_file* file)
{
    if(!file) return;
    free(file->raw); free(file->text); memset(file,0,sizeof(*file));
}
int xideTextFileRead(const char* path,xide_text_file* file,xide_model_error* error)
{
    char* native=path ? xidePathNativeUtf8(path) : NULL; size_t size=0;
    unsigned char* data=native ? xrtFileReadAllLimit(native,16u*1024u*1024u,&size) : NULL; free(native);
    if(!data) return failure(error,XIDE_MODEL_IO,"Unable to read text file.");
    int okay=xideTextFileDecode(data,size,file,error); xrtFree(data); return okay;
}
int xideTextFileDecode(const unsigned char* data,size_t size,xide_text_file* file,xide_model_error* error)
{
    xide_text_file loaded={0};
    if(!data || !file || size>16u*1024u*1024u) return failure(error,XIDE_MODEL_INVALID,"Invalid text data or text exceeds the 16 MiB limit.");
    size_t start=size>=3 && !memcmp(data,"\xef\xbb\xbf",3) ? 3 : 0;
    if(memchr(data+start,0,size-start) || (size>start && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,(char*)data+start,(int)(size-start),NULL,0))) {
        return failure(error,XIDE_MODEL_INVALID,"Only UTF-8 text files are supported; binary, UTF-16 and invalid UTF-8 files are rejected.");
    }
    loaded.raw=malloc(size+1); loaded.text=malloc(size-start+1);
    if(!loaded.raw || !loaded.text) { xideTextFileClear(&loaded); return failure(error,XIDE_MODEL_MEMORY,"Text file allocation failed."); }
    memcpy(loaded.raw,data,size); loaded.raw[size]=0; loaded.rawLength=size; loaded.bom=start!=0;
    size_t output=0; int crlf=0,lf=0,cr=0;
    for(size_t i=start;i<size;++i) {
        if(data[i]=='\r') { if(i+1<size && data[i+1]=='\n') { ++i; ++crlf; } else ++cr; loaded.text[output++]='\n'; }
        else { if(data[i]=='\n') ++lf; loaded.text[output++]=(char)data[i]; }
    }
    loaded.text[output]=0;
    loaded.eol=crlf>lf && crlf>=cr ? XUI_CODE_EOL_CRLF : cr>lf ? XUI_CODE_EOL_CR : XUI_CODE_EOL_LF;
    xideTextFileClear(file); *file=loaded; return 1;
}
int xideTextFileEncode(const char* text,int length,int bom,int eol,xide_text_file* file,xide_model_error* error)
{
    if(!text || length<0 || !file) return failure(error,XIDE_MODEL_INVALID,"Invalid text serialization request.");
    size_t size=(size_t)length+(bom ? 3u : 0u);
    if(eol==XUI_CODE_EOL_CRLF) for(int i=0;i<length;++i) if(text[i]=='\n') ++size;
    xide_text_file encoded={0}; encoded.raw=malloc(size+1); encoded.text=malloc((size_t)length+1);
    if(!encoded.raw || !encoded.text) { xideTextFileClear(&encoded); return failure(error,XIDE_MODEL_MEMORY,"Text serialization allocation failed."); }
    size_t output=0;
    if(bom) { memcpy(encoded.raw,"\xef\xbb\xbf",3); output=3; }
    for(int i=0;i<length;++i) {
        if(text[i]=='\n' && eol==XUI_CODE_EOL_CRLF) encoded.raw[output++]='\r';
        encoded.raw[output++]=(unsigned char)(text[i]=='\n' && eol==XUI_CODE_EOL_CR ? '\r' : text[i]);
    }
    encoded.raw[output]=0; memcpy(encoded.text,text,(size_t)length); encoded.text[length]=0;
    encoded.rawLength=output; encoded.bom=bom; encoded.eol=eol;
    xideTextFileClear(file); *file=encoded; return 1;
}
