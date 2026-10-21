/*
 * json_patch_native.c - RFC 6901 JSON Pointer, RFC 6902 JSON Patch, and
 * RFC 7396 JSON Merge Patch over xvalue (xlang var) documents.
 *
 * Semantics:
 *   - apply returns a patched copy; the input document is never mutated.
 *   - Any failing operation aborts the whole patch (RFC 6902 atomicity).
 *   - Failures report an XRT error and return null; the wrapper raises.
 *   - set/remove/get/has operate on copies as well, keeping the API pure.
 *
 * "New"-suffixed container setters consume the passed reference; navigation
 * uses borrowed references returned by ArrayAt/ObjectAt.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_GRAPH
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

#define XJP_DOMAIN "xlang.extlib.json_patch"
#define XJP_MAX_TOKENS 32
#define XJP_MAX_TOKEN 256
#define XJP_MAX_POINTER_BYTES (XJP_MAX_TOKENS * (1u + 2u * (XJP_MAX_TOKEN - 1u)))
#define XJP_MAX_MERGE_DEPTH 64
#define XJP_KEY(text) ((xstrview){text, sizeof(text) - 1u})

typedef struct xjp_token {
    char data[XJP_MAX_TOKEN];
    size_t size;
} xjp_token;

static xstrview xjp_token_view(const xjp_token *token) {
    return (xstrview){token->data, token->size};
}

static void xjp_error(const char *message) {
    if (xrtGetError() == NULL)
        xrtSetErrorInfo(XERR_VALUE, XJP_DOMAIN, 0, message);
}

static int xjp_is_array(const xvalue *v) {
    return v != NULL && xrtValueType(v) == XVALUE_ARRAY;
}

static int xjp_is_object(const xvalue *v) {
    return v != NULL && xrtValueType(v) == XVALUE_OBJECT;
}

static xvalue *xjp_null_fail(const char *message) {
    xjp_error(message);
    return xrtValueNull();
}

/* 按字节长度解析和反转义；键段中的 U+0000 不是字符串结束符。 */
static int xjp_parse_pointer(xstrview pointer, xjp_token tokens[]) {
    int count = 0;
    size_t offset = 0;
    if (pointer.Size == 0u) return pointer.Data != NULL ? 0 : -1;
    if (pointer.Size > XJP_MAX_POINTER_BYTES) return -1;
    if (pointer.Data == NULL || pointer.Data[0] != '/' ||
        !xrtUtf8Valid(pointer, NULL)) return -1;
    offset = 1u;
    while (1) {
        xjp_token *token;
        if (count >= XJP_MAX_TOKENS) return -1;
        token = &tokens[count];
        token->size = 0u;
        while (offset < pointer.Size && pointer.Data[offset] != '/') {
            char ch = pointer.Data[offset++];
            if (ch == '~') {
                if (offset == pointer.Size) return -1;
                ch = pointer.Data[offset++];
                if (ch == '0') ch = '~';
                else if (ch == '1') ch = '/';
                else return -1;
            }
            if (token->size >= XJP_MAX_TOKEN - 1u) return -1;
            token->data[token->size++] = ch;
        }
        count++;
        if (offset == pointer.Size) break;
        offset++;
    }
    return count;
}

/* Borrowed child lookup in an object by key. */
static xvalue *xjp_object_get(xvalue *obj, xstrview key) {
    return xrtValueObjectGet(obj, key);
}

