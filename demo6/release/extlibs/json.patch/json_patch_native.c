/*
 * json_patch_native.c - RFC 6901 JSON Pointer, RFC 6902 JSON Patch, and
 * RFC 7386 JSON Merge Patch over xvalue (xlang var) documents.
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

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XJP_DOMAIN "xlang.extlib.json_patch"
#define XJP_MAX_TOKENS 32
#define XJP_MAX_TOKEN 256

static void xjp_error(const char *message) {
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

/* Parse and unescape a JSON Pointer into tokens. Returns token count or -1. */
static int xjp_parse_pointer(const char *pointer, char tokens[][XJP_MAX_TOKEN]) {
    int count = 0;
    const char *p = pointer;
    if (pointer == NULL) {
        return -1;
    }
    if (*p == '\0') {
        return 0;
    }
    if (*p != '/') {
        return -1;
    }
    p++;
    while (1) {
        const char *start = p;
        size_t pos = 0;
        while (*p != '\0' && *p != '/') {
            if (*p == '~') {
                if (p[1] == '0') {
                    if (pos < XJP_MAX_TOKEN - 1) tokens[count][pos++] = '~';
                    p += 2;
                } else if (p[1] == '1') {
                    if (pos < XJP_MAX_TOKEN - 1) tokens[count][pos++] = '/';
                    p += 2;
                } else {
                    return -1;
                }
            } else {
                if (pos < XJP_MAX_TOKEN - 1) tokens[count][pos++] = *p;
                p++;
            }
        }
        tokens[count][pos] = '\0';
        count++;
        if (*p == '\0') {
            break;
        }
        p++;
        if (count >= XJP_MAX_TOKENS) {
            return -1;
        }
    }
    return count;
}

/* Borrowed child lookup in an object by key. */
static xvalue *xjp_object_get(xvalue *obj, const char *key) {
    int64 i;
    int64 n = xrtValueCount(obj);
    size_t len = strlen(key);
    for (i = 0; i < n; ++i) {
        xstrview k;
        xvalue *v = xrtValueObjectAt(obj, i, &k);
        if (v != NULL && k.Size == len && memcmp(k.Data, key, len) == 0) {
            return v;
        }
    }
    return NULL;
}

/* Parse an array index token; "-" is accepted only for append. */
static int xjp_array_index(const char *token, int64 length, int allow_dash,
    int64 *out) {
    const char *p = token;
    int64 value = 0;
    if (allow_dash && strcmp(token, "-") == 0) {
        *out = length;
        return 1;
    }
    if (*p == '\0') {
        return 0;
    }
    for (; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10 + (*p - '0');
        if (value > length + 1) {
            return 0;
        }
    }
    *out = value;
    return 1;
}

