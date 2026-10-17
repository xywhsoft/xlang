#include "xl6_crypto_internal.h"
xvalue* xl6CryptoHkdfSha256V1(const xvalue* ikm,const xvalue* salt,const xvalue* info,int64_t length)
{xbytesview a,b,c;size_t n;
 if (!xl6CryptoMaterial(ikm,&a) || !xl6CryptoMaterial(salt,&b) || !xl6CryptoMaterial(info,&c)) return NULL;
 if (!xl6CryptoLength(length,255u*32u,&n)) return NULL;
 void* output=xrtMalloc(n?n:1);if (!output) return NULL;
 bool ok=xrtHkdfSha256(b.Data,b.Size,a.Data,a.Size,c.Data,c.Size,output,n);
 return xl6CryptoTakeSecret(output,n,ok);}
xvalue* xl6CryptoHkdfSha256ExtractV1(const xvalue* ikm,const xvalue* salt)
{xbytesview a,b,c;size_t n;
 if (!xl6CryptoMaterial(ikm,&a) || !xl6CryptoMaterial(salt,&b)) return NULL;
 n=32;
 void* output=xrtMalloc(n?n:1);if (!output) return NULL;
 bool ok=xrtHkdfSha256Extract(b.Data,b.Size,a.Data,a.Size,output);
 return xl6CryptoTakeSecret(output,n,ok);}
xvalue* xl6CryptoHkdfSha256ExpandV1(const xvalue* prk,const xvalue* info,int64_t length)
{xbytesview a,b,c;size_t n;
 if (!xl6CryptoMaterial(prk,&a) || !xl6CryptoMaterial(info,&b)) return NULL;
 if (a.Size!=32) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
 if (!xl6CryptoLength(length,255u*32u,&n)) return NULL;
 void* output=xrtMalloc(n?n:1);if (!output) return NULL;
 bool ok=xrtHkdfSha256Expand(a.Data,a.Size,b.Data,b.Size,output,n);
 return xl6CryptoTakeSecret(output,n,ok);}
