#ifndef XEP_INTERNAL_H
#define XEP_INTERNAL_H

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

#define XEP_LIMIT (8u * 1024u * 1024u)
#define XEP_MAX_ENTRIES 104u

typedef struct {
    char name[64];
    uint32 crc;
    uint32 size;
    uint32 offset;
} XepEntry;

typedef struct {
    xbuffer data;
    XepEntry entries[XEP_MAX_ENTRIES];
    size_t count;
} XepArchive;

int xep_append(xbuffer *out, const void *data, size_t size);
int xep_literal(xbuffer *out, const char *text);
int xep_archive_add(XepArchive *archive, const char *name,
    const void *data, size_t size);
int xep_archive_finish(XepArchive *archive);
void *xep_read_entry(const char *path, const char *name);

#endif
