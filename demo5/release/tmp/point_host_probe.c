#include "xrt.h"

int main(void)
{
	xvalue value;
	ptr data;

	xrtThreadAttachCurrent();
	value = xvoCreatePoint(NULL);
	data = xvoGetPoint(value);
	xvoUnref(value);
	xrtThreadDetachCurrent();
	return (data == NULL) ? 19 : 7;
}
