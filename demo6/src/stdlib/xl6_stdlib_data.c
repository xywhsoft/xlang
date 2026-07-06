#include "src/stdlib/xl6_stdlib_data.h"

const char* xl6StdlibCoreClassSource =
    "class object\n"
    "    var __fields = dict{}\n"
    "\n"
    "    func __field__(string name) as var\n"
    "        return self.__fields[name]\n"
    "    end\n"
    "\n"
    "    func __setField__(string name, var value)\n"
    "        self.__fields[name] = value\n"
    "    end\n"
    "\n"
    "    func __item__(string name) as var\n"
    "        return self.__fields[name]\n"
    "    end\n"
    "\n"
    "    func __setItem__(string name, var value)\n"
    "        self.__fields[name] = value\n"
    "    end\n"
    "end\n";

const char* xl6StdlibRandomClassSource =
    "class Random\n"
    "    native point handle\n"
    "    extern \"xrtRandCreate\" create(uint64 seed, uint64 seq)\n"
    "    extern \"xrtRandDestroy\" destroy()\n"
    "    extern \"xrtRandSeedObj\" func seed(uint64 seed, uint64 seq)\n"
    "    extern \"xrtRand32Obj\" func u32() as uint32\n"
    "    extern \"xrtRand64Obj\" func u64() as uint64\n"
    "    extern \"xrtRandRangeObj\" func rangeNative(c_int min, c_int max) as c_int\n"
    "    extern \"xrtRandStrObj\" func stringNative(string chars, c_size size, c_size len) as point\n"
    "\n"
    "    func range(int min, int max) as int\n"
    "        return int(self.rangeNative(c_int(min), c_int(max)))\n"
    "    end\n"
    "\n"
    "    func string(int len = 16, string chars = \"\") as string\n"
    "        if len < 0 then\n"
    "            return \"\"\n"
    "        end\n"
    "        return __xl_std_runtime_take_cstr(self.stringNative(chars, c_size(0), c_size(len)))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibBufferClassSource =
    "class Buffer\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_buffer_create\" create()\n"
    "    extern \"xl6_xrt_buffer_create_step\" create(int step)\n"
    "    extern \"xl6_xrt_buffer_wrap_handle\" create(point handle)\n"
    "    extern \"xl6_xrt_buffer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_buffer_len\" func len() as int\n"
    "    extern \"xl6_xrt_buffer_capacity\" func capacity() as int\n"
    "    extern \"xl6_xrt_buffer_reserve\" func reserve(int size) as bool\n"
    "    extern \"xl6_xrt_buffer_clear\" func clear() as bool\n"
    "    extern \"xl6_xrt_buffer_append_string\" func append(string text) as bool\n"
    "    extern \"xl6_xrt_buffer_append_bytes\" func appendBytes(string data, c_size size) as bool\n"
    "    extern \"xl6_xrt_buffer_append_byte\" func appendByte(int value) as bool\n"
    "    extern \"xl6_xrt_buffer_byte_at\" func byteAt(int index) as int\n"
    "    extern \"xl6_xrt_buffer_set_byte\" func setByte(int index, int value) as bool\n"
    "    extern \"xl6_xrt_buffer_text\" func textNative() as point\n"
    "    extern \"xl6_xrt_buffer_data\" func data() as point\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "\n"
    "    func __len__() as int\n"
    "        return self.len()\n"
    "    end\n"
    "\n"
    "    func __item__(int index) as int\n"
    "        return self.byteAt(index)\n"
    "    end\n"
    "\n"
    "    func __setItem__(int index, int value)\n"
    "        self.setByte(index, value)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibLoggerClassSource =
    "class Logger\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_log_create\" create(string name)\n"
    "    extern \"xl6_xrt_log_wrap_handle\" create(point handle)\n"
    "    extern \"xl6_xrt_log_destroy\" destroy()\n"
    "    extern \"xl6_xrt_log_set_level\" func setLevel(int level)\n"
    "    extern \"xl6_xrt_log_get_level\" func getLevel() as int\n"
    "    extern \"xl6_xrt_log_add_console\" func addConsoleNative(int level, bool color, int format) as bool\n"
    "    extern \"xl6_xrt_log_add_file\" func addFileNative(string path, int level, int format) as bool\n"
    "    extern \"xl6_xrt_log_add_rolling_file\" func addRollingFileNative(string path, int maxSize, int maxBackup, int level, int format) as bool\n"
    "    extern \"xl6_xrt_log_write\" func write(int level, string message)\n"
    "    extern \"xl6_xrt_log_flush\" func flush()\n"
    "\n"
    "    func addConsole(int level = 0, bool color = true, int format = 0) as bool\n"
    "        return self.addConsoleNative(level, color, format)\n"
    "    end\n"
    "\n"
    "    func addFile(string path, int level = 2, int format = 0) as bool\n"
    "        return self.addFileNative(path, level, format)\n"
    "    end\n"
    "\n"
    "    func addRollingFile(string path, int maxSize = 10485760, int maxBackup = 5, int level = 2, int format = 0) as bool\n"
    "        return self.addRollingFileNative(path, maxSize, maxBackup, level, format)\n"
    "    end\n"
    "\n"
    "    func trace(string message)\n"
    "        self.write(0, message)\n"
    "    end\n"
    "\n"
    "    func debug(string message)\n"
    "        self.write(1, message)\n"
    "    end\n"
    "\n"
    "    func info(string message)\n"
    "        self.write(2, message)\n"
    "    end\n"
    "\n"
    "    func warn(string message)\n"
    "        self.write(3, message)\n"
    "    end\n"
    "\n"
    "    func error(string message)\n"
    "        self.write(4, message)\n"
    "    end\n"
    "\n"
    "    func fatal(string message)\n"
    "        self.write(5, message)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibTemplateClassSource =
    "class Template\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_template_parse\" create(string text, string bracket)\n"
    "    extern \"xl6_xrt_template_destroy\" destroy()\n"
    "    extern \"xl6_xrt_template_node_count\" get nodeCount() as uint32\n"
    "    extern \"xl6_xrt_template_expr_count\" get exprCount() as uint32\n"
    "    extern \"xl6_xrt_template_arg_count\" get argCount() as uint32\n"
    "    extern \"xl6_xrt_template_string_pool_size\" get stringPoolSize() as uint32\n"
    "    extern \"xl6_xrt_template_render\" func renderNative(var data) as point\n"
    "\n"
    "    func render(var data = null) as string\n"
    "        return __xl_std_runtime_take_cstr(self.renderNative(data))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibCryptoClassSource =
    "class Sha256\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha256_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha256_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha256_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha256_stream_hex\" func hexNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class Sha384\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha384_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha384_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha384_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha384_stream_hex\" func hexNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class Sha512\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha512_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha512_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha512_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha512_stream_hex\" func hexNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "end\n";

const char* xl6StdlibHttpServerClassSource =
    "class HttpRequest\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_request_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_request_method\" func methodNative() as point\n"
    "    extern \"xl6_xrt_http_request_method_id\" func methodId() as int\n"
    "    extern \"xl6_xrt_http_request_target\" func targetNative() as point\n"
    "    extern \"xl6_xrt_http_request_path\" func pathNative() as point\n"
    "    extern \"xl6_xrt_http_request_query\" func queryNative() as point\n"
    "    extern \"xl6_xrt_http_request_query_count\" func queryCount() as int\n"
    "    extern \"xl6_xrt_http_request_query_has_value\" func queryHasValue(int index) as bool\n"
    "    extern \"xl6_xrt_http_request_query_name\" func queryNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_query_value_at\" func queryValueAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_header\" func headerNative(string name) as point\n"
    "    extern \"xl6_xrt_http_request_header_count\" func headerCount() as int\n"
    "    extern \"xl6_xrt_http_request_header_name\" func headerNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_header_value\" func headerValueNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_param\" func paramNative(string name) as point\n"
    "    extern \"xl6_xrt_http_request_param_count\" func paramCount() as int\n"
    "    extern \"xl6_xrt_http_request_param_name\" func paramNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_param_value\" func paramValueNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_query_value\" func queryValueNative(string name) as point\n"
    "    extern \"xl6_xrt_http_request_cookie\" func cookieNative(string name) as point\n"
    "    extern \"xl6_xrt_http_request_cookie_count\" func cookieCount() as int\n"
    "    extern \"xl6_xrt_http_request_cookie_name\" func cookieNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_cookie_value_at\" func cookieValueAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_body_text\" func bodyNative() as point\n"
    "    extern \"xl6_xrt_http_request_body_data\" func bodyData() as point\n"
    "    extern \"xl6_xrt_http_request_body_size\" func bodySize() as int\n"
    "    extern \"xl6_xrt_http_request_body_buffer\" func bodyBytesNative() as point\n"
    "    extern \"xl6_xrt_http_request_content_length\" func contentLength() as int\n"
    "    extern \"xl6_xrt_http_request_content_type\" func contentTypeNative() as point\n"
    "    extern \"xl6_xrt_http_request_is_form_urlencoded\" func isFormUrlEncoded() as bool\n"
    "    extern \"xl6_xrt_http_request_form_count\" func formCount() as int\n"
    "    extern \"xl6_xrt_http_request_form_has_value\" func formHasValue(int index) as bool\n"
    "    extern \"xl6_xrt_http_request_form_name\" func formNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_form_value_at\" func formValueAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_form_value\" func formValueNative(string name) as point\n"
    "    extern \"xl6_xrt_http_request_is_multipart_form\" func isMultipartForm() as bool\n"
    "    extern \"xl6_xrt_http_request_multipart_count\" func multipartCount() as int\n"
    "    extern \"xl6_xrt_http_request_multipart_is_file\" func multipartIsFile(int index) as bool\n"
    "    extern \"xl6_xrt_http_request_multipart_name\" func multipartNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_multipart_file_name\" func multipartFileNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_multipart_content_type\" func multipartContentTypeNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_multipart_size\" func multipartSize(int index) as int\n"
    "    extern \"xl6_xrt_http_request_multipart_text\" func multipartTextNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_multipart_bytes\" func multipartBytesNative(int index) as point\n"
    "    extern \"xl6_xrt_http_request_multipart_value\" func multipartValueNative(string name) as point\n"
    "    extern \"xl6_xrt_http_request_local_addr\" func localAddrNative() as point\n"
    "    extern \"xl6_xrt_http_request_remote_addr\" func remoteAddrNative() as point\n"
    "    extern \"xl6_xrt_http_request_tls_sni\" func tlsSNINative() as point\n"
    "    extern \"xl6_xrt_http_request_local_port\" func localPort() as int\n"
    "    extern \"xl6_xrt_http_request_remote_port\" func remotePort() as int\n"
    "\n"
    "    func method() as string\n"
    "        return __xl_std_runtime_take_cstr(self.methodNative())\n"
    "    end\n"
    "\n"
    "    func target() as string\n"
    "        return __xl_std_runtime_take_cstr(self.targetNative())\n"
    "    end\n"
    "\n"
    "    func path() as string\n"
    "        return __xl_std_runtime_take_cstr(self.pathNative())\n"
    "    end\n"
    "\n"
    "    func query() as string\n"
    "        return __xl_std_runtime_take_cstr(self.queryNative())\n"
    "    end\n"
    "\n"
    "    func queryName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.queryNameNative(index))\n"
    "    end\n"
    "\n"
    "    func queryValueAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.queryValueAtNative(index))\n"
    "    end\n"
    "\n"
    "    func header(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerNative(name))\n"
    "    end\n"
    "\n"
    "    func headerName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerNameNative(index))\n"
    "    end\n"
    "\n"
    "    func headerValue(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerValueNative(index))\n"
    "    end\n"
    "\n"
    "    func param(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.paramNative(name))\n"
    "    end\n"
    "\n"
    "    func paramName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.paramNameNative(index))\n"
    "    end\n"
    "\n"
    "    func paramValue(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.paramValueNative(index))\n"
    "    end\n"
    "\n"
    "    func queryValue(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.queryValueNative(name))\n"
    "    end\n"
    "\n"
    "    func cookie(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.cookieNative(name))\n"
    "    end\n"
    "\n"
    "    func cookieName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.cookieNameNative(index))\n"
    "    end\n"
    "\n"
    "    func cookieValueAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.cookieValueAtNative(index))\n"
    "    end\n"
    "\n"
    "    func body() as string\n"
    "        return __xl_std_runtime_take_cstr(self.bodyNative())\n"
    "    end\n"
    "\n"
    "    func bodyBytes() as Buffer\n"
    "        point handle = self.bodyBytesNative()\n"
    "        return Buffer(handle)\n"
    "    end\n"
    "\n"
    "    func contentType() as string\n"
    "        return __xl_std_runtime_take_cstr(self.contentTypeNative())\n"
    "    end\n"
    "\n"
    "    func formName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.formNameNative(index))\n"
    "    end\n"
    "\n"
    "    func formValueAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.formValueAtNative(index))\n"
    "    end\n"
    "\n"
    "    func formValue(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.formValueNative(name))\n"
    "    end\n"
    "\n"
    "    func multipartName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.multipartNameNative(index))\n"
    "    end\n"
    "\n"
    "    func multipartFileName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.multipartFileNameNative(index))\n"
    "    end\n"
    "\n"
    "    func multipartContentType(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.multipartContentTypeNative(index))\n"
    "    end\n"
    "\n"
    "    func multipartText(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.multipartTextNative(index))\n"
    "    end\n"
    "\n"
    "    func multipartBytes(int index) as Buffer\n"
    "        point handle = self.multipartBytesNative(index)\n"
    "        return Buffer(handle)\n"
    "    end\n"
    "\n"
    "    func multipartValue(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.multipartValueNative(name))\n"
    "    end\n"
    "\n"
    "    func localAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.localAddrNative())\n"
    "    end\n"
    "\n"
    "    func remoteAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.remoteAddrNative())\n"
    "    end\n"
    "\n"
    "    func tlsSNI() as string\n"
    "        return __xl_std_runtime_take_cstr(self.tlsSNINative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpResponse\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_response_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_response_status\" func statusNative(int code, string reason)\n"
    "    extern \"xl6_xrt_http_response_header\" func header(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_response_add_header\" func addHeader(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_response_cookie\" func cookieNative(string name, string value, string path, int maxAge, int sameSite, c_uint flags) as bool\n"
    "    extern \"xl6_xrt_http_response_delete_cookie\" func deleteCookieNative(string name, string path) as bool\n"
    "    extern \"xl6_xrt_http_response_text\" func textNative(string text, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_response_json_text\" func jsonText(string text) as bool\n"
    "    extern \"xl6_xrt_http_response_body\" func bodyNative(point data, c_size size, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_response_start\" func start() as bool\n"
    "    extern \"xl6_xrt_http_response_send\" func send(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_http_response_write\" func write(string text) as bool\n"
    "    extern \"xl6_xrt_http_response_end\" func finish() as bool\n"
    "    extern \"xl6_xrt_http_response_file\" func fileNative(string path, c_size chunkSize) as bool\n"
    "    extern \"xl6_xrt_http_response_redirect\" func redirectNative(string url, int code) as bool\n"
    "\n"
    "    func status(int code, string reason = \"\")\n"
    "        self.statusNative(code, reason)\n"
    "    end\n"
    "\n"
    "    func text(string text, string contentType = \"text/plain; charset=utf-8\") as bool\n"
    "        return self.textNative(text, contentType)\n"
    "    end\n"
    "\n"
    "    func body(point data, c_size size, string contentType = \"application/octet-stream\") as bool\n"
    "        return self.bodyNative(data, size, contentType)\n"
    "    end\n"
    "\n"
    "    func cookie(string name, string value, string path = \"/\", int maxAge = -1, int sameSite = 0, int flags = 0) as bool\n"
    "        return self.cookieNative(name, value, path, maxAge, sameSite, c_uint(flags))\n"
    "    end\n"
    "\n"
    "    func deleteCookie(string name, string path = \"/\") as bool\n"
    "        return self.deleteCookieNative(name, path)\n"
    "    end\n"
    "\n"
    "    func file(string path, int chunkSize = 65536) as bool\n"
    "        return self.fileNative(path, c_size(chunkSize))\n"
    "    end\n"
    "\n"
    "    func redirect(string url, int code = 302) as bool\n"
    "        return self.redirectNative(url, code)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibHttpClientClassSource =
    "class HttpClientResponse\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_client_response_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_client_response_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_client_response_status\" func status() as int\n"
    "    extern \"xl6_xrt_http_client_response_reason\" func reasonNative() as point\n"
    "    extern \"xl6_xrt_http_client_response_header\" func headerNative(string name) as point\n"
    "    extern \"xl6_xrt_http_client_response_content_length\" func contentLength() as int\n"
    "    extern \"xl6_xrt_http_client_response_body_text\" func bodyNative() as point\n"
    "    extern \"xl6_xrt_http_client_response_body_data\" func bodyData() as point\n"
    "    extern \"xl6_xrt_http_client_response_body_size\" func bodySize() as int\n"
    "    extern \"xl6_xrt_http_client_response_body_buffer\" func bodyBytesNative() as point\n"
    "\n"
    "    func ok() as bool\n"
    "        int code = self.status()\n"
    "        return code >= 200 and code < 400\n"
    "    end\n"
    "\n"
    "    func reason() as string\n"
    "        return __xl_std_runtime_take_cstr(self.reasonNative())\n"
    "    end\n"
    "\n"
    "    func header(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerNative(name))\n"
    "    end\n"
    "\n"
    "    func body() as string\n"
    "        return __xl_std_runtime_take_cstr(self.bodyNative())\n"
    "    end\n"
    "\n"
    "    func bodyBytes() as Buffer\n"
    "        point handle = self.bodyBytesNative()\n"
    "        return Buffer(handle)\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpClientRequest\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_client_request_create\" create()\n"
    "    extern \"xl6_xrt_http_client_request_create_url\" create(string url)\n"
    "    extern \"xl6_xrt_http_client_request_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_client_request_method\" func method(string method) as bool\n"
    "    extern \"xl6_xrt_http_client_request_url\" func url(string url) as bool\n"
    "    extern \"xl6_xrt_http_client_request_header\" func header(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_client_request_body\" func body(string data, c_size size, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_client_request_timeout\" func timeout(int timeoutMs)\n"
    "    extern \"xl6_xrt_http_client_request_idle_timeout\" func idleTimeout(int timeoutMs)\n"
    "    extern \"xl6_xrt_http_client_request_verify_peer\" func verifyPeer(bool value)\n"
    "    extern \"xl6_xrt_http_client_request_execute\" func executeNative() as point\n"
    "\n"
    "    func text(string data, string contentType = \"text/plain; charset=utf-8\") as bool\n"
    "        return self.body(data, __xl_std_crt_strlen(data), contentType)\n"
    "    end\n"
    "\n"
    "    func execute() as HttpClientResponse\n"
    "        return HttpClientResponse(self.executeNative())\n"
    "    end\n"
    "end\n";

const char* xl6StdlibNetClassSource =
    "class TcpStream\n"
    "    native point handle\n"
    "    extern \"xrtNetTcpStreamDestroy\" destroy()\n"
    "    extern \"xrtNetTcpStreamClose\" func close()\n"
    "    extern \"xrtNetStreamSendText\" func send(string text) as bool\n"
    "    extern \"xrtNetStreamSendTextAsync\" func sendAsync(string text) as future<bool>\n"
    "    extern \"xrtNetStreamAvailable\" func availableNative() as c_size\n"
    "    extern \"xrtNetStreamReadText\" func readNative(c_size maxBytes) as point\n"
    "    extern \"xrtNetStreamRecvText\" func recvNative(c_size maxBytes, int timeoutMs) as point\n"
    "    extern \"xrtNetStreamSendBytes\" func sendBytesRaw(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_tcp_stream_read_buffer\" func readBytesNative(c_size maxBytes) as point\n"
    "    extern \"xl6_xrt_tcp_stream_recv_buffer\" func recvBytesNative(c_size maxBytes, int timeoutMs) as point\n"
    "    extern \"xrtNetStreamRecvTextAsync\" func recvAsyncNative(c_size maxBytes, int timeoutMs) as future<string>\n"
    "    extern \"xrtNetStreamDrain\" func drainNative(int timeoutMs) as bool\n"
    "    extern \"xrtNetStreamDrainAsync\" func drainAsyncNative(int timeoutMs) as future<bool>\n"
    "    extern \"xrtNetStreamLocalAddrText\" func localAddrNative() as point\n"
    "    extern \"xrtNetStreamRemoteAddrText\" func remoteAddrNative() as point\n"
    "    extern \"xrtNetStreamLocalPort\" func localPort() as int\n"
    "    extern \"xrtNetStreamRemotePort\" func remotePort() as int\n"
    "\n"
    "    func available() as int\n"
    "        return int(self.availableNative())\n"
    "    end\n"
    "\n"
    "    func read(int maxBytes = 0) as string\n"
    "        return __xl_std_runtime_take_cstr(self.readNative(c_size(maxBytes)))\n"
    "    end\n"
    "\n"
    "    func recv(int timeoutMs = -1, int maxBytes = 0) as string\n"
    "        return __xl_std_runtime_take_cstr(self.recvNative(c_size(maxBytes), timeoutMs))\n"
    "    end\n"
    "\n"
    "    func sendBytes(point data, c_size size) as bool\n"
    "        return self.sendBytesRaw(data, size)\n"
    "    end\n"
    "\n"
    "    func readBytes(int maxBytes = 0) as Buffer\n"
    "        point handle = self.readBytesNative(c_size(maxBytes))\n"
    "        return Buffer(handle)\n"
    "    end\n"
    "\n"
    "    func recvBytes(int timeoutMs = -1, int maxBytes = 0) as Buffer\n"
    "        point handle = self.recvBytesNative(c_size(maxBytes), timeoutMs)\n"
    "        return Buffer(handle)\n"
    "    end\n"
    "\n"
    "    func recvAsync(int timeoutMs = -1, int maxBytes = 0) as future<string>\n"
    "        return self.recvAsyncNative(c_size(maxBytes), timeoutMs)\n"
    "    end\n"
    "\n"
    "    func drain(int timeoutMs = -1) as bool\n"
    "        return self.drainNative(timeoutMs)\n"
    "    end\n"
    "\n"
    "    func drainAsync(int timeoutMs = -1) as future<bool>\n"
    "        return self.drainAsyncNative(timeoutMs)\n"
    "    end\n"
    "\n"
    "    func localAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.localAddrNative())\n"
    "    end\n"
    "\n"
    "    func remoteAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.remoteAddrNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class TcpServer\n"
    "    native point handle\n"
    "    extern \"xrtNetTcpListenerDestroy\" destroy()\n"
    "    extern \"xrtNetTcpListenerPort\" func port() as uint16\n"
    "    extern \"xrtNetTcpAccept\" func acceptNative(int timeoutMs) as point\n"
    "    extern \"xrtNetTcpAcceptAsync\" func acceptAsyncNative(int timeoutMs) as future<point>\n"
    "\n"
    "    func accept(int timeoutMs = -1) as point\n"
    "        return self.acceptNative(timeoutMs)\n"
    "    end\n"
    "\n"
    "    func acceptAsync(int timeoutMs = -1) as future<point>\n"
    "        return self.acceptAsyncNative(timeoutMs)\n"
    "    end\n"
    "end\n"
    "\n"
    "class UdpPacket\n"
    "    native point handle\n"
    "    extern \"xrtNetDgramPacketDestroy\" destroy()\n"
    "    extern \"xrtNetDgramPacketText\" func textNative() as point\n"
    "    extern \"xl6_xrt_udp_packet_buffer\" func bytesNative() as point\n"
    "    extern \"xrtNetDgramPacketFromText\" func addrNative() as point\n"
    "    extern \"xrtNetDgramPacketFromPort\" func port() as int\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "\n"
    "    func bytes() as Buffer\n"
    "        point handle = self.bytesNative()\n"
    "        return Buffer(handle)\n"
    "    end\n"
    "\n"
    "    func addr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.addrNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class UdpSocket\n"
    "    native point handle\n"
    "    extern \"xrtNetUdpDestroy\" destroy()\n"
    "    extern \"xrtNetUdpLocalPort\" func port() as int\n"
    "    extern \"xrtNetUdpLocalAddrText\" func localAddrNative() as point\n"
    "    extern \"xrtNetUdpSendText\" func send(string host, uint16 port, string text) as bool\n"
    "    extern \"xrtNetUdpSendBytes\" func sendBytesRaw(string host, uint16 port, point data, c_size size) as bool\n"
    "    extern \"xrtNetUdpSendTextAsync\" func sendAsync(string host, uint16 port, string text) as future<bool>\n"
    "    extern \"xrtNetUdpRecv\" func recvNative(int timeoutMs) as point\n"
    "    extern \"xrtNetUdpRecvAsync\" func recvAsyncNative(int timeoutMs) as future<point>\n"
    "\n"
    "    func localAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.localAddrNative())\n"
    "    end\n"
    "\n"
    "    func recv(int timeoutMs = -1) as point\n"
    "        return self.recvNative(timeoutMs)\n"
    "    end\n"
    "\n"
    "    func sendBytes(string host, uint16 port, point data, c_size size) as bool\n"
    "        return self.sendBytesRaw(host, port, data, size)\n"
    "    end\n"
    "\n"
    "    func recvAsync(int timeoutMs = -1) as future<point>\n"
    "        return self.recvAsyncNative(timeoutMs)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibFileInfoClassSource =
    "class FileInfo\n"
    "    string path\n"
    "    bool exists\n"
    "    bool isFile\n"
    "    bool isDir\n"
    "    int size\n"
    "    int attr\n"
    "    time accessTime\n"
    "    time changeTime\n"
    "end\n";

const char* xl6StdlibFileClassSource =
    "class File\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_file_open\" create(string path, bool readOnly, int charset)\n"
    "    extern \"xl6_xrt_file_close\" destroy()\n"
    "    extern \"xl6_xrt_file_tell\" get pos() as int\n"
    "    extern \"xl6_xrt_file_size\" get size() as int\n"
    "    extern \"xl6_xrt_file_eof\" get eof() as bool\n"
    "    extern \"xl6_xrt_file_seek\" func seekNative(int offset, int whence) as int\n"
    "    extern \"xl6_xrt_file_set_eof\" func setEOF() as bool\n"
    "    extern \"xl6_xrt_file_read_text\" func readNative(int size) as point\n"
    "    extern \"xl6_xrt_file_write_text\" func writeTextNative(string text, c_size size) as int\n"
    "\n"
    "    func seek(int offset, int whence = 0) as int\n"
    "        return self.seekNative(offset, whence)\n"
    "    end\n"
    "\n"
    "    func readText(int size = -1) as string\n"
    "        return __xl_std_runtime_take_cstr(self.readNative(size))\n"
    "    end\n"
    "\n"
    "    func writeText(string text) as int\n"
    "        return self.writeTextNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func write(string text) as int\n"
    "        return self.writeText(text)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibRegexClassSource =
    "class RegexMatch\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_regex_match_destroy\" destroy()\n"
    "    extern \"xl6_xrt_regex_match_ok\" get ok() as bool\n"
    "    extern \"xl6_xrt_regex_match_byte_start\" get byteStart() as int\n"
    "    extern \"xl6_xrt_regex_match_byte_end\" get byteEnd() as int\n"
    "    extern \"xl6_xrt_regex_match_start\" get start() as int\n"
    "    extern \"xl6_xrt_regex_match_end\" get end() as int\n"
    "    extern \"xl6_xrt_regex_match_text_copy\" func textNative() as point\n"
    "\n"
    "    get text() as var\n"
    "        if not self.ok then\n"
    "            return null\n"
    "        end\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class Regex\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_regex_create_default\" create(string pattern)\n"
    "    extern \"xl6_xrt_regex_create_flags\" create(string pattern, int flags)\n"
    "    extern \"xl6_xrt_regex_destroy\" destroy()\n"
    "    extern \"xl6_xrt_regex_is_match_native\" func isMatchNative(string text, c_size textSize) as c_int\n"
    "    extern \"xl6_xrt_regex_full_match_native\" func fullMatchNative(string text, c_size textSize) as c_int\n"
    "    extern \"xl6_xrt_regex_match_at\" func matchNative(string text, c_size textSize, c_size start) as point\n"
    "    extern \"xl6_xrt_regex_capture_count\" get captureCount() as uint32\n"
    "    extern \"xl6_xrt_regex_capture_name_copy\" func captureNameNative(uint32 index) as point\n"
    "\n"
    "    func isMatch(string text) as bool\n"
    "        return self.isMatchNative(text, __xl_std_crt_strlen(text)) != 0\n"
    "    end\n"
    "\n"
    "    func fullMatch(string text) as bool\n"
    "        return self.fullMatchNative(text, __xl_std_crt_strlen(text)) != 0\n"
    "    end\n"
    "\n"
    "    func match(string text, int start = 0) as RegexMatch\n"
    "        if start < 0 then\n"
    "            start = 0\n"
    "        end\n"
    "        return RegexMatch(self.matchNative(text, __xl_std_crt_strlen(text), c_size(start)))\n"
    "    end\n"
    "\n"
    "    func find(string text, int start = 0) as RegexMatch\n"
    "        return self.match(text, start)\n"
    "    end\n"
    "\n"
    "    func captureName(int index) as string\n"
    "        if index < 0 then\n"
    "            return \"\"\n"
    "        end\n"
    "        return __xl_std_runtime_take_cstr(self.captureNameNative(uint32(index)))\n"
    "    end\n"
    "end\n"
    "\n"
    "class RegexSet\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_regex_set_destroy\" destroy()\n"
    "    extern \"xl6_xrt_regex_set_is_match\" func isMatchNative(string text, c_size textSize, c_size start) as bool\n"
    "    extern \"xl6_xrt_regex_set_first\" func firstNative(string text, c_size textSize, c_size start) as int\n"
    "    extern \"xl6_xrt_regex_set_matches\" func matchesNative(string text, c_size textSize, c_size start, uint32 maxCount) as var\n"
    "\n"
    "    func isMatch(string text, int start = 0) as bool\n"
    "        if start < 0 then\n"
    "            start = 0\n"
    "        end\n"
    "        return self.isMatchNative(text, __xl_std_crt_strlen(text), c_size(start))\n"
    "    end\n"
    "\n"
    "    func first(string text, int start = 0) as int\n"
    "        if start < 0 then\n"
    "            start = 0\n"
    "        end\n"
    "        return self.firstNative(text, __xl_std_crt_strlen(text), c_size(start))\n"
    "    end\n"
    "\n"
    "    func matches(string text, int start = 0, int maxCount = 32) as array\n"
    "        if start < 0 then\n"
    "            start = 0\n"
    "        end\n"
    "        if maxCount < 0 then\n"
    "            maxCount = 0\n"
    "        end\n"
    "        return array(self.matchesNative(text, __xl_std_crt_strlen(text), c_size(start), uint32(maxCount)))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibProcessClassSource =
    "class ProcessResult\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_process_result_destroy\" destroy()\n"
    "    extern \"xl6_xrt_process_result_exit_code\" get exitCode() as int\n"
    "    extern \"xl6_xrt_process_result_success\" get success() as bool\n"
    "    extern \"xl6_xrt_process_result_timed_out\" get timedOut() as bool\n"
    "    extern \"xl6_xrt_process_result_cancelled\" get cancelled() as bool\n"
    "    extern \"xl6_xrt_process_result_spawn_failed\" get spawnFailed() as bool\n"
    "    extern \"xl6_xrt_process_result_killed\" get killed() as bool\n"
    "    extern \"xl6_xrt_process_result_stdout_truncated\" get stdoutTruncated() as bool\n"
    "    extern \"xl6_xrt_process_result_stderr_truncated\" get stderrTruncated() as bool\n"
    "    extern \"xl6_xrt_process_result_duration_ms\" get durationMs() as int\n"
    "    extern \"xl6_xrt_process_result_stdout_copy\" func stdoutNative() as point\n"
    "    extern \"xl6_xrt_process_result_stderr_copy\" func stderrNative() as point\n"
    "\n"
    "    get stdout() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stdoutNative())\n"
    "    end\n"
    "\n"
    "    get stderr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stderrNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class ProcessCommand\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_process_command_create_file\" create(string program, array args)\n"
    "    extern \"xl6_xrt_process_command_create_shell\" create(string command)\n"
    "    extern \"xl6_xrt_process_command_destroy\" destroy()\n"
    "end\n"
    "\n"
    "class PipelineResult\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_process_pipeline_destroy\" destroy()\n"
    "    extern \"xl6_xrt_process_pipeline_success\" get success() as bool\n"
    "    extern \"xl6_xrt_process_pipeline_exit_code\" get exitCode() as int\n"
    "    extern \"xl6_xrt_process_pipeline_failed_index\" get failedIndex() as int\n"
    "    extern \"xl6_xrt_process_pipeline_count\" get count() as int\n"
    "    extern \"xl6_xrt_process_pipeline_completed_count\" get completedCount() as int\n"
    "    extern \"xl6_xrt_process_pipeline_timed_out\" get timedOut() as bool\n"
    "    extern \"xl6_xrt_process_pipeline_duration_ms\" get durationMs() as int\n"
    "    extern \"xl6_xrt_process_pipeline_stdout_copy\" func stdoutNative() as point\n"
    "    extern \"xl6_xrt_process_pipeline_stderr_copy\" func stderrNative() as point\n"
    "    extern \"xl6_xrt_process_pipeline_result_at\" func resultNative(int index) as point\n"
    "\n"
    "    get stdout() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stdoutNative())\n"
    "    end\n"
    "\n"
    "    get stderr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stderrNative())\n"
    "    end\n"
    "\n"
    "    func result(int index) as ProcessResult\n"
    "        return ProcessResult(self.resultNative(index))\n"
    "    end\n"
    "end\n"
    "\n"
    "class Process\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_process_spawn_file\" create(string program, array args)\n"
    "    extern \"xl6_xrt_process_spawn_shell\" create(string command)\n"
    "    extern \"xl6_xrt_process_destroy\" destroy()\n"
    "    extern \"xl6_xrt_process_running\" get running() as bool\n"
    "    extern \"xl6_xrt_process_handle_exit_code\" get exitCode() as int\n"
    "    extern \"xl6_xrt_process_write\" func write(string text) as bool\n"
    "    extern \"xl6_xrt_process_close_stdin\" func closeStdin() as bool\n"
    "    extern \"xl6_xrt_process_wait\" func waitNative(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_process_terminate\" func terminate() as bool\n"
    "    extern \"xl6_xrt_process_kill\" func kill() as bool\n"
    "    extern \"xl6_xrt_process_kill_tree\" func killTree() as bool\n"
    "    extern \"xl6_xrt_process_stdout_copy\" func stdoutNative() as point\n"
    "    extern \"xl6_xrt_process_stderr_copy\" func stderrNative() as point\n"
    "\n"
    "    func writeLine(string text) as bool\n"
    "        return self.write(text + \"\\n\")\n"
    "    end\n"
    "\n"
    "    func wait(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(timeoutMs)\n"
    "    end\n"
    "\n"
    "    get stdout() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stdoutNative())\n"
    "    end\n"
    "\n"
    "    get stderr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stderrNative())\n"
    "    end\n"
    "end\n";

