static void xl6CgenGenericClassDecl(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* classNode;
    const XL6_AstNode* body;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char classTypeName[256];
    char sourceName[512];
    size_t i;

    if ( ctx == NULL || typeNode == NULL || typeNode->childCount == 0 ) {
        return;
    }
    classNode = xl6CgenFindClass(ctx, typeNode->text);
    if ( classNode == NULL || !xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }

    xl6CgenClassTypeRefName(classTypeName, sizeof(classTypeName), typeNode);
    if ( xl6CgenClassWasEmitted(ctx, classTypeName) ) {
        return;
    }
    xl6CgenMarkClassEmitted(ctx, classTypeName);
    xl6CgenAddGenericClassInstance(ctx, classNode, typeNode);

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    body = xl6CgenClassBody(classNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            XL6_AstNode* depType;
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_UNION ) {
                xl6CgenInlineUnionDeclDependencies(ctx, member);
                continue;
            }
            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            depType = xl6CgenCloneCanonicalTypeNode(ctx, member->children[0]);
            if ( depType != NULL && depType->kind == XL6_AST_TYPE && depType->childCount > 0 &&
                 xl6CgenIsClassType(ctx, depType) ) {
                xl6CgenGenericClassDecl(ctx, depType);
            } else {
                xl6CgenTypeDeclDependency(ctx, depType != NULL ? depType : member->children[0]);
            }
            xl6AstFree(depType);
        }
    }

    xl6OutputPrintf(ctx->out, "typedef struct %s {\n", classTypeName);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_UNION ) {
                xl6CgenInlineUnionFieldDecl(ctx, member);
                continue;
            }
            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", ctx->out);
            xl6OutputPuts(xl6CgenTypeName(ctx, member->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            xl6OutputPuts(";\n", ctx->out);
        }
    }
    xl6OutputPrintf(ctx->out, "} %s;\n", classTypeName);

    if ( ctx->needsXValueRuntime ) {
        sourceName[0] = '\0';
        xl6CgenTypeText(ctx, typeNode, sourceName, sizeof(sourceName));
        xl6CgenEmitRecordBoxRuntime(ctx,
                classNode,
                typeNode,
                classTypeName,
                sourceName[0] != '\0' ? sourceName : classTypeName);
    }

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenCollectGenericClassDecls(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_CLASS && xl6CgenClassHasTypeParams(node) ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE && node->childCount > 0 && xl6CgenIsClassType(ctx, node) ) {
        XL6_AstNode* canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, node);
        if ( canonicalType != NULL ) {
            xl6CgenGenericClassDecl(ctx, canonicalType);
            xl6AstFree(canonicalType);
        }
    } else if ( node->kind == XL6_AST_CALL ) {
        XL6_AstNode* classNode = NULL;
        XL6_AstNode* ctorType = xl6CgenGenericClassCtorType(ctx, node, &classNode);
        if ( ctorType != NULL ) {
            char classTypeName[256];
            xl6CgenClassTypeRefName(classTypeName, sizeof(classTypeName), ctorType);
            if ( xl6CgenClassWasEmitted(ctx, classTypeName) ) {
                xl6AstFree(ctorType);
            } else {
                xl6CgenOwnTypeNode(ctx, ctorType);
                xl6CgenGenericClassDecl(ctx, ctorType);
            }
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectGenericClassDecls(ctx, node->children[i]);
    }
}

static void xl6CgenGenericClassMemberSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const char* memberName)
{
    const XL6_AstNode* args;
    size_t i;

    (void)classNode;
    xl6OutputPuts("xl_class_", ctx->out);
    xl6CgenMangledPart(ctx->out, typeNode != NULL ? typeNode->text : "_");
    args = typeNode != NULL && typeNode->childCount > 0 ? typeNode->children[0] : NULL;
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            xl6OutputPuts("__", ctx->out);
            xl6CgenMangledType(ctx, args->children[i]);
        }
    }
    xl6OutputPuts("__", ctx->out);
    xl6CgenMangledPart(ctx->out, memberName != NULL ? memberName : "_");
}

