static void xl6CgenGenericClassDecl(XL6_CgenContext* ctx, const XL6_AstNode* typeNode);
static void xl6CgenNativeExternProto(XL6_CgenContext* ctx, const XL6_AstNode* nativeClassNode, const XL6_AstNode* member);

/*
 * 方法签名可以引用源码顺序中稍后声明的 class。先统一声明类型名，
 * 再输出完整布局，避免标准库注入顺序或互相引用的方法签名污染 C ABI。
 */
static void xl6CgenClassForwardDecls(XL6_CgenContext* ctx, const XL6_AstNode* root)
{
    size_t i;

    if ( ctx == NULL || root == NULL ) {
        return;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* classNode = root->children[i];

        if ( classNode == NULL || classNode->kind != XL6_AST_CLASS ||
             classNode->text == NULL || xl6CgenClassHasTypeParams(classNode) ) {
            continue;
        }
        xl6OutputPuts("typedef struct ", ctx->out);
        xl6CgenClassTypeName(ctx->out, classNode->text);
        xl6OutputPutc(' ', ctx->out);
        xl6CgenClassTypeName(ctx->out, classNode->text);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPutc('\n', ctx->out);
}

static int xl6CgenGenericClassMemberHasSignatureTypes(const XL6_AstNode* member)
{
    return member != NULL &&
           (member->kind == XL6_AST_FUNC ||
            member->kind == XL6_AST_ASYNC_FUNC ||
            member->kind == XL6_AST_GET ||
            member->kind == XL6_AST_LET ||
            member->kind == XL6_AST_CREATE);
}

static void xl6CgenGenericClassTypeDeclDependency(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_AstNode* depType;
    const XL6_AstNode* depTarget;
    XL6_AstNode* depClass;

    if ( ctx == NULL || typeNode == NULL ) {
        return;
    }
    depType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    depTarget = xl6CgenClassHandleTargetType(ctx, depType);
    if ( depTarget == NULL ) {
        depTarget = depType;
    }
    depClass = depTarget != NULL && depTarget->text != NULL ? xl6CgenFindClass(ctx, depTarget->text) : NULL;
    if ( depTarget != NULL && depTarget->childCount > 0 && depClass != NULL &&
         xl6CgenClassHasTypeParams(depClass) ) {
        xl6CgenGenericClassDecl(ctx, depTarget);
    } else {
        xl6CgenTypeDeclDependency(ctx, depType != NULL ? depType : typeNode);
    }
    xl6AstFree(depType);
}

static void xl6CgenGenericClassCallableDeclDependencies(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( ctx == NULL || member == NULL ) {
        return;
    }
    params = xl6CgenCallableParams(member);
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( param != NULL && param->childCount > 0 ) {
                xl6CgenGenericClassTypeDeclDependency(ctx, param->children[0]);
            }
        }
    }
    returns = member->childCount > 2 ? member->children[2] : NULL;
    if ( returns != NULL ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            xl6CgenGenericClassTypeDeclDependency(ctx, returns->children[i]);
        }
    }
}

