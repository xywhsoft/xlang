#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "platform/file_io.h"
#include "platform/path.h"
#include <stdlib.h>
#include <windows.h>

unsigned char* xideIoReadAllLimit(const char* path,size_t limit,size_t* size)
{
 if(size) *size=0;
 char* native=xidePathNativeUtf8(path);
 unsigned char* bytes=native ? xrtFileReadAllLimit(native,limit,size) : NULL;
 free(native); return bytes;
}
int xideIoWriteAtomic(const char* path,xbytesview bytes)
{
 char* native=xidePathNativeUtf8(path);
 int okay=native && xrtFileWriteAtomic(native,bytes);
 free(native); return okay;
}
int xideIoCreateDirectories(const char* path)
{
 char* native=xidePathNativeUtf8(path);
 int okay=native && xrtDirCreateAll(native);
 free(native); return okay;
}
int xideIoCopy(const char* source,const char* target,int overwrite)
{
 char *from=xidePathNativeUtf8(source),*to=xidePathNativeUtf8(target);
 int okay=from && to && xrtFileCopy(from,to,overwrite!=0);
 free(from); free(to); return okay;
}
int xideIoMove(const char* source,const char* target,int overwrite)
{
 char *from=xidePathNativeUtf8(source),*to=xidePathNativeUtf8(target);
 int okay=from && to && xrtFileMove(from,to,overwrite!=0);
 free(from); free(to); return okay;
}
xfile xideIoOpen(const char* path,uint32 flags)
{
 char* native=xidePathNativeUtf8(path);
 xfile file=native ? xrtOpen(native,flags) : NULL;
 free(native); return file;
}
int xideIoStat(const char* path,int follow,xfileinfo* info)
{
 char* native=xidePathNativeUtf8(path);
 int okay=native && xrtPathStat(native,follow!=0,info);
 free(native); return okay;
}
int xideIoExists(const char* path)
{
 char* native=xidePathNativeUtf8(path);
 int exists=native && xrtFileExists(native);
 free(native); return exists;
}

static int64 receiptTime(FILETIME value)
{
 ULARGE_INTEGER ticks; ticks.LowPart=value.dwLowDateTime; ticks.HighPart=value.dwHighDateTime;
 return (int64)(ticks.QuadPart/10u)-INT64_C(11644473600000000);
}
int xideIoReceiptFileStat(xfile file,xfileinfo* info)
{
 FILETIME created,accessed,modified;
 if(!file || !info || !xrtFileStat(file,info) ||
    !GetFileTime((HANDLE)xrtFileNative(file),&created,&accessed,&modified)) return 0;
 if(info->Available&XFILE_INFO_CREATE_TIME) info->Created=receiptTime(created);
 if(info->Available&XFILE_INFO_ACCESS_TIME) info->Accessed=receiptTime(accessed);
 if(info->Available&XFILE_INFO_MODIFY_TIME) info->Modified=receiptTime(modified);
 if(info->Available&XFILE_INFO_CHANGE_TIME) {
  FILE_BASIC_INFO basic;
  if(!GetFileInformationByHandleEx((HANDLE)xrtFileNative(file),FileBasicInfo,&basic,sizeof(basic))) return 0;
  ULARGE_INTEGER ticks; ticks.QuadPart=(ULONGLONG)basic.ChangeTime.QuadPart;
  info->Changed=receiptTime((FILETIME){ticks.LowPart,ticks.HighPart});
 }
 return 1;
}
int xideIoReceiptStat(const char* path,xfileinfo* info)
{
 xfile file=xideIoOpen(path,XFILE_READ); if(!file) return 0;
 int okay=xideIoReceiptFileStat(file,info);
 return xrtClose(file) && okay;
}
