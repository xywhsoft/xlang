// xlang 生成的 C 代码
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lib/xrt/xrt.h"

// xlang 运行时
#define xvoIsTrue(x) (xvoGetBool(x) || xvoGetInt(x) || xvoGetFloat(x))

static xvalue xl_add(xvalue a, xvalue b) {
	int ta = xvoType(a), tb = xvoType(b);
	// 字符串拼接
	if ( (ta == XVO_DT_TEXT) || (tb == XVO_DT_TEXT) ) {
		char sa[64] = {0}, sb[64] = {0};
		str pa = (ta == XVO_DT_TEXT) ? xvoGetText(a) : (str)sa;
		str pb = (tb == XVO_DT_TEXT) ? xvoGetText(b) : (str)sb;
		if ( ta == XVO_DT_INT ) snprintf(sa, 64, "%lld", (long long)xvoGetInt(a));
		else if ( ta == XVO_DT_FLOAT ) snprintf(sa, 64, "%g", xvoGetFloat(a));
		else if ( ta == XVO_DT_BOOL ) snprintf(sa, 64, "%s", xvoGetBool(a) ? "true" : "false");
		if ( tb == XVO_DT_INT ) snprintf(sb, 64, "%lld", (long long)xvoGetInt(b));
		else if ( tb == XVO_DT_FLOAT ) snprintf(sb, 64, "%g", xvoGetFloat(b));
		else if ( tb == XVO_DT_BOOL ) snprintf(sb, 64, "%s", xvoGetBool(b) ? "true" : "false");
		u32 la = (u32)strlen((char*)pa), lb = (u32)strlen((char*)pb);
		str result = (str)malloc(la + lb + 1);
		memcpy(result, pa, la);
		memcpy(result + la, pb, lb + 1);
		return xvoCreateText(result, la + lb, TRUE);
	}
	// 数组合并
	if ( (ta == XVO_DT_ARRAY) && (tb == XVO_DT_ARRAY) ) {
		xvalue arr = xvoCreateArray();
		u32 la = xvoArrayItemCount(a), lb = xvoArrayItemCount(b);
		for ( u32 i = 0; i < la; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(a, i), FALSE);
		for ( u32 i = 0; i < lb; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(b, i), FALSE);
		return arr;
	}
	// 数字加法
	if ( (ta == XVO_DT_INT) && (tb == XVO_DT_INT) )
		return xvoCreateInt(xvoGetInt(a) + xvoGetInt(b));
	return xvoCreateFloat(xvoGetFloat(a) + xvoGetFloat(b));
}

static xvalue xl_sub(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )
		return xvoCreateInt(xvoGetInt(a) - xvoGetInt(b));
	return xvoCreateFloat(xvoGetFloat(a) - xvoGetFloat(b));
}

static xvalue xl_mul(xvalue a, xvalue b) {
	int ta = xvoType(a), tb = xvoType(b);
	// 字符串重复: "ab" * 3 = "ababab"
	if ( (ta == XVO_DT_TEXT) && (tb == XVO_DT_INT) ) {
		str s = xvoGetText(a);
		i64 n = xvoGetInt(b);
		if ( n <= 0 ) return xvoCreateText((str)"", 0, FALSE);
		u32 slen = xvoGetSize(a);
		u32 newlen = slen * (u32)n;
		str result = (str)malloc(newlen + 1);
		for ( i64 i = 0; i < n; i++ ) memcpy(result + i * slen, s, slen);
		result[newlen] = 0;
		return xvoCreateText(result, newlen, TRUE);
	}
	// 数组重复: [1,2] * 3 = [1,2,1,2,1,2]
	if ( (ta == XVO_DT_ARRAY) && (tb == XVO_DT_INT) ) {
		i64 n = xvoGetInt(b);
		xvalue arr = xvoCreateArray();
		if ( n <= 0 ) return arr;
		u32 len = xvoArrayItemCount(a);
		for ( i64 j = 0; j < n; j++ ) {
			for ( u32 i = 0; i < len; i++ ) xvoArrayAppendValue(arr, xvoArrayGetValue(a, i), FALSE);
		}
		return arr;
	}
	// 数字乘法
	if ( (ta == XVO_DT_INT) && (tb == XVO_DT_INT) )
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

static xvalue xl_preinc(xvalue *p) {
	if ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) + 1);
	else *p = xvoCreateFloat(xvoGetFloat(*p) + 1.0);
	return *p;
}

static xvalue xl_predec(xvalue *p) {
	if ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) - 1);
	else *p = xvoCreateFloat(xvoGetFloat(*p) - 1.0);
	return *p;
}

