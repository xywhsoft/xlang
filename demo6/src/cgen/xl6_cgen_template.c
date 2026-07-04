static void xl6CgenEmitTemplateRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_TEMPLATE_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_TEMPLATE_RUNTIME\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("typedef struct XTE_Template_Struct* xtetemplate;\n", out);
    xl6OutputPuts("xtetemplate xteParse(const char* sText, size_t iSize, const char* sBracket);\n", out);
    xl6OutputPuts("void xteDestroyTemplate(xtetemplate hTemplate);\n", out);
    xl6OutputPuts("char* xteMake(xtetemplate hTemplate, xvalue pCurrent, xvalue pGlobal, void* pIncludeMap, size_t* pRetSize);\n", out);
    xl6OutputPuts("unsigned int xteTemplateGetNodeCount(xtetemplate hTemplate);\n", out);
    xl6OutputPuts("unsigned int xteTemplateGetExprCount(xtetemplate hTemplate);\n", out);
    xl6OutputPuts("unsigned int xteTemplateGetArgCount(xtetemplate hTemplate);\n", out);
    xl6OutputPuts("unsigned int xteTemplateGetStringPoolSize(xtetemplate hTemplate);\n\n", out);

    xl6OutputPuts("void* xl6_xrt_template_parse(const char* text, const char* bracket)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    if ( bracket != 0 && bracket[0] == '\\0' ) { bracket = 0; }\n", out);
    xl6OutputPuts("    return xteParse(text, 0u, bracket);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("long long xl6_xrt_template_destroy(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xteDestroyTemplate((xtetemplate)self);\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_template_render(void* self, xvalue data)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( self == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    return xteMake((xtetemplate)self, data, 0, 0, 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("unsigned int xl6_xrt_template_node_count(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? xteTemplateGetNodeCount((xtetemplate)self) : 0u;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("unsigned int xl6_xrt_template_expr_count(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? xteTemplateGetExprCount((xtetemplate)self) : 0u;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("unsigned int xl6_xrt_template_arg_count(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? xteTemplateGetArgCount((xtetemplate)self) : 0u;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("unsigned int xl6_xrt_template_string_pool_size(void* self)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return self != 0 ? xteTemplateGetStringPoolSize((xtetemplate)self) : 0u;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}
