static void xl6CgenHeaderUnionDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount)
{
    const XL6_AstNode* body;
    size_t i;
    char typeName[256];

    if ( out == NULL || root == NULL || node == NULL || node->text == NULL ||
         emittedUnions == NULL || emittedUnionCount == NULL ) {
        return;
    }
    if ( xl6CgenHeaderNameListed(emittedUnions, *emittedUnionCount, node->text) ) {
        return;
    }
    emittedUnions[*emittedUnionCount] = node->text;
    *emittedUnionCount += 1;

    body = node->childCount > 0 ? node->children[0] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL || field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                field->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount,
                emittedEnums,
                emittedEnumCount);
        }
    }

    xl6CgenHeaderUnionTypeTextName(typeName, sizeof(typeName), node->text);
    xl6OutputPrintf(out, "typedef union %s {\n", typeName);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL || field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", out);
            xl6CgenHeaderPutTypeName(out, root, field->children[0]);
            xl6OutputPutc(' ', out);
            xl6CgenHeaderFputsCIdent(out, field->text);
            xl6OutputPuts(";\n", out);
        }
    }
    xl6OutputPrintf(out, "} %s;\n", typeName);
}

static void xl6CgenHeaderEnumValue(
    XL6_Output* out,
    const XL6_AstNode* value)
{
    if ( value == NULL ) {
        xl6OutputPuts("0", out);
        return;
    }
    if ( value->kind == XL6_AST_UNARY && value->text != NULL &&
         value->childCount == 1 ) {
        xl6OutputPuts(value->text, out);
        xl6CgenHeaderEnumValue(out, value->children[0]);
        return;
    }
    xl6OutputPuts(value->text != NULL ? value->text : "0", out);
}

