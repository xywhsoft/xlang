#ifndef XIDE_APP_FEATURES_H
#define XIDE_APP_FEATURES_H

/* The traditional IDE ships without a C LSP runtime. Keep the implementation
 * available for an explicit developer build; C editing and xlang compilation
 * do not depend on this option. */
#ifndef XIDE_ENABLE_C_LSP
#define XIDE_ENABLE_C_LSP 0
#endif
#if XIDE_ENABLE_C_LSP != 0 && XIDE_ENABLE_C_LSP != 1
#error "XIDE_ENABLE_C_LSP must be 0 or 1"
#endif

#endif