static void xl6CgenGenericClassCallableSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* memberNode)
{
    const XL6_AstNode* params;
    const char* name;
    size_t i;

    name = xl6CgenCallableName(memberNode);
    if ( memberNode != NULL && (memberNode->kind == XL6_AST_GET || memberNode->kind == XL6_AST_LET) ) {
        xl6CgenGenericClassMemberSymbol(ctx,
                                        classNode,
                                        typeNode,
                                        memberNode->kind == XL6_AST_GET ? "__get__" : "__let__");
        xl6OutputPuts("__", ctx->out);
        xl6CgenMangledPart(ctx->out, name != NULL ? name : "_");
    } else {
        xl6CgenGenericClassMemberSymbol(ctx, classNode, typeNode, name != NULL ? name : "_");
    }

    if ( memberNode == NULL ||
         (memberNode->kind != XL6_AST_FUNC && memberNode->kind != XL6_AST_CREATE &&
          memberNode->kind != XL6_AST_GET && memberNode->kind != XL6_AST_LET) ||
         xl6CgenClassCallableOverloadCount(classNode, memberNode) <= 1 ) {
        return;
    }

    params = xl6CgenCallableParams(memberNode);
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("__void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts("__", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenMangledType(ctx, param->children[0]);
        } else {
            xl6OutputPutc('_', ctx->out);
        }
    }
}

static void xl6CgenGenericClassParamList(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* params)
{
    size_t i;

    xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
    xl6OutputPuts("* self", ctx->out);
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        xl6OutputPuts(", ", ctx->out);
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "泛型 class 方法暂不支持可变参数");
            xl6OutputPuts("int ", ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
    }
}

static void xl6CgenGenericClassFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructDecl(ctx, funcNode);
        xl6CgenFuncReturnStructName(ctx, funcNode);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, funcNode);
    xl6OutputPutc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenGenericClassParamList(ctx, typeNode, params);
    xl6OutputPutc(')', ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassSpecialProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode, const XL6_AstNode* specialNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    const XL6_AstNode* params;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6OutputPuts("void ", ctx->out);
    xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, specialNode);
    xl6OutputPutc('(', ctx->out);
    params = specialNode->childCount > 0 ? specialNode->children[0] : NULL;
    xl6CgenGenericClassParamList(ctx, typeNode, params);
    xl6OutputPutc(')', ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassDestroyProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->children[0];

    xl6OutputPuts("void ", ctx->out);
    xl6CgenGenericClassMemberSymbol(ctx, classNode, typeNode, "destroy");
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
    xl6OutputPuts("* self)", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}

static void xl6CgenGenericClassMethodProtos(XL6_CgenContext* ctx)
{
    size_t i;
    size_t j;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->genericClassInstanceCount; ++i ) {
        const XL6_AstNode* classNode = ctx->genericClassInstances[i].classNode;
        const XL6_AstNode* typeNode = ctx->genericClassInstances[i].typeNode;
        const XL6_AstNode* body = xl6CgenClassBody(classNode);
        int hasDestroy;

        if ( body == NULL ) {
            continue;
        }
        hasDestroy = 0;
        for ( j = 0; j < body->childCount; ++j ) {
            const XL6_AstNode* member = body->children[j];
            if ( member != NULL &&
                 (member->kind == XL6_AST_FUNC || member->kind == XL6_AST_GET || member->kind == XL6_AST_LET) ) {
                xl6CgenGenericClassFuncProto(ctx, classNode, typeNode, member);
                xl6OutputPuts(";\n", ctx->out);
            } else if ( member != NULL && member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenGenericClassSpecialProto(ctx, classNode, typeNode, member);
                xl6OutputPuts(";\n", ctx->out);
            }
        }
        if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenGenericClassDestroyProto(ctx, classNode, typeNode);
            xl6OutputPuts(";\n", ctx->out);
        }
    }
}

