// xlang 生成的 C 代码
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lib/xrt/xrt.h"

// xlang 运行时
#define xvoIsTrue(x) (xvoGetBool(x) || xvoGetInt(x) || xvoGetFloat(x))

// 运算符重载分发函数（在 body 中实现）
static xvalue xl_op_add(xvalue a, xvalue b);
static xvalue xl_op_sub(xvalue a, xvalue b);
static xvalue xl_op_mul(xvalue a, xvalue b);
static xvalue xl_op_div(xvalue a, xvalue b);
static xvalue xl_op_mod(xvalue a, xvalue b);
static xvalue xl_op_pow(xvalue a, xvalue b);
static xvalue xl_op_neg(xvalue a);
static xvalue xl_op_eq(xvalue a, xvalue b);
static xvalue xl_op_lt(xvalue a, xvalue b);
static xvalue xl_op_le(xvalue a, xvalue b);
static xvalue xl_op_index(xvalue obj, xvalue key);
static xvalue xl_op_newindex(xvalue obj, xvalue key, xvalue val);
static xvalue xl_op_len(xvalue obj);
static xvalue xl_op_iter(xvalue obj);
static xvalue xl_op_next(xvalue obj);
static xvalue xl_op_to(xvalue obj, xvalue type);
static xvalue xl_op_enter(xvalue obj);
static xvalue xl_op_exit(xvalue obj);
static int xl_has_meta_op(xvalue obj, const char* op);

static int xl_is_custom_obj(xvalue obj) {
	if ( xvoType(obj) != XVO_DT_TABLE ) return 0;
	xvalue cn = xvoTableGetValue(obj, (str)"__class__", 0);
	return (cn && (xvoType(cn) == XVO_DT_TEXT));
}

static xvalue xl_add(xvalue a, xvalue b) {
	// 检查运算符重载
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "add") ) return xl_op_add(a, b);
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
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "sub") ) return xl_op_sub(a, b);
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) )
		return xvoCreateInt(xvoGetInt(a) - xvoGetInt(b));
	return xvoCreateFloat(xvoGetFloat(a) - xvoGetFloat(b));
}

static xvalue xl_mul(xvalue a, xvalue b) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "mul") ) return xl_op_mul(a, b);
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
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "div") ) return xl_op_div(a, b);
	return xvoCreateFloat(xvoGetFloat(a) / xvoGetFloat(b));
}

static xvalue xl_mod(xvalue a, xvalue b) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "mod") ) return xl_op_mod(a, b);
	return xvoCreateInt(xvoGetInt(a) % xvoGetInt(b));
}

static xvalue xl_pow(xvalue a, xvalue b) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "pow") ) return xl_op_pow(a, b);
	if ( (xvoType(a) == XVO_DT_INT) && (xvoType(b) == XVO_DT_INT) ) {
		i64 base = xvoGetInt(a), exp = xvoGetInt(b), result = 1;
		if ( exp >= 0 ) { while ( exp-- > 0 ) result *= base; return xvoCreateInt(result); }
	}
	return xvoCreateFloat(pow(xvoGetFloat(a), xvoGetFloat(b)));
}