const char* xl6StdlibTccClassSource =
    "class TccContext\n"
    "    native point handle\n"
    "    extern \"xl6_tcc_context_create\" create()\n"
    "    extern \"xl6_tcc_context_create_output\" create(c_int outputType)\n"
    "    extern \"xl6_tcc_context_destroy\" destroy()\n"
    "    extern \"tcc_set_options\" func setOptions(string options) as c_int\n"
    "    extern \"tcc_add_include_path\" func addIncludePath(string path) as c_int\n"
    "    extern \"tcc_add_sysinclude_path\" func addSysIncludePath(string path) as c_int\n"
    "    extern \"tcc_add_library_path\" func addLibraryPath(string path) as c_int\n"
    "    extern \"tcc_add_library\" func addLibrary(string name) as c_int\n"
    "    extern \"tcc_add_file\" func addFile(string path) as c_int\n"
    "    extern \"tcc_add_symbol\" func addSymbol(string name, point value) as c_int\n"
    "    extern \"tcc_define_symbol\" func defineSymbolNative(string name, string value)\n"
    "    extern \"tcc_undefine_symbol\" func undefineSymbol(string name)\n"
    "    extern \"tcc_set_output_type\" func setOutputType(c_int outputType) as c_int\n"
    "    extern \"tcc_compile_string\" func compileString(string source) as c_int\n"
    "    extern \"tcc_relocate\" func relocate() as c_int\n"
    "    extern \"tcc_get_symbol\" func getSymbol(string name) as point\n"
    "    extern \"xl6_tcc_context_run_args\" func runNative(array args) as c_int\n"
    "    extern \"xl6_tcc_context_output_file\" func outputFile(string path) as c_int\n"
    "\n"
    "    func defineSymbol(string name, string value = \"\")\n"
    "        self.defineSymbolNative(name, value)\n"
    "    end\n"
    "\n"
    "    func run() as int\n"
    "        return int(self.runNative([]))\n"
    "    end\n"
    "\n"
    "    func run(array args) as int\n"
    "        return int(self.runNative(args))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibXlangClassSource =
    "class XLangContext\n"
    "    native point handle\n"
    "    extern \"xl6_xlang_context_create\" create()\n"
    "    extern \"xl6_xlang_context_destroy\" destroy()\n"
    "    extern \"xlSetWorkDir\" func setWorkDir(string path) as bool\n"
    "    extern \"xlSetTempDir\" func setTempDir(string path) as bool\n"
    "    extern \"xlSetSaveTemps\" func setSaveTemps(bool enabled) as bool\n"
    "    extern \"xlRunScriptString\" func run(string source) as bool\n"
    "    extern \"xlRunScriptFile\" func runFile(string path) as bool\n"
    "    extern \"xlClearLastError\" func clearLastError()\n"
    "    extern \"xl6_xlang_context_last_error_copy\" func lastErrorNative() as point\n"
    "\n"
    "    get lastError() as string\n"
    "        return __xl_std_runtime_take_cstr(self.lastErrorNative())\n"
    "    end\n"
    "end\n";

static const XL6_StdlibParam xl6StdMathUnaryFloatParams[] = {
    {"float", "x"}
};

static const XL6_StdlibParam xl6StdMathPowParams[] = {
    {"float", "x"},
    {"float", "y"}
};

static const XL6_StdlibParam xl6StdMathAtan2Params[] = {
    {"float", "y"},
    {"float", "x"}
};

static const XL6_StdlibParam xl6StdMathBinaryFloatParams[] = {
    {"float", "a"},
    {"float", "b"}
};

static const XL6_StdlibParam xl6StdMathClampParams[] = {
    {"float", "value"},
    {"float", "minValue"},
    {"float", "maxValue"}
};

static const XL6_StdlibParam xl6StdMathModParams[] = {
    {"float", "value"},
    {"float", "divisor"}
};