static void xl6CgenClassDecls(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    char cTypeName[512];
    size_t i;

    if ( classNode == NULL || classNode->text == NULL || xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        return;
        xl6CgenError(ctx, classNode, "当前 C 生成暂不支持泛型 class");
        return;
    }

    xl6CgenMarkClassEmitted(ctx, classNode->text);

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( baseClass != NULL ) {
            xl6CgenClassDecls(ctx, baseClass);
        }
    }

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_UNION ) {
                xl6CgenInlineUnionDeclDependencies(ctx, member);
                continue;
            }
            if ( member->kind == XL6_AST_FIELD && member->childCount > 0 ) {
                xl6CgenTypeDeclDependency(ctx, member->children[0]);
            }
        }
    }

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" {\n", ctx->out);
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenClassTypeName(ctx->out, base->text);
        xl6OutputPuts(" base;\n", ctx->out);
    }
    xl6CgenVirtualFieldDecl(ctx, classNode);

    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_UNION ) {
                xl6CgenInlineUnionFieldDecl(ctx, member);
                continue;
            }
            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", ctx->out);
            xl6OutputPuts(xl6CgenTypeName(ctx, member->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            xl6OutputPuts(";\n", ctx->out);
        }
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenStaticClassFieldDecls(ctx, classNode);

    if ( body != NULL ) {
        int hasDestroy = 0;
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_FUNC || member->kind == XL6_AST_GET || member->kind == XL6_AST_LET ) {
                xl6CgenClassFuncProto(ctx, classNode, member);
                xl6OutputPuts(";\n", ctx->out);
            } else if ( member->kind == XL6_AST_CREATE ) {
                if ( xl6CTextEquals(member->text, "destroy") ) {
                    hasDestroy = 1;
                }
                xl6CgenClassSpecialProto(ctx, classNode, member);
                xl6OutputPuts(";\n", ctx->out);
            }
        }
        if ( !hasDestroy && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            xl6CgenClassDestroyProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoCopyProtocol(ctx, classNode, "copy", "__copy__") ) {
            xl6CgenClassAutoCopyProto(ctx, classNode, "__copy__");
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoCopyProtocol(ctx, classNode, "clone", "__clone__") ) {
            xl6CgenClassAutoCopyProto(ctx, classNode, "__clone__");
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__toValue__") ) {
            xl6CgenClassAutoToValueProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__fromValue__") ) {
            xl6CgenClassAutoFromValueProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenClassCopyBuiltinHelperProtos(ctx, classNode);
        xl6CgenClassPersistenceHelperProtos(ctx, classNode);
    }
    xl6CgenVirtualTableStructDecl(ctx, classNode);
    xl6CgenVirtualTableObjectDecls(ctx, classNode);

    if ( ctx->needsXValueRuntime ) {
        xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
        xl6CgenEmitRecordBoxRuntime(ctx, classNode, NULL, cTypeName, classNode->text);
    }
}

static void xl6CgenClassDefaultInitFields(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    char basePath[512];
    size_t i;

    if ( classNode == NULL || varPath == NULL ) {
        return;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        baseClass = xl6CgenFindClass(ctx, base->text);
        if ( baseClass != NULL ) {
            snprintf(basePath, sizeof(basePath), "%s.base", varPath);
            xl6CgenClassDefaultInitFields(ctx, baseClass, basePath);
        }
    }
    xl6CgenClassInitVTables(ctx, classNode, varPath);

    body = classNode->childCount > 1 ? classNode->children[1] : NULL;
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        const XL6_AstNode* initExpr;
        XL6_AstNode* fieldCustom;
        XL6_AstNode* ctorCustom;

        if ( !xl6CgenIsInstanceField(member) || member->childCount < 2 ) {
            continue;
        }
        fieldType = member->children[0];
        initExpr = member->children[1];
        fieldCustom = fieldType != NULL ? xl6CgenFindCustom(ctx, fieldType->text) : NULL;
        ctorCustom = NULL;
        if ( fieldCustom != NULL && xl6CgenCallIsCustomCtor(ctx, initExpr, &ctorCustom) ) {
            if ( !xl6CTextEquals(fieldCustom->text, ctorCustom->text) ) {
                xl6CgenError(ctx, initExpr, "custom field initializer constructor type does not match field type");
                continue;
            }
            xl6CgenIndent(ctx);
            xl6OutputPuts(varPath, ctx->out);
            xl6OutputPutc('.', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenCustomCtorValue(ctx, ctorCustom, initExpr);
            xl6OutputPuts(";\n", ctx->out);
            continue;
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts(varPath, ctx->out);
        xl6OutputPutc('.', ctx->out);
        xl6CgenFputsCIdent(ctx->out, member->text);
        xl6OutputPuts(" = ", ctx->out);
        xl6CgenExpr(ctx, initExpr);
        xl6OutputPuts(";\n", ctx->out);
    }
}

static void xl6CgenCustomParamList(XL6_CgenContext* ctx, const XL6_AstNode* params, int includeSelf)
{
    size_t i;
    int wrote;

    wrote = 0;
    if ( includeSelf ) {
        xl6OutputPuts("void* self", ctx->out);
        wrote = 1;
    }
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( wrote ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
                 xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
                xl6CgenError(ctx, param, "custom extern parameter must have a non-vararg C ABI type");
                xl6OutputPuts("int ", ctx->out);
            } else {
                xl6OutputPuts(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
                xl6OutputPutc(' ', ctx->out);
            }
            xl6CgenFputsCIdent(ctx->out, param->text);
            wrote = 1;
        }
    }
    if ( !wrote ) {
        xl6OutputPuts("void", ctx->out);
    }
}

