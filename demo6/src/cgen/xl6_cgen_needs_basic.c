#include "src/cgen/xl6_cgen_needs_basic.h"

#include <string.h>

static int xl6CgenNeedsBasicTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static int xl6CgenNeedsBasicIsRangeCall(const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount < 2 ) {
        return 0;
    }
    return node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6CgenNeedsBasicTextEquals(node->children[0]->text, "range");
}

static int xl6CgenNeedsBasicIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "array") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "array<var>"));
}

static int xl6CgenNeedsBasicIsListXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "list") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "list<var>"));
}

static int xl6CgenNeedsBasicIsSetXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "set") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "set<var>"));
}

static int xl6CgenNeedsBasicIsDictXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "dict") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "dict<var>"));
}

static int xl6CgenNeedsBasicIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "var") ||
            xl6CgenNeedsBasicIsArrayXValueType(typeNode) ||
            xl6CgenNeedsBasicIsListXValueType(typeNode) ||
            xl6CgenNeedsBasicIsSetXValueType(typeNode) ||
            xl6CgenNeedsBasicIsDictXValueType(typeNode));
}

static int xl6CgenNeedsBasicIsBuiltinObjectClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CgenNeedsBasicTextEquals(node->text, "object");
}

static int xl6CgenNeedsBasicIsToStringMemberCallSyntax(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }

    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ||
         !xl6CgenNeedsBasicTextEquals(callee->text, "toString") ) {
        return 0;
    }

    args = node->childCount > 1 ? node->children[1] : NULL;
    return args == NULL || args->childCount == 0;
}

static int xl6CgenNeedsBasicIsValueDerive(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_DERIVE &&
           (xl6CgenNeedsBasicTextEquals(node->text, "value") ||
            xl6CgenNeedsBasicTextEquals(node->text, "json") ||
            xl6CgenNeedsBasicTextEquals(node->text, "xson"));
}

static int xl6CgenNeedsBasicIsPersistenceGenericCall(const XL6_AstNode* node)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    instant = node->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount == 0 ) {
        return 0;
    }
    nameNode = instant->children[0];
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           (xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_json_parse") ||
            xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_json_read") ||
            xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_xson_parse") ||
            xl6CgenNeedsBasicTextEquals(nameNode->text, "__xl_std_xson_read"));
}

static int xl6CgenNeedsBasicIsCryptoRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha1_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha256_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha384_hex") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_sha512_hex");
}

static int xl6CgenNeedsBasicIsUrlRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_part") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_flag") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_port") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_resolve") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_url_normalize_path");
}

static int xl6CgenNeedsBasicIsQueryRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_encode") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_decode") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_valid") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_pair") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_query_append");
}

static int xl6CgenNeedsBasicIsRegexRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_is_match") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_regex_find");
}

static int xl6CgenNeedsBasicIsProcessRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_exec") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_success") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_process_exit_code");
}

static int xl6CgenNeedsBasicIsDirRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_dir_list");
}

static int xl6CgenNeedsBasicIsStringEdgeRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_starts_with") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_ends_with");
}

static int xl6CgenNeedsBasicIsStdStringRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_split") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_chars") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_bytes") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_join") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_string_split_lines");
}

static int xl6CgenNeedsBasicIsPathRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_path_join");
}

static int xl6CgenNeedsBasicIsHttpServerRuntimeCall(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }
    return xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_create") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_start") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_port") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_stop") ||
           xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_http_server_destroy");
}

int xl6CgenNodeNeedsRangeContains(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "in") &&
         node->childCount > 1 && xl6CgenNeedsBasicIsRangeCall(node->children[1]) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRangeContains(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsIntPow(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "**") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && xl6CgenNeedsBasicTextEquals(node->text, "**=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsIntPow(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsFunctionRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicTextEquals(node->text, "function") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "thread") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFunctionRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsApprox(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "~=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsApprox(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTimeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TIME ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTimeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsXValueRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsBuiltinObjectClass(node) ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicIsXValueType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE_IS || node->kind == XL6_AST_TYPE_AS ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsToStringMemberCallSyntax(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsValueDerive(node) ||
         xl6CgenNeedsBasicIsPersistenceGenericCall(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "var") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ARRAY_LITERAL || node->kind == XL6_AST_LIST_LITERAL ||
         node->kind == XL6_AST_SET_LITERAL || node->kind == XL6_AST_DICT_LITERAL ||
         node->kind == XL6_AST_TYPED_LITERAL ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXValueRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsCryptoRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsCryptoRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsCryptoRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsUrlRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsUrlRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsUrlRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsQueryRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsQueryRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsQueryRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsRegexRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsRegexRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRegexRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsProcessRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsProcessRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsProcessRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsDirRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsDirRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsDirRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsStringEdgeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsStringEdgeRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStringEdgeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsStdStringRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsStdStringRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStdStringRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsPathRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsPathRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsPathRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsHttpServerRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsHttpServerRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttpServerRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
