static void xl6CgenForIn(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* iterableType;

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "code generation error");
        return;
    }
    if ( xl6CgenIsRangeCall(node->children[1]) ) {
        xl6CgenForRange(ctx, node);
        return;
    }

    iterableType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    if ( !xl6CgenIsForIterableType(iterableType) ) {
        xl6CgenError(ctx, node->children[1], "for in right side must be range, array, list, set, dict, generator or var");
        xl6AstFree(iterableType);
        return;
    }

    if ( xl6CgenIsGeneratorType(iterableType) ) {
        xl6CgenForGeneratorIterable(ctx, node, iterableType);
    } else if ( xl6CgenExprUsesTypedContainerStorage(ctx, node->children[1]) ||
                xl6CgenExprIsTypedContainerReturnCall(ctx, node->children[1]) ) {
        xl6CgenForTypedContainerIterable(ctx, node, iterableType);
    } else {
        xl6CgenForXValueIterable(ctx, node, iterableType);
    }
    xl6AstFree(iterableType);
}

static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return;
    }

    switch ( node->kind ) {
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
        xl6CgenDecl(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_MULTI_VAR_DECL:
        xl6CgenMultiDecl(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_ASSIGN:
        xl6CgenAssignStmt(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_MULTI_ASSIGN:
        xl6CgenMultiAssign(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_RETURN:
        if ( ctx->currentAsyncWorker ) {
            xl6CgenAsyncReturnStmt(ctx, node);
            break;
        }
        if ( node->childCount > 0 ) {
            int id = ctx->tempId++;
            char returnName[64];

            if ( ctx->currentReturns != NULL && ctx->currentReturns->childCount > 1 ) {
                size_t i;

                xl6CgenIndent(ctx);
                if ( ctx->currentReturnFunc != NULL ) {
                    xl6CgenFuncReturnStructName(ctx, ctx->currentReturnFunc);
                } else {
                    xl6CgenFunctionReturnStructName(ctx, ctx->currentReturnParams, ctx->currentReturns);
                }
                xl6OutputPrintf(ctx->out, " __xl6_return_%d;\n", id);
                for ( i = 0; i < node->childCount; ++i ) {
                    const XL6_AstNode* retType = ctx->currentReturns->children[i];
                    char returnValuePath[96];

                    xl6CgenIndent(ctx);
                    xl6OutputPrintf(ctx->out, "__xl6_return_%d.value%u = ", id, (unsigned)i);
                    if ( xl6CgenIsXValueType(retType) ) {
                        xl6CgenXValueFromExprForTarget(ctx, retType, node->children[i]);
                    } else if ( xl6CgenIsFunctionType(retType) ) {
                        xl6CgenFunctionValueExpr(ctx, node->children[i], retType);
                    } else {
                        xl6CgenExpr(ctx, node->children[i]);
                    }
                    xl6OutputPuts(";\n", ctx->out);
                    if ( xl6CgenIsFunctionType(retType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[i]) ) {
                        xl6CgenIndent(ctx);
                        xl6OutputPrintf(ctx->out, "if (__xl6_return_%d.value%u.retain != 0 && __xl6_return_%d.value%u.env != 0) {\n",
                                        id, (unsigned)i, id, (unsigned)i);
                        ctx->indent += 1;
                        xl6CgenIndent(ctx);
                        xl6OutputPrintf(ctx->out, "__xl6_return_%d.value%u.retain(__xl6_return_%d.value%u.env);\n",
                                        id, (unsigned)i, id, (unsigned)i);
                        ctx->indent -= 1;
                        xl6CgenIndent(ctx);
                        xl6OutputPuts("}\n", ctx->out);
                    }
                    snprintf(returnValuePath, sizeof(returnValuePath), "__xl6_return_%d.value%u", id, (unsigned)i);
                    xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, retType, returnValuePath, node->children[i]);
                }
            } else if ( ctx->currentReturnType != NULL &&
                        ctx->currentClass == NULL &&
                        ctx->currentCustom == NULL &&
                        xl6CgenFuncSingleReturnUsesTypedContainerStorage(ctx, ctx->currentReturnFunc) ) {
                xl6CgenTypedContainerReturnStmt(ctx, ctx->currentReturnType, node->children[0], id);
            } else {
                xl6CgenIndent(ctx);
                xl6OutputPuts(ctx->currentReturnType != NULL ? xl6CgenTypeName(ctx, ctx->currentReturnType) : xl6CgenInferExprType(ctx, node->children[0]), ctx->out);
                xl6OutputPrintf(ctx->out, " __xl6_return_%d = ", id);
                if ( xl6CgenIsXValueType(ctx->currentReturnType) ) {
                    xl6CgenXValueFromExprForTarget(ctx, ctx->currentReturnType, node->children[0]);
                } else if ( xl6CgenIsFunctionType(ctx->currentReturnType) ) {
                    xl6CgenFunctionValueExpr(ctx, node->children[0], ctx->currentReturnType);
                } else {
                    xl6CgenExpr(ctx, node->children[0]);
                }
                xl6OutputPuts(";\n", ctx->out);
                if ( xl6CgenIsFunctionType(ctx->currentReturnType) && xl6CgenExprIsFunctionBorrow(ctx, node->children[0]) ) {
                    snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                    xl6CgenRetainFunctionValue(ctx, returnName);
                }
                snprintf(returnName, sizeof(returnName), "__xl6_return_%d", id);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, ctx->currentReturnType, returnName, node->children[0]);
            }
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "return __xl6_return_%d;\n", id);
        } else {
            xl6CgenEmitCleanupsSince(ctx, 0);
            xl6CgenIndent(ctx);
            xl6OutputPuts("return;\n", ctx->out);
        }
        break;
    case XL6_AST_EXPR_STMT:
        xl6CgenIndent(ctx);
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_IF:
        xl6CgenIf(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_WHILE:
        xl6CgenWhile(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_DO_WHILE:
        xl6CgenDoWhile(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_FOR:
        xl6CgenForIn(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;
    case XL6_AST_THROW:
        xl6CgenThrowStmt(ctx, node);
        break;
    case XL6_AST_ASSERT:
        xl6CgenAssertStmt(ctx, node);
        break;
    case XL6_AST_TRY:
        xl6CgenTryStmt(ctx, node);
        break;
    case XL6_AST_BREAK:
        xl6CgenLoopJump(ctx, node, 0);
        break;
    case XL6_AST_CONTINUE:
        xl6CgenLoopJump(ctx, node, 1);
        break;
    case XL6_AST_YIELD:
        xl6CgenYieldStmt(ctx, node);
        xl6CgenEmitExceptionPropagate(ctx, node);
        break;

    default:
        xl6CgenError(ctx, node, "code generation error");
        break;
    }
}
