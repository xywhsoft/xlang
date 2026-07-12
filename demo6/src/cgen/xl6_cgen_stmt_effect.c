/*
    这里只判断一条语句执行后是否可能留下运行时错误。
    未知调用和协议相关表达式一律保守处理；纯字面量、名称和类型节点不轮询 TLS。
*/
static int xl6CgenNodeMayRaise(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    switch ( node->kind ) {
    case XL6_AST_CALL:
    case XL6_AST_MEMBER:
    case XL6_AST_SAFE_MEMBER:
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
    case XL6_AST_ASSIGN:
    case XL6_AST_MULTI_ASSIGN:
    case XL6_AST_CONDITIONAL:
    case XL6_AST_BINARY:
    case XL6_AST_UNARY:
    case XL6_AST_TYPE_AS:
    case XL6_AST_AWAIT:
    case XL6_AST_ANON_FUNC:
    case XL6_AST_INTERP_STRING:
    case XL6_AST_ARRAY_LITERAL:
    case XL6_AST_LIST_LITERAL:
    case XL6_AST_SET_LITERAL:
    case XL6_AST_DICT_LITERAL:
    case XL6_AST_TYPED_LITERAL:
    case XL6_AST_FOR:
    case XL6_AST_THROW:
    case XL6_AST_ASSERT:
    case XL6_AST_YIELD:
        return 1;

    case XL6_AST_NAME:
    case XL6_AST_TYPE:
    case XL6_AST_INT:
    case XL6_AST_FLOAT:
    case XL6_AST_STRING:
    case XL6_AST_INTERP_TEXT:
    case XL6_AST_TIME:
    case XL6_AST_BOOL:
    case XL6_AST_NULL:
        return 0;

    default:
        break;
    }

    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeMayRaise(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
