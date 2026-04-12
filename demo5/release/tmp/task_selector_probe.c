#include <stdio.h>
#include "test/xl_test.h"
int main(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
	int rc = xlTestRunSelector("runtime.task");
	printf("rc=%d\n", rc);
	return rc;
}
