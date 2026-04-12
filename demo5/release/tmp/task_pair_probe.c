#include <stdio.h>
#include "test/xl_test.h"
int main(void)
{
	int rc = 0;
	printf("run=task_basic\n");
	rc = xlTestRunSelector("runtime.task_basic");
	printf("rc1=%d\n", rc);
	printf("run=task_value_basic\n");
	rc = xlTestRunSelector("runtime.task_value_basic");
	printf("rc2=%d\n", rc);
	return rc;
}
