/* 从核心标准库原样迁出的 xhttp 高级语言绑定资产，等待扩展包接入。 */

const char* xl6StdlibHttpSemanticsClassSource =
    "class HttpHeaders\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_headers_create\" create()\n"
    "    extern \"xl6_xrt_http_headers_parse\" create(string block)\n"
    "    extern \"xl6_xrt_http_headers_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_headers_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_headers_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_headers_clone\" func cloneNative() as point\n"
    "    extern \"xl6_xrt_http_headers_clear\" func clear()\n"
    "    extern \"xl6_xrt_http_headers_count\" get count() as int\n"
    "    extern \"xl6_xrt_http_headers_bytes\" get bytes() as int\n"
    "    extern \"xl6_xrt_http_headers_append\" func append(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_headers_set\" func set(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_headers_remove\" func remove(string name) as int\n"
    "    extern \"xl6_xrt_http_headers_contains\" func contains(string name) as bool\n"
    "    extern \"xl6_xrt_http_headers_count_name\" func countName(string name) as int\n"
    "    extern \"xl6_xrt_http_headers_name_at\" func nameAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_headers_value_at\" func valueAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_headers_get\" func getNative(string name) as point\n"
    "    extern \"xl6_xrt_http_headers_get_nth\" func getNthNative(string name, int index) as point\n"
    "    extern \"xl6_xrt_http_headers_parse_append\" func parseAppend(string block) as bool\n"
    "    extern \"xl6_xrt_http_headers_build\" func buildNative() as point\n"
    "    extern \"xl6_xrt_http_headers_compact\" func compact() as bool\n"
    "\n"
    "    func clone() as HttpHeaders\n"
    "        return HttpHeaders(self.cloneNative())\n"
    "    end\n"
    "\n"
    "    func nameAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.nameAtNative(index))\n"
    "    end\n"
    "\n"
    "    func valueAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.valueAtNative(index))\n"
    "    end\n"
    "\n"
    "    func get(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.getNative(name))\n"
    "    end\n"
    "\n"
    "    func getNth(string name, int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.getNthNative(name, index))\n"
    "    end\n"
    "\n"
    "    func build() as string\n"
    "        return __xl_std_runtime_take_cstr(self.buildNative())\n"
    "    end\n"
    "\n"
    "    func __toString__() as string\n"
    "        return self.build()\n"
    "    end\n"
    "end\n"
    "\n"
    "class SearchParams\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_search_params_create\" create()\n"
    "    extern \"xl6_xrt_http_search_params_parse\" create(string query)\n"
    "    extern \"xl6_xrt_http_search_params_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_search_params_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_search_params_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_search_params_clone\" func cloneNative() as point\n"
    "    extern \"xl6_xrt_http_search_params_clear\" func clear()\n"
    "    extern \"xl6_xrt_http_search_params_count\" get count() as int\n"
    "    extern \"xl6_xrt_http_search_params_bytes\" get bytes() as int\n"
    "    extern \"xl6_xrt_http_search_params_append\" func append(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_search_params_set\" func set(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_search_params_delete\" func delete(string name) as int\n"
    "    extern \"xl6_xrt_http_search_params_has\" func has(string name) as bool\n"
    "    extern \"xl6_xrt_http_search_params_count_name\" func countName(string name) as int\n"
    "    extern \"xl6_xrt_http_search_params_name_at\" func nameAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_search_params_value_at\" func valueAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_search_params_has_value_at\" func hasValueAt(int index) as bool\n"
    "    extern \"xl6_xrt_http_search_params_get\" func getNative(string name) as point\n"
    "    extern \"xl6_xrt_http_search_params_get_nth\" func getNthNative(string name, int index) as point\n"
    "    extern \"xl6_xrt_http_search_params_parse_append\" func parseAppend(string query) as bool\n"
    "    extern \"xl6_xrt_http_search_params_sort\" func sort() as bool\n"
    "    extern \"xl6_xrt_http_search_params_build\" func buildNative() as point\n"
    "    extern \"xl6_xrt_http_search_params_compact\" func compact() as bool\n"
    "\n"
    "    func clone() as SearchParams\n"
    "        return SearchParams(self.cloneNative())\n"
    "    end\n"
    "\n"
    "    func nameAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.nameAtNative(index))\n"
    "    end\n"
    "\n"
    "    func valueAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.valueAtNative(index))\n"
    "    end\n"
    "\n"
    "    func get(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.getNative(name))\n"
    "    end\n"
    "\n"
    "    func getNth(string name, int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.getNthNative(name, index))\n"
    "    end\n"
    "\n"
    "    func build() as string\n"
    "        return __xl_std_runtime_take_cstr(self.buildNative())\n"
    "    end\n"
    "\n"
    "    func __toString__() as string\n"
    "        return self.build()\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpContext\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_context_create\" create()\n"
    "    extern \"xl6_xrt_http_context_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_context_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_context_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_context_with_cancel\" func withCancelNative() as point\n"
    "    extern \"xl6_xrt_http_context_with_deadline\" func withDeadlineNative(int deadlineMs) as point\n"
    "    extern \"xl6_xrt_http_context_with_timeout\" func withTimeoutNative(int timeoutMs) as point\n"
    "    extern \"xl6_xrt_http_context_cancel\" func cancel() as bool\n"
    "    extern \"xl6_xrt_http_context_cancelled\" get cancelled() as bool\n"
    "    extern \"xl6_xrt_http_context_done\" get done() as bool\n"
    "    extern \"xl6_xrt_http_context_deadline\" get deadlineMs() as int\n"
    "\n"
    "    func withCancel() as HttpContext\n"
    "        return HttpContext(self.withCancelNative())\n"
    "    end\n"
    "\n"
    "    func withDeadline(int deadlineMs) as HttpContext\n"
    "        return HttpContext(self.withDeadlineNative(deadlineMs))\n"
    "    end\n"
    "\n"
    "    func withTimeout(int timeoutMs) as HttpContext\n"
    "        return HttpContext(self.withTimeoutNative(timeoutMs))\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpBody\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_body_create\" create()\n"
    "    extern \"xl6_xrt_http_body_create_text\" create(string text)\n"
    "    extern \"xl6_xrt_http_body_create_bytes\" create(point data, c_size size)\n"
    "    extern \"xl6_xrt_http_body_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_body_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_body_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_body_content_length\" get contentLength() as int\n"
    "    extern \"xl6_xrt_http_body_flags\" get flags() as int\n"
    "    extern \"xl6_xrt_http_body_replayable\" get replayable() as bool\n"
    "end\n"
    "\n"
    "class FormDataEncoded\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_form_encoded_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_form_encoded_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_form_encoded_body\" func bodyNative() as point\n"
    "    extern \"xl6_xrt_http_form_encoded_content_type\" func contentTypeNative() as point\n"
    "\n"
    "    func body() as HttpBody\n"
    "        return HttpBody(self.bodyNative())\n"
    "    end\n"
    "\n"
    "    func contentType() as string\n"
    "        return __xl_std_runtime_take_cstr(self.contentTypeNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class FormData\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_form_data_create\" create()\n"
    "    extern \"xl6_xrt_http_form_data_parse\" create(string contentType, point data, c_size size)\n"
    "    extern \"xl6_xrt_http_form_data_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_form_data_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_form_data_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_form_data_clone\" func cloneNative() as point\n"
    "    extern \"xl6_xrt_http_form_data_clear\" func clear()\n"
    "    extern \"xl6_xrt_http_form_data_count\" get count() as int\n"
    "    extern \"xl6_xrt_http_form_data_count_name\" func countName(string name) as int\n"
    "    extern \"xl6_xrt_http_form_data_has\" func has(string name) as bool\n"
    "    extern \"xl6_xrt_http_form_data_delete\" func delete(string name) as int\n"
    "    extern \"xl6_xrt_http_form_data_append_text\" func append(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_form_data_append_bytes\" func appendBytesNative(string name, point data, c_size size, string fileName, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_form_data_append_body\" func appendBodyNative(string name, point body, string fileName, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_form_data_name_at\" func nameAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_form_data_file_name_at\" func fileNameAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_form_data_content_type_at\" func contentTypeAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_form_data_length_at\" func contentLengthAt(int index) as int\n"
    "    extern \"xl6_xrt_http_form_data_flags_at\" func flagsAt(int index) as int\n"
    "    extern \"xl6_xrt_http_form_data_build\" func buildNative(string boundary) as point\n"
    "\n"
    "    func clone() as FormData\n"
    "        return FormData(self.cloneNative())\n"
    "    end\n"
    "\n"
    "    func append(string name, Buffer data, string fileName = \"\", string contentType = \"application/octet-stream\") as bool\n"
    "        return self.appendBytesNative(name, data.data(), c_size(data.len()), fileName, contentType)\n"
    "    end\n"
    "\n"
    "    func append(string name, HttpBody body, string fileName = \"\", string contentType = \"application/octet-stream\") as bool\n"
    "        return self.appendBodyNative(name, body.nativeHandle(), fileName, contentType)\n"
    "    end\n"
    "\n"
    "    func nameAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.nameAtNative(index))\n"
    "    end\n"
    "\n"
    "    func fileNameAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.fileNameAtNative(index))\n"
    "    end\n"
    "\n"
    "    func contentTypeAt(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.contentTypeAtNative(index))\n"
    "    end\n"
    "\n"
    "    func build(string boundary = \"\") as FormDataEncoded\n"
    "        return FormDataEncoded(self.buildNative(boundary))\n"
    "    end\n"
    "end\n"
    "\n"
    "class CookieJar\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_cookie_jar_create\" create()\n"
    "    extern \"xl6_xrt_http_cookie_jar_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_cookie_jar_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_cookie_jar_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_cookie_jar_clear\" func clear()\n"
    "    extern \"xl6_xrt_http_cookie_jar_count\" get count() as int\n"
    "    extern \"xl6_xrt_http_cookie_jar_purge\" func purgeExpiredNative(int nowUnix) as int\n"
    "    extern \"xl6_xrt_http_cookie_jar_set\" func setCookieNative(string url, string setCookie, int nowUnix) as int\n"
    "    extern \"xl6_xrt_http_cookie_jar_header\" func headerNative(string url, int nowUnix) as point\n"
    "\n"
    "    func purgeExpired(int nowUnix = 0) as int\n"
    "        return self.purgeExpiredNative(nowUnix)\n"
    "    end\n"
    "\n"
    "    func setCookie(string url, string setCookie, int nowUnix = 0) as int\n"
    "        return self.setCookieNative(url, setCookie, nowUnix)\n"
    "    end\n"
    "\n"
    "    func header(string url, int nowUnix = 0) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerNative(url, nowUnix))\n"
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
    "    extern \"xl6_xrt_http_request_connection_open\" func connectionOpen() as bool\n"
    "    extern \"xl6_xrt_http_request_close_connection\" func closeConnectionNative(bool abortConnection) as bool\n"
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
    "        return Buffer(point(handle))\n"
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
    "        return Buffer(point(handle))\n"
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
    "\n"
    "    func closeConnection(bool abortConnection = false) as bool\n"
    "        return self.closeConnectionNative(abortConnection)\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpResponse\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_response_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_response_status\" func statusNative(int code, string reason)\n"
    "    extern \"xl6_xrt_http_response_header\" func header(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_response_add_header\" func addHeader(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_response_status_code\" func statusCode() as int\n"
    "    extern \"xl6_xrt_http_response_reason\" func reasonNative() as point\n"
    "    extern \"xl6_xrt_http_response_header_value\" func headerValueNative(string name) as point\n"
    "    extern \"xl6_xrt_http_response_header_count\" func headerCount() as int\n"
    "    extern \"xl6_xrt_http_response_header_name\" func headerNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_response_header_value_at\" func headerValueAtNative(int index) as point\n"
    "    extern \"xl6_xrt_http_response_body_size\" func bodySize() as int\n"
    "    extern \"xl6_xrt_http_response_body_buffer\" func bodyBytesNative() as point\n"
    "    extern \"xl6_xrt_http_response_committed\" func committed() as bool\n"
    "    extern \"xl6_xrt_http_response_cookie\" func cookieNative(string name, string value, string path, int maxAge, int sameSite, c_uint flags) as bool\n"
    "    extern \"xl6_xrt_http_response_delete_cookie\" func deleteCookieNative(string name, string path) as bool\n"
    "    extern \"xl6_xrt_http_response_text\" func textNative(string text, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_response_json_text\" func jsonText(string text) as bool\n"
    "    extern \"xl6_xrt_http_response_body\" func bodyNative(point data, c_size size, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_response_reserve_body\" func reserveBodyNative(c_size size) as bool\n"
    "    extern \"xl6_xrt_http_response_append_body\" func appendBodyNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_http_response_append_text\" func appendTextNative(string text) as bool\n"
    "    extern \"xl6_xrt_http_response_start\" func start() as bool\n"
    "    extern \"xl6_xrt_http_response_send\" func send(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_http_response_write\" func write(string text) as bool\n"
    "    extern \"xl6_xrt_http_response_end\" func finish() as bool\n"
    "    extern \"xl6_xrt_http_response_file\" func fileNative(string path, c_size chunkSize) as bool\n"
    "    extern \"xl6_xrt_http_response_file_range\" func fileRangeNative(string path, int offset, int length, c_size chunkSize) as bool\n"
    "    extern \"xl6_xrt_http_response_redirect\" func redirectNative(string url, int code) as bool\n"
    "\n"
    "    func status(int code, string reason = \"\")\n"
    "        self.statusNative(code, reason)\n"
    "    end\n"
    "\n"
    "    func reason() as string\n"
    "        return __xl_std_runtime_take_cstr(self.reasonNative())\n"
    "    end\n"
    "\n"
    "    func header(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerValueNative(name))\n"
    "    end\n"
    "\n"
    "    func headerName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerNameNative(index))\n"
    "    end\n"
    "\n"
    "    func headerValue(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.headerValueAtNative(index))\n"
    "    end\n"
    "\n"
    "    func bodyBytes() as Buffer\n"
    "        return Buffer(point(self.bodyBytesNative()))\n"
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
    "    func reserveBody(int size) as bool\n"
    "        return self.reserveBodyNative(c_size(size))\n"
    "    end\n"
    "\n"
    "    func appendBody(point data, c_size size) as bool\n"
    "        return self.appendBodyNative(data, size)\n"
    "    end\n"
    "\n"
    "    func appendText(string text) as bool\n"
    "        return self.appendTextNative(text)\n"
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
    "    func fileRange(string path, int offset, int length, int chunkSize = 65536) as bool\n"
    "        return self.fileRangeNative(path, offset, length, c_size(chunkSize))\n"
    "    end\n"
    "\n"
    "    func redirect(string url, int code = 302) as bool\n"
    "        return self.redirectNative(url, code)\n"
    "    end\n"
    "\n";