static void xl6CgenGenericClassDecl(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* classNode;
    const XL6_AstNode* body;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    char classTypeName[256];
    char sourceName[512];
    size_t i;
    int fieldCount;

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
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_UNION ) {
                xl6CgenInlineUnionDeclDependencies(ctx, member);
                continue;
            }
            if ( xl6CgenGenericClassMemberHasSignatureTypes(member) ) {
                xl6CgenGenericClassCallableDeclDependencies(ctx, member);
                continue;
            }
            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            xl6CgenGenericClassTypeDeclDependency(ctx, member->children[0]);
        }
    }

    xl6OutputPrintf(ctx->out, "typedef struct %s {\n", classTypeName);
    fieldCount = 0;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_UNION ) {
                xl6CgenInlineUnionFieldDecl(ctx, member);
                fieldCount += 1;
                continue;
            }
            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", ctx->out);
            if ( xl6CgenIsTypedContainerType(ctx, member->children[0]) ) {
                xl6OutputPuts(xl6CgenTypedContainerCType(ctx, member->children[0]), ctx->out);
            } else {
                xl6OutputPuts(xl6CgenTypeName(ctx, member->children[0]), ctx->out);
            }
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            xl6OutputPuts(";\n", ctx->out);
            fieldCount += 1;
        }
    }
    if ( fieldCount == 0 ) {
        xl6OutputPuts("    int __xl6_empty;\n", ctx->out);
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
        xl6OutputPuts(xl6CgenFuncSingleReturnCType(ctx, funcNode), ctx->out);
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
            if ( member != NULL && member->kind == XL6_AST_ASYNC_FUNC ) {
                xl6CgenAsyncClassFuncProto(
                    ctx,
                    classNode,
                    typeNode,
                    member);
                xl6OutputPuts(";\n", ctx->out);
            } else if ( member != NULL &&
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
        if ( ctx->needsXValueRuntime ) {
            const XL6_AstNode* prevParams = ctx->currentGenericParams;
            const XL6_AstNode* prevArgs = ctx->currentGenericArgs;
            char cTypeName[256];

            ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
            ctx->currentGenericArgs = typeNode->childCount > 0 ? typeNode->children[0] : NULL;
            xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
            xl6CgenEmitDynamicMethodXCallProtos(ctx, classNode, typeNode);
            xl6CgenEmitDynamicMethodTable(ctx, classNode, typeNode, cTypeName);
            ctx->currentGenericParams = prevParams;
            ctx->currentGenericArgs = prevArgs;
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
    int fieldCount;

    if ( classNode == NULL || classNode->text == NULL || xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }
    if ( xl6CgenClassHasTypeParams(classNode) ) {
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

    xl6OutputPuts("struct ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" {\n", ctx->out);
    fieldCount = 0;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenClassTypeName(ctx->out, base->text);
        xl6OutputPuts(" base;\n", ctx->out);
        fieldCount += 1;
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
                fieldCount += 1;
                continue;
            }
            if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", ctx->out);
            if ( xl6CgenIsTypedContainerType(ctx, member->children[0]) ) {
                xl6OutputPuts(xl6CgenTypedContainerCType(ctx, member->children[0]), ctx->out);
            } else {
                xl6OutputPuts(xl6CgenTypeName(ctx, member->children[0]), ctx->out);
            }
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, member->text);
            xl6OutputPuts(";\n", ctx->out);
            fieldCount += 1;
        }
    }
    if ( fieldCount == 0 ) {
        xl6OutputPuts("    int __xl6_empty;\n", ctx->out);
    }
    xl6OutputPuts("};\n", ctx->out);

    xl6CgenStaticClassFieldDecls(ctx, classNode);

    if ( body != NULL ) {
        int hasDestroy = 0;
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member == NULL ) {
                continue;
            }
            if ( member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ||
                 member->kind == XL6_AST_NATIVE_EXTERN_FUNC ||
                 xl6CgenIsExternClassProperty(member) ) {
                xl6CgenNativeExternProto(ctx, classNode, member);
                xl6OutputPuts(";\n", ctx->out);
            }
            if ( member->kind == XL6_AST_ASYNC_FUNC ) {
                xl6CgenAsyncClassFuncProto(
                    ctx,
                    classNode,
                    NULL,
                    member);
                xl6OutputPuts(";\n", ctx->out);
            } else if ( member->kind == XL6_AST_FUNC ||
                 member->kind == XL6_AST_EXPORT_FUNC ||
                 member->kind == XL6_AST_GET ||
                 member->kind == XL6_AST_LET ||
                 member->kind == XL6_AST_NATIVE_EXTERN_FUNC ) {
                xl6CgenClassFuncProto(ctx, classNode, member);
                xl6OutputPuts(";\n", ctx->out);
                if ( member->kind == XL6_AST_EXPORT_FUNC &&
                     member->text != NULL && member->text[0] != '\0' ) {
                    xl6CgenClassExportWrapperProto(ctx, classNode, member);
                    xl6OutputPuts(";\n", ctx->out);
                }
            } else if ( member->kind == XL6_AST_CREATE || member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ) {
                if ( xl6CTextEquals(xl6CgenCallableName(member), "destroy") ) {
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
            xl6CgenClassAutoToValueFormatProto(ctx, classNode, NULL);
            xl6OutputPuts(";\n", ctx->out);
            if ( xl6CgenClassHasFormatDerive(classNode, "json") ) {
                xl6CgenClassAutoToValueFormatProto(ctx, classNode, "json");
                xl6OutputPuts(";\n", ctx->out);
            }
            if ( xl6CgenClassHasFormatDerive(classNode, "xson") ) {
                xl6CgenClassAutoToValueFormatProto(ctx, classNode, "xson");
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6CgenClassAutoToValueProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__fromValue__") ) {
            xl6CgenClassAutoFromValueFormatProto(ctx, classNode, NULL);
            xl6OutputPuts(";\n", ctx->out);
            if ( xl6CgenClassHasFormatDerive(classNode, "json") ) {
                xl6CgenClassAutoFromValueFormatProto(ctx, classNode, "json");
                xl6OutputPuts(";\n", ctx->out);
            }
            if ( xl6CgenClassHasFormatDerive(classNode, "xson") ) {
                xl6CgenClassAutoFromValueFormatProto(ctx, classNode, "xson");
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6CgenClassAutoFromValueProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "string", "__toString__") ) {
            xl6CgenClassAutoToStringProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "hash", "__hash__") ) {
            xl6CgenClassAutoHashProto(ctx, classNode);
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "equals", "__eq__") ) {
            xl6CgenClassAutoEqualsProto(ctx, classNode, "__eq__");
            xl6OutputPuts(";\n", ctx->out);
        }
        if ( xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "equals", "__ne__") ) {
            xl6CgenClassAutoEqualsProto(ctx, classNode, "__ne__");
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenClassPersistenceHelperProtos(ctx, classNode);
    }
    xl6CgenVirtualTableStructDecl(ctx, classNode);
    xl6CgenVirtualTableObjectDecls(ctx, classNode);

    if ( ctx->needsXValueRuntime ) {
        xl6CgenEmitDynamicMethodXCallProtos(ctx, classNode, NULL);
        xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
        if ( xl6CgenClassIsPureNativeHandle(classNode) ) {
            xl6CgenEmitHandleBoxRuntime(ctx, classNode, cTypeName, classNode->text);
        } else {
            xl6CgenEmitRecordBoxRuntime(ctx, classNode, NULL, cTypeName, classNode->text);
        }
    }
}

/*
 * class 字段属于可跨线程共享的对象。
 * nullable 容器与普通容器共享 C ABI，但默认值必须是 null，不能隐式创建空容器。
 */
