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
static xvalue xl_index_set(xvalue obj, xvalue idx, xvalue val) {
	i64 i = xvoGetInt(idx);
	if ( i < 0 ) i = xl__len_i64(obj) + i;
	xvoArraySetValue(obj, (u32)i, val, FALSE);
	return val;
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
	if ( xvoType(typeVal) == XVO_DT_TEXT ) {
		str typeName = xvoGetText(typeVal);
		int dt = xvoType(val);
		if ( strcmp((char*)typeName, "int") == 0 ) return xvoCreateBool(dt == XVO_DT_INT);
		if ( strcmp((char*)typeName, "float") == 0 ) return xvoCreateBool(dt == XVO_DT_FLOAT);
		if ( strcmp((char*)typeName, "string") == 0 ) return xvoCreateBool(dt == XVO_DT_TEXT);
		if ( strcmp((char*)typeName, "bool") == 0 ) return xvoCreateBool(dt == XVO_DT_BOOL);
		if ( strcmp((char*)typeName, "null") == 0 ) return xvoCreateBool(dt == XVO_DT_NULL);
		if ( strcmp((char*)typeName, "array") == 0 ) return xvoCreateBool(dt == XVO_DT_ARRAY);
		if ( strcmp((char*)typeName, "list") == 0 ) return xvoCreateBool(dt == XVO_DT_LIST);
		if ( strcmp((char*)typeName, "table") == 0 ) return xvoCreateBool(dt == XVO_DT_TABLE);
	}
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
		case XVO_DT_TIME:  return xvoCreateText((str)"time", 0, FALSE);
		case XVO_DT_ARRAY: return xvoCreateText((str)"array", 0, FALSE);
		case XVO_DT_LIST:  return xvoCreateText((str)"list", 0, FALSE);
		case XVO_DT_COLL:  return xvoCreateText((str)"set", 0, FALSE);
		case XVO_DT_TABLE: {
			xvalue cls = xvoTableGetValue(v, (str)"__class__", 0);
			if ( cls && (xvoType(cls) == XVO_DT_TEXT) ) return cls;
			return xvoCreateText((str)"table", 0, FALSE);
		}
		case XVO_DT_FUNC:  return xvoCreateText((str)"function", 0, FALSE);
		default: return xvoCreateText((str)"unknown", 0, FALSE);
	}
}

static xvalue xl_typeof(xvalue v) {
	return xvoCreateInt(xvoType(v));
}

static xvalue xl_range(xvalue start, xvalue end, xvalue step) {
	xvalue arr = xvoCreateArray();
	i64 iStart = xvoGetInt(start);
	i64 iEnd = xvoGetInt(end);
	i64 iStep = (xvoType(step) == XVO_DT_NULL) ? 1 : xvoGetInt(step);
	if ( iStep == 0 ) return arr;
	if ( iStep > 0 ) {
		for ( i64 i = iStart; i < iEnd; i += iStep ) {
			xvoArrayAppendValue(arr, xvoCreateInt(i), FALSE);
		}
	} else {
		for ( i64 i = iStart; i > iEnd; i += iStep ) {
			xvoArrayAppendValue(arr, xvoCreateInt(i), FALSE);
		}
	}
	return arr;
}

static xvalue xl_input(xvalue prompt) {
	if ( xvoType(prompt) == XVO_DT_TEXT ) {
		printf("%s", (char*)xvoGetText(prompt));
		fflush(stdout);
	}
	char szBuf[4096] = {0};
	if ( fgets(szBuf, sizeof(szBuf), stdin) ) {
		size_t len = strlen(szBuf);
		if ( (len > 0) && (szBuf[len-1] == '\n') ) szBuf[--len] = 0;
		if ( (len > 0) && (szBuf[len-1] == '\r') ) szBuf[--len] = 0;
		return xvoCreateText((str)szBuf, (u32)len, FALSE);
	}
	return xvoCreateText((str)"", 0, FALSE);
}

static xvalue xl_abs(xvalue v) {
	if ( xvoType(v) == XVO_DT_INT ) {
		i64 n = xvoGetInt(v);
		return xvoCreateInt(n < 0 ? -n : n);
	}
	f64 f = xvoGetFloat(v);
	return xvoCreateFloat(f < 0 ? -f : f);
}

static xvalue xl_min(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {
		i64 ia = xvoGetInt(a), ib = xvoGetInt(b);
		return xvoCreateInt(ia < ib ? ia : ib);
	}
	f64 fa = xvoGetFloat(a), fb = xvoGetFloat(b);
	return xvoCreateFloat(fa < fb ? fa : fb);
}

