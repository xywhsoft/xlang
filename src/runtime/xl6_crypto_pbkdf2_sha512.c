#include "xl6_crypto_internal.h"
xvalue* xl6CryptoPbkdf2Sha512V1(const xvalue* password,const xvalue* salt,int64_t iterations,int64_t length)
{xbytesview p,s;size_t n;
 if (!xl6CryptoMaterial(password,&p) || !xl6CryptoMaterial(salt,&s)) return NULL;
 if (iterations<=0 || (uint64_t)iterations>UINT32_MAX) {xrtSetErrorKind(XERR_RANGE);return NULL;}
 if (!xl6CryptoLength(length,16u*1024u*1024u,&n)) return NULL;
 void* output=xrtMalloc(n?n:1);if (!output) return NULL;
 bool ok=xrtPbkdf2Sha512(p.Data,p.Size,s.Data,s.Size,(uint32)iterations,output,n);
 return xl6CryptoTakeSecret(output,n,ok);}

/* Synchronous readonly span: the caller keeps the input owner alive. */
xvalue* xl6CryptoPbkdf2Sha512SpanV1(xbytesview password,xbytesview salt,int64_t iterations,int64_t length)
{xbytesview p,s;size_t n;
 if (!xl6BufferSpanInput(password,-1,&p) || !xl6BufferSpanInput(salt,-1,&s)) return NULL;
 if (iterations<=0 || (uint64_t)iterations>UINT32_MAX) {xrtSetErrorKind(XERR_RANGE);return NULL;}
 if (!xl6CryptoLength(length,16u*1024u*1024u,&n)) return NULL;
 void* output=xrtMalloc(n?n:1);if (!output) return NULL;
 bool ok=xrtPbkdf2Sha512(p.Data,p.Size,s.Data,s.Size,(uint32)iterations,output,n);
 return xl6CryptoTakeSecret(output,n,ok);}
