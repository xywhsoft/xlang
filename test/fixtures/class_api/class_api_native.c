#include <stdint.h>

#define XRT_MODULE_VALUE
#include "xrt.h"

int64_t xl6_class_api_sum(xvalue* point)
{
	xvalue* x;
	xvalue* y;
	int64_t xValue;
	int64_t yValue;

	x = xrtValueObjectGet(point, xrtStrView("x"));
	y = xrtValueObjectGet(point, xrtStrView("y"));
	if ( x == NULL || y == NULL ||
		 !xrtValueGetInt(x, &xValue) || !xrtValueGetInt(y, &yValue) ) {
		return 0;
	}
	return xValue + yValue;
}