static xvalue xl_max(xvalue a, xvalue b) {
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {
		i64 ia = xvoGetInt(a), ib = xvoGetInt(b);
		return xvoCreateInt(ia > ib ? ia : ib);
	}
	f64 fa = xvoGetFloat(a), fb = xvoGetFloat(b);
	return xvoCreateFloat(fa > fb ? fa : fb);
}

static xvalue xl_int(xvalue v) {
	switch ( xvoType(v) ) {
		case XVO_DT_INT:   return v;
		case XVO_DT_FLOAT: return xvoCreateInt((i64)xvoGetFloat(v));
		case XVO_DT_BOOL:  return xvoCreateInt(xvoGetBool(v) ? 1 : 0);
		case XVO_DT_TEXT:  return xvoCreateInt(atoll((char*)xvoGetText(v)));
		default: return xvoCreateInt(0);
	}
}

static xvalue xl_float(xvalue v) {
	switch ( xvoType(v) ) {
		case XVO_DT_FLOAT: return v;
		case XVO_DT_INT:   return xvoCreateFloat((f64)xvoGetInt(v));
		case XVO_DT_BOOL:  return xvoCreateFloat(xvoGetBool(v) ? 1.0 : 0.0);
		case XVO_DT_TEXT:  return xvoCreateFloat(atof((char*)xvoGetText(v)));
		default: return xvoCreateFloat(0.0);
	}
}

static xvalue xl_string(xvalue v) {
	char szBuf[128] = {0};
	switch ( xvoType(v) ) {
		case XVO_DT_TEXT:  return v;
		case XVO_DT_INT:   snprintf(szBuf, 128, "%lld", (long long)xvoGetInt(v)); break;
		case XVO_DT_FLOAT: snprintf(szBuf, 128, "%g", xvoGetFloat(v)); break;
		case XVO_DT_BOOL:  snprintf(szBuf, 128, "%s", xvoGetBool(v) ? "true" : "false"); break;
		case XVO_DT_NULL:  snprintf(szBuf, 128, "null"); break;
		default:           snprintf(szBuf, 128, "[object]"); break;
	}
	return xvoCreateText((str)szBuf, (u32)strlen(szBuf), FALSE);
}

static xvalue xl_tostring(xvalue v) { return xl_string(v); }

static xvalue xl_template_concat(xvalue first, ...) {
	va_list args;
	va_start(args, first);
	int count = va_arg(args, int);
	va_end(args);
	if ( count <= 0 ) return xvoCreateText((str)"", 0, FALSE);
	if ( count == 1 ) return first;
	// 计算总长度并拼接
	size_t iTotalLen = 0;
	xvalue parts[64] = {0};
	if ( count > 64 ) count = 64;
	va_start(args, first);
	parts[0] = first;
	for ( int i = 1; i < count; i++ ) parts[i] = va_arg(args, xvalue);
	va_arg(args, int); // skip count
	va_end(args);
	for ( int i = 0; i < count; i++ ) {
		if ( xvoType(parts[i]) == XVO_DT_TEXT ) iTotalLen += xvoGetSize(parts[i]);
		else { parts[i] = xl_string(parts[i]); iTotalLen += xvoGetSize(parts[i]); }
	}
	char *pszResult = (char*)malloc(iTotalLen + 1);
	pszResult[0] = 0;
	for ( int i = 0; i < count; i++ ) strcat(pszResult, (char*)xvoGetText(parts[i]));
	xvalue result = xvoCreateText((str)pszResult, (u32)iTotalLen, FALSE);
	free(pszResult);
	return result;
}

static xvalue xl_bool(xvalue v) {
	return xvoCreateBool(xvoIsTrue(v));
}

static xvalue xl_list(int count, ...) {
	xvalue lst = xvoCreateList();
	va_list args;
	va_start(args, count);
	for ( int i = 0; i < count; i++ ) {
		xvoListSetValue(lst, (i64)i, va_arg(args, xvalue), FALSE);
	}
	va_end(args);
	return lst;
}

