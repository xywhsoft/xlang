// xlang 生成的 C 代码
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lib/xrt/xrt.h"

// xlang 运行时
#define xvoIsTrue(x) (xvoGetBool(x) || xvoGetInt(x) || xvoGetFloat(x))

static xvalue xl_add(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )
		return xvoCreateInt(xvoGetInt(a) + xvoGetInt(b));
	return xvoCreateFloat(xvoGetFloat(a) + xvoGetFloat(b));
}

static xvalue xl_sub(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )
		return xvoCreateInt(xvoGetInt(a) - xvoGetInt(b));
	return xvoCreateFloat(xvoGetFloat(a) - xvoGetFloat(b));
}

static xvalue xl_mul(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )
		return xvoCreateInt(xvoGetInt(a) * xvoGetInt(b));
	return xvoCreateFloat(xvoGetFloat(a) * xvoGetFloat(b));
}

static xvalue xl_div(xvalue a, xvalue b) {
	return xvoCreateFloat(xvoGetFloat(a) / xvoGetFloat(b));
}

static xvalue xl_mod(xvalue a, xvalue b) {
	return xvoCreateInt(xvoGetInt(a) % xvoGetInt(b));
}

static xvalue xl_pow(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {
		i64 base = xvoGetInt(a), exp = xvoGetInt(b), result = 1;
		if ( exp >= 0 ) { while ( exp-- > 0 ) result *= base; return xvoCreateInt(result); }
	}
	return xvoCreateFloat(pow(xvoGetFloat(a), xvoGetFloat(b)));
}

static xvalue xl_neg(xvalue a) {
	if ( xvoType(a) == XVO_DT_INT ) return xvoCreateInt(-xvoGetInt(a));
	return xvoCreateFloat(-xvoGetFloat(a));
}

static xvalue xl_not(xvalue a) { return xvoCreateBool(!xvoIsTrue(a)); }
static xvalue xl_eq(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) == xvoGetInt(b)); }
static xvalue xl_ne(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) != xvoGetInt(b)); }
static xvalue xl_lt(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) < xvoGetInt(b)); }
static xvalue xl_le(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) <= xvoGetInt(b)); }
static xvalue xl_gt(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) > xvoGetInt(b)); }
static xvalue xl_ge(xvalue a, xvalue b) { return xvoCreateBool(xvoGetInt(a) >= xvoGetInt(b)); }
static xvalue xl_and(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) && xvoIsTrue(b)); }
static xvalue xl_or(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) || xvoIsTrue(b)); }
static xvalue xl_coalesce(xvalue a, xvalue b) { return xvoType(a) == XVO_DT_NULL ? b : a; }
static i64 xl__len_i64(xvalue obj) {
	if ( xvoType(obj) == XVO_DT_TEXT ) return xvoGetSize(obj);
	if ( xvoType(obj) == XVO_DT_ARRAY ) return xvoArrayItemCount(obj);
	if ( xvoType(obj) == XVO_DT_LIST ) return xvoListItemCount(obj);
	return 0;
}
static xvalue xl_index(xvalue obj, xvalue idx) {
	i64 i = xvoGetInt(idx);
	if ( i < 0 ) i = xl__len_i64(obj) + i;
	return xvoArrayGetValue(obj, (u32)i);
}
static xvalue xl_slice(xvalue obj, xvalue start, xvalue end) {
	i64 len = xl__len_i64(obj);
	i64 s = xvoGetInt(start);
	i64 e = xvoGetInt(end);
	if ( s < 0 ) s = len + s;
	if ( e < 0 ) e = len + e;
	if ( s < 0 ) s = 0;
	if ( e > len ) e = len;
	if ( xvoType(obj) == XVO_DT_TEXT ) {
		str p = xvoGetText(obj);
		return xvoCreateText(p + s, (u32)(e - s), FALSE);
	}
	xvalue arr = xvoCreateArray();
	for ( i64 i = s; i < e; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(obj, (u32)i), FALSE);
	return arr;
}
static xvalue xl_member(xvalue obj, const char* name) { return xvoTableGetValue(obj, (str)name, 0); }

