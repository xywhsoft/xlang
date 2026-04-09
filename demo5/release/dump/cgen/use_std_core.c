/*
ModuleLoadOrder
- [0] std.core -> <native:std.core> native=1 binding=std.core
- [1] <root> -> D:\Git\x-lang\demo5\examples\native\use_std_core.xl native=0 binding=
ModuleExports
- module=std.core
  export 恒等 -> xl_mod_std_core__恒等
  export 版本 -> xl_mod_std_core__版本
- module=<root>
NativeRegistry
- key=std.core binding=std.core path=<native:std.core>
- key=std.time binding=std.time path=<native:std.time>
- key=std.task binding=std.task path=<native:std.task>
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

static int xl_mod_std_core__恒等(int x)
{
	return x;
}

int xl_mod_std_core__版本 = 5;
static int 入口()
{
	return xl_mod_std_core__恒等(xl_mod_std_core__版本);
}

int xl_generated_entry(void)
{
	return 入口();
}
