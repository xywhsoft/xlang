static const char* xl6CgenTypedContainerEqualsFunc(const char* kind)
{
    if ( xl6CTextEquals(kind, "array") ) { return "xl6TypedArrayEquals"; }
    if ( xl6CTextEquals(kind, "list") ) { return "xl6TypedListEquals"; }
    if ( xl6CTextEquals(kind, "set") ) { return "xl6TypedSetEquals"; }
    if ( xl6CTextEquals(kind, "dict") ) { return "xl6TypedDictEquals"; }
    return NULL;
}

static int xl6CgenTypedContainerBinaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* resultType;
    XL6_AstNode* leftType;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    const char* kind;
    const char* op;
    const char* eqFunc;

    resultType = NULL;
    if ( !xl6CgenTypedContainerBinaryInfo(ctx, node, &resultType, NULL) ) {
        return 0;
    }
    xl6AstFree(resultType);

    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;
    op = node->text != NULL ? node->text : "";
    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    concreteType = xl6CgenTypedContainerConcreteType(ctx, leftType);
    kind = concreteType != NULL ? concreteType->text : NULL;

    if ( kind == NULL ) {
        xl6AstFree(leftType);
        return 0;
    }

    eqFunc = xl6CgenTypedContainerEqualsFunc(kind);
    if ( eqFunc != NULL && (xl6CTextEquals(op, "==") || xl6CTextEquals(op, "!=")) ) {
        if ( xl6CTextEquals(op, "!=") ) {
            xl6OutputPuts("(!", ctx->out);
        }
        xl6OutputPuts(eqFunc, ctx->out);
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPutc(')', ctx->out);
        if ( xl6CTextEquals(op, "!=") ) {
            xl6OutputPutc(')', ctx->out);
        }
        xl6AstFree(leftType);
        return 1;
    }

    if ( xl6CTextEquals(kind, "array") && xl6CTextEquals(op, "+") ) {
        xl6OutputPuts("xl6TypedArrayConcat(", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(leftType);
        return 1;
    }

    if ( xl6CTextEquals(kind, "list") ) {
        if ( xl6CTextEquals(op, "+") ) {
            xl6OutputPuts("xl6_xrt_typed_list_concat(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPutc(')', ctx->out);
            xl6AstFree(leftType);
            return 1;
        }
        if ( xl6CTextEquals(op, "-") ) {
            xl6OutputPuts("xl6_xrt_typed_list_without_key(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts("))", ctx->out);
            xl6AstFree(leftType);
            return 1;
        }
    }

    if ( xl6CTextEquals(kind, "dict") ) {
        if ( xl6CTextEquals(op, "+") ) {
            xl6OutputPuts("xl6_xrt_typed_dict_concat(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPutc(')', ctx->out);
            xl6AstFree(leftType);
            return 1;
        }
        if ( xl6CTextEquals(op, "-") ) {
            xl6OutputPuts("xl6_xrt_typed_dict_without_key(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts(", 0)", ctx->out);
            xl6AstFree(leftType);
            return 1;
        }
    }

    if ( xl6CTextEquals(kind, "set") ) {
        if ( xl6CTextEquals(op, "|") || xl6CTextEquals(op, "&") ||
             xl6CTextEquals(op, "-") || xl6CTextEquals(op, "^") ) {
            if ( xl6CTextEquals(op, "|") ) { xl6OutputPuts("xl6TypedSetUnion(", ctx->out); }
            else if ( xl6CTextEquals(op, "&") ) { xl6OutputPuts("xl6TypedSetIntersection(", ctx->out); }
            else if ( xl6CTextEquals(op, "-") ) { xl6OutputPuts("xl6TypedSetDifference(", ctx->out); }
            else { xl6OutputPuts("xl6TypedSetSymmetricDifference(", ctx->out); }
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPutc(')', ctx->out);
            xl6AstFree(leftType);
            return 1;
        }
        if ( xl6CTextEquals(op, "<") || xl6CTextEquals(op, "<=") ||
             xl6CTextEquals(op, ">") || xl6CTextEquals(op, ">=") ) {
            int mode = xl6CTextEquals(op, "<=") ? 0 :
                       xl6CTextEquals(op, "<") ? 1 :
                       xl6CTextEquals(op, ">=") ? 2 : 3;
            xl6OutputPrintf(ctx->out, "xl6_xrt_typed_set_relation(");
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPrintf(ctx->out, ", %d)", mode);
            xl6AstFree(leftType);
            return 1;
        }
    }

    xl6AstFree(leftType);
    return 0;
}

static int xl6CgenCheckedIntegerDivisionExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    const char* helper;
    int isUnsigned;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         (!xl6CTextEquals(node->text, "/") && !xl6CTextEquals(node->text, "%")) ) {
        return 0;
    }

    rawTypeNode = xl6CgenCloneExprTypeForInference(ctx, node);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawTypeNode));
    if ( typeNode == NULL || !xl6CgenIsIntegerTypeName(typeNode->text) ) {
        xl6AstFree(rawTypeNode);
        return 0;
    }

    isUnsigned = xl6CgenIsUnsignedIntegerTypeName(typeNode->text);
    helper = xl6CTextEquals(node->text, "/") ?
             (isUnsigned ? "xl6_u64_div_checked" : "xl6_i64_div_checked") :
             (isUnsigned ? "xl6_u64_mod_checked" : "xl6_i64_mod_checked");
    xl6OutputPrintf(ctx->out, "%s((%s)(", helper, isUnsigned ? "unsigned long long" : "long long");
    xl6CgenExpr(ctx, node->children[0]);
    xl6OutputPrintf(ctx->out, "), (%s)(", isUnsigned ? "unsigned long long" : "long long");
    xl6CgenExpr(ctx, node->children[1]);
    xl6OutputPrintf(ctx->out, "), %d, %d)", node->line, node->column);
    xl6AstFree(rawTypeNode);
    return 1;
}

static int xl6CgenCheckedIntegerShiftExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    const char* cType;
    const char* helper;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         (!xl6CTextEquals(node->text, "<<") && !xl6CTextEquals(node->text, ">>")) ) {
        return 0;
    }

    rawTypeNode = xl6CgenCloneExprTypeForInference(ctx, node);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawTypeNode));
    if ( typeNode == NULL || !xl6CgenIsIntegerTypeName(typeNode->text) ) {
        xl6AstFree(rawTypeNode);
        return 0;
    }

    cType = xl6CgenTypeName(ctx, typeNode);
    helper = xl6CTextEquals(node->text, "<<") ? "xl6_u64_shl_checked" : "xl6_u64_shr_checked";
    xl6OutputPrintf(ctx->out, "((%s)%s((unsigned long long)(", cType, helper);
    xl6CgenExpr(ctx, node->children[0]);
    xl6OutputPuts("), (long long)(", ctx->out);
    xl6CgenExpr(ctx, node->children[1]);
    xl6OutputPrintf(ctx->out,
                    "), (unsigned int)(sizeof(%s) * 8u), %d, %d))",
                    cType,
                    node->line,
                    node->column);
    xl6AstFree(rawTypeNode);
    return 1;
}

static void xl6CgenExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t replacementIndex;

    if ( node == NULL ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }

    for ( replacementIndex = 0; replacementIndex < ctx->exprReplacementCount; ++replacementIndex ) {
        const XL6_CgenExprReplacement* replacement = &ctx->exprReplacements[replacementIndex];
        if ( replacement->expr == node && replacement->cName != NULL ) {
            xl6OutputPuts(replacement->cName, ctx->out);
            return;
        }
    }

    switch ( node->kind ) {
    case XL6_AST_NAME:
        if ( xl6CgenRuntimeNarrowedNameExpr(ctx, node) ) {
            break;
        }
        if ( xl6CgenNullableNarrowedNameExpr(ctx, node) ) {
            break;
        }
        if ( xl6CgenAnonCaptureIndex(ctx->currentAnonFunc, node->text) >= 0 ) {
            xl6OutputPuts("__env->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, node->text);
            break;
        }
        if ( xl6CgenEmitImplicitSelfFieldReference(ctx, node->text) ) {
            break;
        }
        if ( xl6CgenFindTrackedVarType(ctx, node->text) == NULL ) {
            const XL6_AstNode* externalVar = xl6CgenFindExternalVar(ctx, node->text);
            if ( externalVar != NULL ) {
                xl6CgenExternalVarStorageName(ctx->out, externalVar);
                break;
            }
        }
        if ( xl6CgenFindVarType(ctx, node->text) == NULL && xl6CgenIsKnownTypeName(ctx, node->text) ) {
            xl6CgenEmitTypeIdForName(ctx, node->text, node);
            break;
        }
        xl6CgenNamedValueReference(ctx, node->text);
        break;
    case XL6_AST_INT:
        xl6CgenIntLiteralExpr(ctx, node);
        break;
    case XL6_AST_FLOAT:
        xl6CgenFloatLiteralExpr(ctx, node);
        break;
    case XL6_AST_STRING:
        xl6CgenStringLiteralExpr(ctx, node);
        break;
    case XL6_AST_INTERP_STRING:
        xl6CgenFStringExpr(ctx, node);
        break;
    case XL6_AST_TIME:
        xl6CgenTimeLiteralExpr(ctx, node);
        break;
    case XL6_AST_BOOL:
        xl6OutputPuts(xl6CTextEquals(node->text, "true") ? "1" : "0", ctx->out);
        break;
    case XL6_AST_NULL:
        xl6OutputPuts("0", ctx->out);
        break;
    case XL6_AST_ARRAY_LITERAL:
        xl6CgenArrayLiteralExpr(ctx, node);
        break;
    case XL6_AST_LIST_LITERAL:
        xl6CgenListLiteralExpr(ctx, node);
        break;
    case XL6_AST_SET_LITERAL:
        xl6CgenSetLiteralExpr(ctx, node);
        break;
    case XL6_AST_DICT_LITERAL:
        xl6CgenDictLiteralExpr(ctx, node);
        break;
    case XL6_AST_TYPED_LITERAL:
        xl6CgenTypedLiteralExpr(ctx, node);
        break;
    case XL6_AST_ANON_FUNC:
        xl6CgenAnonMakeCall(ctx, node);
        break;
    case XL6_AST_AWAIT:
        xl6CgenAwaitExpr(ctx, node);
        break;
    case XL6_AST_MEMBER:
        if ( !xl6CgenNumericEnumMemberExpr(ctx, node) &&
             !xl6CgenStaticClassFieldExpr(ctx, node) &&
             !xl6CgenErrorMemberAccess(ctx, node) &&
             !xl6CgenStaticStringMemberAccess(ctx, node) &&
             !xl6CgenTypedContainerMemberAccess(ctx, node) &&
             !xl6CgenClassPropertyMemberAccess(ctx, node) &&
             !xl6CgenXValueMemberAccess(ctx, node) &&
             !xl6CgenPlainRecordFieldAccess(ctx, node) ) {
            xl6CgenClassFieldAccess(ctx, node);
        }
        break;
    case XL6_AST_SAFE_MEMBER:
        if ( !xl6CgenSafeClassFieldAccess(ctx, node) &&
             !xl6CgenSafeStringMemberAccess(ctx, node) &&
             !xl6CgenSafeTypedContainerMemberAccess(ctx, node) &&
             !xl6CgenSafeXValueMemberAccess(ctx, node) ) {
            xl6CgenError(ctx, node, "safe member access has no semantic lowering plan");
            xl6OutputPuts("0", ctx->out);
        }
        break;
    case XL6_AST_INDEX:
        xl6CgenIndexExpr(ctx, node);
        break;
    case XL6_AST_SAFE_INDEX:
        xl6CgenIndexExpr(ctx, node);
        break;
    case XL6_AST_TYPE_IS:
        xl6CgenRuntimeTypeIsExpr(ctx, node);
        break;
    case XL6_AST_TYPE_AS:
        xl6CgenRuntimeTypeAsExpr(ctx, node);
        break;
    case XL6_AST_BINARY:
        if ( xl6CgenNullableNullComparison(ctx, node) ) {
            break;
        }
        if ( xl6CgenFunctionNullComparison(ctx, node) ) {
            break;
        }
        if ( xl6CgenBinaryPlusIsString(ctx, node) ) {
            xl6CgenStringConcatExpr(ctx, node);
            break;
        }
        if ( xl6CgenClassProtocolBinaryExpr(ctx, node) ) {
            break;
        }
        if ( xl6CgenTypedContainerBinaryExpr(ctx, node) ) {
            break;
        }
        if ( xl6CTextEquals(node->text, "~=") ) {
            if ( xl6CgenBinaryApproxIsXValue(ctx, node) ) {
                xl6CgenXValueApproxExpr(ctx, node);
            } else {
                xl6CgenApproxExpr(ctx, node);
            }
            break;
        }
        if ( xl6CgenBinaryLogicalIsXValue(ctx, node) ) {
            xl6CgenXValueLogicalExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryCompareIsTypeValue(ctx, node) ) {
            xl6CgenTypeCompareExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryCompareIsString(ctx, node) ) {
            xl6CgenStringCompareExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryCompareIsXValue(ctx, node) ) {
            xl6CgenXValueCompareExpr(ctx, node);
            break;
        }
        if ( xl6CTextEquals(node->text, "**") ) {
            xl6CgenPowExpr(ctx, node);
            break;
        }
        if ( xl6CgenBinaryArithmeticIsXValue(ctx, node) ) {
            xl6CgenXValueBinaryExpr(ctx, node);
            break;
        }
        if ( xl6CgenCheckedIntegerDivisionExpr(ctx, node) ) {
            break;
        }
        if ( xl6CgenCheckedIntegerShiftExpr(ctx, node) ) {
            break;
        }
        if ( xl6CTextEquals(node->text, "in") ) {
            xl6CgenInExpr(ctx, node);
            break;
        }
        if ( xl6CTextEquals(node->text, "??") ) {
            xl6CgenNullCoalesceExpr(ctx, node);
            break;
        }
        xl6OutputPutc('(', ctx->out);
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        if ( xl6CTextEquals(node->text, "and") ) {
            xl6OutputPuts(" && ", ctx->out);
        } else if ( xl6CTextEquals(node->text, "or") ) {
            xl6OutputPuts(" || ", ctx->out);
        } else {
            xl6OutputPrintf(ctx->out, " %s ", node->text != NULL ? node->text : "?");
        }
        if ( node->childCount > 1 ) {
            xl6CgenExpr(ctx, node->children[1]);
        }
        xl6OutputPutc(')', ctx->out);
        break;
    case XL6_AST_UNARY:
        if ( xl6CgenClassProtocolUnaryExpr(ctx, node) ) {
            break;
        }
        if ( xl6CgenUnaryIsXValue(ctx, node) ) {
            if ( xl6CTextEquals(node->text, "not") || xl6CTextEquals(node->text, "!") ) {
                xl6CgenXValueNotExpr(ctx, node);
            } else {
                xl6CgenXValueUnaryExpr(ctx, node);
            }
            break;
        }
        if ( xl6CTextEquals(node->text, "not") ) {
            xl6OutputPuts("(!", ctx->out);
        } else {
            xl6OutputPrintf(ctx->out, "(%s", node->text != NULL ? node->text : "!");
        }
        if ( node->childCount > 0 ) {
            xl6CgenExpr(ctx, node->children[0]);
        }
        xl6OutputPutc(')', ctx->out);
        break;
    case XL6_AST_CONDITIONAL:
        if ( xl6CgenConditionalIsXValue(ctx, node) ) {
            xl6CgenXValueConditionalExpr(ctx, node);
            break;
        }
        xl6OutputPutc('(', ctx->out);
        if ( node->childCount > 0 ) {
            xl6CgenConditionExpr(ctx, node->children[0]);
        }
        xl6OutputPuts(" ? ", ctx->out);
        if ( node->childCount > 1 ) {
            xl6CgenExpr(ctx, node->children[1]);
        }
        xl6OutputPuts(" : ", ctx->out);
        if ( node->childCount > 2 ) {
            xl6CgenExpr(ctx, node->children[2]);
        }
        xl6OutputPutc(')', ctx->out);
        break;
    case XL6_AST_MATCH:
        xl6CgenMatchExpr(ctx, node);
        break;
    case XL6_AST_CALL:
        if ( node->childCount > 0 ) {
            const XL6_AstNode* callee = node->children[0];
            XL6_AstNode* knownFunc = NULL;
            int copyExternString = 0;
            if ( callee->kind == XL6_AST_MEMBER ) {
                if ( xl6CgenTaggedEnumCtorExpr(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenWeakLockCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenPersistenceStdlibCall(ctx, node) ) {
                    ;
                } else if ( xl6CgenStaticClassMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenUserProtocolCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenIsImplicitToStringCall(ctx, node) ) {
                    /* toString 是隐性类协议语法，优先于 var 的动态函数字段调用。 */
                    xl6CgenImplicitToStringCall(ctx, node, callee);
                } else if ( xl6CgenMemberFunctionInvoke(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenTaskNamespaceCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenThreadMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenSyncMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenSpecialCollectionMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenChannelMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenFutureMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenContainerMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( xl6CgenNativeClassMethodCall(ctx, node, callee) ) {
                    ;
                } else if ( !xl6CgenClassMethodCall(ctx, node, callee) ) {
                    xl6CgenError(ctx, callee, "找不到 class 方法");
                    xl6OutputPuts("0", ctx->out);
                }
                break;
            }
            if ( callee->kind != XL6_AST_NAME &&
                 callee->kind != XL6_AST_GENERIC_INSTANT &&
                 xl6CgenClassProtocolCallExpr(ctx, node) ) {
                break;
            }
            if ( xl6CgenBaseCtorCall(ctx, node) ) {
                break;
            }
            if ( xl6CgenThreadCtorCall(ctx, node, callee) ) {
                break;
            }
            if ( xl6CgenSyncCtorCall(ctx, node, callee) ) {
                break;
            }
            if ( xl6CgenClassConstructorExpr(ctx, node) ) {
                break;
            }
            /*
                显式泛型调用已经由 HIR 解析为静态实例，必须先进入下方泛型直调。
                它本身也携带完整 func 类型，但不能因此误降级成动态函数对象调用。
            */
            if ( callee->kind != XL6_AST_NAME &&
                 callee->kind != XL6_AST_GENERIC_INSTANT ) {
                XL6_AstNode* calleeType = xl6CgenCloneExprTypeForInference(ctx, callee);
                if ( xl6CgenIsFunctionType(calleeType) ) {
                    xl6CgenFunctionInvokeExpr(ctx, callee, calleeType, node->childCount > 1 ? node->children[1] : NULL);
                    xl6AstFree(calleeType);
                    break;
                }
                if ( calleeType != NULL && xl6CTextEquals(calleeType->text, "var") ) {
                    xl6CgenVarFunctionInvokeExpr(ctx, callee, node->childCount > 1 ? node->children[1] : NULL);
                    xl6AstFree(calleeType);
                    break;
                }
                xl6AstFree(calleeType);
            }
            if ( callee->kind == XL6_AST_GENERIC_INSTANT ) {
                const XL6_AstNode* instantName = callee->childCount > 0 ? callee->children[0] : NULL;
                const XL6_AstNode* typeArgs = callee->childCount > 1 ? callee->children[1] : NULL;
                const XL6_AstNode* prevParams = ctx->currentGenericParams;
                const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                if ( xl6CgenSpecialCollectionCtorCall(ctx, node, callee) ) {
                    break;
                }
                if ( xl6CgenChannelCtorCall(ctx, node, callee) ) {
                    break;
                }
                if ( xl6CgenPersistenceGenericCall(ctx, node) ) {
                    break;
                }

                if ( instantName == NULL || instantName->kind != XL6_AST_NAME ) {
                    xl6CgenError(ctx, callee, "当前 C 生成只支持函数名形式的泛型实例化");
                    xl6OutputPuts("0", ctx->out);
                    break;
                }

                knownFunc = xl6CgenResolveExplicitGenericCallFunc(ctx, node, instantName->text, typeArgs, 1);
                if ( knownFunc == NULL ) {
                    xl6CgenError(ctx, callee, "找不到匹配的泛型函数声明");
                    xl6OutputPuts("0", ctx->out);
                    break;
                }

                xl6CgenAddGenericInstance(ctx, knownFunc, typeArgs);
                ctx->currentGenericParams = xl6CgenFuncTypeParams(knownFunc);
                ctx->currentGenericArgs = typeArgs;
                if ( knownFunc->kind == XL6_AST_EXPORT_FUNC ) {
                    xl6CgenExportInternalSymbol(ctx, knownFunc);
                } else {
                    xl6CgenFuncSymbol(ctx, knownFunc);
                }
                xl6OutputPutc('(', ctx->out);
                if ( node->childCount > 1 ) {
                    xl6CgenKnownCallArgs(ctx, knownFunc, node->children[1]);
                }
                xl6OutputPutc(')', ctx->out);
                ctx->currentGenericParams = prevParams;
                ctx->currentGenericArgs = prevArgs;
                break;
            }
            if ( callee->kind == XL6_AST_NAME ) {
                if ( xl6CTextEquals(callee->text, "xcall") ) {
                    const XL6_AstNode* args = node->childCount > 1 ? node->children[1] : NULL;
                    const XL6_AstNode* arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;

                    /* 语义阶段已经验证签名；这里仅生成唯一的显式装箱路径。 */
                    xl6CgenXValueFromExpr(
                        ctx,
                        arg != NULL && arg->kind == XL6_AST_ARG && arg->childCount > 0 ?
                            arg->children[0] : NULL);
                    break;
                }
				const XL6_HirNode* callPlan = xl6CgenHirNode(ctx, node);
				const XL6_AstNode* resolvedTarget;
				XL6_AstNode* externFunc;
				XL6_AstNode* exportFunc;
				int isResolvedDirectCall = callPlan != NULL &&
					(callPlan->callKind == XL6_SEM_CALL_DIRECT ||
					 callPlan->callKind == XL6_SEM_CALL_GENERIC_DIRECT);
				const XL6_AstNode* functionVarType = isResolvedDirectCall ?
					NULL : xl6CgenFindVarType(ctx, callee->text);

				/*
					typed HIR 已完成重载选择。extern/export 直调必须使用
					该目标，不能再按名称取第一个声明。
				*/
				resolvedTarget = isResolvedDirectCall ? xl6CgenHirTarget(ctx, node) : NULL;
				externFunc = resolvedTarget != NULL &&
					resolvedTarget->kind == XL6_AST_EXTERN_FUNC ?
					(XL6_AstNode*)resolvedTarget :
					(!isResolvedDirectCall ? xl6CgenFindExternFunc(ctx, callee->text) : NULL);
				exportFunc = resolvedTarget != NULL &&
					resolvedTarget->kind == XL6_AST_EXPORT_FUNC ?
					(XL6_AstNode*)resolvedTarget :
					(!isResolvedDirectCall ? xl6CgenFindExportFunc(ctx, callee->text) : NULL);
                if ( xl6CTextEquals(callee->text, "type") ) {
                    const XL6_AstNode* args = node->childCount > 1 ? node->children[1] : NULL;
                    const XL6_AstNode* arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
                    xl6CgenEmitTypeOfExpr(ctx, arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL);
                    break;
                }
                if ( xl6CgenCopyBuiltinCall(ctx, node) ) {
                    break;
                }
                if ( xl6CgenPersistenceStdlibCall(ctx, node) ) {
                    break;
                }
                if ( xl6CgenIsBuiltinConvertTypeName(callee->text) && xl6CgenIsKnownTypeName(ctx, callee->text) ) {
                    xl6CgenTypeConvertExpr(ctx, node);
                    break;
                }
                if ( functionVarType != NULL && xl6CTextEquals(functionVarType->text, "var") ) {
                    xl6CgenVarFunctionInvoke(ctx, callee->text, node->childCount > 1 ? node->children[1] : NULL);
                    break;
                }
                if ( xl6CgenIsNullableType(functionVarType) ) {
                    XL6_AstNode* narrowedType =
                        xl6CgenCloneExprTypeForInference(ctx, callee);

                    if ( xl6CgenIsFunctionType(narrowedType) ) {
                        xl6CgenFunctionInvokeExpr(
                            ctx,
                            callee,
                            narrowedType,
                            node->childCount > 1 ? node->children[1] : NULL);
                        xl6AstFree(narrowedType);
                        break;
                    }
                    xl6AstFree(narrowedType);
                }
                if ( xl6CgenIsFunctionType(functionVarType) ) {
                    xl6CgenFunctionInvoke(ctx, callee->text, functionVarType, node->childCount > 1 ? node->children[1] : NULL);
                    break;
                }
                /* 已解析的直接函数调用不能被同名局部变量的后端跟踪记录改写。 */
                if ( !isResolvedDirectCall && xl6CgenClassProtocolCallExpr(ctx, node) ) {
                    break;
                }
                if ( externFunc != NULL ) {
                    knownFunc = externFunc;
                    copyExternString = xl6CgenCallableReturnsString(ctx, externFunc);
                    if ( copyExternString ) {
                        xl6OutputPuts("xl6_xrt_external_string_copy(", ctx->out);
                    }
                    if ( xl6CgenExternCallNeedsWrapper(ctx, externFunc) ) {
                        xl6CgenExternCallWrapperSymbol(ctx, externFunc);
                    } else {
                        xl6CgenCStringSymbol(ctx->out, externFunc->text);
                    }
                } else if ( exportFunc != NULL && xl6CgenExportFuncOverloadCount(ctx, callee->text) <= 1 ) {
                    knownFunc = exportFunc;
                    xl6CgenExportInternalSymbol(ctx, exportFunc);
                } else {
                    XL6_AstNode* inferredArgs = NULL;
                    knownFunc = xl6CgenResolveCallFuncWithTypeArgs(ctx, node, callee->text, &inferredArgs);
                    if ( knownFunc != NULL && inferredArgs != NULL ) {
                        const XL6_AstNode* prevParams = ctx->currentGenericParams;
                        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

                        ctx->currentGenericParams = xl6CgenFuncTypeParams(knownFunc);
                        ctx->currentGenericArgs = inferredArgs;
                        if ( knownFunc->kind == XL6_AST_EXPORT_FUNC ) {
                            xl6CgenExportInternalSymbol(ctx, knownFunc);
                        } else {
                            xl6CgenFuncSymbol(ctx, knownFunc);
                        }
                        xl6OutputPutc('(', ctx->out);
                        if ( node->childCount > 1 ) {
                            xl6CgenKnownCallArgs(ctx, knownFunc, node->children[1]);
                        }
                        xl6OutputPutc(')', ctx->out);
                        ctx->currentGenericParams = prevParams;
                        ctx->currentGenericArgs = prevArgs;
                        xl6CgenAddOwnedGenericInstance(ctx, knownFunc, inferredArgs);
                        break;
                    }
                    xl6AstFree(inferredArgs);
                    if ( knownFunc != NULL ) {
                        if ( knownFunc->kind == XL6_AST_EXPORT_FUNC ) {
                            xl6CgenExportInternalSymbol(ctx, knownFunc);
                        } else {
                            xl6CgenFuncSymbol(ctx, knownFunc);
                        }
                    } else {
                        xl6CgenExpr(ctx, callee);
                    }
                }
            } else {
                xl6CgenExpr(ctx, callee);
            }
            xl6OutputPutc('(', ctx->out);
            if ( node->childCount > 1 ) {
                if ( knownFunc != NULL ) {
                    xl6CgenKnownCallArgs(ctx, knownFunc, node->children[1]);
                } else {
                    xl6CgenCallArgs(ctx, node->children[1]);
                }
            }
            xl6OutputPutc(')', ctx->out);
            if ( copyExternString ) {
                xl6OutputPutc(')', ctx->out);
            }
            break;
        }
        xl6OutputPuts("()", ctx->out);
        break;
    default:
    {
        char message[160];

        snprintf(message, sizeof(message), "C generator does not support expression node: %s", xl6AstKindName(node->kind));
        xl6CgenError(ctx, node, message);
        xl6OutputPuts("0", ctx->out);
        break;
    }
    }
}

static int xl6CgenTypeNodeApproxKind(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int8") ||
         xl6CTextEquals(typeNode->text, "uint8") || xl6CTextEquals(typeNode->text, "int16") ||
         xl6CTextEquals(typeNode->text, "uint16") || xl6CTextEquals(typeNode->text, "int32") ||
         xl6CTextEquals(typeNode->text, "uint32") || xl6CTextEquals(typeNode->text, "int64") ||
         xl6CTextEquals(typeNode->text, "uint64") ) {
        return 1;
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float32") ||
         xl6CTextEquals(typeNode->text, "float64") ) {
        return 2;
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return 3;
    }
    if ( xl6CTextEquals(typeNode->text, "time") ) {
        return 4;
    }
    return 0;
}

static int xl6CgenExprApproxKind(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return 1;
    case XL6_AST_FLOAT:
        return 2;
    case XL6_AST_STRING:
        return 3;
    case XL6_AST_TIME:
        return 4;
    case XL6_AST_NAME:
        return xl6CgenTypeNodeApproxKind(ctx, xl6CgenFindVarType(ctx, expr->text));
    case XL6_AST_MEMBER: {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
        int kind = xl6CgenTypeNodeApproxKind(ctx, memberType);
        xl6AstFree(memberType);
        return kind;
    }
    case XL6_AST_CALL: {
        XL6_AstNode* callType = xl6CgenCloneExprTypeForInference(ctx, expr);
        int kind = xl6CgenTypeNodeApproxKind(ctx, callType);
        xl6AstFree(callType);
        return kind;
    }
    case XL6_AST_UNARY:
        if ( xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ) {
            return expr->childCount > 0 ? xl6CgenExprApproxKind(ctx, expr->children[0]) : 0;
        }
        return 0;
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ||
             xl6CTextEquals(expr->text, "*") || xl6CTextEquals(expr->text, "/") ||
             xl6CTextEquals(expr->text, "%") || xl6CTextEquals(expr->text, "**") ) {
            int leftKind;
            int rightKind;
            if ( expr->childCount < 2 ) {
                return 0;
            }
            leftKind = xl6CgenExprApproxKind(ctx, expr->children[0]);
            rightKind = xl6CgenExprApproxKind(ctx, expr->children[1]);
            if ( leftKind == 2 || rightKind == 2 ) {
                return 2;
            }
            if ( leftKind == 1 && rightKind == 1 ) {
                return 1;
            }
            return 0;
        }
        return 0;
    default:
        return 0;
    }
}

