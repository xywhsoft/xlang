#ifndef XL6_FIXTURE_XGE_ASYNC_FILE_H
#define XL6_FIXTURE_XGE_ASYNC_FILE_H
#include <stdio.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
static char fixtureAsyncFile[2048];
static const char* fixtureAsyncWrite(void)
{
 static const unsigned char data[]={0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,24,32,3,2,1};
 const char* root=getenv("TMPDIR");if(!root)root=getenv("TEMP");if(!root)root=".";
#if defined(_WIN32)
 unsigned long pid=(unsigned long)GetCurrentProcessId();
#else
 unsigned long pid=(unsigned long)getpid();
#endif
 int count=snprintf(fixtureAsyncFile,sizeof(fixtureAsyncFile),"%s/xge-async-fixture-%lu.tga",root,pid);
 if(count<=0 || (size_t)count>=sizeof(fixtureAsyncFile))return "";
 FILE* file=fopen(fixtureAsyncFile,"wb");if(!file)return "";
 size_t written=fwrite(data,1,sizeof(data),file);int closed=fclose(file);
 return written==sizeof(data) && !closed ? fixtureAsyncFile:"";
}
static int fixtureAsyncRemove(void){return remove(fixtureAsyncFile);}
#endif
