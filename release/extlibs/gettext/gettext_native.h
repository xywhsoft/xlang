#ifndef XL_GETTEXT_NATIVE_H
#define XL_GETTEXT_NATIVE_H

#include <stdint.h>
typedef struct xvalue xvalue;
typedef struct xbytesview xbytesview;

#define XGT_MAX_CATALOG_BYTES (64u * 1024u * 1024u)

#ifdef __cplusplus
extern "C" {
#endif

/* 目录返回值用 xrtValueRelease；以下四个查询结果用 xrtFree。 */
xvalue *xgt_parse_po(const char *text);
xvalue *xgt_parse_mo(xvalue *bytes_value);
xvalue *xgt_parse_mo_span(xbytesview data);
xvalue *xgt_load_mo(const char *path);
char *xgt_t(xvalue *catalog, const char *msgid);
char *xgt_tn(xvalue *catalog, const char *msgid,
    const char *msgid_plural, int64_t count);
char *xgt_tc(xvalue *catalog, const char *context, const char *msgid);
char *xgt_tnc(xvalue *catalog, const char *context, const char *msgid,
    const char *msgid_plural, int64_t count);

/* 借用目录译文或传入的原文；调用期间目录和原文必须保持有效，不得释放结果。
 * catalog 必须是目录对象；缺 context/msgid 时仍检查其余实参和复数规则。
 * tc/tnc 的 NULL context 表示原文返回，空字符串则是可查询的空上下文。 */
const char *xgt_t_borrowed(xvalue *catalog, const char *msgid);
const char *xgt_tn_borrowed(xvalue *catalog, const char *msgid,
    const char *msgid_plural, int64_t count);
const char *xgt_tc_borrowed(xvalue *catalog, const char *context, const char *msgid);
const char *xgt_tnc_borrowed(xvalue *catalog, const char *context, const char *msgid,
    const char *msgid_plural, int64_t count);

#ifdef __cplusplus
}
#endif
#endif