static xvalue xl_neg(xvalue a) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "neg") ) return xl_op_neg(a);
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
static xvalue xl_eq(xvalue a, xvalue b) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "eq") ) return xl_op_eq(a, b);
	return xvoCreateBool(xvoGetInt(a) == xvoGetInt(b));
}
static xvalue xl_ne(xvalue a, xvalue b) { return xvoCreateBool(!xvoIsTrue(xl_eq(a, b))); }
static xvalue xl_approx(xvalue a, xvalue b) {
	double da = xvoGetFloat(a), db = xvoGetFloat(b);
	double diff = da - db;
	if ( diff < 0 ) diff = -diff;
	return xvoCreateBool(diff < 1e-9);
}
static xvalue xl_lt(xvalue a, xvalue b) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "lt") ) return xl_op_lt(a, b);
	return xvoCreateBool(xvoGetInt(a) < xvoGetInt(b));
}
static xvalue xl_le(xvalue a, xvalue b) {
	if ( xl_is_custom_obj(a) && xl_has_meta_op(a, "le") ) return xl_op_le(a, b);
	return xvoCreateBool(xvoGetInt(a) <= xvoGetInt(b));
}
static xvalue xl_gt(xvalue a, xvalue b) { return xl_lt(b, a); }
static xvalue xl_ge(xvalue a, xvalue b) { return xl_le(b, a); }
static xvalue xl_and(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) && xvoIsTrue(b)); }
static xvalue xl_or(xvalue a, xvalue b) { return xvoCreateBool(xvoIsTrue(a) || xvoIsTrue(b)); }
static xvalue xl_coalesce(xvalue a, xvalue b) { return xvoType(a) == XVO_DT_NULL ? b : a; }
static i64 xl__len_i64(xvalue obj) {
	if ( xl_is_custom_obj(obj) && xl_has_meta_op(obj, "len") ) return xvoGetInt(xl_op_len(obj));
	if ( xvoType(obj) == XVO_DT_TEXT ) return xvoGetSize(obj);
	if ( xvoType(obj) == XVO_DT_ARRAY ) return xvoArrayItemCount(obj);
	if ( xvoType(obj) == XVO_DT_LIST ) return xvoListItemCount(obj);
	return 0;
}
static xvalue xl_index(xvalue obj, xvalue idx) {
	if ( xl_is_custom_obj(obj) && xl_has_meta_op(obj, "index") ) return xl_op_index(obj, idx);
	i64 i = xvoGetInt(idx);
	if ( i < 0 ) i = xl__len_i64(obj) + i;
	return xvoArrayGetValue(obj, (u32)i);
}
static xvalue xl_index_set(xvalue obj, xvalue idx, xvalue val) {
	if ( xl_is_custom_obj(obj) && xl_has_meta_op(obj, "newindex") ) return xl_op_newindex(obj, idx, val);
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
static xvalue xl_meta_get(xvalue obj, const char* name);
static xvalue xl_meta_let(xvalue obj, const char* name, xvalue val);
static int xl_has_meta_get(xvalue obj);
static int xl_has_meta_let(xvalue obj);
static xvalue xl_member(xvalue obj, const char* name) {
	xvalue val = xvoTableGetValue(obj, (str)name, 0);
	if ( (val == NULL) || (xvoType(val) == XVO_DT_NULL) ) {
		if ( xl_has_meta_get(obj) ) return xl_meta_get(obj, name);
	}
	return val ? val : xvoCreateNull();
}
static xvalue xl_safe_member(xvalue obj, const char* name) {
	if ( !obj || (xvoType(obj) == XVO_DT_NULL) ) return xvoCreateNull();
	return xl_member(obj, name);
}
static xvalue xl_member_set(xvalue obj, const char* name, xvalue val) {
	if ( xl_has_meta_let(obj) ) return xl_meta_let(obj, name, val);
	xvoTableSetValue(obj, (str)name, 0, val, FALSE);
	return val;
}

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
	i64 iStep;
	if ( xvoType(step) == XVO_DT_NULL ) {
		iStep = (iStart <= iEnd) ? 1 : -1;
	} else {
		iStep = xvoGetInt(step);
	}
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

static xvalue xl_round(xvalue v) {
	if ( xvoType(v) == XVO_DT_INT ) return v;
	f64 f = xvoGetFloat(v);
	return xvoCreateInt((i64)(f >= 0 ? f + 0.5 : f - 0.5));
}

static xvalue xl_floor(xvalue v) {
	if ( xvoType(v) == XVO_DT_INT ) return v;
	f64 f = xvoGetFloat(v);
	i64 i = (i64)f;
	return xvoCreateInt((f < 0 && f != i) ? i - 1 : i);
}

static xvalue xl_ceil(xvalue v) {
	if ( xvoType(v) == XVO_DT_INT ) return v;
	f64 f = xvoGetFloat(v);
	i64 i = (i64)f;
	return xvoCreateInt((f > 0 && f != i) ? i + 1 : i);
}

static xvalue xl_chr(xvalue v) {
	char szBuf[8] = {0};
	i64 code = xvoGetInt(v);
	if ( code < 0x80 ) {
		szBuf[0] = (char)code;
	} else if ( code < 0x800 ) {
		szBuf[0] = (char)(0xC0 | (code >> 6));
		szBuf[1] = (char)(0x80 | (code & 0x3F));
	} else if ( code < 0x10000 ) {
		szBuf[0] = (char)(0xE0 | (code >> 12));
		szBuf[1] = (char)(0x80 | ((code >> 6) & 0x3F));
		szBuf[2] = (char)(0x80 | (code & 0x3F));
	} else {
		szBuf[0] = (char)(0xF0 | (code >> 18));
		szBuf[1] = (char)(0x80 | ((code >> 12) & 0x3F));
		szBuf[2] = (char)(0x80 | ((code >> 6) & 0x3F));
		szBuf[3] = (char)(0x80 | (code & 0x3F));
	}
	return xvoCreateText((str)szBuf, (u32)strlen(szBuf), FALSE);
}

static xvalue xl_ord(xvalue v) {
	if ( xvoType(v) != XVO_DT_TEXT ) return xvoCreateInt(0);
	const unsigned char* s = (const unsigned char*)xvoGetText(v);
	if ( !s || !*s ) return xvoCreateInt(0);
	i64 code = 0;
	if ( (s[0] & 0x80) == 0 ) {
		code = s[0];
	} else if ( (s[0] & 0xE0) == 0xC0 ) {
		code = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
	} else if ( (s[0] & 0xF0) == 0xE0 ) {
		code = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
	} else if ( (s[0] & 0xF8) == 0xF0 ) {
		code = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
	}
	return xvoCreateInt(code);
}

static xvalue xl_hash(xvalue v) {
	u64 h = 0;
	switch ( xvoType(v) ) {
		case XVO_DT_NULL:  h = 0; break;
		case XVO_DT_BOOL:  h = xvoGetBool(v) ? 1 : 0; break;
		case XVO_DT_INT:   h = (u64)xvoGetInt(v); break;
		case XVO_DT_FLOAT: { f64 f = xvoGetFloat(v); h = *(u64*)&f; } break;
		case XVO_DT_TEXT:  {
			const char* s = (const char*)xvoGetText(v);
			while ( *s ) { h = h * 31 + *s++; }
		} break;
		default: h = (u64)(size_t)v; break;
	}
	return xvoCreateInt((i64)h);
}

static xvalue xl_id(xvalue v) {
	return xvoCreateInt((i64)(size_t)v);
}

static xvalue xl_int(xvalue v) {
	// 检查自定义对象的 meta to 方法
	if ( xl_is_custom_obj(v) && xl_has_meta_op(v, "to") ) {
		return xl_op_to(v, xvoCreateText((str)"int", 3, FALSE));
	}
	switch ( xvoType(v) ) {
		case XVO_DT_INT:   return v;
		case XVO_DT_FLOAT: return xvoCreateInt((i64)xvoGetFloat(v));
		case XVO_DT_BOOL:  return xvoCreateInt(xvoGetBool(v) ? 1 : 0);
		case XVO_DT_TEXT:  return xvoCreateInt(atoll((char*)xvoGetText(v)));
		default: return xvoCreateInt(0);
	}
}

static xvalue xl_float(xvalue v) {
	// 检查自定义对象的 meta to 方法
	if ( xl_is_custom_obj(v) && xl_has_meta_op(v, "to") ) {
		return xl_op_to(v, xvoCreateText((str)"float", 5, FALSE));
	}
	switch ( xvoType(v) ) {
		case XVO_DT_FLOAT: return v;
		case XVO_DT_INT:   return xvoCreateFloat((f64)xvoGetInt(v));
		case XVO_DT_BOOL:  return xvoCreateFloat(xvoGetBool(v) ? 1.0 : 0.0);
		case XVO_DT_TEXT:  return xvoCreateFloat(atof((char*)xvoGetText(v)));
		default: return xvoCreateFloat(0.0);
	}
}

static xvalue xl_string(xvalue v) {
	// 检查自定义对象的 meta to 方法
	if ( xl_is_custom_obj(v) && xl_has_meta_op(v, "to") ) {
		return xl_op_to(v, xvoCreateText((str)"string", 6, FALSE));
	}
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

static xvalue xl_template_concat(int count, ...) {
	if ( count <= 0 ) return xvoCreateText((str)"", 0, FALSE);
	va_list args;
	va_start(args, count);
	xvalue parts[64] = {0};
	if ( count > 64 ) count = 64;
	for ( int i = 0; i < count; i++ ) parts[i] = va_arg(args, xvalue);
	va_end(args);
	if ( count == 1 ) return parts[0];
	// 计算总长度并拼接
	size_t iTotalLen = 0;
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

static xvalue xl_trim(xvalue v) {
	if ( xvoType(v) != XVO_DT_TEXT ) return v;
	str s = xvoGetText(v);
	size_t iSize = xvoGetSize(v);
	str result = xrtTrim(s, iSize, NULL, 0, FALSE, &iSize);
	return xvoCreateText(result, (u32)iSize, TRUE);
}

static xvalue xl_ltrim(xvalue v) {
	if ( xvoType(v) != XVO_DT_TEXT ) return v;
	str s = xvoGetText(v);
	size_t iSize = xvoGetSize(v);
	str result = xrtLTrim(s, iSize, NULL, 0, FALSE, &iSize);
	return xvoCreateText(result, (u32)iSize, TRUE);
}

static xvalue xl_rtrim(xvalue v) {
	if ( xvoType(v) != XVO_DT_TEXT ) return v;
	str s = xvoGetText(v);
	size_t iSize = xvoGetSize(v);
	str result = xrtRTrim(s, iSize, NULL, 0, FALSE, &iSize);
	return xvoCreateText(result, (u32)iSize, TRUE);
}

static xvalue xl_upper(xvalue v) {
	if ( xvoType(v) != XVO_DT_TEXT ) return v;
	str s = xvoGetText(v);
	u32 iSize = xvoGetSize(v);
	str result = xrtUCase(s, iSize, FALSE);
	return xvoCreateText(result, iSize, TRUE);
}

static xvalue xl_lower(xvalue v) {
	if ( xvoType(v) != XVO_DT_TEXT ) return v;
	str s = xvoGetText(v);
	u32 iSize = xvoGetSize(v);
	str result = xrtLCase(s, iSize, FALSE);
	return xvoCreateText(result, iSize, TRUE);
}

static xvalue xl_replace(xvalue src, xvalue old, xvalue rep) {
	if ( xvoType(src) != XVO_DT_TEXT ) return src;
	str s = xvoGetText(src);
	size_t iSize = xvoGetSize(src);
	str sOld = xvoGetText(old);
	size_t iOldSize = xvoGetSize(old);
	str sRep = xvoGetText(rep);
	size_t iRepSize = xvoGetSize(rep);
	size_t iRetSize = 0;
	str result = xrtReplace(s, iSize, sOld, iOldSize, sRep, iRepSize, &iRetSize);
	return xvoCreateText(result, (u32)iRetSize, TRUE);
}

static xvalue xl_split(xvalue src, xvalue sep) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateArray();
	str s = xvoGetText(src);
	size_t iSize = xvoGetSize(src);
	str sSep = xvoGetText(sep);
	size_t iSepSize = xvoGetSize(sep);
	size_t iRetSize = 0;
	str *arrParts = xrtSplit(s, iSize, sSep, iSepSize, FALSE, &iRetSize);
	xvalue arr = xvoCreateArray();
	for ( size_t i = 0; i < iRetSize; i++ ) {
		xvoArrayAppendText(arr, arrParts[i], 0, TRUE);
	}
	return arr;
}

static xvalue xl_join(xvalue arr, xvalue sep) {
	if ( xvoType(arr) != XVO_DT_ARRAY ) return xvoCreateText((str)"", 0, FALSE);
	str sSep = xvoType(sep) == XVO_DT_TEXT ? xvoGetText(sep) : (str)"";
	u32 iSepLen = xvoType(sep) == XVO_DT_TEXT ? xvoGetSize(sep) : 0;
	u32 iCount = xvoArrayItemCount(arr);
	if ( iCount == 0 ) return xvoCreateText((str)"", 0, FALSE);
	size_t iTotalLen = 0;
	for ( u32 i = 0; i < iCount; i++ ) {
		xvalue v = xvoArrayGetValue(arr, i);
		if ( xvoType(v) == XVO_DT_TEXT ) iTotalLen += xvoGetSize(v);
		if ( i > 0 ) iTotalLen += iSepLen;
	}
	char *pszResult = (char*)malloc(iTotalLen + 1);
	pszResult[0] = 0;
	for ( u32 i = 0; i < iCount; i++ ) {
		if ( i > 0 && iSepLen > 0 ) strcat(pszResult, (char*)sSep);
		xvalue v = xvoArrayGetValue(arr, i);
		if ( xvoType(v) == XVO_DT_TEXT ) strcat(pszResult, (char*)xvoGetText(v));
	}
	xvalue result = xvoCreateText((str)pszResult, (u32)strlen(pszResult), FALSE);
	free(pszResult);
	return result;
}

static xvalue xl_find(xvalue src, xvalue sub) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateInt(-1);
	str s = xvoGetText(src);
	str sSub = xvoGetText(sub);
	str found = strstr(s, sSub);
	return xvoCreateInt(found ? (i64)(found - s) : -1);
}

static xvalue xl_contains(xvalue src, xvalue sub) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	str s = xvoGetText(src);
	str sSub = xvoGetText(sub);
	return xvoCreateBool(strstr(s, sSub) != NULL);
}

