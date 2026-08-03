#include "src/cgen/xl6_cgen_runtime.h"

void xl6CgenEmitTempScopeRuntime(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_XRT_TEMP_SCOPE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XRT_TEMP_SCOPE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xrtTempScope {\n", out);
    xl6OutputPuts("    void* pThreadData;\n", out);
    xl6OutputPuts("    void* pCurrent;\n", out);
    xl6OutputPuts("    void* pSpill;\n", out);
    xl6OutputPuts("    unsigned int iUsed;\n", out);
    xl6OutputPuts("    unsigned int iDepth;\n", out);
    xl6OutputPuts("    unsigned long long iCurrentBytes;\n", out);
    xl6OutputPuts("    bool bActive;\n", out);
    xl6OutputPuts("} xrtTempScope;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("xrtTempScope xrtTempScopeBegin(void);\n", out);
    xl6OutputPuts("void xrtTempScopeEnd(xrtTempScope* pScope);\n", out);
    xl6OutputPuts("char* xrtTempStringTake(char* sText, size_t iSize);\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", out);
    xl6OutputPuts("void xrtFree(void* pMemory);\n", out);
    xl6OutputPuts("void* xrtThreadLocalGet(const char* sKey, size_t* pSize);\n", out);
    xl6OutputPuts("void* xrtThreadLocalGetOrCreate(const char* sKey, size_t iSize);\n", out);
    xl6OutputPuts("bool xrtThreadLocalRemove(const char* sKey);\n", out);
    xl6OutputPuts("bool xrtThreadIsAttached(void);\n", out);
    xl6OutputPuts("void* xrtThreadAttachCurrent(void);\n", out);
    xl6OutputPuts("void xl6_runtime_fatal(int code);\n\n", out);

    /*
     * XRT 的临时内存负责无析构值；xlang 还需要把 getter 返回的引用值
     * 延长到调用者表达式作用域。这里仅记录所有权，不复制对象本体。
     */
    xl6OutputPuts("typedef void (*XL6_TempManagedDrop)(void* value);\n", out);
    xl6OutputPuts("typedef struct XL6_TempManagedEntry {\n", out);
    xl6OutputPuts("    struct XL6_TempManagedEntry* previous;\n", out);
    xl6OutputPuts("    void* value;\n", out);
    xl6OutputPuts("    XL6_TempManagedDrop drop;\n", out);
    xl6OutputPuts("    unsigned int depth;\n", out);
    xl6OutputPuts("} XL6_TempManagedEntry;\n", out);
    xl6OutputPuts("typedef struct XL6_TempManagedState {\n", out);
    xl6OutputPuts("    XL6_TempManagedEntry* top;\n", out);
    xl6OutputPuts("    unsigned int depth;\n", out);
    xl6OutputPuts("} XL6_TempManagedState;\n", out);
    xl6OutputPuts("typedef struct XL6_TempScope {\n", out);
    xl6OutputPuts("    xrtTempScope xrt;\n", out);
    xl6OutputPuts("    unsigned int depth;\n", out);
    xl6OutputPuts("    bool active;\n", out);
    xl6OutputPuts("} XL6_TempScope;\n\n", out);

    xl6OutputPuts("static XL6_TempManagedState* xl6_temp_managed_state_existing(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t size = 0u;\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state = (XL6_TempManagedState*)xrtThreadLocalGet(\"xlang.temp.managed.v1\", &size);\n", out);
    xl6OutputPuts("    return state != 0 && size == sizeof(XL6_TempManagedState) ? state : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_TempManagedState* xl6_temp_managed_state(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state = (XL6_TempManagedState*)xrtThreadLocalGetOrCreate(\"xlang.temp.managed.v1\", sizeof(XL6_TempManagedState));\n", out);
    xl6OutputPuts("    if ( state == 0 && !xrtThreadIsAttached() && xrtThreadAttachCurrent() != 0 ) {\n", out);
    xl6OutputPuts("        state = (XL6_TempManagedState*)xrtThreadLocalGetOrCreate(\"xlang.temp.managed.v1\", sizeof(XL6_TempManagedState));\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( state == 0 ) { xl6_runtime_fatal(3); }\n", out);
    xl6OutputPuts("    return state;\n", out);
    xl6OutputPuts("}\n\n", out);

    /*
     * 返回值被显式接管或释放时，必须先撤销一条对应的临时所有权记录。
     * 同一容器可能因多次 retain 拥有多条记录，因此每次只移除一条。
     */
    xl6OutputPuts("static bool xl6_temp_managed_disown(void* value, XL6_TempManagedDrop drop)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
    xl6OutputPuts("    XL6_TempManagedEntry** link;\n", out);
    xl6OutputPuts("    XL6_TempManagedEntry* entry;\n", out);
    xl6OutputPuts("    if ( value == 0 || drop == 0 ) { return false; }\n", out);
    xl6OutputPuts("    state = xl6_temp_managed_state_existing();\n", out);
    xl6OutputPuts("    if ( state == 0 ) { return false; }\n", out);
    xl6OutputPuts("    link = &state->top;\n", out);
    xl6OutputPuts("    while ( *link != 0 ) {\n", out);
    xl6OutputPuts("        entry = *link;\n", out);
    xl6OutputPuts("        if ( entry->value == value && entry->drop == drop ) {\n", out);
    xl6OutputPuts("            *link = entry->previous;\n", out);
    xl6OutputPuts("            xrtFree(entry);\n", out);
    xl6OutputPuts("            return true;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        link = &entry->previous;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    /*
     * 所有消费表达式临时值的 helper 都通过这一入口释放。
     * 未登记的普通所有值仍会正常释放，已登记的临时值则先撤销作用域托管，避免二次释放。
     */
    xl6OutputPuts("static inline void xl6_temp_managed_release(void* value, XL6_TempManagedDrop drop)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( value == 0 || drop == 0 ) { return; }\n", out);
    xl6OutputPuts("    (void)xl6_temp_managed_disown(value, drop);\n", out);
    xl6OutputPuts("    drop(value);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_temp_managed_drop_above(XL6_TempManagedState* state, unsigned int targetDepth)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    while ( state != 0 && state->top != 0 && state->top->depth > targetDepth ) {\n", out);
    xl6OutputPuts("        XL6_TempManagedEntry* entry = state->top;\n", out);
    xl6OutputPuts("        state->top = entry->previous;\n", out);
    xl6OutputPuts("        if ( entry->value != 0 && entry->drop != 0 ) { entry->drop(entry->value); }\n", out);
    xl6OutputPuts("        xrtFree(entry);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_TempScope xl6_temp_scope_begin(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempScope scope;\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state = xl6_temp_managed_state();\n", out);
    xl6OutputPuts("    scope.xrt = xrtTempScopeBegin();\n", out);
    xl6OutputPuts("    scope.depth = ++state->depth;\n", out);
    xl6OutputPuts("    scope.active = true;\n", out);
    xl6OutputPuts("    return scope;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_temp_scope_end(XL6_TempScope* scope)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
    xl6OutputPuts("    if ( scope == 0 || !scope->active ) { return; }\n", out);
    xl6OutputPuts("    state = xl6_temp_managed_state();\n", out);
    xl6OutputPuts("    if ( state->depth != scope->depth ) { xl6_runtime_fatal(3); return; }\n", out);
    xl6OutputPuts("    xl6_temp_managed_drop_above(state, scope->depth - 1u);\n", out);
    xl6OutputPuts("    state->depth--;\n", out);
    xl6OutputPuts("    scope->active = false;\n", out);
    xl6OutputPuts("    xrtTempScopeEnd(&scope->xrt);\n", out);
    xl6OutputPuts("    if ( state->depth == 0u ) { (void)xrtThreadLocalRemove(\"xlang.temp.managed.v1\"); }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_temp_promote_to(void* value, XL6_TempManagedDrop drop, unsigned int targetDepth)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
    xl6OutputPuts("    XL6_TempManagedEntry* entry;\n", out);
    xl6OutputPuts("    if ( value == 0 || drop == 0 ) { return value; }\n", out);
    xl6OutputPuts("    state = xl6_temp_managed_state();\n", out);
    xl6OutputPuts("    if ( targetDepth >= state->depth ) { xl6_runtime_fatal(3); return 0; }\n", out);
    xl6OutputPuts("    xl6_temp_managed_drop_above(state, targetDepth);\n", out);
    xl6OutputPuts("    if ( targetDepth == 0 ) { return value; }\n", out);
    xl6OutputPuts("    entry = (XL6_TempManagedEntry*)xrtMalloc(sizeof(XL6_TempManagedEntry));\n", out);
    xl6OutputPuts("    if ( entry == 0 ) { drop(value); xl6_runtime_fatal(3); return 0; }\n", out);
    xl6OutputPuts("    entry->previous = state->top;\n", out);
    xl6OutputPuts("    entry->value = value;\n", out);
    xl6OutputPuts("    entry->drop = drop;\n", out);
    xl6OutputPuts("    entry->depth = targetDepth;\n", out);
    xl6OutputPuts("    state->top = entry;\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
}

void xl6CgenEmitApproxProtos(XL6_Output* out)
{
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("bool xrtIntApprox(long long a, long long b);\n", out);
    xl6OutputPuts("bool xrtNumApprox(double a, double b);\n\n", out);
    xl6OutputPuts("bool xrtTimeApprox(long long a, long long b);\n\n", out);
}

void xl6CgenEmitTimeRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("long long xrtTimeSerial(int iHour, int iMinute, int iSecond);\n", out);
    xl6OutputPuts("long long xrtDateSerial(long long iYear, int iMonth, int iDay);\n", out);
    xl6OutputPuts("long long xrtDateTimeSerial(long long iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);\n\n", out);
    xl6OutputPuts("long long xrtStrToTime(const char* sTime, size_t iSize);\n\n", out);
}

void xl6CgenEmitStringRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <stdarg.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n\n", out);
    xl6OutputPuts("void* xrtTempMemory(size_t iSize);\n\n", out);
    xl6OutputPuts("int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);\n", out);
    xl6OutputPuts("const char* xrtFindStr(const char* sText, size_t iSize, const char* sSubText, size_t iSubSize, bool bCase);\n", out);
    xl6OutputPuts("bool xrtStrApprox(const char* s1, size_t len1, const char* s2, size_t len2);\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_external_string_copy(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy;\n\n", out);
    xl6OutputPuts("    if ( len == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = (char*)xrtTempMemory(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(copy, text, len + 1);\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("#ifndef XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xbuffer_struct { char* Buffer; unsigned int Length; unsigned int AllocLength; unsigned int AllocStep; } xbuffer_struct, *xbuffer;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void xrtBufferInit(xbuffer pBuf, unsigned int iStep);\n", out);
    xl6OutputPuts("void xrtBufferUnit(xbuffer pBuf);\n", out);
    xl6OutputPuts("bool xrtBufferAppend(xbuffer pBuf, void* pData, unsigned int iSize, unsigned int bStrMode);\n\n", out);

    xl6OutputPuts("static void xl6_xrt_fstr_append(xbuffer buffer, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( buffer == 0 || text == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtBufferAppend(buffer, (void*)text, 0, 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_fstr_take(xbuffer buffer)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text;\n\n", out);
    xl6OutputPuts("    unsigned int length;\n\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( buffer->Buffer == 0 ) {\n", out);
    xl6OutputPuts("        xrtBufferAppend(buffer, (void*)\"\", 0, 1u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = buffer->Buffer;\n", out);
    xl6OutputPuts("    length = buffer->Length;\n", out);
    xl6OutputPuts("    buffer->Buffer = 0;\n", out);
    xl6OutputPuts("    buffer->Length = 0;\n", out);
    xl6OutputPuts("    buffer->AllocLength = 0;\n", out);
    xl6OutputPuts("    xrtBufferUnit(buffer);\n", out);
    xl6OutputPuts("    return text != 0 ? xrtTempStringTake(text, length) : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_fstr_join(int count, ...)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer_struct buffer;\n", out);
    xl6OutputPuts("    va_list args;\n", out);
    xl6OutputPuts("    int i;\n\n", out);
    xl6OutputPuts("    xrtBufferInit(&buffer, 256u);\n", out);
    xl6OutputPuts("    va_start(args, count);\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xl6_xrt_fstr_append(&buffer, va_arg(args, const char*));\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    va_end(args);\n", out);
    xl6OutputPuts("    return xl6_xrt_fstr_take(&buffer);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_str_concat(const char* left, const char* right)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t leftLen = left != 0 ? strlen(left) : 0;\n", out);
    xl6OutputPuts("    size_t rightLen = right != 0 ? strlen(right) : 0;\n", out);
    xl6OutputPuts("    size_t totalLen = leftLen + rightLen;\n", out);
    xl6OutputPuts("    char* text;\n\n", out);
    xl6OutputPuts("    if ( totalLen == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = (char*)xrtTempMemory(totalLen + 1);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( leftLen > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(text, left, leftLen);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( rightLen > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(text + leftLen, right, rightLen);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text[totalLen] = 0;\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_str_dup(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy;\n\n", out);
    xl6OutputPuts("    if ( len == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = (char*)xrtTempMemory(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(copy, text, len);\n", out);
    xl6OutputPuts("    copy[len] = 0;\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);
}

void xl6CgenEmitClassHandleRuntime(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_CLASS_DATA_HELPER\n", out);
    xl6OutputPuts("#define XL6_CLASS_DATA_HELPER\n", out);
    xl6OutputPuts("static void* xl6_class_data(xvalue value, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* data = value != 0 ? xvoGetRecordData(value) : 0;\n", out);
    xl6OutputPuts("    if ( data == 0 && value != 0 ) { data = xvoGetHandleSlot(value); }\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"NullReferenceError\", \"null class dereference\", file, line, column);\n", out);
    xl6OutputPuts("        xl6_runtime_fatal(4);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return data;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("static bool xl6_class_identity_equal_release(xvalue left, xvalue right)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    bool leftNull = left == 0 || xvoIsNull(left);\n", out);
    xl6OutputPuts("    bool rightNull = right == 0 || xvoIsNull(right);\n", out);
    xl6OutputPuts("    bool equal = (leftNull && rightNull) || (!leftNull && !rightNull && left == right);\n", out);
    xl6OutputPuts("    if ( left != 0 ) { xvoUnref(left); }\n", out);
    xl6OutputPuts("    if ( right != 0 ) { xvoUnref(right); }\n", out);
    xl6OutputPuts("    return equal;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}

void xl6CgenEmitIntPowHelper(XL6_Output* out)
{
    xl6OutputPuts("static long long xl6_int_pow_i64(long long base, long long exp)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = 1;\n\n", out);
    xl6OutputPuts("    if ( exp < 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( exp > 0 ) {\n", out);
    xl6OutputPuts("        if ( (exp & 1) != 0 ) {\n", out);
    xl6OutputPuts("            result *= base;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        exp >>= 1;\n", out);
    xl6OutputPuts("        if ( exp > 0 ) {\n", out);
    xl6OutputPuts("            base *= base;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
}

void xl6CgenEmitRangeContainsHelper(XL6_Output* out)
{
    xl6OutputPuts("static int xl6_range_contains_i64(long long value, long long start, long long stop, long long step)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( step == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( step > 0 ) {\n", out);
    xl6OutputPuts("        if ( value < start || value >= stop ) {\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        return ((value - start) % step) == 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( value > start || value <= stop ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return ((start - value) % -step) == 0;\n", out);
    xl6OutputPuts("}\n\n", out);
}
