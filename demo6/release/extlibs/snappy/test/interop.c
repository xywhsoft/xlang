#include "xrt_snappy.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv)
{
	unsigned char chunk[8192];
	xbuffer* input = NULL;
	xbuffer* output = NULL;
	xbytesview view;
	xrtsnappyerror error;
	size_t count;
	int ok;

	if (argc != 2 || (strcmp(argv[1], "encode") != 0 && strcmp(argv[1], "decode") != 0)) {
		fprintf(stderr, "usage: interop encode|decode\n");
		return 2;
	}
#if defined(_WIN32)
	if (_setmode(_fileno(stdin), _O_BINARY) < 0 || _setmode(_fileno(stdout), _O_BINARY) < 0) {
		return 2;
	}
#endif
	input = xrtBufferCreate();
	if (input == NULL) { return 1; }
	while ((count = fread(chunk, 1u, sizeof(chunk), stdin)) != 0u) {
		if (!xrtBufferAppend(input, (xbytesview){chunk, count})) {
			xrtBufferDestroy(input);
			return 1;
		}
	}
	if (ferror(stdin)) {
		xrtBufferDestroy(input);
		return 1;
	}
	view = xrtBufferView(input);
	if (strcmp(argv[1], "encode") == 0) {
		ok = xrtSnappyCompress(view.Data, view.Size, NULL, &output, &error);
	} else {
		ok = xrtSnappyDecompress(view.Data, view.Size, NULL, &output, &error);
	}
	xrtBufferDestroy(input);
	if (!ok) {
		fprintf(stderr, "%s\n", error.sMessage);
		return 1;
	}
	view = xrtBufferView(output);
	ok = fwrite(view.Data, 1u, view.Size, stdout) == view.Size && fflush(stdout) == 0;
	xrtBufferDestroy(output);
	return ok ? 0 : 1;
}