static void xl6CgenHeaderEnumDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const XL6_AstNode* enumType,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount)
{
    const XL6_AstNode* variants;
    const XL6_AstNode* typeParams;
    const XL6_AstNode* typeArgs;
    XL6_AstNode typeRef;
    char typeName[512];
    size_t i;

    if ( out == NULL || root == NULL || node == NULL || node->text == NULL ||
         emittedEnums == NULL || emittedEnumCount == NULL ) {
        return;
    }
    memset(&typeRef, 0, sizeof(typeRef));
    if ( enumType == NULL ) {
        typeRef.kind = XL6_AST_TYPE;
        typeRef.text = node->text;
        enumType = &typeRef;
    }
    if ( xl6CgenHeaderEnumHasTypeParams(node) &&
         (enumType->childCount == 0 || enumType->children[0] == NULL ||
          enumType->children[0]->kind != XL6_AST_LIST) ) {
        return;
    }
    xl6CgenHeaderEnumTypeRefName(typeName, sizeof(typeName), enumType);
    if ( xl6CgenHeaderNameListed(emittedEnums, *emittedEnumCount, typeName) ) {
        return;
    }
    emittedEnums[*emittedEnumCount] =
        xrtStrDupN(typeName, strlen(typeName));
    if ( emittedEnums[*emittedEnumCount] == NULL ) {
        return;
    }
    *emittedEnumCount += 1;
    variants = node->childCount > 1 ? node->children[1] : NULL;
    typeParams = node->childCount > 2 ? node->children[2] : NULL;
    typeArgs = enumType->childCount > 0 &&
               enumType->children[0] != NULL &&
               enumType->children[0]->kind == XL6_AST_LIST ?
        enumType->children[0] :
        NULL;

    if ( (node->flags & XL6_AST_FLAG_NUMERIC_ENUM) != 0 ) {
        if ( node->childCount > 0 ) {
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                node->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount,
                emittedEnums,
                emittedEnumCount);
        }
    } else {
        for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
            const XL6_AstNode* variant = variants->children[i];
            const XL6_AstNode* params =
                variant != NULL && variant->childCount > 0 ?
                    variant->children[0] :
                    NULL;
            size_t p;

            for ( p = 0; params != NULL && p < params->childCount; ++p ) {
                const XL6_AstNode* param = params->children[p];

                if ( param != NULL && param->childCount > 0 ) {
                    XL6_AstNode* concreteType = xl6CgenHeaderSubstituteType(
                        typeParams,
                        typeArgs,
                        param->children[0]);

                    xl6CgenHeaderTypeDependencyDecl(
                        out,
                        root,
                        concreteType != NULL ?
                            concreteType :
                            param->children[0],
                        emittedUnions,
                        emittedUnionCount,
                        emittedClasses,
                        emittedClassCount,
                        emittedEnums,
                        emittedEnumCount);
                    xl6AstFree(concreteType);
                }
            }
        }
    }

    if ( (node->flags & XL6_AST_FLAG_NUMERIC_ENUM) != 0 ) {
        xl6OutputPuts("typedef ", out);
        xl6CgenHeaderPutTypeName(
            out,
            root,
            node->childCount > 0 ? node->children[0] : NULL);
        xl6OutputPrintf(out, " %s;\n", typeName);
        for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
            const XL6_AstNode* variant = variants->children[i];

            xl6OutputPrintf(out, "#define XL6_ENUM_%s__", typeName);
            xl6CgenHeaderFputsCIdent(out, variant != NULL ? variant->text : NULL);
            xl6OutputPrintf(out, " ((%s)(", typeName);
            xl6CgenHeaderEnumValue(
                out,
                variant != NULL && variant->childCount > 0 ?
                    variant->children[0] :
                    NULL);
            xl6OutputPuts("))\n", out);
        }
        xl6OutputPutc('\n', out);
        if ( (node->flags & XL6_AST_FLAG_PUBLIC) != 0 ) {
            xl6OutputPrintf(
                out,
                "XL6_C_API const xl6_type_desc* xl6_enum_type_%s(void);\n\n",
                typeName);
        }
        return;
    }

    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];

        xl6OutputPrintf(out, "#define XL6_ENUM_TAG_%s__", typeName);
        xl6CgenHeaderFputsCIdent(out, variant != NULL ? variant->text : NULL);
        xl6OutputPrintf(out, " %uu\n", (unsigned)(i + 1u));
    }
    xl6OutputPrintf(
        out,
        "typedef struct %s {\n    uint32_t tag;\n    union {\n",
        typeName);
    for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
        const XL6_AstNode* variant = variants->children[i];
        const XL6_AstNode* params =
            variant != NULL && variant->childCount > 0 ?
                variant->children[0] :
                NULL;
        size_t p;

        if ( params == NULL || params->childCount == 0 ) {
            continue;
        }
        xl6OutputPuts("        struct {\n", out);
        for ( p = 0; p < params->childCount; ++p ) {
            const XL6_AstNode* param = params->children[p];
            XL6_AstNode* concreteType = xl6CgenHeaderSubstituteType(
                typeParams,
                typeArgs,
                param != NULL && param->childCount > 0 ?
                    param->children[0] :
                    NULL);

            xl6OutputPuts("            ", out);
            xl6CgenHeaderPutTypeName(
                out,
                root,
                concreteType != NULL ?
                    concreteType :
                    (param != NULL && param->childCount > 0 ?
                        param->children[0] :
                        NULL));
            xl6OutputPutc(' ', out);
            xl6CgenHeaderFputsCIdent(out, param != NULL ? param->text : NULL);
            xl6OutputPuts(";\n", out);
            xl6AstFree(concreteType);
        }
        xl6OutputPuts("        } ", out);
        xl6CgenHeaderFputsCIdent(out, variant != NULL ? variant->text : NULL);
        xl6OutputPuts(";\n", out);
    }
    xl6OutputPuts("        unsigned char __empty;\n    } payload;\n} ", out);
    xl6OutputPuts(typeName, out);
    xl6OutputPuts(";\n\n", out);
    if ( (node->flags & XL6_AST_FLAG_PUBLIC) != 0 ) {
        xl6OutputPrintf(
            out,
            "XL6_C_API const xl6_type_desc* xl6_enum_type_%s(void);\n",
            typeName);
        xl6OutputPrintf(
            out,
            "XL6_C_API bool xl6_enum_copy_%s(%s* out, const %s* source);\n",
            typeName,
            typeName,
            typeName);
        xl6OutputPrintf(
            out,
            "XL6_C_API bool xl6_enum_move_%s(%s* out, %s* source);\n",
            typeName,
            typeName,
            typeName);
        xl6OutputPrintf(
            out,
            "XL6_C_API void xl6_enum_drop_%s(%s* value);\n",
            typeName,
            typeName);
        for ( i = 0; variants != NULL && i < variants->childCount; ++i ) {
            const XL6_AstNode* variant = variants->children[i];
            const XL6_AstNode* params =
                variant != NULL && variant->childCount > 0 ?
                    variant->children[0] :
                    NULL;
            size_t p;

            xl6OutputPrintf(out, "XL6_C_API bool xl6_enum_make_%s__", typeName);
            xl6CgenHeaderFputsCIdent(out, variant != NULL ? variant->text : NULL);
            xl6OutputPrintf(out, "(%s* out", typeName);
            for ( p = 0; params != NULL && p < params->childCount; ++p ) {
                const XL6_AstNode* param = params->children[p];
                XL6_AstNode* concreteType = xl6CgenHeaderSubstituteType(
                    typeParams,
                    typeArgs,
                    param != NULL && param->childCount > 0 ?
                        param->children[0] :
                        NULL);

                xl6OutputPuts(", ", out);
                xl6CgenHeaderPutTypeName(
                    out,
                    root,
                    concreteType != NULL ?
                        concreteType :
                        (param != NULL && param->childCount > 0 ?
                            param->children[0] :
                            NULL));
                xl6OutputPutc(' ', out);
                xl6CgenHeaderFputsCIdent(out, param != NULL ? param->text : NULL);
                xl6AstFree(concreteType);
            }
            xl6OutputPuts(");\n", out);
        }
        xl6OutputPutc('\n', out);
    }
}