/* Parse an array index token; "-" is accepted only for append. */
static int xjp_array_index(xstrview token, int64 length, int allow_dash,
    int64 *out) {
    size_t i;
    int64 value = 0;
    if (allow_dash && token.Size == 1u && token.Data[0] == '-') {
        *out = length;
        return 1;
    }
    if (token.Size == 0u ||
        (token.Size > 1u && token.Data[0] == '0')) return 0;
    for (i = 0u; i < token.Size; ++i) {
        int digit;
        if (token.Data[i] < '0' || token.Data[i] > '9') return 0;
        digit = token.Data[i] - '0';
        if (digit > length || value > (length - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
    }
    *out = value;
    return 1;
}

/* Walk all but the last token; returns a borrowed container or NULL. */
static xvalue *xjp_resolve_parent(xvalue *root,
    xjp_token tokens[], int count) {
    xvalue *current = root;
    int i;
    for (i = 0; i < count - 1; ++i) {
        xstrview token = xjp_token_view(&tokens[i]);
        if (xjp_is_object(current)) {
            current = xjp_object_get(current, token);
        } else if (xjp_is_array(current)) {
            int64 index;
            if (!xjp_array_index(token, xrtValueCount(current), 0, &index) ||
                (size_t)index >= xrtValueCount(current)) {
                return NULL;
            }
            current = xrtValueArrayAt(current, index);
        } else {
            return NULL;
        }
        if (current == NULL) {
            return NULL;
        }
    }
    return current;
}

/* Insert or replace in an object container (consumes item). */
static int xjp_object_add(xvalue *obj, xstrview key, xvalue *item) {
    return xrtValueObjectSetNew(obj, key, item);
}

/* Insert into an array at index (consumes item). */
static int xjp_array_add(xvalue *arr, int64 index, xvalue *item) {
    return xrtValueArrayInsertNew(arr, (size_t)index, item);
}

static int xjp_remove_at(xvalue *parent, xstrview token) {
    if (xjp_is_object(parent)) {
        return xrtValueObjectRemove(parent, token);
    }
    if (xjp_is_array(parent)) {
        int64 index;
        if (!xjp_array_index(token, xrtValueCount(parent), 0, &index) ||
            (size_t)index >= xrtValueCount(parent)) {
            return 0;
        }
        return xrtValueArrayRemove(parent, (size_t)index, 1);
    }
    return 0;
}

/* Apply one operation to the (cloned) root. Returns 1 on success. */
static int xjp_apply_op(xvalue **rootRef, xvalue *op) {
    xvalue *root = *rootRef;
    xstrview opName;
    xstrview pathText;
    xjp_token tokens[XJP_MAX_TOKENS];
    int count;
    xvalue *parent;
    xstrview last;

    if (!xjp_is_object(op)) {
        xjp_error("patch entries must be objects");
        return 0;
    }
    {
        xvalue *opField = xjp_object_get(op, XJP_KEY("op"));
        xvalue *pathField = xjp_object_get(op, XJP_KEY("path"));
        if (opField == NULL || pathField == NULL ||
            !xrtValueGetString(opField, &opName) ||
            !xrtValueGetString(pathField, &pathText)) {
            xjp_error("each patch entry needs string 'op' and 'path'");
            return 0;
        }
    }
    count = xjp_parse_pointer(pathText, tokens);
    if (count < 0) {
        xjp_error("invalid JSON Pointer in 'path'");
        return 0;
    }

    if (opName.Size == 3 && memcmp(opName.Data, "add", 3) == 0) {
        xvalue *value = xjp_object_get(op, XJP_KEY("value"));
        xvalue *item;
        if (value == NULL) {
            xjp_error("add requires 'value'");
            return 0;
        }
        if (count == 0) {
            item = xrtValueDeepClone(value);
            if (item == NULL) return 0;
            xrtValueRelease(root);
            *rootRef = item;
            return 1;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL) {
            xjp_error("add target parent does not exist");
            return 0;
        }
        last = xjp_token_view(&tokens[count - 1]);
        item = xrtValueDeepClone(value);
        if (item == NULL) {
            return 0;
        }
        if (xjp_is_object(parent)) {
            if (!xjp_object_add(parent, last, item)) {
                return 0;
            }
            return 1;
        }
        if (xjp_is_array(parent)) {
            int64 index;
            if (!xjp_array_index(last, xrtValueCount(parent), 1, &index)) {
                xrtValueRelease(item);
                xjp_error("add on array needs a numeric index or '-'");
                return 0;
            }
            if (!xjp_array_add(parent, index, item)) {
                return 0;
            }
            return 1;
        }
        xrtValueRelease(item);
        xjp_error("add target parent is not a container");
        return 0;
    }

    if (opName.Size == 6 && memcmp(opName.Data, "remove", 6) == 0) {
        if (count == 0) {
            xvalue *empty = xrtValueNull();
            if (empty == NULL) return 0;
            xrtValueRelease(root);
            *rootRef = empty;
            return 1;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL ||
            !xjp_remove_at(parent, xjp_token_view(&tokens[count - 1]))) {
            xjp_error("remove target does not exist");
            return 0;
        }
        return 1;
    }

    if (opName.Size == 7 && memcmp(opName.Data, "replace", 7) == 0) {
        xvalue *value = xjp_object_get(op, XJP_KEY("value"));
        if (value == NULL) {
            xjp_error("replace requires 'value'");
            return 0;
        }
        if (count == 0) {
            xvalue *replacement = xrtValueDeepClone(value);
            if (replacement == NULL) return 0;
            xrtValueRelease(root);
            *rootRef = replacement;
            return 1;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL) {
            xjp_error("replace target parent does not exist");
            return 0;
        }
        last = xjp_token_view(&tokens[count - 1]);
        if (xjp_is_object(parent)) {
            if (xjp_object_get(parent, last) == NULL) {
                xjp_error("replace target does not exist");
                return 0;
            }
            {
                xvalue *item = xrtValueDeepClone(value);
                if (item == NULL ||
                    !xjp_object_add(parent, last, item)) {
                    return 0;
                }
            }
            return 1;
        }
        if (xjp_is_array(parent)) {
            int64 index;
            if (!xjp_array_index(last, xrtValueCount(parent), 0, &index) ||
                (size_t)index >= xrtValueCount(parent)) {
                xjp_error("replace target does not exist");
                return 0;
            }
            {
                xvalue *item = xrtValueDeepClone(value);
                if (item == NULL ||
                    !xrtValueArraySetNew(parent, index, item)) {
                    return 0;
                }
            }
            return 1;
        }
        xjp_error("replace target parent is not a container");
        return 0;
    }

    if ((opName.Size == 4 && memcmp(opName.Data, "move", 4) == 0) ||
        (opName.Size == 4 && memcmp(opName.Data, "copy", 4) == 0)) {
        int isMove = opName.Data[0] == 'm';
        xvalue *fromField = xjp_object_get(op, XJP_KEY("from"));
        xstrview fromText;
        xjp_token fromTokens[XJP_MAX_TOKENS];
        int fromCount;
        xvalue *fromParent;
        xvalue *fromValue;
        xvalue *item;
        if (fromField == NULL || !xrtValueGetString(fromField, &fromText)) {
            xjp_error("move/copy requires string 'from'");
            return 0;
        }
        fromCount = xjp_parse_pointer(fromText, fromTokens);
        if (fromCount < 0) {
            xjp_error("invalid 'from' pointer");
            return 0;
        }
        if (isMove && fromCount < count) {
            int i;
            for (i = 0; i < fromCount; ++i)
                if (fromTokens[i].size != tokens[i].size ||
                    memcmp(fromTokens[i].data, tokens[i].data,
                        tokens[i].size) != 0) break;
            if (i == fromCount) {
                xjp_error("move source cannot contain its target");
                return 0;
            }
        }
        if (fromCount == 0) {
            fromParent = NULL;
            fromValue = root;
        } else {
            fromParent = xjp_resolve_parent(root, fromTokens, fromCount);
            if (fromParent == NULL) {
                xjp_error("move/copy source parent does not exist");
                return 0;
            }
            last = xjp_token_view(&fromTokens[fromCount - 1]);
            if (xjp_is_object(fromParent)) {
                fromValue = xjp_object_get(fromParent, last);
            } else if (xjp_is_array(fromParent)) {
                int64 index;
                if (!xjp_array_index(last, xrtValueCount(fromParent), 0,
                        &index) || (size_t)index >= xrtValueCount(fromParent)) {
                    xjp_error("move/copy source does not exist");
                    return 0;
                }
                fromValue = xrtValueArrayAt(fromParent, index);
            } else {
                xjp_error("move/copy source parent is not a container");
                return 0;
            }
        }
        if (fromValue == NULL) {
            xjp_error("move/copy source does not exist");
            return 0;
        }
        if (isMove && fromCount == count) {
            int i;
            for (i = 0; i < count; ++i)
                if (fromTokens[i].size != tokens[i].size ||
                    memcmp(fromTokens[i].data, tokens[i].data,
                        tokens[i].size) != 0) break;
            if (i == count) return 1;
        }
        item = xrtValueDeepClone(fromValue);
        if (item == NULL) {
            return 0;
        }
        if (isMove && fromParent != NULL && !xjp_remove_at(fromParent, last)) {
            xrtValueRelease(item);
            xjp_error("move failed to remove source");
            return 0;
        }
        if (count == 0) {
            xrtValueRelease(root);
            *rootRef = item;
            return 1;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL) {
            xrtValueRelease(item);
            xjp_error("move/copy target parent does not exist");
            return 0;
        }
        last = xjp_token_view(&tokens[count - 1]);
        if (xjp_is_object(parent)) {
            if (!xjp_object_add(parent, last, item)) {
                return 0;
            }
            return 1;
        }
        if (xjp_is_array(parent)) {
            int64 index;
            if (!xjp_array_index(last, xrtValueCount(parent), 1, &index)) {
                xrtValueRelease(item);
                xjp_error("move/copy on array needs a numeric index or '-'");
                return 0;
            }
            if (!xjp_array_add(parent, index, item)) {
                return 0;
            }
            return 1;
        }
        xrtValueRelease(item);
        xjp_error("move/copy target parent is not a container");
        return 0;
    }

    if (opName.Size == 4 && memcmp(opName.Data, "test", 4) == 0) {
        xvalue *expected = xjp_object_get(op, XJP_KEY("value"));
        xvalue *actual;
        if (expected == NULL) {
            xjp_error("test requires 'value'");
            return 0;
        }
        if (count == 0) {
            actual = root;
        } else {
            parent = xjp_resolve_parent(root, tokens, count);
            if (parent == NULL) {
                xjp_error("test target parent does not exist");
                return 0;
            }
            last = xjp_token_view(&tokens[count - 1]);
            if (xjp_is_object(parent)) {
                actual = xjp_object_get(parent, last);
            } else if (xjp_is_array(parent)) {
                int64 index;
                if (!xjp_array_index(last, xrtValueCount(parent), 0,
                        &index) || (size_t)index >= xrtValueCount(parent)) {
                    xjp_error("test target does not exist");
                    return 0;
                }
                actual = xrtValueArrayAt(parent, index);
            } else {
                xjp_error("test target parent is not a container");
                return 0;
            }
        }
        if (actual == NULL || !xrtValueEqual(actual, expected)) {
            xjp_error("test comparison failed");
            return 0;
        }
        return 1;
    }

    xjp_error("unknown patch operation");
    return 0;
}

/* 只克隆一次目标；递归修改这份副本，失败时由外层整体释放。 */
static int xjp_merge_into(xvalue *result, xvalue *patch, size_t depth) {
    int64 i, n;
    if (depth >= XJP_MAX_MERGE_DEPTH) {
        xjp_error("merge patch exceeds object nesting limit");
        return 0;
    }
    n = xrtValueCount(patch);
    for (i = 0; i < n; ++i) {
        xstrview key;
        xvalue *item = xrtValueObjectAt(patch, i, &key);
        if (item == NULL) continue;
        if (xrtValueType(item) == XVALUE_NULL) {
            xrtValueObjectRemove(result, key);
        } else if (xjp_is_object(item)) {
            xvalue *child = xjp_object_get(result, key);
            if (!xjp_is_object(child)) {
                child = xrtValueObject();
                if (child == NULL || !xrtValueObjectSetNew(result, key, child))
                    return 0;
            }
            if (!xjp_merge_into(child, item, depth + 1u)) return 0;
        } else {
            xvalue *copy = xrtValueDeepClone(item);
            if (copy == NULL || !xrtValueObjectSetNew(result, key, copy))
                return 0;
        }
    }
    return 1;
}

/* RFC 7396 Merge Patch 返回独立拥有的结果。 */
static xvalue *xjp_merge_value(xvalue *target, xvalue *patch) {
    xvalue *result;
    if (!xjp_is_object(patch)) return xrtValueDeepClone(patch);
    result = xjp_is_object(target) ? xrtValueDeepClone(target) : xrtValueObject();
    if (result == NULL) return NULL;
    if (!xjp_merge_into(result, patch, 0u)) {
        xrtValueRelease(result);
        return NULL;
    }
    return result;
}

xvalue *xjsonpatch_apply(xvalue *doc, xvalue *patch) {
    xvalue *root;
    int64 i;
    int64 n;
    xrtClearError();
    if (doc == NULL || patch == NULL || !xjp_is_array(patch)) {
        return xjp_null_fail("apply requires a document and a patch array");
    }
    root = xrtValueDeepClone(doc);
    if (root == NULL) {
        return xjp_null_fail("apply failed to clone the document");
    }
    n = xrtValueCount(patch);
    for (i = 0; i < n; ++i) {
        xvalue *op = xrtValueArrayAt(patch, i);
        if (op == NULL || !xjp_apply_op(&root, op)) {
            xrtValueRelease(root);
            return xrtValueNull();
        }
    }
    return root;
}

xvalue *xjsonpatch_merge(xvalue *doc, xvalue *patch) {
    xvalue *result;
    xrtClearError();
    if (doc == NULL || patch == NULL) {
        return xjp_null_fail("merge requires a document and a patch object");
    }
    result = xjp_merge_value(doc, patch);
    if (result != NULL) return result;
    if (xrtGetError() == NULL) xjp_error("merge patch failed");
    return xrtValueNull();
}

static xvalue *xjp_locate(xvalue *root, xstrview pointer) {
    xjp_token tokens[XJP_MAX_TOKENS];
    int count = xjp_parse_pointer(pointer, tokens);
    xvalue *parent;
    xstrview last;
    if (count < 0) {
        xjp_error("invalid JSON Pointer");
        return NULL;
    }
    if (count == 0) {
        return root;
    }
    parent = xjp_resolve_parent(root, tokens, count);
    if (parent == NULL) {
        return NULL;
    }
    last = xjp_token_view(&tokens[count - 1]);
    if (xjp_is_object(parent)) {
        return xjp_object_get(parent, last);
    }
    if (xjp_is_array(parent)) {
        int64 index;
        if (xjp_array_index(last, xrtValueCount(parent), 0, &index) &&
            (size_t)index < xrtValueCount(parent)) {
            return xrtValueArrayAt(parent, index);
        }
    }
    return NULL;
}

xvalue *xjsonpatch_get_len(xvalue *doc, const char *pointer, size_t size) {
    xvalue *found;
    xrtClearError();
    if (doc == NULL) {
        return xjp_null_fail("get requires a document");
    }
    found = xjp_locate(doc, (xstrview){pointer, size});
    if (found == NULL) {
        return xjp_null_fail("get target does not exist");
    }
    return xrtValueDeepClone(found);
}

bool xjsonpatch_has_len(xvalue *doc, const char *pointer, size_t size) {
    xrtClearError();
    if (doc == NULL) {
        xjp_error("has requires a document");
        return 0;
    }
    return xjp_locate(doc, (xstrview){pointer, size}) != NULL;
}

xvalue *xjsonpatch_set_len(xvalue *doc, const char *pointer,
    size_t size, xvalue *value) {
    xvalue *root;
    xjp_token tokens[XJP_MAX_TOKENS];
    int count = xjp_parse_pointer((xstrview){pointer, size}, tokens);
    xvalue *parent;
    xstrview last;
    xvalue *item;
    xrtClearError();
    if (doc == NULL || value == NULL || count < 0) {
        return xjp_null_fail("set requires a document, a value, and a path");
    }
    if (count == 0) return xrtValueDeepClone(value);
    root = xrtValueDeepClone(doc);
    if (root == NULL) {
        return xjp_null_fail("set failed to clone the document");
    }
    parent = xjp_resolve_parent(root, tokens, count);
    if (parent == NULL) {
        xrtValueRelease(root);
        return xjp_null_fail("set target parent does not exist");
    }
    last = xjp_token_view(&tokens[count - 1]);
    item = xrtValueDeepClone(value);
    if (item == NULL) {
        xrtValueRelease(root);
        return xjp_null_fail("set failed to clone the value");
    }
    if (xjp_is_object(parent)) {
        if (!xjp_object_add(parent, last, item)) {
            xrtValueRelease(root);
            return xjp_null_fail("set failed on object");
        }
        return root;
    }
    if (xjp_is_array(parent)) {
        int64 index;
        if (!xjp_array_index(last, xrtValueCount(parent), 1, &index)) {
            xrtValueRelease(item);
            xrtValueRelease(root);
            return xjp_null_fail("set on array needs a numeric index or '-'");
        }
        if ((size_t)index == xrtValueCount(parent)) {
            if (!xrtValueArrayAppendNew(parent, item)) {
                xrtValueRelease(root);
                return xjp_null_fail("set failed to append");
            }
            return root;
        }
        if (!xrtValueArraySetNew(parent, index, item)) {
            xrtValueRelease(root);
            return xjp_null_fail("set failed on array");
        }
        return root;
    }
    xrtValueRelease(item);
    xrtValueRelease(root);
    return xjp_null_fail("set target parent is not a container");
}

xvalue *xjsonpatch_remove_len(xvalue *doc, const char *pointer,
    size_t size) {
    xvalue *root;
    xjp_token tokens[XJP_MAX_TOKENS];
    int count = xjp_parse_pointer((xstrview){pointer, size}, tokens);
    xvalue *parent;
    xrtClearError();
    if (doc == NULL || count < 0) {
        return xjp_null_fail("remove requires a document and a path");
    }
    if (count == 0) return xrtValueNull();
    root = xrtValueDeepClone(doc);
    if (root == NULL) {
        return xjp_null_fail("remove failed to clone the document");
    }
    parent = xjp_resolve_parent(root, tokens, count);
    if (parent == NULL ||
        !xjp_remove_at(parent, xjp_token_view(&tokens[count - 1]))) {
        xrtValueRelease(root);
        return xjp_null_fail("remove target does not exist");
    }
    return root;
}

/* C 字符串便利入口；需要访问含 NUL 的键时使用对应 _len 入口。 */
static size_t xjp_c_pointer_length(const char *pointer) {
    size_t size = 0u;
    if (pointer == NULL) return 0u;
    while (size <= XJP_MAX_POINTER_BYTES && pointer[size] != '\0') size++;
    return size;
}

xvalue *xjsonpatch_get(xvalue *doc, const char *pointer) {
    return xjsonpatch_get_len(doc, pointer, xjp_c_pointer_length(pointer));
}

bool xjsonpatch_has(xvalue *doc, const char *pointer) {
    return xjsonpatch_has_len(doc, pointer, xjp_c_pointer_length(pointer));
}

xvalue *xjsonpatch_set(xvalue *doc, const char *pointer, xvalue *value) {
    return xjsonpatch_set_len(doc, pointer,
        xjp_c_pointer_length(pointer), value);
}

xvalue *xjsonpatch_remove(xvalue *doc, const char *pointer) {
    return xjsonpatch_remove_len(doc, pointer,
        xjp_c_pointer_length(pointer));
}

static int xjp_pointer_value(xvalue *pointer, xstrview *out) {
    /* 参数错误必须属于本次调用，不能继承上一次的 XRT 错误。 */
    xrtClearError();
    if (pointer != NULL && xrtValueGetString(pointer, out)) return 1;
    xjp_error("pointer must be a JSON string");
    return 0;
}

xvalue *xjsonpatch_get_value(xvalue *doc, xvalue *pointer) {
    xstrview text;
    if (!xjp_pointer_value(pointer, &text)) return xrtValueNull();
    return xjsonpatch_get_len(doc, text.Data, text.Size);
}

bool xjsonpatch_has_value(xvalue *doc, xvalue *pointer) {
    xstrview text;
    return xjp_pointer_value(pointer, &text) &&
        xjsonpatch_has_len(doc, text.Data, text.Size);
}

xvalue *xjsonpatch_set_value(xvalue *doc, xvalue *pointer,
    xvalue *value) {
    xstrview text;
    if (!xjp_pointer_value(pointer, &text)) return xrtValueNull();
    return xjsonpatch_set_len(doc, text.Data, text.Size, value);
}

xvalue *xjsonpatch_remove_value(xvalue *doc, xvalue *pointer) {
    xstrview text;
    if (!xjp_pointer_value(pointer, &text)) return xrtValueNull();
    return xjsonpatch_remove_len(doc, text.Data, text.Size);
}

bool xjsonpatch_has_error(void) {
    return xrtGetError() != NULL;
}

static char *xjp_encode_pointer(xstrview key, size_t *out_size) {
    size_t i, needed = 2, pos = 0;
    char *encoded;
    if (key.Data == NULL) {
        xjp_error("pointer key must be valid UTF-8");
        return NULL;
    }
    /* 限制原始键段长度，与指针解析后的 255 字节边界一致。 */
    if (key.Size >= XJP_MAX_TOKEN) {
        xjp_error("pointer key exceeds the maximum segment length");
        return NULL;
    }
    if (!xrtUtf8Valid(key, NULL)) {
        xjp_error("pointer key must be valid UTF-8");
        return NULL;
    }
    for (i = 0; i < key.Size; ++i) {
        size_t step = key.Data[i] == '~' || key.Data[i] == '/' ? 2 : 1;
        needed += step;
    }
    encoded = xrtMalloc(needed);
    if (encoded == NULL) return NULL;
    encoded[pos++] = '/';
    for (i = 0; i < key.Size; ++i) {
        if (key.Data[i] == '~' || key.Data[i] == '/') {
            encoded[pos++] = '~';
            encoded[pos++] = key.Data[i] == '~' ? '0' : '1';
        } else {
            encoded[pos++] = key.Data[i];
        }
    }
    encoded[pos] = '\0';
    if (out_size != NULL) *out_size = pos;
    return encoded;
}

void *xjsonpatch_encode_pointer_len(const char *key, size_t size) {
    xrtClearError();
    if (key == NULL) {
        xjp_error("pointer key must be valid UTF-8");
        return NULL;
    }
    if (size >= XJP_MAX_TOKEN) {
        xjp_error("pointer key exceeds the maximum segment length");
        return NULL;
    }
    if (memchr(key, 0, size) != NULL) {
        xjp_error("C string result cannot contain NUL bytes");
        return NULL;
    }
    return xjp_encode_pointer((xstrview){key, size}, NULL);
}

xvalue *xjsonpatch_encode_pointer_value(xvalue *key) {
    xstrview text;
    size_t size;
    char *encoded;
    xvalue *result;
    xrtClearError();
    if (key == NULL || !xrtValueGetString(key, &text))
        return xjp_null_fail("pointer key must be a string");
    encoded = xjp_encode_pointer(text, &size);
    if (encoded == NULL) return xrtValueNull();
    result = xrtValueString((xstrview){encoded, size});
    xrtFree(encoded);
    return result != NULL ? result : xrtValueNull();
}