static xvalue xl_array(int count, ...) {
	xvalue arr = xvoCreateArray();
	va_list args;
	va_start(args, count);
	for ( int i = 0; i < count; i++ ) {
		xvoArrayAppendValue(arr, va_arg(args, xvalue), FALSE);
	}
	va_end(args);
	return arr;
}

static xvalue xl_table(int count, ...) {
	xvalue tbl = xvoCreateTable();
	va_list args;
	va_start(args, count);
	for ( int i = 0; i < count; i++ ) {
		const char* key = va_arg(args, const char*);
		xvalue val = va_arg(args, xvalue);
		xvoTableSetValue(tbl, (str)key, 0, val, FALSE);
	}
	va_end(args);
	return tbl;
}

static xvalue xl_print(xvalue v) {
	switch ( xvoType(v) ) {
		case XVO_DT_NULL:  printf("null"); break;
		case XVO_DT_BOOL:  printf("%s", xvoGetBool(v) ? "true" : "false"); break;
		case XVO_DT_INT:   printf("%lld", xvoGetInt(v)); break;
		case XVO_DT_FLOAT: printf("%g", xvoGetFloat(v)); break;
		case XVO_DT_TEXT:  printf("%s", (char*)xvoGetText(v)); break;
		default: printf("[object]"); break;
	}
	return xvoCreateNull();
}

static xvalue xl_println(xvalue v) { xl_print(v); printf("\n"); return xvoCreateNull(); }

static xvalue xl_len(xvalue v) {
	switch ( xvoType(v) ) {
		case XVO_DT_TEXT:  return xvoCreateInt(strlen((char*)xvoGetText(v)));
		case XVO_DT_ARRAY: return xvoCreateInt(xvoArrayItemCount(v));
		case XVO_DT_LIST:  return xvoCreateInt(xvoListItemCount(v));
		case XVO_DT_TABLE: return xvoCreateInt(xvoTableItemCount(v));
		default: return xvoCreateInt(0);
	}
}

static xvalue xl_type(xvalue v) {
	switch ( xvoType(v) ) {
		case XVO_DT_NULL:  return xvoCreateText((str)"null", 0, FALSE);
		case XVO_DT_BOOL:  return xvoCreateText((str)"bool", 0, FALSE);
		case XVO_DT_INT:   return xvoCreateText((str)"int", 0, FALSE);
		case XVO_DT_FLOAT: return xvoCreateText((str)"float", 0, FALSE);
		case XVO_DT_TEXT:  return xvoCreateText((str)"string", 0, FALSE);
		case XVO_DT_ARRAY: return xvoCreateText((str)"array", 0, FALSE);
		case XVO_DT_LIST:  return xvoCreateText((str)"list", 0, FALSE);
		case XVO_DT_TABLE: return xvoCreateText((str)"table", 0, FALSE);
		default: return xvoCreateText((str)"unknown", 0, FALSE);
	}
}

// 闭包运行时
typedef struct XL_ClosureEnv {
	int iRefCount;
	int iVarCount;
	xvalue *pVars;
} XL_ClosureEnv;

typedef struct XL_Closure {
	xvalue (*pFunc)(XL_ClosureEnv*, xvalue);
	XL_ClosureEnv *pEnv;
} XL_Closure;

static XL_ClosureEnv* xl_env_create(int count) {
	XL_ClosureEnv *pEnv = (XL_ClosureEnv*)malloc(sizeof(XL_ClosureEnv));
	pEnv->iRefCount = 1;
	pEnv->iVarCount = count;
	pEnv->pVars = (xvalue*)malloc(sizeof(xvalue) * count);
	for ( int i = 0; i < count; i++ ) pEnv->pVars[i] = NULL;
	return pEnv;
}

static void xl_env_ref(XL_ClosureEnv *pEnv) {
	if ( pEnv ) pEnv->iRefCount++;
}

