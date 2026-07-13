static int xl6CgenIsBuiltinTypeName(const char* name)
{
    static const char* names[] = {
        "null", "bool",
        "int", "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
        "float", "float32", "float64",
        "c_char", "c_schar", "c_uchar", "c_short", "c_ushort", "c_int", "c_uint",
        "c_long", "c_ulong", "c_longlong", "c_ulonglong",
        "c_size", "c_ssize", "c_ptrdiff", "c_intptr", "c_uintptr", "c_bool",
        "c_float", "c_double", "c_wchar",
        "string", "time", "point", "error", "function", "var", "type",
        "array", "list", "set", "dict", "__class_handle",
        "future", "task", "channel", "generator",
        "thread", "mutex", "cond", "rwlock", "sem"
    };
    size_t i;

    if ( name == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6CTextEquals(name, names[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenIsIntegerTypeName(const char* name)
{
    return xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int8") ||
           xl6CTextEquals(name, "uint8") || xl6CTextEquals(name, "int16") ||
           xl6CTextEquals(name, "uint16") || xl6CTextEquals(name, "int32") ||
           xl6CTextEquals(name, "uint32") || xl6CTextEquals(name, "int64") ||
           xl6CTextEquals(name, "uint64") ||
           xl6CTextEquals(name, "c_char") || xl6CTextEquals(name, "c_schar") ||
           xl6CTextEquals(name, "c_uchar") || xl6CTextEquals(name, "c_short") ||
           xl6CTextEquals(name, "c_ushort") || xl6CTextEquals(name, "c_int") ||
           xl6CTextEquals(name, "c_uint") || xl6CTextEquals(name, "c_long") ||
           xl6CTextEquals(name, "c_ulong") || xl6CTextEquals(name, "c_longlong") ||
           xl6CTextEquals(name, "c_ulonglong") || xl6CTextEquals(name, "c_size") ||
           xl6CTextEquals(name, "c_ssize") || xl6CTextEquals(name, "c_ptrdiff") ||
           xl6CTextEquals(name, "c_intptr") || xl6CTextEquals(name, "c_uintptr") ||
           xl6CTextEquals(name, "c_wchar");
}

static int xl6CgenIsFloatTypeName(const char* name)
{
    return xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float32") ||
           xl6CTextEquals(name, "float64") ||
           xl6CTextEquals(name, "c_float") || xl6CTextEquals(name, "c_double");
}

static int xl6CgenIsBoolTypeName(const char* name)
{
    return xl6CTextEquals(name, "bool") || xl6CTextEquals(name, "c_bool");
}

typedef struct XL6_CgenIntTypeInfo {
    int bits;
    int isSigned;
} XL6_CgenIntTypeInfo;

static int xl6CgenIntTypeInfo(const char* name, XL6_CgenIntTypeInfo* outInfo)
{
    XL6_CgenIntTypeInfo info;

    if ( name == NULL ) {
        return 0;
    }
    info.bits = 0;
    info.isSigned = 1;
    if ( xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int64") ) {
        info.bits = 64;
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "uint64") ) {
        info.bits = 64;
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "int8") ) {
        info.bits = 8;
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "uint8") ) {
        info.bits = 8;
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "int16") ) {
        info.bits = 16;
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "uint16") ) {
        info.bits = 16;
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "int32") ) {
        info.bits = 32;
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "uint32") ) {
        info.bits = 32;
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_char") ) {
        info.bits = (int)(sizeof(char) * CHAR_BIT);
        info.isSigned = (CHAR_MIN < 0);
    } else if ( xl6CTextEquals(name, "c_schar") ) {
        info.bits = (int)(sizeof(signed char) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "c_uchar") ) {
        info.bits = (int)(sizeof(unsigned char) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_short") ) {
        info.bits = (int)(sizeof(short) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "c_ushort") ) {
        info.bits = (int)(sizeof(unsigned short) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_int") ) {
        info.bits = (int)(sizeof(int) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "c_uint") ) {
        info.bits = (int)(sizeof(unsigned int) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_long") ) {
        info.bits = (int)(sizeof(long) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "c_ulong") ) {
        info.bits = (int)(sizeof(unsigned long) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_longlong") ) {
        info.bits = (int)(sizeof(long long) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "c_ulonglong") ) {
        info.bits = (int)(sizeof(unsigned long long) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_size") || xl6CTextEquals(name, "c_uintptr") ) {
        info.bits = (int)(sizeof(size_t) * CHAR_BIT);
        info.isSigned = 0;
    } else if ( xl6CTextEquals(name, "c_ssize") || xl6CTextEquals(name, "c_ptrdiff") || xl6CTextEquals(name, "c_intptr") ) {
        info.bits = (int)(sizeof(size_t) * CHAR_BIT);
        info.isSigned = 1;
    } else if ( xl6CTextEquals(name, "c_wchar") ) {
        info.bits = (int)(sizeof(wchar_t) * CHAR_BIT);
        info.isSigned = ((wchar_t)-1) < (wchar_t)0;
    } else {
        return 0;
    }
    if ( outInfo != NULL ) {
        *outInfo = info;
    }
    return info.bits > 0;
}

static int xl6CgenFloatTypeBits(const char* name)
{
    if ( xl6CTextEquals(name, "float32") || xl6CTextEquals(name, "c_float") ) {
        return 32;
    }
    if ( xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float64") || xl6CTextEquals(name, "c_double") ) {
        return 64;
    }
    return 0;
}

static int xl6CgenLiteralDigitValue(char ch)
{
    if ( ch >= '0' && ch <= '9' ) {
        return ch - '0';
    }
    if ( ch >= 'a' && ch <= 'f' ) {
        return ch - 'a' + 10;
    }
    if ( ch >= 'A' && ch <= 'F' ) {
        return ch - 'A' + 10;
    }
    return -1;
}

static int xl6CgenParseUIntLiteralText(const char* text, uint64* outValue)
{
    const char* p;
    int base;
    uint64 value;

    if ( text == NULL || text[0] == '\0' ) {
        return 0;
    }
    p = text;
    base = 10;
    if ( p[0] == '0' && p[1] == 'x' ) {
        base = 16;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'o' ) {
        base = 8;
        p += 2;
    } else if ( p[0] == '0' && p[1] == 'b' ) {
        base = 2;
        p += 2;
    }

    value = 0;
    while ( *p != '\0' ) {
        int digit;

        if ( *p == '_' ) {
            ++p;
            continue;
        }
        digit = xl6CgenLiteralDigitValue(*p);
        if ( digit < 0 || digit >= base || value > (~(uint64)0 - (uint64)digit) / (uint64)base ) {
            return 0;
        }
        value = value * (uint64)base + (uint64)digit;
        ++p;
    }
    if ( outValue != NULL ) {
        *outValue = value;
    }
    return 1;
}

static int xl6CgenExprIntegerLiteralValue(const XL6_AstNode* expr, uint64* outMagnitude, int* outNegative)
{
    const XL6_AstNode* valueNode;
    int negative;

    if ( expr == NULL ) {
        return 0;
    }
    valueNode = expr;
    negative = 0;
    if ( expr->kind == XL6_AST_UNARY && expr->childCount == 1 &&
         (xl6CTextEquals(expr->text, "-") || xl6CTextEquals(expr->text, "+")) ) {
        negative = xl6CTextEquals(expr->text, "-");
        valueNode = expr->children[0];
    }
    if ( valueNode == NULL || valueNode->kind != XL6_AST_INT ) {
        return 0;
    }
    if ( !xl6CgenParseUIntLiteralText(valueNode->text, outMagnitude) ) {
        return 0;
    }
    if ( outNegative != NULL ) {
        *outNegative = negative;
    }
    return 1;
}

static uint64 xl6CgenUnsignedMaxForBits(int bits)
{
    if ( bits >= 64 ) {
        return ~(uint64)0;
    }
    return (((uint64)1) << bits) - 1;
}

static int xl6CgenExprIntegerLiteralFitsType(const XL6_AstNode* expr, const char* targetType)
{
    XL6_CgenIntTypeInfo info;
    uint64 magnitude;
    int negative;

    if ( !xl6CgenIntTypeInfo(targetType, &info) ||
         !xl6CgenExprIntegerLiteralValue(expr, &magnitude, &negative) ) {
        return 0;
    }
    if ( info.isSigned ) {
        uint64 positiveMax = info.bits >= 64 ? 0x7fffffffffffffffULL : ((((uint64)1) << (info.bits - 1)) - 1);
        uint64 negativeMax = info.bits >= 64 ? 0x8000000000000000ULL : (((uint64)1) << (info.bits - 1));
        return negative ? magnitude <= negativeMax : magnitude <= positiveMax;
    }
    return !negative && magnitude <= xl6CgenUnsignedMaxForBits(info.bits);
}

static int xl6CgenCanSafeWidenTypeName(const char* source, const char* target)
{
    XL6_CgenIntTypeInfo srcInt;
    XL6_CgenIntTypeInfo dstInt;
    int srcFloatBits;
    int dstFloatBits;

    if ( source == NULL || target == NULL ) {
        return 0;
    }
    if ( xl6CTextEquals(source, target) ) {
        return 1;
    }
    if ( xl6CgenIsBoolTypeName(source) && xl6CgenIsBoolTypeName(target) ) {
        return 1;
    }
    if ( xl6CgenIsBoolTypeName(source) &&
         (xl6CgenIntTypeInfo(target, NULL) || xl6CgenFloatTypeBits(target) > 0) ) {
        return 1;
    }
    if ( xl6CgenIntTypeInfo(source, &srcInt) && xl6CgenIntTypeInfo(target, &dstInt) ) {
        if ( srcInt.isSigned && dstInt.isSigned ) {
            return dstInt.bits >= srcInt.bits;
        }
        if ( !srcInt.isSigned && !dstInt.isSigned ) {
            return dstInt.bits >= srcInt.bits;
        }
        if ( !srcInt.isSigned && dstInt.isSigned ) {
            return dstInt.bits > srcInt.bits;
        }
        return 0;
    }
    srcFloatBits = xl6CgenFloatTypeBits(source);
    dstFloatBits = xl6CgenFloatTypeBits(target);
    if ( srcFloatBits > 0 && dstFloatBits > 0 ) {
        return dstFloatBits >= srcFloatBits;
    }
    return 0;
}

static int xl6CgenIsLanguageBoolType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    return concreteType != NULL && xl6CTextEquals(concreteType->text, "bool");
}

static int xl6CgenIsXValueTypeName(const char* name)
{
    return xl6CTextEquals(name, "var") ||
           xl6CTextEquals(name, "array") ||
           xl6CTextEquals(name, "list") ||
           xl6CTextEquals(name, "set") ||
           xl6CTextEquals(name, "dict") ||
           xl6CTextEquals(name, "__class_handle");
}

static int xl6CgenIsStringTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    return typeNode != NULL && xl6CTextEquals(typeNode->text, "string");
}

static int xl6CgenIsBuiltinConvertTypeName(const char* name)
{
    return xl6CgenIsIntegerTypeName(name) || xl6CgenIsFloatTypeName(name) ||
           xl6CgenIsBoolTypeName(name) || xl6CTextEquals(name, "string") ||
           xl6CTextEquals(name, "time") || xl6CTextEquals(name, "point") ||
           xl6CgenIsXValueTypeName(name);
}

static const char* xl6CgenConvertCTypeName(const char* name)
{
    if ( xl6CTextEquals(name, "c_char") ) {
        return "char";
    }
    if ( xl6CTextEquals(name, "c_schar") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(name, "c_uchar") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(name, "c_short") ) {
        return "short";
    }
    if ( xl6CTextEquals(name, "c_ushort") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(name, "c_int") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "c_uint") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(name, "c_long") ) {
        return "long";
    }
    if ( xl6CTextEquals(name, "c_ulong") ) {
        return "unsigned long";
    }
    if ( xl6CTextEquals(name, "c_longlong") ) {
        return "long long";
    }
    if ( xl6CTextEquals(name, "c_ulonglong") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(name, "c_size") ) {
        return "size_t";
    }
    if ( xl6CTextEquals(name, "c_ssize") ) {
        return "xl6_ssize_t";
    }
    if ( xl6CTextEquals(name, "c_ptrdiff") ) {
        return "ptrdiff_t";
    }
    if ( xl6CTextEquals(name, "c_intptr") ) {
        return "intptr_t";
    }
    if ( xl6CTextEquals(name, "c_uintptr") ) {
        return "uintptr_t";
    }
    if ( xl6CTextEquals(name, "c_bool") ) {
        return "bool";
    }
    if ( xl6CTextEquals(name, "c_float") ) {
        return "float";
    }
    if ( xl6CTextEquals(name, "c_double") ) {
        return "double";
    }
    if ( xl6CTextEquals(name, "c_wchar") ) {
        return "wchar_t";
    }
    if ( xl6CTextEquals(name, "int") || xl6CTextEquals(name, "int64") ||
         xl6CTextEquals(name, "time") ) {
        return "long long";
    }
    if ( xl6CTextEquals(name, "int8") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(name, "uint8") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(name, "int16") ) {
        return "short";
    }
    if ( xl6CTextEquals(name, "uint16") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(name, "int32") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "uint32") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(name, "uint64") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(name, "float") || xl6CTextEquals(name, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(name, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(name, "bool") ) {
        return "int";
    }
    if ( xl6CTextEquals(name, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(name, "point") ) {
        return "void*";
    }
    if ( xl6CgenIsXValueTypeName(name) ) {
        return "xvalue";
    }
    return "int";
}

static int xl6CgenIsKnownTypeName(XL6_CgenContext* ctx, const char* name)
{
    return xl6CgenIsBuiltinTypeName(name) ||
           xl6CgenFindTypeAlias(ctx, name) != NULL ||
           xl6CgenFindClass(ctx, name) != NULL ||
           xl6CgenFindNativeBackedClass(ctx, name) != NULL ||
           xl6CgenFindStruct(ctx, name) != NULL ||
           xl6CgenFindUnion(ctx, name) != NULL;
}

static const char* xl6CgenTypeName(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    static char typeTextBuffer[256];

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);

    if ( typeNode == NULL || typeNode->text == NULL ) {
        xl6CgenError(ctx, typeNode, "缺少类型");
        return "int";
    }

    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "long long";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "signed char";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "unsigned char";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "short";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "unsigned short";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "unsigned int";
    }
    if ( xl6CTextEquals(typeNode->text, "c_char") || xl6CTextEquals(typeNode->text, "c_schar") ||
         xl6CTextEquals(typeNode->text, "c_uchar") || xl6CTextEquals(typeNode->text, "c_short") ||
         xl6CTextEquals(typeNode->text, "c_ushort") || xl6CTextEquals(typeNode->text, "c_int") ||
         xl6CTextEquals(typeNode->text, "c_uint") || xl6CTextEquals(typeNode->text, "c_long") ||
         xl6CTextEquals(typeNode->text, "c_ulong") || xl6CTextEquals(typeNode->text, "c_longlong") ||
         xl6CTextEquals(typeNode->text, "c_ulonglong") || xl6CTextEquals(typeNode->text, "c_size") ||
         xl6CTextEquals(typeNode->text, "c_ssize") || xl6CTextEquals(typeNode->text, "c_ptrdiff") ||
         xl6CTextEquals(typeNode->text, "c_intptr") || xl6CTextEquals(typeNode->text, "c_uintptr") ||
         xl6CTextEquals(typeNode->text, "c_bool") || xl6CTextEquals(typeNode->text, "c_float") ||
         xl6CTextEquals(typeNode->text, "c_double") || xl6CTextEquals(typeNode->text, "c_wchar") ) {
        return xl6CgenConvertCTypeName(typeNode->text);
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "int";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "time") ) {
        return "long long";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    if ( xl6CTextEquals(typeNode->text, "future") ||
         xl6CTextEquals(typeNode->text, "task") ) {
        return "xfuture*";
    }
    if ( xl6CTextEquals(typeNode->text, "channel") ) {
        return "xchannel";
    }
    if ( xl6CTextEquals(typeNode->text, "thread") ||
         xl6CTextEquals(typeNode->text, "mutex") ||
         xl6CTextEquals(typeNode->text, "cond") ||
         xl6CTextEquals(typeNode->text, "rwlock") ||
         xl6CTextEquals(typeNode->text, "sem") ) {
        return "void*";
    }
    if ( xl6CTextEquals(typeNode->text, "generator") ) {
        return "XL6_Generator*";
    }
    if ( xl6CTextEquals(typeNode->text, "error") ) {
        return "XL6_Error*";
    }
    if ( xl6CTextEquals(typeNode->text, "type") ) {
        return "unsigned long long";
    }
    if ( xl6CTextEquals(typeNode->text, "var") || xl6CTextEquals(typeNode->text, "__class_handle") ||
         xl6CTextEquals(typeNode->text, "array") || xl6CTextEquals(typeNode->text, "array<var>") ||
         xl6CTextEquals(typeNode->text, "list") || xl6CTextEquals(typeNode->text, "list<var>") ||
         xl6CTextEquals(typeNode->text, "set") || xl6CTextEquals(typeNode->text, "set<var>") ||
         xl6CTextEquals(typeNode->text, "dict") || xl6CTextEquals(typeNode->text, "dict<var>") ) {
        return "xvalue";
    }
    if ( xl6CTextEquals(typeNode->text, "function") ) {
        return "XL6_Function";
    }
    if ( xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
        xl6CgenNativeBackedTypeRefName(typeTextBuffer, sizeof(typeTextBuffer), typeNode->text);
        return typeTextBuffer;
    }
    if ( xl6CgenIsClassType(ctx, typeNode) ) {
        XL6_AstNode* canonicalType = NULL;
        const XL6_AstNode* refType = typeNode;

        if ( typeNode->childCount > 0 ) {
            canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
            if ( canonicalType != NULL ) {
                refType = canonicalType;
            }
        }
        xl6CgenClassTypeRefName(typeTextBuffer, sizeof(typeTextBuffer), refType);
        xl6AstFree(canonicalType);
        return typeTextBuffer;
    }
    if ( xl6CgenIsStructType(ctx, typeNode) ) {
        xl6CgenStructTypeTextName(typeTextBuffer, sizeof(typeTextBuffer), typeNode->text);
        return typeTextBuffer;
    }
    if ( xl6CgenIsUnionType(ctx, typeNode) ) {
        xl6CgenUnionTypeTextName(typeTextBuffer, sizeof(typeTextBuffer), typeNode->text);
        return typeTextBuffer;
    }

    snprintf(typeTextBuffer, sizeof(typeTextBuffer), "C generator does not support type node: %s (%s)",
             typeNode->text != NULL ? typeNode->text : "<missing>", xl6AstKindName(typeNode->kind));
    xl6CgenError(ctx, typeNode, typeTextBuffer);
    return "int";
}

static const char* xl6CgenCAbiTypeName(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);

    if ( typeNode == NULL || typeNode->text == NULL ) {
        return "void";
    }
    if ( xl6CTextEquals(typeNode->text, "bool") ) {
        return "bool";
    }
    if ( xl6CTextEquals(typeNode->text, "c_char") || xl6CTextEquals(typeNode->text, "c_schar") ||
         xl6CTextEquals(typeNode->text, "c_uchar") || xl6CTextEquals(typeNode->text, "c_short") ||
         xl6CTextEquals(typeNode->text, "c_ushort") || xl6CTextEquals(typeNode->text, "c_int") ||
         xl6CTextEquals(typeNode->text, "c_uint") || xl6CTextEquals(typeNode->text, "c_long") ||
         xl6CTextEquals(typeNode->text, "c_ulong") || xl6CTextEquals(typeNode->text, "c_longlong") ||
         xl6CTextEquals(typeNode->text, "c_ulonglong") || xl6CTextEquals(typeNode->text, "c_size") ||
         xl6CTextEquals(typeNode->text, "c_ssize") || xl6CTextEquals(typeNode->text, "c_ptrdiff") ||
         xl6CTextEquals(typeNode->text, "c_intptr") || xl6CTextEquals(typeNode->text, "c_uintptr") ||
         xl6CTextEquals(typeNode->text, "c_bool") || xl6CTextEquals(typeNode->text, "c_float") ||
         xl6CTextEquals(typeNode->text, "c_double") || xl6CTextEquals(typeNode->text, "c_wchar") ) {
        return xl6CgenConvertCTypeName(typeNode->text);
    }
    if ( xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int64") ) {
        return "int64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int8") ) {
        return "int8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint8") ) {
        return "uint8_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int16") ) {
        return "int16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint16") ) {
        return "uint16_t";
    }
    if ( xl6CTextEquals(typeNode->text, "int32") ) {
        return "int32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint32") ) {
        return "uint32_t";
    }
    if ( xl6CTextEquals(typeNode->text, "uint64") ) {
        return "uint64_t";
    }
    if ( xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float64") ) {
        return "double";
    }
    if ( xl6CTextEquals(typeNode->text, "float32") ) {
        return "float";
    }
    if ( xl6CTextEquals(typeNode->text, "string") ) {
        return "const char*";
    }
    if ( xl6CTextEquals(typeNode->text, "point") ) {
        return "void*";
    }
    if ( xl6CgenTypedContainerConcreteType(ctx, typeNode) != NULL ) {
        return xl6CgenTypedContainerCType(ctx, typeNode);
    }
    return xl6CgenTypeName(ctx, typeNode);
}

static XL6_AstNode* xl6CgenFindFunc(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( xl6CgenIsFuncDeclNode(node) &&
             !xl6CgenFuncIsGeneric(node) &&
             node->childCount > 0 &&
             node->children[0] != NULL && xl6CTextEquals(node->children[0]->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static int xl6CgenTypeNodeEqual(const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return a == b;
    }
    if ( a->kind != b->kind || a->childCount != b->childCount ) {
        return 0;
    }
    if ( (a->text == NULL) != (b->text == NULL) ) {
        return 0;
    }
    if ( a->text != NULL && !xl6CTextEquals(a->text, b->text) ) {
        return 0;
    }
    for ( i = 0; i < a->childCount; ++i ) {
        if ( !xl6CgenTypeNodeEqual(a->children[i], b->children[i]) ) {
            return 0;
        }
    }
    return 1;
}
