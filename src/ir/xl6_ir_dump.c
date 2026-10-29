#include "src/ir/xl6_ir_dump.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/common/xl6_output.h"

static void xl6IrIndent(XL6_Output* out, int indent)
{
    int i;

    for ( i = 0; i < indent; ++i ) {
        xl6OutputPuts("  ", out);
    }
}

static void xl6IrText(XL6_Output* out, const char* text)
{
    xl6OutputPuts(text != NULL ? text : "", out);
}

static int xl6IrTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void xl6IrDumpType(XL6_Output* out, const XL6_AstNode* typeNode)
{
    size_t i;
    const XL6_AstNode* args;
    const XL6_AstNode* returns;

    if ( typeNode == NULL ) {
        xl6OutputPuts("<missing-type>", out);
        return;
    }

    if ( xl6IrTextEquals(typeNode->text, "function") && typeNode->childCount == 2 ) {
        args = typeNode->children[0];
        returns = typeNode->children[1];
        xl6OutputPuts("func(", out);
        if ( args != NULL ) {
            for ( i = 0; i < args->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", out);
                }
                xl6IrDumpType(out, args->children[i]);
            }
        }
        xl6OutputPutc(')', out);
        if ( returns != NULL && returns->childCount > 0 ) {
            xl6OutputPuts(" as ", out);
            if ( returns->childCount > 1 ) {
                xl6OutputPutc('(', out);
            }
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", out);
                }
                xl6IrDumpType(out, returns->children[i]);
            }
            if ( returns->childCount > 1 ) {
                xl6OutputPutc(')', out);
            }
        }
        return;
    }

    xl6IrText(out, typeNode->text);
    if ( typeNode->childCount == 0 ) {
        return;
    }

    args = typeNode->children[0];
    if ( (args == NULL) || (args->kind != XL6_AST_LIST) ) {
        return;
    }

    xl6OutputPutc('<', out);
    for ( i = 0; i < args->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", out);
        }
        xl6IrDumpType(out, args->children[i]);
    }
    xl6OutputPutc('>', out);
}

static void xl6IrDumpExpr(XL6_Output* out, const XL6_AstNode* node);
static void xl6IrDumpParams(XL6_Output* out, const XL6_AstNode* params);
static void xl6IrDumpReturns(XL6_Output* out, const XL6_AstNode* returns);

static void xl6IrDumpTypeParams(XL6_Output* out, const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL || params->childCount == 0 ) {
        return;
    }
    xl6OutputPutc('<', out);
    for ( i = 0; i < params->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", out);
        }
        xl6IrText(out, params->children[i]->text);
    }
    xl6OutputPutc('>', out);
}

static void xl6IrDumpExprList(XL6_Output* out, const XL6_AstNode* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", out);
        }
        xl6IrDumpExpr(out, list->children[i]);
    }
}

static void xl6IrDumpPairList(XL6_Output* out, const XL6_AstNode* node)
{
    size_t i;

    for ( i = 0; i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        if ( i > 0 ) {
            xl6OutputPuts(", ", out);
        }
        if ( (pair != NULL) && (pair->kind == XL6_AST_KEY_VALUE) && (pair->childCount >= 2) ) {
            xl6IrDumpExpr(out, pair->children[0]);
            xl6OutputPuts(": ", out);
            xl6IrDumpExpr(out, pair->children[1]);
        }
    }
}

static void xl6IrDumpCallArgs(XL6_Output* out, const XL6_AstNode* args)
{
    size_t i;

    if ( args == NULL ) {
        return;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( i > 0 ) {
            xl6OutputPuts(", ", out);
        }
        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            xl6IrText(out, arg->text);
            xl6OutputPuts(" = ", out);
            if ( arg->childCount > 0 ) {
                xl6IrDumpExpr(out, arg->children[0]);
            }
        } else if ( arg->childCount > 0 ) {
            xl6IrDumpExpr(out, arg->children[0]);
        }
    }
}

