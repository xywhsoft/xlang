#include <stdio.h>

#include "xrt.h"
#include "xlrt_generated.h"

int main(void)
{
	int iAttached;

	iAttached = xlrt_attach_generated_runtime();
	printf("host=%s\n", xlrt_host_name());
	printf("cwd=%s\n", xlrt_env_cwd());
	printf("now=%s\n", xrtNowStr());
	xlrt_detach_generated_runtime(iAttached);
	return xlrt_env_exists("PATH") ? 7 : 3;
}
