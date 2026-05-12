#include <stddef.h>

#include "xl_test.h"
#include "xrt.h"

int main(int argc, char** argv)
{
	xrtInit();
	return xlTestRunSelector((argc > 1) ? argv[1] : NULL);
}
