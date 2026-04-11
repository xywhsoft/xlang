#include "xrt.h"
#include "xlrt_generated.h"

int main(void)
{
	xvalue value;
	const char* text;
	int result;

	xrtThreadAttachCurrent();
	value = xvoCreatePoint(NULL);
	text = xl_value_to_string(value);
	result = ((text != NULL) && (text[0] != '\0')) ? 19 : 0;
	xvoUnref(value);
	xrtThreadDetachCurrent();
	return result;
}
