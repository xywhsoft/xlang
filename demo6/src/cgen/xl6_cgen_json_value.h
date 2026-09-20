#ifndef XL6_CGEN_JSON_VALUE_H
#define XL6_CGEN_JSON_VALUE_H

#include "src/common/xl6_output.h"

/* Backend-neutral operation set. The caller selects verified dependencies;
 * the emitter never inspects declarations, source names, or IR types. */
enum {
    XL6_CGEN_JSON_PARSE = 1u,
    XL6_CGEN_JSON_VALID = 2u,
    XL6_CGEN_JSON_PARSE_FILE = 4u,
    XL6_CGEN_JSON_STRINGIFY = 8u,
    XL6_CGEN_JSON_WRITE_FILE = 16u,
    XL6_CGEN_JSON_VALUE_ALL = 31u
};
void xl6CgenEmitJsonValueRuntime(
    XL6_Output* out, unsigned operations, int hasXrtDeclarations);

#endif
