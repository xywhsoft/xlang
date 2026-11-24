#ifndef XL6_XGE3_CUSTOM_PROXY_H
#define XL6_XGE3_CUSTOM_PROXY_H

#include "xge3_callback_elements.h"
#include "__xl_runtime/xl6_foreign_writable_bytes.h"

/* Keep the named xlang pack distinct from explicit native descriptor imports.
 * The generated C calls the native value-owned backend factory directly. */
#define xl6Xui3CreateCustomProxy xuiProxyCreateCustomOwned

#endif