static xvalue xl_substr(xvalue src, xvalue start, xvalue length) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	i64 iStart = xvoGetInt(start);
	i64 iLen = xvoType(length) == XVO_DT_NULL ? (i64)iSize - iStart : xvoGetInt(length);
	if ( iStart < 0 ) iStart = 0;
	if ( iStart >= iSize ) return xvoCreateText((str)"", 0, FALSE);
	if ( iLen <= 0 ) return xvoCreateText((str)"", 0, FALSE);
	if ( iStart + iLen > iSize ) iLen = iSize - iStart;
	return xvoCreateText(s + iStart, (u32)iLen, FALSE);
}

static xvalue xl_startswith(xvalue src, xvalue prefix) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	str sPrefix = xvoGetText(prefix);
	u32 iPrefixSize = xvoGetSize(prefix);
	if ( iPrefixSize > iSize ) return xvoCreateBool(FALSE);
	return xvoCreateBool(memcmp(s, sPrefix, iPrefixSize) == 0);
}

static xvalue xl_endswith(xvalue src, xvalue suffix) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	str sSuffix = xvoGetText(suffix);
	u32 iSuffixSize = xvoGetSize(suffix);
	if ( iSuffixSize > iSize ) return xvoCreateBool(FALSE);
	return xvoCreateBool(memcmp(s + iSize - iSuffixSize, sSuffix, iSuffixSize) == 0);
}