static void xl6IrDumpExpr(XL6_Output* out, const XL6_AstNode* node)
{
    if ( node == NULL ) {
        xl6OutputPuts("<null>", out);
        return;
    }

    switch ( node->kind ) {
    case XL6_AST_NAME:
    case XL6_AST_INT:
    case XL6_AST_FLOAT:
    case XL6_AST_STRING:
    case XL6_AST_TIME:
    case XL6_AST_BOOL:
        xl6IrText(out, node->text);
        break;
    case XL6_AST_NULL:
        xl6OutputPuts("null", out);
        break;
    case XL6_AST_ANON_FUNC:
        xl6OutputPuts("func(", out);
        if ( node->childCount > 0 ) {
            xl6IrDumpParams(out, node->children[0]);
        }
        xl6OutputPutc(')', out);
        if ( node->childCount > 1 ) {
            xl6IrDumpReturns(out, node->children[1]);
        }
        xl6OutputPuts(" <body>", out);
        break;
    case XL6_AST_BINARY:
        xl6OutputPutc('(', out);
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPrintf(out, " %s ", node->text != NULL ? node->text : "?");
        if ( node->childCount > 1 ) {
            xl6IrDumpExpr(out, node->children[1]);
        }
        xl6OutputPutc(')', out);
        break;
    case XL6_AST_CONDITIONAL:
        xl6OutputPutc('(', out);
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPuts(" ? ", out);
        if ( node->childCount > 1 ) {
            xl6IrDumpExpr(out, node->children[1]);
        }
        xl6OutputPuts(" : ", out);
        if ( node->childCount > 2 ) {
            xl6IrDumpExpr(out, node->children[2]);
        }
        xl6OutputPutc(')', out);
        break;
    case XL6_AST_UNARY:
        xl6OutputPrintf(out, "(%s ", node->text != NULL ? node->text : "?");
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPutc(')', out);
        break;
    case XL6_AST_CALL:
        xl6OutputPuts("call ", out);
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPutc('(', out);
        if ( node->childCount > 1 ) {
            xl6IrDumpCallArgs(out, node->children[1]);
        }
        xl6OutputPutc(')', out);
        break;
    case XL6_AST_MEMBER:
    case XL6_AST_SAFE_MEMBER:
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPuts(node->kind == XL6_AST_SAFE_MEMBER ? "?." : ".", out);
        xl6IrText(out, node->text);
        break;
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPuts(node->kind == XL6_AST_SAFE_INDEX ? "?[" : "[", out);
        if ( node->childCount > 1 ) {
            xl6IrDumpExpr(out, node->children[1]);
        }
        xl6OutputPutc(']', out);
        break;
    case XL6_AST_GENERIC_INSTANT:
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPuts("::<", out);
        if ( node->childCount > 1 ) {
            const XL6_AstNode* typeArgs = node->children[1];
            size_t i;
            for ( i = 0; i < typeArgs->childCount; ++i ) {
                if ( i > 0 ) {
                    xl6OutputPuts(", ", out);
                }
                xl6IrDumpType(out, typeArgs->children[i]);
            }
        }
        xl6OutputPutc('>', out);
        break;
    case XL6_AST_ARRAY_LITERAL:
        xl6OutputPutc('[', out);
        xl6IrDumpExprList(out, node);
        xl6OutputPutc(']', out);
        break;
    case XL6_AST_LIST_LITERAL:
        xl6OutputPuts("list[", out);
        xl6IrDumpPairList(out, node);
        xl6OutputPutc(']', out);
        break;
    case XL6_AST_SET_LITERAL:
        xl6OutputPuts("set{", out);
        xl6IrDumpExprList(out, node);
        xl6OutputPutc('}', out);
        break;
    case XL6_AST_DICT_LITERAL:
        xl6OutputPuts("dict{", out);
        xl6IrDumpPairList(out, node);
        xl6OutputPutc('}', out);
        break;
    case XL6_AST_TYPED_LITERAL:
        xl6IrText(out, node->text);
        if ( node->childCount > 0 ) {
            const XL6_AstNode* literal = node->children[0];
            if ( (literal->kind == XL6_AST_ARRAY_LITERAL) || (literal->kind == XL6_AST_LIST_LITERAL) ) {
                xl6OutputPutc('[', out);
            } else {
                xl6OutputPutc('{', out);
            }
            if ( literal->kind == XL6_AST_LIST_LITERAL || literal->kind == XL6_AST_DICT_LITERAL ) {
                xl6IrDumpPairList(out, literal);
            } else {
                xl6IrDumpExprList(out, literal);
            }
            if ( (literal->kind == XL6_AST_ARRAY_LITERAL) || (literal->kind == XL6_AST_LIST_LITERAL) ) {
                xl6OutputPutc(']', out);
            } else {
                xl6OutputPutc('}', out);
            }
        }
        break;
    default:
        xl6OutputPrintf(out, "<expr:%s>", xl6AstKindName(node->kind));
        break;
    }
}