static void xl6CgenHeaderClassDecl(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    size_t i;
    char typeName[256];

    if ( out == NULL || root == NULL || node == NULL || node->text == NULL ||
         emittedClasses == NULL || emittedClassCount == NULL ) {
        return;
    }
    /*
        开放泛型 class 没有单一 C 布局，不能把类型参数 T 退化成 void 字段。
        当前 C ABI 明确禁止泛型 class 导出；具体实例布局由生成模块内部持有，
        因此自动注入给辅助 C 源的头文件只跳过开放模板，不伪造兼容结构。
    */
    if ( xl6CgenHeaderClassHasTypeParams(node) ) {
        return;
    }
    if ( xl6CgenHeaderNameListed(emittedClasses, *emittedClassCount, node->text) ) {
        return;
    }
    emittedClasses[*emittedClassCount] = node->text;
    *emittedClassCount += 1;

    base = node->childCount > 0 ? node->children[0] : NULL;
    if ( base != NULL && base->kind == XL6_AST_NAME ) {
        xl6CgenHeaderClassDecl(
            out,
            root,
            xl6CgenHeaderFindClass(root, base->text),
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount,
            emittedEnums,
            emittedEnumCount);
    }

    body = node->childCount > 1 ? node->children[1] : NULL;
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL ) {
                continue;
            }
            if ( field->kind == XL6_AST_UNION ) {
                xl6CgenHeaderInlineUnionDependencyDecls(
                    out,
                    root,
                    field,
                    emittedUnions,
                    emittedUnionCount,
                    emittedClasses,
                    emittedClassCount,
                    emittedEnums,
                    emittedEnumCount);
                continue;
            }
            if ( field->kind != XL6_AST_FIELD || (field->flags & XL6_AST_FLAG_STATIC) != 0 || field->childCount == 0 ) {
                continue;
            }
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                field->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount,
                emittedEnums,
                emittedEnumCount);
        }
    }

    xl6CgenHeaderClassTypeTextName(typeName, sizeof(typeName), node->text);
    xl6OutputPrintf(out, "typedef struct %s {\n", typeName);
    if ( base != NULL && base->kind == XL6_AST_NAME && xl6CgenHeaderFindClass(root, base->text) != NULL ) {
        xl6OutputPuts("    ", out);
        xl6CgenHeaderPutTypeName(out, root, base);
        xl6OutputPuts(" base;\n", out);
    }
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field == NULL ) {
                continue;
            }
            if ( field->kind == XL6_AST_UNION ) {
                xl6CgenHeaderInlineUnionDecl(out, root, field);
                continue;
            }
            if ( field->kind != XL6_AST_FIELD || (field->flags & XL6_AST_FLAG_STATIC) != 0 || field->childCount == 0 ) {
                continue;
            }
            xl6OutputPuts("    ", out);
            xl6CgenHeaderPutTypeName(out, root, field->children[0]);
            xl6OutputPutc(' ', out);
            xl6CgenHeaderFputsCIdent(out, field->text);
            xl6OutputPuts(";\n", out);
        }
    }
    xl6OutputPrintf(out, "} %s;\n", typeName);
}