static xvalue xl_postinc(xvalue *p) {
	xvalue old = *p;
	if ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) + 1);
	else *p = xvoCreateFloat(xvoGetFloat(*p) + 1.0);
	return old;
}

static xvalue xl_postdec(xvalue *p) {
	xvalue old = *p;
	if ( xvoType(*p) == XVO_DT_INT ) *p = xvoCreateInt(xvoGetInt(*p) - 1);
	else *p = xvoCreateFloat(xvoGetFloat(*p) - 1.0);
	return old;
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

static xvalue xl_in(xvalue val, xvalue container) {
	int dt = xvoType(container);
	if ( dt == XVO_DT_TEXT ) {
		str s = xvoGetText(container);
		str sub = xvoGetText(val);
		return xvoCreateBool(strstr((char*)s, (char*)sub) != NULL);
	}
	if ( dt == XVO_DT_ARRAY ) {
		u32 len = xvoArrayItemCount(container);
		for ( u32 i = 0; i < len; i++ ) {
			if ( xvoIsTrue(xl_eq(val, xvoArrayGetValue(container, i))) ) return xvoCreateBool(TRUE);
		}
		return xvoCreateBool(FALSE);
	}
	if ( dt == XVO_DT_TABLE ) {
		str key = xvoGetText(val);
		xvalue v = xvoTableGetValue(container, key, 0);
		return xvoCreateBool((v != NULL) && (xvoType(v) != XVO_DT_NULL));
	}
	return xvoCreateBool(FALSE);
}
static xvalue xl_is(xvalue val, xvalue typeVal) {
	return xvoCreateBool(xvoType(val) == xvoGetInt(typeVal));
}
static xvalue xl_bitand(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) & xvoGetInt(b)); }
static xvalue xl_bitor(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) | xvoGetInt(b)); }
static xvalue xl_bitxor(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) ^ xvoGetInt(b)); }
static xvalue xl_shl(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) << xvoGetInt(b)); }
static xvalue xl_shr(xvalue a, xvalue b) { return xvoCreateInt(xvoGetInt(a) >> xvoGetInt(b)); }
static xvalue xl_bitnot(xvalue a) { return xvoCreateInt(~xvoGetInt(a)); }

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

// 函数声明

// 程序入口
int main(int argc, char* argv[])
{
	xrtInit();

	// 初始化函数变量

	{
		xvalue xl__start = xvoCreateInt(1);
		xvalue xl__end = xvoCreateInt(100000);
		xvalue xl__step = xvoCreateInt(1);
		for ( xvalue xlv_i = xl__start; (xvoGetInt(xl__step) > 0) ? (xvoGetInt(xlv_i) <= xvoGetInt(xl__end)) : (xvoGetInt(xlv_i) >= xvoGetInt(xl__end)); ) {
			{
				xvalue xlv_a = xvoCreateInt(1);
				xvalue xlv_b = xvoCreateInt(2);
				xvalue xlv_c = xvoCreateInt(3);
				xvalue xlv_result = ({ xvalue xl__left = xl_add(xlv_a, xlv_b); xvalue xl__right = xlv_c; xvalue xl__result = xl_add(xl__left, xl__right); xvoUnref(xl__left); xl__result; });
				xvoUnref(xlv_a);
				xvoUnref(xlv_b);
				xvoUnref(xlv_c);
				xvoUnref(xlv_result);
			}
			{ xvalue xl__old = xlv_i; xlv_i = xl_add(xlv_i, xl__step); xvoUnref(xl__old); }
			xl__continue_0:;
		}
	}
	xl__break_0:;
	xl_println(xvoCreateText((str)"Small test done", 0, FALSE));
	{
		xvalue xl__start = xvoCreateInt(1);
		xvalue xl__end = xvoCreateInt(1000000);
		xvalue xl__step = xvoCreateInt(1);
		for ( xvalue xlv_i = xl__start; (xvoGetInt(xl__step) > 0) ? (xvoGetInt(xlv_i) <= xvoGetInt(xl__end)) : (xvoGetInt(xlv_i) >= xvoGetInt(xl__end)); ) {
			{
				xvalue xlv_temp = ({ xvalue xl__left = xl_add(xlv_i, xlv_i); xvalue xl__right = xlv_i; xvalue xl__result = xl_add(xl__left, xl__right); xvoUnref(xl__left); xl__result; });
				xvoUnref(xlv_temp);
			}
			{ xvalue xl__old = xlv_i; xlv_i = xl_add(xlv_i, xl__step); xvoUnref(xl__old); }
			xl__continue_1:;
		}
	}
	xl__break_1:;
	xl_println(xvoCreateText((str)"Medium test done", 0, FALSE));

	xrtUnit();
	return 0;
}
