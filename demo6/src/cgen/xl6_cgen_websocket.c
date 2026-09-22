#include "src/cgen/xl6_cgen_websocket.h"
#include "src/cgen/xl6_cgen_native_void_abi.h"

#define XL6_WEBSOCKET_SOURCE(...) #__VA_ARGS__

#include "xl6_cgen_websocket_protocol.c"
#include "xl6_cgen_websocket_stream.c"



/* 按需输出相互独立的 WebSocket 协议片段。 */
void xl6CgenEmitWebSocketProtocolRuntime(
	XL6_Output* out,
	int hasFrame,
	int hasHandshake,
	int hasUpgrade,
	int hasStreamConfig,
	int hasStream,
	int hasStreamTls
)
{
	if ( out == NULL ) {
		return;
	}
	xl6CgenEmitNativeVoidAbiBegin(out);
	xl6OutputPuts(
		"static char* xlir_ws_string_dup(const char* value, size_t size)\n"
		"{ const char* source = value != NULL ? value : \"\"; return size == 0 ? xrtStrDup(source) : xrtStrDupN(source, size); }\n"
		"#define xl6_xrt_string_dup xlir_ws_string_dup\n",
		out);
	if ( hasStream ) {
		xl6OutputPuts(
				"static XL6_Function xlir_ws_function_copy_value(XL6_Function value)\n"
				"{ if (value.retain != NULL && value.env != NULL) value.retain(value.env); return value; }\n"
				"static void xlir_ws_function_release_value(XL6_Function value)\n"
				"{ if (value.release != NULL && value.env != NULL) value.release(value.env); }\n"
				"static xvalue* xlir_ws_function_call_var(XL6_Function function, int count, ...)\n"
				"{ xvalue* arguments[6] = {0}; xvalue* result = NULL; va_list values; int index; if (count != 6 || function.invoke_var == NULL) return xrtValueNull(); va_start(values, count); for (index = 0; index < count; ++index) arguments[index] = va_arg(values, xvalue*); va_end(values); result = function.invoke_var(function.env, count, arguments); for (index = 0; index < count; ++index) xrtValueRelease(arguments[index]); return result != NULL ? result : xrtValueNull(); }\n"
				"static void* xlir_ws_owner_handle(xvalue* owner)\n"
				"{ void* handle = NULL; return owner != NULL && xrtValueGetPointer(owner, &handle) ? handle : NULL; }\n"
				"static int xlir_ws_error_has(void) { return xrtGetError() != NULL; }\n"
				"static void xlir_ws_error_publish_xrt(void) { }\n"
				"#define xl6_function_copy_value xlir_ws_function_copy_value\n"
				"#define xl6_function_release_value xlir_ws_function_release_value\n"
				"#define xl6_function_call_var xlir_ws_function_call_var\n"
				"#define xl6ValueHandleData xlir_ws_owner_handle\n"
				"#define xl6_error_has xlir_ws_error_has\n"
				"#define xl6_error_publish_xrt xlir_ws_error_publish_xrt\n",
			out);
	}
	if ( hasFrame ) {
		xl6OutputPuts(xl6WebSocketFrameSource, out);
		xl6OutputPuts("\n\n", out);
	}
	if ( hasHandshake || hasUpgrade ) {
		xl6OutputPuts(xl6WebSocketHandshakeSource, out);
		xl6OutputPuts("\n\n", out);
	}
	if ( hasUpgrade ) {
		xl6OutputPuts(xl6WebSocketUpgradeSource, out);
		xl6OutputPuts("\n\n", out);
	}
	if ( hasStreamConfig || hasStream ) {
		xl6OutputPuts(xl6WebSocketStreamConfigSource, out);
		xl6OutputPuts("\n\n", out);
	}
	if ( hasStream ) {
		xl6OutputPuts(xl6WebSocketStreamSource, out);
		xl6OutputPuts("\n\n", out);
	}
	if ( hasStreamTls ) {
		xl6OutputPuts(xl6WebSocketStreamTlsSource, out);
		xl6OutputPuts("\n\n", out);
	}
	xl6CgenEmitNativeVoidAbiEnd(out);
	if ( hasStream ) {
		xl6OutputPuts(
				"#undef xl6_error_publish_xrt\n"
				"#undef xl6_error_has\n"
				"#undef xl6ValueHandleData\n"
				"#undef xl6_function_call_var\n"
				"#undef xl6_function_release_value\n"
				"#undef xl6_function_copy_value\n",
			out);
	}
	xl6OutputPuts("#undef xl6_xrt_string_dup\n\n", out);
}



#undef XL6_WEBSOCKET_SOURCE
