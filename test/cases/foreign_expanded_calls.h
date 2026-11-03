#ifndef FOREIGN_EXPANDED_CALLS_H
#define FOREIGN_EXPANDED_CALLS_H
#include "test/cases/foreign_managed_view.h"
typedef struct SpanPoint {float x,y;} SpanPoint;
int span_points_write(SpanPoint*,int);
int span_points_sum(const SpanPoint*,int);
typedef struct span_floats {float values[3];} span_floats;
#endif