static void xl6IrDumpBlock(XL6_Output* out, const XL6_AstNode* block, int indent);

static void xl6IrDumpReturn(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    xl6IrIndent(out, indent);
    xl6OutputPuts("return", out);
    if ( node->childCount > 0 ) {
        xl6OutputPutc(' ', out);
        xl6IrDumpExprList(out, node);
    }
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpDecl(XL6_Output* out, const XL6_AstNode* node, int indent, const char* kind)
{
    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "%s %s : ", kind, node->text != NULL ? node->text : "<unnamed>");
    if ( node->childCount > 0 ) {
        xl6IrDumpType(out, node->children[0]);
    } else {
        xl6OutputPuts("<missing-type>", out);
    }
    if ( node->childCount > 1 ) {
        xl6OutputPuts(" = ", out);
        xl6IrDumpExpr(out, node->children[1]);
    }
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpMultiDecl(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    size_t i;
    const XL6_AstNode* names;

    xl6IrIndent(out, indent);
    xl6OutputPuts("var ", out);
    names = node->childCount > 0 ? node->children[0] : NULL;
    if ( names != NULL ) {
        for ( i = 0; i < names->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrText(out, names->children[i]->text);
        }
    }
    xl6OutputPuts(" : auto = ", out);
    if ( node->childCount > 1 ) {
        xl6IrDumpExpr(out, node->children[1]);
    }
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpMultiAssign(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    size_t i;
    const XL6_AstNode* targets;

    xl6IrIndent(out, indent);
    targets = node->childCount > 0 ? node->children[0] : NULL;
    if ( targets != NULL ) {
        for ( i = 0; i < targets->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrDumpExpr(out, targets->children[i]);
        }
    }
    xl6OutputPuts(" = ", out);
    if ( node->childCount > 1 ) {
        xl6IrDumpExpr(out, node->children[1]);
    }
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpIf(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    size_t i;
    const XL6_AstNode* elseifList;

    xl6IrIndent(out, indent);
    xl6OutputPuts("if ", out);
    if ( node->childCount > 0 ) {
        xl6IrDumpExpr(out, node->children[0]);
    }
    xl6OutputPutc('\n', out);

    if ( node->childCount > 1 ) {
        xl6IrDumpBlock(out, node->children[1], indent + 1);
    }

    if ( node->childCount > 2 ) {
        elseifList = node->children[2];
        for ( i = 0; i < elseifList->childCount; ++i ) {
            const XL6_AstNode* item = elseifList->children[i];
            xl6IrIndent(out, indent);
            xl6OutputPuts("elseif ", out);
            if ( item->childCount > 0 ) {
                xl6IrDumpExpr(out, item->children[0]);
            }
            xl6OutputPutc('\n', out);
            if ( item->childCount > 1 ) {
                xl6IrDumpBlock(out, item->children[1], indent + 1);
            }
        }
    }

    if ( node->childCount > 3 && node->children[3]->childCount > 0 ) {
        xl6IrIndent(out, indent);
        xl6OutputPuts("else\n", out);
        xl6IrDumpBlock(out, node->children[3], indent + 1);
    }

    xl6IrIndent(out, indent);
    xl6OutputPuts("end_if\n", out);
}

static void xl6IrDumpWhile(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    xl6IrIndent(out, indent);
    xl6OutputPuts("while ", out);
    if ( node->childCount > 0 ) {
        xl6IrDumpExpr(out, node->children[0]);
    }
    xl6OutputPutc('\n', out);
    if ( node->childCount > 1 ) {
        xl6IrDumpBlock(out, node->children[1], indent + 1);
    }
    xl6IrIndent(out, indent);
    xl6OutputPuts("end_while\n", out);
}

static void xl6IrDumpDoWhile(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    xl6IrIndent(out, indent);
    xl6OutputPuts("do\n", out);
    if ( node->childCount > 0 ) {
        xl6IrDumpBlock(out, node->children[0], indent + 1);
    }
    xl6IrIndent(out, indent);
    xl6OutputPuts("while ", out);
    if ( node->childCount > 1 ) {
        xl6IrDumpExpr(out, node->children[1]);
    }
    xl6OutputPutc('\n', out);
    xl6IrIndent(out, indent);
    xl6OutputPuts("end_do\n", out);
}

static void xl6IrDumpFor(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    size_t i;
    const XL6_AstNode* targets;

    xl6IrIndent(out, indent);
    xl6OutputPuts("for ", out);
    if ( node->childCount > 0 ) {
        targets = node->children[0];
        for ( i = 0; i < targets->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrText(out, targets->children[i]->text);
        }
    }
    xl6OutputPuts(" in ", out);
    if ( node->childCount > 1 ) {
        xl6IrDumpExpr(out, node->children[1]);
    }
    xl6OutputPutc('\n', out);
    if ( node->childCount > 2 ) {
        xl6IrDumpBlock(out, node->children[2], indent + 1);
    }
    xl6IrIndent(out, indent);
    xl6OutputPuts("end_for\n", out);
}

static void xl6IrDumpStmt(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    if ( node == NULL ) {
        return;
    }

    switch ( node->kind ) {
    case XL6_AST_FUNC:
        break;
    case XL6_AST_VAR_DECL:
        xl6IrDumpDecl(out, node, indent, "var");
        break;
    case XL6_AST_MULTI_VAR_DECL:
        xl6IrDumpMultiDecl(out, node, indent);
        break;
    case XL6_AST_CONST_DECL:
        xl6IrDumpDecl(out, node, indent, "const");
        break;
    case XL6_AST_ASSIGN:
        xl6IrIndent(out, indent);
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPrintf(out, " %s ", node->text != NULL ? node->text : "=");
        if ( node->childCount > 1 ) {
            xl6IrDumpExpr(out, node->children[1]);
        }
        xl6OutputPutc('\n', out);
        break;
    case XL6_AST_MULTI_ASSIGN:
        xl6IrDumpMultiAssign(out, node, indent);
        break;
    case XL6_AST_RETURN:
        xl6IrDumpReturn(out, node, indent);
        break;
    case XL6_AST_EXPR_STMT:
        xl6IrIndent(out, indent);
        if ( node->childCount > 0 ) {
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPutc('\n', out);
        break;
    case XL6_AST_IF:
        xl6IrDumpIf(out, node, indent);
        break;
    case XL6_AST_WHILE:
        xl6IrDumpWhile(out, node, indent);
        break;
    case XL6_AST_DO_WHILE:
        xl6IrDumpDoWhile(out, node, indent);
        break;
    case XL6_AST_FOR:
        xl6IrDumpFor(out, node, indent);
        break;
    case XL6_AST_BREAK:
        xl6IrIndent(out, indent);
        xl6OutputPuts("break", out);
        if ( node->childCount > 0 ) {
            xl6OutputPutc(' ', out);
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPutc('\n', out);
        break;
    case XL6_AST_CONTINUE:
        xl6IrIndent(out, indent);
        xl6OutputPuts("continue", out);
        if ( node->childCount > 0 ) {
            xl6OutputPutc(' ', out);
            xl6IrDumpExpr(out, node->children[0]);
        }
        xl6OutputPutc('\n', out);
        break;
    default:
        xl6IrIndent(out, indent);
        xl6OutputPrintf(out, "<stmt:%s>\n", xl6AstKindName(node->kind));
        break;
    }
}

static void xl6IrDumpBlock(XL6_Output* out, const XL6_AstNode* block, int indent)
{
    size_t i;

    if ( block == NULL ) {
        return;
    }
    for ( i = 0; i < block->childCount; ++i ) {
        xl6IrDumpStmt(out, block->children[i], indent);
    }
}

static void xl6IrDumpParams(XL6_Output* out, const XL6_AstNode* params)
{
    size_t i;

    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrText(out, param->text);
            xl6OutputPuts(": ", out);
            if ( param->childCount > 0 ) {
                xl6IrDumpType(out, param->children[0]);
            }
            if ( param->childCount > 1 ) {
                xl6OutputPuts(" = ", out);
                xl6IrDumpExpr(out, param->children[1]);
            }
        }
    }
}

static void xl6IrDumpReturns(XL6_Output* out, const XL6_AstNode* returns)
{
    size_t i;

    if ( returns == NULL || returns->childCount == 0 ) {
        return;
    }
    xl6OutputPuts(" as ", out);
    if ( returns->childCount > 1 ) {
        xl6OutputPutc('(', out);
    }
    for ( i = 0; i < returns->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", out);
        }
        xl6IrDumpType(out, returns->children[i]);
    }
    if ( returns->childCount > 1 ) {
        xl6OutputPutc(')', out);
    }
}

static void xl6IrDumpFuncAt(XL6_Output* out, const XL6_AstNode* node, int indent, const char* endLabel)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeParams;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "func %s", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    typeParams = node->childCount > 4 ? node->children[4] : NULL;
    xl6IrDumpTypeParams(out, typeParams);
    xl6OutputPutc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    xl6IrDumpParams(out, params);
    xl6OutputPutc(')', out);

    returns = node->childCount > 2 ? node->children[2] : NULL;
    xl6IrDumpReturns(out, returns);
    xl6OutputPutc('\n', out);

    if ( node->childCount > 3 ) {
        xl6IrDumpBlock(out, node->children[3], indent + 1);
    }
    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "%s\n", endLabel != NULL ? endLabel : "end_func");
}