static void xl6CgenCustomExternProto(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* member)
{
    const XL6_AstNode* nameNode;
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    (void)customNode;
    if ( member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ) {
        nameNode = member->childCount > 0 ? member->children[0] : NULL;
        params = member->childCount > 1 ? member->children[1] : NULL;
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, "create") ) {
            xl6OutputPuts("void* ", ctx->out);
            xl6CgenCStringSymbol(ctx->out, member->text);
            xl6OutputPutc('(', ctx->out);
            xl6CgenCustomParamList(ctx, params, 0);
            xl6OutputPutc(')', ctx->out);
        } else if ( nameNode != NULL && xl6CTextEquals(nameNode->text, "destroy") ) {
            xl6OutputPuts("long long ", ctx->out);
            xl6CgenCStringSymbol(ctx->out, member->text);
            xl6OutputPuts("(void* self)", ctx->out);
        }
        return;
    }

    nameNode = member->childCount > 0 ? member->children[0] : NULL;
    (void)nameNode;
    params = member->childCount > 1 ? member->children[1] : NULL;
    returns = member->childCount > 2 ? member->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "custom extern method cannot return multiple values");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenCStringSymbol(ctx->out, member->text);
    xl6OutputPutc('(', ctx->out);
    xl6CgenCustomParamList(ctx, params, 1);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenCustomHelperParamList(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* params)
{
    size_t i;

    xl6CgenCustomTypeName(ctx->out, customNode->text);
    xl6OutputPuts("* self", ctx->out);
    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts(", ", ctx->out);
        if ( param->childCount == 0 || xl6CTextEquals(param->children[0]->text, "array<var>") ||
             xl6CTextEquals(param->children[0]->text, "dict<var>") ) {
            xl6CgenError(ctx, param, "custom helper C 生成暂不支持可变参数方法");
            xl6OutputPuts("int ", ctx->out);
        } else {
            xl6OutputPuts(xl6CgenTypeName(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, param->text);
    }
}

static void xl6CgenCustomFuncProto(XL6_CgenContext* ctx, const XL6_AstNode* customNode, const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    returns = funcNode->childCount > 2 ? funcNode->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenError(ctx, returns, "custom helper method cannot return multiple values");
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenCustomCallableSymbol(ctx, customNode, funcNode);
    xl6OutputPutc('(', ctx->out);
    params = funcNode->childCount > 1 ? funcNode->children[1] : NULL;
    xl6CgenCustomHelperParamList(ctx, customNode, params);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenCustomDecls(XL6_CgenContext* ctx, const XL6_AstNode* customNode)
{
    const XL6_AstNode* body;
    char cTypeName[512];
    size_t i;

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenCustomTypeName(ctx->out, customNode->text);
    xl6OutputPuts(" { void* handle; } ", ctx->out);
    xl6CgenCustomTypeName(ctx->out, customNode->text);
    xl6OutputPuts(";\n", ctx->out);

    if ( ctx->needsXValueRuntime ) {
        xl6CgenCustomTypeRefName(cTypeName, sizeof(cTypeName), customNode->text);
        xl6CgenEmitHandleBoxRuntime(ctx, customNode, cTypeName, customNode->text);
    }

    if ( customNode->childCount == 0 ) {
        return;
    }
    body = customNode->children[0];
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL || member->kind == XL6_AST_CUSTOM_EXTERN_FUNC ) {
            xl6CgenCustomExternProto(ctx, customNode, member);
            xl6OutputPuts(";\n", ctx->out);
        } else if ( member->kind == XL6_AST_FUNC ) {
            xl6CgenCustomFuncProto(ctx, customNode, member);
            xl6OutputPuts(";\n", ctx->out);
        }
    }
}