static xvalue xl_list_method(xvalue lst, const char* method, xvalue args) {
	if ( strcmp(method, "add") == 0 ) {
		u32 len = xvoListItemCount(lst);
		xvalue val = xvoArrayGetValue(args, 0);
		xvoListSetValue(lst, (i64)len, val, FALSE);
		return lst;
	}
	if ( strcmp(method, "at") == 0 ) {
		i64 idx = xvoGetInt(xvoArrayGetValue(args, 0));
		return xvoListGetValue(lst, idx);
	}
	if ( strcmp(method, "put") == 0 ) {
		i64 idx = xvoGetInt(xvoArrayGetValue(args, 0));
		xvalue val = xvoArrayGetValue(args, 1);
		xvoListSetValue(lst, idx, val, FALSE);
		return val;
	}
	if ( strcmp(method, "remove") == 0 ) {
		i64 idx = xvoGetInt(xvoArrayGetValue(args, 0));
		xvoListRemove(lst, idx);
		return lst;
	}
	if ( strcmp(method, "has") == 0 ) {
		i64 idx = xvoGetInt(xvoArrayGetValue(args, 0));
		return xvoCreateBool(xvoListExists(lst, idx));
	}
	if ( strcmp(method, "clear") == 0 ) {
		xvoListClear(lst);
		return lst;
	}
	if ( strcmp(method, "len") == 0 ) {
		return xvoCreateInt(xvoListItemCount(lst));
	}
	return xvoCreateNull();
}

static xvalue xl_set(int count, ...) {
	xvalue s = xvoCreateTable();
	char szKey[64];
	va_list args;
	va_start(args, count);
	for ( int i = 0; i < count; i++ ) {
		xvalue v = va_arg(args, xvalue);
		switch ( xvoType(v) ) {
			case XVO_DT_INT: snprintf(szKey, 64, "i%lld", (long long)xvoGetInt(v)); break;
			case XVO_DT_TEXT: snprintf(szKey, 64, "s%s", (char*)xvoGetText(v)); break;
			default: snprintf(szKey, 64, "p%p", (void*)v); break;
		}
		xvoTableSetValue(s, (str)szKey, 0, v, FALSE);
	}
	va_end(args);
	return s;
}

static xvalue xl_set_method(xvalue s, const char* method, xvalue args) {
	char szKey[64];
	if ( strcmp(method, "add") == 0 ) {
		xvalue v = xvoArrayGetValue(args, 0);
		switch ( xvoType(v) ) {
			case XVO_DT_INT: snprintf(szKey, 64, "i%lld", (long long)xvoGetInt(v)); break;
			case XVO_DT_TEXT: snprintf(szKey, 64, "s%s", (char*)xvoGetText(v)); break;
			default: snprintf(szKey, 64, "p%p", (void*)v); break;
		}
		xvoTableSetValue(s, (str)szKey, 0, v, FALSE);
		return s;
	}
	if ( strcmp(method, "remove") == 0 ) {
		xvalue v = xvoArrayGetValue(args, 0);
		switch ( xvoType(v) ) {
			case XVO_DT_INT: snprintf(szKey, 64, "i%lld", (long long)xvoGetInt(v)); break;
			case XVO_DT_TEXT: snprintf(szKey, 64, "s%s", (char*)xvoGetText(v)); break;
			default: snprintf(szKey, 64, "p%p", (void*)v); break;
		}
		xvoTableRemove(s, (str)szKey, 0);
		return s;
	}
	if ( strcmp(method, "has") == 0 ) {
		xvalue v = xvoArrayGetValue(args, 0);
		switch ( xvoType(v) ) {
			case XVO_DT_INT: snprintf(szKey, 64, "i%lld", (long long)xvoGetInt(v)); break;
			case XVO_DT_TEXT: snprintf(szKey, 64, "s%s", (char*)xvoGetText(v)); break;
			default: snprintf(szKey, 64, "p%p", (void*)v); break;
		}
		xvalue found = xvoTableGetValue(s, (str)szKey, 0);
		return xvoCreateBool((found != NULL) && (xvoType(found) != XVO_DT_NULL));
	}
	if ( strcmp(method, "clear") == 0 ) {
		xvoTableClear(s);
		return s;
	}
	if ( strcmp(method, "len") == 0 ) {
		return xvoCreateInt(xvoTableItemCount(s));
	}
	return xvoCreateNull();
}

