#ifndef XLS_MODEL_SOURCE_CONTROL_H
#define XLS_MODEL_SOURCE_CONTROL_H
#include "analysis/frontend_raw_body.h"
/* Construction-thread stack metadata carried by raw.modelSource. The
 * previous checked module remains caller-owned; no model/payload cache here. */
typedef struct xls_model_source_control {const void* memo;size_t prefixEnd;int prefixActive;} xls_model_source_control;
size_t xlsRawBodyModelPrefix(const xls_raw_scope*,const xls_syntax*);
size_t xlsRawBodySyntaxUnitCount(const xls_syntax*);
xls_raw_unit* xlsRawBodySyntaxUnitAt(const xls_syntax*,size_t);
#endif