const char* xl6StdlibHttpServerClassEndSource =
    "end\n";


const char* xl6StdlibHttpClientClassSource =
    "extern \"xl6_xrt_http_client_request_stream_async\" func __xl_std_runtime_http_client_stream_async(point request, func(int, string) as bool onHeaders, func(point, int) as bool onBody) as future<HttpClientResponse>\n"
    "extern \"xl6_xrt_http_client_request_stream_client_async\" func __xl_std_runtime_http_client_stream_client_async(point request, point client, func(int, string) as bool onHeaders, func(point, int) as bool onBody) as future<HttpClientResponse>\n"
    "\n"
    "class HttpCallInfo\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_call_info_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_call_info_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_call_info_state\" get state() as int\n"
    "    extern \"xl6_xrt_http_call_info_phase\" get phase() as int\n"
    "    extern \"xl6_xrt_http_call_info_result\" get result() as int\n"
    "    extern \"xl6_xrt_http_call_info_error\" get error() as int\n"
    "    extern \"xl6_xrt_http_call_info_submitted\" get submittedUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_started\" get startedUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_transport_ready\" get transportReadyUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_request_sent\" get requestSentUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_first_byte\" get firstByteUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_headers\" get headersUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_last_progress\" get lastProgressUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_completed\" get completedUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_connect_duration\" get connectDurationUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_ttfb\" get timeToFirstByteUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_total_duration\" get totalDurationUs() as int\n"
    "    extern \"xl6_xrt_http_call_info_request_bytes\" get requestWireBytes() as int\n"
    "    extern \"xl6_xrt_http_call_info_response_wire_bytes\" get responseWireBytes() as int\n"
    "    extern \"xl6_xrt_http_call_info_response_body_bytes\" get responseBodyBytes() as int\n"
    "    extern \"xl6_xrt_http_call_info_redirects\" get redirects() as int\n"
    "    extern \"xl6_xrt_http_call_info_retries\" get retries() as int\n"
    "    extern \"xl6_xrt_http_call_info_reused\" get reusedConnection() as bool\n"
    "    extern \"xl6_xrt_http_call_info_secure\" get secure() as bool\n"
    "    extern \"xl6_xrt_http_call_info_cache\" get cache() as int\n"
    "    extern \"xl6_xrt_http_call_info_state_name\" func stateNameNative() as point\n"
    "    extern \"xl6_xrt_http_call_info_phase_name\" func phaseNameNative() as point\n"
    "    extern \"xl6_xrt_http_call_info_result_name\" func resultNameNative() as point\n"
    "    extern \"xl6_xrt_http_call_info_error_name\" func errorNameNative() as point\n"
    "\n"
    "    func errorName() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorNameNative())\n"
    "    end\n"
    "\n"
    "    func phaseName() as string\n"
    "        return __xl_std_runtime_take_cstr(self.phaseNameNative())\n"
    "    end\n"
    "\n"
    "    func stateName() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stateNameNative())\n"
    "    end\n"
    "\n"
    "    func resultName() as string\n"
    "        return __xl_std_runtime_take_cstr(self.resultNameNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpClientResponse\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_client_response_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_http_client_response_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_client_response_status\" func status() as int\n"
    "    extern \"xl6_xrt_http_client_response_flags\" func flags() as int\n"
    "    extern \"xl6_xrt_http_client_response_header_count\" func headerCount() as int\n"
    "    extern \"xl6_xrt_http_client_response_version\" func versionNative() as point\n"
    "    extern \"xl6_xrt_http_client_response_reason\" func reasonNative() as point\n"
    "    extern \"xl6_xrt_http_client_response_header\" func headerNative(string name) as point\n"
    "    extern \"xl6_xrt_http_client_response_header_name\" func headerNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http_client_response_header_value\" func headerValueNative(int index) as point\n"
    "    extern \"xl6_xrt_http_client_response_content_length\" func contentLength() as int\n"
    "    extern \"xl6_xrt_http_client_response_body_text\" func bodyNative() as point\n"
    "    extern \"xl6_xrt_http_client_response_body_data\" func bodyData() as point\n"
    "    extern \"xl6_xrt_http_client_response_body_size\" func bodySize() as int\n"
    "    extern \"xl6_xrt_http_client_response_body_buffer\" func bodyBytesNative() as point\n"
    "    extern \"xl6_xrt_http_client_response_call_info\" func callInfoNative() as point\n"
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
    "    func version() as string\n"
    "        return __xl_std_runtime_take_cstr(self.versionNative())\n"
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
    "    func body() as string\n"
    "        return __xl_std_runtime_take_cstr(self.bodyNative())\n"
    "    end\n"
    "\n"
    "    func bodyBytes() as Buffer\n"
    "        point handle = self.bodyBytesNative()\n"
    "        return Buffer(point(handle))\n"
    "    end\n"
    "\n"
    "    func callInfo() as HttpCallInfo\n"
    "        return HttpCallInfo(point(self.callInfoNative()))\n"
    "    end\n"
    "end\n"
    "\n"
    "class HttpClient\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_client_create\" create()\n"
    "    extern \"xl6_xrt_http_client_create_engine\" create(point engine)\n"
    "    extern \"xl6_xrt_http_client_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_client_close_idle\" func closeIdle() as int\n"
    "    extern \"xl6_xrt_http_client_native\" func nativeHandle() as point\n"
    "end\n"
    "\n"
    "class HttpClientRequest\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_client_request_create_url\" create(string url)\n"
    "    extern \"xl6_xrt_http_client_request_create_method_url\" create(string method, string url)\n"
    "    extern \"xl6_xrt_http_client_request_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_client_request_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_client_request_method\" func method(string method) as bool\n"
    "    extern \"xl6_xrt_http_client_request_url\" func url(string url) as bool\n"
    "    extern \"xl6_xrt_http_client_request_header\" func header(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_client_request_add_header\" func addHeader(string name, string value) as bool\n"
    "    extern \"xl6_xrt_http_client_request_remove_header\" func removeHeader(string name) as int\n"
    "    extern \"xl6_xrt_http_client_request_body\" func body(string data, c_size size, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_client_request_body_raw\" func bodyRaw(point data, c_size size, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_client_request_body_source\" func bodySourceNative(point body, string contentType) as bool\n"
    "    extern \"xl6_xrt_http_client_request_form_data\" func formDataNative(point form) as bool\n"
    "    extern \"xl6_xrt_http_client_request_context\" func contextNative(point context) as bool\n"
    "    extern \"xl6_xrt_http_client_request_timeout\" func timeout(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_http_client_request_idle_timeout\" func idleTimeout(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_http_client_request_proxy\" func proxyNative(point proxy) as bool\n"
    "    extern \"xl6_xrt_http_client_request_execute\" func executeNative() as point\n"
    "    extern \"xl6_xrt_http_client_request_execute_client\" func executeClientNative(point client) as point\n"
    "    extern \"xl6_xrt_http_client_request_execute_async\" func executeAsync() as future<HttpClientResponse>\n"
    "    extern \"xl6_xrt_http_client_request_execute_client_async\" func executeClientAsyncNative(point client) as future<HttpClientResponse>\n"
    "\n"
    "    func text(string data, string contentType = \"text/plain; charset=utf-8\") as bool\n"
    "        return self.body(data, __xl_std_crt_strlen(data), contentType)\n"
    "    end\n"
    "\n"
    "    func bytes(Buffer data, string contentType = \"application/octet-stream\") as bool\n"
    "        return self.bodyRaw(data.data(), c_size(data.len()), contentType)\n"
    "    end\n"
    "\n"
    "    func body(HttpBody data, string contentType = \"application/octet-stream\") as bool\n"
    "        return self.bodySourceNative(data.nativeHandle(), contentType)\n"
    "    end\n"
    "\n"
    "    func formData(FormData data) as bool\n"
    "        return self.formDataNative(data.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func context(HttpContext value) as bool\n"
    "        return self.contextNative(value.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func proxy(NetProxy value) as bool\n"
    "        return self.proxyNative(value.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func execute() as HttpClientResponse\n"
    "        return HttpClientResponse(self.executeNative())\n"
    "    end\n"
    "\n"
    "    func execute(HttpClient client) as HttpClientResponse\n"
    "        return HttpClientResponse(self.executeClientNative(client.nativeHandle()))\n"
    "    end\n"
    "\n"
    "    func executeAsync(HttpClient client) as future<HttpClientResponse>\n"
    "        return self.executeClientAsyncNative(client.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func stream(func(int, string) as bool onHeaders, func(point, int) as bool onBody) as future<HttpClientResponse>\n"
    "        return __xl_std_runtime_http_client_stream_async(self.nativeHandle(), onHeaders, onBody)\n"
    "    end\n"
    "\n"
    "    func stream(HttpClient client, func(int, string) as bool onHeaders, func(point, int) as bool onBody) as future<HttpClientResponse>\n"
    "        return __xl_std_runtime_http_client_stream_client_async(self.nativeHandle(), client.nativeHandle(), onHeaders, onBody)\n"
    "    end\n"
    "end\n";


    {"int", "affinity"}
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

