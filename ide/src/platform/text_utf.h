#ifndef XIDE_PLATFORM_TEXT_UTF_H
#define XIDE_PLATFORM_TEXT_UTF_H
#include <xrt.h>

/* Verified UTF-8 prefix of a bounded (at most 4096-byte) chunk, limited to
 * complete UTF-16 scalars. Read/Written carry progress even for a split tail
 * or a capacity boundary. Invalid input can return no progress. No allocation.
 * Windows uses its existing strict system codec; other hosts reuse XRT. */
xutfresult xideUtf8To16Prefix(xstrview text,int maximum);
#endif
