#ifndef XLRT_GENERATED_H
#define XLRT_GENERATED_H

static int xlrt_attach_generated_runtime(void)
{
	if ( xrtThreadIsAttached() ) {
		return 0;
	}

	return (xrtThreadAttachCurrent() != NULL) ? 1 : -1;
}

static void xlrt_detach_generated_runtime(int bAttachedHere)
{
	if ( (bAttachedHere > 0) && xrtThreadIsAttached() ) {
		xrtThreadDetachCurrent();
	}
}

static void xl_core_print_int(int value)
{
	printf("%d", value);
}

static void xl_core_print_float(double value)
{
	printf("%g", value);
}

static void xl_core_print_string(const char* value)
{
	fputs((value != NULL) ? value : "", stdout);
}

static void xl_core_print_bool(int value)
{
	fputs(value ? "true" : "false", stdout);
}

static void xl_core_println_void(void)
{
	fputc('\n', stdout);
}

static void xl_core_println_int(int value)
{
	xl_core_print_int(value);
	xl_core_println_void();
}

static void xl_core_println_float(double value)
{
	xl_core_print_float(value);
	xl_core_println_void();
}

static void xl_core_println_string(const char* value)
{
	xl_core_print_string(value);
	xl_core_println_void();
}

static void xl_core_println_bool(int value)
{
	xl_core_print_bool(value);
	xl_core_println_void();
}

#endif
