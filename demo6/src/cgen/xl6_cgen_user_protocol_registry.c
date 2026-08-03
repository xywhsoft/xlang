static void xl6CgenUserProtocolDefinitions(XL6_CgenContext* ctx)
{
    size_t protocolIndex;
    size_t classIndex;
    int witnessCount = 0;

    if ( !xl6CgenHasUserProtocols(ctx) ) {
        return;
    }

    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        const XL6_AstNode* body;
        size_t requirementIndex;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        body = xl6CgenProtocolBody(protocolNode);
        for ( classIndex = 0;
              classIndex < ctx->root->childCount;
              ++classIndex ) {
            const XL6_AstNode* classNode = ctx->root->children[classIndex];

            if ( classNode == NULL ||
                 classNode->kind != XL6_AST_CLASS ||
                 !xl6CgenClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     protocolNode) ) {
                continue;
            }
            for ( requirementIndex = 0;
                  body != NULL && requirementIndex < body->childCount;
                  ++requirementIndex ) {
                const XL6_AstNode* requirement =
                    body->children[requirementIndex];
                const XL6_AstNode* ownerClass = NULL;
                const XL6_AstNode* candidate =
                    xl6CgenFindProtocolWitnessMember(
                        ctx,
                        classNode,
                        requirement,
                        &ownerClass);

                if ( candidate != NULL && ownerClass != NULL ) {
                    xl6CgenProtocolThunk(
                        ctx,
                        classNode,
                        protocolNode,
                        requirement,
                        ownerClass,
                        candidate);
                }
            }
        }
        for ( requirementIndex = 0;
              body != NULL && requirementIndex < body->childCount;
              ++requirementIndex ) {
            xl6CgenProtocolDispatchFunction(
                ctx,
                protocolNode,
                body->children[requirementIndex]);
        }
    }

    /*
        元数据对象均为静态存储期。XRT 注册表只借用这些对象，
        模块卸载前必须先注销 witness。
    */
    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        const XL6_AstNode* body;
        XL6_AstNode protocolType;
        unsigned long long protocolTypeId;
        size_t requirementIndex;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        memset(&protocolType, 0, sizeof(protocolType));
        protocolType.kind = XL6_AST_TYPE;
        protocolType.text = protocolNode->text;
        protocolTypeId = xl6CgenTypeIdForTypeNode(ctx, &protocolType);
        body = xl6CgenProtocolBody(protocolNode);

        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenProtocolTypeDescSymbol(ctx->out, protocolNode);
        xl6OutputPuts("(void)\n{\n", ctx->out);
        xl6OutputPuts("    static xrt_type_desc desc = {\n", ctx->out);
        xl6OutputPrintf(
            ctx->out,
            "        0x%08XU, XL6_XRT_TYPE_KIND_PROTOCOL,\n",
            (unsigned int)(protocolTypeId & 0xFFFFFFFFULL));
        xl6OutputPuts("        ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, protocolNode->text);
        xl6OutputPrintf(
            ctx->out,
            ", %uu,\n"
            "        \"xvalue\", 6u, sizeof(void*), sizeof(void*),\n"
            "        0, 0, 0, 0\n"
            "    };\n"
            "    const xrt_type_desc* valueType = xrtTypeValue();\n"
            "    if (desc.Ops == 0 && valueType != 0) { desc.Ops = valueType->Ops; }\n"
            "    return &desc;\n"
            "}\n\n",
            (unsigned)strlen(protocolNode->text));

        for ( requirementIndex = 0;
              body != NULL && requirementIndex < body->childCount;
              ++requirementIndex ) {
            const XL6_AstNode* requirement =
                body->children[requirementIndex];
            char sigName[256];

            snprintf(
                sigName,
                sizeof(sigName),
                "xl6_protocol_sig_%u_%u",
                (unsigned)protocolIndex,
                (unsigned)requirementIndex);
            if ( xl6CgenCallableParams(requirement) != NULL &&
                 xl6CgenCallableParams(requirement)->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    "static xrt_func_param %s_params[%uu];\n",
                    sigName,
                    (unsigned)xl6CgenCallableParams(requirement)->childCount);
            }
            if ( requirement->childCount > 2 &&
                 requirement->children[2] != NULL &&
                 requirement->children[2]->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    "static const xrt_type_desc* %s_returns[%uu];\n",
                    sigName,
                    (unsigned)requirement->children[2]->childCount);
            }
            xl6OutputPrintf(
                ctx->out,
                "static xrt_func_sig %s;\n",
                sigName);
        }
        if ( body != NULL && body->childCount > 0 ) {
            xl6OutputPrintf(
                ctx->out,
                "static xrtprotocolrequirement "
                "xl6_protocol_requirements_%u[%uu];\n",
                (unsigned)protocolIndex,
                (unsigned)body->childCount);
        }
        xl6OutputPrintf(
            ctx->out,
            "static xrtprotocol xl6_protocol_%u;\n\n",
            (unsigned)protocolIndex);

        for ( classIndex = 0;
              classIndex < ctx->root->childCount;
              ++classIndex ) {
            const XL6_AstNode* classNode = ctx->root->children[classIndex];

            if ( classNode == NULL ||
                 classNode->kind != XL6_AST_CLASS ||
                 !xl6CgenClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     protocolNode) ) {
                continue;
            }
            if ( body != NULL && body->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    "static xrtprotocolentry "
                    "xl6_protocol_entries_%u_%u[%uu];\n",
                    (unsigned)classIndex,
                    (unsigned)protocolIndex,
                    (unsigned)body->childCount);
            }
            xl6OutputPuts("static xrtprotocolwitness ", ctx->out);
            xl6CgenProtocolWitnessSymbol(
                ctx,
                classNode,
                protocolNode);
            xl6OutputPuts(";\n", ctx->out);
            witnessCount += 1;
        }
        xl6OutputPutc('\n', ctx->out);
    }

    xl6OutputPuts("static int xl6_protocol_module_init(void)\n{\n", ctx->out);
    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        const XL6_AstNode* body;
        size_t requirementIndex;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        body = xl6CgenProtocolBody(protocolNode);
        for ( requirementIndex = 0;
              body != NULL && requirementIndex < body->childCount;
              ++requirementIndex ) {
            const XL6_AstNode* requirement =
                body->children[requirementIndex];
            const XL6_AstNode* params =
                xl6CgenCallableParams(requirement);
            const XL6_AstNode* returns =
                requirement->childCount > 2
                    ? requirement->children[2]
                    : NULL;
            unsigned long long signatureId =
                xl6CgenProtocolRequirementSignatureId(ctx, requirement);
            char sigName[256];
            char paramsName[300];
            char returnsName[300];
            char runtimeName[384];
            size_t itemIndex;

            snprintf(
                sigName,
                sizeof(sigName),
                "xl6_protocol_sig_%u_%u",
                (unsigned)protocolIndex,
                (unsigned)requirementIndex);
            snprintf(
                paramsName,
                sizeof(paramsName),
                "%s_params",
                sigName);
            snprintf(
                returnsName,
                sizeof(returnsName),
                "%s_returns",
                sigName);
            for ( itemIndex = 0;
                  params != NULL && itemIndex < params->childCount;
                  ++itemIndex ) {
                const XL6_AstNode* param = params->children[itemIndex];
                xl6OutputPrintf(
                    ctx->out,
                    "    %s_params[%uu].Name = ",
                    sigName,
                    (unsigned)itemIndex);
                xl6CgenCStringLiteral(
                    ctx->out,
                    param != NULL && param->text != NULL
                        ? param->text
                        : "");
                xl6OutputPrintf(
                    ctx->out,
                    ";\n    %s_params[%uu].NameSize = %uu;\n"
                    "    %s_params[%uu].Type = ",
                    sigName,
                    (unsigned)itemIndex,
                    (unsigned)strlen(
                        param != NULL && param->text != NULL
                            ? param->text
                            : ""),
                    sigName,
                    (unsigned)itemIndex);
                xl6CgenEmitXrtTypeDescRef(
                    ctx,
                    param != NULL && param->childCount > 0
                        ? param->children[0]
                        : NULL);
                xl6OutputPrintf(
                    ctx->out,
                    ";\n    %s_params[%uu].DefaultValue = 0;\n"
                    "    %s_params[%uu].Flags = 0u;\n",
                    sigName,
                    (unsigned)itemIndex,
                    sigName,
                    (unsigned)itemIndex);
            }
            for ( itemIndex = 0;
                  returns != NULL && itemIndex < returns->childCount;
                  ++itemIndex ) {
                xl6OutputPrintf(
                    ctx->out,
                    "    %s_returns[%uu] = ",
                    sigName,
                    (unsigned)itemIndex);
                xl6CgenEmitXrtTypeDescRef(
                    ctx,
                    returns->children[itemIndex]);
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6OutputPrintf(
                ctx->out,
                "    %s.TypeId = 0x%016llXULL;\n"
                "    %s.Name = ",
                sigName,
                signatureId,
                sigName);
            xl6CgenCStringLiteral(
                ctx->out,
                xl6CgenProtocolRequirementName(
                    requirement,
                    runtimeName,
                    sizeof(runtimeName)));
            xl6OutputPrintf(
                ctx->out,
                ";\n    %s.NameSize = %uu;\n"
                "    %s.ParamCount = %uu;\n"
                "    %s.Params = %s;\n"
                "    %s.ReturnCount = %uu;\n"
                "    %s.ReturnTypes = %s;\n"
                "    %s.Flags = 0u;\n",
                sigName,
                (unsigned)strlen(runtimeName),
                sigName,
                (unsigned)(params != NULL ? params->childCount : 0),
                sigName,
                params != NULL && params->childCount > 0
                    ? paramsName
                    : "0",
                sigName,
                (unsigned)(returns != NULL ? returns->childCount : 0),
                sigName,
                returns != NULL && returns->childCount > 0
                    ? returnsName
                    : "0",
                sigName);
            xl6OutputPrintf(
                ctx->out,
                "    xl6_protocol_requirements_%u[%uu].Name = ",
                (unsigned)protocolIndex,
                (unsigned)requirementIndex);
            xl6CgenCStringLiteral(ctx->out, runtimeName);
            xl6OutputPrintf(
                ctx->out,
                ";\n    xl6_protocol_requirements_%u[%uu].NameSize = %uu;\n"
                "    xl6_protocol_requirements_%u[%uu].Signature = &%s;\n",
                (unsigned)protocolIndex,
                (unsigned)requirementIndex,
                (unsigned)strlen(runtimeName),
                (unsigned)protocolIndex,
                (unsigned)requirementIndex,
                sigName);
        }
        xl6OutputPrintf(
            ctx->out,
            "    xl6_protocol_%u.Type = ",
            (unsigned)protocolIndex);
        xl6CgenProtocolTypeDescSymbol(ctx->out, protocolNode);
        xl6OutputPrintf(
            ctx->out,
            "();\n"
            "    xl6_protocol_%u.RequirementCount = %uu;\n"
            "    xl6_protocol_%u.Requirements = ",
            (unsigned)protocolIndex,
            (unsigned)(body != NULL ? body->childCount : 0),
            (unsigned)protocolIndex);
        if ( body != NULL && body->childCount > 0 ) {
            xl6OutputPrintf(
                ctx->out,
                "xl6_protocol_requirements_%u;\n",
                (unsigned)protocolIndex);
        } else {
            xl6OutputPuts("0;\n", ctx->out);
        }

        for ( classIndex = 0;
              classIndex < ctx->root->childCount;
              ++classIndex ) {
            const XL6_AstNode* classNode = ctx->root->children[classIndex];

            if ( classNode == NULL ||
                 classNode->kind != XL6_AST_CLASS ||
                 !xl6CgenClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     protocolNode) ) {
                continue;
            }
            for ( requirementIndex = 0;
                  body != NULL && requirementIndex < body->childCount;
                  ++requirementIndex ) {
                const XL6_AstNode* requirement =
                    body->children[requirementIndex];
                char runtimeName[384];

                xl6OutputPrintf(
                    ctx->out,
                    "    xl6_protocol_entries_%u_%u[%uu].Name = ",
                    (unsigned)classIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex);
                xl6CgenCStringLiteral(
                    ctx->out,
                    xl6CgenProtocolRequirementName(
                        requirement,
                        runtimeName,
                        sizeof(runtimeName)));
                xl6OutputPrintf(
                    ctx->out,
                    ";\n"
                    "    xl6_protocol_entries_%u_%u[%uu].NameSize = %uu;\n"
                    "    xl6_protocol_entries_%u_%u[%uu].Signature = "
                    "&xl6_protocol_sig_%u_%u;\n"
                    "    xl6_protocol_entries_%u_%u[%uu].Entry = (ptr)",
                    (unsigned)classIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex,
                    (unsigned)strlen(runtimeName),
                    (unsigned)classIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex,
                    (unsigned)classIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex);
                xl6CgenProtocolThunkSymbol(
                    ctx,
                    classNode,
                    protocolNode,
                    requirement);
                xl6OutputPuts(";\n", ctx->out);
            }
            xl6OutputPuts("    ", ctx->out);
            xl6CgenProtocolWitnessSymbol(
                ctx,
                classNode,
                protocolNode);
            xl6OutputPrintf(
                ctx->out,
                ".Protocol = &xl6_protocol_%u;\n    ",
                (unsigned)protocolIndex);
            xl6CgenProtocolWitnessSymbol(
                ctx,
                classNode,
                protocolNode);
            xl6OutputPuts(".ConcreteType = ", ctx->out);
            {
                char classTypeName[512];
                xl6CgenClassTypeRefName(
                    classTypeName,
                    sizeof(classTypeName),
                    classNode);
                xl6CgenTypeDescSymbol(ctx->out, classTypeName);
                xl6OutputPuts("();\n", ctx->out);
            }
            xl6OutputPuts("    ", ctx->out);
            xl6CgenProtocolWitnessSymbol(
                ctx,
                classNode,
                protocolNode);
            xl6OutputPrintf(
                ctx->out,
                ".EntryCount = %uu;\n    ",
                (unsigned)(body != NULL ? body->childCount : 0));
            xl6CgenProtocolWitnessSymbol(
                ctx,
                classNode,
                protocolNode);
            if ( body != NULL && body->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    ".Entries = xl6_protocol_entries_%u_%u;\n",
                    (unsigned)classIndex,
                    (unsigned)protocolIndex);
            } else {
                xl6OutputPuts(".Entries = 0;\n", ctx->out);
            }
            xl6OutputPuts("    if ( !xrtProtocolRegister(&", ctx->out);
            xl6CgenProtocolWitnessSymbol(
                ctx,
                classNode,
                protocolNode);
            xl6OutputPuts(") ) { goto fail; }\n", ctx->out);
        }
    }
    xl6OutputPuts("    return 1;\n", ctx->out);
    if ( witnessCount > 0 ) {
        xl6OutputPuts("fail:\n", ctx->out);
        for ( protocolIndex = 0;
              protocolIndex < ctx->root->childCount;
              ++protocolIndex ) {
            const XL6_AstNode* protocolNode =
                ctx->root->children[protocolIndex];
            if ( protocolNode == NULL ||
                 protocolNode->kind != XL6_AST_PROTOCOL ||
                 xl6CgenProtocolIsGeneric(protocolNode) ) {
                continue;
            }
            for ( classIndex = 0;
                  classIndex < ctx->root->childCount;
                  ++classIndex ) {
                const XL6_AstNode* classNode =
                    ctx->root->children[classIndex];
                if ( classNode != NULL &&
                     classNode->kind == XL6_AST_CLASS &&
                     xl6CgenClassSatisfiesProtocol(
                         ctx,
                         classNode,
                         protocolNode) ) {
                    xl6OutputPuts(
                        "    (void)xrtProtocolUnregister(&",
                        ctx->out);
                    xl6CgenProtocolWitnessSymbol(
                        ctx,
                        classNode,
                        protocolNode);
                    xl6OutputPuts(");\n", ctx->out);
                }
            }
        }
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static void xl6_protocol_module_unit(void)\n{\n", ctx->out);
    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode =
            ctx->root->children[protocolIndex];
        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        for ( classIndex = 0;
              classIndex < ctx->root->childCount;
              ++classIndex ) {
            const XL6_AstNode* classNode =
                ctx->root->children[classIndex];
            if ( classNode != NULL &&
                 classNode->kind == XL6_AST_CLASS &&
                 xl6CgenClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     protocolNode) ) {
                xl6OutputPuts(
                    "    (void)xrtProtocolUnregister(&",
                    ctx->out);
                xl6CgenProtocolWitnessSymbol(
                    ctx,
                    classNode,
                    protocolNode);
                xl6OutputPuts(");\n", ctx->out);
            }
        }
    }
    xl6OutputPuts("}\n\n", ctx->out);

    xl6CgenUserGenericProtocolDefinitions(ctx);
    xl6CgenUserGenericClassProtocolDefinitions(ctx);
}

