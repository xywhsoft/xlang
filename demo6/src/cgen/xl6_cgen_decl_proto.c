static void xl6CgenFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* nameNode;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( xl6CgenFuncIsMainEntry(node) ) {
        /* xlang main 固定暴露为运行时入口，便于 xl.dll 绕过 C main 直接调用。 */
        xl6OutputPuts("int", ctx->out);
    } else if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructName(ctx, node);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenFuncSingleReturnCType(ctx, node), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    nameNode = node->childCount > 0 ? node->children[0] : NULL;
    (void)nameNode;
    xl6OutputPutc(' ', ctx->out);
    xl6CgenFuncSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( param->childCount == 0 ) {
                xl6CgenError(ctx, param, "函数参数缺少类型");
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenParamCType(ctx, param->children[0]), ctx->out);
                xl6OutputPutc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenExportInternalProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructName(ctx, node);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenFuncSingleReturnCType(ctx, node), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    xl6OutputPutc(' ', ctx->out);
    xl6CgenExportInternalSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CgenParamIsCVararg(param) ) {
                xl6CgenError(ctx, param, "当前 C 生成暂不支持可变参数 export 函数");
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenParamCType(ctx, param->children[0]), ctx->out);
                xl6OutputPutc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenExportWrapperProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    xl6OutputPutc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node->text);
    xl6OutputPutc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( (params == NULL || params->childCount == 0) && (returns == NULL || returns->childCount <= 1) ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        int wroteParam = 0;

        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                const XL6_AstNode* param = params->children[i];
                if ( wroteParam ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                if ( param->childCount == 0 || xl6CgenParamIsCVararg(param) ) {
                    xl6CgenError(ctx, param, "当前 C 生成暂不支持可变参数 export wrapper");
                    xl6OutputPuts("int ", ctx->out);
                } else {
                    xl6OutputPuts(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                    if ( (param->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ) {
                        xl6OutputPutc('*', ctx->out);
                    }
                    xl6OutputPutc(' ', ctx->out);
                }
                xl6CgenFputsCIdent(ctx->out, param->text);
                wroteParam = 1;
            }
        }
        if ( returns != NULL && returns->childCount > 1 ) {
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( wroteParam ) {
                    xl6OutputPuts(", ", ctx->out);
                }
                xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[i]), ctx->out);
                xl6OutputPrintf(ctx->out, "* out%u", (unsigned)i);
                wroteParam = 1;
            }
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenClassExportWrapperProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    int wroteParam;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    xl6OutputPutc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node->text);
    xl6OutputPutc('(', ctx->out);

    xl6CgenClassTypeName(ctx->out, classNode != NULL ? classNode->text : "_");
    xl6OutputPuts("* self", ctx->out);
    wroteParam = 1;

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( wroteParam ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CgenParamIsCVararg(param) ) {
                xl6CgenError(ctx, param, "class C ABI export wrapper does not support vararg parameters");
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                if ( (param->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ) {
                    xl6OutputPutc('*', ctx->out);
                }
                xl6OutputPutc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
            wroteParam = 1;
        }
    }
    if ( returns != NULL && returns->childCount > 1 ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( wroteParam ) {
                xl6OutputPuts(", ", ctx->out);
            }
            xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[i]), ctx->out);
            xl6OutputPrintf(ctx->out, "* out%u", (unsigned)i);
            wroteParam = 1;
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenExternFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "当前 C 生成暂不支持多返回值 extern 函数");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }

    xl6OutputPutc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, node->text);
    xl6OutputPutc('(', ctx->out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( xl6CgenParamIsCVararg(param) ) {
                xl6OutputPuts("...", ctx->out);
                continue;
            }
            if ( param->childCount == 0 ) {
                xl6CgenError(ctx, param, "当前 C 生成暂不支持可变参数 extern 函数");
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                xl6OutputPutc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenExternVarDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenExternalVarTypeNode(node);
    xl6OutputPuts("extern ", ctx->out);
    xl6OutputPuts(xl6CgenCAbiTypeName(ctx, typeNode), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenExternalVarStorageName(ctx->out, node);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenExportVarDef(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;
    const XL6_AstNode* initExpr;

    typeNode = xl6CgenExternalVarTypeNode(node);
    initExpr = xl6CgenExternalVarInitExpr(node);
    if ( node != NULL && node->kind == XL6_AST_EXPORT_CONST ) {
        xl6OutputPuts("const ", ctx->out);
    }
    xl6OutputPuts(xl6CgenCAbiTypeName(ctx, typeNode), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenExternalVarStorageName(ctx->out, node);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenExpr(ctx, initExpr);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenClassParamListEx(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* params, int includeSelf)
{
    size_t i;
    int wroteParam;

    wroteParam = 0;
    if ( includeSelf ) {
        xl6CgenClassTypeName(ctx->out, classNode->text);
        xl6OutputPuts("* self", ctx->out);
        wroteParam = 1;
    }
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( wroteParam ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "class C 生成暂不支持可变参数方法");
            xl6OutputPuts("int ", ctx->out);
        } else {
            xl6OutputPuts(xl6CgenParamCType(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
        wroteParam = 1;
    }
}

static void xl6CgenClassParamList(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* params)
{
    xl6CgenClassParamListEx(ctx, classNode, params, 1);
}

static void xl6CgenClassFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    nameNode = funcNode->childCount > 0 ? funcNode->children[0] : NULL;
    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructDecl(ctx, funcNode);
        xl6CgenFuncReturnStructName(ctx, funcNode);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenFuncSingleReturnCType(ctx, funcNode), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenClassCallableSymbol(ctx, classNode, funcNode);
    xl6OutputPutc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenClassParamListEx(ctx, classNode, params, !xl6CgenIsStaticMember(funcNode));
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenClassSpecialProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* params;
    const char* name;

    xl6OutputPuts("void ", ctx->out);
    name = xl6CgenCallableName(specialNode);
    if ( xl6CTextEquals(name, "create") ) {
        xl6CgenClassCallableSymbol(ctx, classNode, specialNode);
    } else {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, name != NULL ? name : "_");
    }
    xl6OutputPutc('(', ctx->out);
    params = xl6CgenCallableParams(specialNode);
    xl6CgenClassParamList(ctx, classNode, params);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenClassDestroyProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6OutputPuts("void ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self)", ctx->out);
}

static void xl6CgenClassAutoCopyProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* protocolName)
{
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, protocolName);
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self)", ctx->out);
}

static void xl6CgenEmitClassBaseTypeDesc(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    char cTypeName[512];

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME || base->text == NULL ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), base);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
}