static int xl6CgenExprIntPowKind(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    switch ( expr->kind ) {
    case XL6_AST_INT:
        return 1;
    case XL6_AST_NAME:
        return xl6CgenTypeNodeApproxKind(ctx, xl6CgenFindVarType(ctx, expr->text)) == 1;
    case XL6_AST_MEMBER: {
        XL6_AstNode* memberType = xl6CgenCloneMemberFieldTypeForInference(ctx, expr);
        int isInt = xl6CgenTypeNodeApproxKind(ctx, memberType) == 1;
        xl6AstFree(memberType);
        return isInt;
    }
    case XL6_AST_CALL: {
        XL6_AstNode* callType = xl6CgenCloneExprTypeForInference(ctx, expr);
        int isInt = xl6CgenTypeNodeApproxKind(ctx, callType) == 1;
        xl6AstFree(callType);
        return isInt;
    }
    case XL6_AST_UNARY:
        if ( xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ||
             xl6CTextEquals(expr->text, "~") ) {
            return expr->childCount > 0 && xl6CgenExprIntPowKind(ctx, expr->children[0]);
        }
        return 0;
    case XL6_AST_BINARY:
        if ( xl6CTextEquals(expr->text, "+") || xl6CTextEquals(expr->text, "-") ||
             xl6CTextEquals(expr->text, "*") || xl6CTextEquals(expr->text, "/") ||
             xl6CTextEquals(expr->text, "%") || xl6CTextEquals(expr->text, "**") ||
             xl6CTextEquals(expr->text, "<<") || xl6CTextEquals(expr->text, ">>") ||
             xl6CTextEquals(expr->text, "&") || xl6CTextEquals(expr->text, "|") ||
             xl6CTextEquals(expr->text, "^") ) {
            return expr->childCount > 1 &&
                   xl6CgenExprIntPowKind(ctx, expr->children[0]) &&
                   xl6CgenExprIntPowKind(ctx, expr->children[1]);
        }
        return 0;
    default:
        return 0;
    }
}