static void xl6IrDumpFunc(XL6_Output* out, const XL6_AstNode* node)
{
    xl6IrDumpFuncAt(out, node, 0, "end_func");
}

static void xl6IrDumpExternFunc(XL6_Output* out, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    xl6OutputPrintf(out, "extern %s func %s(", node->text != NULL ? node->text : "\"\"", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrText(out, param->text);
            xl6OutputPuts(": ", out);
            if ( param->childCount > 0 ) {
                xl6IrDumpType(out, param->children[0]);
            }
        }
    }
    xl6OutputPutc(')', out);

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 0 ) {
        xl6OutputPuts(" as ", out);
        if ( returns->childCount > 1 ) {
            xl6OutputPutc('(', out);
        }
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrDumpType(out, returns->children[i]);
        }
        if ( returns->childCount > 1 ) {
            xl6OutputPutc(')', out);
        }
    }
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpExportFunc(XL6_Output* out, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    xl6OutputPrintf(out, "export %s func %s(", node->text != NULL ? node->text : "\"\"", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrText(out, param->text);
            xl6OutputPuts(": ", out);
            if ( param->childCount > 0 ) {
                xl6IrDumpType(out, param->children[0]);
            }
        }
    }
    xl6OutputPutc(')', out);

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 0 ) {
        xl6OutputPuts(" as ", out);
        if ( returns->childCount > 1 ) {
            xl6OutputPutc('(', out);
        }
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", out);
            }
            xl6IrDumpType(out, returns->children[i]);
        }
        if ( returns->childCount > 1 ) {
            xl6OutputPutc(')', out);
        }
    }
    xl6OutputPutc('\n', out);

    if ( node->childCount > 3 ) {
        xl6IrDumpBlock(out, node->children[3], 1);
    }
    xl6OutputPuts("end_export\n", out);
}

