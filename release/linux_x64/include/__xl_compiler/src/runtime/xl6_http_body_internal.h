#ifndef XL6_HTTP_BODY_INTERNAL_H
#define XL6_HTTP_BODY_INTERNAL_H
#include "xl6_http_internal.h"
typedef struct XL6_HttpBodyReader {
    xatomic32 gate;xhttp1body body;xhttpfield* scratch;XL6_HttpHeaders* trailers;
    size_t consumed;bool failed;
} XL6_HttpBodyReader;
/* Caller holds the body mutation guard; consumes no external queue. */
int64_t xl6HttpBodyReadLocked(XL6_HttpBodyReader*,xbytesview,xbuffer*,bool);
#endif