static xvalue xl_repeat(xvalue src, xvalue times) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	i64 iTimes = xvoGetInt(times);
	if ( iTimes <= 0 ) return xvoCreateText((str)"", 0, FALSE);
	size_t iLen = iSize * iTimes;
	char *pszResult = (char*)malloc(iLen + 1);
	pszResult[0] = 0;
	for ( i64 i = 0; i < iTimes; i++ ) strcat(pszResult, (char*)s);
	xvalue result = xvoCreateText((str)pszResult, (u32)iLen, FALSE);
	free(pszResult);
	return result;
}

static xvalue xl_reverse(xvalue src) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	char *pszResult = (char*)malloc(iSize + 1);
	for ( u32 i = 0; i < iSize; i++ ) pszResult[i] = s[iSize - 1 - i];
	pszResult[iSize] = 0;
	xvalue result = xvoCreateText((str)pszResult, iSize, FALSE);
	free(pszResult);
	return result;
}

static xvalue xl_rand() {
	return xvoCreateInt((i64)xrtRand64());
}

static xvalue xl_random() {
	return xvoCreateFloat((f64)xrtRand64() / (f64)0xFFFFFFFFFFFFFFFFULL);
}

static xvalue xl_randrange(xvalue a, xvalue b) {
	i64 iMin = xvoGetInt(a);
	i64 iMax = xvoGetInt(b);
	if ( iMin > iMax ) { i64 t = iMin; iMin = iMax; iMax = t; }
	return xvoCreateInt(iMin + (i64)(xrtRand64() % (u64)(iMax - iMin + 1)));
}

