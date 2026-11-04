#ifndef FOREIGN_VALUE_PROJECTION_H
#define FOREIGN_VALUE_PROJECTION_H
#include "test/cases/foreign_callback_span.h"
#include "test/cases/foreign_inplace_factory.h"
#ifdef __cplusplus
extern "C" {
#endif
int value_projection_size(xbytesview);
int value_projection_pair(xbytesview, xbytesview);
int value_projection_record(PlainRecord);
#ifdef __cplusplus
}
#endif
#endif
