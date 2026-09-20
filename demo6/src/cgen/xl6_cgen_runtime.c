#include "src/cgen/xl6_cgen_runtime.h"



/* 输出表达式临时值作用域及托管所有权运行时。 */
void xl6CgenEmitTempScopeRuntime(
	XL6_Output* out,
	int hasStringRuntime,
	int hasAsyncFileRuntime,
	int hasNetRuntime
)
{
	if ( hasStringRuntime ) {
		xl6OutputPuts("static inline char* xl6_xrt_string_dup(const char* sText, size_t iSize);\n", out);
	}
    xl6OutputPuts("void xl6_runtime_fatal(int code);\n", out);
    xl6OutputPuts("void xl6_runtime_fatal_message(int code, const char* message);\n\n", out);
	if ( hasAsyncFileRuntime ) {
		xl6OutputPuts("static bool xl6_async_file_runtime_init(void);\n", out);
		xl6OutputPuts("static void xl6_async_file_runtime_unit(void);\n\n", out);
	}
	if ( hasNetRuntime ) {
		xl6OutputPuts("static bool xl6_net_runtime_init(void);\n", out);
		xl6OutputPuts("static void xl6_net_runtime_unit(void);\n\n", out);
	}

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
    xl6OutputPuts("    xtempmark xrt;\n", out);
    xl6OutputPuts("    unsigned int depth;\n", out);
    xl6OutputPuts("    bool active;\n", out);
    xl6OutputPuts("} XL6_TempScope;\n\n", out);

    xl6OutputPuts("static char* xl6_temp_string_take(char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* result;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return \"\"; }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { size = strlen(text); }\n", out);
    xl6OutputPuts("    result = xrtTempStr(xrtTempCurrent(), (xstrview){ text, size });\n", out);
    xl6OutputPuts("    xrtFree(text);\n", out);
    xl6OutputPuts("    return result != 0 ? result : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);
	xl6OutputPuts("static xthreadkey* xl6_temp_managed_key = 0;\n", out);
	xl6OutputPuts("static void xl6_temp_managed_state_destroy(void* value);\n\n", out);

	xl6OutputPuts("static bool xl6_temp_managed_runtime_init(void)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    if ( xl6_temp_managed_key != 0 ) { return true; }\n", out);
	xl6OutputPuts("    xl6_temp_managed_key = xrtThreadKeyCreate(xl6_temp_managed_state_destroy);\n", out);
	if ( hasAsyncFileRuntime || hasNetRuntime ) {
		xl6OutputPuts("    if ( xl6_temp_managed_key == 0 ) { return false; }\n", out);
	}
	if ( hasAsyncFileRuntime ) {
		xl6OutputPuts("    if ( !xl6_async_file_runtime_init() ) {\n", out);
        xl6OutputPuts("        (void)xrtThreadKeyDestroy(xl6_temp_managed_key);\n", out);
        xl6OutputPuts("        xl6_temp_managed_key = 0;\n", out);
        xl6OutputPuts("        return false;\n", out);
        xl6OutputPuts("    }\n", out);
	}
	if ( hasNetRuntime ) {
		xl6OutputPuts("    if ( !xl6_net_runtime_init() ) {\n", out);
		if ( hasAsyncFileRuntime ) {
			xl6OutputPuts("        xl6_async_file_runtime_unit();\n", out);
		}
		xl6OutputPuts("        (void)xrtThreadKeyDestroy(xl6_temp_managed_key);\n", out);
		xl6OutputPuts("        xl6_temp_managed_key = 0;\n", out);
		xl6OutputPuts("        return false;\n", out);
		xl6OutputPuts("    }\n", out);
	}
	if ( hasAsyncFileRuntime || hasNetRuntime ) {
		xl6OutputPuts("    return true;\n", out);
	} else {
		xl6OutputPuts("    return xl6_temp_managed_key != 0;\n", out);
    }
	xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_TempManagedState* xl6_temp_managed_state_existing(void)\n", out);
    xl6OutputPuts("{\n", out);
	xl6OutputPuts("    if ( xl6_temp_managed_key == 0 ) { return 0; }\n", out);
	xl6OutputPuts("    return (XL6_TempManagedState*)xrtThreadKeyGet(xl6_temp_managed_key);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_TempManagedState* xl6_temp_managed_state(void)\n", out);
    xl6OutputPuts("{\n", out);
	xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
	xl6OutputPuts("    if ( xl6_temp_managed_key == 0 ) { xl6_runtime_fatal_message(3, \"temporary ownership key is unavailable\"); return 0; }\n", out);
	xl6OutputPuts("    state = (XL6_TempManagedState*)xrtThreadKeyGet(xl6_temp_managed_key);\n", out);
	xl6OutputPuts("    if ( state != 0 ) { return state; }\n", out);
	xl6OutputPuts("    state = (XL6_TempManagedState*)xrtMalloc(sizeof(XL6_TempManagedState));\n", out);
	xl6OutputPuts("    if ( state == 0 ) { xl6_runtime_fatal_message(3, \"temporary ownership state allocation failed\"); return 0; }\n", out);
	xl6OutputPuts("    memset(state, 0, sizeof(XL6_TempManagedState));\n", out);
	xl6OutputPuts("    if ( !xrtThreadKeySet(xl6_temp_managed_key, state) ) {\n", out);
	xl6OutputPuts("        xrtFree(state);\n", out);
	xl6OutputPuts("        xl6_runtime_fatal_message(3, \"temporary ownership state registration failed\");\n", out);
	xl6OutputPuts("        return 0;\n", out);
	xl6OutputPuts("    }\n", out);
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

	xl6OutputPuts("static void xl6_temp_managed_state_destroy(void* value)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    XL6_TempManagedState* state = (XL6_TempManagedState*)value;\n", out);
	xl6OutputPuts("    if ( state == 0 ) { return; }\n", out);
	xl6OutputPuts("    xl6_temp_managed_drop_above(state, 0u);\n", out);
	xl6OutputPuts("    xrtFree(state);\n", out);
	xl6OutputPuts("}\n\n", out);

	xl6OutputPuts("static void xl6_temp_managed_runtime_unit(void)\n", out);
	xl6OutputPuts("{\n", out);
	xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
	if ( hasAsyncFileRuntime ) {
		xl6OutputPuts("    xl6_async_file_runtime_unit();\n", out);
	}
	if ( hasNetRuntime ) {
		xl6OutputPuts("    xl6_net_runtime_unit();\n", out);
	}
	xl6OutputPuts("    if ( xl6_temp_managed_key == 0 ) { return; }\n", out);
	xl6OutputPuts("    state = (XL6_TempManagedState*)xrtThreadKeyTake(xl6_temp_managed_key);\n", out);
	xl6OutputPuts("    xl6_temp_managed_state_destroy(state);\n", out);
	xl6OutputPuts("    if ( !xrtThreadKeyDestroy(xl6_temp_managed_key) ) { xl6_runtime_fatal_message(3, \"temporary ownership key destruction failed\"); }\n", out);
	xl6OutputPuts("    xl6_temp_managed_key = 0;\n", out);
	xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static XL6_TempScope xl6_temp_scope_begin(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempScope scope;\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state = xl6_temp_managed_state();\n", out);
    xl6OutputPuts("    memset(&scope, 0, sizeof(scope));\n", out);
    xl6OutputPuts("    scope.xrt = xrtTempBegin(xrtTempCurrent());\n", out);
    xl6OutputPuts("    if ( !scope.xrt.Active ) { xl6_runtime_fatal_message(3, \"temporary arena scope creation failed\"); return scope; }\n", out);
    xl6OutputPuts("    scope.depth = ++state->depth;\n", out);
    xl6OutputPuts("    scope.active = true;\n", out);
    xl6OutputPuts("    return scope;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_temp_scope_end(XL6_TempScope* scope)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
    xl6OutputPuts("    if ( scope == 0 || !scope->active ) { return; }\n", out);
    xl6OutputPuts("    state = xl6_temp_managed_state();\n", out);
    xl6OutputPuts("    if ( state->depth != scope->depth ) { xl6_runtime_fatal_message(3, \"temporary scopes ended out of order\"); return; }\n", out);
    xl6OutputPuts("    xl6_temp_managed_drop_above(state, scope->depth - 1u);\n", out);
    xl6OutputPuts("    state->depth--;\n", out);
    xl6OutputPuts("    scope->active = false;\n", out);
    xl6OutputPuts("    if ( !xrtTempEnd(&scope->xrt) ) { xl6_runtime_fatal_message(3, \"temporary arena scope rollback failed\"); return; }\n", out);
	xl6OutputPuts("    if ( state->depth == 0u ) {\n", out);
	xl6OutputPuts("        XL6_TempManagedState* taken = (XL6_TempManagedState*)xrtThreadKeyTake(xl6_temp_managed_key);\n", out);
	xl6OutputPuts("        if ( taken != state ) { xl6_runtime_fatal_message(3, \"temporary ownership state was replaced unexpectedly\"); return; }\n", out);
	xl6OutputPuts("        xl6_temp_managed_state_destroy(taken);\n", out);
	xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_temp_promote_to(void* value, XL6_TempManagedDrop drop, unsigned int targetDepth)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_TempManagedState* state;\n", out);
    xl6OutputPuts("    XL6_TempManagedEntry* entry;\n", out);
    xl6OutputPuts("    if ( value == 0 || drop == 0 ) { return value; }\n", out);
    xl6OutputPuts("    state = xl6_temp_managed_state();\n", out);
    xl6OutputPuts("    if ( targetDepth >= state->depth ) { xl6_runtime_fatal_message(3, \"temporary ownership promotion target is invalid\"); return 0; }\n", out);
    xl6OutputPuts("    xl6_temp_managed_drop_above(state, targetDepth);\n", out);
    xl6OutputPuts("    if ( targetDepth == 0 ) { return value; }\n", out);
    xl6OutputPuts("    entry = (XL6_TempManagedEntry*)xrtMalloc(sizeof(XL6_TempManagedEntry));\n", out);
    xl6OutputPuts("    if ( entry == 0 ) { drop(value); xl6_runtime_fatal_message(3, \"temporary ownership entry allocation failed\"); return 0; }\n", out);
    xl6OutputPuts("    entry->previous = state->top;\n", out);
    xl6OutputPuts("    entry->value = value;\n", out);
    xl6OutputPuts("    entry->drop = drop;\n", out);
    xl6OutputPuts("    entry->depth = targetDepth;\n", out);
    xl6OutputPuts("    state->top = entry;\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
}

void xl6CgenEmitStringRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <stdarg.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n\n", out);
    xl6OutputPuts("xstrview xrtStrView(const char* text);\n", out);
    xl6OutputPuts("xstrview xrtStrViewN(const char* text, size_t size);\n", out);
    xl6OutputPuts("int xrtStrCompare(xstrview left, xstrview right);\n", out);
    xl6OutputPuts("size_t xrtStrFind(xstrview text, xstrview part, size_t start);\n", out);
    xl6OutputPuts("double xrtUtf8Similarity(xstrview left, xstrview right);\n\n", out);
    xl6OutputPuts("void* xrtTemp(size_t iSize);\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_external_string_copy(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy;\n\n", out);
    xl6OutputPuts("    if ( len == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = (char*)xrtTemp(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(copy, text, len + 1);\n", out);
    xl6OutputPuts("    return copy;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static void xl6_xrt_fstr_append(xbuffer* buffer, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( buffer == 0 || text == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtBufferAppend(buffer, (xbytesview){ (const unsigned char*)text, strlen(text) });\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_fstr_take(xbuffer* buffer)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text;\n\n", out);
    xl6OutputPuts("    size_t length = 0u;\n\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    length = buffer->Size;\n", out);
    xl6OutputPuts("    if ( !xrtBufferAppendByte(buffer, 0u) ) { xrtBufferUnit(buffer); return \"\"; }\n", out);
    xl6OutputPuts("    text = (char*)xrtBufferTake(buffer, 0, 0);\n", out);
    xl6OutputPuts("    xrtBufferUnit(buffer);\n", out);
    xl6OutputPuts("    return text != 0 ? xl6_temp_string_take(text, length) : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_fstr_join(int count, ...)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer buffer;\n", out);
    xl6OutputPuts("    va_list args;\n", out);
    xl6OutputPuts("    int i;\n\n", out);
    xl6OutputPuts("    if ( !xrtBufferInit(&buffer) ) { return \"\"; }\n", out);
    xl6OutputPuts("    xrtBufferReserve(&buffer, 256u);\n", out);
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
    xl6OutputPuts("    text = (char*)xrtTemp(totalLen + 1);\n", out);
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
    xl6OutputPuts("    copy = (char*)xrtTemp(len + 1);\n", out);
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
	xl6OutputPuts("static inline void* xl6_class_data(xvalue* value, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* data = value != 0 ? xl6ValueRecordData(value) : 0;\n", out);
    xl6OutputPuts("    if ( data == 0 && value != 0 ) { data = xl6ValueHandleSlot(value); }\n", out);
    xl6OutputPuts("    if ( data == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"NullReferenceError\", \"null class dereference\", file, line, column);\n", out);
    xl6OutputPuts("        xl6_runtime_fatal(4);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return data;\n", out);
    xl6OutputPuts("}\n", out);
	xl6OutputPuts("static inline bool xl6_class_identity_equal_release(xvalue* left, xvalue* right)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    bool leftNull = left == 0 || xl6_xrt_value_is_null(left);\n", out);
    xl6OutputPuts("    bool rightNull = right == 0 || xl6_xrt_value_is_null(right);\n", out);
    xl6OutputPuts("    bool equal = (leftNull && rightNull) || (!leftNull && !rightNull && left == right);\n", out);
    xl6OutputPuts("    if ( left != 0 ) { xrtValueRelease(left); }\n", out);
    xl6OutputPuts("    if ( right != 0 ) { xrtValueRelease(right); }\n", out);
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