static void xl6CgenEmitProtocolModuleInit(XL6_CgenContext* ctx)
{
    if ( xl6CgenHasUserProtocols(ctx) ) {
        xl6OutputPuts(
            "    if ( !xl6_protocol_module_init() ) {\n"
            "        xl6_error_raise(\"RuntimeError\", "
            "\"protocol witness registration failed\", \"\", 0, 0);\n"
            "        goto __xl6_module_init_error;\n"
            "    }\n"
            "    if ( !xl6_generic_class_protocol_module_init() ) {\n"
            "        xl6_error_raise(\"RuntimeError\", "
            "\"generic class protocol witness registration failed\", \"\", 0, 0);\n"
            "        goto __xl6_module_init_error;\n"
            "    }\n"
            "    if ( !xl6_generic_protocol_module_init() ) {\n"
            "        xl6_error_raise(\"RuntimeError\", "
            "\"generic protocol witness registration failed\", \"\", 0, 0);\n"
            "        goto __xl6_module_init_error;\n"
            "    }\n",
            ctx->out);
    }
}

static void xl6CgenEmitProtocolModuleUnit(XL6_CgenContext* ctx)
{
    if ( xl6CgenHasUserProtocols(ctx) ) {
        xl6OutputPuts(
            "    xl6_generic_protocol_module_unit();\n"
            "    xl6_generic_class_protocol_module_unit();\n"
            "    xl6_protocol_module_unit();\n",
            ctx->out);
    }
}