static void xl6IrDumpExternalVar(XL6_Output* out, const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    typeNode = node->childCount > 1 ? node->children[1] : NULL;
    initExpr = node->childCount > 2 ? node->children[2] : NULL;

    if ( node->kind == XL6_AST_EXTERN_VAR && (node->flags & XL6_AST_FLAG_PUBLIC) != 0 ) {
        xl6OutputPuts("export extern ", out);
    } else if ( node->kind == XL6_AST_EXTERN_VAR ) {
        xl6OutputPuts("extern ", out);
    } else {
        xl6OutputPuts("export ", out);
    }
    if ( node->text != NULL && node->text[0] != '\0' ) {
        xl6OutputPrintf(out, "%s ", node->text);
    }
    if ( node->kind == XL6_AST_EXPORT_CONST ) {
        xl6OutputPuts("const ", out);
    }
    xl6IrDumpType(out, typeNode);
    xl6OutputPrintf(out, " %s", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    if ( initExpr != NULL ) {
        xl6OutputPuts(" = ", out);
        xl6IrDumpExpr(out, initExpr);
    }
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpTypeAlias(XL6_Output* out, const XL6_AstNode* node)
{
    const XL6_AstNode* target;

    target = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    if ( node != NULL && node->kind == XL6_AST_EXTERN_TYPE_ALIAS ) {
        xl6OutputPuts("extern type ", out);
    } else {
        xl6OutputPuts("typedef ", out);
    }
    xl6OutputPrintf(out, "%s = ", (node != NULL && node->text != NULL) ? node->text : "<unnamed>");
    xl6IrDumpType(out, target);
    xl6OutputPutc('\n', out);
}

static void xl6IrDumpClassSpecial(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    const XL6_AstNode* params;

    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "%s(", node->text != NULL ? node->text : "<special>");
    params = node->childCount > 0 ? node->children[0] : NULL;
    xl6IrDumpParams(out, params);
    xl6OutputPuts(")\n", out);
    if ( node->childCount > 1 ) {
        xl6IrDumpBlock(out, node->children[1], indent + 1);
    }
    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "end_%s\n", node->text != NULL ? node->text : "special");
}

