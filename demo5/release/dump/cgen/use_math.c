/*
ModuleLoadOrder
- [0] math -> D:\Git\x-lang\demo5\examples\module\math.xl
- [1] <root> -> D:\Git\x-lang\demo5\examples\module\use_math.xl
ModuleExports
- module=math
  export 加一 -> xl_mod_math__加一
  export 基数 -> xl_mod_math__基数
- module=<root>
*/
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

int xl_generated_had_exception = 0;
int xl_generated_exception_kind = 0;
const char* xl_generated_exception_text = NULL;
long long xl_generated_exception_int = 0;
double xl_generated_exception_float = 0.0;

static void xl_generated_raise_int(long long value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 1;
	xl_generated_exception_int = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_float(double value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 2;
	xl_generated_exception_float = value;
	xl_generated_exception_text = NULL;
}

static void xl_generated_raise_string(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_text = value;
}

static void xl_generated_raise_text(const char* value)
{
	xl_generated_had_exception = 1;
	xl_generated_exception_kind = 3;
	xl_generated_exception_text = value;
}

static int xl_mod_math__加一(int x)
{
	return (x + 1);
}

int xl_mod_math__基数 = 41;
static int 入口()
{
	return xl_mod_math__加一(xl_mod_math__基数);
}

int xl_generated_entry(void)
{
	return 入口();
}
