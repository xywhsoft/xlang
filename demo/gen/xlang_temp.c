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
static xvalue xl_index(xvalue obj, xvalue idx) { return xvoArrayGetValue(obj, xvoGetInt(idx)); }
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
static xvalue xlv_i = NULL;
static xvalue xlv_x = NULL;

// 函数声明

// 程序入口
int main(int argc, char* argv[])
{
	xrtInit();

	// 初始化函数变量

	xl_println(xvoCreateText((str)"Test break n / continue n", 0, FALSE));
	xl_println(xvoCreateText((str)"break 2 test:", 0, FALSE));
	xlv_i = xvoCreateInt(0);
	while ( xvoIsTrue(xl_lt(xlv_i, xvoCreateInt(3))) ) {
		(xlv_i = xl_add(xlv_i, xvoCreateInt(1)));
		xvalue xlv_j = xvoCreateInt(0);
		while ( xvoIsTrue(xl_lt(xlv_j, xvoCreateInt(3))) ) {
			(xlv_j = xl_add(xlv_j, xvoCreateInt(1)));
			xl_println(xlv_i);
			xl_println(xlv_j);
			if ( xvoIsTrue(xl_eq(xlv_j, xvoCreateInt(2))) ) {
				goto xl__break_0;
			} else {
			}
			xl__continue_1:;
		}
		xl__break_1:;
		xl__continue_0:;
	}
	xl__break_0:;
	xl_println(xvoCreateText((str)"after break 2", 0, FALSE));
	xl_println(xvoCreateText((str)"continue 2 test:", 0, FALSE));
	xlv_x = xvoCreateInt(0);
	while ( xvoIsTrue(xl_lt(xlv_x, xvoCreateInt(3))) ) {
		(xlv_x = xl_add(xlv_x, xvoCreateInt(1)));
		xvalue xlv_y = xvoCreateInt(0);
		while ( xvoIsTrue(xl_lt(xlv_y, xvoCreateInt(3))) ) {
			(xlv_y = xl_add(xlv_y, xvoCreateInt(1)));
			if ( xvoIsTrue(xl_eq(xlv_y, xvoCreateInt(2))) ) {
				goto xl__continue_2;
			} else {
			}
			xl_println(xlv_x);
			xl_println(xlv_y);
			xl__continue_3:;
		}
		xl__break_3:;
		xl_println(xvoCreateText((str)"inner loop done", 0, FALSE));
		xl__continue_2:;
	}
	xl__break_2:;
	xl_println(xvoCreateText((str)"done", 0, FALSE));

	xrtUnit();
	return 0;
}