static xvalue xl_sqrt(xvalue v) {
	f64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);
	return xvoCreateFloat(sqrt(f));
}

static xvalue xl_log(xvalue v) {
	f64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);
	return xvoCreateFloat(log(f));
}

static xvalue xl_sin(xvalue v) {
	f64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);
	return xvoCreateFloat(sin(f));
}

static xvalue xl_cos(xvalue v) {
	f64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);
	return xvoCreateFloat(cos(f));
}

static xvalue xl_tan(xvalue v) {
	f64 f = (xvoType(v) == XVO_DT_INT) ? (f64)xvoGetInt(v) : xvoGetFloat(v);
	return xvoCreateFloat(tan(f));
}

static xvalue xl_year(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtYear(xvoGetTime(t)));
}

static xvalue xl_month(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtMonth(xvoGetTime(t)));
}

static xvalue xl_day(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtDay(xvoGetTime(t)));
}

static xvalue xl_hour(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtHour(xvoGetTime(t)));
}

static xvalue xl_minute(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtMinute(xvoGetTime(t)));
}

static xvalue xl_second(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtSecond(xvoGetTime(t)));
}

static xvalue xl_weekday(xvalue t) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtWeekday(xvoGetTime(t)));
}

static xvalue xl_strftime(xvalue t, xvalue fmt) {
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateText((str)"", 0, FALSE);
	str sFmt = xvoGetText(fmt);
	str s = xrtTimeFormat(xvoGetTime(t), sFmt);
	return xvoCreateText(s, (u32)strlen((char*)s), TRUE);
}

static xvalue xl_timestamp(xvalue t) {
	if ( xvoType(t) == XVO_DT_NULL ) return xvoCreateInt(xrtToUnixTime(xrtNow()));
	if ( xvoType(t) != XVO_DT_TIME ) return xvoCreateInt(0);
	return xvoCreateInt(xrtToUnixTime(xvoGetTime(t)));
}

static xvalue xl_fromtimestamp(xvalue ts) {
	i64 iTs = xvoGetInt(ts);
	return xvoCreateTime(xrtFromUnixTime(iTs));
}

static xvalue xl_sleep(xvalue ms) {
	i64 iMs = xvoGetInt(ms);
	xrtSleep((u32)iMs);
	return xvoCreateNull();
}

static xvalue xl_timer() {
	return xvoCreateFloat(xrtTimer());
}

static xvalue xl_readfile(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateNull();
	str sPath = xvoGetText(path);
	size_t iSize = 0;
	str sContent = xrtFileReadAll(sPath, XRT_CP_UTF8, &iSize);
	if ( !sContent ) return xvoCreateNull();
	return xvoCreateText(sContent, (u32)iSize, TRUE);
}

static xvalue xl_writefile(xvalue path, xvalue content) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	str sPath = xvoGetText(path);
	str sContent = xvoGetText(content);
	u32 iSize = xvoGetSize(content);
	int ret = xrtFileWriteAll(sPath, sContent, iSize, XRT_CP_UTF8);
	return xvoCreateBool(ret > 0);
}

