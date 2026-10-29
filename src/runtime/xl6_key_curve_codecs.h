#ifndef XL6_KEY_CURVE_CODECS_H
#define XL6_KEY_CURVE_CODECS_H
#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"
extern const XL6_KeyImportV1 xl6CryptoX25519ImportSpecV1;
extern const XL6_KeyExportV1 xl6CryptoX25519ExportSpecV1;
extern const XL6_KeyImportV1 xl6CryptoX448ImportSpecV1;
extern const XL6_KeyExportV1 xl6CryptoX448ExportSpecV1;
extern const XL6_KeyImportV1 xl6CryptoEd25519ImportSpecV1;
extern const XL6_KeyExportV1 xl6CryptoEd25519ExportSpecV1;
extern const XL6_KeyImportV1 xl6CryptoP256ImportSpecV1;
extern const XL6_KeyExportV1 xl6CryptoP256ExportSpecV1;
extern const XL6_KeyImportV1 xl6CryptoP384ImportSpecV1;
extern const XL6_KeyExportV1 xl6CryptoP384ExportSpecV1;
void* xl6CryptoX25519FromPkcs8SpanV1(xbytesview);
bool xl6CryptoX25519WritePkcs8SpanV1(void*,bool,xbuffer*);
bool xl6CryptoX25519WriteSpkiSpanV1(void*,bool,xbuffer*);
void* xl6CryptoX448FromPkcs8SpanV1(xbytesview);
bool xl6CryptoX448WritePkcs8SpanV1(void*,bool,xbuffer*);
bool xl6CryptoX448WriteSpkiSpanV1(void*,bool,xbuffer*);
void* xl6CryptoEd25519FromPkcs8SpanV1(xbytesview);
bool xl6CryptoEd25519WritePkcs8SpanV1(void*,bool,xbuffer*);
bool xl6CryptoEd25519WriteSpkiSpanV1(void*,bool,xbuffer*);
void* xl6CryptoP256FromPkcs8SpanV1(xbytesview);
bool xl6CryptoP256WritePkcs8SpanV1(void*,bool,xbuffer*);
void* xl6CryptoP256FromSec1SpanV1(xbytesview);
bool xl6CryptoP256WriteSec1SpanV1(void*,bool,xbuffer*);
bool xl6CryptoP256WriteSpkiSpanV1(void*,bool,xbuffer*);
void* xl6CryptoP384FromPkcs8SpanV1(xbytesview);
bool xl6CryptoP384WritePkcs8SpanV1(void*,bool,xbuffer*);
void* xl6CryptoP384FromSec1SpanV1(xbytesview);
bool xl6CryptoP384WriteSec1SpanV1(void*,bool,xbuffer*);
bool xl6CryptoP384WriteSpkiSpanV1(void*,bool,xbuffer*);
void* xl6CryptoRsaPublicFromSpkiSpanV1(xbytesview);
bool xl6CryptoRsaPublicWriteSpkiSpanV1(void*,bool,xbuffer*);
void* xl6CryptoRsaPublicFromPkcs1SpanV1(xbytesview);
bool xl6CryptoRsaPublicWritePkcs1SpanV1(void*,bool,xbuffer*);
void* xl6CryptoRsaPrivateFromPkcs8SpanV1(xbytesview);
bool xl6CryptoRsaPrivateWritePkcs8SpanV1(void*,bool,xbuffer*);
void* xl6CryptoRsaPrivateFromPkcs1SpanV1(xbytesview);
bool xl6CryptoRsaPrivateWritePkcs1SpanV1(void*,bool,xbuffer*);
#endif