static void xl_env_unref(XL_ClosureEnv *pEnv) {
	if ( pEnv && (--pEnv->iRefCount == 0) ) {
		for ( int i = 0; i < pEnv->iVarCount; i++ ) {
			if ( pEnv->pVars[i] ) xvoUnref(pEnv->pVars[i]);
		}
		free(pEnv->pVars);
		free(pEnv);
	}
}

static xvalue xl_closure_create(xvalue (*pFunc)(XL_ClosureEnv*, xvalue), XL_ClosureEnv *pEnv) {
	XL_Closure *pClosure = (XL_Closure*)malloc(sizeof(XL_Closure));
	pClosure->pFunc = pFunc;
	pClosure->pEnv = pEnv;
	xl_env_ref(pEnv);
	return xvoCreatePoint(pClosure);
}

static xvalue xl_closure_call(xvalue closure, xvalue args) {
	XL_Closure *pClosure = (XL_Closure*)xvoGetPoint(closure);
	if ( !pClosure || !pClosure->pFunc ) return xvoCreateNull();
	return pClosure->pFunc(pClosure->pEnv, args);
}

static void xl_closure_free(xvalue closure) {
	XL_Closure *pClosure = (XL_Closure*)xvoGetPoint(closure);
	if ( pClosure ) {
		xl_env_unref(pClosure->pEnv);
		free(pClosure);
	}
}

static xvalue xl_call_var(xvalue func, xvalue args) {
	if ( xvoType(func) == XVO_DT_POINT ) {
		// 闭包调用
		return xl_closure_call(func, args);
	}
	return xvoCreateNull();
}

// 全局变量
static xvalue xlv_arr = NULL;
static xvalue xlv_slice1 = NULL;
static xvalue xlv_slice2 = NULL;
static xvalue xlv_slice3 = NULL;
static xvalue xlv_s = NULL;
static xvalue xlv_nums = NULL;
static xvalue xlv_one = NULL;
static xvalue xlv_all = NULL;
static xvalue xlv_filename = NULL;
static xvalue xlv_items = NULL;

// 函数声明

