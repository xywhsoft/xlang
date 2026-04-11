#include <string.h>
#include "xrt.h"
#include "xlrt_generated.h"

int main(int argc, char** argv)
{
	const char* mode;
	xvalue value;
	const char* text;
	xtime timeValue;
	int result;

	xrtThreadAttachCurrent();
	mode = (argc > 1) ? argv[1] : "";
	result = 0;

	if ( strcmp(mode, "float_from_text") == 0 ) {
		value = xvoCreateText((ptr)"2.5", 3u, FALSE);
		result = (xvoGetFloat(value) > 2.4 && xvoGetFloat(value) < 2.6) ? 11 : 1;
		xvoUnref(value);
	} else if ( strcmp(mode, "bool_from_text") == 0 ) {
		value = xvoCreateText((ptr)"ok", 2u, FALSE);
		result = xvoGetBool(value) ? 12 : 1;
		xvoUnref(value);
	} else if ( strcmp(mode, "time_from_text") == 0 ) {
		value = xvoCreateText((ptr)"2025-01-01", 10u, FALSE);
		timeValue = xvoGetTime(value);
		result = (timeValue != 0) ? 13 : 1;
		xvoUnref(value);
	} else if ( strcmp(mode, "string_from_int") == 0 ) {
		value = xvoCreateInt((int64)3);
		text = xl_value_to_string(value);
		result = (text != NULL && strcmp(text, "3") == 0) ? 14 : 1;
		xvoUnref(value);
	} else if ( strcmp(mode, "string_from_time") == 0 ) {
		value = xvoCreateTime((xtime)xrtStrToTime((str)"2025-01-01", 10u));
		text = xl_value_to_string(value);
		result = (text != NULL && strstr(text, "2025-01-01") != NULL) ? 15 : 1;
		xvoUnref(value);
	}

	xrtThreadDetachCurrent();
	return result;
}
