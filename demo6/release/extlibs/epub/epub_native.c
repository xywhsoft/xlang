/*
 * epub_native.c - EPUB e-book format helpers
 * Tier 3: document format utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xepub_version(long long id) {
    switch (id) {
        case 2: return "EPUB 2.0.1";
        case 3: return "EPUB 3.3";
        default: return "Unknown";
    }
}

const char* xepub_mimetype(long long id) {
    switch (id) {
        case 0: return "application/epub+zip";
        case 1: return "application/xhtml+xml";
        case 2: return "text/css";
        case 3: return "image/jpeg";
        case 4: return "image/png";
        case 5: return "image/svg+xml";
        case 6: return "audio/mpeg";
        case 7: return "video/mp4";
        case 8: return "application/x-dtbncx+xml";
        case 9: return "application/smil+xml";
        default: return "application/octet-stream";
    }
}

const char* xepub_spine_prop(long long id) {
    switch (id) {
        case 0: return "linear";
        case 1: return "non-linear";
        case 2: return "page-spread-left";
        case 3: return "page-spread-right";
        case 4: return "rendition:layout-pre-paginated";
        case 5: return "rendition:layout-reflowable";
        case 6: return "rendition:spread-landscape";
        case 7: return "rendition:spread-portrait";
        default: return "Unknown";
    }
}

const char* xepub_nav_type(long long id) {
    switch (id) {
        case 0: return "toc";
        case 1: return "landmarks";
        case 2: return "page-list";
        case 3: return "lot";
        case 4: return "loi";
        case 5: return "loa";
        case 6: return "lov";
        default: return "Unknown";
    }
}

const char* xepub_metadata_field(long long id) {
    switch (id) {
        case 0: return "dc:title";
        case 1: return "dc:creator";
        case 2: return "dc:language";
        case 3: return "dc:identifier";
        case 4: return "dc:date";
        case 5: return "dc:publisher";
        case 6: return "dc:description";
        case 7: return "dc:subject";
        case 8: return "dc:rights";
        case 9: return "meta";
        default: return "Unknown";
    }
}

long long xepub_validate_isbn(const char *isbn) {
    int len = 0;
    int sum = 0;
    int i;
    for (i = 0; isbn[i]; i++) {
        if (isbn[i] >= '0' && isbn[i] <= '9') {
            int d = isbn[i] - '0';
            if (len % 2 == 0) sum += d;
            else sum += d * 3;
            len++;
        } else if (isbn[i] == 'X' || isbn[i] == 'x') {
            if (len == 9) { sum += 10; len++; }
            else return 0;
        }
    }
    if (len == 13) return (sum % 10 == 0) ? 1 : 0;
    if (len == 10) {
        /* ISBN-10 check */
        sum = 0;
        int pos = 0;
        for (i = 0; isbn[i]; i++) {
            if (isbn[i] >= '0' && isbn[i] <= '9') {
                sum += (isbn[i] - '0') * (10 - pos);
                pos++;
            } else if ((isbn[i] == 'X' || isbn[i] == 'x') && pos == 9) {
                sum += 10;
                pos++;
            }
        }
        return (sum % 11 == 0) ? 1 : 0;
    }
    return 0;
}

const char* xepub_encryption(long long id) {
    switch (id) {
        case 0: return "none";
        case 1: return "Adobe DRM";
        case 2: return "IDPF font obfuscation";
        case 3: return "AES-128-CBC";
        case 4: return "AES-256-CBC";
        default: return "Unknown";
    }
}
