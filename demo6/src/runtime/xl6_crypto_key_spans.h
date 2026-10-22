#ifndef XL6_CRYPTO_KEY_SPANS_H
#define XL6_CRYPTO_KEY_SPANS_H
#include "include/xruntime_decl.h"
/* Private synchronous constructors. They copy the complete material into the
 * ordinary owned key payload; no borrowed view or foreign owner is retained. */
void* xl6CryptoX25519CreateSpanV1(xbytesview);
void* xl6CryptoX448CreateSpanV1(xbytesview);
void* xl6CryptoEd25519CreateSpanV1(xbytesview);
void* xl6CryptoP256CreateSpanV1(xbytesview);
void* xl6CryptoP384CreateSpanV1(xbytesview);
#endif
