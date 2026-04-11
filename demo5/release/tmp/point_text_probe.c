#include <string.h>
#include "xrt.h"

int main(void)
{
	xvalue value;
	const char* text;
	int result;

	xrtThreadAttachCurrent();
	value = xvoCreatePoint(NULL);
	text = xvoGetText(value);
	result = (text != NULL && strlen(text) > 0) ? 19 : 0;
	xvoUnref(value);
	xrtThreadDetachCurrent();
	return result;
}