static const XL6_StdlibParam xl6StdRuntimeHttpServerCreateEngineParams[] = {
    {"point", "engine"},
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

static const XL6_StdlibParam xl6StdRuntimeHttpServerCreateTlsEngineParams[] = {
    {"point", "engine"},
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


    "    return UdpSocket(point(__xl_std_xrt_udp_bind(host, uint16(port))))\n"
    "end\n";

static const char xl6StdHttpServerCreateBody[] =
    "func __xl_std_http_server_create(int port = 0, string host = \"127.0.0.1\", int backlog = 128, int recvLimit = 1048576, int bodyLimit = 1048576, int headerLimit = 100, int headerBytesLimit = 65536, int workerCount = 1, int headerTimeoutMs = 30000, int bodyTimeoutMs = 30000, int idleTimeoutMs = 60000, int writeTimeoutMs = 30000, int multipartPartLimit = 1000, int multipartPartSizeLimit = 0) as point\n"
    "    return __xl_std_runtime_http_server_create(host, c_int(port), c_uint(backlog), c_uint(recvLimit), c_uint(bodyLimit), c_uint(headerLimit), c_uint(headerBytesLimit), c_uint(workerCount), c_uint(headerTimeoutMs), c_uint(bodyTimeoutMs), c_uint(idleTimeoutMs), c_uint(writeTimeoutMs), c_uint(multipartPartLimit), c_uint(multipartPartSizeLimit))\n"
    "end\n";

static const char xl6StdHttpServerCreateEngineBody[] =
    "func __xl_std_http_server_create(NetEngine engine, int port = 0, string host = \"127.0.0.1\", int backlog = 128, int recvLimit = 1048576, int bodyLimit = 1048576, int headerLimit = 100, int headerBytesLimit = 65536, int workerCount = 0, int headerTimeoutMs = 30000, int bodyTimeoutMs = 30000, int idleTimeoutMs = 60000, int writeTimeoutMs = 30000, int multipartPartLimit = 1000, int multipartPartSizeLimit = 0) as point\n"
    "    return __xl_std_runtime_http_server_create_engine(engine.nativeHandle(), host, c_int(port), c_uint(backlog), c_uint(recvLimit), c_uint(bodyLimit), c_uint(headerLimit), c_uint(headerBytesLimit), c_uint(workerCount), c_uint(headerTimeoutMs), c_uint(bodyTimeoutMs), c_uint(idleTimeoutMs), c_uint(writeTimeoutMs), c_uint(multipartPartLimit), c_uint(multipartPartSizeLimit))\n"
    "end\n";

static const char xl6StdHttpServerCreateTlsBody[] =
    "func __xl_std_http_server_create_tls(string certFile, string keyFile, int port = 0, string host = \"127.0.0.1\", int backlog = 128, int recvLimit = 1048576, int bodyLimit = 1048576, int headerLimit = 100, int headerBytesLimit = 65536, int workerCount = 1, int headerTimeoutMs = 30000, int bodyTimeoutMs = 30000, int idleTimeoutMs = 60000, int writeTimeoutMs = 30000, int multipartPartLimit = 1000, int multipartPartSizeLimit = 0) as point\n"
    "    return __xl_std_runtime_http_server_create_tls(certFile, keyFile, host, c_int(port), c_uint(backlog), c_uint(recvLimit), c_uint(bodyLimit), c_uint(headerLimit), c_uint(headerBytesLimit), c_uint(workerCount), c_uint(headerTimeoutMs), c_uint(bodyTimeoutMs), c_uint(idleTimeoutMs), c_uint(writeTimeoutMs), c_uint(multipartPartLimit), c_uint(multipartPartSizeLimit))\n"
    "end\n";

static const char xl6StdHttpServerCreateTlsEngineBody[] =
    "func __xl_std_http_server_create_tls(NetEngine engine, string certFile, string keyFile, int port = 0, string host = \"127.0.0.1\", int backlog = 128, int recvLimit = 1048576, int bodyLimit = 1048576, int headerLimit = 100, int headerBytesLimit = 65536, int workerCount = 0, int headerTimeoutMs = 30000, int bodyTimeoutMs = 30000, int idleTimeoutMs = 60000, int writeTimeoutMs = 30000, int multipartPartLimit = 1000, int multipartPartSizeLimit = 0) as point\n"
    "    return __xl_std_runtime_http_server_create_tls_engine(engine.nativeHandle(), certFile, keyFile, host, c_int(port), c_uint(backlog), c_uint(recvLimit), c_uint(bodyLimit), c_uint(headerLimit), c_uint(headerBytesLimit), c_uint(workerCount), c_uint(headerTimeoutMs), c_uint(bodyTimeoutMs), c_uint(idleTimeoutMs), c_uint(writeTimeoutMs), c_uint(multipartPartLimit), c_uint(multipartPartSizeLimit))\n"
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
    "    return Buffer(point(handle))\n"
    "end\n";

static const char xl6StdHttpClientBody[] =
    "func __xl_std_http_client() as HttpClient\n"
    "    return HttpClient()\n"
    "end\n";

static const char xl6StdHttpClientEngineBody[] =
    "func __xl_std_http_client(NetEngine engine) as HttpClient\n"
    "    return HttpClient(engine.nativeHandle())\n"
    "end\n";

static const char xl6StdHttpClientTlsBody[] =
    "extern \"xl6_xrt_http_client_create_tls\" func __xl_std_runtime_http_client_create_tls(point config) as HttpClient\n"
    "func __xl_std_http_client(TlsConfig config) as HttpClient\n"
    "    return __xl_std_runtime_http_client_create_tls(config.nativeHandle())\n"
    "end\n";

static const char xl6StdHttpCopyChunkBody[] =
    "func __xl_std_http_copy_chunk(point data, int size) as Buffer\n"
    "    Buffer result = Buffer()\n"
    "    if size > 0 then\n"
    "        result.appendRaw(data, c_size(size))\n"
    "    end\n"
    "    return result\n"
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