static int xl6CgenAnonCaptureIndex(const XL6_CgenAnonFunc* info, const char* name)
{
    size_t i;

    if ( info == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( xl6CTextEquals(info->captures[i].name, name) ) {
            return (int)i;
        }
    }
    return -1;
}

static void xl6CgenRegisterAnonCaptureTypes(XL6_CgenContext* ctx, const XL6_CgenAnonFunc* info)
{
    size_t i;

    if ( ctx == NULL || info == NULL ) {
        return;
    }
    for ( i = 0; i < info->captureCount; ++i ) {
        if ( xl6CgenParamTypeUsesTypedContainerStorage(ctx, info->captures[i].type) ) {
            xl6CgenAddVarTypeWithFlags(ctx,
                                       info->captures[i].name,
                                       info->captures[i].type,
                                       XL6_CGEN_VAR_TYPED_CONTAINER | XL6_CGEN_VAR_BORROWED);
        } else {
            xl6CgenAddVarType(ctx, info->captures[i].name, info->captures[i].type);
        }
    }
}

static void xl6CgenPowExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( !xl6CgenExprIntPowKind(ctx, left) || !xl6CgenExprIntPowKind(ctx, right) ) {
        xl6CgenError(ctx, node, "当前 C 生成 ** 只支持整数表达式");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_int_pow_i64(", ctx->out);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, right);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenStringConcatOperandExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( xl6CgenBinaryArithmeticIsXValue(ctx, node) ) {
        xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
        xl6CgenExpr(ctx, node);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenExpr(ctx, node);
}

static void xl6CgenStringConcatExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    xl6OutputPuts("xl6_xrt_str_concat(", ctx->out);
    xl6CgenStringConcatOperandExpr(ctx, left);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenStringConcatOperandExpr(ctx, right);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenStringCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    const char* op;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    op = node != NULL && node->text != NULL ? node->text : "==";

    xl6OutputPuts("(xrtStrCompare(xrtStrView(", ctx->out);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts("), xrtStrView(", ctx->out);
    xl6CgenExpr(ctx, right);
    xl6OutputPuts(")) ", ctx->out);
    xl6OutputPuts(op, ctx->out);
    xl6OutputPuts(" 0)", ctx->out);
}

static void xl6CgenApproxExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int leftKind;
    int rightKind;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    leftKind = xl6CgenExprApproxKind(ctx, left);
    rightKind = xl6CgenExprApproxKind(ctx, right);

    if ( leftKind == 3 && rightKind == 3 ) {
        xl6OutputPuts("(xrtUtf8Similarity(xrtStrView(", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts("), xrtStrView(", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(")) >= 0.95)", ctx->out);
        return;
    }

    if ( leftKind == 4 && rightKind == 4 ) {
        xl6OutputPuts("xl6_xrt_time_approx(", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( leftKind == 0 || rightKind == 0 || leftKind == 3 || rightKind == 3 ||
         leftKind == 4 || rightKind == 4 ) {
        xl6CgenError(ctx, node, "当前 C 生成 ~= 只支持整数、浮点、字符串和时间表达式");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts((leftKind == 2 || rightKind == 2)
        ? "xl6_xrt_num_approx("
        : "xl6_xrt_int_approx(", ctx->out);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, right);
    xl6OutputPutc(')', ctx->out);
}
