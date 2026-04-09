#include <stddef.h>

#include "xl_test.h"

int main(int argc, char** argv)
{
	return xlTestRunSelector((argc > 1) ? argv[1] : NULL);
}
