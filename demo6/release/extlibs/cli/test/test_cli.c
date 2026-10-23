#define XRT_MODULE_MEMORY_DEBUG
#include "../include/cli_native.h"

#include <stdio.h>
#include <string.h>

static xstrview view(const char* text)
{
	return (xstrview){ text, strlen(text) };
}

static bool add_text(xvalue* array, const char* text, size_t size)
{
	return xrtValueArrayAppendNew(array,
		xrtValueString((xstrview){ text, size }));
}

static bool test_long_inline_value(void)
{
	char token[1034];
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* result = NULL;
	xvalue* parsed;
	xstrview value;
	bool ok = false;
	memcpy(token, "--output=", 9u);
	memset(token + 9u, 'x', 1024u);
	token[1033] = '\0';
	if ( args == NULL || spec == NULL || options == NULL ||
		!add_text(args, token, 1033u) ||
		!xrtValueObjectSetNew(options, view("output"),
			xrtValueString(view("o"))) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	parsed = result != NULL ? xrtValueObjectGet(result,
		view("options")) : NULL;
	parsed = parsed != NULL ? xrtValueObjectGet(parsed,
		view("output")) : NULL;
	ok = parsed != NULL && xrtValueGetString(parsed, &value) &&
		value.Size == 1024u && memcmp(value.Data, token + 9u, 1024u) == 0;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_utf8_names(int short_form)
{
	static const char option[] = "\xE8\xBE\x93\xE5\x87\xBA"; /* 输出 */
	static const char positional[] = "\xE6\xBA\x90"; /* 源 */
	static const char value[] = "\xE7\xBB\x93\xE6\x9E\x9C.txt"; /* 结果.txt */
	static const char input[] = "\xE8\xBE\x93\xE5\x85\xA5.txt"; /* 输入.txt */
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* names = xrtValueArray();
	xvalue* result = NULL;
	xvalue* fields;
	xstrview text;
	bool accepted = false;
	bool ok = false;
	if (args == NULL || spec == NULL || options == NULL || names == NULL ||
		!xrtValueObjectSetNew(options, view(option), xrtValueString(view("o"))) ||
		!add_text(names, positional, strlen(positional)) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("positional"), &names)) goto cleanup;
	if (short_form == 2) {
		static const char attached[] = "-o\xE7\xBB\x93\xE6\x9E\x9C.txt";
		if (!add_text(args, attached, sizeof(attached) - 1u)) goto cleanup;
	} else if (short_form) {
		if (!add_text(args, "-o", 2u) || !add_text(args, value, strlen(value)))
			goto cleanup;
	} else {
		static const char long_token[] = "--\xE8\xBE\x93\xE5\x87\xBA="
			"\xE7\xBB\x93\xE6\x9E\x9C.txt";
		if (!add_text(args, long_token, sizeof(long_token) - 1u)) goto cleanup;
	}
	if (!add_text(args, input, strlen(input))) goto cleanup;
	result = xcli_parse(args, spec);
	fields = result != NULL ? xrtValueObjectGet(result, view("options")) : NULL;
	if (fields == NULL ||
		!xrtValueGetString(xrtValueObjectGet(fields, view(option)), &text) ||
		text.Size != strlen(value) || memcmp(text.Data, value, text.Size) != 0)
		goto cleanup;
	fields = xrtValueObjectGet(result, view("positional"));
	ok = fields != NULL &&
		xrtValueGetString(xrtValueObjectGet(fields, view(positional)), &text) &&
		text.Size == strlen(input) && memcmp(text.Data, input, text.Size) == 0 &&
		xrtValueGetBool(xrtValueObjectGet(result, view("ok")), &accepted) &&
		accepted && xrtGetError() == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(names);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_invalid_unicode_name(const char* name, size_t size,
	bool option)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* names = option ? xrtValueObject() : xrtValueArray();
	xvalue* result = NULL;
	bool ok = false;
	if (args == NULL || spec == NULL || names == NULL) goto cleanup;
	if (option) {
		if (!xrtValueObjectSetNew(names, (xstrview){ name, size },
			xrtValueString(view("n"))) ||
			!xrtValueObjectSetTake(spec, view("options"), &names)) goto cleanup;
	} else {
		if (!add_text(names, name, size) ||
			!xrtValueObjectSetTake(spec, view("flags"), &names)) goto cleanup;
	}
	result = xcli_parse(args, spec);
	ok = result == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(names);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_embedded_nul(void)
{
	static const char token[] = { 'a', '\0', 'b' };
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* result = NULL;
	bool ok = false;
	if ( args != NULL && spec != NULL && add_text(args, token, sizeof(token)) ) {
		result = xcli_parse(args, spec);
		ok = result == NULL;
	}
	xrtValueRelease(result);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_unknown_spec_field(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* result = NULL;
	bool ok = false;
	if ( args != NULL && spec != NULL &&
		 xrtValueObjectSetNew(spec, view("flagz"), xrtValueArray()) ) {
		result = xcli_parse(args, spec);
		ok = result == NULL && xrtGetError() != NULL &&
			xrtErrorKind(xrtGetError()) == XERR_VALUE;
	}
	xrtValueRelease(result);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_short_error_stops(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* result = NULL;
	xvalue* flag_values;
	xstrview message;
	bool accepted = true, verbose = true;
	bool ok = false;
	if (args == NULL || spec == NULL || flags == NULL ||
		!add_text(args, "-z", 2u) || !add_text(args, "-v", 2u) ||
		!add_text(args, "--bad", 5u) || !add_text(flags, "v", 1u) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags)) goto cleanup;
	result = xcli_parse(args, spec);
	flag_values = result != NULL ? xrtValueObjectGet(result, view("flags")) : NULL;
	ok = result != NULL &&
		xrtValueGetBool(xrtValueObjectGet(result, view("ok")), &accepted) &&
		!accepted &&
		xrtValueGetString(xrtValueObjectGet(result, view("error")), &message) &&
		message.Size == strlen("unknown short option -z") &&
		memcmp(message.Data, "unknown short option -z", message.Size) == 0 &&
		flag_values != NULL &&
		xrtValueGetBool(xrtValueObjectGet(flag_values, view("v")), &verbose) &&
		!verbose && xrtGetError() == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(flags);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_invalid_short_byte(const char* token, size_t size)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* result = NULL;
	xstrview message;
	bool accepted = true;
	bool ok = false;
	if (args == NULL || spec == NULL ||
		!add_text(args, token, size)) goto cleanup;
	result = xcli_parse(args, spec);
	ok = result != NULL &&
		xrtValueGetBool(xrtValueObjectGet(result, view("ok")), &accepted) &&
		!accepted &&
		xrtValueGetString(xrtValueObjectGet(result, view("error")), &message) &&
		message.Size == strlen("short option must be printable ASCII") &&
		memcmp(message.Data, "short option must be printable ASCII",
			message.Size) == 0;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_end_of_options(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* positions = xrtValueArray();
	xvalue* result = NULL;
	xvalue* fields;
	xstrview text;
	bool accepted = false, verbose = true;
	bool ok = false;
	if (args == NULL || spec == NULL || flags == NULL || positions == NULL ||
		!add_text(args, "--", 2u) || !add_text(args, "-v", 2u) ||
		!add_text(args, "--output=x", 10u) || !add_text(args, "-", 1u) ||
		!add_text(flags, "v", 1u) || !add_text(positions, "src", 3u) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags) ||
		!xrtValueObjectSetTake(spec, view("positional"), &positions)) goto cleanup;
	result = xcli_parse(args, spec);
	fields = result != NULL ? xrtValueObjectGet(result, view("flags")) : NULL;
	if (fields == NULL ||
		!xrtValueGetBool(xrtValueObjectGet(fields, view("v")), &verbose) ||
		verbose) goto cleanup;
	fields = xrtValueObjectGet(result, view("positional"));
	if (fields == NULL ||
		!xrtValueGetString(xrtValueObjectGet(fields, view("src")), &text) ||
		text.Size != 2u || memcmp(text.Data, "-v", 2u) != 0) goto cleanup;
	fields = xrtValueObjectGet(result, view("rest"));
	if (fields == NULL || xrtValueCount(fields) != 2 ||
		!xrtValueGetString(xrtValueArrayAt(fields, 0), &text) ||
		text.Size != 10u || memcmp(text.Data, "--output=x", 10u) != 0 ||
		!xrtValueGetString(xrtValueArrayAt(fields, 1), &text) ||
		text.Size != 1u || text.Data[0] != '-') goto cleanup;
	ok = xrtValueGetBool(xrtValueObjectGet(result, view("ok")), &accepted) &&
		accepted && xrtGetError() == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(positions);
	xrtValueRelease(flags);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_long_option_name(void)
{
	char name[201];
	char token[211];
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* result = NULL;
	xvalue* parsed;
	xstrview value;
	bool ok = false;
	memset(name, 'n', 200u);
	name[200] = '\0';
	memcpy(token, "--", 2u);
	memcpy(token + 2u, name, 200u);
	memcpy(token + 202u, "=value", 7u);
	if ( args == NULL || spec == NULL || options == NULL ||
		!add_text(args, token, 208u) ||
		!xrtValueObjectSetNew(options, view(name),
			xrtValueString(view("o"))) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	parsed = result != NULL ? xrtValueObjectGet(result,
		view("options")) : NULL;
	parsed = parsed != NULL ? xrtValueObjectGet(parsed,
		view(name)) : NULL;
	ok = parsed != NULL && xrtValueGetString(parsed, &value) &&
		value.Size == 5u && memcmp(value.Data, "value", 5u) == 0;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_long_error(int mode)
{
	char name[201];
	char token[207];
	char expected[256];
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* result = NULL;
	xvalue* error;
	xstrview message;
	bool accepted = true;
	bool ok = false;
	memset(name, 'n', 200u);
	name[200] = '\0';
	snprintf(token, sizeof(token), "--%s%s", name,
		mode == 2 ? "=yes" : "");
	if (args == NULL || spec == NULL || options == NULL || flags == NULL ||
		!add_text(args, token, strlen(token))) goto cleanup;
	if (mode == 1 && !xrtValueObjectSetNew(options, view(name),
		xrtValueString(view("o")))) goto cleanup;
	if (mode == 2 && !add_text(flags, name, 200u)) goto cleanup;
	if (!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags)) goto cleanup;
	result = xcli_parse(args, spec);
	if (mode == 0) snprintf(expected, sizeof(expected),
		"unknown option --%s", name);
	if (mode == 1) snprintf(expected, sizeof(expected),
		"missing value for --%s", name);
	if (mode == 2) snprintf(expected, sizeof(expected),
		"flag --%s does not take a value", name);
	error = result != NULL ? xrtValueObjectGet(result, view("error")) : NULL;
	ok = result != NULL &&
		xrtValueGetBool(xrtValueObjectGet(result, view("ok")), &accepted) &&
		!accepted && error != NULL && xrtValueGetString(error, &message) &&
		message.Size == strlen(expected) &&
		memcmp(message.Data, expected, message.Size) == 0 &&
		xrtGetError() == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(flags);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_flag_rejects_inline_value(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* result = NULL;
	xvalue* okValue;
	bool accepted = true;
	bool ok = false;
	if ( args == NULL || spec == NULL || flags == NULL ||
		!add_text(args, "--verbose=yes", 13u) ||
		!add_text(flags, "verbose", 7u) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags) ) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	okValue = result != NULL ? xrtValueObjectGet(result, view("ok")) : NULL;
	ok = okValue != NULL && xrtValueGetBool(okValue, &accepted) && !accepted;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(flags);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_invalid_spec_entry(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* result = NULL;
	bool ok = false;
	if ( args == NULL || spec == NULL || flags == NULL ||
		!xrtValueArrayAppendNew(flags, xrtValueInt(7)) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags) ) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	ok = result == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(flags);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_late_invalid_argument(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* result = NULL;
	bool ok = false;
	if ( args == NULL || spec == NULL ||
		!add_text(args, "first", 5u) ||
		!xrtValueArrayAppendNew(args, xrtValueInt(42)) ) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	ok = result == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_short_alias(const char* option_alias,
	const char* flag_alias, const char* single_flag, bool valid)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* shorts = xrtValueObject();
	xvalue* flags = xrtValueArray();
	xvalue* result = NULL;
	xvalue* parsed;
	xstrview output;
	bool verbose = false;
	bool ok = false;
	if (args == NULL || spec == NULL || options == NULL ||
		shorts == NULL || flags == NULL ||
		(valid && !add_text(args, "-voresult", 9u)) ||
		!xrtValueObjectSetNew(options, view("output"),
			xrtValueString(view(option_alias))) ||
		(flag_alias != NULL && !xrtValueObjectSetNew(shorts,
			view("verbose"), xrtValueString(view(flag_alias)))) ||
		(flag_alias != NULL && !add_text(flags, "verbose", 7u)) ||
		(single_flag != NULL && !add_text(flags, single_flag,
			strlen(single_flag))) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("shorts"), &shorts) ||
		!xrtValueObjectSetTake(spec, view("flags"), &flags)) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	if (valid) {
		parsed = result != NULL ? xrtValueObjectGet(result, view("flags")) : NULL;
		ok = parsed != NULL &&
			xrtValueGetBool(xrtValueObjectGet(parsed, view("verbose")), &verbose) &&
			verbose;
		parsed = result != NULL ? xrtValueObjectGet(result, view("options")) : NULL;
		ok = ok && parsed != NULL &&
			xrtValueGetString(xrtValueObjectGet(parsed, view("output")), &output) &&
			output.Size == 6u && memcmp(output.Data, "result", 6u) == 0 &&
			xrtGetError() == NULL;
	} else {
		ok = result == NULL && xrtGetError() != NULL;
	}
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(flags);
	xrtValueRelease(shorts);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_undeclared_short_flag(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* shorts = xrtValueObject();
	xvalue* result = NULL;
	bool ok = false;
	if (args == NULL || spec == NULL || shorts == NULL ||
		!xrtValueObjectSetNew(shorts, view("verbose"),
			xrtValueString(view("v"))) ||
		!xrtValueObjectSetTake(spec, view("shorts"), &shorts)) {
		goto cleanup;
	}
	result = xcli_parse(args, spec);
	ok = result == NULL && xrtGetError() != NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(shorts);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_undeclared_default(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* defaults = xrtValueObject();
	xvalue* result = NULL;
	bool ok = false;
	if (args == NULL || spec == NULL || options == NULL || defaults == NULL ||
		!xrtValueObjectSetNew(options, view("output"), xrtValueString(view("o"))) ||
		!xrtValueObjectSetNew(defaults, view("outpt"), xrtValueString(view("file"))) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("defaults"), &defaults)) goto cleanup;
	result = xcli_parse(args, spec);
	ok = result == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(defaults);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_declared_default(void)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* defaults = xrtValueObject();
	xvalue* result = NULL;
	xvalue* parsed;
	xstrview value;
	bool ok = false;
	if (args == NULL || spec == NULL || options == NULL || defaults == NULL ||
		!xrtValueObjectSetNew(options, view("output"), xrtValueString(view("o"))) ||
		!xrtValueObjectSetNew(defaults, view("output"), xrtValueString(view("file"))) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("defaults"), &defaults)) goto cleanup;
	result = xcli_parse(args, spec);
	parsed = result != NULL ? xrtValueObjectGet(result, view("options")) : NULL;
	parsed = parsed != NULL ? xrtValueObjectGet(parsed, view("output")) : NULL;
	ok = parsed != NULL && xrtValueGetString(parsed, &value) &&
		value.Size == 4u && memcmp(value.Data, "file", value.Size) == 0 &&
		xrtGetError() == NULL;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(defaults);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_invalid_default_value(xvalue* defaultValue)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* options = xrtValueObject();
	xvalue* defaults = xrtValueObject();
	xvalue* result = NULL;
	bool ok = false;
	if (args == NULL || spec == NULL || options == NULL || defaults == NULL ||
		defaultValue == NULL ||
		!xrtValueObjectSetNew(options, view("output"), xrtValueString(view("o"))) ||
		!xrtValueObjectSetTake(defaults, view("output"), &defaultValue) ||
		!xrtValueObjectSetTake(spec, view("options"), &options) ||
		!xrtValueObjectSetTake(spec, view("defaults"), &defaults)) goto cleanup;
	result = xcli_parse(args, spec);
	ok = result == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(defaultValue);
	xrtValueRelease(defaults);
	xrtValueRelease(options);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_duplicate_declared_name(const char* key)
{
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* names = xrtValueArray();
	xvalue* result = NULL;
	bool ok = false;
	if (args == NULL || spec == NULL || names == NULL ||
		!add_text(names, "input", 5u) || !add_text(names, "input", 5u) ||
		!xrtValueObjectSetTake(spec, view(key), &names)) goto cleanup;
	result = xcli_parse(args, spec);
	ok = result == NULL && xrtGetError() != NULL &&
		xrtErrorKind(xrtGetError()) == XERR_VALUE;
cleanup:
	xrtValueRelease(result);
	xrtValueRelease(names);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

static bool test_allocation_failures(int mode)
{
	static const char* first[] = {
		"hello", "hello", "--output", "-o", "--unknown"
	};
	xvalue* args = xrtValueArray();
	xvalue* spec = xrtValueObject();
	xvalue* positional = xrtValueArray();
	xvalue* options = xrtValueObject();
	unsigned failure;
	unsigned rejected = 0u;
	unsigned succeeded = 0u;
	bool ok = false;
	if (args == NULL || spec == NULL || positional == NULL ||
		options == NULL ||
		!add_text(args, first[mode], strlen(first[mode])) ||
		((mode == 2 || mode == 3) && !add_text(args, "hello", 5u)) ||
		(mode == 1 && !add_text(positional, "src", 3u)) ||
		((mode == 2 || mode == 3) &&
			!xrtValueObjectSetNew(options, view("output"),
				xrtValueString(view("o")))) ||
		!xrtValueObjectSetTake(spec, view("positional"), &positional) ||
		!xrtValueObjectSetTake(spec, view("options"), &options))
		goto cleanup;
	for (failure = 0u; failure < 64u && succeeded < 2u; ++failure) {
		xvalue* result;
		bool triggered;
		xerrkind kind;
		xmemdebugsnapshot before, after;
		xrtClearError();
		xrtMemDebugSnapshot(&before);
		if (!xrtMemDebugFailAfter(failure)) goto cleanup;
		result = xcli_parse(args, spec);
		triggered = xrtMemDebugFailTriggered();
		kind = xrtErrorKind(xrtGetError());
		xrtMemDebugFailClear();
		xrtClearError();
		if (triggered) {
			if (result != NULL || kind != XERR_MEMORY) {
				fprintf(stderr, "cli allocation point %u: result=%p kind=%d\n",
					failure, (void*)result, (int)kind);
				xrtValueRelease(result);
				goto cleanup;
			}
			++rejected;
		} else {
			bool accepted = true;
			xvalue* acceptedValue = result != NULL ?
				xrtValueObjectGet(result, view("ok")) : NULL;
			if (result == NULL || kind != XERR_NONE ||
				!xrtValueGetBool(acceptedValue, &accepted) ||
				accepted != (mode != 4)) {
				xrtValueRelease(result);
				goto cleanup;
			}
			++succeeded;
		}
		xrtValueRelease(result);
		xrtClearError();
		xrtMemDebugSnapshot(&after);
		if (mode == 4 && (before.LiveCount != after.LiveCount ||
			before.LiveBytes != after.LiveBytes)) {
			fprintf(stderr, "cli long error allocation point %u leaked\n", failure);
			goto cleanup;
		}
	}
	ok = rejected >= 2u && succeeded == 2u;
cleanup:
	xrtMemDebugFailClear();
	xrtClearError();
	xrtValueRelease(options);
	xrtValueRelease(positional);
	xrtValueRelease(spec);
	xrtValueRelease(args);
	return ok;
}

int main(void)
{
	if ( !test_long_inline_value() ) {
		fputs("cli inline value was truncated\n", stderr);
		return 1;
	}
	if ( !test_utf8_names(0) || !test_utf8_names(1) ||
		!test_utf8_names(2) ) {
		fputs("cli UTF-8 option or positional name was not preserved\n", stderr);
		return 1;
	}
	if (!test_invalid_unicode_name("a\xC2\xA0" "b", 4u, false) ||
		!test_invalid_unicode_name("a\xE3\x80\x80" "b", 5u, true) ||
		!test_invalid_unicode_name("a\xC2\x85" "b", 4u, true) ||
		!test_invalid_unicode_name("a\xE2\x80\xAF" "b", 5u, false) ||
		!test_invalid_unicode_name("a\xE2\x81\x9F" "b", 5u, true) ||
		!test_invalid_unicode_name("a\xE2\x80\x8B" "b", 5u, false) ||
		!test_invalid_unicode_name("a\xE2\x80\xAE" "b", 5u, true) ||
		!test_invalid_unicode_name("a\xE2\x81\xA6" "b", 5u, true) ||
		!test_invalid_unicode_name("a\xEF\xBB\xBF" "b", 5u, false) ||
		!test_invalid_unicode_name("a\xFF" "b", 3u, false)) {
		fputs("cli accepted invisible, control or invalid UTF-8 name\n", stderr);
		return 1;
	}
	if ( !test_embedded_nul() ) {
		fputs("cli argument containing NUL was accepted\n", stderr);
		return 1;
	}
	if ( !test_unknown_spec_field() ) {
		fputs("cli unknown spec field was silently ignored\n", stderr);
		return 1;
	}
	if ( !test_short_error_stops() ) {
		fputs("cli short option error did not stop parsing\n", stderr);
		return 1;
	}
	if ( !test_invalid_short_byte("-\xE4\xB8\xAD", 4u) ||
		!test_invalid_short_byte("-\t", 2u) ||
		!test_invalid_short_byte("-\x1B", 2u) ) {
		fputs("cli invalid short option produced an invalid error\n", stderr);
		return 1;
	}
	if ( !test_end_of_options() ) {
		fputs("cli end-of-options arguments were reinterpreted\n", stderr);
		return 1;
	}
	if ( !test_long_option_name() ) {
		fputs("cli long option name was ignored\n", stderr);
		return 1;
	}
	if ( !test_long_error(0) || !test_long_error(1) ||
		!test_long_error(2) ) {
		fputs("cli long option error was truncated\n", stderr);
		return 1;
	}
	if ( !test_flag_rejects_inline_value() ) {
		fputs("cli inline flag value was silently ignored\n", stderr);
		return 1;
	}
	if ( !test_invalid_spec_entry() ) {
		fputs("cli invalid spec entry was ignored\n", stderr);
		return 1;
	}
	if ( !test_late_invalid_argument() ) {
		fputs("cli invalid later argument was accepted\n", stderr);
		return 1;
	}
	if ( !test_short_alias("ov", "v", NULL, false) ||
		!test_short_alias("oo", NULL, NULL, false) ||
		!test_short_alias("\xC3\xA9", NULL, NULL, false) ||
		!test_short_alias("-", NULL, NULL, false) ||
		!test_short_alias("o", NULL, "o", false) ||
		!test_short_alias("o", NULL, "output", false) ||
		!test_undeclared_short_flag() ||
		!test_short_alias("o", "v", NULL, true) ) {
		fputs("cli short alias collision or invalid byte was accepted\n", stderr);
		return 1;
	}
	if ( !test_undeclared_default() ) {
		fputs("cli accepted a default for an undeclared option\n", stderr);
		return 1;
	}
	if ( !test_declared_default() ) {
		fputs("cli rejected a declared option default\n", stderr);
		return 1;
	}
	if ( !test_invalid_default_value(xrtValueInt(7)) ||
		!test_invalid_default_value(xrtValueString(
			(xstrview){ "file\0bad", 8u })) ) {
		fputs("cli accepted a non-text or NUL-truncated option default\n", stderr);
		return 1;
	}
	if ( !test_duplicate_declared_name("positional") ||
		!test_duplicate_declared_name("flags") ) {
		fputs("cli accepted duplicate declared names\n", stderr);
		return 1;
	}
	if ( !test_allocation_failures(0) ||
		!test_allocation_failures(1) ||
		!test_allocation_failures(2) ||
		!test_allocation_failures(3) ||
		!test_allocation_failures(4) ) {
		fputs("cli allocation failure was misclassified or not recovered\n", stderr);
		return 1;
	}
	puts("test_cli: PASS");
	return 0;
}