static int xl6CgenClassHasBaseType(const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    return base != NULL && base->kind == XL6_AST_NAME && base->text != NULL;
}

static void xl6CgenEmitRecordBoxRuntime(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const char* cTypeName,
    const char* sourceName)
{
    int needsDestroy;
    int isGeneric;
    int canCopy;
    int canClone;
    int hasValueEquality;
    const XL6_AstNode* copyMethod;
    const XL6_AstNode* cloneMethod;
    const XL6_AstNode* hashMethod;
    const XL6_AstNode* equalsMethod;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    if ( ctx == NULL || cTypeName == NULL || sourceName == NULL ) {
        return;
    }
    isGeneric = typeNode != NULL && typeNode->childCount > 0 && xl6CgenClassHasTypeParams(classNode);
    needsDestroy = isGeneric ?
        xl6CgenGenericClassNeedsDestroy(ctx, classNode, typeNode) :
        xl6CgenClassNeedsDestroy(ctx, classNode);
    copyMethod = xl6CgenFindClassFunc(classNode, "__copy__");
    cloneMethod = xl6CgenFindClassFunc(classNode, "__clone__");
    hashMethod = xl6CgenFindClassFunc(classNode, "__hash__");
    equalsMethod = xl6CgenFindClassFunc(classNode, "__eq__");
    canCopy = classNode != NULL &&
              (xl6CgenClassHasDerive(classNode, "copy") || copyMethod != NULL);
    canClone = classNode != NULL &&
               (xl6CgenClassHasDerive(classNode, "clone") || cloneMethod != NULL);
    hasValueEquality = classNode != NULL &&
                       (xl6CgenClassHasDerive(classNode, "hash") || hashMethod != NULL) &&
                       (xl6CgenClassHasDerive(classNode, "equals") || equalsMethod != NULL);
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( isGeneric ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = typeNode->children[0];
    }

    /* 类型操作先于泛型方法声明输出，用户协议需要在这里提供准确原型。 */
    if ( isGeneric && copyMethod != NULL ) {
        xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, copyMethod);
        xl6OutputPuts(";\n", ctx->out);
    }
    if ( isGeneric && cloneMethod != NULL ) {
        xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, cloneMethod);
        xl6OutputPuts(";\n", ctx->out);
    }

    if ( canCopy ) {
        xl6OutputPrintf(ctx->out, "static void xl6_type_copy_%s(void* dst, const void* src)\n{\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    if ( dst == 0 ) {\n");
        xl6OutputPrintf(ctx->out, "        return;\n");
        xl6OutputPrintf(ctx->out, "    }\n");
        xl6OutputPrintf(ctx->out, "    if ( src != 0 ) {\n");
        if ( copyMethod != NULL ) {
            xl6OutputPuts("        xvalue protocolValue = ", ctx->out);
            if ( isGeneric ) {
                xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, copyMethod);
            } else {
                xl6CgenClassCallableSymbol(ctx, classNode, copyMethod);
            }
            xl6OutputPuts("((", ctx->out);
            xl6OutputPuts(cTypeName, ctx->out);
            xl6OutputPuts("*)src);\n", ctx->out);
            xl6OutputPrintf(ctx->out,
                            "        const %s* protocolData = protocolValue != 0 ? (const %s*)xvoGetRecordData(protocolValue) : 0;\n",
                            cTypeName,
                            cTypeName);
            xl6OutputPuts("        if ( protocolData != 0 ) {\n", ctx->out);
            xl6OutputPrintf(ctx->out, "            memcpy(dst, protocolData, sizeof(%s));\n", cTypeName);
        } else {
            xl6OutputPrintf(ctx->out, "        memcpy(dst, src, sizeof(%s));\n", cTypeName);
        }
        if ( needsDestroy ) {
            char dstPath[512];
            int prevIndent = ctx->indent;

            snprintf(dstPath, sizeof(dstPath), "(*(%s*)dst)", cTypeName);
            ctx->indent = copyMethod != NULL ? 3 : 2;
            xl6CgenRetainClassFunctionFields(ctx, classNode, dstPath);
            ctx->indent = prevIndent;
        }
        if ( copyMethod != NULL ) {
            xl6OutputPuts("        } else {\n", ctx->out);
            xl6OutputPrintf(ctx->out, "            memset(dst, 0, sizeof(%s));\n", cTypeName);
            xl6OutputPuts("        }\n", ctx->out);
            xl6OutputPuts("        if ( protocolValue != 0 ) { xvoUnref(protocolValue); }\n", ctx->out);
        }
        xl6OutputPrintf(ctx->out, "    } else {\n");
        xl6OutputPrintf(ctx->out, "        memset(dst, 0, sizeof(%s));\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    }\n");
        xl6OutputPrintf(ctx->out, "}\n\n");
    }

    if ( canClone ) {
        xl6OutputPrintf(ctx->out, "static void xl6_type_clone_%s(void* dst, const void* src)\n{\n", cTypeName);
        xl6OutputPuts("    if ( dst == 0 ) {\n        return;\n    }\n", ctx->out);
        xl6OutputPuts("    if ( src != 0 ) {\n", ctx->out);
        if ( cloneMethod != NULL ) {
            xl6OutputPuts("        xvalue protocolValue = ", ctx->out);
            if ( isGeneric ) {
                xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, cloneMethod);
            } else {
                xl6CgenClassCallableSymbol(ctx, classNode, cloneMethod);
            }
            xl6OutputPuts("((", ctx->out);
            xl6OutputPuts(cTypeName, ctx->out);
            xl6OutputPuts("*)src);\n", ctx->out);
            xl6OutputPrintf(ctx->out,
                            "        const %s* protocolData = protocolValue != 0 ? (const %s*)xvoGetRecordData(protocolValue) : 0;\n",
                            cTypeName,
                            cTypeName);
            xl6OutputPuts("        if ( protocolData != 0 ) {\n", ctx->out);
            xl6OutputPrintf(ctx->out, "            memcpy(dst, protocolData, sizeof(%s));\n", cTypeName);
            if ( needsDestroy ) {
                char dstPath[512];
                int prevIndent = ctx->indent;

                snprintf(dstPath, sizeof(dstPath), "(*(%s*)dst)", cTypeName);
                ctx->indent = 3;
                xl6CgenRetainClassFunctionFields(ctx, classNode, dstPath);
                ctx->indent = prevIndent;
            }
            xl6OutputPuts("        } else {\n", ctx->out);
            xl6OutputPrintf(ctx->out, "            memset(dst, 0, sizeof(%s));\n", cTypeName);
            xl6OutputPuts("        }\n", ctx->out);
            xl6OutputPuts("        if ( protocolValue != 0 ) { xvoUnref(protocolValue); }\n", ctx->out);
        } else {
            char dstPath[512];
            int prevIndent = ctx->indent;

            xl6OutputPrintf(ctx->out, "        memcpy(dst, src, sizeof(%s));\n", cTypeName);
            snprintf(dstPath, sizeof(dstPath), "(*(%s*)dst)", cTypeName);
            ctx->indent = 2;
            xl6CgenDeepCopyClassFunctionFields(ctx, classNode, dstPath);
            ctx->indent = prevIndent;
        }
        xl6OutputPuts("    } else {\n        memset(dst, 0, sizeof(", ctx->out);
        xl6OutputPuts(cTypeName, ctx->out);
        xl6OutputPuts("));\n    }\n}\n\n", ctx->out);
    }

    if ( needsDestroy ) {
        xl6OutputPuts("void ", ctx->out);
        if ( isGeneric ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, typeNode, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "(%s* self);\n", cTypeName);
        xl6OutputPrintf(ctx->out, "static void xl6_type_drop_%s(void* obj)\n{\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    if ( obj != 0 ) {\n");
        xl6OutputPrintf(ctx->out, "        ");
        if ( isGeneric ) {
            xl6CgenGenericClassMemberSymbol(ctx, classNode, typeNode, "destroy");
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        }
        xl6OutputPrintf(ctx->out, "((%s*)obj);\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    }\n");
        xl6OutputPrintf(ctx->out, "}\n\n");
    }

    xl6OutputPrintf(ctx->out, "static int xl6_type_compare_%s(const void* a, const void* b)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    if ( a == b ) {\n");
    xl6OutputPrintf(ctx->out, "        return 0;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    if ( a == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return -1;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    if ( b == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return 1;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    if ( hasValueEquality ) {
        xl6OutputPuts("    if ( ", ctx->out);
        if ( equalsMethod != NULL ) {
            if ( isGeneric ) {
                xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, equalsMethod);
            } else {
                xl6CgenClassCallableSymbol(ctx, classNode, equalsMethod);
            }
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__eq__");
        }
        xl6OutputPrintf(ctx->out, "((%s*)a, *(const %s*)b) ) {\n", cTypeName, cTypeName);
        xl6OutputPuts("        return 0;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
        xl6OutputPrintf(ctx->out, "    { int raw = memcmp(a, b, sizeof(%s)); return raw != 0 ? raw : 1; }\n", cTypeName);
    } else {
        xl6OutputPrintf(ctx->out, "    return memcmp(a, b, sizeof(%s));\n", cTypeName);
    }
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static unsigned long long xl6_type_hash_%s(const void* obj)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    if ( obj == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return 0;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    if ( hasValueEquality ) {
        xl6OutputPuts("    return (unsigned long long)", ctx->out);
        if ( hashMethod != NULL ) {
            if ( isGeneric ) {
                xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, hashMethod);
            } else {
                xl6CgenClassCallableSymbol(ctx, classNode, hashMethod);
            }
        } else {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__hash__");
        }
        xl6OutputPrintf(ctx->out, "((%s*)obj);\n", cTypeName);
    } else {
        xl6OutputPrintf(ctx->out, "    return xrtHash64((const char*)obj, sizeof(%s));\n", cTypeName);
    }
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static xvalue xl6_type_box_%s(const void* obj, const xrt_type_desc* type)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xvalue boxed;\n");
    xl6OutputPrintf(ctx->out, "    if ( type == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return xvoCreateNull();\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    boxed = xvoCreateRecord(type, obj);\n");
    xl6OutputPrintf(ctx->out, "    return boxed != 0 ? boxed : xvoCreateNull();\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static bool xl6_type_unbox_%s(xvalue value, void* out, const xrt_type_desc* type)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    const xrt_type_desc* actual;\n");
    xl6OutputPrintf(ctx->out, "    void* data;\n");
    xl6OutputPrintf(ctx->out, "    if ( value == 0 || out == 0 || type == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    actual = xvoTypeDesc(value);\n");
    xl6OutputPrintf(ctx->out, "    if ( actual != type ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    data = xvoGetRecordData(value);\n");
    xl6OutputPrintf(ctx->out, "    if ( data == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    if ( canCopy ) {
        xl6OutputPrintf(ctx->out, "    xl6_type_copy_%s(out, data);\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    return true;\n");
    } else {
        xl6OutputPuts("    return false;\n", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static const xrt_type_ops xl6_type_ops_%s = {\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    if ( canCopy ) {
        xl6OutputPrintf(ctx->out, "    xl6_type_copy_%s,\n", cTypeName);
    } else {
        xl6OutputPuts("    0,\n", ctx->out);
    }
    xl6OutputPuts("    0,\n", ctx->out);
    if ( needsDestroy ) {
        xl6OutputPrintf(ctx->out, "    xl6_type_drop_%s,\n", cTypeName);
    } else {
        xl6OutputPuts("    0,\n", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "    xl6_type_compare_%s,\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_type_hash_%s,\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    xl6_type_box_%s,\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_type_unbox_%s,\n", cTypeName);
    if ( canClone ) {
        xl6OutputPrintf(ctx->out, "    xl6_type_clone_%s\n", cTypeName);
    } else {
        xl6OutputPuts("    0\n", ctx->out);
    }
    xl6OutputPuts("};\n\n", ctx->out);

    if ( hasValueEquality ) {
        xl6OutputPuts("#ifndef XL6_CLASS_TYPE_EXTRA_DEFINED\n", ctx->out);
        xl6OutputPuts("#define XL6_CLASS_TYPE_EXTRA_DEFINED\n", ctx->out);
        xl6OutputPuts("#define XL6_CLASS_TYPE_FLAG_VALUE_EQUALITY 0x00000001u\n", ctx->out);
        xl6OutputPuts("#define XL6_CLASS_TYPE_EXTRA_MAGIC 0x584C3643u\n", ctx->out);
        xl6OutputPuts("typedef struct xl6_class_type_extra { unsigned int magic; unsigned int flags; } xl6_class_type_extra;\n", ctx->out);
        xl6OutputPuts("#endif\n", ctx->out);
        xl6OutputPrintf(ctx->out,
                        "static const xl6_class_type_extra xl6_type_extra_%s = { XL6_CLASS_TYPE_EXTRA_MAGIC, XL6_CLASS_TYPE_FLAG_VALUE_EQUALITY };\n\n",
                        cTypeName);
    }

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    xl6OutputPuts("    static xrt_type_desc desc = {\n", ctx->out);
    xl6OutputPuts("        0u,\n", ctx->out);
    xl6OutputPuts("        XL6_XRT_TYPE_KIND_CLASS,\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, sourceName);
    xl6OutputPrintf(ctx->out, ", %uu,\n", (unsigned)strlen(sourceName));
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, ", %uu,\n", (unsigned)strlen(cTypeName));
    xl6OutputPrintf(ctx->out, "        sizeof(%s), sizeof(void*),\n", cTypeName);
    xl6OutputPrintf(ctx->out, "        &xl6_type_ops_%s, 0, ", cTypeName);
    if ( hasValueEquality ) {
        xl6OutputPrintf(ctx->out, "&xl6_type_extra_%s", cTypeName);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPuts(", 0\n", ctx->out);
    xl6OutputPuts("    };\n", ctx->out);
    if ( xl6CgenClassHasBaseType(classNode) ) {
        xl6OutputPuts("    if ( desc.BaseType == 0 ) { desc.BaseType = ", ctx->out);
        xl6CgenEmitClassBaseTypeDesc(ctx, classNode);
        xl6OutputPuts("; }\n", ctx->out);
    }
    xl6OutputPuts("    return &desc;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static xvalue ", ctx->out);
    xl6CgenBoxSymbol(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, "(%s value)\n{\n", cTypeName);
    xl6OutputPuts("    xvalue boxed = xvoCreateRecord(", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(), &value);\n", ctx->out);
    xl6OutputPuts("    return boxed != 0 ? boxed : xvoCreateNull();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPrintf(ctx->out, "static %s ", cTypeName);
    xl6CgenDefaultClassSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s value = {0};\n", cTypeName);
    xl6CgenClassDefaultInitFields(ctx, classNode, "value");
    xl6CgenIndent(ctx);
    xl6OutputPuts("return value;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenEmitPlainRecordBoxRuntime(XL6_CgenContext* ctx, const char* cTypeName, const char* sourceName)
{
    if ( ctx == NULL || cTypeName == NULL || sourceName == NULL ) {
        return;
    }

    xl6OutputPrintf(ctx->out, "static void xl6_type_copy_%s(void* dst, const void* src)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    if ( dst == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    if ( src != 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        memcpy(dst, src, sizeof(%s));\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    } else {\n");
    xl6OutputPrintf(ctx->out, "        memset(dst, 0, sizeof(%s));\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static int xl6_type_compare_%s(const void* a, const void* b)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    if ( a == b ) {\n");
    xl6OutputPrintf(ctx->out, "        return 0;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    if ( a == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return -1;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    if ( b == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return 1;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    return memcmp(a, b, sizeof(%s));\n", cTypeName);
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static unsigned long long xl6_type_hash_%s(const void* obj)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    if ( obj == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return 0;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    return xrtHash64((const char*)obj, sizeof(%s));\n", cTypeName);
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static xvalue xl6_type_box_%s(const void* obj, const xrt_type_desc* type)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xvalue boxed;\n");
    xl6OutputPrintf(ctx->out, "    if ( type == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return xvoCreateNull();\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    boxed = xvoCreateRecord(type, obj);\n");
    xl6OutputPrintf(ctx->out, "    return boxed != 0 ? boxed : xvoCreateNull();\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static bool xl6_type_unbox_%s(xvalue value, void* out, const xrt_type_desc* type)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    const xrt_type_desc* actual;\n");
    xl6OutputPrintf(ctx->out, "    void* data;\n");
    xl6OutputPrintf(ctx->out, "    if ( value == 0 || out == 0 || type == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    actual = xvoTypeDesc(value);\n");
    xl6OutputPrintf(ctx->out, "    if ( actual != type ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    data = xvoGetRecordData(value);\n");
    xl6OutputPrintf(ctx->out, "    if ( data == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    xl6_type_copy_%s(out, data);\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    return true;\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static const xrt_type_ops xl6_type_ops_%s = {\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    xl6_type_copy_%s,\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    xl6_type_compare_%s,\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_type_hash_%s,\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    xl6_type_box_%s,\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_type_unbox_%s\n", cTypeName);
    xl6OutputPuts("};\n\n", ctx->out);

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    xl6OutputPuts("    static const xrt_type_desc desc = {\n", ctx->out);
    xl6OutputPuts("        0u,\n", ctx->out);
    xl6OutputPuts("        XL6_XRT_TYPE_KIND_RECORD,\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, sourceName);
    xl6OutputPrintf(ctx->out, ", %uu,\n", (unsigned)strlen(sourceName));
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, ", %uu,\n", (unsigned)strlen(cTypeName));
    xl6OutputPrintf(ctx->out, "        sizeof(%s), sizeof(void*),\n", cTypeName);
    xl6OutputPrintf(ctx->out, "        &xl6_type_ops_%s, 0, 0\n", cTypeName);
    xl6OutputPuts("    };\n", ctx->out);
    xl6OutputPuts("    return &desc;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static xvalue ", ctx->out);
    xl6CgenBoxSymbol(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, "(%s value)\n{\n", cTypeName);
    xl6OutputPuts("    xvalue boxed = xvoCreateRecord(", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(), &value);\n", ctx->out);
    xl6OutputPuts("    return boxed != 0 ? boxed : xvoCreateNull();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitHandleBoxRuntime(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const char* cTypeName, const char* sourceName)
{
    const XL6_AstNode* destroyNode;

    if ( ctx == NULL || cTypeName == NULL || sourceName == NULL ) {
        return;
    }
    if ( nativeClassNode != NULL && nativeClassNode->kind == XL6_AST_CLASS ) {
        destroyNode = xl6CgenFindClassSpecial(nativeClassNode, "destroy");
    } else {
        destroyNode = xl6CgenFindNativeBackedClassSpecial(nativeClassNode, "destroy");
    }

    if ( destroyNode != NULL ) {
        xl6OutputPuts("long long ", ctx->out);
        xl6CgenCStringSymbol(ctx->out, destroyNode->text);
        xl6OutputPuts("(void* self);\n", ctx->out);
        xl6OutputPrintf(ctx->out, "static void xl6_type_drop_%s(void* obj)\n{\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    void** handle = (void**)obj;\n");
        xl6OutputPrintf(ctx->out, "    if ( handle != 0 && *handle != 0 ) {\n");
        xl6OutputPrintf(ctx->out, "        ");
        xl6CgenCStringSymbol(ctx->out, destroyNode->text);
        xl6OutputPrintf(ctx->out, "(*handle);\n");
        xl6OutputPrintf(ctx->out, "        *handle = 0;\n");
        xl6OutputPrintf(ctx->out, "    }\n");
        xl6OutputPrintf(ctx->out, "}\n\n");
    }

    xl6OutputPrintf(ctx->out, "static int xl6_type_compare_%s(const void* a, const void* b)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    uintptr_t left = (uintptr_t)((const %s*)a != 0 ? ((const %s*)a)->handle : 0);\n", cTypeName, cTypeName);
    xl6OutputPrintf(ctx->out, "    uintptr_t right = (uintptr_t)((const %s*)b != 0 ? ((const %s*)b)->handle : 0);\n", cTypeName, cTypeName);
    xl6OutputPrintf(ctx->out, "    if ( left == right ) {\n");
    xl6OutputPrintf(ctx->out, "        return 0;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    return left < right ? -1 : 1;\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static unsigned long long xl6_type_hash_%s(const void* obj)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    void* handle = ((const %s*)obj != 0) ? ((const %s*)obj)->handle : 0;\n", cTypeName, cTypeName);
    xl6OutputPrintf(ctx->out, "    return xrtHash64((const char*)&handle, sizeof(handle));\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static xvalue xl6_type_box_%s(const void* obj, const xrt_type_desc* type)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    const %s* value = (const %s*)obj;\n", cTypeName, cTypeName);
    xl6OutputPrintf(ctx->out, "    void* handle = value != 0 ? value->handle : 0;\n");
    xl6OutputPrintf(ctx->out, "    xvalue boxed;\n");
    xl6OutputPrintf(ctx->out, "    if ( type == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return xvoCreateNull();\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    boxed = xvoCreateHandle(type, handle, XL6_XRT_HANDLE_FLAG_BORROWED | XL6_XRT_HANDLE_FLAG_NULLABLE);\n");
    xl6OutputPrintf(ctx->out, "    return boxed != 0 ? boxed : xvoCreateNull();\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static bool xl6_type_unbox_%s(xvalue value, void* out, const xrt_type_desc* type)\n{\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    const xrt_type_desc* actual;\n");
    xl6OutputPrintf(ctx->out, "    if ( value == 0 || out == 0 || type == 0 ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    actual = xvoTypeDesc(value);\n");
    xl6OutputPrintf(ctx->out, "    if ( actual != type ) {\n");
    xl6OutputPrintf(ctx->out, "        return false;\n");
    xl6OutputPrintf(ctx->out, "    }\n");
    xl6OutputPrintf(ctx->out, "    ((%s*)out)->handle = xvoGetHandleData(value);\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    return true;\n");
    xl6OutputPrintf(ctx->out, "}\n\n");

    xl6OutputPrintf(ctx->out, "static const xrt_type_ops xl6_type_ops_%s = {\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    /* native handle 没有映射复制协议时只能共享，绝不能复制指针并伪造所有权。 */
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPuts("    0,\n", ctx->out);
    if ( destroyNode != NULL ) {
        xl6OutputPrintf(ctx->out, "    xl6_type_drop_%s,\n", cTypeName);
    } else {
        xl6OutputPuts("    0,\n", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "    xl6_type_compare_%s,\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_type_hash_%s,\n", cTypeName);
    xl6OutputPuts("    0,\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    xl6_type_box_%s,\n", cTypeName);
    xl6OutputPrintf(ctx->out, "    xl6_type_unbox_%s\n", cTypeName);
    xl6OutputPuts("};\n\n", ctx->out);

    xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(void)\n{\n", ctx->out);
    xl6OutputPuts("    static xrt_type_desc desc = {\n", ctx->out);
    xl6OutputPuts("        0u,\n", ctx->out);
    xl6OutputPuts("        XL6_XRT_TYPE_KIND_HANDLE,\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, sourceName);
    xl6OutputPrintf(ctx->out, ", %uu,\n", (unsigned)strlen(sourceName));
    xl6OutputPuts("        ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, ", %uu,\n", (unsigned)strlen(cTypeName));
    xl6OutputPuts("        sizeof(void*), sizeof(void*),\n", ctx->out);
    xl6OutputPrintf(ctx->out, "        &xl6_type_ops_%s, 0, 0, 0\n", cTypeName);
    xl6OutputPuts("    };\n", ctx->out);
    if ( xl6CgenClassHasBaseType(nativeClassNode) ) {
        xl6OutputPuts("    if ( desc.BaseType == 0 ) { desc.BaseType = ", ctx->out);
        xl6CgenEmitClassBaseTypeDesc(ctx, nativeClassNode);
        xl6OutputPuts("; }\n", ctx->out);
    }
    xl6OutputPuts("    return &desc;\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static xvalue ", ctx->out);
    xl6CgenBoxSymbol(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, "(%s value)\n{\n", cTypeName);
    xl6OutputPuts("    xvalue boxed = xvoCreateHandle(", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("(), value.handle, ", ctx->out);
    xl6OutputPuts(destroyNode != NULL ? "XL6_XRT_HANDLE_FLAG_OWNED" : "XL6_XRT_HANDLE_FLAG_BORROWED", ctx->out);
    xl6OutputPuts(" | XL6_XRT_HANDLE_FLAG_NULLABLE);\n", ctx->out);
    xl6OutputPuts("    return boxed != 0 ? boxed : xvoCreateNull();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}
