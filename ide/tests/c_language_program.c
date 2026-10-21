/* Controlled stdio transport/negotiation fixture, never a semantic provider. */
#define _WIN32_WINNT 0x0601
#include "app_xrt.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int frame(xvalue* message)
{ size_t n=0; char* json=xrtJsonStringify(message,false,&n); if(!json) return 0; printf("Content-Length: %zu\r\n\r\n",n); fwrite(json,1,n,stdout); fflush(stdout); xrtFree(json); return 1; }
int main(void)
{
 _setmode(_fileno(stdout),_O_BINARY); _setmode(_fileno(stdin),_O_BINARY);
 FILE* mode=fopen("c-language-mode.txt","rb"); if(mode) {
 char label[64]={0}; fread(label,1,sizeof(label)-1,mode); fclose(mode);
 if(!strcmp(label,"notification flood")) {
  xvalue* ready=xrtJsonParse(sv("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{\"textDocumentSync\":2,\"positionEncoding\":\"utf-16\",\"completionProvider\":{},\"hoverProvider\":true,\"definitionProvider\":true,\"documentSymbolProvider\":true}}}"));
  xvalue* noise=xrtJsonParse(sv("{\"jsonrpc\":\"2.0\",\"method\":\"window/logMessage\",\"params\":{\"type\":3,\"message\":\"transport noise\"}}")); int okay=ready && noise && frame(ready);
  for(int i=0;okay && i<100;++i) okay=frame(noise);
  xrtValueRelease(ready); xrtValueRelease(noise); if(okay) Sleep(10000); return okay ? 0 : 2;
 }
 xvalue* bad=xrtJsonParse(sv("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{\"textDocumentSync\":2,\"positionEncoding\":\"utf-8\"}}}")); int okay=bad && frame(bad); xrtValueRelease(bad); if(okay) Sleep(10000); return okay ? 0 : 2; }
 int count=0; wchar_t** args=CommandLineToArgvW(GetCommandLineW(),&count); if(!args) return 2;
 xvalue *message=xrtValueObject(),*values=xrtValueArray(); int okay=message && values;
 for(int i=1;okay && i<count;++i) { int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,args[i],-1,NULL,0,NULL,NULL); char* text=n>0 ? malloc((size_t)n) : NULL; okay=text && WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,args[i],-1,text,n,NULL,NULL) && xrtValueArrayAppendNew(values,xrtValueString(sv(text))); free(text); }
 if(okay) okay=xrtValueObjectSetNew(message,sv("jsonrpc"),xrtValueString(sv("2.0"))) && xrtValueObjectSetNew(message,sv("method"),xrtValueString(sv("transport.arguments"))) && xrtValueObjectSet(message,sv("params"),values) && frame(message);
 fputs("isolated fixture stderr\n",stderr); fflush(stderr);
 xrtValueRelease(message); xrtValueRelease(values); LocalFree(args); if(okay) Sleep(10000); return okay ? 0 : 2;
}
