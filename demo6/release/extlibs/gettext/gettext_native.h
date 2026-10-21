#ifndef XL_GETTEXT_NATIVE_H
#define XL_GETTEXT_NATIVE_H

#include <stdint.h>
typedef struct xvalue xvalue;

#define XGT_MAX_CATALOG_BYTES (64u * 1024u * 1024u)

#ifdef __cplusplus
extern "C" {
#endif

/* 目录返回值用 xrtValueRelease；查询结果用 xrtFree。 */
xvalue *xgt_parse_po(const char *text);
xvalue *xgt_parse_mo(xvalue *bytes_value);
xvalue *xgt_load_mo(const char *path);
void *xgt_t(xvalue *catalog, const char *msgid);
void *xgt_tn(xvalue *catalog, const char *msgid,
    const char *msgid_plural, int64_t count);
void *xgt_tc(xvalue *catalog, const char *context, const char *msgid);
void *xgt_tnc(xvalue *catalog, const char *context, const char *msgid,
    const char *msgid_plural, int64_t count);

#ifdef __cplusplus
}
#endif
#endif