/* Walk all but the last token; returns a borrowed container or NULL. */
static xvalue *xjp_resolve_parent(xvalue *root,
    char tokens[][XJP_MAX_TOKEN], int count) {
    xvalue *current = root;
    int i;
    for (i = 0; i < count - 1; ++i) {
        const char *token = tokens[i];
        if (xjp_is_object(current)) {
            current = xjp_object_get(current, token);
        } else if (xjp_is_array(current)) {
            int64 index;
            if (!xjp_array_index(token, xrtValueCount(current), 0, &index) ||
                index >= xrtValueCount(current)) {
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

static xstrview xjp_key_view(const char *key) {
    xstrview view;
    view.Data = key;
    view.Size = strlen(key);
    return view;
}

/* Insert or replace in an object container (consumes item). */
static int xjp_object_add(xvalue *obj, const char *key, xvalue *item) {
    return xrtValueObjectSetNew(obj, xjp_key_view(key), item);
}

/* Insert into an array at index (consumes item). */
static int xjp_array_add(xvalue *arr, int64 index, xvalue *item) {
    int64 n = xrtValueCount(arr);
    int64 i;
    if (index == n) {
        return xrtValueArrayAppendNew(arr, item);
    }
    if (!xrtValueArrayAppendNew(arr, xrtValueDeepClone(
            xrtValueArrayAt(arr, n - 1)))) {
        return 0;
    }
    for (i = n - 1; i > index; --i) {
        xvalue *prev = xrtValueArrayAt(arr, i - 1);
        if (prev == NULL ||
            !xrtValueArraySetNew(arr, i, xrtValueDeepClone(prev))) {
            return 0;
        }
    }
    return xrtValueArraySetNew(arr, index, item);
}

static int xjp_remove_at(xvalue *parent, const char *token) {
    if (xjp_is_object(parent)) {
        return xrtValueObjectRemove(parent, xjp_key_view(token));
    }
    if (xjp_is_array(parent)) {
        int64 index;
        if (!xjp_array_index(token, xrtValueCount(parent), 0, &index) ||
            index >= xrtValueCount(parent)) {
            return 0;
        }
        return xrtValueArrayRemove(parent, (size_t)index, 1);
    }
    return 0;
}

/* Apply one operation to the (cloned) root. Returns 1 on success. */
static int xjp_apply_op(xvalue *root, xvalue *op) {
    xstrview opName;
    xstrview pathText;
    char tokens[XJP_MAX_TOKENS][XJP_MAX_TOKEN];
    int count;
    xvalue *parent;
    const char *last;

    {
        xvalue *opField = xjp_object_get(op, "op");
        xvalue *pathField = xjp_object_get(op, "path");
        if (opField == NULL || pathField == NULL ||
            !xrtValueGetString(opField, &opName) ||
            !xrtValueGetString(pathField, &pathText)) {
            xjp_error("each patch entry needs string 'op' and 'path'");
            return 0;
        }
    }
    {
        char pathBuf[XJP_MAX_TOKEN * 4];
        if (pathText.Size >= sizeof(pathBuf)) {
            xjp_error("path is too long");
            return 0;
        }
        memcpy(pathBuf, pathText.Data, pathText.Size);
        pathBuf[pathText.Size] = '\0';
        count = xjp_parse_pointer(pathBuf, tokens);
        if (count < 0) {
            xjp_error("invalid JSON Pointer in 'path'");
            return 0;
        }
    }

    if (opName.Size == 3 && memcmp(opName.Data, "add", 3) == 0) {
        xvalue *value = xjp_object_get(op, "value");
        xvalue *item;
        if (value == NULL) {
            xjp_error("add requires 'value'");
            return 0;
        }
        if (count == 0) {
            /* add "" replaces the whole document - handled by caller. */
            xjp_error("add with empty path is not supported");
            return 0;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL) {
            xjp_error("add target parent does not exist");
            return 0;
        }
        last = tokens[count - 1];
        item = xrtValueDeepClone(value);
        if (item == NULL) {
            return 0;
        }
        if (xjp_is_object(parent)) {
            if (!xjp_object_add(parent, last, item)) {
                xrtValueRelease(item);
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
                xrtValueRelease(item);
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
            xjp_error("remove with empty path is not supported");
            return 0;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL || !xjp_remove_at(parent, tokens[count - 1])) {
            xjp_error("remove target does not exist");
            return 0;
        }
        return 1;
    }

    if (opName.Size == 7 && memcmp(opName.Data, "replace", 7) == 0) {
        xvalue *value = xjp_object_get(op, "value");
        if (value == NULL) {
            xjp_error("replace requires 'value'");
            return 0;
        }
        if (count == 0) {
            xjp_error("replace with empty path is not supported");
            return 0;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL) {
            xjp_error("replace target parent does not exist");
            return 0;
        }
        last = tokens[count - 1];
        if (xjp_is_object(parent)) {
            if (xjp_object_get(parent, last) == NULL) {
                xjp_error("replace target does not exist");
                return 0;
            }
            {
                xvalue *item = xrtValueDeepClone(value);
                if (item == NULL ||
                    !xjp_object_add(parent, last, item)) {
                    if (item != NULL) xrtValueRelease(item);
                    return 0;
                }
            }
            return 1;
        }
        if (xjp_is_array(parent)) {
            int64 index;
            if (!xjp_array_index(last, xrtValueCount(parent), 0, &index) ||
                index >= xrtValueCount(parent)) {
                xjp_error("replace target does not exist");
                return 0;
            }
            {
                xvalue *item = xrtValueDeepClone(value);
                if (item == NULL ||
                    !xrtValueArraySetNew(parent, index, item)) {
                    if (item != NULL) xrtValueRelease(item);
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
        xvalue *fromField = xjp_object_get(op, "from");
        xstrview fromText;
        char fromTokens[XJP_MAX_TOKENS][XJP_MAX_TOKEN];
        int fromCount;
        xvalue *fromParent;
        xvalue *fromValue;
        xvalue *item;
        if (fromField == NULL || !xrtValueGetString(fromField, &fromText)) {
            xjp_error("move/copy requires string 'from'");
            return 0;
        }
        {
            char fromBuf[XJP_MAX_TOKEN * 4];
            if (fromText.Size >= sizeof(fromBuf)) {
                xjp_error("from is too long");
                return 0;
            }
            memcpy(fromBuf, fromText.Data, fromText.Size);
            fromBuf[fromText.Size] = '\0';
            fromCount = xjp_parse_pointer(fromBuf, fromTokens);
            if (fromCount <= 0) {
                xjp_error("invalid 'from' pointer");
                return 0;
            }
        }
        fromParent = xjp_resolve_parent(root, fromTokens, fromCount);
        if (fromParent == NULL) {
            xjp_error("move/copy source parent does not exist");
            return 0;
        }
        last = fromTokens[fromCount - 1];
        if (xjp_is_object(fromParent)) {
            fromValue = xjp_object_get(fromParent, last);
        } else if (xjp_is_array(fromParent)) {
            int64 index;
            if (!xjp_array_index(last, xrtValueCount(fromParent), 0,
                    &index) || index >= xrtValueCount(fromParent)) {
                xjp_error("move/copy source does not exist");
                return 0;
            }
            fromValue = xrtValueArrayAt(fromParent, index);
        } else {
            xjp_error("move/copy source parent is not a container");
            return 0;
        }
        if (fromValue == NULL) {
            xjp_error("move/copy source does not exist");
            return 0;
        }
        item = xrtValueDeepClone(fromValue);
        if (item == NULL) {
            return 0;
        }
        if (isMove && !xjp_remove_at(fromParent, last)) {
            xrtValueRelease(item);
            xjp_error("move failed to remove source");
            return 0;
        }
        if (count == 0) {
            xjp_error("move/copy with empty path is not supported");
            xrtValueRelease(item);
            return 0;
        }
        parent = xjp_resolve_parent(root, tokens, count);
        if (parent == NULL) {
            xrtValueRelease(item);
            xjp_error("move/copy target parent does not exist");
            return 0;
        }
        last = tokens[count - 1];
        if (xjp_is_object(parent)) {
            if (!xjp_object_add(parent, last, item)) {
                xrtValueRelease(item);
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
                xrtValueRelease(item);
                return 0;
            }
            return 1;
        }
        xrtValueRelease(item);
        xjp_error("move/copy target parent is not a container");
        return 0;
    }

    if (opName.Size == 4 && memcmp(opName.Data, "test", 4) == 0) {
        xvalue *expected = xjp_object_get(op, "value");
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
            last = tokens[count - 1];
            if (xjp_is_object(parent)) {
                actual = xjp_object_get(parent, last);
            } else if (xjp_is_array(parent)) {
                int64 index;
                if (!xjp_array_index(last, xrtValueCount(parent), 0,
                        &index) || index >= xrtValueCount(parent)) {
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

/* RFC 7386 merge into a fresh value; returns an owned reference. */
static xvalue *xjp_merge_value(xvalue *target, xvalue *patch) {
    if (!xjp_is_object(patch)) {
        return xrtValueDeepClone(patch);
    }
    {
        xvalue *result;
        int64 i;
        int64 n;
        if (xjp_is_object(target)) {
            result = xrtValueDeepClone(target);
        } else {
            result = xrtValueObject();
        }
        if (result == NULL) {
            return xrtValueNull();
        }
        n = xrtValueCount(patch);
        for (i = 0; i < n; ++i) {
            xstrview key;
            xvalue *item = xrtValueObjectAt(patch, i, &key);
            if (item == NULL) {
                continue;
            }
            if (item != NULL && xrtValueType(item) == XVALUE_NULL) {
                char keyBuf[XJP_MAX_TOKEN];
                if (key.Size >= sizeof(keyBuf)) {
                    xrtValueRelease(result);
                    return xjp_null_fail("merge patch key is too long");
                }
                memcpy(keyBuf, key.Data, key.Size);
                keyBuf[key.Size] = '\0';
                xrtValueObjectRemove(result, xjp_key_view(keyBuf));
            } else {
                xvalue *child = NULL;
                xvalue *merged;
                char keyBuf[XJP_MAX_TOKEN];
                if (key.Size >= sizeof(keyBuf)) {
                    xrtValueRelease(result);
                    return xjp_null_fail("merge patch key is too long");
                }
                memcpy(keyBuf, key.Data, key.Size);
                keyBuf[key.Size] = '\0';
                if (xjp_is_object(item)) {
                    child = xjp_object_get(result, keyBuf);
                }
                merged = xjp_merge_value(child, item);
                if (merged == NULL ||
                    !xrtValueObjectSetNew(result, xjp_key_view(keyBuf), merged)) {
                    if (merged != NULL) xrtValueRelease(merged);
                    xrtValueRelease(result);
                    return xjp_null_fail("merge patch failed");
                }
            }
        }
        return result;
    }
}

xvalue *xjsonpatch_apply(xvalue *doc, xvalue *patch) {
    xvalue *root;
    int64 i;
    int64 n;
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
        if (op == NULL || !xjp_apply_op(root, op)) {
            xrtValueRelease(root);
            return xrtValueNull();
        }
    }
    return root;
}

xvalue *xjsonpatch_merge(xvalue *doc, xvalue *patch) {
    if (doc == NULL || patch == NULL) {
        return xjp_null_fail("merge requires a document and a patch object");
    }
    return xjp_merge_value(doc, patch);
}

static xvalue *xjp_locate(xvalue *root, const char *pointer) {
    char tokens[XJP_MAX_TOKENS][XJP_MAX_TOKEN];
    int count = xjp_parse_pointer(pointer, tokens);
    xvalue *parent;
    const char *last;
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
    last = tokens[count - 1];
    if (xjp_is_object(parent)) {
        return xjp_object_get(parent, last);
    }
    if (xjp_is_array(parent)) {
        int64 index;
        if (xjp_array_index(last, xrtValueCount(parent), 0, &index) &&
            index < xrtValueCount(parent)) {
            return xrtValueArrayAt(parent, index);
        }
    }
    return NULL;
}

xvalue *xjsonpatch_get(xvalue *doc, const char *pointer) {
    xvalue *found;
    if (doc == NULL) {
        return xjp_null_fail("get requires a document");
    }
    found = xjp_locate(doc, pointer);
    if (found == NULL) {
        return xjp_null_fail("get target does not exist");
    }
    return xrtValueDeepClone(found);
}

long long xjsonpatch_has(xvalue *doc, const char *pointer) {
    if (doc == NULL) {
        return 0;
    }
    return xjp_locate(doc, pointer) != NULL;
}

xvalue *xjsonpatch_set(xvalue *doc, const char *pointer, xvalue *value) {
    xvalue *root;
    char tokens[XJP_MAX_TOKENS][XJP_MAX_TOKEN];
    int count = xjp_parse_pointer(pointer, tokens);
    xvalue *parent;
    const char *last;
    xvalue *item;
    if (doc == NULL || value == NULL || count <= 0) {
        return xjp_null_fail("set requires a document, a value, and a path");
    }
    root = xrtValueDeepClone(doc);
    if (root == NULL) {
        return xjp_null_fail("set failed to clone the document");
    }
    parent = xjp_resolve_parent(root, tokens, count);
    if (parent == NULL) {
        xrtValueRelease(root);
        return xjp_null_fail("set target parent does not exist");
    }
    last = tokens[count - 1];
    item = xrtValueDeepClone(value);
    if (item == NULL) {
        xrtValueRelease(root);
        return xjp_null_fail("set failed to clone the value");
    }
    if (xjp_is_object(parent)) {
        if (!xjp_object_add(parent, last, item)) {
            xrtValueRelease(item);
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
        if (index == xrtValueCount(parent)) {
            if (!xrtValueArrayAppendNew(parent, item)) {
                xrtValueRelease(item);
                xrtValueRelease(root);
                return xjp_null_fail("set failed to append");
            }
            return root;
        }
        if (!xrtValueArraySetNew(parent, index, item)) {
            xrtValueRelease(item);
            xrtValueRelease(root);
            return xjp_null_fail("set failed on array");
        }
        return root;
    }
    xrtValueRelease(item);
    xrtValueRelease(root);
    return xjp_null_fail("set target parent is not a container");
}

xvalue *xjsonpatch_remove(xvalue *doc, const char *pointer) {
    xvalue *root;
    char tokens[XJP_MAX_TOKENS][XJP_MAX_TOKEN];
    int count = xjp_parse_pointer(pointer, tokens);
    xvalue *parent;
    if (doc == NULL || count <= 0) {
        return xjp_null_fail("remove requires a document and a path");
    }
    root = xrtValueDeepClone(doc);
    if (root == NULL) {
        return xjp_null_fail("remove failed to clone the document");
    }
    parent = xjp_resolve_parent(root, tokens, count);
    if (parent == NULL || !xjp_remove_at(parent, tokens[count - 1])) {
        xrtValueRelease(root);
        return xjp_null_fail("remove target does not exist");
    }
    return root;
}

const char *xjsonpatch_encode_pointer(const char *key) {
    static char encoded[XJP_MAX_TOKEN * 2 + 1];
    size_t pos = 0;
    const char *p = key;
    if (key == NULL) {
        encoded[0] = '\0';
        return encoded;
    }
    encoded[pos++] = '/';
    while (*p != '\0' && pos < sizeof(encoded) - 2) {
        if (*p == '~') {
            encoded[pos++] = '~';
            encoded[pos++] = '0';
        } else if (*p == '/') {
            encoded[pos++] = '~';
            encoded[pos++] = '1';
        } else {
            encoded[pos++] = *p;
        }
        p++;
    }
    encoded[pos] = '\0';
    return encoded;
}