static void xl6IrDumpNativeExternSpecial(XL6_Output* out, const XL6_AstNode* node, int indent);
static void xl6IrDumpNativeExternFunc(XL6_Output* out, const XL6_AstNode* node, int indent);

static void xl6IrDumpClass(XL6_Output* out, const XL6_AstNode* node)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    const XL6_AstNode* typeParams;
    size_t i;

    xl6OutputPrintf(out, "class %s", node->text != NULL ? node->text : "<unnamed>");
    typeParams = node->childCount > 2 ? node->children[2] : NULL;
    xl6IrDumpTypeParams(out, typeParams);
    base = node->childCount > 0 ? node->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        xl6OutputPrintf(out, " extends %s", base->text != NULL ? base->text : "<unnamed>");
    }
    xl6OutputPutc('\n', out);

    body = node->childCount > 1 ? node->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_FIELD ) {
                xl6IrDumpDecl(out, member, 1, "field");
            } else if ( member->kind == XL6_AST_FUNC ) {
                xl6IrDumpFuncAt(out, member, 1, "end_func");
            } else if ( member->kind == XL6_AST_CREATE ) {
                xl6IrDumpClassSpecial(out, member, 1);
            } else if ( member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
                xl6IrDumpNativeExternSpecial(out, member, 1);
            } else if ( member->kind == XL6_AST_NATIVE_EXTERN_FUNC ) {
                xl6IrDumpNativeExternFunc(out, member, 1);
            } else {
                xl6IrIndent(out, 1);
                xl6OutputPrintf(out, "<class-member:%s>\n", xl6AstKindName(member->kind));
            }
        }
    }
    xl6OutputPuts("end_class\n", out);
}

static void xl6IrDumpNativeExternSpecial(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;

    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "extern %s %s(", node->text != NULL ? node->text : "\"\"", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<special>");
    xl6IrDumpParams(out, params);
    xl6OutputPuts(")\n", out);
}

static void xl6IrDumpNativeExternFunc(XL6_Output* out, const XL6_AstNode* node, int indent)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;
    returns = node->childCount > 2 ? node->children[2] : NULL;

    xl6IrIndent(out, indent);
    xl6OutputPrintf(out, "extern %s func %s(", node->text != NULL ? node->text : "\"\"", (nameNode != NULL && nameNode->text != NULL) ? nameNode->text : "<unnamed>");
    xl6IrDumpParams(out, params);
    xl6OutputPutc(')', out);
    xl6IrDumpReturns(out, returns);
    xl6OutputPutc('\n', out);
}

void xl6IrDumpText(XL6_Output* out, const XL6_AstNode* root)
{
    size_t i;

    if ( out == NULL ) {
        return;
    }
    xl6OutputPuts("xlir 0\n", out);
    if ( root == NULL ) {
        return;
    }

    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* child = root->children[i];
        if ( child->kind == XL6_AST_IMPORT ) {
            xl6OutputPuts("import ", out);
            xl6IrText(out, child->text);
            xl6OutputPutc('\n', out);
        } else if ( child->kind == XL6_AST_FUNC ) {
            xl6IrDumpFunc(out, child);
        } else if ( child->kind == XL6_AST_EXTERN_FUNC ) {
            xl6IrDumpExternFunc(out, child);
        } else if ( child->kind == XL6_AST_EXPORT_FUNC ) {
            xl6IrDumpExportFunc(out, child);
        } else if ( child->kind == XL6_AST_EXTERN_VAR ||
                    child->kind == XL6_AST_EXPORT_VAR ||
                    child->kind == XL6_AST_EXPORT_CONST ) {
            xl6IrDumpExternalVar(out, child);
        } else if ( child->kind == XL6_AST_TYPE_ALIAS || child->kind == XL6_AST_EXTERN_TYPE_ALIAS ) {
            xl6IrDumpTypeAlias(out, child);
        } else if ( child->kind == XL6_AST_CLASS ) {
            xl6IrDumpClass(out, child);
        } else {
            xl6IrDumpStmt(out, child, 0);
        }
    }
}

void xl6IrDumpJson(XL6_Output* out, const XL6_AstNode* root)
{
    if ( out == NULL ) {
        return;
    }
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("  \"stage\":\"ir\",\n", out);
    xl6OutputPuts("  \"root\":", out);
    xl6AstDumpJson(out, root);
    xl6OutputPuts("}\n", out);
}