static xvalue xl_appendfile(xvalue path, xvalue content) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	str sPath = xvoGetText(path);
	str sContent = xvoGetText(content);
	u32 iSize = xvoGetSize(content);
	int ret = xrtFileAppend(sPath, sContent, iSize, XRT_CP_UTF8);
	return xvoCreateBool(ret > 0);
}

static xvalue xl_exists(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	return xvoCreateBool(xrtPathExists(xvoGetText(path)));
}

static xvalue xl_isfile(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	return xvoCreateBool(xrtFileExists(xvoGetText(path)));
}

static xvalue xl_isdir(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	return xvoCreateBool(xrtDirExists(xvoGetText(path)));
}

static xvalue xl_filesize(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateInt(0);
	return xvoCreateInt(xrtFileGetSize(xvoGetText(path)));
}

static xvalue xl_mkdir(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	return xvoCreateBool(xrtDirCreateAll(xvoGetText(path)));
}

static xvalue xl_remove(xvalue path) {
	if ( xvoType(path) != XVO_DT_TEXT ) return xvoCreateBool(FALSE);
	return xvoCreateBool(xrtFileDelete(xvoGetText(path)));
}

static xvalue xl_json_parse(xvalue src) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateNull();
	str s = xvoGetText(src);
	return xrtParseJSON(s, 0);
}

static xvalue xl_json_stringify(xvalue v) {
	size_t iSize = 0;
	str s = xrtStringifyJSON(v, TRUE, &iSize);
	if ( !s ) return xvoCreateText((str)"", 0, FALSE);
	return xvoCreateText(s, (u32)iSize, TRUE);
}

static xvalue xl_base64_encode(xvalue src) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	str result = xrtBase64Encode((ptr)s, iSize, NULL);
	if ( !result ) return xvoCreateText((str)"", 0, FALSE);
	return xvoCreateText(result, (u32)strlen((char*)result), TRUE);
}

static xvalue xl_base64_decode(xvalue src) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	size_t iSize = xvoGetSize(src);
	ptr result = xrtBase64Decode(s, iSize, NULL);
	if ( !result ) return xvoCreateText((str)"", 0, FALSE);
	size_t iRetSize = iSize * 3 / 4;
	return xvoCreateText((str)result, (u32)iRetSize, TRUE);
}

static xvalue xl_hex_encode(xvalue src) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	u32 iSize = xvoGetSize(src);
	str result = xrtHexEncode((ptr)s, iSize);
	if ( !result ) return xvoCreateText((str)"", 0, FALSE);
	return xvoCreateText(result, iSize * 2, TRUE);
}

static xvalue xl_hex_decode(xvalue src) {
	if ( xvoType(src) != XVO_DT_TEXT ) return xvoCreateText((str)"", 0, FALSE);
	str s = xvoGetText(src);
	size_t iSize = xvoGetSize(src);
	ptr result = xrtHexDecode(s, iSize);
	if ( !result ) return xvoCreateText((str)"", 0, FALSE);
	return xvoCreateText((str)result, (u32)(iSize / 2), TRUE);
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
static xvalue xl_meta_get(xvalue obj, const char* name);
static xvalue xl_meta_let(xvalue obj, const char* name, xvalue val);
static xvalue xl_meta_call(xvalue obj, xvalue args);
static int xl_has_meta_get(xvalue obj);
static int xl_has_meta_let(xvalue obj);
static int xl_has_meta_call(xvalue obj);

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
static xvalue xlv_s = NULL;
static xvalue xlv_arr = NULL;
static xvalue xlv_t = NULL;
static xvalue xlv_obj = NULL;
static xvalue xlv_jsonStr = NULL;
static xvalue xlv_parsed = NULL;
static xvalue xlv_encoded = NULL;

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

// meta get 属性获取拦截
static xvalue xl_meta_get(xvalue obj, const char* name)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// meta let 属性设置拦截
static xvalue xl_meta_let(xvalue obj, const char* name, xvalue val)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// meta call 对象调用拦截
static xvalue xl_meta_call(xvalue obj, xvalue args)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 检查对象是否有 meta get
static int xl_has_meta_get(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return 0;
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;
	str className = xvoGetText(classNameVal);
	return 0;
}

// 检查对象是否有 meta let
static int xl_has_meta_let(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return 0;
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;
	str className = xvoGetText(classNameVal);
	return 0;
}

// 检查对象是否有 meta call
static int xl_has_meta_call(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return 0;
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;
	str className = xvoGetText(classNameVal);
	return 0;
}

// 检查对象是否有特定运算符重载
static int xl_has_meta_op(xvalue obj, const char* op)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return 0;
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return 0;
	str className = xvoGetText(classNameVal);
	return 0;
}

