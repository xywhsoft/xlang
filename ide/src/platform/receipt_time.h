#ifndef XLANG_IDE_RECEIPT_TIME_H
#define XLANG_IDE_RECEIPT_TIME_H
#include "app_xrt.h"
#include <stdio.h>
#include <string.h>

/* File handles are observed in Unix microseconds. Wire v1 uses those values;
   v2 uses XRT's Common Era UTC milliseconds. Keep handle-to-handle checks at
   full precision; convert only the comparison against compiler metadata. */
static inline int64 xideReceiptMilliseconds(int64 value)
{
 int64 ms=value/1000;
 if(value<0 && value%1000) --ms;
 return ms+INT64_C(62135596800000);
}
static inline xfileinfo xideReceiptInfo(const xfileinfo* info,int schema)
{
 xfileinfo result=*info;
 if(schema==2) {
  if(result.Available&XFILE_INFO_MODIFY_TIME) result.Modified=xideReceiptMilliseconds(result.Modified);
  if(result.Available&XFILE_INFO_CHANGE_TIME) result.Changed=xideReceiptMilliseconds(result.Changed);
 }
 return result;
}
static inline const xvalue* xideReceiptField(const xvalue* value,const char* key)
{
 return xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,(xstrview){key,strlen(key)}) : NULL;
}
static inline int xideReceiptMetadataSchema(const xvalue* metadata)
{
 if(xrtValueType(metadata)!=XVALUE_OBJECT || xrtValueCount(metadata)!=6) return 0;
 int us=xideReceiptField(metadata,"modifiedUs") && xideReceiptField(metadata,"changedUs");
 int ms=xideReceiptField(metadata,"modifiedMs") && xideReceiptField(metadata,"changedMs");
 int schema=us && !ms ? 1 : ms && !us ? 2 : 0;
 xstrview modified={0},changed={0};
 return schema && xrtValueGetString(xideReceiptField(metadata,schema==1 ? "modifiedUs" : "modifiedMs"),&modified) &&
  xrtValueGetString(xideReceiptField(metadata,schema==1 ? "changedUs" : "changedMs"),&changed) ? schema : 0;
}
static inline int xideReceiptMetadataTimesMatch(const xvalue* metadata,const xfileinfo* info)
{
 int schema=xideReceiptMetadataSchema(metadata);
 if(!schema) return 0;
 xfileinfo wire=xideReceiptInfo(info,schema); char text[32]; xstrview value={0};
 snprintf(text,sizeof(text),"%lld",(long long)wire.Modified);
 if(!xrtValueGetString(xideReceiptField(metadata,schema==1 ? "modifiedUs" : "modifiedMs"),&value) || value.Size!=strlen(text) || memcmp(value.Data,text,value.Size)) return 0;
 snprintf(text,sizeof(text),"%lld",(long long)wire.Changed);
 return xrtValueGetString(xideReceiptField(metadata,schema==1 ? "changedUs" : "changedMs"),&value) && value.Size==strlen(text) && !memcmp(value.Data,text,value.Size);
}
#endif
