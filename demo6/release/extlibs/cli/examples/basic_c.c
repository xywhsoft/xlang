#include "../include/cli_native.h"

#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 使用包内解析器；正式包仍按清单分离编译。 */
#include "../cli_native.c"
#endif

static xstrview view(const char* text)
{
	return (xstrview){ text, strlen(text) };
}

static bool add_text(xvalue* array, const char* text)
{
	return xrtValueArrayAppendNew(array, xrtValueString(view(text)));
}

static xvalue* field(xvalue* object, const char* name)
{
	return object != NULL ? xrtValueObjectGet(object, view(name)) : NULL;
}

int main(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* options = xrtValueObject();
	xvalue* positional = xrtValueArray();
	xvalue* result = NULL;
	xstrview output, source;
	bool verbose = false;
	int status = 1;

	if ( args == NULL || spec == NULL || flags == NULL ||
		options == NULL || positional == NULL ||
		!add_text(args, "--verbose") ||
		!add_text(args, "--output=out.txt") ||
		!add_text(args, "in.txt") ||
		!add_text(flags, "verbose") ||
		!add_text(positional, "src") ||
		!xrtValueObjectSetNew(options, view("output"),
			xrtValueString(view("o"))) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("positional"), &positional) ) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	if ( result == NULL ||
		!xrtValueGetBool(field(field(result, "flags"), "verbose"), &verbose) ||
		!xrtValueGetString(field(field(result, "options"), "output"), &output) ||
		!xrtValueGetString(field(field(result, "positional"), "src"), &source) ) {
		goto cleanup;
	}
	printf("verbose=%s output=%.*s src=%.*s\n", verbose ? "true" : "false",
		(int)output.Size, output.Data, (int)source.Size, source.Data);
	status = 0;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(positional);
	xrtValueRelease(options);
	xrtValueRelease(flags);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return status;
}