// 运算符重载: add
static xvalue xl_op_add(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: sub
static xvalue xl_op_sub(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: mul
static xvalue xl_op_mul(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: div
static xvalue xl_op_div(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: mod
static xvalue xl_op_mod(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: pow
static xvalue xl_op_pow(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: neg
static xvalue xl_op_neg(xvalue a)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: eq
static xvalue xl_op_eq(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: lt
static xvalue xl_op_lt(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: le
static xvalue xl_op_le(xvalue a, xvalue b)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: len
static xvalue xl_op_len(xvalue a)
{
	if ( xvoType(a) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(a, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: index
static xvalue xl_op_index(xvalue obj, xvalue key)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 运算符重载: newindex
static xvalue xl_op_newindex(xvalue obj, xvalue key, xvalue val)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 元方法: iter
static xvalue xl_op_iter(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 元方法: next
static xvalue xl_op_next(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 元方法: to (类型转换)
static xvalue xl_op_to(xvalue obj, xvalue type)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 元方法: enter (with 语句)
static xvalue xl_op_enter(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return obj;
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return obj;
	str className = xvoGetText(classNameVal);
	return obj;
}

// 元方法: exit (with 语句)
static xvalue xl_op_exit(xvalue obj)
{
	if ( xvoType(obj) != XVO_DT_TABLE ) return xvoCreateNull();
	xvalue classNameVal = xvoTableGetValue(obj, (str)"__class__", 0);
	if ( (!classNameVal) || (xvoType(classNameVal) == XVO_DT_NULL) ) return xvoCreateNull();
	str className = xvoGetText(classNameVal);
	return xvoCreateNull();
}

// 程序入口
int main(int argc, char* argv[])
{
	xrtInit();

	// 初始化类静态成员

	// 初始化函数变量

	xl_println(xvoCreateText((str)"========== 字符串函数测试 ==========", 0, FALSE));
	xlv_s = xvoCreateText((str)"  Hello World  ", 0, FALSE);
	xl_println(({ xvalue xl__left = xl_add(xvoCreateText((str)"trim: [", 0, FALSE), xl_trim(xlv_s)); xvalue xl__right = xvoCreateText((str)"]", 0, FALSE); xvalue xl__result = xl_add(xl__left, xl__right); xvoUnref(xl__left); xl__result; }));
	xl_println(({ xvalue xl__left = xl_add(xvoCreateText((str)"ltrim: [", 0, FALSE), xl_ltrim(xlv_s)); xvalue xl__right = xvoCreateText((str)"]", 0, FALSE); xvalue xl__result = xl_add(xl__left, xl__right); xvoUnref(xl__left); xl__result; }));
	xl_println(({ xvalue xl__left = xl_add(xvoCreateText((str)"rtrim: [", 0, FALSE), xl_rtrim(xlv_s)); xvalue xl__right = xvoCreateText((str)"]", 0, FALSE); xvalue xl__result = xl_add(xl__left, xl__right); xvoUnref(xl__left); xl__result; }));
	xl_println(xl_add(xvoCreateText((str)"upper: ", 0, FALSE), xl_upper(xvoCreateText((str)"hello", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"lower: ", 0, FALSE), xl_lower(xvoCreateText((str)"HELLO", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"replace: ", 0, FALSE), xl_replace(xvoCreateText((str)"hello world", 0, FALSE), xvoCreateText((str)"world", 0, FALSE), xvoCreateText((str)"xlang", 0, FALSE))));
	xlv_arr = xl_split(xvoCreateText((str)"a,b,c,d", 0, FALSE), xvoCreateText((str)",", 0, FALSE));
	xl_println(({ xvalue xl__left = xl_add(xvoCreateText((str)"split: ", 0, FALSE), xl_len(xlv_arr)); xvalue xl__right = xvoCreateText((str)" items", 0, FALSE); xvalue xl__result = xl_add(xl__left, xl__right); xvoUnref(xl__left); xl__result; }));
	xl_println(xl_add(xvoCreateText((str)"join: ", 0, FALSE), xl_join(xlv_arr, xvoCreateText((str)"-", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"find: ", 0, FALSE), xl_find(xvoCreateText((str)"hello world", 0, FALSE), xvoCreateText((str)"world", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"contains: ", 0, FALSE), xl_contains(xvoCreateText((str)"hello world", 0, FALSE), xvoCreateText((str)"world", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"substr: ", 0, FALSE), xl_substr(xvoCreateText((str)"hello world", 0, FALSE), xvoCreateInt(0), xvoCreateInt(5))));
	xl_println(xl_add(xvoCreateText((str)"startswith: ", 0, FALSE), xl_startswith(xvoCreateText((str)"hello", 0, FALSE), xvoCreateText((str)"hel", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"endswith: ", 0, FALSE), xl_endswith(xvoCreateText((str)"hello", 0, FALSE), xvoCreateText((str)"llo", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"repeat: ", 0, FALSE), xl_repeat(xvoCreateText((str)"ab", 0, FALSE), xvoCreateInt(3))));
	xl_println(xl_add(xvoCreateText((str)"reverse: ", 0, FALSE), xl_reverse(xvoCreateText((str)"hello", 0, FALSE))));
	xl_println(xvoCreateText((str)"\n========== 数学函数测试 ==========", 0, FALSE));
	xl_println(xl_add(xvoCreateText((str)"rand: ", 0, FALSE), xl_rand()));
	xl_println(xl_add(xvoCreateText((str)"random: ", 0, FALSE), xl_random()));
	xl_println(xl_add(xvoCreateText((str)"randrange(1,10): ", 0, FALSE), xl_randrange(xvoCreateInt(1), xvoCreateInt(10))));
	xl_println(xl_add(xvoCreateText((str)"pow(2,10): ", 0, FALSE), xl_pow(xvoCreateInt(2), xvoCreateInt(10))));
	xl_println(xl_add(xvoCreateText((str)"sqrt(16): ", 0, FALSE), xl_sqrt(xvoCreateInt(16))));
	xl_println(xl_add(xvoCreateText((str)"log(2.718): ", 0, FALSE), xl_log(xvoCreateFloat(2.718))));
	xl_println(xl_add(xvoCreateText((str)"sin(0): ", 0, FALSE), xl_sin(xvoCreateInt(0))));
	xl_println(xl_add(xvoCreateText((str)"cos(0): ", 0, FALSE), xl_cos(xvoCreateInt(0))));
	xl_println(xvoCreateText((str)"\n========== 时间函数测试 ==========", 0, FALSE));
	xlv_t = xvoCreateTime(xrtNow());
	xl_println(xl_add(xvoCreateText((str)"now: ", 0, FALSE), xlv_t));
	xl_println(xl_add(xvoCreateText((str)"year: ", 0, FALSE), xl_year(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"month: ", 0, FALSE), xl_month(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"day: ", 0, FALSE), xl_day(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"hour: ", 0, FALSE), xl_hour(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"minute: ", 0, FALSE), xl_minute(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"second: ", 0, FALSE), xl_second(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"weekday: ", 0, FALSE), xl_weekday(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"strftime: ", 0, FALSE), xl_strftime(xlv_t, xvoCreateText((str)"yyyy-mm-dd hh:nn:ss", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"timestamp: ", 0, FALSE), xl_timestamp(xlv_t)));
	xl_println(xl_add(xvoCreateText((str)"timer: ", 0, FALSE), xl_timer()));
	xl_println(xvoCreateText((str)"\n========== 文件函数测试 ==========", 0, FALSE));
	xl_println(xl_add(xvoCreateText((str)"exists(.): ", 0, FALSE), xl_exists(xvoCreateText((str)".", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"isdir(.): ", 0, FALSE), xl_isdir(xvoCreateText((str)".", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"isfile(test_stdlib.xl): ", 0, FALSE), xl_isfile(xvoCreateText((str)"test_stdlib.xl", 0, FALSE))));
	xl_println(xvoCreateText((str)"\n========== JSON函数测试 ==========", 0, FALSE));
	xlv_obj = xl_table(2, "name", xvoCreateText((str)"xlang", 0, FALSE), "version", xvoCreateFloat(1));
	xlv_jsonStr = xl_json_stringify(xlv_obj);
	xl_println(xl_add(xvoCreateText((str)"json_stringify: ", 0, FALSE), xlv_jsonStr));
	xlv_parsed = xl_json_parse(xvoCreateText((str)"{\"a\":1,\"b\":2}", 0, FALSE));
	xl_println(xl_add(xvoCreateText((str)"json_parse: ", 0, FALSE), xl_index(xlv_parsed, xvoCreateText((str)"a", 0, FALSE))));
	xl_println(xvoCreateText((str)"\n========== 编码函数测试 ==========", 0, FALSE));
	xlv_encoded = xl_base64_encode(xvoCreateText((str)"Hello World", 0, FALSE));
	xl_println(xl_add(xvoCreateText((str)"base64_encode: ", 0, FALSE), xlv_encoded));
	xl_println(xl_add(xvoCreateText((str)"base64_decode: ", 0, FALSE), xl_base64_decode(xlv_encoded)));
	xl_println(xl_add(xvoCreateText((str)"hex_encode: ", 0, FALSE), xl_hex_encode(xvoCreateText((str)"ABC", 0, FALSE))));
	xl_println(xl_add(xvoCreateText((str)"hex_decode: ", 0, FALSE), xl_hex_decode(xvoCreateText((str)"414243", 0, FALSE))));
	xl_println(xvoCreateText((str)"\n========== 所有测试完成 ==========", 0, FALSE));

	xrtUnit();
	return 0;
}