static const XL6_StdlibParam xl6StdStringLenParams[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdXrtStrCompParams[] = {
    {"string", "left"},
    {"string", "right"},
    {"c_size", "size"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtInStrParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "part"},
    {"c_size", "partSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtCheckStrParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "chars"},
    {"c_size", "charsSize"}
};

static const XL6_StdlibParam xl6StdXrtStrLikeParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "pattern"},
    {"c_size", "patternSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtStrApproxParams[] = {
    {"string", "left"},
    {"c_size", "leftSize"},
    {"string", "right"},
    {"c_size", "rightSize"}
};

static const XL6_StdlibParam xl6StdXrtIsUtf8Params[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtIntFormatParams[] = {
    {"int", "value"},
    {"string", "format"}
};

static const XL6_StdlibParam xl6StdXrtNumFormatParams[] = {
    {"float", "value"},
    {"string", "format"}
};

static const XL6_StdlibParam xl6StdRuntimeTakeCstrParams[] = {
    {"point", "value"}
};

static const XL6_StdlibParam xl6StdRuntimePrintParams[] = {
    {"var", "values"},
    {"bool", "newline"}
};

static const XL6_StdlibParam xl6StdRuntimeStringSplitParams[] = {
    {"string", "text"},
    {"string", "sep"}
};

static const XL6_StdlibParam xl6StdRuntimeStringTextParams[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdRuntimeStringJoinParams[] = {
    {"var", "values"},
    {"string", "sep"}
};

static const XL6_StdlibParam xl6StdRuntimeStringEdgeParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "edge"},
    {"c_size", "edgeSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoHmacParams[] = {
    {"string", "key"},
    {"c_size", "keySize"},
    {"string", "text"},
    {"c_size", "textSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoAeadParams[] = {
    {"string", "key"},
    {"c_size", "keySize"},
    {"string", "nonce"},
    {"c_size", "nonceSize"},
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "aad"},
    {"c_size", "aadSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoHkdfParams[] = {
    {"string", "key"},
    {"c_size", "keySize"},
    {"string", "salt"},
    {"c_size", "saltSize"},
    {"string", "info"},
    {"c_size", "infoSize"},
    {"c_size", "outSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoRandomParams[] = {
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoChacha20Params[] = {
    {"string", "key"},
    {"c_size", "keySize"},
    {"string", "nonce"},
    {"c_size", "nonceSize"},
    {"string", "text"},
    {"c_size", "textSize"},
    {"c_uint", "counter"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoTextHexParams[] = {
    {"string", "hex"},
    {"c_size", "hexSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoSignParams[] = {
    {"string", "seedHex"},
    {"c_size", "seedHexSize"},
    {"string", "text"},
    {"c_size", "textSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoVerifyParams[] = {
    {"string", "publicKeyHex"},
    {"c_size", "publicKeyHexSize"},
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "signatureHex"},
    {"c_size", "signatureHexSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoSharedSecretParams[] = {
    {"string", "privateKeyHex"},
    {"c_size", "privateKeyHexSize"},
    {"string", "publicKeyHex"},
    {"c_size", "publicKeyHexSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoRsaVerifyParams[] = {
    {"string", "hashHex"},
    {"c_size", "hashHexSize"},
    {"string", "signatureHex"},
    {"c_size", "signatureHexSize"},
    {"string", "modulusHex"},
    {"c_size", "modulusHexSize"},
    {"string", "exponentHex"},
    {"c_size", "exponentHexSize"}
};

static const XL6_StdlibParam xl6StdRuntimeUrlPartParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_int", "part"}
};

static const XL6_StdlibParam xl6StdRuntimeUrlTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeUrlResolveParams[] = {
    {"string", "base"},
    {"c_size", "baseSize"},
    {"string", "ref"},
    {"c_size", "refSize"}
};

static const XL6_StdlibParam xl6StdRuntimeQueryTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeQueryPairParams[] = {
    {"string", "name"},
    {"c_size", "nameSize"},
    {"string", "value"},
    {"c_size", "valueSize"}
};

static const XL6_StdlibParam xl6StdRuntimeQueryAppendParams[] = {
    {"string", "query"},
    {"c_size", "querySize"},
    {"string", "name"},
    {"c_size", "nameSize"},
    {"string", "value"},
    {"c_size", "valueSize"}
};

static const XL6_StdlibParam xl6StdRuntimeRegexTextParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "pattern"},
    {"c_int", "flags"}
};

static const XL6_StdlibParam xl6StdRuntimeRegexMatchParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "pattern"},
    {"c_int", "flags"},
    {"c_size", "start"}
};

static const XL6_StdlibParam xl6StdRuntimeRegexSetCreateParams[] = {
    {"var", "patterns"},
    {"c_int", "flags"}
};

static const XL6_StdlibParam xl6StdRuntimeProcessExecParams[] = {
    {"string", "command"},
    {"c_int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdRuntimeProcessRunFileParams[] = {
    {"string", "program"},
    {"var", "args"},
    {"c_int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdRuntimeProcessPipelineParams[] = {
    {"var", "commands"},
    {"c_int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdRuntimeLogCreateParams[] = {
    {"string", "name"}
};

static const XL6_StdlibParam xl6StdRuntimeLogLevelParams[] = {
    {"int", "level"}
};

static const XL6_StdlibParam xl6StdRuntimeLogDefaultAddConsoleParams[] = {
    {"int", "level"},
    {"bool", "color"},
    {"int", "format"}
};

static const XL6_StdlibParam xl6StdRuntimeLogDefaultAddFileParams[] = {
    {"string", "path"},
    {"int", "level"},
    {"int", "format"}
};

static const XL6_StdlibParam xl6StdRuntimeLogDefaultAddRollingFileParams[] = {
    {"string", "path"},
    {"int", "maxSize"},
    {"int", "maxBackup"},
    {"int", "level"},
    {"int", "format"}
};

static const XL6_StdlibParam xl6StdRuntimeLogWriteParams[] = {
    {"int", "level"},
    {"string", "message"}
};

static const XL6_StdlibParam xl6StdTemplateCompileParams[] = {
    {"string", "text"},
    {"string", "bracket"}
};

static const XL6_StdlibParam xl6StdTemplateRenderParams[] = {
    {"string", "text"},
    {"var", "data"},
    {"string", "bracket"}
};

static const XL6_StdlibParam xl6StdTemplateRenderFileParams[] = {
    {"string", "path"},
    {"var", "data"},
    {"string", "bracket"}
};

static const XL6_StdlibParam xl6StdRuntimeDirListParams[] = {
    {"string", "path"},
    {"c_bool", "recursive"},
    {"c_int", "mode"}
};

static const XL6_StdlibParam xl6StdRuntimeDirGlobParams[] = {
    {"string", "path"},
    {"string", "pattern"},
    {"c_bool", "recursive"},
    {"c_int", "mode"}
};

static const XL6_StdlibParam xl6StdRuntimeDirEntriesParams[] = {
    {"string", "path"},
    {"c_bool", "recursive"},
    {"c_bool", "includeLeave"},
    {"string", "pattern"},
    {"c_int", "mode"}
};

static const XL6_StdlibParam xl6StdXrtDirCountParams[] = {
    {"string", "path"},
    {"c_bool", "recursive"},
    {"c_int", "mode"}
};

static const XL6_StdlibParam xl6StdXrtDirSizeParams[] = {
    {"string", "path"},
    {"c_bool", "recursive"}
};

static const XL6_StdlibParam xl6StdRuntimePathJoinParams[] = {
    {"string", "first"},
    {"string", "second"},
    {"string", "third"},
    {"string", "fourth"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerCreateParams[] = {
    {"string", "host"},
    {"c_int", "port"},
    {"c_uint", "backlog"},
    {"c_uint", "recvLimit"},
    {"c_uint", "bodyLimit"},
    {"c_uint", "headerLimit"},
    {"c_uint", "headerBytesLimit"},
    {"c_uint", "workerCount"},
    {"c_uint", "headerTimeoutMs"},
    {"c_uint", "bodyTimeoutMs"},
    {"c_uint", "idleTimeoutMs"},
    {"c_uint", "writeTimeoutMs"},
    {"c_uint", "multipartPartLimit"},
    {"c_uint", "multipartPartSizeLimit"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerCreateTlsParams[] = {
    {"string", "certFile"},
    {"string", "keyFile"},
    {"string", "host"},
    {"c_int", "port"},
    {"c_uint", "backlog"},
    {"c_uint", "recvLimit"},
    {"c_uint", "bodyLimit"},
    {"c_uint", "headerLimit"},
    {"c_uint", "headerBytesLimit"},
    {"c_uint", "workerCount"},
    {"c_uint", "headerTimeoutMs"},
    {"c_uint", "bodyTimeoutMs"},
    {"c_uint", "idleTimeoutMs"},
    {"c_uint", "writeTimeoutMs"},
    {"c_uint", "multipartPartLimit"},
    {"c_uint", "multipartPartSizeLimit"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerHandleParams[] = {
    {"point", "server"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerDrainParams[] = {
    {"point", "server"},
    {"int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppHandleParams[] = {
    {"point", "app"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerRouteParams[] = {
    {"point", "server"},
    {"string", "pattern"},
    {"func(string, string, string, string) as string", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerGenericRouteParams[] = {
    {"point", "server"},
    {"string", "method"},
    {"string", "pattern"},
    {"func(string, string, string, string) as string", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerRequestRouteParams[] = {
    {"point", "server"},
    {"string", "pattern"},
    {"func(point, point) as bool", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerGenericRequestRouteParams[] = {
    {"point", "server"},
    {"string", "method"},
    {"string", "pattern"},
    {"func(point, point) as bool", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerStreamRouteParams[] = {
    {"point", "server"},
    {"string", "pattern"},
    {"func(point) as bool", "beginHandler"},
    {"func(point, point, int) as bool", "chunkHandler"},
    {"func(point, point) as bool", "endHandler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerGenericStreamRouteParams[] = {
    {"point", "server"},
    {"string", "method"},
    {"string", "pattern"},
    {"func(point) as bool", "beginHandler"},
    {"func(point, point, int) as bool", "chunkHandler"},
    {"func(point, point) as bool", "endHandler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerErrorParams[] = {
    {"point", "server"},
    {"func(int, string, point, point) as bool", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppRouteParams[] = {
    {"point", "app"},
    {"string", "pattern"},
    {"func(string, string, string, string) as string", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppGenericRouteParams[] = {
    {"point", "app"},
    {"string", "method"},
    {"string", "pattern"},
    {"func(string, string, string, string) as string", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppRequestRouteParams[] = {
    {"point", "app"},
    {"string", "pattern"},
    {"func(point, point) as bool", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppGenericRequestRouteParams[] = {
    {"point", "app"},
    {"string", "method"},
    {"string", "pattern"},
    {"func(point, point) as bool", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppStreamRouteParams[] = {
    {"point", "app"},
    {"string", "pattern"},
    {"func(point) as bool", "beginHandler"},
    {"func(point, point, int) as bool", "chunkHandler"},
    {"func(point, point) as bool", "endHandler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppGenericStreamRouteParams[] = {
    {"point", "app"},
    {"string", "method"},
    {"string", "pattern"},
    {"func(point) as bool", "beginHandler"},
    {"func(point, point, int) as bool", "chunkHandler"},
    {"func(point, point) as bool", "endHandler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppErrorParams[] = {
    {"point", "app"},
    {"func(int, string, point, point) as bool", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerStaticParams[] = {
    {"point", "server"},
    {"string", "mount"},
    {"string", "root"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerStaticExParams[] = {
    {"point", "server"},
    {"string", "mount"},
    {"string", "root"},
    {"string", "indexNames"},
    {"string", "cacheControl"},
    {"c_size", "chunkSize"},
    {"uint32", "flags"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppStaticParams[] = {
    {"point", "app"},
    {"string", "mount"},
    {"string", "root"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpAppStaticExParams[] = {
    {"point", "app"},
    {"string", "mount"},
    {"string", "root"},
    {"string", "indexNames"},
    {"string", "cacheControl"},
    {"c_size", "chunkSize"},
    {"uint32", "flags"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerAppParams[] = {
    {"point", "server"},
    {"point", "app"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerHostParams[] = {
    {"point", "server"},
    {"string", "host"},
    {"point", "app"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerRemoveHostParams[] = {
    {"point", "server"},
    {"string", "host"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerTlsHostParams[] = {
    {"point", "server"},
    {"string", "host"},
    {"string", "certFile"},
    {"string", "keyFile"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerRemoveTlsHostParams[] = {
    {"point", "server"},
    {"string", "host"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpMimeByPathParams[] = {
    {"string", "path"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpBodyChunkParams[] = {
    {"point", "data"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtStringCaseParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_bool", "revise"}
};

static const XL6_StdlibParam xl6StdXrtStringTrimParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"string", "chars"},
    {"c_size", "charsSize"},
    {"c_bool", "revise"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringReplaceParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"string", "oldText"},
    {"c_size", "oldSize"},
    {"string", "newText"},
    {"c_size", "newSize"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringSizeParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtStringFindParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"string", "part"},
    {"c_size", "partSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtStringSliceParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"int", "start"},
    {"int", "count"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringCountParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"int", "count"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringIndexParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"int", "index"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringByteAtParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"int", "index"}
};

static const XL6_StdlibParam xl6StdXrtStringRemoveParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"string", "part"},
    {"c_size", "partSize"},
    {"c_bool", "ignoreCase"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringInsertParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"int", "start"},
    {"string", "part"},
    {"c_size", "partSize"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringPadParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"int", "width"},
    {"string", "pad"},
    {"c_size", "padSize"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringReverseParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtTimeToStrParams[] = {
    {"int", "value"},
    {"c_int", "format"}
};

static const XL6_StdlibParam xl6StdXrtTimeFormatParams[] = {
    {"int", "value"},
    {"string", "pattern"}
};

static const XL6_StdlibParam xl6StdXrtTimeParseParams[] = {
    {"string", "text"},
    {"string", "pattern"}
};

static const XL6_StdlibParam xl6StdXrtTryStrToTimeParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"point", "outValue"}
};

static const XL6_StdlibParam xl6StdXrtTryTimeParseParams[] = {
    {"string", "text"},
    {"string", "pattern"},
    {"point", "outValue"}
};

static const XL6_StdlibParam xl6StdXrtYearParams[] = {
    {"c_int", "year"}
};

static const XL6_StdlibParam xl6StdXrtYearMonthParams[] = {
    {"c_int", "year"},
    {"c_int", "month"}
};

static const XL6_StdlibParam xl6StdTimeFromTimeParams[] = {
    {"c_int", "hour"},
    {"c_int", "minute"},
    {"c_int", "second"}
};

static const XL6_StdlibParam xl6StdTimeFromDateParams[] = {
    {"int", "year"},
    {"c_int", "month"},
    {"c_int", "day"}
};

static const XL6_StdlibParam xl6StdTimeFromDateTimeParams[] = {
    {"int", "year"},
    {"c_int", "month"},
    {"c_int", "day"},
    {"c_int", "hour"},
    {"c_int", "minute"},
    {"c_int", "second"}
};

static const XL6_StdlibParam xl6StdTimeUnaryParams[] = {
    {"time", "value"}
};

static const XL6_StdlibParam xl6StdTimeBinaryParams[] = {
    {"time", "left"},
    {"time", "right"}
};

static const XL6_StdlibParam xl6StdTimeInRangeParams[] = {
    {"time", "value"},
    {"time", "start"},
    {"time", "endValue"}
};

static const XL6_StdlibParam xl6StdXrtDateAddParams[] = {
    {"c_int", "interval"},
    {"int", "amount"},
    {"int", "value"}
};

static const XL6_StdlibParam xl6StdXrtDateDiffParams[] = {
    {"c_int", "interval"},
    {"int", "left"},
    {"int", "right"}
};

static const XL6_StdlibParam xl6StdXrtStrToTimeParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtIntUnaryParams[] = {
    {"int", "value"}
};

static const XL6_StdlibParam xl6StdXrtIntBinaryParams[] = {
    {"int", "left"},
    {"int", "right"}
};

static const XL6_StdlibParam xl6StdXrtIntInRangeParams[] = {
    {"int", "value"},
    {"int", "start"},
    {"int", "endValue"}
};

static const XL6_StdlibParam xl6StdXrtTimeOverlapParams[] = {
    {"int", "start1"},
    {"int", "end1"},
    {"int", "start2"},
    {"int", "end2"}
};

static const XL6_StdlibParam xl6StdTimeSleepParams[] = {
    {"int", "ms"}
};

static const XL6_StdlibParam xl6StdXrtSleepParams[] = {
    {"c_uint", "ms"}
};

static const XL6_StdlibParam xl6StdXrtWeekBoundaryParams[] = {
    {"int", "value"},
    {"c_int", "startDay"}
};

static const XL6_StdlibParam xl6StdXrtRandRangeParams[] = {
    {"c_int", "min"},
    {"c_int", "max"}
};

static const XL6_StdlibParam xl6StdXrtRandSeedParams[] = {
    {"uint64", "seed"},
    {"uint64", "seq"}
};

static const XL6_StdlibParam xl6StdXrtRandStrParams[] = {
    {"string", "chars"},
    {"c_size", "size"},
    {"c_size", "len"}
};

static const XL6_StdlibParam xl6StdPathUnaryParams[] = {
    {"string", "path"}
};

static const XL6_StdlibParam xl6StdXrtPathIsAbsParams[] = {
    {"string", "path"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtPathRandomParams[] = {
    {"string", "head"},
    {"c_size", "headSize"},
    {"string", "foot"},
    {"c_size", "footSize"},
    {"c_size", "len"}
};

static const XL6_StdlibParam xl6StdPathRelParams[] = {
    {"string", "base"},
    {"string", "target"}
};

static const XL6_StdlibParam xl6StdPathWithExtParams[] = {
    {"string", "path"},
    {"string", "ext"}
};

static const XL6_StdlibParam xl6StdPathWithNameParams[] = {
    {"string", "path"},
    {"string", "nameExt"}
};

static const XL6_StdlibParam xl6StdFileWriteTextParams[] = {
    {"string", "path"},
    {"string", "text"},
    {"int", "charset"}
};

static const XL6_StdlibParam xl6StdFileWriteTextUtf8Params[] = {
    {"string", "path"},
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdFileSetSizeParams[] = {
    {"string", "path"},
    {"int", "size"}
};

static const XL6_StdlibParam xl6StdXrtFileSetSizeParams[] = {
    {"string", "path"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdFileSetAttrParams[] = {
    {"string", "path"},
    {"int", "attr"}
};

static const XL6_StdlibParam xl6StdXrtFileSetAttrParams[] = {
    {"string", "path"},
    {"c_int", "attr"}
};

static const XL6_StdlibParam xl6StdXrtFileWriteTextParams[] = {
    {"string", "path"},
    {"string", "text"},
    {"c_size", "size"},
    {"c_int", "charset"}
};

static const XL6_StdlibParam xl6StdFileReadTextParams[] = {
    {"string", "path"},
    {"int", "charset"}
};

static const XL6_StdlibParam xl6StdXrtFileReadTextParams[] = {
    {"string", "path"},
    {"c_int", "charset"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdFileCopyMoveParams[] = {
    {"string", "source"},
    {"string", "target"},
    {"bool", "overwrite"}
};

static const XL6_StdlibParam xl6StdXrtFileCopyMoveParams[] = {
    {"string", "source"},
    {"string", "target"},
    {"c_bool", "overwrite"}
};

static const XL6_StdlibParam xl6StdFileReplaceParams[] = {
    {"string", "source"},
    {"string", "target"}
};

static const XL6_StdlibParam xl6StdTcpConnectParams[] = {
    {"string", "host"},
    {"uint16", "port"},
    {"int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdTcpListenParams[] = {
    {"string", "host"},
    {"uint16", "port"},
    {"c_uint", "backlog"}
};

static const XL6_StdlibParam xl6StdTcpAcceptParams[] = {
    {"point", "server"},
    {"int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdNetHandleParams[] = {
    {"point", "handle"}
};

static const XL6_StdlibParam xl6StdUdpBindParams[] = {
    {"string", "host"},
    {"uint16", "port"}
};

static const XL6_StdlibParam xl6StdUdpSendParams[] = {
    {"point", "socket"},
    {"string", "host"},
    {"uint16", "port"},
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdUdpRecvParams[] = {
    {"point", "socket"},
    {"int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdXrtJsonParseParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtXsonValidParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_uint", "flags"}
};

static const XL6_StdlibParam xl6StdXrtJsonStringifyParams[] = {
    {"var", "value"},
    {"c_int", "format"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtXsonStringifyParams[] = {
    {"var", "value"},
    {"c_int", "format"},
    {"c_uint", "flags"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtJsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"c_int", "format"}
};

static const XL6_StdlibParam xl6StdXrtXsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"c_int", "format"},
    {"c_uint", "flags"}
};

static const XL6_StdlibParam xl6StdJsonStringifyParams[] = {
    {"var", "value"},
    {"bool", "pretty"}
};

static const XL6_StdlibParam xl6StdJsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"bool", "pretty"}
};

static const XL6_StdlibParam xl6StdXrtCodecTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtCodecBytesParams[] = {
    {"point", "data"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtCodecBase64Params[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"point", "table"}
};

static const XL6_StdlibParam xl6StdXrtCodecBase64BytesParams[] = {
    {"point", "data"},
    {"c_size", "size"},
    {"point", "table"}
};

static const XL6_StdlibParam xl6StdXrtCodecPercentParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_bool", "spaceAsPlus"},
    {"point", "outSize"}
};

static const XL6_StdlibParam xl6StdXrtCodecFormUrlParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"point", "outSize"}
};

static const XL6_StdlibParam xl6StdXrtCodecDecodeBufferParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtCodecBase64DecodeBufferParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"point", "table"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingTextParams[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingTextDetectParams[] = {
    {"string", "text"},
    {"bool", "withBom"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingBytesParams[] = {
    {"point", "data"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingBytesDetectParams[] = {
    {"point", "data"},
    {"c_size", "size"},
    {"bool", "withBom"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingToBytesParams[] = {
    {"string", "text"},
    {"int", "toCharset"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingFromBytesParams[] = {
    {"point", "data"},
    {"c_size", "size"},
    {"int", "fromCharset"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingConvertBytesParams[] = {
    {"point", "data"},
    {"c_size", "size"},
    {"int", "fromCharset"},
    {"int", "toCharset"}
};

static const XL6_StdlibParam xl6StdRuntimeEncodingCharsetParams[] = {
    {"int", "charset"}
};

static const XL6_StdlibParam xl6StdXrtHashParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtHash32SeedParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_uint", "seed"}
};

static const XL6_StdlibParam xl6StdXrtHash64SeedParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_ulonglong", "seed"}
};

static const char xl6StdMathRoundBody[] =
    "func __xl_std_math_round(float x) as float\n"
    "    if x >= 0.0 then\n"
    "        return __xl_std_math_floor(x + 0.5)\n"
    "    else\n"
    "        return __xl_std_math_ceil(x - 0.5)\n"
    "    end\n"
    "end\n";

static const char xl6StdMathSignBody[] =
    "func __xl_std_math_sign(float x) as int\n"
    "    return int(__xl_std_xrt_math_sign(x))\n"
    "end\n";

static const char xl6StdStringCompareBody[] =
    "func __xl_std_string_compare(string left, string right, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_str_comp(left, right, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringLenBody[] =
    "func __xl_std_string_len(string text) as int\n"
    "    return int(__xl_std_xrt_str_char_len(text, 0))\n"
    "end\n";

static const char xl6StdStringByteLenBody[] =
    "func __xl_std_string_byte_len(string text) as int\n"
    "    return int(__xl_std_xrt_str_byte_len(text, 0))\n"
    "end\n";

static const char xl6StdStringEqualsBody[] =
    "func __xl_std_string_equals(string left, string right, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_str_comp(left, right, 0, ignoreCase) == 0\n"
    "end\n";

static const char xl6StdStringContainsBody[] =
    "func __xl_std_string_contains(string text, string part, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_in_str(text, 0, part, 0, ignoreCase) != 0\n"
    "end\n";

static const char xl6StdStringIndexOfBody[] =
    "func __xl_std_string_index_of(string text, string part, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_str_index_of(text, 0, part, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringLastIndexOfBody[] =
    "func __xl_std_string_last_index_of(string text, string part, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_str_last_index_of(text, 0, part, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringCountBody[] =
    "func __xl_std_string_count(string text, string part, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_str_count(text, 0, part, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringStartsWithBody[] =
    "func __xl_std_string_starts_with(string text, string prefix, bool ignoreCase = false) as bool\n"
    "    return __xl_std_runtime_string_starts_with(text, __xl_std_crt_strlen(text), prefix, __xl_std_crt_strlen(prefix), ignoreCase)\n"
    "end\n";

static const char xl6StdStringEndsWithBody[] =
    "func __xl_std_string_ends_with(string text, string suffix, bool ignoreCase = false) as bool\n"
    "    return __xl_std_runtime_string_ends_with(text, __xl_std_crt_strlen(text), suffix, __xl_std_crt_strlen(suffix), ignoreCase)\n"
    "end\n";

static const char xl6StdStringHasAnyBody[] =
    "func __xl_std_string_has_any(string text, string chars) as bool\n"
    "    return __xl_std_xrt_check_str(text, 0, chars, 0) != null\n"
    "end\n";

static const char xl6StdStringLikeBody[] =
    "func __xl_std_string_like(string text, string pattern, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_str_like(text, 0, pattern, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringApproxBody[] =
    "func __xl_std_string_approx(string left, string right) as bool\n"
    "    return __xl_std_xrt_str_approx(left, 0, right, 0)\n"
    "end\n";

static const char xl6StdStringSimBody[] =
    "func __xl_std_string_sim(string left, string right) as float\n"
    "    return float(__xl_std_xrt_str_sim(left, 0, right, 0))\n"
    "end\n";

static const char xl6StdStringIsUtf8Body[] =
    "func __xl_std_string_is_utf8(string text) as bool\n"
    "    return __xl_std_xrt_is_utf8(text, 0)\n"
    "end\n";

static const char xl6StdStringFormatIntBody[] =
    "func __xl_std_string_format_int(int value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_int_format(value, format))\n"
    "end\n";

static const char xl6StdStringFormatFloatBody[] =
    "func __xl_std_string_format_float(float value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_num_format(value, format))\n"
    "end\n";

static const char xl6StdStringLowerBody[] =
    "func __xl_std_string_lower(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_lcase(text, 0, false))\n"
    "end\n";

static const char xl6StdStringUpperBody[] =
    "func __xl_std_string_upper(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_ucase(text, 0, false))\n"
    "end\n";

static const char xl6StdStringTrimBody[] =
    "func __xl_std_string_trim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_trim(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringLTrimBody[] =
    "func __xl_std_string_ltrim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_ltrim(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringRTrimBody[] =
    "func __xl_std_string_rtrim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_rtrim(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringFilterBody[] =
    "func __xl_std_string_filter(string text, string chars) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_filter(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringReplaceBody[] =
    "func __xl_std_string_replace(string text, string oldText, string newText) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_replace(text, 0, oldText, 0, newText, 0, null))\n"
    "end\n";

static const char xl6StdStringSplitBody[] =
    "func __xl_std_string_split(string text, string sep) as array\n"
    "    return array(__xl_std_runtime_string_split(text, sep))\n"
    "end\n";

static const char xl6StdStringSliceBody[] =
    "func __xl_std_string_slice(string text, int start, int count = -1) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_slice(text, 0, start, count, null))\n"
    "end\n";

static const char xl6StdStringLeftBody[] =
    "func __xl_std_string_left(string text, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_left(text, 0, count, null))\n"
    "end\n";

static const char xl6StdStringRightBody[] =
    "func __xl_std_string_right(string text, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_right(text, 0, count, null))\n"
    "end\n";

static const char xl6StdStringMidBody[] =
    "func __xl_std_string_mid(string text, int start, int count = -1) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_mid(text, 0, start, count, null))\n"
    "end\n";

static const char xl6StdStringCharAtBody[] =
    "func __xl_std_string_char_at(string text, int index) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_char_at(text, 0, index, null))\n"
    "end\n";

static const char xl6StdStringByteAtBody[] =
    "func __xl_std_string_byte_at(string text, int index) as int\n"
    "    return int(__xl_std_xrt_str_byte_at(text, 0, index))\n"
    "end\n";

static const char xl6StdStringJoinBody[] =
    "func __xl_std_string_join(array values, string sep = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_string_join(values, sep))\n"
    "end\n";

static const char xl6StdStringRepeatBody[] =
    "func __xl_std_string_repeat(string text, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_repeat(text, 0, count, null))\n"
    "end\n";

static const char xl6StdStringIsEmptyBody[] =
    "func __xl_std_string_is_empty(string text) as bool\n"
    "    return __xl_std_xrt_str_is_empty(text, 0)\n"
    "end\n";

static const char xl6StdStringIsBlankBody[] =
    "func __xl_std_string_is_blank(string text) as bool\n"
    "    return __xl_std_xrt_str_is_blank(text, 0)\n"
    "end\n";

static const char xl6StdStringRemoveBody[] =
    "func __xl_std_string_remove(string text, string part, bool ignoreCase = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_remove(text, 0, part, 0, ignoreCase, null))\n"
    "end\n";

static const char xl6StdStringRemoveAtBody[] =
    "func __xl_std_string_remove_at(string text, int start, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_remove_at(text, 0, start, count, null))\n"
    "end\n";

static const char xl6StdStringInsertBody[] =
    "func __xl_std_string_insert(string text, int start, string part) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_insert(text, 0, start, part, 0, null))\n"
    "end\n";

static const char xl6StdStringPadLeftBody[] =
    "func __xl_std_string_pad_left(string text, int width, string pad = \" \") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_pad_left(text, 0, width, pad, 0, null))\n"
    "end\n";

static const char xl6StdStringPadRightBody[] =
    "func __xl_std_string_pad_right(string text, int width, string pad = \" \") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_pad_right(text, 0, width, pad, 0, null))\n"
    "end\n";

static const char xl6StdStringPadCenterBody[] =
    "func __xl_std_string_pad_center(string text, int width, string pad = \" \") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_pad_center(text, 0, width, pad, 0, null))\n"
    "end\n";

static const char xl6StdStringReverseBody[] =
    "func __xl_std_string_reverse(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_str_reverse(text, 0, null))\n"
    "end\n";

static const char xl6StdStringSplitLinesBody[] =
    "func __xl_std_string_split_lines(string text) as array\n"
    "    return array(__xl_std_runtime_string_split_lines(text))\n"
    "end\n";

static const char xl6StdStringCharsBody[] =
    "func __xl_std_string_chars(string text) as array\n"
    "    return array(__xl_std_runtime_string_chars(text))\n"
    "end\n";

static const char xl6StdStringBytesBody[] =
    "func __xl_std_string_bytes(string text) as array\n"
    "    return array(__xl_std_runtime_string_bytes(text))\n"
    "end\n";

static const char xl6StdRegexCompileBody[] =
    "func __xl_std_regex_compile(string pattern, int flags = 0) as Regex\n"
    "    return Regex(pattern, flags)\n"
    "end\n";

static const char xl6StdRegexIsMatchBody[] =
    "func __xl_std_regex_is_match(string text, string pattern, int flags = 0) as bool\n"
    "    return __xl_std_runtime_regex_is_match(text, __xl_std_crt_strlen(text), pattern, c_int(flags))\n"
    "end\n";

static const char xl6StdRegexFullMatchBody[] =
    "func __xl_std_regex_full_match(string text, string pattern, int flags = 0) as bool\n"
    "    return __xl_std_runtime_regex_full_match(text, __xl_std_crt_strlen(text), pattern, c_int(flags))\n"
    "end\n";

static const char xl6StdRegexFindBody[] =
    "func __xl_std_regex_find(string text, string pattern, int flags = 0) as int\n"
    "    return int(__xl_std_runtime_regex_find(text, __xl_std_crt_strlen(text), pattern, c_int(flags)))\n"
    "end\n";

static const char xl6StdRegexMatchBody[] =
    "func __xl_std_regex_match(string text, string pattern, int flags = 0, int start = 0) as RegexMatch\n"
    "    if start < 0 then\n"
    "        start = 0\n"
    "    end\n"
    "    return RegexMatch(__xl_std_runtime_regex_match(text, __xl_std_crt_strlen(text), pattern, c_int(flags), c_size(start)))\n"
    "end\n";

static const char xl6StdRegexSetBody[] =
    "func __xl_std_regex_set(array patterns, int flags = 0) as RegexSet\n"
    "    return RegexSet(__xl_std_runtime_regex_set_create(patterns, c_int(flags)))\n"
    "end\n";

static const char xl6StdProcessExecBody[] =
    "func __xl_std_process_exec(string command, int timeoutMs = 0) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_process_exec(command, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdProcessSuccessBody[] =
    "func __xl_std_process_success(string command, int timeoutMs = 0) as bool\n"
    "    return __xl_std_runtime_process_success(command, c_int(timeoutMs))\n"
    "end\n";

static const char xl6StdProcessExitCodeBody[] =
    "func __xl_std_process_exit_code(string command, int timeoutMs = 0) as int\n"
    "    return int(__xl_std_runtime_process_exit_code(command, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdProcessRunBody[] =
    "func __xl_std_process_run(string command, int timeoutMs = 0) as ProcessResult\n"
    "    return ProcessResult(__xl_std_runtime_process_run(command, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdProcessRunFileBody[] =
    "func __xl_std_process_run_file(string program, array args = [], int timeoutMs = 0) as ProcessResult\n"
    "    return ProcessResult(__xl_std_runtime_process_run_file(program, args, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdProcessCmdBody[] =
    "func __xl_std_process_cmd(string program, array args = []) as ProcessCommand\n"
    "    return ProcessCommand(program, args)\n"
    "end\n";

static const char xl6StdProcessShellBody[] =
    "func __xl_std_process_shell(string command) as ProcessCommand\n"
    "    return ProcessCommand(command)\n"
    "end\n";

static const char xl6StdProcessPipelineBody[] =
    "func __xl_std_process_pipeline(array commands, int timeoutMs = 0) as PipelineResult\n"
    "    return PipelineResult(__xl_std_runtime_process_pipeline(commands, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdProcessStartBody[] =
    "func __xl_std_process_start(string command) as Process\n"
    "    return Process(command)\n"
    "end\n";

static const char xl6StdProcessStartFileBody[] =
    "func __xl_std_process_start_file(string program, array args = []) as Process\n"
    "    return Process(program, args)\n"
    "end\n";

static const char xl6StdLogCreateBody[] =
    "func __xl_std_log_create(string name = \"xlang\") as Logger\n"
    "    return Logger(name)\n"
    "end\n";

static const char xl6StdLogDefaultBody[] =
    "func __xl_std_log_default() as Logger\n"
    "    return Logger(__xl_std_runtime_log_default())\n"
    "end\n";

static const char xl6StdLogSetLevelBody[] =
    "func __xl_std_log_set_level(int level)\n"
    "    __xl_std_runtime_log_default_set_level(level)\n"
    "end\n";

static const char xl6StdLogGetLevelBody[] =
    "func __xl_std_log_get_level() as int\n"
    "    return __xl_std_runtime_log_default_get_level()\n"
    "end\n";

static const char xl6StdLogLevelNameBody[] =
    "func __xl_std_log_level_name(int level) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_log_level_name(level))\n"
    "end\n";

static const char xl6StdLogAddConsoleBody[] =
    "func __xl_std_log_add_console(int level = 0, bool color = true, int format = 0) as bool\n"
    "    return __xl_std_runtime_log_default_add_console(level, color, format)\n"
    "end\n";

static const char xl6StdLogAddFileBody[] =
    "func __xl_std_log_add_file(string path, int level = 2, int format = 0) as bool\n"
    "    return __xl_std_runtime_log_default_add_file(path, level, format)\n"
    "end\n";

static const char xl6StdLogAddRollingFileBody[] =
    "func __xl_std_log_add_rolling_file(string path, int maxSize = 10485760, int maxBackup = 5, int level = 2, int format = 0) as bool\n"
    "    return __xl_std_runtime_log_default_add_rolling_file(path, maxSize, maxBackup, level, format)\n"
    "end\n";

static const char xl6StdLogWriteBody[] =
    "func __xl_std_log_write(int level, string message)\n"
    "    __xl_std_runtime_log_default_write(level, message)\n"
    "end\n";

static const char xl6StdLogTraceBody[] =
    "func __xl_std_log_trace(string message)\n"
    "    __xl_std_runtime_log_default_write(0, message)\n"
    "end\n";

static const char xl6StdLogDebugBody[] =
    "func __xl_std_log_debug(string message)\n"
    "    __xl_std_runtime_log_default_write(1, message)\n"
    "end\n";

static const char xl6StdLogInfoBody[] =
    "func __xl_std_log_info(string message)\n"
    "    __xl_std_runtime_log_default_write(2, message)\n"
    "end\n";

static const char xl6StdLogWarnBody[] =
    "func __xl_std_log_warn(string message)\n"
    "    __xl_std_runtime_log_default_write(3, message)\n"
    "end\n";

static const char xl6StdLogErrorBody[] =
    "func __xl_std_log_error(string message)\n"
    "    __xl_std_runtime_log_default_write(4, message)\n"
    "end\n";

static const char xl6StdLogFatalBody[] =
    "func __xl_std_log_fatal(string message)\n"
    "    __xl_std_runtime_log_default_write(5, message)\n"
    "end\n";

static const char xl6StdLogFlushBody[] =
    "func __xl_std_log_flush()\n"
    "    __xl_std_runtime_log_default_flush()\n"
    "end\n";

static const char xl6StdTemplateCompileBody[] =
    "func __xl_std_template_compile(string text, string bracket = \"{}\") as Template\n"
    "    return Template(text, bracket)\n"
    "end\n";

static const char xl6StdTemplateRenderBody[] =
    "func __xl_std_template_render(string text, var data = null, string bracket = \"{}\") as string\n"
    "    Template tpl = Template(text, bracket)\n"
    "    return tpl.render(data)\n"
    "end\n";

static const char xl6StdTemplateRenderFileBody[] =
    "func __xl_std_template_render_file(string path, var data = null, string bracket = \"{}\") as string\n"
    "    string text = __xl_std_file_read_text(path, 65001)\n"
    "    return __xl_std_template_render(text, data, bracket)\n"
    "end\n";

static const char xl6StdTimeFromDateTimeBody[] =
    "func __xl_std_time_from_datetime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) as time\n"
    "    return time(__xl_std_xrt_datetime_serial(year, c_int(month), c_int(day), c_int(hour), c_int(minute), c_int(second)))\n"
    "end\n";

static const char xl6StdTimeNowBody[] =
    "func __xl_std_time_now() as time\n"
    "    return time(__xl_std_xrt_now())\n"
    "end\n";

static const char xl6StdTimeTodayBody[] =
    "func __xl_std_time_today() as time\n"
    "    return time(__xl_std_xrt_date())\n"
    "end\n";

static const char xl6StdTimeClockBody[] =
    "func __xl_std_time_clock() as time\n"
    "    return time(__xl_std_xrt_time())\n"
    "end\n";

static const char xl6StdTimeFromTimeBody[] =
    "func __xl_std_time_from_time(int hour, int minute, int second) as time\n"
    "    return time(__xl_std_xrt_time_serial(c_int(hour), c_int(minute), c_int(second)))\n"
    "end\n";

static const char xl6StdTimeFromDateBody[] =
    "func __xl_std_time_from_date(int year, int month, int day) as time\n"
    "    return time(__xl_std_xrt_date_serial(year, c_int(month), c_int(day)))\n"
    "end\n";

static const char xl6StdTimeParseBody[] =
    "func __xl_std_time_parse(string text) as time\n"
    "    return time(__xl_std_xrt_str_to_time(text, 0))\n"
    "end\n";

static const char xl6StdTimeParsePatternBody[] =
    "func __xl_std_time_parse_pattern(string text, string pattern) as time\n"
    "    return time(__xl_std_xrt_time_parse(text, pattern))\n"
    "end\n";

static const char xl6StdTimeTryParseBody[] =
    "func __xl_std_time_try_parse(string text) as (bool, time)\n"
    "    if __xl_std_xrt_try_str_to_time(text, 0, null) then\n"
    "        return true, time(__xl_std_xrt_str_to_time(text, 0))\n"
    "    end\n"
    "    return false, time(0)\n"
    "end\n";

static const char xl6StdTimeTryParsePatternBody[] =
    "func __xl_std_time_try_parse_pattern(string text, string pattern) as (bool, time)\n"
    "    if __xl_std_xrt_try_time_parse(text, pattern, null) then\n"
    "        return true, time(__xl_std_xrt_time_parse(text, pattern))\n"
    "    end\n"
    "    return false, time(0)\n"
    "end\n";

static const char xl6StdTimeYearBody[] =
    "func __xl_std_time_year(time value) as int\n"
    "    return __xl_std_xrt_year(int(value))\n"
    "end\n";

static const char xl6StdTimeMonthBody[] =
    "func __xl_std_time_month(time value) as int\n"
    "    return __xl_std_xrt_month(int(value))\n"
    "end\n";

static const char xl6StdTimeDayBody[] =
    "func __xl_std_time_day(time value) as int\n"
    "    return __xl_std_xrt_day(int(value))\n"
    "end\n";

static const char xl6StdTimeHourBody[] =
    "func __xl_std_time_hour(time value) as int\n"
    "    return __xl_std_xrt_hour(int(value))\n"
    "end\n";

static const char xl6StdTimeMinuteBody[] =
    "func __xl_std_time_minute(time value) as int\n"
    "    return __xl_std_xrt_minute(int(value))\n"
    "end\n";

static const char xl6StdTimeSecondBody[] =
    "func __xl_std_time_second(time value) as int\n"
    "    return __xl_std_xrt_second(int(value))\n"
    "end\n";

static const char xl6StdTimeWeekdayBody[] =
    "func __xl_std_time_weekday(time value) as int\n"
    "    return __xl_std_xrt_weekday(int(value))\n"
    "end\n";

static const char xl6StdTimeDayOfYearBody[] =
    "func __xl_std_time_day_of_year(time value) as int\n"
    "    return __xl_std_xrt_day_of_year(int(value))\n"
    "end\n";

static const char xl6StdTimeQuarterBody[] =
    "func __xl_std_time_quarter(time value) as int\n"
    "    return __xl_std_xrt_quarter(int(value))\n"
    "end\n";

static const char xl6StdTimeIsLeapYearBody[] =
    "func __xl_std_time_is_leap_year(int year) as bool\n"
    "    return __xl_std_xrt_is_leap_year(c_int(year))\n"
    "end\n";

static const char xl6StdTimeDaysInMonthBody[] =
    "func __xl_std_time_days_in_month(int year, int month) as int\n"
    "    return int(__xl_std_xrt_days_in_month(c_int(year), c_int(month)))\n"
    "end\n";

static const char xl6StdTimeDaysInYearBody[] =
    "func __xl_std_time_days_in_year(int year) as int\n"
    "    return int(__xl_std_xrt_days_in_year(c_int(year)))\n"
    "end\n";

static const char xl6StdTimeDatePartBody[] =
    "func __xl_std_time_date_part(time value) as time\n"
    "    return time(__xl_std_xrt_date_part(int(value)))\n"
    "end\n";

static const char xl6StdTimeTimePartBody[] =
    "func __xl_std_time_time_part(time value) as time\n"
    "    return time(__xl_std_xrt_time_part(int(value)))\n"
    "end\n";

static const char xl6StdTimeIsSameDayBody[] =
    "func __xl_std_time_is_same_day(time left, time right) as bool\n"
    "    return __xl_std_xrt_is_same_day(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeIsSameMonthBody[] =
    "func __xl_std_time_is_same_month(time left, time right) as bool\n"
    "    return __xl_std_xrt_is_same_month(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeIsSameYearBody[] =
    "func __xl_std_time_is_same_year(time left, time right) as bool\n"
    "    return __xl_std_xrt_is_same_year(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeInRangeBody[] =
    "func __xl_std_time_in_range(time value, time start, time endValue) as bool\n"
    "    return __xl_std_xrt_time_in_range(int(value), int(start), int(endValue))\n"
    "end\n";

static const char xl6StdTimeAddYearsBody[] =
    "func __xl_std_time_add_years(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(1, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddMonthsBody[] =
    "func __xl_std_time_add_months(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(2, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddDaysBody[] =
    "func __xl_std_time_add_days(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(3, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddHoursBody[] =
    "func __xl_std_time_add_hours(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(4, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddMinutesBody[] =
    "func __xl_std_time_add_minutes(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(5, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddSecondsBody[] =
    "func __xl_std_time_add_seconds(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(6, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddWeeksBody[] =
    "func __xl_std_time_add_weeks(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(7, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddQuartersBody[] =
    "func __xl_std_time_add_quarters(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(8, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeDiffYearsBody[] =
    "func __xl_std_time_diff_years(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(1, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffMonthsBody[] =
    "func __xl_std_time_diff_months(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(2, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffDaysBody[] =
    "func __xl_std_time_diff_days(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(3, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffHoursBody[] =
    "func __xl_std_time_diff_hours(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(4, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffMinutesBody[] =
    "func __xl_std_time_diff_minutes(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(5, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffSecondsBody[] =
    "func __xl_std_time_diff_seconds(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(6, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffWeeksBody[] =
    "func __xl_std_time_diff_weeks(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(7, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeDiffQuartersBody[] =
    "func __xl_std_time_diff_quarters(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(8, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeTimerBody[] =
    "func __xl_std_time_timer() as float\n"
    "    return __xl_std_xrt_timer()\n"
    "end\n";

static const char xl6StdTimeSleepBody[] =
    "func __xl_std_time_sleep(int ms)\n"
    "    __xl_std_xrt_sleep(c_uint(ms))\n"
    "end\n";

static const char xl6StdTimeNowUtcBody[] =
    "func __xl_std_time_now_utc() as time\n"
    "    return time(__xl_std_xrt_now_utc())\n"
    "end\n";

static const char xl6StdTimeTimezoneOffsetBody[] =
    "func __xl_std_time_timezone_offset() as int\n"
    "    return __xl_std_xrt_timezone_offset()\n"
    "end\n";

static const char xl6StdTimeRangeOverlapBody[] =
    "func __xl_std_time_range_overlap(time start1, time end1, time start2, time end2) as bool\n"
    "    return __xl_std_xrt_time_range_overlap(int(start1), int(end1), int(start2), int(end2))\n"
    "end\n";

static const char xl6StdTimeApproxBody[] =
    "func __xl_std_time_approx(time left, time right) as bool\n"
    "    return __xl_std_xrt_time_approx(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeToUnixBody[] =
    "func __xl_std_time_to_unix(time value) as int\n"
    "    return __xl_std_xrt_time_to_unix(int(value))\n"
    "end\n";

static const char xl6StdTimeFromUnixBody[] =
    "func __xl_std_time_from_unix(int seconds) as time\n"
    "    return time(__xl_std_xrt_time_from_unix(seconds))\n"
    "end\n";

static const char xl6StdTimeUtcToLocalBody[] =
    "func __xl_std_time_utc_to_local(time value) as time\n"
    "    return time(__xl_std_xrt_utc_to_local(int(value)))\n"
    "end\n";

static const char xl6StdTimeLocalToUtcBody[] =
    "func __xl_std_time_local_to_utc(time value) as time\n"
    "    return time(__xl_std_xrt_local_to_utc(int(value)))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfMonthBody[] =
    "func __xl_std_time_first_day_of_month(time value) as time\n"
    "    return time(__xl_std_xrt_first_day_of_month(int(value)))\n"
    "end\n";

static const char xl6StdTimeLastDayOfMonthBody[] =
    "func __xl_std_time_last_day_of_month(time value) as time\n"
    "    return time(__xl_std_xrt_last_day_of_month(int(value)))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfYearBody[] =
    "func __xl_std_time_first_day_of_year(time value) as time\n"
    "    return time(__xl_std_xrt_first_day_of_year(int(value)))\n"
    "end\n";

static const char xl6StdTimeLastDayOfYearBody[] =
    "func __xl_std_time_last_day_of_year(time value) as time\n"
    "    return time(__xl_std_xrt_last_day_of_year(int(value)))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfWeekBody[] =
    "func __xl_std_time_first_day_of_week(time value, int startDay = 1) as time\n"
    "    return time(__xl_std_xrt_first_day_of_week(int(value), c_int(startDay)))\n"
    "end\n";

static const char xl6StdTimeLastDayOfWeekBody[] =
    "func __xl_std_time_last_day_of_week(time value, int startDay = 1) as time\n"
    "    return time(__xl_std_xrt_last_day_of_week(int(value), c_int(startDay)))\n"
    "end\n";

static const char xl6StdTimeWeekOfYearBody[] =
    "func __xl_std_time_week_of_year(time value) as int\n"
    "    return __xl_std_xrt_week_of_year(int(value))\n"
    "end\n";

static const char xl6StdTimeWeekOfMonthBody[] =
    "func __xl_std_time_week_of_month(time value) as int\n"
    "    return __xl_std_xrt_week_of_month(int(value))\n"
    "end\n";

static const char xl6StdTimeFormatBody[] =
    "func __xl_std_time_format(time value, int format = 0) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_to_str(int(value), c_int(format)))\n"
    "end\n";

static const char xl6StdTimeFormatPatternBody[] =
    "func __xl_std_time_format_pattern(time value, string pattern) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_format(int(value), pattern))\n"
    "end\n";

static const char xl6StdTimeDateStringBody[] =
    "func __xl_std_time_date_string(time value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_to_str(int(value), 1))\n"
    "end\n";

static const char xl6StdTimeTimeStringBody[] =
    "func __xl_std_time_time_string(time value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_to_str(int(value), 2))\n"
    "end\n";

static const char xl6StdTimeRelativeBody[] =
    "func __xl_std_time_relative(time value, time base) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_relative_time(int(value), int(base)))\n"
    "end\n";

static const char xl6StdRandomU32Body[] =
    "func __xl_std_random_u32() as uint32\n"
    "    return uint32(__xl_std_xrt_rand32())\n"
    "end\n";

static const char xl6StdRandomU64Body[] =
    "func __xl_std_random_u64() as uint64\n"
    "    return uint64(__xl_std_xrt_rand64())\n"
    "end\n";

static const char xl6StdRandomRangeBody[] =
    "func __xl_std_random_range(int min, int max) as int\n"
    "    return __xl_std_xrt_rand_range(c_int(min), c_int(max))\n"
    "end\n";

static const char xl6StdRandomSeedBody[] =
    "func __xl_std_random_seed(uint64 seed, uint64 seq)\n"
    "    __xl_std_xrt_rand_seed_thread(seed, seq)\n"
    "end\n";

static const char xl6StdRandomStringBody[] =
    "func __xl_std_random_string(int len = 16, string chars = \"\") as string\n"
    "    if len < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_rand_str(chars, 0, c_size(len)))\n"
    "end\n";

static const char xl6StdPathExistsBody[] =
    "func __xl_std_path_exists(string path) as bool\n"
    "    return __xl_std_xrt_path_exists(path)\n"
    "end\n";

static const char xl6StdPathIsAbsBody[] =
    "func __xl_std_path_is_abs(string path) as bool\n"
    "    return __xl_std_xrt_path_is_abs(path, 0)\n"
    "end\n";

static const char xl6StdPathNameExtBody[] =
    "func __xl_std_path_name_ext(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_name_ext(path, 0))\n"
    "end\n";

static const char xl6StdPathNameBody[] =
    "func __xl_std_path_name(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_name(path, 0))\n"
    "end\n";

static const char xl6StdPathExtBody[] =
    "func __xl_std_path_ext(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_ext(path, 0))\n"
    "end\n";

static const char xl6StdPathDirBody[] =
    "func __xl_std_path_dir(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_dir(path, 0))\n"
    "end\n";

static const char xl6StdPathJoinBody[] =
    "func __xl_std_path_join(string first, string second, string third = \"\", string fourth = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_path_join(first, second, third, fourth))\n"
    "end\n";

static const char xl6StdPathRandomBody[] =
    "func __xl_std_path_random(string head = \"\", string foot = \"\", int len = 12) as string\n"
    "    if len < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_random(head, 0, foot, 0, c_size(len)))\n"
    "end\n";

static const char xl6StdPathSepBody[] =
    "func __xl_std_path_sep() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_sep())\n"
    "end\n";

static const char xl6StdPathNormalizeBody[] =
    "func __xl_std_path_normalize(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_normalize(path, 0))\n"
    "end\n";

static const char xl6StdPathAbsBody[] =
    "func __xl_std_path_abs(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_abs(path, 0))\n"
    "end\n";

static const char xl6StdPathRelBody[] =
    "func __xl_std_path_rel(string base, string target) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_rel(base, target))\n"
    "end\n";

static const char xl6StdPathCwdBody[] =
    "func __xl_std_path_cwd() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_cwd())\n"
    "end\n";

static const char xl6StdPathSetCwdBody[] =
    "func __xl_std_path_set_cwd(string path) as bool\n"
    "    return __xl_std_xrt_path_set_cwd(path)\n"
    "end\n";

static const char xl6StdPathHomeBody[] =
    "func __xl_std_path_home() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_home())\n"
    "end\n";

static const char xl6StdPathTempBody[] =
    "func __xl_std_path_temp() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_temp())\n"
    "end\n";

static const char xl6StdPathAppDirBody[] =
    "func __xl_std_path_app_dir() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_app_dir())\n"
    "end\n";

static const char xl6StdPathWithExtBody[] =
    "func __xl_std_path_with_ext(string path, string ext) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_with_ext(path, ext))\n"
    "end\n";

static const char xl6StdPathWithNameBody[] =
    "func __xl_std_path_with_name(string path, string nameExt) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_with_name(path, nameExt))\n"
    "end\n";

static const char xl6StdFileExistsBody[] =
    "func __xl_std_file_exists(string path) as bool\n"
    "    return __xl_std_xrt_file_exists(path)\n"
    "end\n";

static const char xl6StdFileSizeBody[] =
    "func __xl_std_file_size(string path) as int\n"
    "    return int(__xl_std_xrt_file_get_size(path))\n"
    "end\n";

static const char xl6StdFileSetSizeBody[] =
    "func __xl_std_file_set_size(string path, int size) as bool\n"
    "    return __xl_std_xrt_file_set_size(path, c_size(size))\n"
    "end\n";

static const char xl6StdFileAttrBody[] =
    "func __xl_std_file_attr(string path) as int\n"
    "    return __xl_std_xrt_file_get_attr(path)\n"
    "end\n";

static const char xl6StdFileSetAttrBody[] =
    "func __xl_std_file_set_attr(string path, int attr) as bool\n"
    "    return __xl_std_xrt_file_set_attr(path, c_int(attr))\n"
    "end\n";

static const char xl6StdFileAccessTimeBody[] =
    "func __xl_std_file_access_time(string path) as time\n"
    "    return time(__xl_std_xrt_file_get_access_time(path))\n"
    "end\n";

static const char xl6StdFileChangeTimeBody[] =
    "func __xl_std_file_change_time(string path) as time\n"
    "    return time(__xl_std_xrt_file_get_change_time(path))\n"
    "end\n";

static const char xl6StdFileWriteTextBody[] =
    "func __xl_std_file_write_text(string path, string text, int charset = 65001) as bool\n"
    "    return __xl_std_xrt_file_write_all(path, text, 0, c_int(charset)) != 0\n"
    "end\n";

static const char xl6StdFileAppendTextBody[] =
    "func __xl_std_file_append_text(string path, string text, int charset = 65001) as bool\n"
    "    return __xl_std_xrt_file_append(path, text, 0, c_int(charset)) != 0\n"
    "end\n";

static const char xl6StdFileReadTextBody[] =
    "func __xl_std_file_read_text(string path, int charset = 65001) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_file_read_all(path, c_int(charset), null))\n"
    "end\n";

static const char xl6StdFileCopyBody[] =
    "func __xl_std_file_copy(string source, string target, bool overwrite = true) as bool\n"
    "    return __xl_std_xrt_file_copy(source, target, overwrite)\n"
    "end\n";

static const char xl6StdFileMoveBody[] =
    "func __xl_std_file_move(string source, string target, bool overwrite = true) as bool\n"
    "    return __xl_std_xrt_file_move(source, target, overwrite)\n"
    "end\n";

static const char xl6StdFileDeleteBody[] =
    "func __xl_std_file_delete(string path) as bool\n"
    "    return __xl_std_xrt_file_delete(path)\n"
    "end\n";

static const char xl6StdFileTouchBody[] =
    "func __xl_std_file_touch(string path) as bool\n"
    "    return __xl_std_xrt_file_touch(path)\n"
    "end\n";

static const char xl6StdFileRenameBody[] =
    "func __xl_std_file_rename(string source, string target, bool overwrite = false) as bool\n"
    "    return __xl_std_xrt_file_rename(source, target, overwrite)\n"
    "end\n";

static const char xl6StdFileReplaceBody[] =
    "func __xl_std_file_replace(string source, string target) as bool\n"
    "    return __xl_std_xrt_file_replace(source, target)\n"
    "end\n";

static const char xl6StdFileWriteTextAtomicBody[] =
    "func __xl_std_file_write_text_atomic(string path, string text, int charset = 65001) as bool\n"
    "    return __xl_std_xrt_file_write_all_atomic(path, text, 0, c_int(charset))\n"
    "end\n";

static const char xl6StdFileInfoBody[] =
    "func __xl_std_file_info(string filePath) as FileInfo\n"
    "    FileInfo info\n"
    "    info.path = filePath\n"
    "    info.exists = __xl_std_xrt_path_exists(filePath)\n"
    "    info.isFile = __xl_std_xrt_file_exists(filePath)\n"
    "    info.isDir = __xl_std_xrt_dir_exists(filePath)\n"
    "    if info.isFile then\n"
    "        info.size = int(__xl_std_xrt_file_get_size(filePath))\n"
    "        info.attr = __xl_std_xrt_file_get_attr(filePath)\n"
    "        info.accessTime = time(__xl_std_xrt_file_get_access_time(filePath))\n"
    "        info.changeTime = time(__xl_std_xrt_file_get_change_time(filePath))\n"
    "    end\n"
    "    return info\n"
    "end\n";

static const char xl6StdFileOpenBody[] =
    "func __xl_std_file_open(string filePath, bool readOnly = false, int charset = 65001) as File\n"
    "    return File(filePath, readOnly, charset)\n"
    "end\n";

static const char xl6StdDirExistsBody[] =
    "func __xl_std_dir_exists(string path) as bool\n"
    "    return __xl_std_xrt_dir_exists(path)\n"
    "end\n";

static const char xl6StdDirCreateBody[] =
    "func __xl_std_dir_create(string path) as bool\n"
    "    return __xl_std_xrt_dir_create(path)\n"
    "end\n";

static const char xl6StdDirCreateAllBody[] =
    "func __xl_std_dir_create_all(string path) as bool\n"
    "    return __xl_std_xrt_dir_create_all(path)\n"
    "end\n";

static const char xl6StdDirCopyBody[] =
    "func __xl_std_dir_copy(string source, string target, bool overwrite = true) as bool\n"
    "    if not __xl_std_xrt_dir_exists(source) then\n"
    "        return false\n"
    "    end\n"
    "    __xl_std_xrt_dir_copy(source, target, overwrite)\n"
    "    return __xl_std_xrt_dir_exists(target)\n"
    "end\n";

static const char xl6StdDirMoveBody[] =
    "func __xl_std_dir_move(string source, string target, bool overwrite = true) as bool\n"
    "    if not __xl_std_xrt_dir_exists(source) then\n"
    "        return false\n"
    "    end\n"
    "    __xl_std_xrt_dir_move(source, target, overwrite)\n"
    "    return (not __xl_std_xrt_dir_exists(source)) and __xl_std_xrt_dir_exists(target)\n"
    "end\n";

static const char xl6StdDirDeleteBody[] =
    "func __xl_std_dir_delete(string path) as bool\n"
    "    if not __xl_std_xrt_dir_exists(path) then\n"
    "        return false\n"
    "    end\n"
    "    __xl_std_xrt_dir_delete(path)\n"
    "    return not __xl_std_xrt_dir_exists(path)\n"
    "end\n";

static const char xl6StdDirListBody[] =
    "func __xl_std_dir_list(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_list(path, recursive, 0))\n"
    "end\n";

static const char xl6StdDirFilesBody[] =
    "func __xl_std_dir_files(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_list(path, recursive, 1))\n"
    "end\n";

static const char xl6StdDirDirsBody[] =
    "func __xl_std_dir_dirs(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_list(path, recursive, 2))\n"
    "end\n";

static const char xl6StdDirEmptyBody[] =
    "func __xl_std_dir_empty(string path) as bool\n"
    "    return __xl_std_xrt_dir_is_empty(path)\n"
    "end\n";

static const char xl6StdDirCountBody[] =
    "func __xl_std_dir_count(string path, bool recursive = false) as int\n"
    "    return int(__xl_std_xrt_dir_count(path, recursive, 0))\n"
    "end\n";

static const char xl6StdDirSizeBody[] =
    "func __xl_std_dir_size(string path, bool recursive = true) as int\n"
    "    return int(__xl_std_xrt_dir_size(path, recursive))\n"
    "end\n";

static const char xl6StdDirCleanBody[] =
    "func __xl_std_dir_clean(string path) as bool\n"
    "    return __xl_std_xrt_dir_clean(path)\n"
    "end\n";

static const char xl6StdDirEnsureEmptyBody[] =
    "func __xl_std_dir_ensure_empty(string path) as bool\n"
    "    return __xl_std_xrt_dir_ensure_empty(path)\n"
    "end\n";

static const char xl6StdDirGlobBody[] =
    "func __xl_std_dir_glob(string path, string pattern = \"*\", bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_glob(path, pattern, recursive, 0))\n"
    "end\n";

static const char xl6StdDirEntriesBody[] =
    "func __xl_std_dir_entries(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_entries(path, recursive, false, \"\", 0))\n"
    "end\n";

static const char xl6StdDirMatchBody[] =
    "func __xl_std_dir_match(string path, string pattern = \"*\", bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_entries(path, recursive, false, pattern, 0))\n"
    "end\n";

static const char xl6StdDirWalkBody[] =
    "func __xl_std_dir_walk(string path, string pattern = \"*\") as array\n"
    "    return array(__xl_std_runtime_dir_entries(path, true, true, pattern, 0))\n"
    "end\n";

static const char xl6StdJsonParseBody[] =
    "func __xl_std_json_parse(string text) as var\n"
    "    return __xl_std_xrt_json_parse(text, 0)\n"
    "end\n";

static const char xl6StdJsonValidBody[] =
    "func __xl_std_json_valid(string text) as bool\n"
    "    return __xl_std_xrt_json_valid(text, 0)\n"
    "end\n";

static const char xl6StdJsonReadBody[] =
    "func __xl_std_json_read(string path) as var\n"
    "    return __xl_std_xrt_json_parse_file(path)\n"
    "end\n";

static const char xl6StdJsonStringifyBody[] =
    "func __xl_std_json_stringify(var value, bool pretty = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_json_stringify(value, c_int(pretty), null))\n"
    "end\n";

static const char xl6StdJsonWriteBody[] =
    "func __xl_std_json_write(string path, var value, bool pretty = false) as bool\n"
    "    return __xl_std_xrt_json_write_file(path, value, c_int(pretty)) != 0\n"
    "end\n";

static const char xl6StdXsonParseBody[] =
    "func __xl_std_xson_parse(string text) as var\n"
    "    return __xl_std_xrt_xson_parse(text, 0)\n"
    "end\n";

static const char xl6StdXsonValidBody[] =
    "func __xl_std_xson_valid(string text, bool ignoreUnsupported = false) as bool\n"
    "    if ignoreUnsupported then\n"
    "        return __xl_std_xrt_xson_valid(text, 0, c_uint(2))\n"
    "    end\n"
    "    return __xl_std_xrt_xson_valid(text, 0, c_uint(0))\n"
    "end\n";

static const char xl6StdXsonReadBody[] =
    "func __xl_std_xson_read(string path) as var\n"
    "    return __xl_std_xrt_xson_parse_file(path)\n"
    "end\n";

static const char xl6StdXsonStringifyBody[] =
    "func __xl_std_xson_stringify(var value, bool pretty = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_xson_stringify(value, c_int(pretty), 0, null))\n"
    "end\n";

static const char xl6StdXsonWriteBody[] =
    "func __xl_std_xson_write(string path, var value, bool pretty = false) as bool\n"
    "    return __xl_std_xrt_xson_write_file(path, value, c_int(pretty), 0) != 0\n"
    "end\n";

static const char xl6StdCodecHexEncodeBody[] =
    "func __xl_std_codec_hex_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_hex_encode(text, 0))\n"
    "end\n";

static const char xl6StdCodecHexDecodeBody[] =
    "func __xl_std_codec_hex_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_hex_decode(text, 0))\n"
    "end\n";

static const char xl6StdCodecBase64EncodeBody[] =
    "func __xl_std_codec_base64_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_base64_encode(text, 0, null))\n"
    "end\n";

static const char xl6StdCodecBase64DecodeBody[] =
    "func __xl_std_codec_base64_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_base64_decode(text, 0, null))\n"
    "end\n";

static const char xl6StdCodecHexEncodeBytesBody[] =
    "func __xl_std_codec_hex_encode_bytes(point data, c_size size) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_hex_encode_bytes(data, size))\n"
    "end\n";

static const char xl6StdCodecHexDecodeBytesBody[] =
    "func __xl_std_codec_hex_decode_bytes(string text) as Buffer\n"
    "    return Buffer(__xl_std_xrt_buffer_from_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCodecBase64EncodeBytesBody[] =
    "func __xl_std_codec_base64_encode_bytes(point data, c_size size) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_base64_encode_bytes(data, size, null))\n"
    "end\n";

static const char xl6StdCodecBase64DecodeBytesBody[] =
    "func __xl_std_codec_base64_decode_bytes(string text) as Buffer\n"
    "    return Buffer(__xl_std_xrt_buffer_from_base64(text, __xl_std_crt_strlen(text), null))\n"
    "end\n";

static const char xl6StdCodecPercentEncodeBody[] =
    "func __xl_std_codec_percent_encode(string text, bool spaceAsPlus = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_percent_encode(text, __xl_std_crt_strlen(text), c_bool(spaceAsPlus), null))\n"
    "end\n";

static const char xl6StdCodecPercentDecodeBody[] =
    "func __xl_std_codec_percent_decode(string text, bool plusAsSpace = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_percent_decode(text, __xl_std_crt_strlen(text), c_bool(plusAsSpace), null))\n"
    "end\n";

static const char xl6StdCodecFormUrlEncodeBody[] =
    "func __xl_std_codec_form_url_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_form_url_encode(text, __xl_std_crt_strlen(text), null))\n"
    "end\n";

static const char xl6StdCodecFormUrlDecodeBody[] =
    "func __xl_std_codec_form_url_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_form_url_decode(text, __xl_std_crt_strlen(text), null))\n"
    "end\n";

static const char xl6StdEncodingCharSizeBody[] =
    "func __xl_std_encoding_char_size(int charset) as int\n"
    "    return int(__xl_std_runtime_encoding_char_size(charset))\n"
    "end\n";

static const char xl6StdEncodingIsUtf8Body[] =
    "func __xl_std_encoding_is_utf8(string text) as bool\n"
    "    return __xl_std_runtime_encoding_is_utf8_text(text)\n"
    "end\n";

static const char xl6StdEncodingIsUtf8BytesBody[] =
    "func __xl_std_encoding_is_utf8_bytes(point data, c_size size) as bool\n"
    "    return __xl_std_runtime_encoding_is_utf8_bytes(data, size)\n"
    "end\n";

static const char xl6StdEncodingDetectBody[] =
    "func __xl_std_encoding_detect(string text, bool withBom = true) as int\n"
    "    return int(__xl_std_runtime_encoding_detect_text(text, withBom))\n"
    "end\n";

static const char xl6StdEncodingDetectBytesBody[] =
    "func __xl_std_encoding_detect_bytes(point data, c_size size, bool withBom = true) as int\n"
    "    return int(__xl_std_runtime_encoding_detect_bytes(data, size, withBom))\n"
    "end\n";

static const char xl6StdEncodingToBytesBody[] =
    "func __xl_std_encoding_to_bytes(string text, int toCharset = 65001) as Buffer\n"
    "    return Buffer(__xl_std_runtime_encoding_to_bytes(text, toCharset))\n"
    "end\n";

static const char xl6StdEncodingFromBytesBody[] =
    "func __xl_std_encoding_from_bytes(point data, c_size size, int fromCharset = 65001) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data, size, fromCharset))\n"
    "end\n";

static const char xl6StdEncodingConvertBytesBody[] =
    "func __xl_std_encoding_convert_bytes(point data, c_size size, int fromCharset, int toCharset) as Buffer\n"
    "    return Buffer(__xl_std_runtime_encoding_convert_bytes(data, size, fromCharset, toCharset))\n"
    "end\n";

static const char xl6StdEncodingNameBody[] =
    "func __xl_std_encoding_name(int charset) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_name(charset))\n"
    "end\n";

static const char xl6StdHashU32Body[] =
    "func __xl_std_hash_u32(string text) as uint32\n"
    "    return __xl_std_xrt_hash32(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdHashU32WithSeedBody[] =
    "func __xl_std_hash_u32_with_seed(string text, uint32 seed) as uint32\n"
    "    return __xl_std_xrt_hash32_seed(text, __xl_std_crt_strlen(text), c_uint(seed))\n"
    "end\n";

static const char xl6StdHashU64Body[] =
    "func __xl_std_hash_u64(string text) as uint64\n"
    "    return __xl_std_xrt_hash64(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdHashU64WithSeedBody[] =
    "func __xl_std_hash_u64_with_seed(string text, uint64 seed) as uint64\n"
    "    return __xl_std_xrt_hash64_seed(text, __xl_std_crt_strlen(text), c_ulonglong(seed))\n"
    "end\n";

static const char xl6StdHashU64MicroBody[] =
    "func __xl_std_hash_u64_micro(string text) as uint64\n"
    "    return __xl_std_xrt_hash64_micro(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdHashU64MicroWithSeedBody[] =
    "func __xl_std_hash_u64_micro_with_seed(string text, uint64 seed) as uint64\n"
    "    return __xl_std_xrt_hash64_micro_seed(text, __xl_std_crt_strlen(text), c_ulonglong(seed))\n"
    "end\n";

static const char xl6StdHashU64NanoBody[] =
    "func __xl_std_hash_u64_nano(string text) as uint64\n"
    "    return __xl_std_xrt_hash64_nano(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdHashU64NanoWithSeedBody[] =
    "func __xl_std_hash_u64_nano_with_seed(string text, uint64 seed) as uint64\n"
    "    return __xl_std_xrt_hash64_nano_seed(text, __xl_std_crt_strlen(text), c_ulonglong(seed))\n"
    "end\n";

static const char xl6StdCryptoSha1Body[] =
    "func __xl_std_crypto_sha1(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha1_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoSha256Body[] =
    "func __xl_std_crypto_sha256(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha256_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoSha384Body[] =
    "func __xl_std_crypto_sha384(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha384_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoSha512Body[] =
    "func __xl_std_crypto_sha512(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha512_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoHmacSha256Body[] =
    "func __xl_std_crypto_hmac_sha256(string key, string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_hmac_sha256_hex(key, __xl_std_crt_strlen(key), text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoHmacSha384Body[] =
    "func __xl_std_crypto_hmac_sha384(string key, string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_hmac_sha384_hex(key, __xl_std_crt_strlen(key), text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoHmacSha512Body[] =
    "func __xl_std_crypto_hmac_sha512(string key, string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_hmac_sha512_hex(key, __xl_std_crt_strlen(key), text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoChaCha20Poly1305EncryptBody[] =
    "func __xl_std_crypto_chacha20_poly1305_encrypt(string key, string nonce, string text, string aad = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_chacha20_poly1305_encrypt_hex(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), text, __xl_std_crt_strlen(text), aad, __xl_std_crt_strlen(aad)))\n"
    "end\n";

static const char xl6StdCryptoChaCha20Poly1305DecryptBody[] =
    "func __xl_std_crypto_chacha20_poly1305_decrypt(string key, string nonce, string cipherHex, string aad = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_chacha20_poly1305_decrypt_text(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), cipherHex, __xl_std_crt_strlen(cipherHex), aad, __xl_std_crt_strlen(aad)))\n"
    "end\n";

static const char xl6StdCryptoAes128GcmEncryptBody[] =
    "func __xl_std_crypto_aes128_gcm_encrypt(string key, string nonce, string text, string aad = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_aes128_gcm_encrypt_hex(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), text, __xl_std_crt_strlen(text), aad, __xl_std_crt_strlen(aad)))\n"
    "end\n";

static const char xl6StdCryptoAes128GcmDecryptBody[] =
    "func __xl_std_crypto_aes128_gcm_decrypt(string key, string nonce, string cipherHex, string aad = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_aes128_gcm_decrypt_text(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), cipherHex, __xl_std_crt_strlen(cipherHex), aad, __xl_std_crt_strlen(aad)))\n"
    "end\n";

static const char xl6StdCryptoAes256GcmEncryptBody[] =
    "func __xl_std_crypto_aes256_gcm_encrypt(string key, string nonce, string text, string aad = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_aes256_gcm_encrypt_hex(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), text, __xl_std_crt_strlen(text), aad, __xl_std_crt_strlen(aad)))\n"
    "end\n";

static const char xl6StdCryptoAes256GcmDecryptBody[] =
    "func __xl_std_crypto_aes256_gcm_decrypt(string key, string nonce, string cipherHex, string aad = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_aes256_gcm_decrypt_text(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), cipherHex, __xl_std_crt_strlen(cipherHex), aad, __xl_std_crt_strlen(aad)))\n"
    "end\n";

static const char xl6StdCryptoHkdfSha256Body[] =
    "func __xl_std_crypto_hkdf_sha256(string key, string salt = \"\", string info = \"\", int len = 32) as string\n"
    "    if len <= 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_hkdf_sha256_hex(key, __xl_std_crt_strlen(key), salt, __xl_std_crt_strlen(salt), info, __xl_std_crt_strlen(info), c_size(len)))\n"
    "end\n";

static const char xl6StdCryptoHkdfSha384Body[] =
    "func __xl_std_crypto_hkdf_sha384(string key, string salt = \"\", string info = \"\", int len = 32) as string\n"
    "    if len <= 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_hkdf_sha384_hex(key, __xl_std_crt_strlen(key), salt, __xl_std_crt_strlen(salt), info, __xl_std_crt_strlen(info), c_size(len)))\n"
    "end\n";

static const char xl6StdCryptoRandomHexBody[] =
    "func __xl_std_crypto_random_hex(int len) as string\n"
    "    if len <= 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_random_hex(c_size(len)))\n"
    "end\n";

static const char xl6StdCryptoSha256StreamBody[] =
    "func __xl_std_crypto_sha256_stream() as Sha256\n"
    "    return Sha256()\n"
    "end\n";

static const char xl6StdCryptoSha384StreamBody[] =
    "func __xl_std_crypto_sha384_stream() as Sha384\n"
    "    return Sha384()\n"
    "end\n";

static const char xl6StdCryptoSha512StreamBody[] =
    "func __xl_std_crypto_sha512_stream() as Sha512\n"
    "    return Sha512()\n"
    "end\n";

static const char xl6StdCryptoChaCha20EncryptBody[] =
    "func __xl_std_crypto_chacha20_encrypt(string key, string nonce, string text, int counter = 1) as string\n"
    "    if counter < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_chacha20_encrypt_hex(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), text, __xl_std_crt_strlen(text), c_uint(counter)))\n"
    "end\n";

static const char xl6StdCryptoChaCha20DecryptBody[] =
    "func __xl_std_crypto_chacha20_decrypt(string key, string nonce, string cipherHex, int counter = 1) as string\n"
    "    if counter < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_chacha20_decrypt_text(key, __xl_std_crt_strlen(key), nonce, __xl_std_crt_strlen(nonce), cipherHex, __xl_std_crt_strlen(cipherHex), c_uint(counter)))\n"
    "end\n";

static const char xl6StdCryptoEd25519KeypairBody[] =
    "func __xl_std_crypto_ed25519_keypair() as var\n"
    "    return __xl_std_runtime_ed25519_keypair()\n"
    "end\n";

static const char xl6StdCryptoEd25519PublicKeyBody[] =
    "func __xl_std_crypto_ed25519_public_key(string seedHex) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_ed25519_public_key_hex(seedHex, __xl_std_crt_strlen(seedHex)))\n"
    "end\n";

static const char xl6StdCryptoEd25519SignBody[] =
    "func __xl_std_crypto_ed25519_sign(string seedHex, string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_ed25519_sign_hex(seedHex, __xl_std_crt_strlen(seedHex), text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoEd25519VerifyBody[] =
    "func __xl_std_crypto_ed25519_verify(string publicKeyHex, string text, string signatureHex) as bool\n"
    "    return __xl_std_runtime_ed25519_verify(publicKeyHex, __xl_std_crt_strlen(publicKeyHex), text, __xl_std_crt_strlen(text), signatureHex, __xl_std_crt_strlen(signatureHex))\n"
    "end\n";

static const char xl6StdCryptoX25519KeypairBody[] =
    "func __xl_std_crypto_x25519_keypair() as var\n"
    "    return __xl_std_runtime_x25519_keypair()\n"
    "end\n";

static const char xl6StdCryptoX25519SharedSecretBody[] =
    "func __xl_std_crypto_x25519_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_x25519_shared_secret_hex(privateKeyHex, __xl_std_crt_strlen(privateKeyHex), publicKeyHex, __xl_std_crt_strlen(publicKeyHex)))\n"
    "end\n";

static const char xl6StdCryptoX448KeypairBody[] =
    "func __xl_std_crypto_x448_keypair() as var\n"
    "    return __xl_std_runtime_x448_keypair()\n"
    "end\n";

static const char xl6StdCryptoX448SharedSecretBody[] =
    "func __xl_std_crypto_x448_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_x448_shared_secret_hex(privateKeyHex, __xl_std_crt_strlen(privateKeyHex), publicKeyHex, __xl_std_crt_strlen(publicKeyHex)))\n"
    "end\n";

static const char xl6StdCryptoRsaPssVerifyBody[] =
    "func __xl_std_crypto_rsa_pss_verify(string hashHex, string signatureHex, string modulusHex, string exponentHex) as bool\n"
    "    return __xl_std_runtime_rsa_pss_verify(hashHex, __xl_std_crt_strlen(hashHex), signatureHex, __xl_std_crt_strlen(signatureHex), modulusHex, __xl_std_crt_strlen(modulusHex), exponentHex, __xl_std_crt_strlen(exponentHex))\n"
    "end\n";

static const char xl6StdCryptoRsaPkcs1VerifyBody[] =
    "func __xl_std_crypto_rsa_pkcs1_verify(string hashHex, string signatureHex, string modulusHex, string exponentHex) as bool\n"
    "    return __xl_std_runtime_rsa_pkcs1_verify(hashHex, __xl_std_crt_strlen(hashHex), signatureHex, __xl_std_crt_strlen(signatureHex), modulusHex, __xl_std_crt_strlen(modulusHex), exponentHex, __xl_std_crt_strlen(exponentHex))\n"
    "end\n";

static const char xl6StdIdXidBody[] =
    "func __xl_std_id_xid() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_make_xids())\n"
    "end\n";

static const char xl6StdNetLocalIPBody[] =
    "func __xl_std_net_local_ip() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_ip())\n"
    "end\n";

static const char xl6StdNetLocalMACBody[] =
    "func __xl_std_net_local_mac() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_mac())\n"
    "end\n";

static const char xl6StdNetLocalNameBody[] =
    "func __xl_std_net_local_name() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_name())\n"
    "end\n";

static const char xl6StdTcpConnectBody[] =
    "func __xl_std_tcp_connect(string host, int port, int timeoutMs = 5000) as point\n"
    "    if port <= 0 or port > 65535 then\n"
    "        return null\n"
    "    end\n"
    "    return __xl_std_xrt_tcp_connect(host, uint16(port), timeoutMs)\n"
    "end\n";

static const char xl6StdTcpConnectAsyncBody[] =
    "func __xl_std_tcp_connect_async(string host, int port, int timeoutMs = 5000) as future<point>\n"
    "    if port <= 0 or port > 65535 then\n"
    "        return __xl_std_xrt_tcp_connect_async(\"\", uint16(0), timeoutMs)\n"
    "    end\n"
    "    return __xl_std_xrt_tcp_connect_async(host, uint16(port), timeoutMs)\n"
    "end\n";

static const char xl6StdTcpListenBody[] =
    "func __xl_std_tcp_listen(int port, string host = \"127.0.0.1\", int backlog = 128) as point\n"
    "    if port < 0 or port > 65535 then\n"
    "        return null\n"
    "    end\n"
    "    if backlog < 0 then\n"
    "        backlog = 0\n"
    "    end\n"
    "    return __xl_std_xrt_tcp_listen(host, uint16(port), c_uint(backlog))\n"
    "end\n";

static const char xl6StdUdpBindBody[] =
    "func __xl_std_udp_bind(int port, string host = \"127.0.0.1\") as point\n"
    "    if port < 0 or port > 65535 then\n"
    "        return null\n"
    "    end\n"
    "    return __xl_std_xrt_udp_bind(host, uint16(port))\n"
    "end\n";

static const char xl6StdUrlValidBody[] =
    "func __xl_std_url_valid(string text) as bool\n"
    "    return __xl_std_runtime_url_flag(text, __xl_std_crt_strlen(text), 0)\n"
    "end\n";

static const char xl6StdUrlIsAbsoluteBody[] =
    "func __xl_std_url_is_absolute(string text) as bool\n"
    "    return __xl_std_runtime_url_flag(text, __xl_std_crt_strlen(text), 1)\n"
    "end\n";

static const char xl6StdUrlIsSecureBody[] =
    "func __xl_std_url_is_secure(string text) as bool\n"
    "    return __xl_std_runtime_url_flag(text, __xl_std_crt_strlen(text), 2)\n"
    "end\n";

static const char xl6StdUrlPortBody[] =
    "func __xl_std_url_port(string text) as int\n"
    "    return int(__xl_std_runtime_url_port(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdUrlSchemeBody[] =
    "func __xl_std_url_scheme(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 1))\n"
    "end\n";

static const char xl6StdUrlAuthorityBody[] =
    "func __xl_std_url_authority(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 2))\n"
    "end\n";

static const char xl6StdUrlUserInfoBody[] =
    "func __xl_std_url_user_info(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 3))\n"
    "end\n";

static const char xl6StdUrlHostBody[] =
    "func __xl_std_url_host(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 4))\n"
    "end\n";

static const char xl6StdUrlPathBody[] =
    "func __xl_std_url_path(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 5))\n"
    "end\n";

static const char xl6StdUrlQueryBody[] =
    "func __xl_std_url_query(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 6))\n"
    "end\n";

static const char xl6StdUrlFragmentBody[] =
    "func __xl_std_url_fragment(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 7))\n"
    "end\n";

static const char xl6StdUrlTargetBody[] =
    "func __xl_std_url_target(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 8))\n"
    "end\n";

static const char xl6StdUrlResolveBody[] =
    "func __xl_std_url_resolve(string base, string ref) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_resolve(base, __xl_std_crt_strlen(base), ref, __xl_std_crt_strlen(ref)))\n"
    "end\n";

static const char xl6StdUrlNormalizePathBody[] =
    "func __xl_std_url_normalize_path(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_normalize_path(path, __xl_std_crt_strlen(path)))\n"
    "end\n";

static const char xl6StdQueryEncodeBody[] =
    "func __xl_std_query_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_encode(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdQueryDecodeBody[] =
    "func __xl_std_query_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_decode(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdQueryValidBody[] =
    "func __xl_std_query_valid(string text) as bool\n"
    "    return __xl_std_runtime_query_valid(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdQueryPairBody[] =
    "func __xl_std_query_pair(string name, string value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_pair(name, __xl_std_crt_strlen(name), value, __xl_std_crt_strlen(value)))\n"
    "end\n";

static const char xl6StdQueryAppendBody[] =
    "func __xl_std_query_append(string query, string name, string value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_append(query, __xl_std_crt_strlen(query), name, __xl_std_crt_strlen(name), value, __xl_std_crt_strlen(value)))\n"
    "end\n";

static const char xl6StdHttpServerCreateBody[] =
    "func __xl_std_http_server_create(int port = 0, string host = \"127.0.0.1\", int backlog = 128, int recvLimit = 1048576, int bodyLimit = 1048576, int headerLimit = 100, int headerBytesLimit = 65536, int workerCount = 1, int headerTimeoutMs = 30000, int bodyTimeoutMs = 30000, int idleTimeoutMs = 60000, int writeTimeoutMs = 30000, int multipartPartLimit = 1000, int multipartPartSizeLimit = 0) as point\n"
    "    return __xl_std_runtime_http_server_create(host, c_int(port), c_uint(backlog), c_uint(recvLimit), c_uint(bodyLimit), c_uint(headerLimit), c_uint(headerBytesLimit), c_uint(workerCount), c_uint(headerTimeoutMs), c_uint(bodyTimeoutMs), c_uint(idleTimeoutMs), c_uint(writeTimeoutMs), c_uint(multipartPartLimit), c_uint(multipartPartSizeLimit))\n"
    "end\n";

static const char xl6StdHttpServerCreateTlsBody[] =
    "func __xl_std_http_server_create_tls(string certFile, string keyFile, int port = 0, string host = \"127.0.0.1\", int backlog = 128, int recvLimit = 1048576, int bodyLimit = 1048576, int headerLimit = 100, int headerBytesLimit = 65536, int workerCount = 1, int headerTimeoutMs = 30000, int bodyTimeoutMs = 30000, int idleTimeoutMs = 60000, int writeTimeoutMs = 30000, int multipartPartLimit = 1000, int multipartPartSizeLimit = 0) as point\n"
    "    return __xl_std_runtime_http_server_create_tls(certFile, keyFile, host, c_int(port), c_uint(backlog), c_uint(recvLimit), c_uint(bodyLimit), c_uint(headerLimit), c_uint(headerBytesLimit), c_uint(workerCount), c_uint(headerTimeoutMs), c_uint(bodyTimeoutMs), c_uint(idleTimeoutMs), c_uint(writeTimeoutMs), c_uint(multipartPartLimit), c_uint(multipartPartSizeLimit))\n"
    "end\n";

static const char xl6StdHttpAppCreateBody[] =
    "func __xl_std_http_app_create() as point\n"
    "    return __xl_std_runtime_http_app_create()\n"
    "end\n";

static const char xl6StdHttpAppDestroyBody[] =
    "func __xl_std_http_app_destroy(point app)\n"
    "    __xl_std_runtime_http_app_destroy(app)\n"
    "end\n";

static const char xl6StdHttpAppRouteBody[] =
    "func __xl_std_http_app_route(point app, string method, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_app_route(app, method, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppRouteRequestBody[] =
    "func __xl_std_http_app_route_request(point app, string method, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_app_route_request(app, method, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppGetBody[] =
    "func __xl_std_http_app_get(point app, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_app_get(app, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppPostBody[] =
    "func __xl_std_http_app_post(point app, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_app_post(app, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppAnyBody[] =
    "func __xl_std_http_app_any(point app, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_app_any(app, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppGetRequestBody[] =
    "func __xl_std_http_app_get_request(point app, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_app_get_request(app, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppPostRequestBody[] =
    "func __xl_std_http_app_post_request(point app, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_app_post_request(app, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppAnyRequestBody[] =
    "func __xl_std_http_app_any_request(point app, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_app_any_request(app, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpAppRouteStreamRequestBody[] =
    "func __xl_std_http_app_route_stream_request(point app, string method, string pattern, func(point) as bool beginHandler, func(point, point, int) as bool chunkHandler, func(point, point) as bool endHandler) as bool\n"
    "    return __xl_std_runtime_http_app_route_stream_request(app, method, pattern, beginHandler, chunkHandler, endHandler)\n"
    "end\n";

static const char xl6StdHttpAppPostStreamRequestBody[] =
    "func __xl_std_http_app_post_stream_request(point app, string pattern, func(point) as bool beginHandler, func(point, point, int) as bool chunkHandler, func(point, point) as bool endHandler) as bool\n"
    "    return __xl_std_runtime_http_app_post_stream_request(app, pattern, beginHandler, chunkHandler, endHandler)\n"
    "end\n";

static const char xl6StdHttpAppAnyStreamRequestBody[] =
    "func __xl_std_http_app_any_stream_request(point app, string pattern, func(point) as bool beginHandler, func(point, point, int) as bool chunkHandler, func(point, point) as bool endHandler) as bool\n"
    "    return __xl_std_runtime_http_app_any_stream_request(app, pattern, beginHandler, chunkHandler, endHandler)\n"
    "end\n";

static const char xl6StdHttpAppStaticBody[] =
    "func __xl_std_http_app_static(point app, string mount, string root) as bool\n"
    "    return __xl_std_runtime_http_app_static(app, mount, root)\n"
    "end\n";

static const char xl6StdHttpAppStaticExBody[] =
    "func __xl_std_http_app_static_ex(point app, string mount, string root, string indexNames = \"index.html;index.htm\", string cacheControl = \"\", int chunkSize = 65536, int flags = 0) as bool\n"
    "    return __xl_std_runtime_http_app_static_ex(app, mount, root, indexNames, cacheControl, c_size(chunkSize), uint32(flags))\n"
    "end\n";

static const char xl6StdHttpAppErrorBody[] =
    "func __xl_std_http_app_error(point app, func(int, string, point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_app_error(app, handler)\n"
    "end\n";

static const char xl6StdHttpServerSetAppBody[] =
    "func __xl_std_http_server_set_app(point server, point app) as bool\n"
    "    return __xl_std_runtime_http_server_set_app(server, app)\n"
    "end\n";

static const char xl6StdHttpServerReloadAppBody[] =
    "func __xl_std_http_server_reload_app(point server, point app) as bool\n"
    "    return __xl_std_runtime_http_server_reload_app(server, app)\n"
    "end\n";

static const char xl6StdHttpServerHostBody[] =
    "func __xl_std_http_server_host(point server, string host, point app) as bool\n"
    "    return __xl_std_runtime_http_server_host(server, host, app)\n"
    "end\n";

static const char xl6StdHttpServerRemoveHostBody[] =
    "func __xl_std_http_server_remove_host(point server, string host) as bool\n"
    "    return __xl_std_runtime_http_server_remove_host(server, host)\n"
    "end\n";

static const char xl6StdHttpServerTlsHostBody[] =
    "func __xl_std_http_server_tls_host(point server, string host, string certFile, string keyFile) as bool\n"
    "    return __xl_std_runtime_http_server_tls_host(server, host, certFile, keyFile)\n"
    "end\n";

static const char xl6StdHttpServerRemoveTlsHostBody[] =
    "func __xl_std_http_server_remove_tls_host(point server, string host) as bool\n"
    "    return __xl_std_runtime_http_server_remove_tls_host(server, host)\n"
    "end\n";

static const char xl6StdHttpServerErrorBody[] =
    "func __xl_std_http_server_error(point server, func(int, string, point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_server_error(server, handler)\n"
    "end\n";

static const char xl6StdHttpServerRouteBody[] =
    "func __xl_std_http_server_route(point server, string method, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_server_route(server, method, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerRouteRequestBody[] =
    "func __xl_std_http_server_route_request(point server, string method, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_server_route_request(server, method, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerGetBody[] =
    "func __xl_std_http_server_get(point server, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_server_get(server, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerPostBody[] =
    "func __xl_std_http_server_post(point server, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_server_post(server, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerAnyBody[] =
    "func __xl_std_http_server_any(point server, string pattern, func(string, string, string, string) as string handler) as bool\n"
    "    return __xl_std_runtime_http_server_any(server, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerGetRequestBody[] =
    "func __xl_std_http_server_get_request(point server, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_server_get_request(server, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerPostRequestBody[] =
    "func __xl_std_http_server_post_request(point server, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_server_post_request(server, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerAnyRequestBody[] =
    "func __xl_std_http_server_any_request(point server, string pattern, func(point, point) as bool handler) as bool\n"
    "    return __xl_std_runtime_http_server_any_request(server, pattern, handler)\n"
    "end\n";

static const char xl6StdHttpServerRouteStreamRequestBody[] =
    "func __xl_std_http_server_route_stream_request(point server, string method, string pattern, func(point) as bool beginHandler, func(point, point, int) as bool chunkHandler, func(point, point) as bool endHandler) as bool\n"
    "    return __xl_std_runtime_http_server_route_stream_request(server, method, pattern, beginHandler, chunkHandler, endHandler)\n"
    "end\n";

static const char xl6StdHttpServerPostStreamRequestBody[] =
    "func __xl_std_http_server_post_stream_request(point server, string pattern, func(point) as bool beginHandler, func(point, point, int) as bool chunkHandler, func(point, point) as bool endHandler) as bool\n"
    "    return __xl_std_runtime_http_server_post_stream_request(server, pattern, beginHandler, chunkHandler, endHandler)\n"
    "end\n";

static const char xl6StdHttpServerAnyStreamRequestBody[] =
    "func __xl_std_http_server_any_stream_request(point server, string pattern, func(point) as bool beginHandler, func(point, point, int) as bool chunkHandler, func(point, point) as bool endHandler) as bool\n"
    "    return __xl_std_runtime_http_server_any_stream_request(server, pattern, beginHandler, chunkHandler, endHandler)\n"
    "end\n";

static const char xl6StdHttpServerStaticBody[] =
    "func __xl_std_http_server_static(point server, string mount, string root) as bool\n"
    "    return __xl_std_runtime_http_server_static(server, mount, root)\n"
    "end\n";

static const char xl6StdHttpServerStaticExBody[] =
    "func __xl_std_http_server_static_ex(point server, string mount, string root, string indexNames = \"index.html;index.htm\", string cacheControl = \"\", int chunkSize = 65536, int flags = 0) as bool\n"
    "    return __xl_std_runtime_http_server_static_ex(server, mount, root, indexNames, cacheControl, c_size(chunkSize), uint32(flags))\n"
    "end\n";

static const char xl6StdHttpServerStartBody[] =
    "func __xl_std_http_server_start(point server) as bool\n"
    "    return __xl_std_runtime_http_server_start(server)\n"
    "end\n";

static const char xl6StdHttpServerPortBody[] =
    "func __xl_std_http_server_port(point server) as int\n"
    "    return int(__xl_std_runtime_http_server_port(server))\n"
    "end\n";

static const char xl6StdHttpServerRequestCountBody[] =
    "func __xl_std_http_server_request_count(point server) as int\n"
    "    return int(__xl_std_runtime_http_server_request_count(server))\n"
    "end\n";

static const char xl6StdHttpServerConnectionCountBody[] =
    "func __xl_std_http_server_connection_count(point server) as int\n"
    "    return int(__xl_std_runtime_http_server_connection_count(server))\n"
    "end\n";

static const char xl6StdHttpServerStopBody[] =
    "func __xl_std_http_server_stop(point server)\n"
    "    __xl_std_runtime_http_server_stop(server)\n"
    "end\n";

static const char xl6StdHttpServerDrainBody[] =
    "func __xl_std_http_server_drain(point server, int timeoutMs = -1) as bool\n"
    "    return __xl_std_runtime_http_server_drain(server, timeoutMs)\n"
    "end\n";

static const char xl6StdHttpServerDestroyBody[] =
    "func __xl_std_http_server_destroy(point server)\n"
    "    __xl_std_runtime_http_server_destroy(server)\n"
    "end\n";

static const char xl6StdHttpServerMimeByPathBody[] =
    "func __xl_std_http_server_mime_by_path(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_http_mime_by_path(path))\n"
    "end\n";

static const char xl6StdHttpServerChunkTextBody[] =
    "func __xl_std_http_server_chunk_text(point data, int size) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_http_body_chunk_text(data, c_size(size)))\n"
    "end\n";

static const char xl6StdHttpServerChunkBytesBody[] =
    "func __xl_std_http_server_chunk_bytes(point data, int size) as Buffer\n"
    "    point handle = __xl_std_runtime_http_body_chunk_bytes(data, c_size(size))\n"
    "    return Buffer(handle)\n"
    "end\n";

static const char xl6StdHttpRequestBody[] =
    "func __xl_std_http_request(string method, string url, string body = \"\", string contentType = \"\", int timeoutMs = 30000) as HttpClientResponse\n"
    "    HttpClientRequest req = HttpClientRequest(url)\n"
    "    req.method(method)\n"
    "    req.timeout(timeoutMs)\n"
    "    if body != \"\" or contentType != \"\" then\n"
    "        req.text(body, contentType)\n"
    "    end\n"
    "    return req.execute()\n"
    "end\n";

static const char xl6StdHttpGetBody[] =
    "func __xl_std_http_get(string url, int timeoutMs = 30000) as HttpClientResponse\n"
    "    return __xl_std_http_request(\"GET\", url, \"\", \"\", timeoutMs)\n"
    "end\n";

static const char xl6StdHttpPostBody[] =
    "func __xl_std_http_post(string url, string body = \"\", string contentType = \"text/plain; charset=utf-8\", int timeoutMs = 30000) as HttpClientResponse\n"
    "    return __xl_std_http_request(\"POST\", url, body, contentType, timeoutMs)\n"
    "end\n";

static const char xl6StdPrintBody[] =
    "func __xl_std_print(...)\n"
    "    __xl_std_runtime_print(arg, false)\n"
    "end\n";

static const char xl6StdPrintlnBody[] =
    "func __xl_std_println(...)\n"
    "    __xl_std_runtime_print(arg, true)\n"
    "end\n";

static const char xl6StdTccContextBody[] =
    "func __xl_std_tcc_context() as TccContext\n"
    "    return TccContext()\n"
    "end\n";

static const char xl6StdTccRunBody[] =
    "func __xl_std_tcc_run(string source) as int\n"
    "    TccContext ctx = TccContext()\n"
    "    if ctx.compileString(source) != 0 then\n"
    "        return -1\n"
    "    end\n"
    "    return ctx.run()\n"
    "end\n";

static const char xl6StdTccRunFileBody[] =
    "func __xl_std_tcc_run_file(string path) as int\n"
    "    TccContext ctx = TccContext()\n"
    "    if ctx.addFile(path) != 0 then\n"
    "        return -1\n"
    "    end\n"
    "    return ctx.run()\n"
    "end\n";

static const char xl6StdTccCompileBody[] =
    "func __xl_std_tcc_compile(string source, string output, int outputType = 2) as int\n"
    "    TccContext ctx = TccContext(c_int(outputType))\n"
    "    if ctx.compileString(source) != 0 then\n"
    "        return -1\n"
    "    end\n"
    "    return int(ctx.outputFile(output))\n"
    "end\n";

static const char xl6StdTccCompileFileBody[] =
    "func __xl_std_tcc_compile_file(string path, string output, int outputType = 2) as int\n"
    "    TccContext ctx = TccContext(c_int(outputType))\n"
    "    if ctx.addFile(path) != 0 then\n"
    "        return -1\n"
    "    end\n"
    "    return int(ctx.outputFile(output))\n"
    "end\n";

static const char xl6StdTccVfsMountBody[] =
    "extern \"xl6_tcc_vfs_mount_text\" func __xl_std_tcc_vfs_mount_text(string path, string text) as c_int\n"
    "func __xl_std_tcc_vfs_mount(string path, string text) as bool\n"
    "    return __xl_std_tcc_vfs_mount_text(path, text) != 0\n"
    "end\n";

static const char xl6StdTccVfsClearBody[] =
    "extern \"tcc_vfs_clear_dynamic\" func __xl_std_tcc_vfs_clear_dynamic()\n"
    "func __xl_std_tcc_vfs_clear()\n"
    "    __xl_std_tcc_vfs_clear_dynamic()\n"
    "end\n";

static const char xl6StdXlangContextBody[] =
    "func __xl_std_xlang_context() as XLangContext\n"
    "    return XLangContext()\n"
    "end\n";

static const char xl6StdXlangRunBody[] =
    "extern \"xl6_xlang_run_source\" func __xl_std_xlang_run_source(string source) as c_int\n"
    "func __xl_std_xlang_run(string source) as bool\n"
    "    return __xl_std_xlang_run_source(source) != 0\n"
    "end\n";

static const char xl6StdXlangRunFileBody[] =
    "extern \"xl6_xlang_run_file\" func __xl_std_xlang_run_file_native(string path) as c_int\n"
    "func __xl_std_xlang_run_file(string path) as bool\n"
    "    return __xl_std_xlang_run_file_native(path) != 0\n"
    "end\n";

const XL6_StdlibConst xl6StdlibConsts[] = {
    {"math", "pi", XL6_AST_FLOAT, "3.14159265358979323846264338327950288"},
    {"math", "tau", XL6_AST_FLOAT, "6.28318530717958647692528676655900576"},
    {"math", "e", XL6_AST_FLOAT, "2.71828182845904523536028747135266250"},
    {"math", "nan", XL6_AST_FLOAT, "(0.0/0.0)"},
    {"math", "inf", XL6_AST_FLOAT, "(1.0/0.0)"},
    {"regex", "ignoreCase", XL6_AST_INT, "1"},
    {"regex", "multiline", XL6_AST_INT, "2"},
    {"regex", "dotAll", XL6_AST_INT, "4"},
    {"regex", "ungreedy", XL6_AST_INT, "8"},
    {"log", "traceLevel", XL6_AST_INT, "0"},
    {"log", "debugLevel", XL6_AST_INT, "1"},
    {"log", "infoLevel", XL6_AST_INT, "2"},
    {"log", "warnLevel", XL6_AST_INT, "3"},
    {"log", "errorLevel", XL6_AST_INT, "4"},
    {"log", "fatalLevel", XL6_AST_INT, "5"},
    {"log", "offLevel", XL6_AST_INT, "6"},
    {"log", "textFormat", XL6_AST_INT, "0"},
    {"log", "simpleFormat", XL6_AST_INT, "1"},
    {"log", "jsonFormat", XL6_AST_INT, "2"},
    {"file", "seekSet", XL6_AST_INT, "0"},
    {"file", "seekCur", XL6_AST_INT, "1"},
    {"file", "seekEnd", XL6_AST_INT, "2"},
    {"encoding", "autoDetect", XL6_AST_INT, "-2"},
    {"encoding", "binary", XL6_AST_INT, "-1"},
    {"encoding", "oem", XL6_AST_INT, "0"},
    {"encoding", "utf8", XL6_AST_INT, "65001"},
    {"encoding", "utf16", XL6_AST_INT, "1200"},
    {"encoding", "utf16BE", XL6_AST_INT, "1201"},
    {"encoding", "utf32", XL6_AST_INT, "65005"},
    {"encoding", "utf32BE", XL6_AST_INT, "65006"},
    {"encoding", "bom", XL6_AST_INT, "1073741824"},
    {"tcc", "MEMORY", XL6_AST_INT, "1"},
    {"tcc", "EXE", XL6_AST_INT, "2"},
    {"tcc", "OBJ", XL6_AST_INT, "3"},
    {"tcc", "DLL", XL6_AST_INT, "4"},
    {"tcc", "PREPROCESS", XL6_AST_INT, "5"},
    {"http.server", "allowDotfiles", XL6_AST_INT, "1"},
    {"http.server", "allowBackslash", XL6_AST_INT, "2"},
    {"http.server", "sameSiteUnspecified", XL6_AST_INT, "0"},
    {"http.server", "sameSiteLax", XL6_AST_INT, "1"},
    {"http.server", "sameSiteStrict", XL6_AST_INT, "2"},
    {"http.server", "sameSiteNone", XL6_AST_INT, "3"},
    {"http.server", "cookieSecure", XL6_AST_INT, "64"},
    {"http.server", "cookieHttpOnly", XL6_AST_INT, "128"},
    {"http.server", "cookiePartitioned", XL6_AST_INT, "256"},
    {"http.server", "cookieSameParty", XL6_AST_INT, "512"}
};

const XL6_StdlibFunc xl6StdlibFuncs[] = {
    {"__crt_string", "strlen", "__xl_std_crt_strlen", "strlen", xl6StdStringLenParams, 1, "c_size", NULL},
    {"__xl_runtime", "takeCstr", "__xl_std_runtime_take_cstr", "xl6_xrt_take_cstr", xl6StdRuntimeTakeCstrParams, 1, "string", NULL},
    {"__xl_runtime", "print", "__xl_std_runtime_print", "xl6_xrt_print", xl6StdRuntimePrintParams, 2, "", NULL},
    {"__xl_runtime", "stringSplit", "__xl_std_runtime_string_split", "xl6_xrt_string_split", xl6StdRuntimeStringSplitParams, 2, "var", NULL},
    {"__xl_runtime", "stringChars", "__xl_std_runtime_string_chars", "xl6_xrt_string_chars", xl6StdRuntimeStringTextParams, 1, "var", NULL},
    {"__xl_runtime", "stringBytes", "__xl_std_runtime_string_bytes", "xl6_xrt_string_bytes", xl6StdRuntimeStringTextParams, 1, "var", NULL},
    {"__xl_runtime", "stringJoin", "__xl_std_runtime_string_join", "xl6_xrt_string_join", xl6StdRuntimeStringJoinParams, 2, "point", NULL},
    {"__xl_runtime", "stringSplitLines", "__xl_std_runtime_string_split_lines", "xl6_xrt_string_split_lines", xl6StdRuntimeStringTextParams, 1, "var", NULL},
    {"__xl_runtime", "stringStartsWith", "__xl_std_runtime_string_starts_with", "xl6_xrt_string_starts_with", xl6StdRuntimeStringEdgeParams, 5, "c_bool", NULL},
    {"__xl_runtime", "stringEndsWith", "__xl_std_runtime_string_ends_with", "xl6_xrt_string_ends_with", xl6StdRuntimeStringEdgeParams, 5, "c_bool", NULL},
    {"__xl_runtime", "sha1Hex", "__xl_std_runtime_sha1_hex", "xl6_xrt_sha1_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "sha256Hex", "__xl_std_runtime_sha256_hex", "xl6_xrt_sha256_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "sha384Hex", "__xl_std_runtime_sha384_hex", "xl6_xrt_sha384_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "sha512Hex", "__xl_std_runtime_sha512_hex", "xl6_xrt_sha512_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "hmacSha256Hex", "__xl_std_runtime_hmac_sha256_hex", "xl6_xrt_hmac_sha256_hex", xl6StdRuntimeCryptoHmacParams, 4, "point", NULL},
    {"__xl_runtime", "hmacSha384Hex", "__xl_std_runtime_hmac_sha384_hex", "xl6_xrt_hmac_sha384_hex", xl6StdRuntimeCryptoHmacParams, 4, "point", NULL},
    {"__xl_runtime", "hmacSha512Hex", "__xl_std_runtime_hmac_sha512_hex", "xl6_xrt_hmac_sha512_hex", xl6StdRuntimeCryptoHmacParams, 4, "point", NULL},
    {"__xl_runtime", "chacha20EncryptHex", "__xl_std_runtime_chacha20_encrypt_hex", "xl6_xrt_chacha20_encrypt_hex", xl6StdRuntimeCryptoChacha20Params, 7, "point", NULL},
    {"__xl_runtime", "chacha20DecryptText", "__xl_std_runtime_chacha20_decrypt_text", "xl6_xrt_chacha20_decrypt_text", xl6StdRuntimeCryptoChacha20Params, 7, "point", NULL},
    {"__xl_runtime", "chacha20Poly1305EncryptHex", "__xl_std_runtime_chacha20_poly1305_encrypt_hex", "xl6_xrt_chacha20_poly1305_encrypt_hex", xl6StdRuntimeCryptoAeadParams, 8, "point", NULL},
    {"__xl_runtime", "chacha20Poly1305DecryptText", "__xl_std_runtime_chacha20_poly1305_decrypt_text", "xl6_xrt_chacha20_poly1305_decrypt_text", xl6StdRuntimeCryptoAeadParams, 8, "point", NULL},
    {"__xl_runtime", "aes128GcmEncryptHex", "__xl_std_runtime_aes128_gcm_encrypt_hex", "xl6_xrt_aes128_gcm_encrypt_hex", xl6StdRuntimeCryptoAeadParams, 8, "point", NULL},
    {"__xl_runtime", "aes128GcmDecryptText", "__xl_std_runtime_aes128_gcm_decrypt_text", "xl6_xrt_aes128_gcm_decrypt_text", xl6StdRuntimeCryptoAeadParams, 8, "point", NULL},
    {"__xl_runtime", "aes256GcmEncryptHex", "__xl_std_runtime_aes256_gcm_encrypt_hex", "xl6_xrt_aes256_gcm_encrypt_hex", xl6StdRuntimeCryptoAeadParams, 8, "point", NULL},
    {"__xl_runtime", "aes256GcmDecryptText", "__xl_std_runtime_aes256_gcm_decrypt_text", "xl6_xrt_aes256_gcm_decrypt_text", xl6StdRuntimeCryptoAeadParams, 8, "point", NULL},
    {"__xl_runtime", "hkdfSha256Hex", "__xl_std_runtime_hkdf_sha256_hex", "xl6_xrt_hkdf_sha256_hex", xl6StdRuntimeCryptoHkdfParams, 7, "point", NULL},
    {"__xl_runtime", "hkdfSha384Hex", "__xl_std_runtime_hkdf_sha384_hex", "xl6_xrt_hkdf_sha384_hex", xl6StdRuntimeCryptoHkdfParams, 7, "point", NULL},
    {"__xl_runtime", "randomHex", "__xl_std_runtime_random_hex", "xl6_xrt_random_hex", xl6StdRuntimeCryptoRandomParams, 1, "point", NULL},
    {"__xl_runtime", "ed25519Keypair", "__xl_std_runtime_ed25519_keypair", "xl6_xrt_ed25519_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "ed25519PublicKeyHex", "__xl_std_runtime_ed25519_public_key_hex", "xl6_xrt_ed25519_public_key_hex", xl6StdRuntimeCryptoTextHexParams, 2, "point", NULL},
    {"__xl_runtime", "ed25519SignHex", "__xl_std_runtime_ed25519_sign_hex", "xl6_xrt_ed25519_sign_hex", xl6StdRuntimeCryptoSignParams, 4, "point", NULL},
    {"__xl_runtime", "ed25519Verify", "__xl_std_runtime_ed25519_verify", "xl6_xrt_ed25519_verify", xl6StdRuntimeCryptoVerifyParams, 6, "c_bool", NULL},
    {"__xl_runtime", "x25519Keypair", "__xl_std_runtime_x25519_keypair", "xl6_xrt_x25519_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "x25519SharedSecretHex", "__xl_std_runtime_x25519_shared_secret_hex", "xl6_xrt_x25519_shared_secret_hex", xl6StdRuntimeCryptoSharedSecretParams, 4, "point", NULL},
    {"__xl_runtime", "x448Keypair", "__xl_std_runtime_x448_keypair", "xl6_xrt_x448_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "x448SharedSecretHex", "__xl_std_runtime_x448_shared_secret_hex", "xl6_xrt_x448_shared_secret_hex", xl6StdRuntimeCryptoSharedSecretParams, 4, "point", NULL},
    {"__xl_runtime", "rsaPssVerify", "__xl_std_runtime_rsa_pss_verify", "xl6_xrt_rsa_pss_verify", xl6StdRuntimeCryptoRsaVerifyParams, 8, "c_bool", NULL},
    {"__xl_runtime", "rsaPkcs1Verify", "__xl_std_runtime_rsa_pkcs1_verify", "xl6_xrt_rsa_pkcs1_verify", xl6StdRuntimeCryptoRsaVerifyParams, 8, "c_bool", NULL},
    {"__xl_runtime", "urlPart", "__xl_std_runtime_url_part", "xl6_xrt_url_part", xl6StdRuntimeUrlPartParams, 3, "point", NULL},
    {"__xl_runtime", "urlFlag", "__xl_std_runtime_url_flag", "xl6_xrt_url_flag", xl6StdRuntimeUrlPartParams, 3, "c_bool", NULL},
    {"__xl_runtime", "urlPort", "__xl_std_runtime_url_port", "xl6_xrt_url_port", xl6StdRuntimeUrlTextParams, 2, "c_int", NULL},
    {"__xl_runtime", "urlResolve", "__xl_std_runtime_url_resolve", "xl6_xrt_url_resolve", xl6StdRuntimeUrlResolveParams, 4, "point", NULL},
    {"__xl_runtime", "urlNormalizePath", "__xl_std_runtime_url_normalize_path", "xl6_xrt_url_normalize_path", xl6StdRuntimeUrlTextParams, 2, "point", NULL},
    {"__xl_runtime", "queryEncode", "__xl_std_runtime_query_encode", "xl6_xrt_query_encode", xl6StdRuntimeQueryTextParams, 2, "point", NULL},
    {"__xl_runtime", "queryDecode", "__xl_std_runtime_query_decode", "xl6_xrt_query_decode", xl6StdRuntimeQueryTextParams, 2, "point", NULL},
    {"__xl_runtime", "queryValid", "__xl_std_runtime_query_valid", "xl6_xrt_query_valid", xl6StdRuntimeQueryTextParams, 2, "c_bool", NULL},
    {"__xl_runtime", "queryPair", "__xl_std_runtime_query_pair", "xl6_xrt_query_pair", xl6StdRuntimeQueryPairParams, 4, "point", NULL},
    {"__xl_runtime", "queryAppend", "__xl_std_runtime_query_append", "xl6_xrt_query_append", xl6StdRuntimeQueryAppendParams, 6, "point", NULL},
    {"__xl_runtime", "regexIsMatch", "__xl_std_runtime_regex_is_match", "xl6_xrt_regex_is_match", xl6StdRuntimeRegexTextParams, 4, "c_bool", NULL},
    {"__xl_runtime", "regexFullMatch", "__xl_std_runtime_regex_full_match", "xl6_xrt_regex_full_match", xl6StdRuntimeRegexTextParams, 4, "c_bool", NULL},
    {"__xl_runtime", "regexFind", "__xl_std_runtime_regex_find", "xl6_xrt_regex_find", xl6StdRuntimeRegexTextParams, 4, "c_ssize", NULL},
    {"__xl_runtime", "regexMatch", "__xl_std_runtime_regex_match", "xl6_xrt_regex_match", xl6StdRuntimeRegexMatchParams, 5, "point", NULL},
    {"__xl_runtime", "regexSetCreate", "__xl_std_runtime_regex_set_create", "xl6_xrt_regex_set_create", xl6StdRuntimeRegexSetCreateParams, 2, "point", NULL},
    {"__xl_runtime", "processExec", "__xl_std_runtime_process_exec", "xl6_xrt_process_exec", xl6StdRuntimeProcessExecParams, 2, "point", NULL},
    {"__xl_runtime", "processSuccess", "__xl_std_runtime_process_success", "xl6_xrt_process_success", xl6StdRuntimeProcessExecParams, 2, "c_bool", NULL},
    {"__xl_runtime", "processExitCode", "__xl_std_runtime_process_exit_code", "xl6_xrt_process_exit_code", xl6StdRuntimeProcessExecParams, 2, "c_int", NULL},
    {"__xl_runtime", "processRun", "__xl_std_runtime_process_run", "xl6_xrt_process_run_shell_result", xl6StdRuntimeProcessExecParams, 2, "point", NULL},
    {"__xl_runtime", "processRunFile", "__xl_std_runtime_process_run_file", "xl6_xrt_process_run_file_result", xl6StdRuntimeProcessRunFileParams, 3, "point", NULL},
    {"__xl_runtime", "processPipeline", "__xl_std_runtime_process_pipeline", "xl6_xrt_process_pipeline_run", xl6StdRuntimeProcessPipelineParams, 2, "point", NULL},
    {"__xl_runtime", "logCreate", "__xl_std_runtime_log_create", "xl6_xrt_log_create", xl6StdRuntimeLogCreateParams, 1, "point", NULL},
    {"__xl_runtime", "logDefault", "__xl_std_runtime_log_default", "xl6_xrt_log_default", NULL, 0, "point", NULL},
    {"__xl_runtime", "logDefaultSetLevel", "__xl_std_runtime_log_default_set_level", "xl6_xrt_log_default_set_level", xl6StdRuntimeLogLevelParams, 1, "", NULL},
    {"__xl_runtime", "logDefaultGetLevel", "__xl_std_runtime_log_default_get_level", "xl6_xrt_log_default_get_level", NULL, 0, "int", NULL},
    {"__xl_runtime", "logDefaultAddConsole", "__xl_std_runtime_log_default_add_console", "xl6_xrt_log_default_add_console", xl6StdRuntimeLogDefaultAddConsoleParams, 3, "bool", NULL},
    {"__xl_runtime", "logDefaultAddFile", "__xl_std_runtime_log_default_add_file", "xl6_xrt_log_default_add_file", xl6StdRuntimeLogDefaultAddFileParams, 3, "bool", NULL},
    {"__xl_runtime", "logDefaultAddRollingFile", "__xl_std_runtime_log_default_add_rolling_file", "xl6_xrt_log_default_add_rolling_file", xl6StdRuntimeLogDefaultAddRollingFileParams, 5, "bool", NULL},
    {"__xl_runtime", "logDefaultWrite", "__xl_std_runtime_log_default_write", "xl6_xrt_log_default_write", xl6StdRuntimeLogWriteParams, 2, "", NULL},
    {"__xl_runtime", "logDefaultFlush", "__xl_std_runtime_log_default_flush", "xl6_xrt_log_default_flush", NULL, 0, "", NULL},
    {"__xl_runtime", "logLevelName", "__xl_std_runtime_log_level_name", "xl6_xrt_log_level_name", xl6StdRuntimeLogLevelParams, 1, "point", NULL},
    {"__xl_runtime", "encodingCharSize", "__xl_std_runtime_encoding_char_size", "xl6_xrt_encoding_char_size", xl6StdRuntimeEncodingCharsetParams, 1, "int", NULL},
    {"__xl_runtime", "encodingIsUtf8Text", "__xl_std_runtime_encoding_is_utf8_text", "xl6_xrt_encoding_is_utf8_text", xl6StdRuntimeEncodingTextParams, 1, "bool", NULL},
    {"__xl_runtime", "encodingIsUtf8Bytes", "__xl_std_runtime_encoding_is_utf8_bytes", "xl6_xrt_encoding_is_utf8_bytes", xl6StdRuntimeEncodingBytesParams, 2, "bool", NULL},
    {"__xl_runtime", "encodingDetectText", "__xl_std_runtime_encoding_detect_text", "xl6_xrt_encoding_detect_text", xl6StdRuntimeEncodingTextDetectParams, 2, "int", NULL},
    {"__xl_runtime", "encodingDetectBytes", "__xl_std_runtime_encoding_detect_bytes", "xl6_xrt_encoding_detect_bytes", xl6StdRuntimeEncodingBytesDetectParams, 3, "int", NULL},
    {"__xl_runtime", "encodingToBytes", "__xl_std_runtime_encoding_to_bytes", "xl6_xrt_encoding_to_bytes", xl6StdRuntimeEncodingToBytesParams, 2, "point", NULL},
    {"__xl_runtime", "encodingFromBytes", "__xl_std_runtime_encoding_from_bytes", "xl6_xrt_encoding_from_bytes", xl6StdRuntimeEncodingFromBytesParams, 3, "point", NULL},
    {"__xl_runtime", "encodingConvertBytes", "__xl_std_runtime_encoding_convert_bytes", "xl6_xrt_encoding_convert_bytes", xl6StdRuntimeEncodingConvertBytesParams, 4, "point", NULL},
    {"__xl_runtime", "encodingName", "__xl_std_runtime_encoding_name", "xl6_xrt_encoding_name", xl6StdRuntimeEncodingCharsetParams, 1, "point", NULL},
    {"__xl_runtime", "dirList", "__xl_std_runtime_dir_list", "xl6_xrt_dir_list", xl6StdRuntimeDirListParams, 3, "var", NULL},
    {"__xl_runtime", "dirGlob", "__xl_std_runtime_dir_glob", "xl6_xrt_dir_glob", xl6StdRuntimeDirGlobParams, 4, "var", NULL},
    {"__xl_runtime", "dirEntries", "__xl_std_runtime_dir_entries", "xl6_xrt_dir_entries", xl6StdRuntimeDirEntriesParams, 5, "var", NULL},
    {"__xl_runtime", "pathJoin", "__xl_std_runtime_path_join", "xl6_xrt_path_join", xl6StdRuntimePathJoinParams, 4, "point", NULL},
    {"__xl_runtime", "httpServerCreate", "__xl_std_runtime_http_server_create", "xl6_xrt_http_server_create", xl6StdRuntimeHttpServerCreateParams, 14, "point", NULL},
    {"__xl_runtime", "httpServerCreateTls", "__xl_std_runtime_http_server_create_tls", "xl6_xrt_http_server_create_tls", xl6StdRuntimeHttpServerCreateTlsParams, 16, "point", NULL},
    {"__xl_runtime", "httpAppCreate", "__xl_std_runtime_http_app_create", "xl6_xrt_http_app_create", NULL, 0, "point", NULL},
    {"__xl_runtime", "httpAppDestroy", "__xl_std_runtime_http_app_destroy", "xl6_xrt_http_app_destroy", xl6StdRuntimeHttpAppHandleParams, 1, "", NULL},
    {"__xl_runtime", "httpAppRoute", "__xl_std_runtime_http_app_route", "xl6_xrt_http_app_route", xl6StdRuntimeHttpAppGenericRouteParams, 4, "c_bool", NULL},
    {"__xl_runtime", "httpAppRouteRequest", "__xl_std_runtime_http_app_route_request", "xl6_xrt_http_app_route_request", xl6StdRuntimeHttpAppGenericRequestRouteParams, 4, "c_bool", NULL},
    {"__xl_runtime", "httpAppGet", "__xl_std_runtime_http_app_get", "xl6_xrt_http_app_get", xl6StdRuntimeHttpAppRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppPost", "__xl_std_runtime_http_app_post", "xl6_xrt_http_app_post", xl6StdRuntimeHttpAppRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppAny", "__xl_std_runtime_http_app_any", "xl6_xrt_http_app_any", xl6StdRuntimeHttpAppRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppGetRequest", "__xl_std_runtime_http_app_get_request", "xl6_xrt_http_app_get_request", xl6StdRuntimeHttpAppRequestRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppPostRequest", "__xl_std_runtime_http_app_post_request", "xl6_xrt_http_app_post_request", xl6StdRuntimeHttpAppRequestRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppAnyRequest", "__xl_std_runtime_http_app_any_request", "xl6_xrt_http_app_any_request", xl6StdRuntimeHttpAppRequestRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppRouteStreamRequest", "__xl_std_runtime_http_app_route_stream_request", "xl6_xrt_http_app_route_stream_request", xl6StdRuntimeHttpAppGenericStreamRouteParams, 6, "c_bool", NULL},
    {"__xl_runtime", "httpAppPostStreamRequest", "__xl_std_runtime_http_app_post_stream_request", "xl6_xrt_http_app_post_stream_request", xl6StdRuntimeHttpAppStreamRouteParams, 5, "c_bool", NULL},
    {"__xl_runtime", "httpAppAnyStreamRequest", "__xl_std_runtime_http_app_any_stream_request", "xl6_xrt_http_app_any_stream_request", xl6StdRuntimeHttpAppStreamRouteParams, 5, "c_bool", NULL},
    {"__xl_runtime", "httpAppStatic", "__xl_std_runtime_http_app_static", "xl6_xrt_http_app_static", xl6StdRuntimeHttpAppStaticParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpAppStaticEx", "__xl_std_runtime_http_app_static_ex", "xl6_xrt_http_app_static_ex", xl6StdRuntimeHttpAppStaticExParams, 7, "c_bool", NULL},
    {"__xl_runtime", "httpAppError", "__xl_std_runtime_http_app_error", "xl6_xrt_http_app_error", xl6StdRuntimeHttpAppErrorParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerSetApp", "__xl_std_runtime_http_server_set_app", "xl6_xrt_http_server_set_app", xl6StdRuntimeHttpServerAppParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerReloadApp", "__xl_std_runtime_http_server_reload_app", "xl6_xrt_http_server_reload_app", xl6StdRuntimeHttpServerAppParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerHost", "__xl_std_runtime_http_server_host", "xl6_xrt_http_server_host", xl6StdRuntimeHttpServerHostParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerRemoveHost", "__xl_std_runtime_http_server_remove_host", "xl6_xrt_http_server_remove_host", xl6StdRuntimeHttpServerRemoveHostParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerTlsHost", "__xl_std_runtime_http_server_tls_host", "xl6_xrt_http_server_tls_host", xl6StdRuntimeHttpServerTlsHostParams, 4, "c_bool", NULL},
    {"__xl_runtime", "httpServerRemoveTlsHost", "__xl_std_runtime_http_server_remove_tls_host", "xl6_xrt_http_server_remove_tls_host", xl6StdRuntimeHttpServerRemoveTlsHostParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerError", "__xl_std_runtime_http_server_error", "xl6_xrt_http_server_error", xl6StdRuntimeHttpServerErrorParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerRoute", "__xl_std_runtime_http_server_route", "xl6_xrt_http_server_route", xl6StdRuntimeHttpServerGenericRouteParams, 4, "c_bool", NULL},
    {"__xl_runtime", "httpServerRouteRequest", "__xl_std_runtime_http_server_route_request", "xl6_xrt_http_server_route_request", xl6StdRuntimeHttpServerGenericRequestRouteParams, 4, "c_bool", NULL},
    {"__xl_runtime", "httpServerGet", "__xl_std_runtime_http_server_get", "xl6_xrt_http_server_get", xl6StdRuntimeHttpServerRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerPost", "__xl_std_runtime_http_server_post", "xl6_xrt_http_server_post", xl6StdRuntimeHttpServerRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerAny", "__xl_std_runtime_http_server_any", "xl6_xrt_http_server_any", xl6StdRuntimeHttpServerRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerGetRequest", "__xl_std_runtime_http_server_get_request", "xl6_xrt_http_server_get_request", xl6StdRuntimeHttpServerRequestRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerPostRequest", "__xl_std_runtime_http_server_post_request", "xl6_xrt_http_server_post_request", xl6StdRuntimeHttpServerRequestRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerAnyRequest", "__xl_std_runtime_http_server_any_request", "xl6_xrt_http_server_any_request", xl6StdRuntimeHttpServerRequestRouteParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerRouteStreamRequest", "__xl_std_runtime_http_server_route_stream_request", "xl6_xrt_http_server_route_stream_request", xl6StdRuntimeHttpServerGenericStreamRouteParams, 6, "c_bool", NULL},
    {"__xl_runtime", "httpServerPostStreamRequest", "__xl_std_runtime_http_server_post_stream_request", "xl6_xrt_http_server_post_stream_request", xl6StdRuntimeHttpServerStreamRouteParams, 5, "c_bool", NULL},
    {"__xl_runtime", "httpServerAnyStreamRequest", "__xl_std_runtime_http_server_any_stream_request", "xl6_xrt_http_server_any_stream_request", xl6StdRuntimeHttpServerStreamRouteParams, 5, "c_bool", NULL},
    {"__xl_runtime", "httpServerStatic", "__xl_std_runtime_http_server_static", "xl6_xrt_http_server_static", xl6StdRuntimeHttpServerStaticParams, 3, "c_bool", NULL},
    {"__xl_runtime", "httpServerStaticEx", "__xl_std_runtime_http_server_static_ex", "xl6_xrt_http_server_static_ex", xl6StdRuntimeHttpServerStaticExParams, 7, "c_bool", NULL},
    {"__xl_runtime", "httpServerStart", "__xl_std_runtime_http_server_start", "xl6_xrt_http_server_start", xl6StdRuntimeHttpServerHandleParams, 1, "c_bool", NULL},
    {"__xl_runtime", "httpServerPort", "__xl_std_runtime_http_server_port", "xl6_xrt_http_server_port", xl6StdRuntimeHttpServerHandleParams, 1, "c_int", NULL},
    {"__xl_runtime", "httpServerRequestCount", "__xl_std_runtime_http_server_request_count", "xl6_xrt_http_server_request_count", xl6StdRuntimeHttpServerHandleParams, 1, "c_int", NULL},
    {"__xl_runtime", "httpServerConnectionCount", "__xl_std_runtime_http_server_connection_count", "xl6_xrt_http_server_connection_count", xl6StdRuntimeHttpServerHandleParams, 1, "c_int", NULL},
    {"__xl_runtime", "httpServerStop", "__xl_std_runtime_http_server_stop", "xl6_xrt_http_server_stop", xl6StdRuntimeHttpServerHandleParams, 1, "", NULL},
    {"__xl_runtime", "httpServerDrain", "__xl_std_runtime_http_server_drain", "xl6_xrt_http_server_drain", xl6StdRuntimeHttpServerDrainParams, 2, "c_bool", NULL},
    {"__xl_runtime", "httpServerDestroy", "__xl_std_runtime_http_server_destroy", "xl6_xrt_http_server_destroy", xl6StdRuntimeHttpServerHandleParams, 1, "", NULL},
    {"__xl_runtime", "httpMimeByPath", "__xl_std_runtime_http_mime_by_path", "xl6_xrt_http_mime_by_path", xl6StdRuntimeHttpMimeByPathParams, 1, "point", NULL},
    {"__xl_runtime", "httpBodyChunkText", "__xl_std_runtime_http_body_chunk_text", "xl6_xrt_http_body_chunk_text", xl6StdRuntimeHttpBodyChunkParams, 2, "point", NULL},
    {"__xl_runtime", "httpBodyChunkBytes", "__xl_std_runtime_http_body_chunk_bytes", "xl6_xrt_http_body_chunk_bytes", xl6StdRuntimeHttpBodyChunkParams, 2, "point", NULL},
    {"tcc", "Context", "__xl_std_tcc_context", NULL, NULL, 0, "TccContext", xl6StdTccContextBody},
    {"tcc", "run", "__xl_std_tcc_run", NULL, NULL, 0, "int", xl6StdTccRunBody},
    {"tcc", "runFile", "__xl_std_tcc_run_file", NULL, NULL, 0, "int", xl6StdTccRunFileBody},
    {"tcc", "compile", "__xl_std_tcc_compile", NULL, NULL, 0, "int", xl6StdTccCompileBody},
    {"tcc", "compileFile", "__xl_std_tcc_compile_file", NULL, NULL, 0, "int", xl6StdTccCompileFileBody},
    {"tcc", "vfsMount", "__xl_std_tcc_vfs_mount", NULL, NULL, 0, "bool", xl6StdTccVfsMountBody},
    {"tcc", "vfsClear", "__xl_std_tcc_vfs_clear", NULL, NULL, 0, "", xl6StdTccVfsClearBody},
    {"xlang", "Context", "__xl_std_xlang_context", NULL, NULL, 0, "XLangContext", xl6StdXlangContextBody},
    {"xlang", "run", "__xl_std_xlang_run", NULL, NULL, 0, "bool", xl6StdXlangRunBody},
    {"xlang", "runFile", "__xl_std_xlang_run_file", NULL, NULL, 0, "bool", xl6StdXlangRunFileBody},

    {"__xrt_string", "strComp", "__xl_std_xrt_str_comp", "xrtStrComp", xl6StdXrtStrCompParams, 4, "c_int", NULL},
    {"__xrt_string", "strByteLen", "__xl_std_xrt_str_byte_len", "xrtStrByteLen", xl6StdXrtStringSizeParams, 2, "c_size", NULL},
    {"__xrt_string", "strCharLen", "__xl_std_xrt_str_char_len", "xrtStrCharLen", xl6StdXrtStringSizeParams, 2, "c_size", NULL},
    {"__xrt_string", "inStr", "__xl_std_xrt_in_str", "xrtInStr", xl6StdXrtInStrParams, 5, "c_uint", NULL},
    {"__xrt_string", "strIndexOf", "__xl_std_xrt_str_index_of", "xrtStrIndexOf", xl6StdXrtStringFindParams, 5, "int", NULL},
    {"__xrt_string", "strLastIndexOf", "__xl_std_xrt_str_last_index_of", "xrtStrLastIndexOf", xl6StdXrtStringFindParams, 5, "int", NULL},
    {"__xrt_string", "strCount", "__xl_std_xrt_str_count", "xrtStrCount", xl6StdXrtStringFindParams, 5, "int", NULL},
    {"__xrt_string", "checkStr", "__xl_std_xrt_check_str", "xrtCheckStr", xl6StdXrtCheckStrParams, 4, "point", NULL},
    {"__xrt_string", "strLike", "__xl_std_xrt_str_like", "xrtStrLike", xl6StdXrtStrLikeParams, 5, "c_bool", NULL},
    {"__xrt_string", "strApprox", "__xl_std_xrt_str_approx", "xrtStrApprox", xl6StdXrtStrApproxParams, 4, "c_bool", NULL},
    {"__xrt_string", "strSim", "__xl_std_xrt_str_sim", "xrtStrSim", xl6StdXrtStrApproxParams, 4, "c_double", NULL},
    {"__xrt_string", "isUtf8", "__xl_std_xrt_is_utf8", "xrtIsUTF8", xl6StdXrtIsUtf8Params, 2, "c_bool", NULL},
    {"__xrt_string", "intFormat", "__xl_std_xrt_int_format", "xrtIntFormat", xl6StdXrtIntFormatParams, 2, "point", NULL},
    {"__xrt_string", "numFormat", "__xl_std_xrt_num_format", "xrtNumFormat", xl6StdXrtNumFormatParams, 2, "point", NULL},
    {"__xrt_string", "lcase", "__xl_std_xrt_lcase", "xrtLCase", xl6StdXrtStringCaseParams, 3, "point", NULL},
    {"__xrt_string", "ucase", "__xl_std_xrt_ucase", "xrtUCase", xl6StdXrtStringCaseParams, 3, "point", NULL},
    {"__xrt_string", "trim", "__xl_std_xrt_trim", "xrtTrim", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "ltrim", "__xl_std_xrt_ltrim", "xrtLTrim", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "rtrim", "__xl_std_xrt_rtrim", "xrtRTrim", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "filter", "__xl_std_xrt_filter", "xrtFilterStr", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "replace", "__xl_std_xrt_replace", "xrtReplace", xl6StdXrtStringReplaceParams, 7, "point", NULL},
    {"__xrt_string", "strSlice", "__xl_std_xrt_str_slice", "xrtStrSlice", xl6StdXrtStringSliceParams, 5, "point", NULL},
    {"__xrt_string", "strLeft", "__xl_std_xrt_str_left", "xrtStrLeft", xl6StdXrtStringCountParams, 4, "point", NULL},
    {"__xrt_string", "strRight", "__xl_std_xrt_str_right", "xrtStrRight", xl6StdXrtStringCountParams, 4, "point", NULL},
    {"__xrt_string", "strMid", "__xl_std_xrt_str_mid", "xrtStrMid", xl6StdXrtStringSliceParams, 5, "point", NULL},
    {"__xrt_string", "strCharAt", "__xl_std_xrt_str_char_at", "xrtStrCharAt", xl6StdXrtStringIndexParams, 4, "point", NULL},
    {"__xrt_string", "strByteAt", "__xl_std_xrt_str_byte_at", "xrtStrByteAt", xl6StdXrtStringByteAtParams, 3, "c_int", NULL},
    {"__xrt_string", "strRepeat", "__xl_std_xrt_str_repeat", "xrtStrRepeat", xl6StdXrtStringCountParams, 4, "point", NULL},
    {"__xrt_string", "strIsEmpty", "__xl_std_xrt_str_is_empty", "xrtStrIsEmpty", xl6StdXrtStringSizeParams, 2, "c_bool", NULL},
    {"__xrt_string", "strIsBlank", "__xl_std_xrt_str_is_blank", "xrtStrIsBlank", xl6StdXrtStringSizeParams, 2, "c_bool", NULL},
    {"__xrt_string", "strRemove", "__xl_std_xrt_str_remove", "xrtStrRemove", xl6StdXrtStringRemoveParams, 6, "point", NULL},
    {"__xrt_string", "strRemoveAt", "__xl_std_xrt_str_remove_at", "xrtStrRemoveAt", xl6StdXrtStringSliceParams, 5, "point", NULL},
    {"__xrt_string", "strInsert", "__xl_std_xrt_str_insert", "xrtStrInsert", xl6StdXrtStringInsertParams, 6, "point", NULL},
    {"__xrt_string", "strPadLeft", "__xl_std_xrt_str_pad_left", "xrtStrPadLeft", xl6StdXrtStringPadParams, 6, "point", NULL},
    {"__xrt_string", "strPadRight", "__xl_std_xrt_str_pad_right", "xrtStrPadRight", xl6StdXrtStringPadParams, 6, "point", NULL},
    {"__xrt_string", "strPadCenter", "__xl_std_xrt_str_pad_center", "xrtStrPadCenter", xl6StdXrtStringPadParams, 6, "point", NULL},
    {"__xrt_string", "strReverse", "__xl_std_xrt_str_reverse", "xrtStrReverse", xl6StdXrtStringReverseParams, 3, "point", NULL},
    {"__xrt_math", "sign", "__xl_std_xrt_math_sign", "xrtMathSign", xl6StdMathUnaryFloatParams, 1, "c_int", NULL},
    {"__xrt_time", "dateAdd", "__xl_std_xrt_date_add", "xrtDateAdd", xl6StdXrtDateAddParams, 3, "int", NULL},
    {"__xrt_time", "dateDiff", "__xl_std_xrt_date_diff", "xrtDateDiff", xl6StdXrtDateDiffParams, 3, "int", NULL},
    {"__xrt_time", "strToTime", "__xl_std_xrt_str_to_time", "xrtStrToTime", xl6StdXrtStrToTimeParams, 2, "int", NULL},
    {"__xrt_time", "tryStrToTime", "__xl_std_xrt_try_str_to_time", "xrtTryStrToTime", xl6StdXrtTryStrToTimeParams, 3, "c_bool", NULL},
    {"__xrt_time", "timeToStr", "__xl_std_xrt_time_to_str", "xrtTimeToStr", xl6StdXrtTimeToStrParams, 2, "point", NULL},
    {"__xrt_time", "timeFormat", "__xl_std_xrt_time_format", "xrtTimeFormat", xl6StdXrtTimeFormatParams, 2, "point", NULL},
    {"__xrt_time", "timeParse", "__xl_std_xrt_time_parse", "xrtTimeParse", xl6StdXrtTimeParseParams, 2, "int", NULL},
    {"__xrt_time", "tryTimeParse", "__xl_std_xrt_try_time_parse", "xrtTryTimeParse", xl6StdXrtTryTimeParseParams, 3, "c_bool", NULL},
    {"__xrt_time", "relativeTime", "__xl_std_xrt_relative_time", "xrtRelativeTime", xl6StdXrtIntBinaryParams, 2, "point", NULL},
    {"__xrt_time", "dateTimeSerial", "__xl_std_xrt_datetime_serial", "xrtDateTimeSerial", xl6StdTimeFromDateTimeParams, 6, "int", NULL},
    {"__xrt_time", "now", "__xl_std_xrt_now", "xrtNow", NULL, 0, "int", NULL},
    {"__xrt_time", "date", "__xl_std_xrt_date", "xrtDate", NULL, 0, "int", NULL},
    {"__xrt_time", "time", "__xl_std_xrt_time", "xrtTime", NULL, 0, "int", NULL},
    {"__xrt_time", "timeSerial", "__xl_std_xrt_time_serial", "xrtTimeSerial", xl6StdTimeFromTimeParams, 3, "int", NULL},
    {"__xrt_time", "dateSerial", "__xl_std_xrt_date_serial", "xrtDateSerial", xl6StdTimeFromDateParams, 3, "int", NULL},
    {"__xrt_time", "year", "__xl_std_xrt_year", "xrtYear", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "month", "__xl_std_xrt_month", "xrtMonth", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "day", "__xl_std_xrt_day", "xrtDay", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "hour", "__xl_std_xrt_hour", "xrtHour", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "minute", "__xl_std_xrt_minute", "xrtMinute", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "second", "__xl_std_xrt_second", "xrtSecond", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "weekday", "__xl_std_xrt_weekday", "xrtWeekday", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "dayOfYear", "__xl_std_xrt_day_of_year", "xrtDayOfYear", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "quarter", "__xl_std_xrt_quarter", "xrtQuarter", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "isLeapYear", "__xl_std_xrt_is_leap_year", "xrtIsLeapYear", xl6StdXrtYearParams, 1, "c_bool", NULL},
    {"__xrt_time", "daysInMonth", "__xl_std_xrt_days_in_month", "xrtDaysInMonth", xl6StdXrtYearMonthParams, 2, "c_int", NULL},
    {"__xrt_time", "daysInYear", "__xl_std_xrt_days_in_year", "xrtDaysInYear", xl6StdXrtYearParams, 1, "c_int", NULL},
    {"__xrt_time", "datePart", "__xl_std_xrt_date_part", "xrtDatePart", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "timePart", "__xl_std_xrt_time_part", "xrtTimePart", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "isSameDay", "__xl_std_xrt_is_same_day", "xrtIsSameDay", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "isSameMonth", "__xl_std_xrt_is_same_month", "xrtIsSameMonth", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "isSameYear", "__xl_std_xrt_is_same_year", "xrtIsSameYear", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "inRange", "__xl_std_xrt_time_in_range", "xrtTimeInRange", xl6StdXrtIntInRangeParams, 3, "c_bool", NULL},
    {"__xrt_time", "timer", "__xl_std_xrt_timer", "xrtTimer", NULL, 0, "c_double", NULL},
    {"__xrt_time", "sleep", "__xl_std_xrt_sleep", "xrtSleep", xl6StdXrtSleepParams, 1, "", NULL},
    {"__xrt_time", "nowUTC", "__xl_std_xrt_now_utc", "xrtNowUTC", NULL, 0, "int", NULL},
    {"__xrt_time", "timezoneOffset", "__xl_std_xrt_timezone_offset", "xrtTimezoneOffset", NULL, 0, "c_int", NULL},
    {"__xrt_time", "rangeOverlap", "__xl_std_xrt_time_range_overlap", "xrtTimeRangeOverlap", xl6StdXrtTimeOverlapParams, 4, "c_bool", NULL},
    {"__xrt_time", "approx", "__xl_std_xrt_time_approx", "xrtTimeApprox", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "toUnix", "__xl_std_xrt_time_to_unix", "xrtToUnixTime", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "fromUnix", "__xl_std_xrt_time_from_unix", "xrtFromUnixTime", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "utcToLocal", "__xl_std_xrt_utc_to_local", "xrtUTCToLocal", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "localToUTC", "__xl_std_xrt_local_to_utc", "xrtLocalToUTC", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfMonth", "__xl_std_xrt_first_day_of_month", "xrtFirstDayOfMonth", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "lastDayOfMonth", "__xl_std_xrt_last_day_of_month", "xrtLastDayOfMonth", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfYear", "__xl_std_xrt_first_day_of_year", "xrtFirstDayOfYear", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "lastDayOfYear", "__xl_std_xrt_last_day_of_year", "xrtLastDayOfYear", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfWeek", "__xl_std_xrt_first_day_of_week", "xrtFirstDayOfWeek", xl6StdXrtWeekBoundaryParams, 2, "int", NULL},
    {"__xrt_time", "lastDayOfWeek", "__xl_std_xrt_last_day_of_week", "xrtLastDayOfWeek", xl6StdXrtWeekBoundaryParams, 2, "int", NULL},
    {"__xrt_time", "weekOfYear", "__xl_std_xrt_week_of_year", "xrtWeekOfYear", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "weekOfMonth", "__xl_std_xrt_week_of_month", "xrtWeekOfMonth", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_random", "rand32", "__xl_std_xrt_rand32", "xrtRand32", NULL, 0, "c_uint", NULL},
    {"__xrt_random", "rand64", "__xl_std_xrt_rand64", "xrtRand64", NULL, 0, "c_ulonglong", NULL},
    {"__xrt_random", "range", "__xl_std_xrt_rand_range", "xrtRandRange", xl6StdXrtRandRangeParams, 2, "c_int", NULL},
    {"__xrt_random", "seedThread", "__xl_std_xrt_rand_seed_thread", "xrtRandSeedThread", xl6StdXrtRandSeedParams, 2, "", NULL},
    {"__xrt_random", "string", "__xl_std_xrt_rand_str", "xrtRandStr", xl6StdXrtRandStrParams, 3, "point", NULL},
    {"__xrt_path", "exists", "__xl_std_xrt_path_exists", "xrtPathExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "isAbs", "__xl_std_xrt_path_is_abs", "xrtPathIsAbs", xl6StdXrtPathIsAbsParams, 2, "c_bool", NULL},
    {"__xrt_path", "getNameExt", "__xl_std_xrt_path_get_name_ext", "xrtPathGetNameExt", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "getName", "__xl_std_xrt_path_get_name", "xrtPathGetName", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "getExt", "__xl_std_xrt_path_get_ext", "xrtPathGetExt", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "getDir", "__xl_std_xrt_path_get_dir", "xrtPathGetDir", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "random", "__xl_std_xrt_path_random", "xrtPathRandom", xl6StdXrtPathRandomParams, 5, "point", NULL},
    {"__xrt_path", "sep", "__xl_std_xrt_path_sep", "xrtPathSep", NULL, 0, "point", NULL},
    {"__xrt_path", "normalize", "__xl_std_xrt_path_normalize", "xrtPathNormalize", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "abs", "__xl_std_xrt_path_abs", "xrtPathAbs", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "rel", "__xl_std_xrt_path_rel", "xrtPathRel", xl6StdPathRelParams, 2, "point", NULL},
    {"__xrt_path", "cwd", "__xl_std_xrt_path_cwd", "xrtPathCwd", NULL, 0, "point", NULL},
    {"__xrt_path", "setCwd", "__xl_std_xrt_path_set_cwd", "xrtPathSetCwd", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "home", "__xl_std_xrt_path_home", "xrtPathHome", NULL, 0, "point", NULL},
    {"__xrt_path", "temp", "__xl_std_xrt_path_temp", "xrtPathTemp", NULL, 0, "point", NULL},
    {"__xrt_path", "appDir", "__xl_std_xrt_path_app_dir", "xrtPathAppDir", NULL, 0, "point", NULL},
    {"__xrt_path", "withExt", "__xl_std_xrt_path_with_ext", "xrtPathWithExt", xl6StdPathWithExtParams, 2, "point", NULL},
    {"__xrt_path", "withName", "__xl_std_xrt_path_with_name", "xrtPathWithName", xl6StdPathWithNameParams, 2, "point", NULL},
    {"__xrt_file", "exists", "__xl_std_xrt_file_exists", "xrtFileExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "getSize", "__xl_std_xrt_file_get_size", "xrtFileGetSize", xl6StdPathUnaryParams, 1, "c_size", NULL},
    {"__xrt_file", "setSize", "__xl_std_xrt_file_set_size", "xrtFileSetSize", xl6StdXrtFileSetSizeParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAttr", "__xl_std_xrt_file_get_attr", "xrtFileGetAttr", xl6StdPathUnaryParams, 1, "c_int", NULL},
    {"__xrt_file", "setAttr", "__xl_std_xrt_file_set_attr", "xrtFileSetAttr", xl6StdXrtFileSetAttrParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAccessTime", "__xl_std_xrt_file_get_access_time", "xrtFileGetAccessTime", xl6StdPathUnaryParams, 1, "int", NULL},
    {"__xrt_file", "getChangeTime", "__xl_std_xrt_file_get_change_time", "xrtFileGetChangeTime", xl6StdPathUnaryParams, 1, "int", NULL},
    {"__xrt_file", "writeAll", "__xl_std_xrt_file_write_all", "xrtFileWriteAll", xl6StdXrtFileWriteTextParams, 4, "c_int", NULL},
    {"__xrt_file", "append", "__xl_std_xrt_file_append", "xrtFileAppend", xl6StdXrtFileWriteTextParams, 4, "c_int", NULL},
    {"__xrt_file", "readAll", "__xl_std_xrt_file_read_all", "xrtFileReadAll", xl6StdXrtFileReadTextParams, 3, "point", NULL},
    {"__xrt_file", "delete", "__xl_std_xrt_file_delete", "xrtFileDelete", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "copy", "__xl_std_xrt_file_copy", "xrtFileCopy", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "move", "__xl_std_xrt_file_move", "xrtFileMove", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "rename", "__xl_std_xrt_file_rename", "xrtFileRename", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "replace", "__xl_std_xrt_file_replace", "xrtFileReplace", xl6StdFileReplaceParams, 2, "c_bool", NULL},
    {"__xrt_file", "touch", "__xl_std_xrt_file_touch", "xrtFileTouch", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "writeAllAtomic", "__xl_std_xrt_file_write_all_atomic", "xrtFileWriteAllAtomic", xl6StdXrtFileWriteTextParams, 4, "c_bool", NULL},
    {"__xrt_dir", "exists", "__xl_std_xrt_dir_exists", "xrtDirExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "create", "__xl_std_xrt_dir_create", "xrtDirCreate", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "createAll", "__xl_std_xrt_dir_create_all", "xrtDirCreateAll", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "copy", "__xl_std_xrt_dir_copy", "xrtDirCopy", xl6StdXrtFileCopyMoveParams, 3, "c_int", NULL},
    {"__xrt_dir", "move", "__xl_std_xrt_dir_move", "xrtDirMove", xl6StdXrtFileCopyMoveParams, 3, "c_int", NULL},
    {"__xrt_dir", "delete", "__xl_std_xrt_dir_delete", "xrtDirDelete", xl6StdPathUnaryParams, 1, "c_int", NULL},
    {"__xrt_dir", "isEmpty", "__xl_std_xrt_dir_is_empty", "xrtDirIsEmpty", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "count", "__xl_std_xrt_dir_count", "xrtDirCount", xl6StdXrtDirCountParams, 3, "int", NULL},
    {"__xrt_dir", "size", "__xl_std_xrt_dir_size", "xrtDirSize", xl6StdXrtDirSizeParams, 2, "int", NULL},
    {"__xrt_dir", "clean", "__xl_std_xrt_dir_clean", "xrtDirClean", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "ensureEmpty", "__xl_std_xrt_dir_ensure_empty", "xrtDirEnsureEmpty", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_json", "parse", "__xl_std_xrt_json_parse", "xrtParseJSON", xl6StdXrtJsonParseParams, 2, "var", NULL},
    {"__xrt_json", "valid", "__xl_std_xrt_json_valid", "xrtJsonValid", xl6StdXrtJsonParseParams, 2, "c_bool", NULL},
    {"__xrt_json", "parseFile", "__xl_std_xrt_json_parse_file", "xrtParseJSON_File", xl6StdPathUnaryParams, 1, "var", NULL},
    {"__xrt_json", "stringify", "__xl_std_xrt_json_stringify", "xrtStringifyJSON", xl6StdXrtJsonStringifyParams, 3, "point", NULL},
    {"__xrt_json", "writeFile", "__xl_std_xrt_json_write_file", "xrtStringifyJSON_File", xl6StdXrtJsonWriteParams, 3, "c_int", NULL},
    {"__xrt_xson", "parse", "__xl_std_xrt_xson_parse", "xrtParseXSON", xl6StdXrtJsonParseParams, 2, "var", NULL},
    {"__xrt_xson", "valid", "__xl_std_xrt_xson_valid", "xrtXsonValid", xl6StdXrtXsonValidParams, 3, "c_bool", NULL},
    {"__xrt_xson", "parseFile", "__xl_std_xrt_xson_parse_file", "xrtParseXSON_File", xl6StdPathUnaryParams, 1, "var", NULL},
    {"__xrt_xson", "stringify", "__xl_std_xrt_xson_stringify", "xrtStringifyXSON", xl6StdXrtXsonStringifyParams, 4, "point", NULL},
    {"__xrt_xson", "writeFile", "__xl_std_xrt_xson_write_file", "xrtStringifyXSON_File", xl6StdXrtXsonWriteParams, 4, "c_int", NULL},
    {"__xrt_codec", "hexEncode", "__xl_std_xrt_hex_encode", "xrtHexEncode", xl6StdXrtCodecTextParams, 2, "point", NULL},
    {"__xrt_codec", "hexDecode", "__xl_std_xrt_hex_decode", "xrtHexDecode", xl6StdXrtCodecTextParams, 2, "point", NULL},
    {"__xrt_codec", "base64Encode", "__xl_std_xrt_base64_encode", "xrtBase64Encode", xl6StdXrtCodecBase64Params, 3, "point", NULL},
    {"__xrt_codec", "base64Decode", "__xl_std_xrt_base64_decode", "xrtBase64Decode", xl6StdXrtCodecBase64Params, 3, "point", NULL},
    {"__xrt_codec", "hexEncodeBytes", "__xl_std_xrt_hex_encode_bytes", "xrtHexEncodeBytes", xl6StdXrtCodecBytesParams, 2, "point", NULL},
    {"__xrt_codec", "base64EncodeBytes", "__xl_std_xrt_base64_encode_bytes", "xrtBase64EncodeBytes", xl6StdXrtCodecBase64BytesParams, 3, "point", NULL},
    {"__xrt_codec", "bufferFromHex", "__xl_std_xrt_buffer_from_hex", "xrtBufferFromHex", xl6StdXrtCodecDecodeBufferParams, 2, "point", NULL},
    {"__xrt_codec", "bufferFromBase64", "__xl_std_xrt_buffer_from_base64", "xrtBufferFromBase64", xl6StdXrtCodecBase64DecodeBufferParams, 3, "point", NULL},
    {"__xrt_codec", "percentEncode", "__xl_std_xrt_percent_encode", "xrtPercentEncode", xl6StdXrtCodecPercentParams, 4, "point", NULL},
    {"__xrt_codec", "percentDecode", "__xl_std_xrt_percent_decode", "xrtPercentDecode", xl6StdXrtCodecPercentParams, 4, "point", NULL},
    {"__xrt_codec", "formUrlEncode", "__xl_std_xrt_form_url_encode", "xrtFormUrlEncode", xl6StdXrtCodecFormUrlParams, 3, "point", NULL},
    {"__xrt_codec", "formUrlDecode", "__xl_std_xrt_form_url_decode", "xrtFormUrlDecode", xl6StdXrtCodecFormUrlParams, 3, "point", NULL},
    {"__xrt_hash", "u32", "__xl_std_xrt_hash32", "xrtHash32", xl6StdXrtHashParams, 2, "c_uint", NULL},
    {"__xrt_hash", "u32WithSeed", "__xl_std_xrt_hash32_seed", "xrtHash32_WithSeed", xl6StdXrtHash32SeedParams, 3, "c_uint", NULL},
    {"__xrt_hash", "u64", "__xl_std_xrt_hash64", "xrtHash64", xl6StdXrtHashParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64WithSeed", "__xl_std_xrt_hash64_seed", "xrtHash64_WithSeed", xl6StdXrtHash64SeedParams, 3, "c_ulonglong", NULL},
    {"__xrt_hash", "u64Micro", "__xl_std_xrt_hash64_micro", "xrtHash64_Micro", xl6StdXrtHashParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64MicroWithSeed", "__xl_std_xrt_hash64_micro_seed", "xrtHash64_Micro_WithSeed", xl6StdXrtHash64SeedParams, 3, "c_ulonglong", NULL},
    {"__xrt_hash", "u64Nano", "__xl_std_xrt_hash64_nano", "xrtHash64_Nano", xl6StdXrtHashParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64NanoWithSeed", "__xl_std_xrt_hash64_nano_seed", "xrtHash64_Nano_WithSeed", xl6StdXrtHash64SeedParams, 3, "c_ulonglong", NULL},
    {"__xrt_id", "makeXIDS", "__xl_std_xrt_make_xids", "xrtMakeXIDS", NULL, 0, "point", NULL},
    {"__xrt_net", "localIP", "__xl_std_xrt_get_local_ip", "xrtGetLocalIP", NULL, 0, "point", NULL},
    {"__xrt_net", "localMAC", "__xl_std_xrt_get_local_mac", "xrtGetLocalMAC", NULL, 0, "point", NULL},
    {"__xrt_net", "localName", "__xl_std_xrt_get_local_name", "xrtGetLocalName", NULL, 0, "point", NULL},
    {"__xrt_tcp", "connect", "__xl_std_xrt_tcp_connect", "xrtNetTcpConnect", xl6StdTcpConnectParams, 3, "point", NULL},
    {"__xrt_tcp", "connectAsync", "__xl_std_xrt_tcp_connect_async", "xrtNetTcpConnectAsync", xl6StdTcpConnectParams, 3, "future<point>", NULL},
    {"__xrt_tcp", "listen", "__xl_std_xrt_tcp_listen", "xrtNetTcpListen", xl6StdTcpListenParams, 3, "point", NULL},
    {"__xrt_tcp", "accept", "__xl_std_xrt_tcp_accept", "xrtNetTcpAccept", xl6StdTcpAcceptParams, 2, "point", NULL},
    {"__xrt_tcp", "listenerDestroy", "__xl_std_xrt_tcp_listener_destroy", "xrtNetTcpListenerDestroy", xl6StdNetHandleParams, 1, "", NULL},
    {"__xrt_tcp", "listenerPort", "__xl_std_xrt_tcp_listener_port", "xrtNetTcpListenerPort", xl6StdNetHandleParams, 1, "uint16", NULL},
    {"__xrt_udp", "bind", "__xl_std_xrt_udp_bind", "xrtNetUdpBind", xl6StdUdpBindParams, 2, "point", NULL},
    {"__xrt_udp", "sendText", "__xl_std_xrt_udp_send_text", "xrtNetUdpSendText", xl6StdUdpSendParams, 4, "c_bool", NULL},
    {"__xrt_udp", "sendTextAsync", "__xl_std_xrt_udp_send_text_async", "xrtNetUdpSendTextAsync", xl6StdUdpSendParams, 4, "future<bool>", NULL},
    {"__xrt_udp", "recv", "__xl_std_xrt_udp_recv", "xrtNetUdpRecv", xl6StdUdpRecvParams, 2, "point", NULL},
    {"__xrt_udp", "recvAsync", "__xl_std_xrt_udp_recv_async", "xrtNetUdpRecvAsync", xl6StdUdpRecvParams, 2, "future<point>", NULL},

    {"", "print", "__xl_std_print", NULL, NULL, 0, "", xl6StdPrintBody},
    {"", "println", "__xl_std_println", NULL, NULL, 0, "", xl6StdPrintlnBody},

    {"math", "sin", "__xl_std_math_sin", "sin", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "cos", "__xl_std_math_cos", "cos", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "tan", "__xl_std_math_tan", "tan", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "asin", "__xl_std_math_asin", "asin", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "acos", "__xl_std_math_acos", "acos", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "atan", "__xl_std_math_atan", "atan", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "atan2", "__xl_std_math_atan2", "atan2", xl6StdMathAtan2Params, 2, "float", NULL},
    {"math", "sqrt", "__xl_std_math_sqrt", "sqrt", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "pow", "__xl_std_math_pow", "pow", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "exp", "__xl_std_math_exp", "exp", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "log", "__xl_std_math_log", "log", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "log10", "__xl_std_math_log10", "log10", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "floor", "__xl_std_math_floor", "floor", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "ceil", "__xl_std_math_ceil", "ceil", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "round", "__xl_std_math_round", NULL, xl6StdMathUnaryFloatParams, 1, "float", xl6StdMathRoundBody},
    {"math", "abs", "__xl_std_math_abs", "fabs", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "min", "__xl_std_math_min", "xrtMathMin", xl6StdMathBinaryFloatParams, 2, "float", NULL},
    {"math", "max", "__xl_std_math_max", "xrtMathMax", xl6StdMathBinaryFloatParams, 2, "float", NULL},
    {"math", "clamp", "__xl_std_math_clamp", "xrtMathClamp", xl6StdMathClampParams, 3, "float", NULL},
    {"math", "sign", "__xl_std_math_sign", NULL, xl6StdMathUnaryFloatParams, 1, "int", xl6StdMathSignBody},
    {"math", "trunc", "__xl_std_math_trunc", "xrtMathTrunc", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "fract", "__xl_std_math_fract", "xrtMathFract", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "mod", "__xl_std_math_mod", "xrtMathMod", xl6StdMathModParams, 2, "float", NULL},
    {"math", "rad", "__xl_std_math_rad", "xrtMathRad", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "deg", "__xl_std_math_deg", "xrtMathDeg", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "isNaN", "__xl_std_math_is_nan", "xrtMathIsNaN", xl6StdMathUnaryFloatParams, 1, "bool", NULL},
    {"math", "isInf", "__xl_std_math_is_inf", "xrtMathIsInf", xl6StdMathUnaryFloatParams, 1, "bool", NULL},
    {"math", "isFinite", "__xl_std_math_is_finite", "xrtMathIsFinite", xl6StdMathUnaryFloatParams, 1, "bool", NULL},
    {"math", "log2", "__xl_std_math_log2", "xrtMathLog2", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "exp2", "__xl_std_math_exp2", "xrtMathExp2", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "log1p", "__xl_std_math_log1p", "xrtMathLog1p", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "expm1", "__xl_std_math_expm1", "xrtMathExpm1", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "cbrt", "__xl_std_math_cbrt", "xrtMathCbrt", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "hypot", "__xl_std_math_hypot", "xrtMathHypot", xl6StdMathPowParams, 2, "float", NULL},

    {"string", "len", "__xl_std_string_len", NULL, NULL, 0, "int", xl6StdStringLenBody},
    {"string", "byteLen", "__xl_std_string_byte_len", NULL, NULL, 0, "int", xl6StdStringByteLenBody},
    {"string", "compare", "__xl_std_string_compare", NULL, NULL, 0, "int", xl6StdStringCompareBody},
    {"string", "equals", "__xl_std_string_equals", NULL, NULL, 0, "bool", xl6StdStringEqualsBody},
    {"string", "contains", "__xl_std_string_contains", NULL, NULL, 0, "bool", xl6StdStringContainsBody},
    {"string", "indexOf", "__xl_std_string_index_of", NULL, NULL, 0, "int", xl6StdStringIndexOfBody},
    {"string", "lastIndexOf", "__xl_std_string_last_index_of", NULL, NULL, 0, "int", xl6StdStringLastIndexOfBody},
    {"string", "count", "__xl_std_string_count", NULL, NULL, 0, "int", xl6StdStringCountBody},
    {"string", "startsWith", "__xl_std_string_starts_with", NULL, NULL, 0, "bool", xl6StdStringStartsWithBody},
    {"string", "endsWith", "__xl_std_string_ends_with", NULL, NULL, 0, "bool", xl6StdStringEndsWithBody},
    {"string", "hasAny", "__xl_std_string_has_any", NULL, NULL, 0, "bool", xl6StdStringHasAnyBody},
    {"string", "like", "__xl_std_string_like", NULL, NULL, 0, "bool", xl6StdStringLikeBody},
    {"string", "approx", "__xl_std_string_approx", NULL, NULL, 0, "bool", xl6StdStringApproxBody},
    {"string", "sim", "__xl_std_string_sim", NULL, NULL, 0, "float", xl6StdStringSimBody},
    {"string", "isUtf8", "__xl_std_string_is_utf8", NULL, NULL, 0, "bool", xl6StdStringIsUtf8Body},
    {"string", "formatInt", "__xl_std_string_format_int", NULL, NULL, 0, "string", xl6StdStringFormatIntBody},
    {"string", "formatFloat", "__xl_std_string_format_float", NULL, NULL, 0, "string", xl6StdStringFormatFloatBody},
    {"string", "lower", "__xl_std_string_lower", NULL, NULL, 0, "string", xl6StdStringLowerBody},
    {"string", "upper", "__xl_std_string_upper", NULL, NULL, 0, "string", xl6StdStringUpperBody},
    {"string", "trim", "__xl_std_string_trim", NULL, NULL, 0, "string", xl6StdStringTrimBody},
    {"string", "ltrim", "__xl_std_string_ltrim", NULL, NULL, 0, "string", xl6StdStringLTrimBody},
    {"string", "rtrim", "__xl_std_string_rtrim", NULL, NULL, 0, "string", xl6StdStringRTrimBody},
    {"string", "filter", "__xl_std_string_filter", NULL, NULL, 0, "string", xl6StdStringFilterBody},
    {"string", "replace", "__xl_std_string_replace", NULL, NULL, 0, "string", xl6StdStringReplaceBody},
    {"string", "split", "__xl_std_string_split", NULL, NULL, 0, "array", xl6StdStringSplitBody},
    {"string", "slice", "__xl_std_string_slice", NULL, NULL, 0, "string", xl6StdStringSliceBody},
    {"string", "left", "__xl_std_string_left", NULL, NULL, 0, "string", xl6StdStringLeftBody},
    {"string", "right", "__xl_std_string_right", NULL, NULL, 0, "string", xl6StdStringRightBody},
    {"string", "mid", "__xl_std_string_mid", NULL, NULL, 0, "string", xl6StdStringMidBody},
    {"string", "charAt", "__xl_std_string_char_at", NULL, NULL, 0, "string", xl6StdStringCharAtBody},
    {"string", "byteAt", "__xl_std_string_byte_at", NULL, NULL, 0, "int", xl6StdStringByteAtBody},
    {"string", "join", "__xl_std_string_join", NULL, NULL, 0, "string", xl6StdStringJoinBody},
    {"string", "repeat", "__xl_std_string_repeat", NULL, NULL, 0, "string", xl6StdStringRepeatBody},
    {"string", "isEmpty", "__xl_std_string_is_empty", NULL, NULL, 0, "bool", xl6StdStringIsEmptyBody},
    {"string", "isBlank", "__xl_std_string_is_blank", NULL, NULL, 0, "bool", xl6StdStringIsBlankBody},
    {"string", "remove", "__xl_std_string_remove", NULL, NULL, 0, "string", xl6StdStringRemoveBody},
    {"string", "removeAt", "__xl_std_string_remove_at", NULL, NULL, 0, "string", xl6StdStringRemoveAtBody},
    {"string", "insert", "__xl_std_string_insert", NULL, NULL, 0, "string", xl6StdStringInsertBody},
    {"string", "padLeft", "__xl_std_string_pad_left", NULL, NULL, 0, "string", xl6StdStringPadLeftBody},
    {"string", "padRight", "__xl_std_string_pad_right", NULL, NULL, 0, "string", xl6StdStringPadRightBody},
    {"string", "padCenter", "__xl_std_string_pad_center", NULL, NULL, 0, "string", xl6StdStringPadCenterBody},
    {"string", "reverse", "__xl_std_string_reverse", NULL, NULL, 0, "string", xl6StdStringReverseBody},
    {"string", "splitLines", "__xl_std_string_split_lines", NULL, NULL, 0, "array", xl6StdStringSplitLinesBody},
    {"string", "chars", "__xl_std_string_chars", NULL, NULL, 0, "array", xl6StdStringCharsBody},
    {"string", "bytes", "__xl_std_string_bytes", NULL, NULL, 0, "array", xl6StdStringBytesBody},

    {"time", "now", "__xl_std_time_now", NULL, NULL, 0, "time", xl6StdTimeNowBody},
    {"time", "today", "__xl_std_time_today", NULL, NULL, 0, "time", xl6StdTimeTodayBody},
    {"time", "clock", "__xl_std_time_clock", NULL, NULL, 0, "time", xl6StdTimeClockBody},
    {"time", "fromTime", "__xl_std_time_from_time", NULL, NULL, 0, "time", xl6StdTimeFromTimeBody},
    {"time", "fromDate", "__xl_std_time_from_date", NULL, NULL, 0, "time", xl6StdTimeFromDateBody},
    {"time", "fromDateTime", "__xl_std_time_from_datetime", NULL, NULL, 0, "time", xl6StdTimeFromDateTimeBody},
    {"time", "parse", "__xl_std_time_parse", NULL, NULL, 0, "time", xl6StdTimeParseBody},
    {"time", "parsePattern", "__xl_std_time_parse_pattern", NULL, NULL, 0, "time", xl6StdTimeParsePatternBody},
    {"time", "tryParse", "__xl_std_time_try_parse", NULL, NULL, 0, "(bool, time)", xl6StdTimeTryParseBody},
    {"time", "tryParsePattern", "__xl_std_time_try_parse_pattern", NULL, NULL, 0, "(bool, time)", xl6StdTimeTryParsePatternBody},
    {"time", "year", "__xl_std_time_year", NULL, NULL, 0, "int", xl6StdTimeYearBody},
    {"time", "month", "__xl_std_time_month", NULL, NULL, 0, "int", xl6StdTimeMonthBody},
    {"time", "day", "__xl_std_time_day", NULL, NULL, 0, "int", xl6StdTimeDayBody},
    {"time", "hour", "__xl_std_time_hour", NULL, NULL, 0, "int", xl6StdTimeHourBody},
    {"time", "minute", "__xl_std_time_minute", NULL, NULL, 0, "int", xl6StdTimeMinuteBody},
    {"time", "second", "__xl_std_time_second", NULL, NULL, 0, "int", xl6StdTimeSecondBody},
    {"time", "weekday", "__xl_std_time_weekday", NULL, NULL, 0, "int", xl6StdTimeWeekdayBody},
    {"time", "dayOfYear", "__xl_std_time_day_of_year", NULL, NULL, 0, "int", xl6StdTimeDayOfYearBody},
    {"time", "quarter", "__xl_std_time_quarter", NULL, NULL, 0, "int", xl6StdTimeQuarterBody},
    {"time", "isLeapYear", "__xl_std_time_is_leap_year", NULL, NULL, 0, "bool", xl6StdTimeIsLeapYearBody},
    {"time", "daysInMonth", "__xl_std_time_days_in_month", NULL, NULL, 0, "int", xl6StdTimeDaysInMonthBody},
    {"time", "daysInYear", "__xl_std_time_days_in_year", NULL, NULL, 0, "int", xl6StdTimeDaysInYearBody},
    {"time", "datePart", "__xl_std_time_date_part", NULL, NULL, 0, "time", xl6StdTimeDatePartBody},
    {"time", "timePart", "__xl_std_time_time_part", NULL, NULL, 0, "time", xl6StdTimeTimePartBody},
    {"time", "isSameDay", "__xl_std_time_is_same_day", NULL, NULL, 0, "bool", xl6StdTimeIsSameDayBody},
    {"time", "isSameMonth", "__xl_std_time_is_same_month", NULL, NULL, 0, "bool", xl6StdTimeIsSameMonthBody},
    {"time", "isSameYear", "__xl_std_time_is_same_year", NULL, NULL, 0, "bool", xl6StdTimeIsSameYearBody},
    {"time", "inRange", "__xl_std_time_in_range", NULL, NULL, 0, "bool", xl6StdTimeInRangeBody},
    {"time", "addYears", "__xl_std_time_add_years", NULL, NULL, 0, "time", xl6StdTimeAddYearsBody},
    {"time", "addMonths", "__xl_std_time_add_months", NULL, NULL, 0, "time", xl6StdTimeAddMonthsBody},
    {"time", "addDays", "__xl_std_time_add_days", NULL, NULL, 0, "time", xl6StdTimeAddDaysBody},
    {"time", "addHours", "__xl_std_time_add_hours", NULL, NULL, 0, "time", xl6StdTimeAddHoursBody},
    {"time", "addMinutes", "__xl_std_time_add_minutes", NULL, NULL, 0, "time", xl6StdTimeAddMinutesBody},
    {"time", "addSeconds", "__xl_std_time_add_seconds", NULL, NULL, 0, "time", xl6StdTimeAddSecondsBody},
    {"time", "addWeeks", "__xl_std_time_add_weeks", NULL, NULL, 0, "time", xl6StdTimeAddWeeksBody},
    {"time", "addQuarters", "__xl_std_time_add_quarters", NULL, NULL, 0, "time", xl6StdTimeAddQuartersBody},
    {"time", "diffYears", "__xl_std_time_diff_years", NULL, NULL, 0, "int", xl6StdTimeDiffYearsBody},
    {"time", "diffMonths", "__xl_std_time_diff_months", NULL, NULL, 0, "int", xl6StdTimeDiffMonthsBody},
    {"time", "diffDays", "__xl_std_time_diff_days", NULL, NULL, 0, "int", xl6StdTimeDiffDaysBody},
    {"time", "diffHours", "__xl_std_time_diff_hours", NULL, NULL, 0, "int", xl6StdTimeDiffHoursBody},
    {"time", "diffMinutes", "__xl_std_time_diff_minutes", NULL, NULL, 0, "int", xl6StdTimeDiffMinutesBody},
    {"time", "diffSeconds", "__xl_std_time_diff_seconds", NULL, NULL, 0, "int", xl6StdTimeDiffSecondsBody},
    {"time", "diffWeeks", "__xl_std_time_diff_weeks", NULL, NULL, 0, "int", xl6StdTimeDiffWeeksBody},
    {"time", "diffQuarters", "__xl_std_time_diff_quarters", NULL, NULL, 0, "int", xl6StdTimeDiffQuartersBody},
    {"time", "timer", "__xl_std_time_timer", NULL, NULL, 0, "float", xl6StdTimeTimerBody},
    {"time", "sleep", "__xl_std_time_sleep", NULL, xl6StdTimeSleepParams, 1, "", xl6StdTimeSleepBody},
    {"time", "nowUTC", "__xl_std_time_now_utc", NULL, NULL, 0, "time", xl6StdTimeNowUtcBody},
    {"time", "timezoneOffset", "__xl_std_time_timezone_offset", NULL, NULL, 0, "int", xl6StdTimeTimezoneOffsetBody},
    {"time", "rangeOverlap", "__xl_std_time_range_overlap", NULL, NULL, 0, "bool", xl6StdTimeRangeOverlapBody},
    {"time", "approx", "__xl_std_time_approx", NULL, NULL, 0, "bool", xl6StdTimeApproxBody},
    {"time", "toUnix", "__xl_std_time_to_unix", NULL, NULL, 0, "int", xl6StdTimeToUnixBody},
    {"time", "fromUnix", "__xl_std_time_from_unix", NULL, NULL, 0, "time", xl6StdTimeFromUnixBody},
    {"time", "utcToLocal", "__xl_std_time_utc_to_local", NULL, NULL, 0, "time", xl6StdTimeUtcToLocalBody},
    {"time", "localToUTC", "__xl_std_time_local_to_utc", NULL, NULL, 0, "time", xl6StdTimeLocalToUtcBody},
    {"time", "firstDayOfMonth", "__xl_std_time_first_day_of_month", NULL, NULL, 0, "time", xl6StdTimeFirstDayOfMonthBody},
    {"time", "lastDayOfMonth", "__xl_std_time_last_day_of_month", NULL, NULL, 0, "time", xl6StdTimeLastDayOfMonthBody},
    {"time", "firstDayOfYear", "__xl_std_time_first_day_of_year", NULL, NULL, 0, "time", xl6StdTimeFirstDayOfYearBody},
    {"time", "lastDayOfYear", "__xl_std_time_last_day_of_year", NULL, NULL, 0, "time", xl6StdTimeLastDayOfYearBody},
    {"time", "firstDayOfWeek", "__xl_std_time_first_day_of_week", NULL, NULL, 0, "time", xl6StdTimeFirstDayOfWeekBody},
    {"time", "lastDayOfWeek", "__xl_std_time_last_day_of_week", NULL, NULL, 0, "time", xl6StdTimeLastDayOfWeekBody},
    {"time", "weekOfYear", "__xl_std_time_week_of_year", NULL, NULL, 0, "int", xl6StdTimeWeekOfYearBody},
    {"time", "weekOfMonth", "__xl_std_time_week_of_month", NULL, NULL, 0, "int", xl6StdTimeWeekOfMonthBody},
    {"time", "format", "__xl_std_time_format", NULL, NULL, 0, "string", xl6StdTimeFormatBody},
    {"time", "formatPattern", "__xl_std_time_format_pattern", NULL, NULL, 0, "string", xl6StdTimeFormatPatternBody},
    {"time", "dateString", "__xl_std_time_date_string", NULL, NULL, 0, "string", xl6StdTimeDateStringBody},
    {"time", "timeString", "__xl_std_time_time_string", NULL, NULL, 0, "string", xl6StdTimeTimeStringBody},
    {"time", "relative", "__xl_std_time_relative", NULL, NULL, 0, "string", xl6StdTimeRelativeBody},

    {"random", "u32", "__xl_std_random_u32", NULL, NULL, 0, "uint32", xl6StdRandomU32Body},
    {"random", "u64", "__xl_std_random_u64", NULL, NULL, 0, "uint64", xl6StdRandomU64Body},
    {"random", "range", "__xl_std_random_range", NULL, NULL, 0, "int", xl6StdRandomRangeBody},
    {"random", "seed", "__xl_std_random_seed", NULL, NULL, 0, "", xl6StdRandomSeedBody},
    {"random", "string", "__xl_std_random_string", NULL, NULL, 0, "string", xl6StdRandomStringBody},

    {"path", "exists", "__xl_std_path_exists", NULL, NULL, 0, "bool", xl6StdPathExistsBody},
    {"path", "isAbs", "__xl_std_path_is_abs", NULL, NULL, 0, "bool", xl6StdPathIsAbsBody},
    {"path", "nameExt", "__xl_std_path_name_ext", NULL, NULL, 0, "string", xl6StdPathNameExtBody},
    {"path", "name", "__xl_std_path_name", NULL, NULL, 0, "string", xl6StdPathNameBody},
    {"path", "ext", "__xl_std_path_ext", NULL, NULL, 0, "string", xl6StdPathExtBody},
    {"path", "dir", "__xl_std_path_dir", NULL, NULL, 0, "string", xl6StdPathDirBody},
    {"path", "join", "__xl_std_path_join", NULL, NULL, 0, "string", xl6StdPathJoinBody},
    {"path", "random", "__xl_std_path_random", NULL, NULL, 0, "string", xl6StdPathRandomBody},
    {"path", "sep", "__xl_std_path_sep", NULL, NULL, 0, "string", xl6StdPathSepBody},
    {"path", "normalize", "__xl_std_path_normalize", NULL, NULL, 0, "string", xl6StdPathNormalizeBody},
    {"path", "abs", "__xl_std_path_abs", NULL, NULL, 0, "string", xl6StdPathAbsBody},
    {"path", "rel", "__xl_std_path_rel", NULL, NULL, 0, "string", xl6StdPathRelBody},
    {"path", "cwd", "__xl_std_path_cwd", NULL, NULL, 0, "string", xl6StdPathCwdBody},
    {"path", "setCwd", "__xl_std_path_set_cwd", NULL, NULL, 0, "bool", xl6StdPathSetCwdBody},
    {"path", "home", "__xl_std_path_home", NULL, NULL, 0, "string", xl6StdPathHomeBody},
    {"path", "temp", "__xl_std_path_temp", NULL, NULL, 0, "string", xl6StdPathTempBody},
    {"path", "appDir", "__xl_std_path_app_dir", NULL, NULL, 0, "string", xl6StdPathAppDirBody},
    {"path", "withExt", "__xl_std_path_with_ext", NULL, NULL, 0, "string", xl6StdPathWithExtBody},
    {"path", "withName", "__xl_std_path_with_name", NULL, NULL, 0, "string", xl6StdPathWithNameBody},

    {"file", "exists", "__xl_std_file_exists", NULL, NULL, 0, "bool", xl6StdFileExistsBody},
    {"file", "size", "__xl_std_file_size", NULL, NULL, 0, "int", xl6StdFileSizeBody},
    {"file", "setSize", "__xl_std_file_set_size", NULL, NULL, 0, "bool", xl6StdFileSetSizeBody},
    {"file", "attr", "__xl_std_file_attr", NULL, NULL, 0, "int", xl6StdFileAttrBody},
    {"file", "setAttr", "__xl_std_file_set_attr", NULL, NULL, 0, "bool", xl6StdFileSetAttrBody},
    {"file", "accessTime", "__xl_std_file_access_time", NULL, NULL, 0, "time", xl6StdFileAccessTimeBody},
    {"file", "changeTime", "__xl_std_file_change_time", NULL, NULL, 0, "time", xl6StdFileChangeTimeBody},
    {"file", "readText", "__xl_std_file_read_text", NULL, xl6StdFileReadTextParams, 2, "string", xl6StdFileReadTextBody},
    {"file", "writeText", "__xl_std_file_write_text", NULL, NULL, 0, "bool", xl6StdFileWriteTextBody},
    {"file", "appendText", "__xl_std_file_append_text", NULL, NULL, 0, "bool", xl6StdFileAppendTextBody},
    {"file", "delete", "__xl_std_file_delete", NULL, NULL, 0, "bool", xl6StdFileDeleteBody},
    {"file", "copy", "__xl_std_file_copy", NULL, NULL, 0, "bool", xl6StdFileCopyBody},
    {"file", "move", "__xl_std_file_move", NULL, NULL, 0, "bool", xl6StdFileMoveBody},
    {"file", "touch", "__xl_std_file_touch", NULL, NULL, 0, "bool", xl6StdFileTouchBody},
    {"file", "rename", "__xl_std_file_rename", NULL, NULL, 0, "bool", xl6StdFileRenameBody},
    {"file", "replace", "__xl_std_file_replace", NULL, NULL, 0, "bool", xl6StdFileReplaceBody},
    {"file", "writeTextAtomic", "__xl_std_file_write_text_atomic", NULL, NULL, 0, "bool", xl6StdFileWriteTextAtomicBody},
    {"file", "info", "__xl_std_file_info", NULL, NULL, 0, "FileInfo", xl6StdFileInfoBody},
    {"file", "open", "__xl_std_file_open", NULL, NULL, 0, "File", xl6StdFileOpenBody},
    {"file", "readTextAsync", "__xl_std_file_read_text_async", "xl6_xrt_file_read_text_async_utf8", xl6StdPathUnaryParams, 1, "future<string>", NULL},
    {"file", "writeTextAsync", "__xl_std_file_write_text_async", "xl6_xrt_file_write_text_async_utf8", xl6StdFileWriteTextUtf8Params, 2, "future<bool>", NULL},
    {"file", "appendTextAsync", "__xl_std_file_append_text_async", "xl6_xrt_file_append_text_async_utf8", xl6StdFileWriteTextUtf8Params, 2, "future<bool>", NULL},
    {"file", "copyAsync", "__xl_std_file_copy_async", "xl6_xrt_file_copy_async_overwrite", xl6StdFileReplaceParams, 2, "future<bool>", NULL},
    {"file", "moveAsync", "__xl_std_file_move_async", "xl6_xrt_file_move_async_overwrite", xl6StdFileReplaceParams, 2, "future<bool>", NULL},
    {"file", "deleteAsync", "__xl_std_file_delete_async", "xl6_xrt_file_delete_async", xl6StdPathUnaryParams, 1, "future<bool>", NULL},

    {"dir", "exists", "__xl_std_dir_exists", NULL, NULL, 0, "bool", xl6StdDirExistsBody},
    {"dir", "create", "__xl_std_dir_create", NULL, NULL, 0, "bool", xl6StdDirCreateBody},
    {"dir", "createAll", "__xl_std_dir_create_all", NULL, NULL, 0, "bool", xl6StdDirCreateAllBody},
    {"dir", "copy", "__xl_std_dir_copy", NULL, NULL, 0, "bool", xl6StdDirCopyBody},
    {"dir", "move", "__xl_std_dir_move", NULL, NULL, 0, "bool", xl6StdDirMoveBody},
    {"dir", "delete", "__xl_std_dir_delete", NULL, NULL, 0, "bool", xl6StdDirDeleteBody},
    {"dir", "list", "__xl_std_dir_list", NULL, NULL, 0, "array", xl6StdDirListBody},
    {"dir", "files", "__xl_std_dir_files", NULL, NULL, 0, "array", xl6StdDirFilesBody},
    {"dir", "dirs", "__xl_std_dir_dirs", NULL, NULL, 0, "array", xl6StdDirDirsBody},
    {"dir", "empty", "__xl_std_dir_empty", NULL, NULL, 0, "bool", xl6StdDirEmptyBody},
    {"dir", "count", "__xl_std_dir_count", NULL, NULL, 0, "int", xl6StdDirCountBody},
    {"dir", "size", "__xl_std_dir_size", NULL, NULL, 0, "int", xl6StdDirSizeBody},
    {"dir", "clean", "__xl_std_dir_clean", NULL, NULL, 0, "bool", xl6StdDirCleanBody},
    {"dir", "ensureEmpty", "__xl_std_dir_ensure_empty", NULL, NULL, 0, "bool", xl6StdDirEnsureEmptyBody},
    {"dir", "glob", "__xl_std_dir_glob", NULL, NULL, 0, "array", xl6StdDirGlobBody},
    {"dir", "entries", "__xl_std_dir_entries", NULL, NULL, 0, "array", xl6StdDirEntriesBody},
    {"dir", "match", "__xl_std_dir_match", NULL, NULL, 0, "array", xl6StdDirMatchBody},
    {"dir", "walk", "__xl_std_dir_walk", NULL, NULL, 0, "array", xl6StdDirWalkBody},

    {"json", "parse", "__xl_std_json_parse", NULL, NULL, 0, "var", xl6StdJsonParseBody},
    {"json", "valid", "__xl_std_json_valid", NULL, NULL, 0, "bool", xl6StdJsonValidBody},
    {"json", "read", "__xl_std_json_read", NULL, NULL, 0, "var", xl6StdJsonReadBody},
    {"json", "stringify", "__xl_std_json_stringify", NULL, xl6StdJsonStringifyParams, 2, "string", xl6StdJsonStringifyBody},
    {"json", "write", "__xl_std_json_write", NULL, xl6StdJsonWriteParams, 3, "bool", xl6StdJsonWriteBody},

    {"xson", "parse", "__xl_std_xson_parse", NULL, NULL, 0, "var", xl6StdXsonParseBody},
    {"xson", "valid", "__xl_std_xson_valid", NULL, NULL, 0, "bool", xl6StdXsonValidBody},
    {"xson", "read", "__xl_std_xson_read", NULL, NULL, 0, "var", xl6StdXsonReadBody},
    {"xson", "stringify", "__xl_std_xson_stringify", NULL, xl6StdJsonStringifyParams, 2, "string", xl6StdXsonStringifyBody},
    {"xson", "write", "__xl_std_xson_write", NULL, xl6StdJsonWriteParams, 3, "bool", xl6StdXsonWriteBody},

    {"codec", "hexEncode", "__xl_std_codec_hex_encode", NULL, NULL, 0, "string", xl6StdCodecHexEncodeBody},
    {"codec", "hexDecode", "__xl_std_codec_hex_decode", NULL, NULL, 0, "string", xl6StdCodecHexDecodeBody},
    {"codec", "base64Encode", "__xl_std_codec_base64_encode", NULL, NULL, 0, "string", xl6StdCodecBase64EncodeBody},
    {"codec", "base64Decode", "__xl_std_codec_base64_decode", NULL, NULL, 0, "string", xl6StdCodecBase64DecodeBody},
    {"codec", "hexEncodeBytes", "__xl_std_codec_hex_encode_bytes", NULL, NULL, 0, "string", xl6StdCodecHexEncodeBytesBody},
    {"codec", "hexDecodeBytes", "__xl_std_codec_hex_decode_bytes", NULL, NULL, 0, "Buffer", xl6StdCodecHexDecodeBytesBody},
    {"codec", "base64EncodeBytes", "__xl_std_codec_base64_encode_bytes", NULL, NULL, 0, "string", xl6StdCodecBase64EncodeBytesBody},
    {"codec", "base64DecodeBytes", "__xl_std_codec_base64_decode_bytes", NULL, NULL, 0, "Buffer", xl6StdCodecBase64DecodeBytesBody},
    {"codec", "percentEncode", "__xl_std_codec_percent_encode", NULL, NULL, 0, "string", xl6StdCodecPercentEncodeBody},
    {"codec", "percentDecode", "__xl_std_codec_percent_decode", NULL, NULL, 0, "string", xl6StdCodecPercentDecodeBody},
    {"codec", "formUrlEncode", "__xl_std_codec_form_url_encode", NULL, NULL, 0, "string", xl6StdCodecFormUrlEncodeBody},
    {"codec", "formUrlDecode", "__xl_std_codec_form_url_decode", NULL, NULL, 0, "string", xl6StdCodecFormUrlDecodeBody},

    {"encoding", "charSize", "__xl_std_encoding_char_size", NULL, NULL, 0, "int", xl6StdEncodingCharSizeBody},
    {"encoding", "isUtf8", "__xl_std_encoding_is_utf8", NULL, NULL, 0, "bool", xl6StdEncodingIsUtf8Body},
    {"encoding", "isUtf8Bytes", "__xl_std_encoding_is_utf8_bytes", NULL, NULL, 0, "bool", xl6StdEncodingIsUtf8BytesBody},
    {"encoding", "detect", "__xl_std_encoding_detect", NULL, NULL, 0, "int", xl6StdEncodingDetectBody},
    {"encoding", "detectBytes", "__xl_std_encoding_detect_bytes", NULL, NULL, 0, "int", xl6StdEncodingDetectBytesBody},
    {"encoding", "toBytes", "__xl_std_encoding_to_bytes", NULL, NULL, 0, "Buffer", xl6StdEncodingToBytesBody},
    {"encoding", "fromBytes", "__xl_std_encoding_from_bytes", NULL, NULL, 0, "string", xl6StdEncodingFromBytesBody},
    {"encoding", "convertBytes", "__xl_std_encoding_convert_bytes", NULL, NULL, 0, "Buffer", xl6StdEncodingConvertBytesBody},
    {"encoding", "name", "__xl_std_encoding_name", NULL, NULL, 0, "string", xl6StdEncodingNameBody},

    {"hash", "u32", "__xl_std_hash_u32", NULL, NULL, 0, "uint32", xl6StdHashU32Body},
    {"hash", "u32WithSeed", "__xl_std_hash_u32_with_seed", NULL, NULL, 0, "uint32", xl6StdHashU32WithSeedBody},
    {"hash", "u64", "__xl_std_hash_u64", NULL, NULL, 0, "uint64", xl6StdHashU64Body},
    {"hash", "u64WithSeed", "__xl_std_hash_u64_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64WithSeedBody},
    {"hash", "u64Micro", "__xl_std_hash_u64_micro", NULL, NULL, 0, "uint64", xl6StdHashU64MicroBody},
    {"hash", "u64MicroWithSeed", "__xl_std_hash_u64_micro_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64MicroWithSeedBody},
    {"hash", "u64Nano", "__xl_std_hash_u64_nano", NULL, NULL, 0, "uint64", xl6StdHashU64NanoBody},
    {"hash", "u64NanoWithSeed", "__xl_std_hash_u64_nano_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64NanoWithSeedBody},

    {"crypto", "sha1", "__xl_std_crypto_sha1", NULL, NULL, 0, "string", xl6StdCryptoSha1Body},
    {"crypto", "sha256", "__xl_std_crypto_sha256", NULL, NULL, 0, "string", xl6StdCryptoSha256Body},
    {"crypto", "sha384", "__xl_std_crypto_sha384", NULL, NULL, 0, "string", xl6StdCryptoSha384Body},
    {"crypto", "sha512", "__xl_std_crypto_sha512", NULL, NULL, 0, "string", xl6StdCryptoSha512Body},
    {"crypto", "hmacSha256", "__xl_std_crypto_hmac_sha256", NULL, NULL, 0, "string", xl6StdCryptoHmacSha256Body},
    {"crypto", "hmacSha384", "__xl_std_crypto_hmac_sha384", NULL, NULL, 0, "string", xl6StdCryptoHmacSha384Body},
    {"crypto", "hmacSha512", "__xl_std_crypto_hmac_sha512", NULL, NULL, 0, "string", xl6StdCryptoHmacSha512Body},
    {"crypto", "sha256Stream", "__xl_std_crypto_sha256_stream", NULL, NULL, 0, "Sha256", xl6StdCryptoSha256StreamBody},
    {"crypto", "sha384Stream", "__xl_std_crypto_sha384_stream", NULL, NULL, 0, "Sha384", xl6StdCryptoSha384StreamBody},
    {"crypto", "sha512Stream", "__xl_std_crypto_sha512_stream", NULL, NULL, 0, "Sha512", xl6StdCryptoSha512StreamBody},
    {"crypto", "chacha20Encrypt", "__xl_std_crypto_chacha20_encrypt", NULL, NULL, 0, "string", xl6StdCryptoChaCha20EncryptBody},
    {"crypto", "chacha20Decrypt", "__xl_std_crypto_chacha20_decrypt", NULL, NULL, 0, "string", xl6StdCryptoChaCha20DecryptBody},
    {"crypto", "chacha20Poly1305Encrypt", "__xl_std_crypto_chacha20_poly1305_encrypt", NULL, NULL, 0, "string", xl6StdCryptoChaCha20Poly1305EncryptBody},
    {"crypto", "chacha20Poly1305Decrypt", "__xl_std_crypto_chacha20_poly1305_decrypt", NULL, NULL, 0, "string", xl6StdCryptoChaCha20Poly1305DecryptBody},
    {"crypto", "aes128GcmEncrypt", "__xl_std_crypto_aes128_gcm_encrypt", NULL, NULL, 0, "string", xl6StdCryptoAes128GcmEncryptBody},
    {"crypto", "aes128GcmDecrypt", "__xl_std_crypto_aes128_gcm_decrypt", NULL, NULL, 0, "string", xl6StdCryptoAes128GcmDecryptBody},
    {"crypto", "aes256GcmEncrypt", "__xl_std_crypto_aes256_gcm_encrypt", NULL, NULL, 0, "string", xl6StdCryptoAes256GcmEncryptBody},
    {"crypto", "aes256GcmDecrypt", "__xl_std_crypto_aes256_gcm_decrypt", NULL, NULL, 0, "string", xl6StdCryptoAes256GcmDecryptBody},
    {"crypto", "hkdfSha256", "__xl_std_crypto_hkdf_sha256", NULL, NULL, 0, "string", xl6StdCryptoHkdfSha256Body},
    {"crypto", "hkdfSha384", "__xl_std_crypto_hkdf_sha384", NULL, NULL, 0, "string", xl6StdCryptoHkdfSha384Body},
    {"crypto", "randomHex", "__xl_std_crypto_random_hex", NULL, NULL, 0, "string", xl6StdCryptoRandomHexBody},
    {"crypto", "ed25519Keypair", "__xl_std_crypto_ed25519_keypair", NULL, NULL, 0, "var", xl6StdCryptoEd25519KeypairBody},
    {"crypto", "ed25519PublicKey", "__xl_std_crypto_ed25519_public_key", NULL, NULL, 0, "string", xl6StdCryptoEd25519PublicKeyBody},
    {"crypto", "ed25519Sign", "__xl_std_crypto_ed25519_sign", NULL, NULL, 0, "string", xl6StdCryptoEd25519SignBody},
    {"crypto", "ed25519Verify", "__xl_std_crypto_ed25519_verify", NULL, NULL, 0, "bool", xl6StdCryptoEd25519VerifyBody},
    {"crypto", "x25519Keypair", "__xl_std_crypto_x25519_keypair", NULL, NULL, 0, "var", xl6StdCryptoX25519KeypairBody},
    {"crypto", "x25519SharedSecret", "__xl_std_crypto_x25519_shared_secret", NULL, NULL, 0, "string", xl6StdCryptoX25519SharedSecretBody},
    {"crypto", "x448Keypair", "__xl_std_crypto_x448_keypair", NULL, NULL, 0, "var", xl6StdCryptoX448KeypairBody},
    {"crypto", "x448SharedSecret", "__xl_std_crypto_x448_shared_secret", NULL, NULL, 0, "string", xl6StdCryptoX448SharedSecretBody},
    {"crypto", "rsaPssVerify", "__xl_std_crypto_rsa_pss_verify", NULL, NULL, 0, "bool", xl6StdCryptoRsaPssVerifyBody},
    {"crypto", "rsaPkcs1Verify", "__xl_std_crypto_rsa_pkcs1_verify", NULL, NULL, 0, "bool", xl6StdCryptoRsaPkcs1VerifyBody},

    {"id", "xid", "__xl_std_id_xid", NULL, NULL, 0, "string", xl6StdIdXidBody},

    {"net", "localIP", "__xl_std_net_local_ip", NULL, NULL, 0, "string", xl6StdNetLocalIPBody},
    {"net", "localMAC", "__xl_std_net_local_mac", NULL, NULL, 0, "string", xl6StdNetLocalMACBody},
    {"net", "localName", "__xl_std_net_local_name", NULL, NULL, 0, "string", xl6StdNetLocalNameBody},

    {"tcp", "connect", "__xl_std_tcp_connect", NULL, NULL, 0, "point", xl6StdTcpConnectBody},
    {"tcp", "connectAsync", "__xl_std_tcp_connect_async", NULL, NULL, 0, "future<point>", xl6StdTcpConnectAsyncBody},
    {"tcp", "listen", "__xl_std_tcp_listen", NULL, NULL, 0, "point", xl6StdTcpListenBody},

    {"udp", "bind", "__xl_std_udp_bind", NULL, NULL, 0, "point", xl6StdUdpBindBody},

    {"url", "valid", "__xl_std_url_valid", NULL, NULL, 0, "bool", xl6StdUrlValidBody},
    {"url", "isAbsolute", "__xl_std_url_is_absolute", NULL, NULL, 0, "bool", xl6StdUrlIsAbsoluteBody},
    {"url", "isSecure", "__xl_std_url_is_secure", NULL, NULL, 0, "bool", xl6StdUrlIsSecureBody},
    {"url", "port", "__xl_std_url_port", NULL, NULL, 0, "int", xl6StdUrlPortBody},
    {"url", "scheme", "__xl_std_url_scheme", NULL, NULL, 0, "string", xl6StdUrlSchemeBody},
    {"url", "authority", "__xl_std_url_authority", NULL, NULL, 0, "string", xl6StdUrlAuthorityBody},
    {"url", "userInfo", "__xl_std_url_user_info", NULL, NULL, 0, "string", xl6StdUrlUserInfoBody},
    {"url", "host", "__xl_std_url_host", NULL, NULL, 0, "string", xl6StdUrlHostBody},
    {"url", "path", "__xl_std_url_path", NULL, NULL, 0, "string", xl6StdUrlPathBody},
    {"url", "query", "__xl_std_url_query", NULL, NULL, 0, "string", xl6StdUrlQueryBody},
    {"url", "fragment", "__xl_std_url_fragment", NULL, NULL, 0, "string", xl6StdUrlFragmentBody},
    {"url", "target", "__xl_std_url_target", NULL, NULL, 0, "string", xl6StdUrlTargetBody},
    {"url", "resolve", "__xl_std_url_resolve", NULL, NULL, 0, "string", xl6StdUrlResolveBody},
    {"url", "normalizePath", "__xl_std_url_normalize_path", NULL, NULL, 0, "string", xl6StdUrlNormalizePathBody},

    {"query", "encode", "__xl_std_query_encode", NULL, NULL, 0, "string", xl6StdQueryEncodeBody},
    {"query", "decode", "__xl_std_query_decode", NULL, NULL, 0, "string", xl6StdQueryDecodeBody},
    {"query", "valid", "__xl_std_query_valid", NULL, NULL, 0, "bool", xl6StdQueryValidBody},
    {"query", "pair", "__xl_std_query_pair", NULL, NULL, 0, "string", xl6StdQueryPairBody},
    {"query", "append", "__xl_std_query_append", NULL, NULL, 0, "string", xl6StdQueryAppendBody},

    {"regex", "compile", "__xl_std_regex_compile", NULL, NULL, 0, "Regex", xl6StdRegexCompileBody},
    {"regex", "isMatch", "__xl_std_regex_is_match", NULL, NULL, 0, "bool", xl6StdRegexIsMatchBody},
    {"regex", "fullMatch", "__xl_std_regex_full_match", NULL, NULL, 0, "bool", xl6StdRegexFullMatchBody},
    {"regex", "find", "__xl_std_regex_find", NULL, NULL, 0, "int", xl6StdRegexFindBody},
    {"regex", "match", "__xl_std_regex_match", NULL, NULL, 0, "RegexMatch", xl6StdRegexMatchBody},
    {"regex", "set", "__xl_std_regex_set", NULL, NULL, 0, "RegexSet", xl6StdRegexSetBody},

    {"process", "exec", "__xl_std_process_exec", NULL, NULL, 0, "string", xl6StdProcessExecBody},
    {"process", "success", "__xl_std_process_success", NULL, NULL, 0, "bool", xl6StdProcessSuccessBody},
    {"process", "exitCode", "__xl_std_process_exit_code", NULL, NULL, 0, "int", xl6StdProcessExitCodeBody},
    {"process", "run", "__xl_std_process_run", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunBody},
    {"process", "runFile", "__xl_std_process_run_file", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunFileBody},
    {"process", "cmd", "__xl_std_process_cmd", NULL, NULL, 0, "ProcessCommand", xl6StdProcessCmdBody},
    {"process", "shell", "__xl_std_process_shell", NULL, NULL, 0, "ProcessCommand", xl6StdProcessShellBody},
    {"process", "pipeline", "__xl_std_process_pipeline", NULL, NULL, 0, "PipelineResult", xl6StdProcessPipelineBody},
    {"process", "start", "__xl_std_process_start", NULL, NULL, 0, "Process", xl6StdProcessStartBody},
    {"process", "startFile", "__xl_std_process_start_file", NULL, NULL, 0, "Process", xl6StdProcessStartFileBody},

    {"log", "create", "__xl_std_log_create", NULL, NULL, 0, "Logger", xl6StdLogCreateBody},
    {"log", "default", "__xl_std_log_default", NULL, NULL, 0, "Logger", xl6StdLogDefaultBody},
    {"log", "setLevel", "__xl_std_log_set_level", NULL, NULL, 0, "", xl6StdLogSetLevelBody},
    {"log", "getLevel", "__xl_std_log_get_level", NULL, NULL, 0, "int", xl6StdLogGetLevelBody},
    {"log", "levelName", "__xl_std_log_level_name", NULL, NULL, 0, "string", xl6StdLogLevelNameBody},
    {"log", "addConsole", "__xl_std_log_add_console", NULL, NULL, 0, "bool", xl6StdLogAddConsoleBody},
    {"log", "addFile", "__xl_std_log_add_file", NULL, NULL, 0, "bool", xl6StdLogAddFileBody},
    {"log", "addRollingFile", "__xl_std_log_add_rolling_file", NULL, NULL, 0, "bool", xl6StdLogAddRollingFileBody},
    {"log", "write", "__xl_std_log_write", NULL, NULL, 0, "", xl6StdLogWriteBody},
    {"log", "trace", "__xl_std_log_trace", NULL, NULL, 0, "", xl6StdLogTraceBody},
    {"log", "debug", "__xl_std_log_debug", NULL, NULL, 0, "", xl6StdLogDebugBody},
    {"log", "info", "__xl_std_log_info", NULL, NULL, 0, "", xl6StdLogInfoBody},
    {"log", "warn", "__xl_std_log_warn", NULL, NULL, 0, "", xl6StdLogWarnBody},
    {"log", "error", "__xl_std_log_error", NULL, NULL, 0, "", xl6StdLogErrorBody},
    {"log", "fatal", "__xl_std_log_fatal", NULL, NULL, 0, "", xl6StdLogFatalBody},
    {"log", "flush", "__xl_std_log_flush", NULL, NULL, 0, "", xl6StdLogFlushBody},

    {"template", "compile", "__xl_std_template_compile", NULL, xl6StdTemplateCompileParams, 2, "Template", xl6StdTemplateCompileBody},
    {"template", "render", "__xl_std_template_render", NULL, xl6StdTemplateRenderParams, 3, "string", xl6StdTemplateRenderBody},
    {"template", "renderFile", "__xl_std_template_render_file", NULL, xl6StdTemplateRenderFileParams, 3, "string", xl6StdTemplateRenderFileBody},

    {"http.server", "create", "__xl_std_http_server_create", NULL, NULL, 0, "point", xl6StdHttpServerCreateBody},
    {"http.server", "createTls", "__xl_std_http_server_create_tls", NULL, NULL, 0, "point", xl6StdHttpServerCreateTlsBody},
    {"http.server", "app", "__xl_std_http_app_create", NULL, NULL, 0, "point", xl6StdHttpAppCreateBody},
    {"http.server", "destroyApp", "__xl_std_http_app_destroy", NULL, NULL, 0, "", xl6StdHttpAppDestroyBody},
    {"http.server", "appRoute", "__xl_std_http_app_route", NULL, NULL, 0, "bool", xl6StdHttpAppRouteBody},
    {"http.server", "appRouteRequest", "__xl_std_http_app_route_request", NULL, NULL, 0, "bool", xl6StdHttpAppRouteRequestBody},
    {"http.server", "appGet", "__xl_std_http_app_get", NULL, NULL, 0, "bool", xl6StdHttpAppGetBody},
    {"http.server", "appPost", "__xl_std_http_app_post", NULL, NULL, 0, "bool", xl6StdHttpAppPostBody},
    {"http.server", "appAny", "__xl_std_http_app_any", NULL, NULL, 0, "bool", xl6StdHttpAppAnyBody},
    {"http.server", "appGetRequest", "__xl_std_http_app_get_request", NULL, NULL, 0, "bool", xl6StdHttpAppGetRequestBody},
    {"http.server", "appPostRequest", "__xl_std_http_app_post_request", NULL, NULL, 0, "bool", xl6StdHttpAppPostRequestBody},
    {"http.server", "appAnyRequest", "__xl_std_http_app_any_request", NULL, NULL, 0, "bool", xl6StdHttpAppAnyRequestBody},
    {"http.server", "appRouteStreamRequest", "__xl_std_http_app_route_stream_request", NULL, NULL, 0, "bool", xl6StdHttpAppRouteStreamRequestBody},
    {"http.server", "appPostStreamRequest", "__xl_std_http_app_post_stream_request", NULL, NULL, 0, "bool", xl6StdHttpAppPostStreamRequestBody},
    {"http.server", "appAnyStreamRequest", "__xl_std_http_app_any_stream_request", NULL, NULL, 0, "bool", xl6StdHttpAppAnyStreamRequestBody},
    {"http.server", "appStatic", "__xl_std_http_app_static", NULL, NULL, 0, "bool", xl6StdHttpAppStaticBody},
    {"http.server", "appStaticEx", "__xl_std_http_app_static_ex", NULL, NULL, 0, "bool", xl6StdHttpAppStaticExBody},
    {"http.server", "appError", "__xl_std_http_app_error", NULL, NULL, 0, "bool", xl6StdHttpAppErrorBody},
    {"http.server", "setApp", "__xl_std_http_server_set_app", NULL, NULL, 0, "bool", xl6StdHttpServerSetAppBody},
    {"http.server", "reloadApp", "__xl_std_http_server_reload_app", NULL, NULL, 0, "bool", xl6StdHttpServerReloadAppBody},
    {"http.server", "host", "__xl_std_http_server_host", NULL, NULL, 0, "bool", xl6StdHttpServerHostBody},
    {"http.server", "removeHost", "__xl_std_http_server_remove_host", NULL, NULL, 0, "bool", xl6StdHttpServerRemoveHostBody},
    {"http.server", "tlsHost", "__xl_std_http_server_tls_host", NULL, NULL, 0, "bool", xl6StdHttpServerTlsHostBody},
    {"http.server", "removeTlsHost", "__xl_std_http_server_remove_tls_host", NULL, NULL, 0, "bool", xl6StdHttpServerRemoveTlsHostBody},
    {"http.server", "error", "__xl_std_http_server_error", NULL, NULL, 0, "bool", xl6StdHttpServerErrorBody},
    {"http.server", "route", "__xl_std_http_server_route", NULL, NULL, 0, "bool", xl6StdHttpServerRouteBody},
    {"http.server", "routeRequest", "__xl_std_http_server_route_request", NULL, NULL, 0, "bool", xl6StdHttpServerRouteRequestBody},
    {"http.server", "get", "__xl_std_http_server_get", NULL, NULL, 0, "bool", xl6StdHttpServerGetBody},
    {"http.server", "post", "__xl_std_http_server_post", NULL, NULL, 0, "bool", xl6StdHttpServerPostBody},
    {"http.server", "any", "__xl_std_http_server_any", NULL, NULL, 0, "bool", xl6StdHttpServerAnyBody},
    {"http.server", "getRequest", "__xl_std_http_server_get_request", NULL, NULL, 0, "bool", xl6StdHttpServerGetRequestBody},
    {"http.server", "postRequest", "__xl_std_http_server_post_request", NULL, NULL, 0, "bool", xl6StdHttpServerPostRequestBody},
    {"http.server", "anyRequest", "__xl_std_http_server_any_request", NULL, NULL, 0, "bool", xl6StdHttpServerAnyRequestBody},
    {"http.server", "routeStreamRequest", "__xl_std_http_server_route_stream_request", NULL, NULL, 0, "bool", xl6StdHttpServerRouteStreamRequestBody},
    {"http.server", "postStreamRequest", "__xl_std_http_server_post_stream_request", NULL, NULL, 0, "bool", xl6StdHttpServerPostStreamRequestBody},
    {"http.server", "anyStreamRequest", "__xl_std_http_server_any_stream_request", NULL, NULL, 0, "bool", xl6StdHttpServerAnyStreamRequestBody},
    {"http.server", "static", "__xl_std_http_server_static", NULL, NULL, 0, "bool", xl6StdHttpServerStaticBody},
    {"http.server", "staticEx", "__xl_std_http_server_static_ex", NULL, NULL, 0, "bool", xl6StdHttpServerStaticExBody},
    {"http.server", "start", "__xl_std_http_server_start", NULL, NULL, 0, "bool", xl6StdHttpServerStartBody},
    {"http.server", "port", "__xl_std_http_server_port", NULL, NULL, 0, "int", xl6StdHttpServerPortBody},
    {"http.server", "requestCount", "__xl_std_http_server_request_count", NULL, NULL, 0, "int", xl6StdHttpServerRequestCountBody},
    {"http.server", "connectionCount", "__xl_std_http_server_connection_count", NULL, NULL, 0, "int", xl6StdHttpServerConnectionCountBody},
    {"http.server", "stop", "__xl_std_http_server_stop", NULL, NULL, 0, "", xl6StdHttpServerStopBody},
    {"http.server", "drain", "__xl_std_http_server_drain", NULL, NULL, 0, "bool", xl6StdHttpServerDrainBody},
    {"http.server", "destroy", "__xl_std_http_server_destroy", NULL, NULL, 0, "", xl6StdHttpServerDestroyBody},
    {"http.server", "mimeByPath", "__xl_std_http_server_mime_by_path", NULL, NULL, 0, "string", xl6StdHttpServerMimeByPathBody},
    {"http.server", "chunkText", "__xl_std_http_server_chunk_text", NULL, NULL, 0, "string", xl6StdHttpServerChunkTextBody},
    {"http.server", "chunkBytes", "__xl_std_http_server_chunk_bytes", NULL, NULL, 0, "Buffer", xl6StdHttpServerChunkBytesBody},
    {"http", "request", "__xl_std_http_request", NULL, NULL, 0, "HttpClientResponse", xl6StdHttpRequestBody},
    {"http", "get", "__xl_std_http_get", NULL, NULL, 0, "HttpClientResponse", xl6StdHttpGetBody},
    {"http", "post", "__xl_std_http_post", NULL, NULL, 0, "HttpClientResponse", xl6StdHttpPostBody}
};


#define XL6_STDLIB_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef char XL6_StdlibConstCountCheck[
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibConsts) == XL6_STDLIB_CONST_COUNT ? 1 : -1];
typedef char XL6_StdlibFuncCountCheck[
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibFuncs) == XL6_STDLIB_FUNC_COUNT ? 1 : -1];

const size_t xl6StdlibConstCount = XL6_STDLIB_CONST_COUNT;
const size_t xl6StdlibFuncCount = XL6_STDLIB_FUNC_COUNT;
