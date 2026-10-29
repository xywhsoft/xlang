#ifndef XLANG_IDE_FILE_IO_H
#define XLANG_IDE_FILE_IO_H
#include "app_xrt.h"

/* Convert ordinary UTF-8 paths only at the disk boundary. Returned buffers and
   handles keep XRT ownership; model, diagnostics and compiler paths stay ordinary. */
unsigned char* xideIoReadAllLimit(const char* path,size_t limit,size_t* size);
int xideIoWriteAtomic(const char* path,xbytesview bytes);
int xideIoCreateDirectories(const char* path);
int xideIoCopy(const char* source,const char* target,int overwrite);
int xideIoMove(const char* source,const char* target,int overwrite);
xfile xideIoOpen(const char* path,uint32 flags);
int xideIoStat(const char* path,int follow,xfileinfo* info);
/* Observe Unix microseconds independently of the SDK's time representation.
   Receipt adapters convert to the wire schema only when comparing metadata.
   Identity, size and times come from one handle. */
int xideIoReceiptFileStat(xfile file,xfileinfo* info);
int xideIoReceiptStat(const char* path,xfileinfo* info);
int xideIoExists(const char* path);
#endif