static xvalue xl_time_method(xvalue t, const char* method, xvalue args) {
	xtime tv = xvoGetTime(t);
	int64 year; int month, day, hour, minute, second, weekday, dayOfYear;
	xrtDecodeSerial(tv, &year, &month, &day, &hour, &minute, &second, &weekday, &dayOfYear);
	if ( strcmp(method, "year") == 0 ) return xvoCreateInt(year);
	if ( strcmp(method, "month") == 0 ) return xvoCreateInt(month);
	if ( strcmp(method, "day") == 0 ) return xvoCreateInt(day);
	if ( strcmp(method, "hour") == 0 ) return xvoCreateInt(hour);
	if ( strcmp(method, "minute") == 0 ) return xvoCreateInt(minute);
	if ( strcmp(method, "second") == 0 ) return xvoCreateInt(second);
	if ( strcmp(method, "weekday") == 0 ) return xvoCreateInt(weekday);
	if ( strcmp(method, "dayOfYear") == 0 ) return xvoCreateInt(dayOfYear);
	if ( strcmp(method, "format") == 0 ) {
		str fmt = xvoGetText(xvoArrayGetValue(args, 0));
		str s = xrtTimeFormat(tv, fmt);
		return xvoCreateText(s, (u32)strlen((char*)s), TRUE);
	}
	if ( strcmp(method, "timestamp") == 0 ) return xvoCreateInt(tv);
	return xvoCreateNull();
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

static xvalue xl_method_call(xvalue obj, const char* method, xvalue args);
static xvalue xl_super_call(xvalue obj, const char* method, xvalue args);

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
static xvalue xlv_dec = NULL;
static xvalue xlv_hex1 = NULL;
static xvalue xlv_hex2 = NULL;
static xvalue xlv_bin1 = NULL;
static xvalue xlv_bin2 = NULL;
static xvalue xlv_oct1 = NULL;
static xvalue xlv_oct2 = NULL;
static xvalue xlv_f1 = NULL;
static xvalue xlv_f2 = NULL;
static xvalue xlv_s1 = NULL;
static xvalue xlv_s2 = NULL;
static xvalue xlv_s3 = NULL;
static xvalue xlv_name = NULL;
static xvalue xlv_age = NULL;
static xvalue xlv_msg = NULL;
static xvalue xlv_greeting = NULL;
static xvalue xlv_plain = NULL;
static xvalue xlv_t1 = NULL;
static xvalue xlv_t2 = NULL;
static xvalue xlv_arr = NULL;
static xvalue xlv_tbl = NULL;
static xvalue xlv_lst = NULL;
static xvalue xlv_b1 = NULL;
static xvalue xlv_b2 = NULL;
static xvalue xlv_n = NULL;

// 函数声明

// 方法分发函数
static xvalue xl_method_call(xvalue obj, const char* method, xvalue args)
{
	int objType = xvoType(obj);
	// list 类型方法
	if ( objType == XVO_DT_LIST ) {
		return xl_list_method(obj, method, args);
	}
	// time 类型方法
	if ( objType == XVO_DT_TIME ) {
		return xl_time_method(obj, method, args);
	}
	// set 类型方法（table 实现）
	if ( objType == XVO_DT_TABLE ) {
		xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
		if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) {
			// 可能是 set（没有 __class__ 的 table）
			return xl_set_method(obj, method, args);
		}
		str className = xvoGetText(classNameVal);
		xvalue parentNameVal = xvoTableGetValue(obj, (str)"__parent__", 0);
		str parentName = parentNameVal ? xvoGetText(parentNameVal) : NULL;
		u32 argc = xvoArrayItemCount(args);

	}
	return xvoCreateNull();
}

// super 方法调用
static xvalue xl_super_call(xvalue obj, const char* method, xvalue args)
{
	xvalue parentNameVal = xvoTableGetValue(obj, (str)"__parent__", 0);
	if ( !parentNameVal ) return xvoCreateNull();
	str parentName = xvoGetText(parentNameVal);
	u32 argc = xvoArrayItemCount(args);

	return xvoCreateNull();
}

