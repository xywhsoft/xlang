#include <string.h>

#include "xrt.h"
#include "xlrt_generated.h"
int main(void)
{
	xvalue null_value;
	xvalue boxed;
	xvalue boxed_copy;
	xvalue point_value;
	void* raw;
	void* again;
	const char* text;
	const char* text_copy;
	int result;

	xlrt_native_error_clear();
	xrtThreadAttachCurrent();

	null_value = xvoCreateNull();
	raw = xvoGetPoint(null_value);
	if ( xlrt_native_error_has() ) {
		xvoUnref(null_value);
		xrtThreadDetachCurrent();
		return 11;
	}
	xvoUnref(null_value);

	boxed = xvoCreatePoint(raw);
	if ( xlrt_native_error_has() ) {
		xvoUnref(boxed);
		xrtThreadDetachCurrent();
		return 12;
	}

	boxed_copy = xvoCopy(boxed);
	again = xvoGetPoint(boxed_copy);
	if ( xlrt_native_error_has() ) {
		xvoUnref(boxed_copy);
		xvoUnref(boxed);
		xrtThreadDetachCurrent();
		return 13;
	}
	xvoUnref(boxed_copy);

	point_value = xvoCreatePoint(again);
	text = xl_value_to_string(point_value);
	if ( xlrt_native_error_has() ) {
		xvoUnref(point_value);
		xvoUnref(boxed);
		xrtThreadDetachCurrent();
		return 14;
	}

	text_copy = (const char*)xrtCopyStr((str)((text != NULL) ? text : ""), strlen((text != NULL) ? text : ""));
	if ( xlrt_native_error_has() ) {
		xvoUnref(point_value);
		xvoUnref(boxed);
		xrtThreadDetachCurrent();
		return 15;
	}

	result = ((text_copy != NULL) && (text_copy[0] != '\0')) ? 19 : 0;
	xvoUnref(point_value);
	xvoUnref(boxed);
	xrtThreadDetachCurrent();
	return result;
}
