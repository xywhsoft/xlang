#ifndef XIDE_REFERENCE_DOCUMENT_H
#define XIDE_REFERENCE_DOCUMENT_H
#include "platform/path.h"
#include <xrt.h>
#include <limits.h>
#include <string.h>
/* Shared transport/plan structural validation; no text or disk reads. */
static inline const xvalue* xideReferenceDocumentField(const xvalue* o,const char* key)
{return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,xrtStrView(key)) : NULL;}
static inline const char* xideReferenceDocumentString(const xvalue* v,size_t limit)
{xstrview s={0};return v && xrtValueGetString(v,&s) && s.Size && s.Size<=limit && !memchr(s.Data,0,s.Size) ? s.Data : NULL;}
static inline int xideReferenceDocumentInteger(const xvalue* v,int* out)
{int64 n=0;if(!v || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX)return 0;*out=(int)n;return 1;}
static inline int xideReferenceDocumentHex(const xvalue* v)
{const char* s=xideReferenceDocumentString(v,64);if(!s || strlen(s)!=64)return 0;for(size_t i=0;i<64;++i)if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f')))return 0;return 1;}
static inline int xideReferenceDocumentValid(const xvalue* d,const char* path)
{
 int size=0;const char *declared=xideReferenceDocumentString(xideReferenceDocumentField(d,"path"),65535),*origin=xideReferenceDocumentString(xideReferenceDocumentField(d,"origin"),16);
 if(!path || !declared || !xidePathLexicalEqual(path,declared) || !origin || !xideReferenceDocumentInteger(xideReferenceDocumentField(d,"normalizedSize"),&size) || size>8*1024*1024 || !xideReferenceDocumentHex(xideReferenceDocumentField(d,"normalizedSha256")))return 0;
 const xvalue* input=xideReferenceDocumentField(d,"input");
 if(!strcmp(origin,"overlay")){int64 version=0;if(input || !xrtValueGetInt(xideReferenceDocumentField(d,"version"),&version) || version<INT_MIN || version>INT_MAX)return 0;}
 else if(!strcmp(origin,"disk")){const char *file=xideReferenceDocumentString(xideReferenceDocumentField(input,"path"),65535),*kind=xideReferenceDocumentString(xideReferenceDocumentField(input,"origin"),16);if(xideReferenceDocumentField(d,"version") || !file || !xidePathLexicalEqual(path,file) || !kind || strcmp(kind,"disk") || !xideReferenceDocumentInteger(xideReferenceDocumentField(input,"rawSize"),&size) || size>8*1024*1024 || !xideReferenceDocumentHex(xideReferenceDocumentField(input,"rawSha256")))return 0;}
 else return 0;
 return 1;
}
#endif
