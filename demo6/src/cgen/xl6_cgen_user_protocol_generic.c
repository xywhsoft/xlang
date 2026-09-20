static void xl6CgenUserGenericProtocolDefinitions(XL6_CgenContext* ctx)
{
    size_t instanceIndex;
    size_t classIndex;
    int witnessCount = 0;

    if ( ctx == NULL || ctx->genericProtocolInstanceCount == 0 ) {
        xl6OutputPuts(
            "static int xl6_generic_protocol_module_init(void) { return 1; }\n"
            "static void xl6_generic_protocol_module_unit(void) { }\n\n",
            ctx->out);
        return;
    }

    /*
        先生成 concrete requirement 的 thunk 和派发函数。
        第一层只接受非泛型 class；泛型 class 在其单态实例路径中另行接入。
    */
    for ( instanceIndex = 0;
          instanceIndex < ctx->genericProtocolInstanceCount;
          ++instanceIndex ) {
        const XL6_CgenGenericProtocolInstance* instance =
            &ctx->genericProtocolInstances[instanceIndex];
        const XL6_AstNode* body =
            xl6CgenProtocolBody(instance->protocolNode);
        size_t requirementIndex;

        for ( classIndex = 0;
              classIndex < xl6CgenProtocolClassCandidateCount(ctx);
              ++classIndex ) {
            const XL6_AstNode* classNode;
            const XL6_AstNode* classType;
            XL6_AstNode classTypeStorage;

            if ( !xl6CgenProtocolClassCandidate(
                     ctx,
                     classIndex,
                     &classNode,
                     &classType,
                     &classTypeStorage) ||
                 !xl6CgenConcreteClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     classType,
                     instance->protocolNode,
                     instance->typeNode) ) {
                continue;
            }
            for ( requirementIndex = 0;
                  body != NULL && requirementIndex < body->childCount;
                  ++requirementIndex ) {
                XL6_AstNode* requirement =
                    xl6CgenConcreteProtocolRequirement(
                        ctx,
                        instance->protocolNode,
                        instance->typeNode,
                        body->children[requirementIndex]);
                const XL6_AstNode* ownerClass = NULL;
                const XL6_AstNode* candidate = requirement != NULL ?
                    xl6CgenFindConcreteProtocolWitnessMember(
                        ctx,
                        classNode,
                        classType,
                        requirement,
                        &ownerClass) : NULL;

                if ( candidate != NULL && ownerClass != NULL ) {
                    xl6CgenGenericProtocolThunk(
                        ctx,
                        classNode,
                        classType,
                        instance->typeNode,
                        requirement,
                        ownerClass,
                        candidate);
                }
                xl6AstFree(requirement);
            }
        }
        for ( requirementIndex = 0;
              body != NULL && requirementIndex < body->childCount;
              ++requirementIndex ) {
            XL6_AstNode* requirement =
                xl6CgenConcreteProtocolRequirement(
                    ctx,
                    instance->protocolNode,
                    instance->typeNode,
                    body->children[requirementIndex]);
            if ( requirement != NULL ) {
                xl6CgenGenericProtocolDispatchFunction(
                    ctx,
                    instance->typeNode,
                    requirement);
            }
            xl6AstFree(requirement);
        }
    }

    /* 静态元数据对象由模块 init 填充，注册表只借用其地址。 */
    for ( instanceIndex = 0;
          instanceIndex < ctx->genericProtocolInstanceCount;
          ++instanceIndex ) {
        const XL6_CgenGenericProtocolInstance* instance =
            &ctx->genericProtocolInstances[instanceIndex];
        const XL6_AstNode* body =
            xl6CgenProtocolBody(instance->protocolNode);

        xl6CgenGenericProtocolDescriptor(ctx, instance->typeNode);
        if ( body != NULL && body->childCount > 0 ) {
            xl6OutputPrintf(
                ctx->out,
                "static xl6_protocol_requirement "
                "xl6_generic_protocol_requirements_%u[%uu];\n",
                (unsigned)instanceIndex,
                (unsigned)body->childCount);
        }
        xl6OutputPrintf(
            ctx->out,
            "static xl6_protocol xl6_generic_protocol_%u;\n",
            (unsigned)instanceIndex);

        for ( classIndex = 0;
              classIndex < xl6CgenProtocolClassCandidateCount(ctx);
              ++classIndex ) {
            const XL6_AstNode* classNode;
            const XL6_AstNode* classType;
            XL6_AstNode classTypeStorage;

            if ( !xl6CgenProtocolClassCandidate(
                     ctx,
                     classIndex,
                     &classNode,
                     &classType,
                     &classTypeStorage) ||
                 !xl6CgenConcreteClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     classType,
                     instance->protocolNode,
                     instance->typeNode) ) {
                continue;
            }
            if ( body != NULL && body->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    "static xl6_protocol_entry "
                    "xl6_generic_protocol_entries_%u_%u[%uu];\n",
                    (unsigned)classIndex,
                    (unsigned)instanceIndex,
                    (unsigned)body->childCount);
            }
            xl6OutputPuts("static xl6_protocol_witness ", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPuts(";\n", ctx->out);
            witnessCount += 1;
        }
        xl6OutputPutc('\n', ctx->out);
    }

    xl6OutputPuts(
        "static int xl6_generic_protocol_module_init(void)\n{\n",
        ctx->out);
    for ( instanceIndex = 0;
          instanceIndex < ctx->genericProtocolInstanceCount;
          ++instanceIndex ) {
        const XL6_CgenGenericProtocolInstance* instance =
            &ctx->genericProtocolInstances[instanceIndex];
        const XL6_AstNode* body =
            xl6CgenProtocolBody(instance->protocolNode);
        size_t requirementIndex;

        for ( requirementIndex = 0;
              body != NULL && requirementIndex < body->childCount;
              ++requirementIndex ) {
            XL6_AstNode* requirement =
                xl6CgenConcreteProtocolRequirement(
                    ctx,
                    instance->protocolNode,
                    instance->typeNode,
                    body->children[requirementIndex]);
            const XL6_AstNode* params;
            const XL6_AstNode* returns;
            unsigned long long signatureId;
            char sigName[128];
            char runtimeName[384];

            if ( requirement == NULL ) {
                continue;
            }
            params = xl6CgenCallableParams(requirement);
            returns = requirement->childCount > 2 ?
                requirement->children[2] : NULL;
            signatureId =
                xl6CgenProtocolRequirementSignatureId(ctx, requirement);
            snprintf(
                sigName,
                sizeof(sigName),
                "xl6_generic_protocol_sig_%u_%u",
                (unsigned)instanceIndex,
                (unsigned)requirementIndex);
            xl6CgenEmitFunctionSigObject(
                ctx,
                sigName,
                xl6CgenProtocolRequirementName(
                    requirement,
                    runtimeName,
                    sizeof(runtimeName)),
                signatureId,
                params,
                returns);
            xl6OutputPrintf(
                ctx->out,
                "    xl6_generic_protocol_requirements_%u[%uu].Name = ",
                (unsigned)instanceIndex,
                (unsigned)requirementIndex);
            xl6CgenCStringLiteral(ctx->out, runtimeName);
            xl6OutputPrintf(
                ctx->out,
                ";\n"
                "    xl6_generic_protocol_requirements_%u[%uu].NameSize = %uu;\n"
                "    xl6_generic_protocol_requirements_%u[%uu].Signature = &%s;\n",
                (unsigned)instanceIndex,
                (unsigned)requirementIndex,
                (unsigned)strlen(runtimeName),
                (unsigned)instanceIndex,
                (unsigned)requirementIndex,
                sigName);
            xl6AstFree(requirement);
        }
        xl6OutputPrintf(
            ctx->out,
            "    xl6_generic_protocol_%u.Type = ",
            (unsigned)instanceIndex);
        xl6CgenGenericProtocolTypeDescSymbol(ctx, instance->typeNode);
        xl6OutputPrintf(
            ctx->out,
            "();\n"
            "    xl6_generic_protocol_%u.RequirementCount = %uu;\n"
            "    xl6_generic_protocol_%u.Requirements = ",
            (unsigned)instanceIndex,
            (unsigned)(body != NULL ? body->childCount : 0),
            (unsigned)instanceIndex);
        if ( body != NULL && body->childCount > 0 ) {
            xl6OutputPrintf(
                ctx->out,
                "xl6_generic_protocol_requirements_%u;\n",
                (unsigned)instanceIndex);
        } else {
            xl6OutputPuts("0;\n", ctx->out);
        }

        for ( classIndex = 0;
              classIndex < xl6CgenProtocolClassCandidateCount(ctx);
              ++classIndex ) {
            const XL6_AstNode* classNode;
            const XL6_AstNode* classType;
            XL6_AstNode classTypeStorage;

            if ( !xl6CgenProtocolClassCandidate(
                     ctx,
                     classIndex,
                     &classNode,
                     &classType,
                     &classTypeStorage) ||
                 !xl6CgenConcreteClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     classType,
                     instance->protocolNode,
                     instance->typeNode) ) {
                continue;
            }
            for ( requirementIndex = 0;
                  body != NULL && requirementIndex < body->childCount;
                  ++requirementIndex ) {
                XL6_AstNode* requirement =
                    xl6CgenConcreteProtocolRequirement(
                        ctx,
                        instance->protocolNode,
                        instance->typeNode,
                        body->children[requirementIndex]);
                char runtimeName[384];

                if ( requirement == NULL ) {
                    continue;
                }
                xl6OutputPrintf(
                    ctx->out,
                    "    xl6_generic_protocol_entries_%u_%u[%uu].Name = ",
                    (unsigned)classIndex,
                    (unsigned)instanceIndex,
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
                    "    xl6_generic_protocol_entries_%u_%u[%uu].NameSize = %uu;\n"
                    "    xl6_generic_protocol_entries_%u_%u[%uu].Signature = "
                    "&xl6_generic_protocol_sig_%u_%u;\n"
                    "    xl6_generic_protocol_entries_%u_%u[%uu].Entry = (ptr)",
                    (unsigned)classIndex,
                    (unsigned)instanceIndex,
                    (unsigned)requirementIndex,
                    (unsigned)strlen(runtimeName),
                    (unsigned)classIndex,
                    (unsigned)instanceIndex,
                    (unsigned)requirementIndex,
                    (unsigned)instanceIndex,
                    (unsigned)requirementIndex,
                    (unsigned)classIndex,
                    (unsigned)instanceIndex,
                    (unsigned)requirementIndex);
                xl6CgenGenericProtocolThunkSymbol(
                    ctx,
                    classType,
                    instance->typeNode,
                    requirement);
                xl6OutputPuts(";\n", ctx->out);
                xl6AstFree(requirement);
            }
            xl6OutputPuts("    ", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPrintf(
                ctx->out,
                ".Protocol = &xl6_generic_protocol_%u;\n    ",
                (unsigned)instanceIndex);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPuts(".ConcreteType = ", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, classType);
            xl6OutputPuts(";\n", ctx->out);
            xl6OutputPuts("    ", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPrintf(
                ctx->out,
                ".EntryCount = %uu;\n    ",
                (unsigned)(body != NULL ? body->childCount : 0));
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            if ( body != NULL && body->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    ".Entries = xl6_generic_protocol_entries_%u_%u;\n",
                    (unsigned)classIndex,
                    (unsigned)instanceIndex);
            } else {
                xl6OutputPuts(".Entries = 0;\n", ctx->out);
            }
            xl6OutputPuts("    if (!xl6ProtocolRegister(&", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPuts(")) { goto fail; }\n", ctx->out);
        }
    }
    xl6OutputPuts("    return 1;\nfail:\n", ctx->out);
    for ( instanceIndex = 0;
          instanceIndex < ctx->genericProtocolInstanceCount;
          ++instanceIndex ) {
        const XL6_CgenGenericProtocolInstance* instance =
            &ctx->genericProtocolInstances[instanceIndex];
        for ( classIndex = 0;
              classIndex < xl6CgenProtocolClassCandidateCount(ctx);
              ++classIndex ) {
            const XL6_AstNode* classNode;
            const XL6_AstNode* classType;
            XL6_AstNode classTypeStorage;
            if ( !xl6CgenProtocolClassCandidate(
                     ctx,
                     classIndex,
                     &classNode,
                     &classType,
                     &classTypeStorage) ||
                 !xl6CgenConcreteClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     classType,
                     instance->protocolNode,
                     instance->typeNode) ) {
                continue;
            }
            xl6OutputPuts("    (void)xl6ProtocolUnregister(&", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPuts(");\n", ctx->out);
        }
    }
    xl6OutputPuts("    return 0;\n}\n\n", ctx->out);

    xl6OutputPuts(
        "static void xl6_generic_protocol_module_unit(void)\n{\n",
        ctx->out);
    if ( witnessCount == 0 ) {
        xl6OutputPuts("    return;\n", ctx->out);
    }
    for ( instanceIndex = 0;
          instanceIndex < ctx->genericProtocolInstanceCount;
          ++instanceIndex ) {
        const XL6_CgenGenericProtocolInstance* instance =
            &ctx->genericProtocolInstances[instanceIndex];
        for ( classIndex = 0;
              classIndex < xl6CgenProtocolClassCandidateCount(ctx);
              ++classIndex ) {
            const XL6_AstNode* classNode;
            const XL6_AstNode* classType;
            XL6_AstNode classTypeStorage;
            if ( !xl6CgenProtocolClassCandidate(
                     ctx,
                     classIndex,
                     &classNode,
                     &classType,
                     &classTypeStorage) ||
                 !xl6CgenConcreteClassSatisfiesProtocol(
                     ctx,
                     classNode,
                     classType,
                     instance->protocolNode,
                     instance->typeNode) ) {
                continue;
            }
            xl6OutputPuts("    (void)xl6ProtocolUnregister(&", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                classType,
                instance->typeNode);
            xl6OutputPuts(");\n", ctx->out);
        }
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

/*
    非泛型 protocol 的描述对象只有一份。
    泛型 class 单态实例仅生成自己的 thunk 和 witness，避免复制协议元数据。
*/
static void xl6CgenUserGenericClassProtocolDefinitions(XL6_CgenContext* ctx)
{
    size_t protocolIndex;
    size_t instanceIndex;
    int witnessCount = 0;

    if ( ctx == NULL || ctx->genericClassInstanceCount == 0 ) {
        xl6OutputPuts(
            "static int xl6_generic_class_protocol_module_init(void) { return 1; }\n"
            "static void xl6_generic_class_protocol_module_unit(void) { }\n\n",
            ctx->out);
        return;
    }

    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        const XL6_AstNode* body;
        XL6_AstNode protocolType;
        size_t requirementIndex;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        memset(&protocolType, 0, sizeof(protocolType));
        protocolType.kind = XL6_AST_TYPE;
        protocolType.text = protocolNode->text;
        body = xl6CgenProtocolBody(protocolNode);

        for ( instanceIndex = 0;
              instanceIndex < ctx->genericClassInstanceCount;
              ++instanceIndex ) {
            const XL6_CgenGenericClassInstance* instance =
                &ctx->genericClassInstances[instanceIndex];

            if ( !xl6CgenConcreteClassSatisfiesProtocol(
                    ctx,
                    instance->classNode,
                    instance->typeNode,
                    protocolNode,
                    NULL) ) {
                continue;
            }
            for ( requirementIndex = 0;
                  body != NULL && requirementIndex < body->childCount;
                  ++requirementIndex ) {
                const XL6_AstNode* requirement =
                    body->children[requirementIndex];
                const XL6_AstNode* ownerClass = NULL;
                const XL6_AstNode* candidate =
                    xl6CgenFindConcreteProtocolWitnessMember(
                        ctx,
                        instance->classNode,
                        instance->typeNode,
                        requirement,
                        &ownerClass);

                if ( candidate != NULL && ownerClass != NULL ) {
                    xl6CgenGenericProtocolThunk(
                        ctx,
                        instance->classNode,
                        instance->typeNode,
                        &protocolType,
                        requirement,
                        ownerClass,
                        candidate);
                }
            }
        }
    }

    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        const XL6_AstNode* body;
        XL6_AstNode protocolType;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        memset(&protocolType, 0, sizeof(protocolType));
        protocolType.kind = XL6_AST_TYPE;
        protocolType.text = protocolNode->text;
        body = xl6CgenProtocolBody(protocolNode);

        for ( instanceIndex = 0;
              instanceIndex < ctx->genericClassInstanceCount;
              ++instanceIndex ) {
            const XL6_CgenGenericClassInstance* instance =
                &ctx->genericClassInstances[instanceIndex];

            if ( !xl6CgenConcreteClassSatisfiesProtocol(
                    ctx,
                    instance->classNode,
                    instance->typeNode,
                    protocolNode,
                    NULL) ) {
                continue;
            }
            if ( body != NULL && body->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    "static xl6_protocol_entry "
                    "xl6_generic_class_protocol_entries_%u_%u[%uu];\n",
                    (unsigned)instanceIndex,
                    (unsigned)protocolIndex,
                    (unsigned)body->childCount);
            }
            xl6OutputPuts("static xl6_protocol_witness ", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
            xl6OutputPuts(";\n", ctx->out);
            witnessCount += 1;
        }
    }
    xl6OutputPutc('\n', ctx->out);

    xl6OutputPuts(
        "static int xl6_generic_class_protocol_module_init(void)\n{\n",
        ctx->out);
    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        const XL6_AstNode* body;
        XL6_AstNode protocolType;
        size_t requirementIndex;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        memset(&protocolType, 0, sizeof(protocolType));
        protocolType.kind = XL6_AST_TYPE;
        protocolType.text = protocolNode->text;
        body = xl6CgenProtocolBody(protocolNode);

        for ( instanceIndex = 0;
              instanceIndex < ctx->genericClassInstanceCount;
              ++instanceIndex ) {
            const XL6_CgenGenericClassInstance* instance =
                &ctx->genericClassInstances[instanceIndex];

            if ( !xl6CgenConcreteClassSatisfiesProtocol(
                    ctx,
                    instance->classNode,
                    instance->typeNode,
                    protocolNode,
                    NULL) ) {
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
                    "    xl6_generic_class_protocol_entries_%u_%u[%uu].Name = ",
                    (unsigned)instanceIndex,
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
                    "    xl6_generic_class_protocol_entries_%u_%u[%uu].NameSize = %uu;\n"
                    "    xl6_generic_class_protocol_entries_%u_%u[%uu].Signature = "
                    "&xl6_protocol_sig_%u_%u;\n"
                    "    xl6_generic_class_protocol_entries_%u_%u[%uu].Entry = (ptr)",
                    (unsigned)instanceIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex,
                    (unsigned)strlen(runtimeName),
                    (unsigned)instanceIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex,
                    (unsigned)instanceIndex,
                    (unsigned)protocolIndex,
                    (unsigned)requirementIndex);
                xl6CgenGenericProtocolThunkSymbol(
                    ctx,
                    instance->typeNode,
                    &protocolType,
                    requirement);
                xl6OutputPuts(";\n", ctx->out);
            }

            xl6OutputPuts("    ", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
            xl6OutputPrintf(
                ctx->out,
                ".Protocol = &xl6_protocol_%u;\n    ",
                (unsigned)protocolIndex);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
            xl6OutputPuts(".ConcreteType = ", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, instance->typeNode);
            xl6OutputPuts(";\n    ", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
            xl6OutputPrintf(
                ctx->out,
                ".EntryCount = %uu;\n    ",
                (unsigned)(body != NULL ? body->childCount : 0));
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
            if ( body != NULL && body->childCount > 0 ) {
                xl6OutputPrintf(
                    ctx->out,
                    ".Entries = xl6_generic_class_protocol_entries_%u_%u;\n",
                    (unsigned)instanceIndex,
                    (unsigned)protocolIndex);
            } else {
                xl6OutputPuts(".Entries = 0;\n", ctx->out);
            }
            xl6OutputPuts("    if ( !xl6ProtocolRegister(&", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
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
            XL6_AstNode protocolType;

            if ( protocolNode == NULL ||
                 protocolNode->kind != XL6_AST_PROTOCOL ||
                 xl6CgenProtocolIsGeneric(protocolNode) ) {
                continue;
            }
            memset(&protocolType, 0, sizeof(protocolType));
            protocolType.kind = XL6_AST_TYPE;
            protocolType.text = protocolNode->text;
            for ( instanceIndex = 0;
                  instanceIndex < ctx->genericClassInstanceCount;
                  ++instanceIndex ) {
                const XL6_CgenGenericClassInstance* instance =
                    &ctx->genericClassInstances[instanceIndex];
                if ( !xl6CgenConcreteClassSatisfiesProtocol(
                        ctx,
                        instance->classNode,
                        instance->typeNode,
                        protocolNode,
                        NULL) ) {
                    continue;
                }
                xl6OutputPuts(
                    "    (void)xl6ProtocolUnregister(&",
                    ctx->out);
                xl6CgenGenericProtocolWitnessSymbol(
                    ctx,
                    instance->typeNode,
                    &protocolType);
                xl6OutputPuts(");\n", ctx->out);
            }
        }
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts(
        "static void xl6_generic_class_protocol_module_unit(void)\n{\n",
        ctx->out);
    for ( protocolIndex = 0;
          protocolIndex < ctx->root->childCount;
          ++protocolIndex ) {
        const XL6_AstNode* protocolNode = ctx->root->children[protocolIndex];
        XL6_AstNode protocolType;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        memset(&protocolType, 0, sizeof(protocolType));
        protocolType.kind = XL6_AST_TYPE;
        protocolType.text = protocolNode->text;
        for ( instanceIndex = 0;
              instanceIndex < ctx->genericClassInstanceCount;
              ++instanceIndex ) {
            const XL6_CgenGenericClassInstance* instance =
                &ctx->genericClassInstances[instanceIndex];
            if ( !xl6CgenConcreteClassSatisfiesProtocol(
                    ctx,
                    instance->classNode,
                    instance->typeNode,
                    protocolNode,
                    NULL) ) {
                continue;
            }
            xl6OutputPuts("    (void)xl6ProtocolUnregister(&", ctx->out);
            xl6CgenGenericProtocolWitnessSymbol(
                ctx,
                instance->typeNode,
                &protocolType);
            xl6OutputPuts(");\n", ctx->out);
        }
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

