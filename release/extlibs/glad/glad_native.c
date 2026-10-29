/*
 * glad_native.c - OpenGL loader generator
 * Tier 5: graphics
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

const char* xglad_gl_type(long long id) {
    switch (id) {
        case 0: return "GL_FLOAT";
        case 1: return "GL_INT";
        case 2: return "GL_UNSIGNED_BYTE";
        case 3: return "GL_UNSIGNED_INT";
        case 4: return "GL_DOUBLE";
        default: return "Unknown";
    }
}

const char* xglad_primitive(long long id) {
    switch (id) {
        case 0: return "GL_TRIANGLES";
        case 1: return "GL_LINES";
        case 2: return "GL_POINTS";
        case 3: return "GL_TRIANGLE_STRIP";
        case 4: return "GL_TRIANGLE_FAN";
        default: return "Unknown";
    }
}

/* Return byte size of GL type */
long long xglad_type_size(long long id) {
    switch (id) {
        case 0: return 4;  /* GL_FLOAT */
        case 1: return 4;  /* GL_INT */
        case 2: return 1;  /* GL_UNSIGNED_BYTE */
        case 3: return 4;  /* GL_UNSIGNED_INT */
        case 4: return 8;  /* GL_DOUBLE */
        default: return 0;
    }
}

const char* xglad_version(long long major, long long minor) {
    snprintf(buf, sizeof(buf), "OpenGL %lld.%lld", major, minor);
    return buf;
}

/* Check if extension name is valid format */
long long xglad_valid_ext(const char *name) {
    if (!name || !*name) return 0;
    if (strncmp(name, "GL_", 3) != 0) return 0;
    return 1;
}