// 程序入口
int main(int argc, char* argv[])
{
	xrtInit();

	// 初始化函数变量

	xl_print(xvoCreateText((str)"=== 数组负索引 ===", 0, FALSE));
	xlv_arr = xl_array(5, xvoCreateInt(10), xvoCreateInt(20), xvoCreateInt(30), xvoCreateInt(40), xvoCreateInt(50));
	xl_print(xvoCreateText((str)"arr = [10, 20, 30, 40, 50]", 0, FALSE));
	xl_print(xvoCreateText((str)"arr[0] (第一个) =", 0, FALSE));
	xl_print(xl_index(xlv_arr, xvoCreateInt(0)));
	xl_print(xvoCreateText((str)"arr[-1] (最后一个) =", 0, FALSE));
	xl_print(xl_index(xlv_arr, xl_neg(xvoCreateInt(1))));
	xl_print(xvoCreateText((str)"arr[-2] (倒数第二个) =", 0, FALSE));
	xl_print(xl_index(xlv_arr, xl_neg(xvoCreateInt(2))));
	xl_print(xvoCreateText((str)"arr[-3] (倒数第三个) =", 0, FALSE));
	xl_print(xl_index(xlv_arr, xl_neg(xvoCreateInt(3))));
	xl_print(xvoCreateText((str)"=== 数组切片 ===", 0, FALSE));
	xl_print(xvoCreateText((str)"arr[0 to 3] (前三个) =", 0, FALSE));
	xlv_slice1 = xl_slice(xlv_arr, xvoCreateInt(0), xvoCreateInt(3));
	xl_print(xl_index(xlv_slice1, xvoCreateInt(0)));
	xl_print(xl_index(xlv_slice1, xvoCreateInt(1)));
	xl_print(xl_index(xlv_slice1, xvoCreateInt(2)));
	xl_print(xvoCreateText((str)"arr[1 to 4] (中间三个) =", 0, FALSE));
	xlv_slice2 = xl_slice(xlv_arr, xvoCreateInt(1), xvoCreateInt(4));
	xl_print(xl_index(xlv_slice2, xvoCreateInt(0)));
	xl_print(xl_index(xlv_slice2, xvoCreateInt(1)));
	xl_print(xl_index(xlv_slice2, xvoCreateInt(2)));
	xl_print(xvoCreateText((str)"arr[2 to 5] (后三个) =", 0, FALSE));
	xlv_slice3 = xl_slice(xlv_arr, xvoCreateInt(2), xvoCreateInt(5));
	xl_print(xl_index(xlv_slice3, xvoCreateInt(0)));
	xl_print(xl_index(xlv_slice3, xvoCreateInt(1)));
	xl_print(xl_index(xlv_slice3, xvoCreateInt(2)));
	xl_print(xvoCreateText((str)"=== 字符串切片 ===", 0, FALSE));
	xlv_s = xvoCreateText((str)"Hello World", 0, FALSE);
	xl_print(xvoCreateText((str)"s = 'Hello World'", 0, FALSE));
	xl_print(xvoCreateText((str)"s[0 to 5] =", 0, FALSE));
	xl_print(xl_slice(xlv_s, xvoCreateInt(0), xvoCreateInt(5)));
	xl_print(xvoCreateText((str)"s[6 to 11] =", 0, FALSE));
	xl_print(xl_slice(xlv_s, xvoCreateInt(6), xvoCreateInt(11)));
	xl_print(xvoCreateText((str)"=== 边界测试 ===", 0, FALSE));
	xlv_nums = xl_array(10, xvoCreateInt(1), xvoCreateInt(2), xvoCreateInt(3), xvoCreateInt(4), xvoCreateInt(5), xvoCreateInt(6), xvoCreateInt(7), xvoCreateInt(8), xvoCreateInt(9), xvoCreateInt(10));
	xl_print(xvoCreateText((str)"nums = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]", 0, FALSE));
	xl_print(xvoCreateText((str)"nums[0 to 1] (只取第一个) =", 0, FALSE));
	xlv_one = xl_slice(xlv_nums, xvoCreateInt(0), xvoCreateInt(1));
	xl_print(xl_index(xlv_one, xvoCreateInt(0)));
	xl_print(xvoCreateText((str)"nums[-1] (最后一个) =", 0, FALSE));
	xl_print(xl_index(xlv_nums, xl_neg(xvoCreateInt(1))));
	xl_print(xvoCreateText((str)"nums[0 to 10] (全部) 长度应为 10", 0, FALSE));
	xlv_all = xl_slice(xlv_nums, xvoCreateInt(0), xvoCreateInt(10));
	xl_print(xvoCreateText((str)"已获取全部元素", 0, FALSE));
	xl_print(xvoCreateText((str)"=== 实际应用场景 ===", 0, FALSE));
	xlv_filename = xvoCreateText((str)"document.txt", 0, FALSE);
	xl_print(xvoCreateText((str)"filename = 'document.txt'", 0, FALSE));
	xl_print(xvoCreateText((str)"后4个字符 (扩展名部分):", 0, FALSE));
	xl_print(xl_slice(xlv_filename, xvoCreateInt(8), xvoCreateInt(12)));
	xlv_items = xl_array(5, xvoCreateText((str)"first", 0, FALSE), xvoCreateText((str)"second", 0, FALSE), xvoCreateText((str)"third", 0, FALSE), xvoCreateText((str)"fourth", 0, FALSE), xvoCreateText((str)"last", 0, FALSE));
	xl_print(xvoCreateText((str)"items 数组:", 0, FALSE));
	xl_print(xvoCreateText((str)"第一个元素: items[0] =", 0, FALSE));
	xl_print(xl_index(xlv_items, xvoCreateInt(0)));
	xl_print(xvoCreateText((str)"最后一个元素: items[-1] =", 0, FALSE));
	xl_print(xl_index(xlv_items, xl_neg(xvoCreateInt(1))));
	xl_print(xvoCreateText((str)"=== 负索引和切片测试完成 ===", 0, FALSE));

	xrtUnit();
	return 0;
}
