#include <stdint.h>
#include "../bson_native.h"

/* XLang int 的 C ABI 在 LP64 平台也应保持 int64_t。 */
extern int64_t xbson_save(const char* path, xvalue* document);
extern int64_t xbson_doc_length(xvalue* data);
extern int64_t xbson_oid_time(const char* hex24);