static void xl6CgenHeaderExportFuncProto(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t i;

    if ( node->kind == XL6_AST_EXPORT_FUNC ) {
        xl6OutputPuts("XL6_C_API ", out);
    }
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6CgenHeaderPutTypeName(out, root, returns->children[0]);
    } else {
        xl6OutputPuts("void", out);
    }
    xl6OutputPutc(' ', out);
    xl6CgenHeaderExportFuncSymbol(out, root, node);
    xl6OutputPutc('(', out);

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( (params == NULL || params->childCount == 0) && (returns == NULL || returns->childCount <= 1) ) {
        xl6OutputPuts("void", out);
    } else {
        int wroteParam = 0;

        if ( params != NULL ) {
            for ( i = 0; i < params->childCount; ++i ) {
                const XL6_AstNode* param = params->children[i];
                if ( wroteParam ) {
                    xl6OutputPuts(", ", out);
                }
                xl6CgenHeaderParamDecl(out, root, param);
                wroteParam = 1;
            }
        }
        if ( returns != NULL && returns->childCount > 1 ) {
            for ( i = 0; i < returns->childCount; ++i ) {
                if ( wroteParam ) {
                    xl6OutputPuts(", ", out);
                }
                xl6CgenHeaderPutTypeName(out, root, returns->children[i]);
                xl6OutputPrintf(out, "* out%u", (unsigned)i);
                wroteParam = 1;
            }
        }
    }
    xl6OutputPuts(");\n", out);
}

static void xl6CgenHeaderClassExportFuncProto(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* classNode, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    char classTypeName[256];
    size_t i;
    int wroteParam;

    if ( out == NULL || classNode == NULL || node == NULL ) {
        return;
    }

    xl6OutputPuts("XL6_C_API ", out);
    returns = node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6CgenHeaderPutTypeName(out, root, returns->children[0]);
    } else {
        xl6OutputPuts("void", out);
    }
    xl6OutputPutc(' ', out);
    xl6CgenHeaderExportFuncSymbol(out, root, node);
    xl6OutputPutc('(', out);

    xl6CgenHeaderClassTypeTextName(classTypeName, sizeof(classTypeName), classNode->text);
    xl6OutputPuts(classTypeName, out);
    xl6OutputPuts("* self", out);
    wroteParam = 1;

    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( wroteParam ) {
                xl6OutputPuts(", ", out);
            }
            xl6CgenHeaderParamDecl(out, root, param);
            wroteParam = 1;
        }
    }
    if ( returns != NULL && returns->childCount > 1 ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            if ( wroteParam ) {
                xl6OutputPuts(", ", out);
            }
            xl6CgenHeaderPutTypeName(out, root, returns->children[i]);
            xl6OutputPrintf(out, "* out%u", (unsigned)i);
            wroteParam = 1;
        }
    }
    xl6OutputPuts(");\n", out);
}

static void xl6CgenHeaderClassExportFuncProtos(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( xl6CgenHeaderClassHasTypeParams(classNode) ) {
        return;
    }
    body = classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_EXPORT_FUNC &&
             member->text != NULL &&
             member->text[0] != '\0' ) {
            xl6CgenHeaderClassExportFuncProto(out, root, classNode, member);
        }
    }
}

static void xl6CgenHeaderCallableDependencyDecls(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node,
    const char** emittedUnions,
    size_t* emittedUnionCount,
    const char** emittedClasses,
    size_t* emittedClassCount,
    const char** emittedEnums,
    size_t* emittedEnumCount)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( node == NULL ) {
        return;
    }
    params = node->childCount > 1 ? node->children[1] : NULL;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        if ( param != NULL && param->childCount > 0 ) {
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                param->children[0],
                emittedUnions,
                emittedUnionCount,
                emittedClasses,
                emittedClassCount,
                emittedEnums,
                emittedEnumCount);
        }
    }
    for ( i = 0; returns != NULL && i < returns->childCount; ++i ) {
        xl6CgenHeaderTypeDependencyDecl(
            out,
            root,
            returns->children[i],
            emittedUnions,
            emittedUnionCount,
            emittedClasses,
            emittedClassCount,
            emittedEnums,
            emittedEnumCount);
    }
}

static size_t xl6CgenHeaderAstNodeCount(const XL6_AstNode* node)
{
    size_t result;
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    result = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        size_t childCount = xl6CgenHeaderAstNodeCount(node->children[i]);

        if ( SIZE_MAX - result < childCount ) {
            return SIZE_MAX;
        }
        result += childCount;
    }
    return result;
}