// 程序入口
int main(int argc, char* argv[])
{
	xrtInit();

	// 初始化类静态成员

	// 初始化函数变量

	xl_println(xvoCreateText((str)"=== 整数字面量 ===", 0, FALSE));
	xlv_dec = xvoCreateInt(255);
	xl_println(xl_add(xvoCreateText((str)"十进制 255 = ", 0, FALSE), xl_string(xlv_dec)));
	xlv_hex1 = xvoCreateInt(255);
	xl_println(xl_add(xvoCreateText((str)"十六进制 0xFF = ", 0, FALSE), xl_string(xlv_hex1)));
	xlv_hex2 = xvoCreateInt(255);
	xl_println(xl_add(xvoCreateText((str)"十六进制 &HFF = ", 0, FALSE), xl_string(xlv_hex2)));
	xlv_bin1 = xvoCreateInt(255);
	xl_println(xl_add(xvoCreateText((str)"二进制 0b11111111 = ", 0, FALSE), xl_string(xlv_bin1)));
	xlv_bin2 = xvoCreateInt(255);
	xl_println(xl_add(xvoCreateText((str)"二进制 &B11111111 = ", 0, FALSE), xl_string(xlv_bin2)));
	xlv_oct1 = xvoCreateInt(255);
	xl_println(xl_add(xvoCreateText((str)"八进制 &O377 = ", 0, FALSE), xl_string(xlv_oct1)));
	xlv_oct2 = xvoCreateInt(15);
	xl_println(xl_add(xvoCreateText((str)"八进制 &o17 = ", 0, FALSE), xl_string(xlv_oct2)));
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 浮点数字面量 ===", 0, FALSE));
	xlv_f1 = xvoCreateFloat(3.14159);
	xl_println(xl_add(xvoCreateText((str)"浮点数 3.14159 = ", 0, FALSE), xl_string(xlv_f1)));
	xlv_f2 = xvoCreateFloat(1.5e+10);
	xl_println(xl_add(xvoCreateText((str)"科学计数法 1.5e10 = ", 0, FALSE), xl_string(xlv_f2)));
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 字符串字面量 ===", 0, FALSE));
	xlv_s1 = xvoCreateText((str)"Hello\tWorld\n", 0, FALSE);
	xl_println(xl_add(xvoCreateText((str)"双引号字符串: ", 0, FALSE), xlv_s1));
	xlv_s2 = xvoCreateText((str)"Hello\\tWorld", 0, FALSE);
	xl_println(xl_add(xvoCreateText((str)"单引号字符串: ", 0, FALSE), xlv_s2));
	xlv_s3 = xvoCreateText((str)"这是\r\n多行\r\n字符串", 0, FALSE);
	xl_println(xl_add(xvoCreateText((str)"多行字符串: ", 0, FALSE), xlv_s3));
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 模板字符串 ===", 0, FALSE));
	xlv_name = xvoCreateText((str)"张三", 0, FALSE);
	xlv_age = xvoCreateInt(25);
	xlv_msg = xl_template_concat(xvoCreateText((str)"姓名: ", 0, FALSE), xl_tostring(xlv_name), xvoCreateText((str)", 年龄: ", 0, FALSE), xl_tostring(xlv_age), 4);
	xl_println(xlv_msg);
	xlv_greeting = xl_template_concat(xvoCreateText((str)"Hello ", 0, FALSE), xl_tostring(xlv_name), xvoCreateText((str)"!", 0, FALSE), 3);
	xl_println(xlv_greeting);
	xlv_plain = xl_template_concat(xvoCreateText((str)"这是纯文本模板", 0, FALSE), 1);
	xl_println(xlv_plain);
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 时间字面量 ===", 0, FALSE));
	xlv_t1 = xvoCreateNull();
	xl_println(xl_add(xvoCreateText((str)"日期: ", 0, FALSE), xl_string(xlv_t1)));
	xlv_t2 = xvoCreateNull();
	xl_println(xl_add(xvoCreateText((str)"日期时间: ", 0, FALSE), xl_string(xlv_t2)));
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 集合字面量 ===", 0, FALSE));
	xlv_arr = xl_array(5, xvoCreateInt(1), xvoCreateInt(2), xvoCreateInt(3), xvoCreateInt(4), xvoCreateInt(5));
	xl_println(xl_add(xvoCreateText((str)"数组长度: ", 0, FALSE), xl_string(xl_len(xlv_arr))));
	xlv_tbl = xl_table(2, "name", xvoCreateText((str)"Tom", 0, FALSE), "age", xvoCreateInt(30));
	xl_println(xl_add(xvoCreateText((str)"字典[name]: ", 0, FALSE), xl_index(xlv_tbl, xvoCreateText((str)"name", 0, FALSE))));
	xlv_lst = xl_table(3, "", xvoCreateText((str)"零", 0, FALSE), "", xvoCreateText((str)"一", 0, FALSE), "", xvoCreateText((str)"二", 0, FALSE));
	xl_println(xl_add(xvoCreateText((str)"列表[1]: ", 0, FALSE), xl_index(xlv_lst, xvoCreateInt(1))));
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 布尔和 null ===", 0, FALSE));
	xlv_b1 = xvoCreateBool(TRUE);
	xlv_b2 = xvoCreateBool(FALSE);
	xlv_n = xvoCreateNull();
	xl_println(xl_add(xvoCreateText((str)"true = ", 0, FALSE), xl_string(xlv_b1)));
	xl_println(xl_add(xvoCreateText((str)"false = ", 0, FALSE), xl_string(xlv_b2)));
	xl_println(xl_add(xvoCreateText((str)"null = ", 0, FALSE), xl_string(xlv_n)));
	xl_println(xvoCreateText((str)"", 0, FALSE));
	xl_println(xvoCreateText((str)"=== 测试完成 ===", 0, FALSE));

	xrtUnit();
	return 0;
}