static void xl6CgenHeaderExportVarDecl(XL6_Output* out, const XL6_AstNode* root, const XL6_AstNode* node)
{
    const XL6_AstNode* typeNode;

    if ( node == NULL || node->text == NULL || node->text[0] == '\0' ) {
        return;
    }
    typeNode = xl6CgenHeaderExternalVarTypeNode(node);
    xl6OutputPuts("extern ", out);
    if ( node->kind == XL6_AST_EXPORT_VAR ||
         node->kind == XL6_AST_EXPORT_CONST ) {
        xl6OutputPuts("XL6_C_API ", out);
    }
    if ( node != NULL && node->kind == XL6_AST_EXPORT_CONST ) {
        xl6OutputPuts("const ", out);
    }
    xl6CgenHeaderPutTypeName(out, root, typeNode);
    xl6OutputPutc(' ', out);
    xl6CgenHeaderCStringSymbol(out, node != NULL ? node->text : NULL);
    xl6OutputPuts(";\n", out);
}

int xl6CgenEmitHeader(XL6_Output* out, XL6_AstNode* root, const char* guardName, XL6_DiagList* diags)
{
    size_t i;
    const char** emittedUnions;
    size_t emittedUnionCount;
    const char** emittedClasses;
    size_t emittedClassCount;
    const char** emittedEnums;
    size_t emittedEnumCount;
    size_t typeCapacity;

    (void)diags;
    if ( out == NULL || root == NULL ) {
        return 0;
    }
    if ( guardName == NULL || guardName[0] == '\0' ) {
        guardName = "XL6_GENERATED_HEADER_H";
    }

    xl6OutputPrintf(out, "#ifndef %s\n", guardName);
    xl6OutputPrintf(out, "#define %s\n\n", guardName);
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stdint.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n\n", out);
    xl6OutputPuts("#ifndef XL6_C_API\n", out);
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("#if defined(XL6_BUILD_SHARED)\n", out);
    xl6OutputPuts("#define XL6_C_API __declspec(dllexport)\n", out);
    xl6OutputPuts("#elif defined(XL6_USE_SHARED)\n", out);
    xl6OutputPuts("#define XL6_C_API __declspec(dllimport)\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("#define XL6_C_API\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#elif defined(__GNUC__) && defined(XL6_BUILD_SHARED)\n", out);
    xl6OutputPuts("#define XL6_C_API __attribute__((visibility(\"default\")))\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("#define XL6_C_API\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#endif\n\n", out);
    xl6OutputPuts("typedef intptr_t xl6_ssize_t;\n\n", out);
	xl6OutputPuts("typedef struct xvalue xvalue;\n", out);
    xl6OutputPuts("#ifndef XL6_XRT_TYPED_CONTAINER_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XRT_TYPED_CONTAINER_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xl6_typed_container* xtarray;\n", out);
    xl6OutputPuts("typedef struct xl6_typed_container* xtlist;\n", out);
    xl6OutputPuts("typedef struct xl6_typed_container* xtset;\n", out);
    xl6OutputPuts("typedef struct xl6_typed_container* xtdict;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef struct xl6_func_sig xl6_func_sig;\n", out);
    xl6OutputPuts("typedef struct xl6_type_desc xl6_type_desc;\n", out);
    xl6OutputPuts("typedef struct xl6_call_frame xl6_call_frame;\n", out);
    xl6OutputPuts("typedef struct xl6_call_result xl6_call_result;\n\n", out);
    xl6OutputPuts("#ifndef XL6_FUNCTION_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_FUNCTION_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_FUNCTION_DEFINED\n", out);
    xl6OutputPuts("typedef void (*XL6_NativeFunction)(void);\n", out);
    xl6OutputPuts("typedef struct XL6_Function {\n", out);
    xl6OutputPuts("    unsigned long long signature;\n", out);
    xl6OutputPuts("    const struct xl6_func_sig* sig;\n", out);
    xl6OutputPuts("    void* env;\n", out);
    xl6OutputPuts("    void* invoke_env;\n", out);
    xl6OutputPuts("    XL6_NativeFunction invoke;\n", out);
    xl6OutputPuts("    XL6_NativeFunction c_entry;\n", out);
    xl6OutputPuts("    xvalue* (*invoke_var)(void* env, int argc, xvalue** argv);\n", out);
    xl6OutputPuts("    bool (*invoke_xcall)(void* env, xl6_call_frame* frame, xl6_call_result* result);\n", out);
    xl6OutputPuts("    void (*retain)(void* env);\n", out);
    xl6OutputPuts("    void (*release)(void* env);\n", out);
    xl6OutputPuts("} XL6_Function;\n", out);
    xl6OutputPuts("#endif\n\n", out);
    if ( !xl6CgenHeaderEmitCallbackTypedefs(out, root) ) {
        return 0;
    }
    xl6OutputPutc('\n', out);
    xl6OutputPuts("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", out);
    xl6CgenHeaderLifecycleDecls(out);

    emittedUnions = NULL;
    emittedClasses = NULL;
    emittedEnums = NULL;
    emittedUnionCount = 0;
    emittedClassCount = 0;
    emittedEnumCount = 0;
    typeCapacity = xl6CgenHeaderAstNodeCount(root);
    if ( typeCapacity == SIZE_MAX ) {
        return 0;
    }
    if ( typeCapacity > 0 ) {
        emittedUnions = (const char**)xrtCalloc(typeCapacity, sizeof(char*));
        emittedClasses = (const char**)xrtCalloc(typeCapacity, sizeof(char*));
        emittedEnums = (const char**)xrtCalloc(typeCapacity, sizeof(char*));
    }
    if ( typeCapacity > 0 &&
         (emittedUnions == NULL || emittedClasses == NULL || emittedEnums == NULL) ) {
        xrtFree(emittedUnions);
        xrtFree(emittedClasses);
        xrtFree(emittedEnums);
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];

        if ( node != NULL &&
             (node->kind == XL6_AST_EXTERN_FUNC ||
              (node->kind == XL6_AST_EXPORT_FUNC &&
               node->text != NULL &&
               node->text[0] != '\0')) ) {
            xl6CgenHeaderCallableDependencyDecls(
                out,
                root,
                node,
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount,
                emittedEnums,
                &emittedEnumCount);
        } else if ( node != NULL && node->kind == XL6_AST_CLASS &&
                    node->childCount > 1 ) {
            const XL6_AstNode* body = node->children[1];
            size_t m;

            for ( m = 0; body != NULL && m < body->childCount; ++m ) {
                const XL6_AstNode* member = body->children[m];

                if ( member != NULL &&
                     member->kind == XL6_AST_EXPORT_FUNC &&
                     member->text != NULL &&
                     member->text[0] != '\0' ) {
                    xl6CgenHeaderCallableDependencyDecls(
                        out,
                        root,
                        member,
                        emittedUnions,
                        &emittedUnionCount,
                        emittedClasses,
                        &emittedClassCount,
                        emittedEnums,
                        &emittedEnumCount);
                }
            }
        } else if ( node != NULL &&
                    (node->kind == XL6_AST_EXTERN_VAR ||
                     node->kind == XL6_AST_EXPORT_VAR ||
                     node->kind == XL6_AST_EXPORT_CONST) ) {
            xl6CgenHeaderTypeDependencyDecl(
                out,
                root,
                xl6CgenHeaderExternalVarTypeNode(node),
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount,
                emittedEnums,
                &emittedEnumCount);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL && node->kind == XL6_AST_UNION ) {
            xl6CgenHeaderUnionDecl(
                out,
                root,
                node,
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount,
                emittedEnums,
                &emittedEnumCount);
        } else if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenHeaderClassDecl(
                out,
                root,
                node,
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount,
                emittedEnums,
                &emittedEnumCount);
        } else if ( node != NULL && node->kind == XL6_AST_ENUM ) {
            xl6CgenHeaderEnumDecl(
                out,
                root,
                node,
                NULL,
                emittedUnions,
                &emittedUnionCount,
                emittedClasses,
                &emittedClassCount,
                emittedEnums,
                &emittedEnumCount);
        }
    }
    xrtFree(emittedUnions);
    xrtFree(emittedClasses);
    for ( i = 0; i < emittedEnumCount; ++i ) {
        xrtFree((void*)emittedEnums[i]);
    }
    xrtFree(emittedEnums);

    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL &&
             (node->kind == XL6_AST_EXTERN_FUNC ||
              (node->kind == XL6_AST_EXPORT_FUNC &&
               node->text != NULL &&
               node->text[0] != '\0')) ) {
            xl6CgenHeaderExportFuncProto(out, root, node);
        } else if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenHeaderClassExportFuncProtos(out, root, node);
        }
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        if ( node != NULL &&
             (node->kind == XL6_AST_EXTERN_VAR ||
              node->kind == XL6_AST_EXPORT_VAR ||
              node->kind == XL6_AST_EXPORT_CONST) &&
             node->text != NULL && node->text[0] != '\0' ) {
            xl6CgenHeaderExportVarDecl(out, root, node);
        }
    }

    xl6OutputPuts("\n#ifdef __cplusplus\n}\n#endif\n\n", out);
    xl6OutputPrintf(out, "#endif /* %s */\n", guardName);
    return 1;
}
