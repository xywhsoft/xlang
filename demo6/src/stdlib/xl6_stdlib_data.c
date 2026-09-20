#include "src/stdlib/xl6_stdlib_data.h"

const char* xl6StdlibCoreClassSource =
    "class DynamicFields\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_dynamic_fields_create\" create()\n"
    "    extern \"xl6_xrt_dynamic_fields_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_dynamic_fields_from_value\" create(var value)\n"
    "    extern \"xl6_xrt_dynamic_fields_destroy\" destroy()\n"
    "    extern \"xl6_xrt_dynamic_fields_native_handle\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_dynamic_fields_count\" func countNative() as int\n"
    "    extern \"xl6_xrt_dynamic_fields_capacity\" func capacityNative() as int\n"
    "    extern \"xl6_xrt_dynamic_fields_reserve\" func reserve(int capacity) as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_trim\" func trim() as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_clear\" func clear() as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_has\" func has(string name) as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_get\" func get(string name) as var\n"
    "    extern \"xl6_xrt_dynamic_fields_copy\" func copy(string name) as var\n"
    "    extern \"xl6_xrt_dynamic_fields_set\" func set(string name, var value) as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_remove\" func remove(string name) as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_take\" func take(string name) as var\n"
    "    extern \"xl6_xrt_dynamic_fields_keys\" func keys() as var\n"
    "    extern \"xl6_xrt_dynamic_fields_values\" func values() as var\n"
    "    extern \"xl6_xrt_dynamic_fields_items\" func items() as var\n"
    "    extern \"xl6_xrt_dynamic_fields_to_value\" func toValue() as var\n"
    "    extern \"xl6_xrt_dynamic_fields_merge\" func mergeNative(point source, bool replace) as bool\n"
    "    extern \"xl6_xrt_dynamic_fields_clone\" func cloneNative() as point\n"
    "\n"
    "    get length() as int\n"
    "        return self.countNative()\n"
    "    end\n"
    "\n"
    "    get capacity() as int\n"
    "        return self.capacityNative()\n"
    "    end\n"
    "\n"
    "    func merge(DynamicFields source, bool replace = true) as bool\n"
    "        return self.mergeNative(source.nativeHandle(), replace)\n"
    "    end\n"
    "\n"
    "    func clone() as DynamicFields\n"
    "        return DynamicFields(self.cloneNative())\n"
    "    end\n"
    "\n"
    "    func __item__(string name) as var\n"
    "        return self.get(name)\n"
    "    end\n"
    "\n"
    "    func __setItem__(string name, var value)\n"
    "        self.set(name, value)\n"
    "    end\n"
    "end\n"
    "\n"
    /* Keep the hidden implementation field nominally typed.  Erasing it to
     * `var` loses the DynamicFields method target before XLIR is built and
     * forces the backend to rediscover a bound method from runtime spelling.
     * The public object protocol still returns/accepts `var`; only this
     * implementation detail remains an owning native-handle value. */
    "func __xl6DynamicFieldsDefault() as DynamicFields\n"
    "    return DynamicFields()\n"
    "end\n"
    "\n"
    "class object\n"
    "    DynamicFields __fields = __xl6DynamicFieldsDefault()\n"
    "\n"
    "    func __field__(string name) as var\n"
    "        return self.__fields.get(name)\n"
    "    end\n"
    "\n"
    "    func __setField__(string name, var value)\n"
    "        self.__fields.set(name, value)\n"
    "    end\n"
    "\n"
    "    func __item__(string name) as var\n"
    "        return self.__fields.get(name)\n"
    "    end\n"
    "\n"
    "    func __setItem__(string name, var value)\n"
    "        self.__fields.set(name, value)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibRandomClassSource =
    "class Random\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_rng_create\" create(uint64 seed, uint64 seq)\n"
    "    extern \"xl6_xrt_rng_destroy\" destroy()\n"
    "    extern \"xl6_xrt_rng_seed\" func seed(uint64 seed, uint64 seq)\n"
    "    extern \"xl6_xrt_rng_u32\" func u32() as uint32\n"
    "    extern \"xl6_xrt_rng_u64\" func u64() as uint64\n"
    "    extern \"xl6_xrt_rng_range\" func rangeNative(c_int min, c_int max) as c_int\n"
    "    extern \"xl6_xrt_rng_string\" func stringNative(string chars, c_size size, c_size len) as point\n"
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
    "class BufferIter\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_buffer_iter_wrap_handle\" create(point handle)\n"
    "    extern \"xl6_xrt_buffer_iter_destroy\" destroy()\n"
    "    extern \"xl6_xrt_buffer_iter_has_next\" func hasNextNative() as bool\n"
    "    extern \"xl6_xrt_buffer_iter_next\" func nextNative() as int\n"
    "\n"
    "    func __next__() as (bool, int)\n"
    "        if not self.hasNextNative() then\n"
    "            return false, 0\n"
    "        end\n"
    "        return true, self.nextNative()\n"
    "    end\n"
    "end\n"
    "\n"
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
    "    extern \"xl6_xrt_buffer_append_raw\" func appendRaw(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_buffer_insert_string\" func insert(int index, string text) as bool\n"
    "    extern \"xl6_xrt_buffer_insert_raw\" func insertBytes(int index, point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_buffer_append_byte\" func appendByte(int value) as bool\n"
    "    extern \"xl6_xrt_buffer_byte_at\" func byteAt(int index) as int\n"
    "    extern \"xl6_xrt_buffer_set_byte\" func setByte(int index, int value) as bool\n"
    "    extern \"xl6_xrt_buffer_text\" func textNative() as point\n"
    "    extern \"xl6_xrt_buffer_data\" func data() as point\n"
    "    extern \"xl6_xrt_buffer_native_handle\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_buffer_iter_create\" func iterNative() as point\n"
    "\n"
    "    func append(Buffer other) as bool\n"
    "        return self.appendRaw(other.data(), c_size(other.len()))\n"
    "    end\n"
    "\n"
    "    func insert(int index, Buffer other) as bool\n"
    "        return self.insertBytes(index, other.data(), c_size(other.len()))\n"
    "    end\n"
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
    "\n"
    "    func __iter__() as BufferIter\n"
    "        return BufferIter(point(self.iterNative()))\n"
    "    end\n"
    "\n"
    "    func __eq__(Buffer other) as bool\n"
    "        if self.len() != other.len() then\n"
    "            return false\n"
    "        end\n"
    "        int i = 0\n"
        "        while i < self.len()\n"
    "            if self.byteAt(i) != other.byteAt(i) then\n"
    "                return false\n"
    "            end\n"
    "            i += 1\n"
    "        end\n"
    "        return true\n"
    "    end\n"
    "\n"
    "    func __ne__(Buffer other) as bool\n"
    "        return not self.__eq__(other)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibXidClassSource =
    "class Xid\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_xid_create\" create()\n"
    "    extern \"xl6_xrt_xid_parse\" create(string encoded)\n"
    "    extern \"xl6_xrt_xid_destroy\" destroy()\n"
    "    extern \"xl6_xrt_xid_format\" func encodeNative() as point\n"
    "\n"
    "    get valid() as bool\n"
    "        return self.handle != null\n"
    "    end\n"
    "\n"
    "    func encode() as string\n"
    "        if not self.valid then\n"
    "            return \"\"\n"
    "        end\n"
    "        return __xl_std_runtime_take_cstr(self.encodeNative())\n"
    "    end\n"
    "\n"
    "    func equals(Xid other) as bool\n"
    "        return self.valid and other.valid and self.encode() == other.encode()\n"
    "    end\n"
    "\n"
    "    func __eq__(Xid other) as bool\n"
    "        return self.equals(other)\n"
    "    end\n"
    "\n"
    "    func __toString__() as string\n"
    "        return self.encode()\n"
    "    end\n"
    "end\n";

const char* xl6StdlibLoggerClassSource =
    "class LogSink\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_log_sink_wrap_handle\" create(point handle)\n"
    "    extern \"xl6_xrt_log_sink_destroy\" destroy()\n"
    "    extern \"xl6_xrt_log_sink_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_log_sink_name\" func nameNative() as point\n"
    "    extern \"xl6_xrt_log_sink_set_level\" func setLevel(int level)\n"
    "    extern \"xl6_xrt_log_sink_get_level\" func getLevel() as int\n"
    "\n"
    "    get name() as string\n"
    "        return __xl_std_runtime_take_cstr(self.nameNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class Logger\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_log_create\" create(string name)\n"
    "    extern \"xl6_xrt_log_wrap_handle\" create(point handle)\n"
    "    extern \"xl6_xrt_log_destroy\" destroy()\n"
    "    extern \"xl6_xrt_log_set_default\" func makeDefault() as bool\n"
    "    extern \"xl6_xrt_log_set_level\" func setLevel(int level)\n"
    "    extern \"xl6_xrt_log_get_level\" func getLevel() as int\n"
    "    extern \"xl6_xrt_log_add_console\" func addConsoleNative(int level, bool color, int format) as bool\n"
    "    extern \"xl6_xrt_log_add_file\" func addFileNative(string path, int level, int format) as bool\n"
    "    extern \"xl6_xrt_log_add_rolling_file\" func addRollingFileNative(string path, int maxSize, int maxBackup, int level, int format) as bool\n"
    "    extern \"xl6_xrt_log_console_sink\" func consoleSinkNative(int level, bool color, int format) as point\n"
    "    extern \"xl6_xrt_log_file_sink\" func fileSinkNative(string path, int level, int format) as point\n"
    "    extern \"xl6_xrt_log_rolling_file_sink\" func rollingFileSinkNative(string path, int maxSize, int maxBackup, int level, int format) as point\n"
    "    extern \"xl6_xrt_log_attach_sink\" func attachSinkNative(point sink) as bool\n"
    "    extern \"xl6_xrt_log_remove_sink\" func removeSinkNative(point sink) as bool\n"
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
    "    func consoleSink(int level = 0, bool color = true, int format = 0) as LogSink\n"
    "        return LogSink(self.consoleSinkNative(level, color, format))\n"
    "    end\n"
    "\n"
    "    func fileSink(string path, int level = 2, int format = 0) as LogSink\n"
    "        return LogSink(self.fileSinkNative(path, level, format))\n"
    "    end\n"
    "\n"
    "    func rollingFileSink(string path, int maxSize = 10485760, int maxBackup = 5, int level = 2, int format = 0) as LogSink\n"
    "        return LogSink(self.rollingFileSinkNative(path, maxSize, maxBackup, level, format))\n"
    "    end\n"
    "\n"
    "    func attachSink(LogSink sink) as bool\n"
    "        return self.attachSinkNative(sink.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func removeSink(LogSink sink) as bool\n"
    "        return self.removeSinkNative(sink.nativeHandle())\n"
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
    "    end\n";

const char* xl6StdlibLoggerCustomMethodSource =
    "\n"
    "    extern \"xl6_xrt_log_custom_sink\" func __xl_std_runtime_log_custom_sink(string name, func(var) handler, int level) as point\n"
    "\n"
    "    func customSink(string name, func(var) handler, int level = 0) as LogSink\n"
    "        return LogSink(self.__xl_std_runtime_log_custom_sink(name, handler, level))\n"
    "    end\n";

const char* xl6StdlibLoggerClassEndSource =
    "end\n";

const char* xl6StdlibSignalClassSource =
    "class SignalHandle\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_signal_handle_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_signal_handle_destroy\" destroy()\n"
    "    extern \"xl6_xrt_signal_handle_close\" func close() as bool\n"
    "    extern \"xl6_xrt_signal_handle_active\" get active() as bool\n"
    "    extern \"xl6_xrt_signal_handle_code\" get code() as int\n"
    "end\n";

const char* xl6StdlibTemplateClassSource =
    "class Template\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_template_parse\" create(string text, string open, string close)\n"
    "    extern \"xl6_xrt_template_wrap_handle\" create(point handle)\n"
    "    extern \"xl6_xrt_template_destroy\" destroy()\n"
    "    extern \"xl6_xrt_template_valid\" get valid() as bool\n"
    "    extern \"xl6_xrt_template_source\" func sourceNative() as point\n"
    "    extern \"xl6_xrt_template_node_count\" get nodeCount() as uint32\n"
    "    extern \"xl6_xrt_template_render\" func renderNative(var data) as point\n"
    "    extern \"xl6_xrt_template_error\" func error() as var\n"
    "    extern \"xl6_xrt_template_node\" func node(int index) as var\n"
    "\n"
    "    get source() as string\n"
    "        return __xl_std_runtime_take_cstr(self.sourceNative())\n"
    "    end\n"
    "\n"
    "    func render(var data = null) as string\n"
    "        return __xl_std_runtime_take_cstr(self.renderNative(data))\n"
    "    end\n"
    "end\n"
    "\n"
    "class TemplateEngine\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_template_engine_create\" create()\n"
    "    extern \"xl6_xrt_template_engine_destroy\" destroy()\n"
    "    extern \"xl6_xrt_template_engine_register_function\" func __xl_std_runtime_template_register_function(string name, func(array<var>) as var handler, int minArgs, int maxArgs) as bool\n"
    "    extern \"xl6_xrt_template_engine_register_statement\" func __xl_std_runtime_template_register_statement(string name, func(array<var>) as var handler, int minArgs, int maxArgs) as bool\n"
    "    extern \"xl6_xrt_template_engine_register_block\" func __xl_std_runtime_template_register_block(string name, func(array<var>) as bool handler, int minArgs, int maxArgs) as bool\n"
    "    extern \"xl6_xrt_template_engine_register_raw_block\" func __xl_std_runtime_template_register_raw_block(string name, func(array<var>, string) as string handler, int minArgs, int maxArgs) as bool\n"
    "    extern \"xl6_xrt_template_engine_compile\" func compileNative(string text, string open, string close) as point\n"
    "    extern \"xl6_xrt_template_engine_compile_file\" func compileFileNative(string path, string open, string close) as point\n"
    "    extern \"xl6_xrt_template_engine_error\" func error() as var\n"
    "\n"
    "    func registerFunction(string name, func(array<var>) as var handler, int minArgs = 0, int maxArgs = 65535) as bool\n"
    "        return self.__xl_std_runtime_template_register_function(name, handler, minArgs, maxArgs)\n"
    "    end\n"
    "\n"
    "    func registerStatement(string name, func(array<var>) as var handler, int minArgs = 0, int maxArgs = 65535) as bool\n"
    "        return self.__xl_std_runtime_template_register_statement(name, handler, minArgs, maxArgs)\n"
    "    end\n"
    "\n"
    "    func registerBlock(string name, func(array<var>) as bool handler, int minArgs = 0, int maxArgs = 65535) as bool\n"
    "        return self.__xl_std_runtime_template_register_block(name, handler, minArgs, maxArgs)\n"
    "    end\n"
    "\n"
    "    func registerRawBlock(string name, func(array<var>, string) as string handler, int minArgs = 0, int maxArgs = 65535) as bool\n"
    "        return self.__xl_std_runtime_template_register_raw_block(name, handler, minArgs, maxArgs)\n"
    "    end\n"
    "\n"
    "    func compile(string text, string open = \"{\", string close = \"}\") as Template\n"
    "        return Template(self.compileNative(text, open, close))\n"
    "    end\n"
    "\n"
    "    func compileFile(string path, string open = \"{\", string close = \"}\") as Template\n"
    "        return Template(self.compileFileNative(path, open, close))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibTaskGroupClassSource =
    "class TaskGroup\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_task_group_create\" create()\n"
    "    extern \"xl6_xrt_task_group_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_task_group_destroy\" destroy()\n"
    "    extern \"xl6_xrt_task_group_child\" func childNative() as point\n"
    "    extern \"xl6_xrt_task_group_add\" func add(var task) as bool\n"
    "    extern \"xl6_xrt_task_group_bind_parent\" func bindParent(var task) as bool\n"
    "    extern \"xl6_xrt_task_group_close\" func close()\n"
    "    extern \"xl6_xrt_task_group_cancel\" func cancel()\n"
    "    extern \"xl6_xrt_task_group_count\" func countNative() as c_int\n"
    "    extern \"xl6_xrt_task_group_reap\" func reapNative() as c_int\n"
    "    extern \"xl6_xrt_task_group_join\" func join() as bool\n"
    "    extern \"xl6_xrt_task_group_join_timeout\" func joinTimeout(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_task_group_join_until\" func joinUntil(int deadlineMs) as bool\n"
    "    extern \"xl6_xrt_task_group_wait\" func wait() as bool\n"
    "    extern \"xl6_xrt_task_group_wait_timeout\" func waitTimeout(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_task_group_wait_until\" func waitUntil(int deadlineMs) as bool\n"
    "    extern \"xl6_xrt_task_group_join_async\" func joinAsync() as future<null>\n"
    "\n"
    "    get valid() as bool\n"
    "        return self.handle != null\n"
    "    end\n"
    "\n"
    "    get count() as int\n"
    "        return int(self.countNative())\n"
    "    end\n"
    "\n"
    "    func child() as TaskGroup\n"
    "        return TaskGroup(self.childNative())\n"
    "    end\n"
    "\n"
    "    func reapCompleted() as int\n"
    "        return int(self.reapNative())\n"
    "    end\n"
    "end\n";

const char* xl6StdlibCryptoClassSource =
    "class Sha1\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha1_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha1_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha1_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha1_stream_hex\" func hexNative() as point\n"
    "    extern \"xl6_xrt_sha1_stream_update_bytes\" func updateBytesNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha1_stream_bytes\" func bytesNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func update(Buffer data) as bool\n"
    "        return self.updateBytesNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "\n"
    "    func bytes() as Buffer\n"
    "        return Buffer(point(self.bytesNative()))\n"
    "    end\n"
    "end\n"
    "\n"
    "class Sha256\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha256_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha256_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha256_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha256_stream_hex\" func hexNative() as point\n"
    "    extern \"xl6_xrt_sha256_stream_update_bytes\" func updateBytesNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha256_stream_bytes\" func bytesNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func update(Buffer data) as bool\n"
    "        return self.updateBytesNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "\n"
    "    func bytes() as Buffer\n"
    "        return Buffer(point(self.bytesNative()))\n"
    "    end\n"
    "end\n"
    "\n"
    "class Sha384\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha384_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha384_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha384_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha384_stream_hex\" func hexNative() as point\n"
    "    extern \"xl6_xrt_sha384_stream_update_bytes\" func updateBytesNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha384_stream_bytes\" func bytesNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func update(Buffer data) as bool\n"
    "        return self.updateBytesNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "\n"
    "    func bytes() as Buffer\n"
    "        return Buffer(point(self.bytesNative()))\n"
    "    end\n"
    "end\n"
    "\n"
    "class Sha512\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_sha512_stream_create\" create()\n"
    "    extern \"xl6_xrt_sha512_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_sha512_stream_update\" func updateNative(string text, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha512_stream_hex\" func hexNative() as point\n"
    "    extern \"xl6_xrt_sha512_stream_update_bytes\" func updateBytesNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_sha512_stream_bytes\" func bytesNative() as point\n"
    "\n"
    "    func update(string text) as bool\n"
    "        return self.updateNative(text, __xl_std_crt_strlen(text))\n"
    "    end\n"
    "\n"
    "    func update(Buffer data) as bool\n"
    "        return self.updateBytesNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func hex() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hexNative())\n"
    "    end\n"
    "\n"
    "    func bytes() as Buffer\n"
    "        return Buffer(point(self.bytesNative()))\n"
    "    end\n"
    "end\n";


const char* xl6StdlibNetAddressClassSource =
    "class NetAddress\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_address_create\" create()\n"
    "    extern \"xl6_xrt_net_address_resolve\" create(string host, int port)\n"
    "    extern \"xl6_xrt_net_address_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_address_text\" func textNative() as point\n"
    "    extern \"xl6_xrt_net_address_family\" get family() as int\n"
    "    extern \"xl6_xrt_net_address_port\" get port() as int\n"
    "    extern \"xl6_xrt_net_address_scope\" get scope() as int\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class NetAddressList\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_address_list_resolve\" create(string host, int port, int family)\n"
    "    extern \"xl6_xrt_net_address_list_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_address_list_count\" func count() as int\n"
    "    extern \"xl6_xrt_net_address_list_text\" func textNative(int index) as point\n"
    "    extern \"xl6_xrt_net_address_list_family\" func family(int index) as int\n"
    "    extern \"xl6_xrt_net_address_list_port\" func port(int index) as int\n"
    "\n"
    "    func text(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative(index))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetInterfaceClassSource =
    "class NetInterfaceAddress\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_interface_address_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_net_interface_address_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_interface_address_text\" func textNative() as point\n"
    "    extern \"xl6_xrt_net_interface_address_family\" get family() as int\n"
    "    extern \"xl6_xrt_net_interface_address_prefix\" get prefixLength() as int\n"
    "    extern \"xl6_xrt_net_interface_address_scope\" get scope() as int\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "\n"
    "    get prefixKnown() as bool\n"
    "        return self.prefixLength != 255\n"
    "    end\n"
    "end\n"
    "\n"
    "class NetInterface\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_interface_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_net_interface_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_interface_name\" func nameNative() as point\n"
    "    extern \"xl6_xrt_net_interface_display_name\" func displayNameNative() as point\n"
    "    extern \"xl6_xrt_net_interface_ipv4_index\" get ipv4Index() as int\n"
    "    extern \"xl6_xrt_net_interface_ipv6_index\" get ipv6Index() as int\n"
    "    extern \"xl6_xrt_net_interface_flags\" get flags() as int\n"
    "    extern \"xl6_xrt_net_interface_mtu\" get mtu() as int\n"
    "    extern \"xl6_xrt_net_interface_hardware\" func hardwareNative() as point\n"
    "    extern \"xl6_xrt_net_interface_address_count\" func addressCount() as int\n"
    "    extern \"xl6_xrt_net_interface_address\" func addressNative(int index) as point\n"
    "    extern \"xl6_xrt_net_interface_has_flag\" func hasFlag(int flag) as bool\n"
    "\n"
    "    func name() as string\n"
    "        return __xl_std_runtime_take_cstr(self.nameNative())\n"
    "    end\n"
    "\n"
    "    func displayName() as string\n"
    "        return __xl_std_runtime_take_cstr(self.displayNameNative())\n"
    "    end\n"
    "\n"
    "    func hardwareAddress() as Buffer\n"
    "        return Buffer(point(self.hardwareNative()))\n"
    "    end\n"
    "\n"
    "    func address(int index) as NetInterfaceAddress\n"
    "        return NetInterfaceAddress(point(self.addressNative(index)))\n"
    "    end\n"
    "\n"
    "    get isUp() as bool\n"
    "        return self.hasFlag(1)\n"
    "    end\n"
    "\n"
    "    get isRunning() as bool\n"
    "        return self.hasFlag(2)\n"
    "    end\n"
    "\n"
    "    get isLoopback() as bool\n"
    "        return self.hasFlag(4)\n"
    "    end\n"
    "\n"
    "    get supportsBroadcast() as bool\n"
    "        return self.hasFlag(8)\n"
    "    end\n"
    "\n"
    "    get isPointToPoint() as bool\n"
    "        return self.hasFlag(16)\n"
    "    end\n"
    "\n"
    "    get supportsMulticast() as bool\n"
    "        return self.hasFlag(32)\n"
    "    end\n"
    "end\n"
    "\n"
    "class NetInterfaceList\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_interfaces_create\" create()\n"
    "    extern \"xl6_xrt_net_interfaces_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_interfaces_count\" func count() as int\n"
    "    extern \"xl6_xrt_net_interfaces_get\" func getNative(int index) as point\n"
    "\n"
    "    func get(int index) as NetInterface\n"
    "        return NetInterface(point(self.getNative(index)))\n"
    "    end\n"
    "\n"
    "    func __len__() as int\n"
    "        return self.count()\n"
    "    end\n"
    "\n"
    "    func __item__(int index) as NetInterface\n"
    "        return self.get(index)\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetBytesClassSource =
    "class NetBytes\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_bytes_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_bytes_len\" func len() as int\n"
    "    extern \"xl6_xrt_net_bytes_data\" func data() as point\n"
    "    extern \"xl6_xrt_net_bytes_buffer\" func bufferNative() as point\n"
    "    extern \"xl6_xrt_net_bytes_text\" func textNative() as point\n"
    "\n"
    "    func buffer() as Buffer\n"
    "        return Buffer(point(self.bufferNative()))\n"
    "    end\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetFrameClassSource =
    "class NetFrame\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_frame_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_net_frame_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_frame_status\" get status() as int\n"
    "    extern \"xl6_xrt_net_frame_payload_offset\" get payloadOffset() as int\n"
    "    extern \"xl6_xrt_net_frame_payload_size\" get payloadSize() as int\n"
    "    extern \"xl6_xrt_net_frame_frame_size\" get frameSize() as int\n"
    "    extern \"xl6_xrt_net_frame_declared\" get declared() as uint64\n"
    "    extern \"xl6_xrt_net_frame_payload\" func payloadNative() as point\n"
    "    extern \"xl6_xrt_net_frame_take_payload\" func takePayloadNative() as point\n"
    "    extern \"xl6_xrt_net_frame_text\" func textNative() as point\n"
    "    extern \"xl6_xrt_net_frame_error_kind\" get errorKind() as int\n"
    "    extern \"xl6_xrt_net_frame_error_code\" get errorCode() as int\n"
    "    extern \"xl6_xrt_net_frame_error_domain\" func errorDomainNative() as point\n"
    "    extern \"xl6_xrt_net_frame_error_operation\" func errorOperationNative() as point\n"
    "    extern \"xl6_xrt_net_frame_error_message\" func errorMessageNative() as point\n"
    "\n"
    "    get ready() as bool\n"
    "        return self.status == 1\n"
    "    end\n"
    "\n"
    "    get more() as bool\n"
    "        return self.status == 0\n"
    "    end\n"
    "\n"
    "    get failed() as bool\n"
    "        return self.status == -1\n"
    "    end\n"
    "\n"
    "    func payload() as Buffer\n"
    "        return Buffer(point(self.payloadNative()))\n"
    "    end\n"
    "\n"
    "    func takePayload() as Buffer\n"
    "        return Buffer(point(self.takePayloadNative()))\n"
    "    end\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "\n"
    "    func errorDomain() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorDomainNative())\n"
    "    end\n"
    "\n"
    "    func errorOperation() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorOperationNative())\n"
    "    end\n"
    "\n"
    "    func errorMessage() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorMessageNative())\n"
    "    end\n"
    "end\n";



const char* xl6StdlibLineFramerClassSource =
    "class LineFramer\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_framer_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_net_framer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_framer_next\" func nextNative(bool consume) as point\n"
    "    extern \"xl6_xrt_net_framer_recv\" func recvNative(int timeoutMs) as point\n"
    "    extern \"xl6_xrt_net_framer_reset\" func reset() as bool\n"
    "\n"
    "    func next(bool consume = true) as NetFrame\n"
    "        return NetFrame(point(self.nextNative(consume)))\n"
    "    end\n"
    "\n"
    "    func recv(int timeoutMs = -1) as NetFrame\n"
    "        return NetFrame(point(self.recvNative(timeoutMs)))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibLengthFramerClassSource =
    "class LengthFramer\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_framer_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_net_framer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_framer_next\" func nextNative(bool consume) as point\n"
    "    extern \"xl6_xrt_net_framer_recv\" func recvNative(int timeoutMs) as point\n"
    "\n"
    "    func next(bool consume = true) as NetFrame\n"
    "        return NetFrame(point(self.nextNative(consume)))\n"
    "    end\n"
    "\n"
    "    func recv(int timeoutMs = -1) as NetFrame\n"
    "        return NetFrame(point(self.recvNative(timeoutMs)))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetEngineStatsClassSource =
    "class NetEngineStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_stats_value\" func value(int index) as uint64\n"
    "\n"
    "    get state() as int\n"
    "        return int(self.value(0))\n"
    "    end\n"
    "\n"
    "    get workers() as uint64\n"
    "        return self.value(1)\n"
    "    end\n"
    "\n"
    "    get postsAccepted() as uint64\n"
    "        return self.value(2)\n"
    "    end\n"
    "\n"
    "    get postsRejected() as uint64\n"
    "        return self.value(3)\n"
    "    end\n"
    "\n"
    "    get postsExecuted() as uint64\n"
    "        return self.value(4)\n"
    "    end\n"
    "\n"
    "    get timersAccepted() as uint64\n"
    "        return self.value(5)\n"
    "    end\n"
    "\n"
    "    get timersRejected() as uint64\n"
    "        return self.value(6)\n"
    "    end\n"
    "\n"
    "    get timersFired() as uint64\n"
    "        return self.value(7)\n"
    "    end\n"
    "\n"
    "    get timersCancelled() as uint64\n"
    "        return self.value(8)\n"
    "    end\n"
    "\n"
    "    get timersClosed() as uint64\n"
    "        return self.value(9)\n"
    "    end\n"
    "\n"
    "    get timerErrors() as uint64\n"
    "        return self.value(10)\n"
    "    end\n"
    "\n"
    "    get events() as uint64\n"
    "        return self.value(11)\n"
    "    end\n"
    "\n"
    "    get waitErrors() as uint64\n"
    "        return self.value(12)\n"
    "    end\n"
    "\n"
    "    get nodeCacheHits() as uint64\n"
    "        return self.value(13)\n"
    "    end\n"
    "\n"
    "    get nodeCacheMisses() as uint64\n"
    "        return self.value(14)\n"
    "    end\n"
    "\n"
    "    get pendingCommands() as uint64\n"
    "        return self.value(15)\n"
    "    end\n"
    "\n"
    "    get activeTimers() as uint64\n"
    "        return self.value(16)\n"
    "    end\n"
    "\n"
    "    get nodeCachedBytes() as uint64\n"
    "        return self.value(17)\n"
    "    end\n"
    "\n"
    "    get liveObjects() as uint64\n"
    "        return self.value(18)\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetResolverStatsClassSource =
    "class NetResolverStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_stats_value\" func value(int index) as uint64\n"
    "\n"
    "    get workers() as uint64\n"
    "        return self.value(0)\n"
    "    end\n"
    "\n"
    "    get submitted() as uint64\n"
    "        return self.value(1)\n"
    "    end\n"
    "\n"
    "    get rejected() as uint64\n"
    "        return self.value(2)\n"
    "    end\n"
    "\n"
    "    get cacheHits() as uint64\n"
    "        return self.value(3)\n"
    "    end\n"
    "\n"
    "    get cacheMisses() as uint64\n"
    "        return self.value(4)\n"
    "    end\n"
    "\n"
    "    get coalesced() as uint64\n"
    "        return self.value(5)\n"
    "    end\n"
    "\n"
    "    get queriesStarted() as uint64\n"
    "        return self.value(6)\n"
    "    end\n"
    "\n"
    "    get resolved() as uint64\n"
    "        return self.value(7)\n"
    "    end\n"
    "\n"
    "    get failed() as uint64\n"
    "        return self.value(8)\n"
    "    end\n"
    "\n"
    "    get cancelled() as uint64\n"
    "        return self.value(9)\n"
    "    end\n"
    "\n"
    "    get outstanding() as uint64\n"
    "        return self.value(10)\n"
    "    end\n"
    "\n"
    "    get activeQueries() as uint64\n"
    "        return self.value(11)\n"
    "    end\n"
    "\n"
    "    get queuedQueries() as uint64\n"
    "        return self.value(12)\n"
    "    end\n"
    "\n"
    "    get runningQueries() as uint64\n"
    "        return self.value(13)\n"
    "    end\n"
    "\n"
    "    get readyCallbacks() as uint64\n"
    "        return self.value(14)\n"
    "    end\n"
    "\n"
    "    get cachedResults() as uint64\n"
    "        return self.value(15)\n"
    "    end\n"
    "end\n";



const char* xl6StdlibTcpStreamStatsClassSource =
    "class TcpStreamStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_stats_value\" func value(int index) as uint64\n"
    "\n"
    "    get state() as int\n"
    "        return int(self.value(0))\n"
    "    end\n"
    "\n"
    "    get receivedBytes() as uint64\n"
    "        return self.value(1)\n"
    "    end\n"
    "\n"
    "    get sentBytes() as uint64\n"
    "        return self.value(2)\n"
    "    end\n"
    "\n"
    "    get readEvents() as uint64\n"
    "        return self.value(3)\n"
    "    end\n"
    "\n"
    "    get writeEvents() as uint64\n"
    "        return self.value(4)\n"
    "    end\n"
    "\n"
    "    get sendRejected() as uint64\n"
    "        return self.value(5)\n"
    "    end\n"
    "\n"
    "    get bufferedBytes() as uint64\n"
    "        return self.value(6)\n"
    "    end\n"
    "\n"
    "    get queuedBytes() as uint64\n"
    "        return self.value(7)\n"
    "    end\n"
    "\n"
    "    get peakQueuedBytes() as uint64\n"
    "        return self.value(8)\n"
    "    end\n"
    "\n"
    "    get readPaused() as bool\n"
    "        return self.value(9) != 0\n"
    "    end\n"
    "\n"
    "    get readBlocked() as bool\n"
    "        return self.value(10) != 0\n"
    "    end\n"
    "\n"
    "    get readEnded() as bool\n"
    "        return self.value(11) != 0\n"
    "    end\n"
    "\n"
    "    get writeEnded() as bool\n"
    "        return self.value(12) != 0\n"
    "    end\n"
    "\n"
    "    get writeBackpressured() as bool\n"
    "        return self.value(13) != 0\n"
    "    end\n"
    "end\n";



const char* xl6StdlibTcpServerStatsClassSource =
    "class TcpServerStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_stats_value\" func value(int index) as uint64\n"
    "\n"
    "    get state() as int\n"
    "        return int(self.value(0))\n"
    "    end\n"
    "\n"
    "    get accepted() as uint64\n"
    "        return self.value(1)\n"
    "    end\n"
    "\n"
    "    get rejected() as uint64\n"
    "        return self.value(2)\n"
    "    end\n"
    "\n"
    "    get errors() as uint64\n"
    "        return self.value(3)\n"
    "    end\n"
    "\n"
    "    get activeAccepts() as uint64\n"
    "        return self.value(4)\n"
    "    end\n"
    "\n"
    "    get activeDispatches() as uint64\n"
    "        return self.value(5)\n"
    "    end\n"
    "\n"
    "    get queuedAccepts() as uint64\n"
    "        return self.value(6)\n"
    "    end\n"
    "\n"
    "    get peakQueuedAccepts() as uint64\n"
    "        return self.value(7)\n"
    "    end\n"
    "\n"
    "    get acceptWaiters() as uint64\n"
    "        return self.value(8)\n"
    "    end\n"
    "end\n";



const char* xl6StdlibUdpStatsClassSource =
    "class UdpStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_stats_value\" func value(int index) as uint64\n"
    "\n"
    "    get state() as int\n"
    "        return int(self.value(0))\n"
    "    end\n"
    "\n"
    "    get receivedPackets() as uint64\n"
    "        return self.value(1)\n"
    "    end\n"
    "\n"
    "    get receivedBytes() as uint64\n"
    "        return self.value(2)\n"
    "    end\n"
    "\n"
    "    get sentPackets() as uint64\n"
    "        return self.value(3)\n"
    "    end\n"
    "\n"
    "    get sentBytes() as uint64\n"
    "        return self.value(4)\n"
    "    end\n"
    "\n"
    "    get truncated() as uint64\n"
    "        return self.value(5)\n"
    "    end\n"
    "\n"
    "    get truncatedDropped() as uint64\n"
    "        return self.value(6)\n"
    "    end\n"
    "\n"
    "    get droppedNewest() as uint64\n"
    "        return self.value(7)\n"
    "    end\n"
    "\n"
    "    get droppedOldest() as uint64\n"
    "        return self.value(8)\n"
    "    end\n"
    "\n"
    "    get receiveErrors() as uint64\n"
    "        return self.value(9)\n"
    "    end\n"
    "\n"
    "    get sendErrors() as uint64\n"
    "        return self.value(10)\n"
    "    end\n"
    "\n"
    "    get sendRejected() as uint64\n"
    "        return self.value(11)\n"
    "    end\n"
    "\n"
    "    get queuedBytes() as uint64\n"
    "        return self.value(16)\n"
    "    end\n"
    "\n"
    "    get peakQueuedBytes() as uint64\n"
    "        return self.value(17)\n"
    "    end\n"
    "\n"
    "    get queuedPackets() as uint64\n"
    "        return self.value(18)\n"
    "    end\n"
    "\n"
    "    get peakQueuedPackets() as uint64\n"
    "        return self.value(19)\n"
    "    end\n"
    "\n"
    "    get receiveQueued() as uint64\n"
    "        return self.value(20)\n"
    "    end\n"
    "\n"
    "    get peakReceiveQueued() as uint64\n"
    "        return self.value(21)\n"
    "    end\n"
    "\n"
    "    get receiveWaiters() as uint64\n"
    "        return self.value(28)\n"
    "    end\n"
    "\n"
    "    get activeReceives() as uint64\n"
    "        return self.value(30)\n"
    "    end\n"
    "\n"
    "    get activeSends() as uint64\n"
    "        return self.value(31)\n"
    "    end\n"
    "\n"
    "    get peakActiveSends() as uint64\n"
    "        return self.value(32)\n"
    "    end\n"
    "\n"
    "    get connected() as bool\n"
    "        return self.value(33) != 0\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetProxyClassSource =
    "class NetProxy\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_proxy_create\" create(int type, string host, int port, string user, string password)\n"
    "    extern \"xl6_xrt_net_proxy_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_proxy_type\" get type() as int\n"
    "    extern \"xl6_xrt_net_proxy_port\" get port() as int\n"
    "    extern \"xl6_xrt_net_proxy_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_net_proxy_host\" func hostNative() as point\n"
    "    extern \"xl6_xrt_net_proxy_user\" func userNative() as point\n"
    "\n"
    "    func host() as string\n"
    "        return __xl_std_runtime_take_cstr(self.hostNative())\n"
    "    end\n"
    "\n"
    "    func user() as string\n"
    "        return __xl_std_runtime_take_cstr(self.userNative())\n"
    "    end\n"
    "end\n";



const char* xl6StdlibNetEngineClassSource =
    "class NetEngine\n"
    "    native point handle\n"
	"    extern \"xl6_xrt_net_engine_create\" create(int workerCount)\n"
    "    extern \"xl6_xrt_net_engine_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_engine_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_net_engine_start\" func start() as bool\n"
    "    extern \"xl6_xrt_net_engine_stop\" func stop()\n"
    "    extern \"xl6_xrt_net_engine_workers\" get workerCount() as int\n"
    "    extern \"xl6_xrt_net_engine_stats\" func statsNative() as point\n"
    "\n"
    "    func post(func() task, int affinity = 0) as bool\n"
    "        return __xl_std_runtime_net_engine_post(self.nativeHandle(), task, affinity)\n"
    "    end\n"
    "\n"
    "    func postDelayed(func() task, int delayMs, int affinity = 0) as bool\n"
    "        return __xl_std_runtime_net_engine_post_delayed(self.nativeHandle(), task, delayMs, affinity)\n"
    "    end\n"
    "\n"
    "    func stats() as NetEngineStats\n"
    "        return NetEngineStats(point(self.statsNative()))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibTcpStreamClassSource =
    "class TcpStream\n"
    "    native point handle\n"
	"    extern \"xl6_xrt_tcp_stream_destroy\" destroy()\n"
	"    extern \"xl6_xrt_tcp_stream_native\" func nativeHandle() as point\n"
	"    extern \"xl6_xrt_tcp_stream_close\" func close()\n"
	"    extern \"xl6_xrt_tcp_stream_shutdown_write\" func shutdownWrite() as bool\n"
	"    extern \"xl6_xrt_tcp_stream_abort\" func abort() as bool\n"
	"    extern \"xl6_xrt_tcp_stream_send_text\" func send(string text) as bool\n"
	"    extern \"xl6_xrt_tcp_stream_send_text_result\" func sendResult(string text) as int\n"
	"    extern \"xl6_xrt_tcp_stream_available\" func availableNative() as c_size\n"
	"    extern \"xl6_xrt_tcp_stream_read_text\" func readNative(c_size maxBytes) as point\n"
	"    extern \"xl6_xrt_tcp_stream_recv_text\" func recvNative(c_size maxBytes, int timeoutMs) as point\n"
	"    extern \"xl6_xrt_tcp_stream_send_bytes\" func sendBytesRaw(point data, c_size size) as bool\n"
	"    extern \"xl6_xrt_tcp_stream_send_bytes_result\" func sendBytesResult(point data, c_size size) as int\n"
    "    extern \"xl6_xrt_tcp_stream_read_buffer\" func readBytesNative(c_size maxBytes) as point\n"
    "    extern \"xl6_xrt_tcp_stream_recv_buffer\" func recvBytesNative(c_size maxBytes, int timeoutMs) as point\n"
	"    extern \"xl6_xrt_tcp_stream_recv_text_async\" func recvAsyncNative(c_size maxBytes) as future<string>\n"
	"    extern \"xl6_xrt_tcp_stream_recv_owned\" func recvOwnedNative(c_size maxBytes, int timeoutMs) as point\n"
	"    extern \"xl6_xrt_tcp_stream_recv_owned_async\" func recvBytesAsyncNative(c_size maxBytes) as future<NetBytes>\n"
	"    extern \"xl6_xrt_tcp_stream_peek_buffer\" func peekNative(int maxBytes) as point\n"
	"    extern \"xl6_xrt_tcp_stream_consume_exact\" func consumeNative(int size) as bool\n"
	"    extern \"xl6_xrt_tcp_stream_wait_available\" func waitAvailable(int minimum, int timeoutMs) as bool\n"
	"    extern \"xl6_xrt_tcp_stream_wait_available_async\" func waitAvailableAsync(int minimum) as future<bool>\n"
	"    extern \"xl6_xrt_tcp_stream_drain\" func drainNative(int timeoutMs) as bool\n"
	"    extern \"xl6_xrt_tcp_stream_drain_async\" func drainAsync() as future<bool>\n"
	"    extern \"xl6_xrt_tcp_stream_writable_async\" func writableAsync() as future<bool>\n"
	"    extern \"xl6_xrt_tcp_stream_local_addr\" func localAddrNative() as point\n"
	"    extern \"xl6_xrt_tcp_stream_remote_addr\" func remoteAddrNative() as point\n"
	"    extern \"xl6_xrt_tcp_stream_local_port\" func localPort() as int\n"
	"    extern \"xl6_xrt_tcp_stream_remote_port\" func remotePort() as int\n"
    "    extern \"xl6_xrt_net_stream_pause\" func pauseRead()\n"
    "    extern \"xl6_xrt_net_stream_resume\" func resumeRead()\n"
    "    extern \"xl6_xrt_net_stream_pending_send\" func pendingSend() as int\n"
    "    extern \"xl6_xrt_net_stream_write_limit\" func writeLimit() as int\n"
    "    extern \"xl6_xrt_net_stream_writable\" func writable() as int\n"
    "    extern \"xl6_xrt_net_stream_wait\" func waitNative(int kind, int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_net_stream_send_vec\" func sendVec(array buffers) as bool\n"
	"    extern \"xl6_xrt_net_stream_send_move\" func sendMoveNative(point buffer) as bool\n"
    "    extern \"xl6_xrt_tcp_stream_stats\" func statsNative() as point\n"
    "\n"
    "    func available() as int\n"
    "        return int(self.availableNative())\n"
    "    end\n"
    "\n"
    "    func read(int maxBytes = 0) as string\n"
    "        return __xl_std_runtime_take_cstr(self.readNative(c_size(maxBytes)))\n"
    "    end\n"
    "\n"
    /*
        XRT 的零上限表示“取走当前全部缓冲”，适合底层批量读取，
        但不适合作为语言层同步 recv 的默认值：高并发下调用方需要
        稳定等待一段数据，而不是暴露底层缓冲快照语义。
    */
    "    func recv(int timeoutMs = -1, int maxBytes = 65536) as string\n"
    "        return __xl_std_runtime_take_cstr(self.recvNative(c_size(maxBytes), timeoutMs))\n"
    "    end\n"
    "\n"
    "    func sendBytes(point data, c_size size) as bool\n"
    "        return self.sendBytesRaw(data, size)\n"
    "    end\n"
    "\n"
    "    func readBytes(int maxBytes = 0) as Buffer\n"
    "        point handle = self.readBytesNative(c_size(maxBytes))\n"
    "        return Buffer(point(handle))\n"
    "    end\n"
    "\n"
    "    func recvBytes(int timeoutMs = -1, int maxBytes = 65536) as Buffer\n"
    "        point handle = self.recvBytesNative(c_size(maxBytes), timeoutMs)\n"
    "        return Buffer(point(handle))\n"
    "    end\n"
    "\n"
	"    func recvOwned(int timeoutMs = -1, int maxBytes = 65536) as NetBytes\n"
	"        return NetBytes(point(self.recvOwnedNative(c_size(maxBytes), timeoutMs)))\n"
	"    end\n"
	"\n"
	"    func recvBytesAsync(int maxBytes = 65536) as future<NetBytes>\n"
	"        return self.recvBytesAsyncNative(c_size(maxBytes))\n"
	"    end\n"
	"\n"
	"    func peek(int maxBytes = 0) as Buffer\n"
	"        return Buffer(point(self.peekNative(maxBytes)))\n"
	"    end\n"
	"\n"
	"    func consume(int size) as bool\n"
	"        return self.consumeNative(size)\n"
	"    end\n"
	"\n"
	"    func recvAsync(int maxBytes = 65536) as future<string>\n"
	"        return self.recvAsyncNative(c_size(maxBytes))\n"
    "    end\n"
    "\n"
    "    func drain(int timeoutMs = -1) as bool\n"
    "        return self.drainNative(timeoutMs)\n"
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
    "    func sendMove(Buffer data) as bool\n"
    "        return self.sendMoveNative(data.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func waitReadable(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(0, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitWritable(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(1, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitDrain(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(2, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitClose(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(3, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func stats() as TcpStreamStats\n"
    "        return TcpStreamStats(point(self.statsNative()))\n"
    "    end\n";



const char* xl6StdlibTcpStreamFileMethodSource =
    "    extern \"xl6_xrt_tcp_stream_send_file_result\" func sendFileResultNative(string path, int offset, int size) as int\n"
    "    extern \"xl6_xrt_tcp_stream_send_file_handle_result\" func sendFileHandleResultNative(point file, int offset, int size) as int\n"
    "    extern \"xl6_xrt_tcp_stream_send_file\" func sendFileNative(string path, int offset, int size, int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_tcp_stream_send_file_handle\" func sendFileHandleNative(point file, int offset, int size, int timeoutMs) as bool\n"
    "\n"
    "    func sendFileResult(string path, int offset = 0, int size = -1) as int\n"
    "        return self.sendFileResultNative(path, offset, size)\n"
    "    end\n"
    "\n"
    "    func sendFile(string path, int offset = 0, int size = -1, int timeoutMs = -1) as bool\n"
    "        return self.sendFileNative(path, offset, size, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func sendFileHandleResult(point file, int offset = 0, int size = -1) as int\n"
    "        return self.sendFileHandleResultNative(file, offset, size)\n"
    "    end\n"
    "\n"
    "    func sendFileHandle(point file, int offset = 0, int size = -1, int timeoutMs = -1) as bool\n"
    "        return self.sendFileHandleNative(file, offset, size, timeoutMs)\n"
    "    end\n";



const char* xl6StdlibTcpStreamLineFrameMethodSource =
    "    extern \"xl6_xrt_net_line_framer_create_text\" func lineFramerTextNative(string delimiter, int maxPayload, bool includeDelimiter) as point\n"
    "    extern \"xl6_xrt_net_line_framer_create_bytes\" func lineFramerBytesNative(point data, c_size size, int maxPayload, bool includeDelimiter) as point\n"
    "\n"
    "    func lineFramer(string delimiter = \"\\n\", int maxPayload = 8192, bool includeDelimiter = false) as LineFramer\n"
    "        return LineFramer(point(self.lineFramerTextNative(delimiter, maxPayload, includeDelimiter)))\n"
    "    end\n"
    "\n"
    "    func lineFramer(Buffer delimiter, int maxPayload = 8192, bool includeDelimiter = false) as LineFramer\n"
    "        return LineFramer(point(self.lineFramerBytesNative(delimiter.data(), c_size(delimiter.len()), maxPayload, includeDelimiter)))\n"
    "    end\n";



const char* xl6StdlibTcpStreamLengthFrameMethodSource =
    "    extern \"xl6_xrt_net_length_framer_create\" func lengthFramerNative(int lengthOffset, int lengthSize, int adjustment, int strip, int maxFrame, bool littleEndian) as point\n"
    "\n"
    "    func lengthFramer(int lengthOffset = 0, int lengthSize = 4, int adjustment = 0, int strip = 4, int maxFrame = 1048576, bool littleEndian = false) as LengthFramer\n"
    "        return LengthFramer(point(self.lengthFramerNative(lengthOffset, lengthSize, adjustment, strip, maxFrame, littleEndian)))\n"
    "    end\n";



const char* xl6StdlibTcpStreamClassEndSource =
    "end\n";



const char* xl6StdlibTcpServerClassSource =
    "class TcpServer\n"
    "    native point handle\n"
	"    extern \"xl6_xrt_tcp_listener_destroy\" destroy()\n"
	"    extern \"xl6_xrt_tcp_listener_port\" func port() as uint16\n"
	"    extern \"xl6_xrt_tcp_listener_close\" func close() as bool\n"
	"    extern \"xl6_xrt_tcp_listener_state\" get state() as int\n"
	"    extern \"xl6_xrt_tcp_accept\" func acceptNative(int timeoutMs) as point\n"
	"    extern \"xl6_xrt_tcp_accept_async\" func acceptAsyncNative() as future<TcpStream>\n"
    "    extern \"xl6_xrt_tcp_listener_stats\" func statsNative() as point\n"
    "\n"
    "    func accept(int timeoutMs = -1) as TcpStream\n"
    "        return TcpStream(point(self.acceptNative(timeoutMs)))\n"
    "    end\n"
    "\n"
	"    func acceptAsync() as future<TcpStream>\n"
	"        return self.acceptAsyncNative()\n"
	"    end\n"
    "\n"
    "    func stats() as TcpServerStats\n"
    "        return TcpServerStats(point(self.statsNative()))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibUdpClassSource =
    "class UdpPacket\n"
    "    native point handle\n"
	"    extern \"xl6_xrt_udp_packet_destroy\" destroy()\n"
	"    extern \"xl6_xrt_udp_packet_text\" func textNative() as point\n"
    "    extern \"xl6_xrt_udp_packet_buffer\" func bytesNative() as point\n"
	"    extern \"xl6_xrt_udp_packet_addr\" func addrNative() as point\n"
	"    extern \"xl6_xrt_udp_packet_port\" func port() as int\n"
	"    extern \"xl6_xrt_udp_packet_truncated\" func truncated() as bool\n"
    "\n"
    "    func text() as string\n"
    "        return __xl_std_runtime_take_cstr(self.textNative())\n"
    "    end\n"
    "\n"
    "    func bytes() as Buffer\n"
    "        point handle = self.bytesNative()\n"
    "        return Buffer(point(handle))\n"
    "    end\n"
    "\n"
    "    func addr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.addrNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class UdpBatch\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_udp_batch_destroy\" destroy()\n"
    "    extern \"xl6_xrt_udp_batch_count\" func count() as int\n"
    "    extern \"xl6_xrt_udp_batch_take\" func takeNative(int index) as point\n"
    "\n"
    "    func take(int index) as UdpPacket\n"
    "        return UdpPacket(point(self.takeNative(index)))\n"
    "    end\n"
    "end\n"
    "\n"
    "class UdpSocket\n"
    "    native point handle\n"
	"    extern \"xl6_xrt_udp_destroy\" destroy()\n"
	"    extern \"xl6_xrt_udp_local_port\" func port() as int\n"
	"    extern \"xl6_xrt_udp_local_addr\" func localAddrNative() as point\n"
	"    extern \"xl6_xrt_udp_send_text\" func send(string host, uint16 port, string text) as bool\n"
	"    extern \"xl6_xrt_udp_send_bytes\" func sendBytesRaw(string host, uint16 port, point data, c_size size) as bool\n"
	"    extern \"xl6_xrt_udp_send_bytes_result\" func sendBytesResult(string host, uint16 port, point data, c_size size) as int\n"
	"    extern \"xl6_xrt_udp_send_vec\" func sendVec(string host, uint16 port, array buffers) as bool\n"
	"    extern \"xl6_xrt_udp_send_move\" func sendMoveNative(string host, uint16 port, point buffer) as bool\n"
	"    extern \"xl6_xrt_udp_send_batch\" func sendBatch(string host, uint16 port, array buffers) as int\n"
	"    extern \"xl6_xrt_udp_recv\" func recvNative(int timeoutMs) as point\n"
	"    extern \"xl6_xrt_udp_recv_async\" func recvAsyncNative() as future<UdpPacket>\n"
	"    extern \"xl6_xrt_udp_recv_batch\" func recvBatchNative(c_size capacity, int timeoutMs) as point\n"
	"    extern \"xl6_xrt_udp_recv_batch_async\" func recvBatchAsyncNative(c_size capacity) as future<UdpBatch>\n"
	"    extern \"xl6_xrt_udp_pending\" func pendingSend() as int\n"
	"    extern \"xl6_xrt_udp_queued\" func queuedPackets() as int\n"
	"    extern \"xl6_xrt_udp_queued_bytes\" func queuedBytes() as int\n"
	"    extern \"xl6_xrt_udp_writable\" func waitWritableNative(c_size size, int timeoutMs) as bool\n"
	"    extern \"xl6_xrt_udp_writable_async\" func writableAsync(c_size size) as future<bool>\n"
    "    extern \"xl6_xrt_udp_stats\" func statsNative() as point\n"
    "\n"
    "    func localAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.localAddrNative())\n"
    "    end\n"
    "\n"
    "    func recv(int timeoutMs = -1) as UdpPacket\n"
    "        return UdpPacket(point(self.recvNative(timeoutMs)))\n"
    "    end\n"
    "\n"
    "    func sendBytes(string host, uint16 port, point data, c_size size) as bool\n"
    "        return self.sendBytesRaw(host, port, data, size)\n"
    "    end\n"
    "\n"
	"    func sendMove(string host, uint16 port, Buffer data) as bool\n"
	"        return self.sendMoveNative(host, port, data.nativeHandle())\n"
	"    end\n"
	"\n"
	"    func waitWritable(c_size size, int timeoutMs = -1) as bool\n"
	"        return self.waitWritableNative(size, timeoutMs)\n"
	"    end\n"
	"\n"
	"    func recvAsync() as future<UdpPacket>\n"
	"        return self.recvAsyncNative()\n"
	"    end\n"
    "\n"
    "    func recvBatch(int capacity, int timeoutMs = -1) as UdpBatch\n"
    "        return UdpBatch(point(self.recvBatchNative(c_size(capacity), timeoutMs)))\n"
    "    end\n"
    "\n"
    "    func recvBatchAsync(int capacity) as future<UdpBatch>\n"
    "        return self.recvBatchAsyncNative(c_size(capacity))\n"
    "    end\n"
    "\n"
    "    func stats() as UdpStats\n"
    "        return UdpStats(point(self.statsNative()))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibTlsSessionClassSource =
    "class TlsResume\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tls_resume_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_tls_resume_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tls_resume_native\" func nativeHandle() as point\n"
    "end\n"
    "\n"
    "class TlsConfig\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tls_config_create\" create()\n"
    "    extern \"xl6_xrt_tls_config_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tls_config_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_tls_config_host\" func host(string name) as bool\n"
    "    extern \"xl6_xrt_tls_config_alpn\" func alpn(string protocols) as bool\n"
    "    extern \"xl6_xrt_tls_config_cert\" func certificate(string certFile, string keyFile) as bool\n"
    "    extern \"xl6_xrt_tls_config_ca\" func ca(string caFile) as bool\n"
    "    extern \"xl6_xrt_tls_config_crl\" func crl(string crlFile) as bool\n"
    "    extern \"xl6_xrt_tls_config_verify\" func verifyPeer(bool enabled)\n"
    "    extern \"xl6_xrt_tls_config_verify_callback\" func __xl_std_runtime_tls_config_verify_callback(func(var) as bool handler) as bool\n"
    "    extern \"xl6_xrt_tls_config_allow_ed25519\" func allowTls12Ed25519(bool enabled)\n"
    "    extern \"xl6_xrt_tls_config_max_version\" func maxVersion(int version)\n"
    "    extern \"xl6_xrt_tls_config_cert_data\" func certificateDataNative(point cert, c_size certSize, point key, c_size keySize) as bool\n"
    "    extern \"xl6_xrt_tls_config_ca_data\" func caDataNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_tls_config_crl_data\" func crlDataNative(point data, c_size size) as bool\n"
    "\n"
    "    func certificateData(Buffer cert, Buffer key) as bool\n"
    "        return self.certificateDataNative(cert.data(), c_size(cert.len()), key.data(), c_size(key.len()))\n"
    "    end\n"
    "\n"
    "    func caData(Buffer data) as bool\n"
    "        return self.caDataNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func crlData(Buffer data) as bool\n"
    "        return self.crlDataNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func verifyCallback(func(var) as bool handler) as bool\n"
    "        return self.__xl_std_runtime_tls_config_verify_callback(handler)\n"
    "    end\n"
    "\n"
    "    func session(bool server = false) as TlsSession\n"
    "        point noResume = null\n"
    "        return TlsSession(self.nativeHandle(), server, noResume)\n"
    "    end\n"
    "\n"
    "    func session(TlsResume resume, bool server = false) as TlsSession\n"
    "        return TlsSession(self.nativeHandle(), server, resume.nativeHandle())\n"
    "    end\n"
    "end\n"
    "\n"
    "class TlsSession\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tls_session_create\" create(point config, bool server, point resume)\n"
    "    extern \"xl6_xrt_tls_session_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tls_session_ready\" get ready() as bool\n"
    "    extern \"xl6_xrt_tls_session_last_status\" get status() as int\n"
    "    extern \"xl6_xrt_tls_session_version\" get version() as int\n"
    "    extern \"xl6_xrt_tls_session_cipher\" get cipher() as int\n"
    "    extern \"xl6_xrt_tls_session_pending_cipher\" get pendingCipher() as int\n"
    "    extern \"xl6_xrt_tls_session_pending_plain\" get pendingPlain() as int\n"
    "    extern \"xl6_xrt_tls_session_resumed\" get resumed() as bool\n"
    "    extern \"xl6_xrt_tls_session_drive\" func handshake() as int\n"
    "    extern \"xl6_xrt_tls_session_feed\" func feedBytes(point data, c_size size) as int\n"
    "    extern \"xl6_xrt_tls_session_peek_cipher\" func peekCipherNative(int maxBytes) as point\n"
    "    extern \"xl6_xrt_tls_session_consume_cipher\" func consumeCipher(int size)\n"
    "    extern \"xl6_xrt_tls_session_write_plain\" func writeBytes(point data, c_size size) as int\n"
    "    extern \"xl6_xrt_tls_session_read_plain\" func readNative(int maxBytes) as point\n"
    "    extern \"xl6_xrt_tls_session_close\" func close() as int\n"
    "    extern \"xl6_xrt_tls_session_sni\" func sniNative() as point\n"
    "    extern \"xl6_xrt_tls_session_alpn\" func alpnNative() as point\n"
    "    extern \"xl6_xrt_tls_session_peer_cert\" func peerCertNative() as point\n"
    "    extern \"xl6_xrt_tls_session_export_resume\" func exportResumeNative() as point\n"
    "    extern \"xl6_xrt_tls_session_set_cert\" func certificate(string certFile, string keyFile) as bool\n"
    "    extern \"xl6_xrt_tls_session_set_cert_data\" func certificateDataNative(point cert, c_size certSize, point key, c_size keySize) as bool\n"
    "    extern \"xl6_xrt_tls_session_allow_ed25519\" func allowTls12Ed25519(bool enabled)\n"
    "\n"
    "    func feed(Buffer data) as int\n"
    "        return self.feedBytes(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func peekCipher(int maxBytes = 0) as Buffer\n"
    "        return Buffer(point(self.peekCipherNative(maxBytes)))\n"
    "    end\n"
    "\n"
    "    func write(Buffer data) as int\n"
    "        return self.writeBytes(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func read(int maxBytes = 0) as Buffer\n"
    "        return Buffer(point(self.readNative(maxBytes)))\n"
    "    end\n"
    "\n"
    "    func sni() as string\n"
    "        return __xl_std_runtime_take_cstr(self.sniNative())\n"
    "    end\n"
    "\n"
    "    func alpn() as string\n"
    "        return __xl_std_runtime_take_cstr(self.alpnNative())\n"
    "    end\n"
    "\n"
    "    func peerCert() as Buffer\n"
    "        return Buffer(point(self.peerCertNative()))\n"
    "    end\n"
    "\n"
    "    func exportResume() as TlsResume\n"
    "        return TlsResume(point(self.exportResumeNative()))\n"
    "    end\n"
    "\n"
    "    func certificateData(Buffer cert, Buffer key) as bool\n"
    "        return self.certificateDataNative(cert.data(), c_size(cert.len()), key.data(), c_size(key.len()))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibTlsStreamClassSource =
    "class TlsStream\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tls_stream_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_tls_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tls_stream_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_tls_stream_close\" func close() as bool\n"
    "    extern \"xl6_xrt_tls_stream_abort\" func abort() as bool\n"
    "    extern \"xl6_xrt_tls_stream_state\" get state() as int\n"
    "    extern \"xl6_xrt_tls_stream_available\" get available() as int\n"
    "    extern \"xl6_xrt_tls_stream_pending\" get pending() as int\n"
    "    extern \"xl6_xrt_tls_stream_async_bytes\" get asyncBytes() as int\n"
    "    extern \"xl6_xrt_tls_stream_async_count\" get asyncCount() as int\n"
    "    extern \"xl6_xrt_tls_stream_send_text\" func sendNative(string text, int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_tls_stream_send_bytes\" func sendBytesNative(point data, c_size size, int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_tls_stream_send_text_async\" func sendAsync(string text) as future<bool>\n"
    "    extern \"xl6_xrt_tls_stream_send_bytes_async\" func sendBytesAsync(point data, c_size size) as future<bool>\n"
    "    extern \"xl6_xrt_tls_stream_send_vec_async\" func sendVecAsync(array buffers) as future<bool>\n"
    "    extern \"xl6_xrt_tls_stream_recv_owned\" func recvOwnedNative(c_size maxBytes, int timeoutMs) as point\n"
    "    extern \"xl6_xrt_tls_stream_recv_owned_async\" func recvBytesAsyncNative(c_size maxBytes) as future<NetBytes>\n"
    "    extern \"xl6_xrt_tls_stream_recv_text_async\" func recvAsyncNative(c_size maxBytes) as future<string>\n"
    "    extern \"xl6_xrt_tls_stream_peek_buffer\" func peekNative(int maxBytes) as point\n"
    "    extern \"xl6_xrt_tls_stream_consume_exact\" func consumeNative(int size) as bool\n"
    "    extern \"xl6_xrt_tls_stream_wait\" func waitNative(int kind, int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_tls_stream_wait_async\" func waitAsyncNative(int kind) as future<bool>\n"
    "    extern \"xl6_xrt_tls_stream_local_addr\" func localAddrNative() as point\n"
    "    extern \"xl6_xrt_tls_stream_remote_addr\" func remoteAddrNative() as point\n"
    "\n"
    "    func send(string text, int timeoutMs = -1) as bool\n"
    "        return self.sendNative(text, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func send(Buffer data, int timeoutMs = -1) as bool\n"
    "        return self.sendBytesNative(data.data(), c_size(data.len()), timeoutMs)\n"
    "    end\n"
    "\n"
    "    func sendBytesAsync(Buffer data) as future<bool>\n"
    "        return self.sendBytesAsync(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func recvOwned(int timeoutMs = -1, int maxBytes = 65536) as NetBytes\n"
    "        return NetBytes(point(self.recvOwnedNative(c_size(maxBytes), timeoutMs)))\n"
    "    end\n"
    "\n"
    "    func recvBytesAsync(int maxBytes = 65536) as future<NetBytes>\n"
    "        return self.recvBytesAsyncNative(c_size(maxBytes))\n"
    "    end\n"
    "\n"
    "    func recvAsync(int maxBytes = 65536) as future<string>\n"
    "        return self.recvAsyncNative(c_size(maxBytes))\n"
    "    end\n"
    "\n"
    "    func peek(int maxBytes = 0) as Buffer\n"
    "        return Buffer(point(self.peekNative(maxBytes)))\n"
    "    end\n"
    "\n"
    "    func consume(int size) as bool\n"
    "        return self.consumeNative(size)\n"
    "    end\n"
    "\n"
    "    func waitOpen(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(0, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitReadable(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(1, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitWritable(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(2, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func drain(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(3, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitEnd(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(4, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func waitClose(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(5, timeoutMs)\n"
    "    end\n"
    "\n"
    "    func readableAsync() as future<bool>\n"
    "        return self.waitAsyncNative(1)\n"
    "    end\n"
    "\n"
    "    func writableAsync() as future<bool>\n"
    "        return self.waitAsyncNative(2)\n"
    "    end\n"
    "\n"
    "    func drainAsync() as future<bool>\n"
    "        return self.waitAsyncNative(3)\n"
    "    end\n"
    "\n"
    "    func endAsync() as future<bool>\n"
    "        return self.waitAsyncNative(4)\n"
    "    end\n"
    "\n"
    "    func closeAsync() as future<bool>\n"
    "        return self.waitAsyncNative(5)\n"
    "    end\n"
    "\n"
    "    func localAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.localAddrNative())\n"
    "    end\n"
    "\n"
    "    func remoteAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.remoteAddrNative())\n"
    "    end\n"
	"end\n";



const char* xl6StdlibTlsServerClassSource =
    "class TlsServerStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_net_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_net_stats_value\" func value(int index) as uint64\n"
    "\n"
    "    get state() as int\n"
    "        return int(self.value(0))\n"
    "    end\n"
    "\n"
    "    get handshakes() as uint64\n"
    "        return self.value(1)\n"
    "    end\n"
    "\n"
    "    get accepted() as uint64\n"
    "        return self.value(2)\n"
    "    end\n"
    "\n"
    "    get rejected() as uint64\n"
    "        return self.value(3)\n"
    "    end\n"
    "\n"
    "    get handshakeErrors() as uint64\n"
    "        return self.value(4)\n"
    "    end\n"
    "\n"
    "    get activeHandshakes() as uint64\n"
    "        return self.value(5)\n"
    "    end\n"
    "\n"
    "    get peakHandshakes() as uint64\n"
    "        return self.value(6)\n"
    "    end\n"
    "\n"
    "    get queuedAccepts() as uint64\n"
    "        return self.value(7)\n"
    "    end\n"
    "\n"
    "    get peakQueuedAccepts() as uint64\n"
    "        return self.value(8)\n"
    "    end\n"
    "\n"
    "    get acceptWaiters() as uint64\n"
    "        return self.value(9)\n"
    "    end\n"
    "end\n"
    "\n"
    "class TlsServer\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tls_server_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_tls_server_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tls_server_close\" func close() as bool\n"
    "    extern \"xl6_xrt_tls_server_state\" get state() as int\n"
    "    extern \"xl6_xrt_tls_server_port\" get port() as int\n"
    "    extern \"xl6_xrt_tls_server_local_addr\" func localAddrNative() as point\n"
    "    extern \"xl6_xrt_tls_server_accept\" func acceptNative(int timeoutMs) as point\n"
    "    extern \"xl6_xrt_tls_server_accept_async\" func acceptAsyncNative() as future<TlsStream>\n"
    "    extern \"xl6_xrt_tls_server_stats\" func statsNative() as point\n"
    "\n"
    "    func localAddr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.localAddrNative())\n"
    "    end\n"
    "\n"
    "    func accept(int timeoutMs = -1) as TlsStream\n"
    "        return TlsStream(point(self.acceptNative(timeoutMs)))\n"
    "    end\n"
    "\n"
    "    func acceptAsync() as future<TlsStream>\n"
    "        return self.acceptAsyncNative()\n"
    "    end\n"
    "\n"
    "    func stats() as TlsServerStats\n"
    "        return TlsServerStats(point(self.statsNative()))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibHttp1ClassSource =
    "extern \"xl6_xrt_http1_request_head\" func __xl_http1_request_head(string method, string target, string fields) as point\n"
    "extern \"xl6_xrt_http1_response_head\" func __xl_http1_response_head(int status, string reason, string fields) as point\n"
    "extern \"xl6_xrt_http1_chunk_head\" func __xl_http1_chunk_head(uint64 size, string extensions) as point\n"
    "extern \"xl6_xrt_http1_chunk\" func __xl_http1_chunk(point data, c_size size) as point\n"
    "extern \"xl6_xrt_http1_chunk_end\" func __xl_http1_chunk_end(string trailers) as point\n"
    "\n"
    "class Http1\n"
    "    static func requestHead(string method, string target, string fields = \"\") as Buffer\n"
    "        return Buffer(point(__xl_http1_request_head(method, target, fields)))\n"
    "    end\n"
    "\n"
    "    static func responseHead(int status, string reason = \"\", string fields = \"\") as Buffer\n"
    "        return Buffer(point(__xl_http1_response_head(status, reason, fields)))\n"
    "    end\n"
    "\n"
    "    static func chunkHead(uint64 size, string extensions = \"\") as Buffer\n"
    "        return Buffer(point(__xl_http1_chunk_head(size, extensions)))\n"
    "    end\n"
    "\n"
    "    static func chunk(Buffer data) as Buffer\n"
    "        return Buffer(point(__xl_http1_chunk(data.data(), c_size(data.len()))))\n"
    "    end\n"
    "\n"
    "    static func chunkEnd(string trailers = \"\") as Buffer\n"
    "        return Buffer(point(__xl_http1_chunk_end(trailers)))\n"
    "    end\n"
    "end\n"
    "\n"
    "class Http1Head\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http1_head_create\" create(point data, c_size size, int kind, int maxFields, int maxHead)\n"
    "    extern \"xl6_xrt_http1_head_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http1_head_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http1_head_status\" get status() as int\n"
    "    extern \"xl6_xrt_http1_head_kind\" get kind() as int\n"
    "    extern \"xl6_xrt_http1_head_version\" get version() as int\n"
    "    extern \"xl6_xrt_http1_head_flags\" get flags() as int\n"
    "    extern \"xl6_xrt_http1_head_code\" get code() as int\n"
    "    extern \"xl6_xrt_http1_head_content_length\" get contentLength() as uint64\n"
    "    extern \"xl6_xrt_http1_head_bytes\" get headerBytes() as int\n"
    "    extern \"xl6_xrt_http1_head_field_count\" get fieldCount() as int\n"
    "    extern \"xl6_xrt_http1_head_error\" get errorCode() as int\n"
    "    extern \"xl6_xrt_http1_head_error_offset\" get errorOffset() as int\n"
    "    extern \"xl6_xrt_http1_head_error_line\" get errorLine() as int\n"
    "    extern \"xl6_xrt_http1_head_method\" func methodNative() as point\n"
    "    extern \"xl6_xrt_http1_head_target\" func targetNative() as point\n"
    "    extern \"xl6_xrt_http1_head_reason\" func reasonNative() as point\n"
    "    extern \"xl6_xrt_http1_head_field_name\" func fieldNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http1_head_field_value\" func fieldValueNative(int index) as point\n"
    "    extern \"xl6_xrt_http1_head_field\" func fieldNative(string name) as point\n"
    "\n"
    "    static func request(Buffer input, int maxFields = 100, int maxHead = 65536) as Http1Head\n"
    "        return Http1Head(input.data(), c_size(input.len()), 1, maxFields, maxHead)\n"
    "    end\n"
    "\n"
    "    static func response(Buffer input, int maxFields = 100, int maxHead = 65536) as Http1Head\n"
    "        return Http1Head(input.data(), c_size(input.len()), 2, maxFields, maxHead)\n"
    "    end\n"
    "\n"
    "    get ready() as bool\n"
    "        return self.status == 1\n"
    "    end\n"
    "\n"
    "    func method() as string\n"
    "        return __xl_std_runtime_take_cstr(self.methodNative())\n"
    "    end\n"
    "\n"
    "    func target() as string\n"
    "        return __xl_std_runtime_take_cstr(self.targetNative())\n"
    "    end\n"
    "\n"
    "    func reason() as string\n"
    "        return __xl_std_runtime_take_cstr(self.reasonNative())\n"
    "    end\n"
    "\n"
    "    func fieldName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.fieldNameNative(index))\n"
    "    end\n"
    "\n"
    "    func fieldValue(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.fieldValueNative(index))\n"
    "    end\n"
    "\n"
    "    func field(string name) as string\n"
    "        return __xl_std_runtime_take_cstr(self.fieldNative(name))\n"
    "    end\n"
    "\n"
    "    func body(string requestMethod = \"\", int maxTrailers = 32, int maxBody = -1) as Http1Body\n"
    "        return Http1Body(self.nativeHandle(), requestMethod, maxTrailers, maxBody)\n"
    "    end\n"
    "end\n"
    "\n"
    "class Http1Body\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http1_body_create\" create(point head, string requestMethod, int maxTrailers, int maxBody)\n"
    "    extern \"xl6_xrt_http1_body_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http1_body_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http1_body_read\" func readRaw(point data, c_size size, bool isEnd) as int\n"
    "    extern \"xl6_xrt_http1_body_status\" get status() as int\n"
    "    extern \"xl6_xrt_http1_body_mode\" get mode() as int\n"
    "    extern \"xl6_xrt_http1_body_length\" get length() as uint64\n"
    "    extern \"xl6_xrt_http1_body_received\" get received() as uint64\n"
    "    extern \"xl6_xrt_http1_body_wire_bytes\" get wireBytes() as uint64\n"
    "    extern \"xl6_xrt_http1_body_consumed\" get consumed() as int\n"
    "    extern \"xl6_xrt_http1_body_data\" get dataPoint() as point\n"
    "    extern \"xl6_xrt_http1_body_data_size\" get dataSize() as int\n"
    "    extern \"xl6_xrt_http1_body_done\" get done() as bool\n"
    "    extern \"xl6_xrt_http1_body_error\" get errorCode() as int\n"
    "    extern \"xl6_xrt_http1_body_error_offset\" get errorOffset() as int\n"
    "    extern \"xl6_xrt_http1_body_trailer_count\" get trailerCount() as int\n"
    "    extern \"xl6_xrt_http1_body_data_copy\" func dataCopyNative() as point\n"
    "    extern \"xl6_xrt_http1_body_trailer_name\" func trailerNameNative(int index) as point\n"
    "    extern \"xl6_xrt_http1_body_trailer_value\" func trailerValueNative(int index) as point\n"
    "\n"
    "    func read(Buffer input, bool isEnd = false) as int\n"
    "        return self.readRaw(input.data(), c_size(input.len()), isEnd)\n"
    "    end\n"
    "\n"
    "    func dataCopy() as Buffer\n"
    "        return Buffer(point(self.dataCopyNative()))\n"
    "    end\n"
    "\n"
    "    func trailerName(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.trailerNameNative(index))\n"
    "    end\n"
    "\n"
    "    func trailerValue(int index) as string\n"
    "        return __xl_std_runtime_take_cstr(self.trailerValueNative(index))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibHttpDecoderClassSource =
    "class HttpDecoder\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_http_decoder_create_default\" create()\n"
    "    extern \"xl6_xrt_http_decoder_create_encoding\" create(string encoding)\n"
    "    extern \"xl6_xrt_http_decoder_create_limit\" create(string encoding, int outputLimit)\n"
    "    extern \"xl6_xrt_http_decoder_create\" create(string encoding, int outputLimit, bool allowRaw)\n"
    "    extern \"xl6_xrt_http_decoder_destroy\" destroy()\n"
    "    extern \"xl6_xrt_http_decoder_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_http_decoder_write\" func writeRaw(point data, c_size size, bool isEnd) as bool\n"
    "    extern \"xl6_xrt_http_decoder_take\" func takeNative() as point\n"
    "    extern \"xl6_xrt_http_decoder_mode\" get mode() as int\n"
    "    extern \"xl6_xrt_http_decoder_done\" get done() as bool\n"
    "    extern \"xl6_xrt_http_decoder_input\" get inputSize() as uint64\n"
    "    extern \"xl6_xrt_http_decoder_output\" get outputSize() as uint64\n"
    "    extern \"xl6_xrt_http_decoder_pending\" get pending() as int\n"
    "\n"
    "    func write(Buffer input, bool isEnd = false) as bool\n"
    "        return self.writeRaw(input.data(), c_size(input.len()), isEnd)\n"
    "    end\n"
    "\n"
    "    func take() as Buffer\n"
    "        return Buffer(point(self.takeNative()))\n"
    "    end\n"
    "end\n";




const char* xl6StdlibWsFrameClassSource =
    "extern \"xl6_xrt_ws_mask\" func __xl_ws_mask(point buffer, uint32 mask, uint64 offset) as bool\n"
    "extern \"xl6_xrt_ws_frame_parse\" func __xl_ws_frame_parse(point data, c_size size, int maskPolicy, int maxPayload, int allowedRsv) as point\n"
    "extern \"xl6_xrt_ws_frame_create\" func __xl_ws_frame_create(int opcode, uint64 payloadSize, bool final, bool masked, uint32 mask, int rsv) as point\n"
    "extern \"xl6_xrt_ws_close_create\" func __xl_ws_close_create(int code, string reason) as point\n"
    "extern \"xl6_xrt_ws_close_parse\" func __xl_ws_close_parse(point data, c_size size) as point\n"
    "\n"
    "class WsFrame\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_ws_frame_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_ws_frame_destroy\" destroy()\n"
    "    extern \"xl6_xrt_ws_frame_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_ws_frame_status\" get status() as int\n"
    "    extern \"xl6_xrt_ws_frame_opcode\" get opcode() as int\n"
    "    extern \"xl6_xrt_ws_frame_flags\" get flags() as int\n"
    "    extern \"xl6_xrt_ws_frame_payload\" get payloadSize() as uint64\n"
    "    extern \"xl6_xrt_ws_frame_head\" get headerSize() as int\n"
    "    extern \"xl6_xrt_ws_frame_mask\" get mask() as uint32\n"
    "    extern \"xl6_xrt_ws_frame_error\" get errorCode() as int\n"
    "    extern \"xl6_xrt_ws_frame_error_offset\" get errorOffset() as int\n"
    "    extern \"xl6_xrt_ws_frame_write\" func writeNative() as point\n"
    "\n"
    "    static func parse(Buffer input, int maskPolicy = 0, int maxPayload = -1, int allowedRsv = 0) as WsFrame\n"
    "        return WsFrame(point(__xl_ws_frame_parse(input.data(), c_size(input.len()), maskPolicy, maxPayload, allowedRsv)))\n"
    "    end\n"
    "\n"
    "    static func make(int opcode, uint64 payloadSize, bool final = true, bool masked = false, uint32 mask = uint32(0), int rsv = 0) as WsFrame\n"
    "        return WsFrame(point(__xl_ws_frame_create(opcode, payloadSize, final, masked, mask, rsv)))\n"
    "    end\n"
    "\n"
    "    get ready() as bool\n"
    "        return self.status == 1\n"
    "    end\n"
    "\n"
    "    get final() as bool\n"
    "        return (self.flags & 1) != 0\n"
    "    end\n"
    "\n"
    "    get masked() as bool\n"
    "        return (self.flags & 2) != 0\n"
    "    end\n"
    "\n"
    "    func write() as Buffer\n"
    "        return Buffer(point(self.writeNative()))\n"
    "    end\n"
    "\n"
    "    static func applyMask(Buffer data, uint32 mask, uint64 offset = uint64(0)) as bool\n"
    "        return __xl_ws_mask(data.nativeHandle(), mask, offset)\n"
    "    end\n"
    "end\n"
    "\n"
    "class WsClose\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_ws_close_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_ws_close_destroy\" destroy()\n"
    "    extern \"xl6_xrt_ws_close_code\" get code() as int\n"
    "    extern \"xl6_xrt_ws_close_reason\" func reasonNative() as point\n"
    "    extern \"xl6_xrt_ws_close_write\" func writeNative() as point\n"
    "\n"
    "    static func make(int code = 0, string reason = \"\") as WsClose\n"
    "        return WsClose(point(__xl_ws_close_create(code, reason)))\n"
    "    end\n"
    "\n"
    "    static func parse(Buffer payload) as WsClose\n"
    "        return WsClose(point(__xl_ws_close_parse(payload.data(), c_size(payload.len()))))\n"
    "    end\n"
    "\n"
    "    func reason() as string\n"
    "        return __xl_std_runtime_take_cstr(self.reasonNative())\n"
    "    end\n"
    "\n"
    "    func write() as Buffer\n"
    "        return Buffer(point(self.writeNative()))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibWsHandshakeClassSource =
    "extern \"xl6_xrt_ws_key_generate\" func __xl_ws_key_generate() as point\n"
    "extern \"xl6_xrt_ws_accept\" func __xl_ws_accept(string key) as point\n"
    "extern \"xl6_xrt_ws_key_valid\" func __xl_ws_key_valid(string key) as bool\n"
    "extern \"xl6_xrt_ws_accept_valid\" func __xl_ws_accept_valid(string key, string accept) as bool\n"
    "extern \"xl6_xrt_ws_protocols_valid\" func __xl_ws_protocols_valid(string protocols) as bool\n"
    "extern \"xl6_xrt_ws_protocols_has\" func __xl_ws_protocols_has(string protocols, string protocol) as bool\n"
    "extern \"xl6_xrt_ws_protocol_select\" func __xl_ws_protocol_select(string client, string server) as point\n"
    "\n"
    "class WsHandshake\n"
    "    static func key() as string\n"
    "        return __xl_std_runtime_take_cstr(__xl_ws_key_generate())\n"
    "    end\n"
    "\n"
    "    static func accept(string key) as string\n"
    "        return __xl_std_runtime_take_cstr(__xl_ws_accept(key))\n"
    "    end\n"
    "\n"
    "    static func protocolSelect(string client, string server) as string\n"
    "        return __xl_std_runtime_take_cstr(__xl_ws_protocol_select(client, server))\n"
    "    end\n"
    "\n"
    "    static func keyValid(string key) as bool\n"
    "        return __xl_ws_key_valid(key)\n"
    "    end\n"
    "\n"
    "    static func acceptValid(string key, string accept) as bool\n"
    "        return __xl_ws_accept_valid(key, accept)\n"
    "    end\n"
    "\n"
    "    static func protocolsValid(string protocols) as bool\n"
    "        return __xl_ws_protocols_valid(protocols)\n"
    "    end\n"
    "\n"
    "    static func protocolsHas(string protocols, string protocol) as bool\n"
    "        return __xl_ws_protocols_has(protocols, protocol)\n"
    "    end\n"
    "end\n";



const char* xl6StdlibWsUpgradeClassSource =
    "extern \"xl6_xrt_ws_upgrade_client\" func __xl_ws_upgrade_client(string host, string protocols, bool deflate, bool requireDeflate) as point\n"
    "extern \"xl6_xrt_ws_upgrade_server\" func __xl_ws_upgrade_server(point request, string protocols, bool deflate, bool requireDeflate) as point\n"
    "\n"
    "class WsUpgrade\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_ws_upgrade_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_ws_upgrade_destroy\" destroy()\n"
    "    extern \"xl6_xrt_ws_upgrade_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_ws_upgrade_role\" get role() as int\n"
    "    extern \"xl6_xrt_ws_upgrade_ready\" get ready() as bool\n"
    "    extern \"xl6_xrt_ws_upgrade_deflate\" get deflateEnabled() as bool\n"
    "    extern \"xl6_xrt_ws_upgrade_key\" func keyNative() as point\n"
    "    extern \"xl6_xrt_ws_upgrade_protocol\" func protocolNative() as point\n"
    "    extern \"xl6_xrt_ws_upgrade_extensions\" func extensionsNative() as point\n"
    "    extern \"xl6_xrt_ws_upgrade_fields\" func fieldsNative() as point\n"
    "    extern \"xl6_xrt_ws_upgrade_check\" func checkNative(point response) as bool\n"
    "\n"
    "    static func client(string host, string protocols = \"\", bool deflate = false, bool requireDeflate = false) as WsUpgrade\n"
    "        return WsUpgrade(point(__xl_ws_upgrade_client(host, protocols, deflate, requireDeflate)))\n"
    "    end\n"
    "\n"
    "    static func server(Http1Head request, string protocols = \"\", bool deflate = false, bool requireDeflate = false) as WsUpgrade\n"
    "        return WsUpgrade(point(__xl_ws_upgrade_server(request.nativeHandle(), protocols, deflate, requireDeflate)))\n"
    "    end\n"
    "\n"
    "    func check(Http1Head response) as bool\n"
    "        return self.checkNative(response.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func key() as string\n"
    "        return __xl_std_runtime_take_cstr(self.keyNative())\n"
    "    end\n"
    "\n"
    "    func protocol() as string\n"
    "        return __xl_std_runtime_take_cstr(self.protocolNative())\n"
    "    end\n"
    "\n"
    "    func extensions() as string\n"
    "        return __xl_std_runtime_take_cstr(self.extensionsNative())\n"
    "    end\n"
    "\n"
    "    func fields() as string\n"
    "        return __xl_std_runtime_take_cstr(self.fieldsNative())\n"
    "    end\n"
    "end\n";



const char* xl6StdlibWebSocketTlsExternSource =
    "extern \"xl6_xrt_ws_stream_attach_tls\" func __xl_std_runtime_ws_stream_attach_tls(var transport, point upgrade, point config, func(int, int, int, uint64, point, c_size) handler, c_size prefix) as point\n";



const char* xl6StdlibWebSocketConfigClassSource =
    "class WebSocketConfig\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_ws_stream_config_create\" create()\n"
    "    extern \"xl6_xrt_ws_stream_config_destroy\" destroy()\n"
    "    extern \"xl6_xrt_ws_stream_config_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_ws_stream_config_limits\" func limits(int messageBytes, int frameBytes, int sendBytes) as bool\n"
    "    extern \"xl6_xrt_ws_stream_config_control\" func control(int reserveBytes, int closeTimeoutMs) as bool\n"
    "    extern \"xl6_xrt_ws_stream_config_auto_pong\" func autoPong(bool enabled)\n"
    "end\n";



const char* xl6StdlibWebSocketClassSource =
    "extern \"xl6_xrt_ws_stream_attach_tcp\" func __xl_std_runtime_ws_stream_attach_tcp(var transport, point upgrade, point config, func(int, int, int, uint64, point, c_size) handler, c_size prefix) as point\n"
    "class WebSocket\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_ws_stream_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_ws_stream_destroy\" destroy()\n"
    "    extern \"xl6_xrt_ws_stream_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_ws_stream_state\" get state() as int\n"
    "    extern \"xl6_xrt_ws_stream_role\" get role() as int\n"
    "    extern \"xl6_xrt_ws_stream_pending\" get pending() as int\n"
    "    extern \"xl6_xrt_ws_stream_writable\" get writable() as int\n"
    "    extern \"xl6_xrt_ws_stream_paused\" get paused() as bool\n"
    "    extern \"xl6_xrt_ws_stream_deflate\" get deflateEnabled() as bool\n"
    "    extern \"xl6_xrt_ws_stream_protocol\" func protocolNative() as point\n"
    "    extern \"xl6_xrt_ws_stream_send\" func sendRaw(int opcode, point data, c_size size, bool compressed) as int\n"
    "    extern \"xl6_xrt_ws_stream_send_text\" func sendTextRaw(string text, bool compressed) as int\n"
    "    extern \"xl6_xrt_ws_stream_send_move\" func sendMoveRaw(int opcode, point buffer) as int\n"
    "    extern \"xl6_xrt_ws_stream_ping\" func pingRaw(point data, c_size size) as int\n"
    "    extern \"xl6_xrt_ws_stream_pong\" func pongRaw(point data, c_size size) as int\n"
    "    extern \"xl6_xrt_ws_stream_close\" func closeResult(int code, string reason) as int\n"
    "    extern \"xl6_xrt_ws_stream_pause\" func pause()\n"
    "    extern \"xl6_xrt_ws_stream_resume\" func resume() as bool\n"
    "    extern \"xl6_xrt_ws_stream_abort\" func abort() as bool\n"
    "    extern \"xl6_xrt_ws_stream_close_flags\" get closeFlags() as int\n"
    "    extern \"xl6_xrt_ws_stream_close_local\" get localCloseCode() as int\n"
    "    extern \"xl6_xrt_ws_stream_close_remote\" get remoteCloseCode() as int\n"
    "    extern \"xl6_xrt_ws_stream_close_reason\" func closeReasonNative() as point\n"
    "    extern \"xl6_xrt_ws_stream_error_kind\" get errorKind() as int\n"
    "    extern \"xl6_xrt_ws_stream_error_code\" get errorCode() as int\n"
    "    extern \"xl6_xrt_ws_stream_error_domain\" func errorDomainNative() as point\n"
    "    extern \"xl6_xrt_ws_stream_error_operation\" func errorOperationNative() as point\n"
    "    extern \"xl6_xrt_ws_stream_error_message\" func errorMessageNative() as point\n"
    "\n"
    "    static func attachTcp(TcpStream transport, WsUpgrade upgrade, func(int, int, int, uint64, point, c_size) handler, int prefix = 0) as WebSocket\n"
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tcp(transport, upgrade.nativeHandle(), null, handler, c_size(prefix))))\n"
    "    end\n"
    "\n"
    "    static func attachTcpWithConfig(TcpStream transport, WsUpgrade upgrade, WebSocketConfig config, func(int, int, int, uint64, point, c_size) handler, int prefix = 0) as WebSocket\n"
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tcp(transport, upgrade.nativeHandle(), config.nativeHandle(), handler, c_size(prefix))))\n"
    "    end\n"
    "\n"
    "    func protocol() as string\n"
    "        return __xl_std_runtime_take_cstr(self.protocolNative())\n"
    "    end\n"
    "\n"
    "    func sendTextResult(string text, bool compressed = false) as int\n"
    "        return self.sendTextRaw(text, compressed)\n"
    "    end\n"
    "\n"
    "    func sendResult(Buffer data, bool compressed = false) as int\n"
    "        return self.sendRaw(2, data.data(), c_size(data.len()), compressed)\n"
    "    end\n"
    "\n"
    "    func send(string text, bool compressed = false) as bool\n"
    "        return self.sendTextResult(text, compressed) == 0\n"
    "    end\n"
    "\n"
    "    func send(Buffer data, bool compressed = false) as bool\n"
    "        return self.sendResult(data, compressed) == 0\n"
    "    end\n"
    "\n"
    "    func sendMoveResult(Buffer data, int opcode = 2) as int\n"
    "        return self.sendMoveRaw(opcode, data.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func sendMove(Buffer data, int opcode = 2) as bool\n"
    "        return self.sendMoveResult(data, opcode) == 0\n"
    "    end\n"
    "\n"
    "    func ping(Buffer payload) as bool\n"
    "        return self.pingRaw(payload.data(), c_size(payload.len())) == 0\n"
    "    end\n"
    "\n"
    "    func pong(Buffer payload) as bool\n"
    "        return self.pongRaw(payload.data(), c_size(payload.len())) == 0\n"
    "    end\n"
    "\n"
    "    func close(int code = 1000, string reason = \"\") as bool\n"
    "        return self.closeResult(code, reason) == 0\n"
    "    end\n"
    "\n"
    "    func closeReason() as string\n"
    "        return __xl_std_runtime_take_cstr(self.closeReasonNative())\n"
    "    end\n"
    "\n"
    "    func errorDomain() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorDomainNative())\n"
    "    end\n"
    "\n"
    "    func errorOperation() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorOperationNative())\n"
    "    end\n"
    "\n"
    "    func errorMessage() as string\n"
    "        return __xl_std_runtime_take_cstr(self.errorMessageNative())\n"
    "    end\n";



const char* xl6StdlibWebSocketTlsMethodSource =
    "    static func attachTls(TlsStream transport, WsUpgrade upgrade, func(int, int, int, uint64, point, c_size) handler, int prefix = 0) as WebSocket\n"
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tls(transport, upgrade.nativeHandle(), null, handler, c_size(prefix))))\n"
    "    end\n"
    "\n"
    "    static func attachTlsWithConfig(TlsStream transport, WsUpgrade upgrade, WebSocketConfig config, func(int, int, int, uint64, point, c_size) handler, int prefix = 0) as WebSocket\n"
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tls(transport, upgrade.nativeHandle(), config.nativeHandle(), handler, c_size(prefix))))\n"
    "    end\n";



const char* xl6StdlibWebSocketClassEndSource =
    "end\n";

const char* xl6StdlibTcpConnectConfigClassSource =
    "class TcpConnectConfig\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tcp_connect_config_create\" create(string host, int port)\n"
    "    extern \"xl6_xrt_tcp_connect_config_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tcp_connect_config_timeout\" func timeout(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_tcp_connect_config_water\" func watermarks(int high, int low) as bool\n"
    "    extern \"xl6_xrt_tcp_connect_config_recv_limit\" func recvLimit(int size) as bool\n"
    "    extern \"xl6_xrt_tcp_connect_config_proxy\" func proxyNative(point proxy) as bool\n"
    "    extern \"xl6_xrt_tcp_connect_config_connect\" func connectNative() as point\n"
    "\n"
    "    func proxy(NetProxy proxy) as bool\n"
    "        return self.proxyNative(proxy.nativeHandle())\n"
    "    end\n"
    "\n"
    "    func connect() as TcpStream\n"
    "        return TcpStream(point(self.connectNative()))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibTcpListenConfigClassSource =
    "class TcpListenConfig\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_tcp_listen_config_create\" create(string host, int port)\n"
    "    extern \"xl6_xrt_tcp_listen_config_destroy\" destroy()\n"
    "    extern \"xl6_xrt_tcp_listen_config_backlog\" func backlog(int value) as bool\n"
    "    extern \"xl6_xrt_tcp_listen_config_water\" func watermarks(int high, int low) as bool\n"
    "    extern \"xl6_xrt_tcp_listen_config_recv_limit\" func recvLimit(int size) as bool\n"
    "    extern \"xl6_xrt_tcp_listen_config_listen\" func listenNative() as point\n"
    "\n"
    "    func listen() as TcpServer\n"
    "        return TcpServer(point(self.listenNative()))\n"
    "    end\n"
    "end\n";



const char* xl6StdlibUdpConfigClassSource =
    "class UdpConfig\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_udp_config_create\" create(string host, int port)\n"
    "    extern \"xl6_xrt_udp_config_destroy\" destroy()\n"
    "    extern \"xl6_xrt_udp_config_recv_batch\" func recvBatch(int value) as bool\n"
    "    extern \"xl6_xrt_udp_config_queues\" func queueLimits(int sendLimit, int recvLimit) as bool\n"
    "    extern \"xl6_xrt_udp_config_bind\" func bindNative() as point\n"
    "\n"
    "    func bind() as UdpSocket\n"
    "        return UdpSocket(point(self.bindNative()))\n"
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
    "    extern \"xl6_xrt_file_native\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_file_tell\" get pos() as int\n"
    "    extern \"xl6_xrt_file_size\" get size() as int\n"
    "    extern \"xl6_xrt_file_eof\" get eof() as bool\n"
    "    extern \"xl6_xrt_file_seek\" func seekNative(int offset, int whence) as int\n"
    "    extern \"xl6_xrt_file_set_eof\" func setEOF() as bool\n"
    "    extern \"xl6_xrt_file_read_text\" func readNative(int size) as point\n"
    "    extern \"xl6_xrt_file_write_text\" func writeTextNative(string text, c_size size) as int\n"
    "    extern \"xl6_xrt_file_read_bytes\" func readBytesNative(int size) as point\n"
    "    extern \"xl6_xrt_file_write_bytes\" func writeBytesNative(point data, c_size size) as int\n"
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
    "    func readBytes(int size = -1) as Buffer\n"
    "        return Buffer(point(self.readBytesNative(size)))\n"
    "    end\n"
    "\n"
    "    func writeBytes(Buffer data) as int\n"
    "        return self.writeBytesNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func write(string text) as int\n"
    "        return self.writeText(text)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibAsyncFileClassSource =
    "class AsyncFile\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_async_file_open\" create(string path, int flags, int shareFlags)\n"
    "    extern \"xl6_xrt_async_file_destroy\" destroy()\n"
	"    extern \"xl6_xrt_async_file_native\" func nativeHandle() as point\n"
	"    extern \"xl6_xrt_async_file_take\" func detach() as point\n"
    "    extern \"xl6_xrt_async_file_close\" func close() as bool\n"
    "    extern \"xl6_xrt_async_file_read\" func readNative(int offset, int size) as future<point>\n"
    "    extern \"xl6_xrt_async_file_write\" func writeNative(int offset, point data, c_size size) as future<int>\n"
    "    extern \"xl6_xrt_async_file_flush\" func flush() as future<bool>\n"
    "    extern \"xl6_xrt_async_file_size\" func size() as future<int>\n"
    "    extern \"xl6_xrt_async_file_resize\" func resize(int size) as future<bool>\n"
    "\n"
    "    func read(int offset, int size) as future<point>\n"
    "        return self.readNative(offset, size)\n"
    "    end\n"
    "\n"
    "    func readAt(int offset, int size) as future<point>\n"
    "        return self.readNative(offset, size)\n"
    "    end\n"
    "\n"
    "    func write(int offset, Buffer data) as future<int>\n"
    "        return self.writeNative(offset, data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func writeAt(int offset, Buffer data) as future<int>\n"
    "        return self.writeNative(offset, data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func setSize(int size) as future<bool>\n"
    "        return self.resize(size)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibJsonClassSource =
    "class JsonWriter\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_json_writer_create_default\" create()\n"
    "    extern \"xl6_xrt_json_writer_create_pretty\" create(bool pretty)\n"
    "    extern \"xl6_xrt_json_writer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_json_writer_begin_object\" func beginObject() as bool\n"
    "    extern \"xl6_xrt_json_writer_begin_object_key\" func beginObject(string key) as bool\n"
    "    extern \"xl6_xrt_json_writer_end_object\" func endObject() as bool\n"
    "    extern \"xl6_xrt_json_writer_begin_array\" func beginArray() as bool\n"
    "    extern \"xl6_xrt_json_writer_begin_array_key\" func beginArray(string key) as bool\n"
    "    extern \"xl6_xrt_json_writer_end_array\" func endArray() as bool\n"
    "    extern \"xl6_xrt_json_writer_null\" func writeNull() as bool\n"
    "    extern \"xl6_xrt_json_writer_null_key\" func writeNull(string key) as bool\n"
    "    extern \"xl6_xrt_json_writer_bool\" func writeBool(bool value) as bool\n"
    "    extern \"xl6_xrt_json_writer_bool_key\" func writeBool(string key, bool value) as bool\n"
    "    extern \"xl6_xrt_json_writer_int\" func writeInt(int value) as bool\n"
    "    extern \"xl6_xrt_json_writer_int_key\" func writeInt(string key, int value) as bool\n"
    "    extern \"xl6_xrt_json_writer_float\" func writeFloat(float value) as bool\n"
    "    extern \"xl6_xrt_json_writer_float_key\" func writeFloat(string key, float value) as bool\n"
    "    extern \"xl6_xrt_json_writer_string\" func writeString(string value) as bool\n"
    "    extern \"xl6_xrt_json_writer_string_key\" func writeString(string key, string value) as bool\n"
    "    extern \"xl6_xrt_json_writer_value\" func writeValue(var value) as bool\n"
    "    extern \"xl6_xrt_json_writer_value_key\" func writeValue(string key, var value) as bool\n"
    "    extern \"xl6_xrt_json_writer_finish\" func finishNative() as point\n"
    "\n"
    "    func finish() as string\n"
    "        return __xl_std_runtime_take_cstr(self.finishNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class XsonWriter\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_xson_writer_create_default\" create()\n"
    "    extern \"xl6_xrt_xson_writer_create_pretty\" create(bool pretty)\n"
    "    extern \"xl6_xrt_xson_writer_create_options\" create(bool pretty, bool ignoreUnsupportedEncode, bool ignoreUnsupportedDecode)\n"
    "    extern \"xl6_xrt_xson_writer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_xson_writer_begin_array\" func beginArray() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_array_key\" func beginArray(string key) as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_array_index\" func beginArray(int index) as bool\n"
    "    extern \"xl6_xrt_xson_writer_end_array\" func endArray() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_list\" func beginList() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_list_key\" func beginList(string key) as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_list_index\" func beginList(int index) as bool\n"
    "    extern \"xl6_xrt_xson_writer_end_list\" func endList() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_dict\" func beginDict() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_dict_key\" func beginDict(string key) as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_dict_index\" func beginDict(int index) as bool\n"
    "    extern \"xl6_xrt_xson_writer_end_dict\" func endDict() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_set\" func beginSet() as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_set_key\" func beginSet(string key) as bool\n"
    "    extern \"xl6_xrt_xson_writer_begin_set_index\" func beginSet(int index) as bool\n"
    "    extern \"xl6_xrt_xson_writer_end_set\" func endSet() as bool\n"
    "    extern \"xl6_xrt_xson_writer_null\" func writeNull() as bool\n"
    "    extern \"xl6_xrt_xson_writer_null_key\" func writeNull(string key) as bool\n"
    "    extern \"xl6_xrt_xson_writer_null_index\" func writeNull(int index) as bool\n"
    "    extern \"xl6_xrt_xson_writer_bool\" func writeBool(bool value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_bool_key\" func writeBool(string key, bool value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_bool_index\" func writeBool(int index, bool value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_int\" func writeInt(int value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_int_key\" func writeInt(string key, int value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_int_index\" func writeInt(int index, int value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_float\" func writeFloat(float value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_float_key\" func writeFloat(string key, float value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_float_index\" func writeFloat(int index, float value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_string\" func writeString(string value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_string_key\" func writeString(string key, string value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_string_index\" func writeString(int index, string value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_time\" func writeTimeRaw(int value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_time_key\" func writeTimeRaw(string key, int value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_time_index\" func writeTimeRaw(int index, int value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_value\" func writeValue(var value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_value_key\" func writeValue(string key, var value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_value_index\" func writeValue(int index, var value) as bool\n"
    "    extern \"xl6_xrt_xson_writer_finish\" func finishNative() as point\n"
    "\n"
    "    func writeTime(time value) as bool\n"
    "        return self.writeTimeRaw(int(value))\n"
    "    end\n"
    "\n"
    "    func writeTime(string key, time value) as bool\n"
    "        return self.writeTimeRaw(key, int(value))\n"
    "    end\n"
    "\n"
    "    func writeTime(int index, time value) as bool\n"
    "        return self.writeTimeRaw(index, int(value))\n"
    "    end\n"
    "\n"
    "    func finish() as string\n"
    "        return __xl_std_runtime_take_cstr(self.finishNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class XsonOptions\n"
    "    bool pretty = false\n"
    "    bool ignoreUnsupportedEncode = false\n"
    "    bool ignoreUnsupportedDecode = false\n"
    "\n"
    "    create(bool pretty = false, bool ignoreUnsupportedEncode = false, bool ignoreUnsupportedDecode = false)\n"
    "        self.pretty = pretty\n"
    "        self.ignoreUnsupportedEncode = ignoreUnsupportedEncode\n"
    "        self.ignoreUnsupportedDecode = ignoreUnsupportedDecode\n"
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
    "    extern \"xl6_xrt_process_result_exit_info\" func exitInfo() as var\n"
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
    "    extern \"xl6_xrt_process_command_work_dir\" func workDir(string path) as bool\n"
    "    extern \"xl6_xrt_process_command_env\" func env(string name, string value) as bool\n"
    "    extern \"xl6_xrt_process_command_clear_env\" func clearEnv() as bool\n"
    "    extern \"xl6_xrt_process_command_inherit_env\" func inheritEnv(bool enabled) as bool\n"
    "    extern \"xl6_xrt_process_command_use_terminal\" func terminalNative(bool enabled) as bool\n"
    "    extern \"xl6_xrt_process_command_new_console\" func newConsoleNative(bool enabled) as bool\n"
    "    extern \"xl6_xrt_process_command_merge_stderr\" func mergeStderrNative(bool enabled) as bool\n"
    "    extern \"xl6_xrt_process_command_process_group\" func processGroupNative(bool enabled) as bool\n"
    "    extern \"xl6_xrt_process_command_hide_window\" func hideWindowNative(bool enabled) as bool\n"
    "    extern \"xl6_xrt_process_command_terminal_size\" func terminalSize(int cols, int rows) as bool\n"
    "    extern \"xl6_xrt_process_command_read_chunk_size\" func readChunkSize(int size) as bool\n"
    "    extern \"xl6_xrt_process_command_max_capture_bytes\" func maxCaptureBytes(int size) as bool\n"
    "    extern \"xl6_xrt_process_command_max_event_count\" func maxEventCount(int count) as bool\n"
    "    extern \"xl6_xrt_process_command_stdio\" func stdioNative(int stream, int mode, bool capture) as bool\n"
    "    extern \"xl6_xrt_process_command_run\" func runNative(string input, c_size inputSize, int timeoutMs) as point\n"
    "    extern \"xl6_xrt_process_command_start\" func startNative() as point\n"
    "\n"
    "    func stdio(int stream, int mode, bool capture = false) as bool\n"
    "        return self.stdioNative(stream, mode, capture)\n"
    "    end\n"
    "\n"
    "    func terminal(bool enabled = true) as bool\n"
    "        return self.terminalNative(enabled)\n"
    "    end\n"
    "\n"
    "    func newConsole(bool enabled = true) as bool\n"
    "        return self.newConsoleNative(enabled)\n"
    "    end\n"
    "\n"
    "    func mergeStderr(bool enabled = true) as bool\n"
    "        return self.mergeStderrNative(enabled)\n"
    "    end\n"
    "\n"
    "    func processGroup(bool enabled = true) as bool\n"
    "        return self.processGroupNative(enabled)\n"
    "    end\n"
    "\n"
    "    func hideWindow(bool enabled = true) as bool\n"
    "        return self.hideWindowNative(enabled)\n"
    "    end\n"
    "\n"
    "    func run(string input = \"\", int timeoutMs = 0) as ProcessResult\n"
    "        return ProcessResult(self.runNative(input, __xl_std_crt_strlen(input), timeoutMs))\n"
    "    end\n"
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
    "    extern \"xl6_xrt_process_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_process_spawn_file\" create(string program, array args)\n"
    "    extern \"xl6_xrt_process_spawn_shell\" create(string command)\n"
    "    extern \"xl6_xrt_process_destroy\" destroy()\n"
    "    extern \"xl6_xrt_process_running\" get running() as bool\n"
    "    extern \"xl6_xrt_process_state\" get state() as int\n"
    "    extern \"xl6_xrt_process_handle_exit_code\" get exitCode() as int\n"
    "    extern \"xl6_xrt_process_write\" func write(string text) as bool\n"
    "    extern \"xl6_xrt_process_write_bytes\" func writeBytesNative(point data, c_size size) as bool\n"
    "    extern \"xl6_xrt_process_close_stdin\" func closeStdin() as bool\n"
    "    extern \"xl6_xrt_process_wait\" func waitNative(int timeoutMs) as bool\n"
    "    extern \"xl6_xrt_process_exit_info\" func exitInfo() as var\n"
    "    extern \"xl6_xrt_process_read_stdout\" func readStdoutNative(int offset, int maxBytes) as var\n"
    "    extern \"xl6_xrt_process_read_stderr\" func readStderrNative(int offset, int maxBytes) as var\n"
    "    extern \"xl6_xrt_process_read_events\" func readEventsNative(int seq, int maxCount) as var\n"
    "    extern \"xl6_xrt_process_interrupt\" func interrupt() as bool\n"
    "    extern \"xl6_xrt_process_resize_terminal\" func resizeTerminal(int cols, int rows) as bool\n"
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
    "    func writeBytes(Buffer data) as bool\n"
    "        return self.writeBytesNative(data.data(), c_size(data.len()))\n"
    "    end\n"
    "\n"
    "    func wait(int timeoutMs = -1) as bool\n"
    "        return self.waitNative(timeoutMs)\n"
    "    end\n"
    "\n"
    "    func readStdout(int offset = 0, int maxBytes = 0) as var\n"
    "        return self.readStdoutNative(offset, maxBytes)\n"
    "    end\n"
    "\n"
    "    func readStderr(int offset = 0, int maxBytes = 0) as var\n"
    "        return self.readStderrNative(offset, maxBytes)\n"
    "    end\n"
    "\n"
    "    func readEvents(int seq = 0, int maxCount = 0) as var\n"
    "        return self.readEventsNative(seq, maxCount)\n"
    "    end\n"
    "\n"
    "    get stdout() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stdoutNative())\n"
    "    end\n"
    "\n"
    "    get stderr() as string\n"
    "        return __xl_std_runtime_take_cstr(self.stderrNative())\n"
    "    end\n";

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
    "end\n"
    "\n"
    "class TccHandler\n"
    "    native point handle\n"
    "    extern \"xl6_tcc_handler_wrap\" create(point handle)\n"
    "    extern \"xl6_tcc_handler_create\" create(string path)\n"
    "    extern \"xl6_tcc_handler_destroy\" destroy()\n"
    "    extern \"xl6_tcc_handler_run_args\" func runNative(array args) as c_int\n"
    "\n"
    "    func run(array args) as int\n"
    "        return int(self.runNative(args))\n"
    "    end\n"
    "end\n";

const char* xl6StdlibProcessWaitAsyncMethodSource =
    "\n"
    "    extern \"xl6_xrt_process_wait_future\" func waitFutureNative() as future<point>\n"
    "\n"
    "    func waitAsync() as future<point>\n"
    "        return self.waitFutureNative()\n"
    "    end\n";

const char* xl6StdlibProcessClassEndSource =
    "end\n";

const char* xl6StdlibXlangClassSource =
    "class XLangContext\n"
    "    native point handle\n"
    "    extern \"xl6_xlang_context_create\" create()\n"
    "    extern \"xl6_xlang_context_destroy\" destroy()\n"
    "    extern \"xl6_xlang_context_set_work_dir\" func setWorkDir(string path) as bool\n"
    "    extern \"xl6_xlang_context_set_temp_dir\" func setTempDir(string path) as bool\n"
    "    extern \"xl6_xlang_context_set_save_temps\" func setSaveTemps(bool enabled) as bool\n"
    "    extern \"xl6_xlang_context_run_source\" func run(string source) as bool\n"
    "    extern \"xl6_xlang_context_run_file\" func runFile(string path) as bool\n"
    "    extern \"xl6_xlang_context_run_source_args\" func runArgNative(string source, array args) as bool\n"
    "    extern \"xl6_xlang_context_run_file_args\" func runFileArgNative(string path, array args) as bool\n"
    "    extern \"xl6_xlang_context_clear_last_error\" func clearLastError()\n"
    "    extern \"xl6_xlang_context_last_error_copy\" func lastErrorNative() as point\n"
    "\n"
    "    func run(string source, array args) as bool\n"
    "        return self.runArgNative(source, args)\n"
    "    end\n"
    "\n"
    "    func runFile(string path, array args) as bool\n"
    "        return self.runFileArgNative(path, args)\n"
    "    end\n"
    "\n"
    "    get lastError() as string\n"
    "        return __xl_std_runtime_take_cstr(self.lastErrorNative())\n"
    "    end\n"
    "end\n"
    "\n"
    "class XLangHandler\n"
    "    native point handle\n"
    "    extern \"xl6_xlang_handler_wrap\" create(point handle)\n"
    "    extern \"xl6_xlang_handler_create\" create(string path, string workDir)\n"
    "    extern \"xl6_xlang_handler_destroy\" destroy()\n"
    "    extern \"xl6_xlang_handler_run_args\" func runNative(array args) as bool\n"
    "    extern \"xl6_xlang_handler_last_error_copy\" func lastErrorNative() as point\n"
    "\n"
    "    func run(array args) as bool\n"
    "        return self.runNative(args)\n"
    "    end\n"
    "\n"
    "    get lastError() as string\n"
    "        return __xl_std_runtime_take_cstr(self.lastErrorNative())\n"
    "    end\n"
    "end\n";

static const XL6_StdlibParam xl6StdMathUnaryFloatParams[] = {
    {"float", "x"}
};

static const XL6_StdlibParam xl6StdMathUnaryIntParams[] = {
    {"int", "x"}
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
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtInStrParams[] = {
    {"string", "text"},
    {"string", "part"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtCheckStrParams[] = {
    {"string", "text"},
    {"string", "chars"}
};

static const XL6_StdlibParam xl6StdXrtStrLikeParams[] = {
    {"string", "text"},
    {"string", "pattern"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtStrApproxParams[] = {
    {"string", "left"},
    {"string", "right"}
};

static const XL6_StdlibParam xl6StdXrtIsUtf8Params[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdXrtIntFormatParams[] = {
    {"int", "value"},
    {"string", "format"}
};

static const XL6_StdlibParam xl6StdXrtUIntFormatParams[] = {
    {"uint64", "value"},
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
    {"string", "edge"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoBytesParams[] = {
    {"point", "data"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoHmacParams[] = {
    {"string", "key"},
    {"c_size", "keySize"},
    {"string", "text"},
    {"c_size", "textSize"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoHmacBytesParams[] = {
    {"point", "key"},
    {"c_size", "keySize"},
    {"point", "data"},
    {"c_size", "dataSize"}
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

static const XL6_StdlibParam xl6StdRuntimeCryptoAeadBytesParams[] = {
    {"point", "key"},
    {"c_size", "keySize"},
    {"point", "nonce"},
    {"c_size", "nonceSize"},
    {"point", "data"},
    {"c_size", "dataSize"},
    {"point", "aad"},
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

static const XL6_StdlibParam xl6StdRuntimeCryptoHkdfBytesParams[] = {
    {"point", "key"},
    {"c_size", "keySize"},
    {"point", "salt"},
    {"c_size", "saltSize"},
    {"point", "info"},
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

static const XL6_StdlibParam xl6StdRuntimeCryptoChacha20BytesParams[] = {
    {"point", "key"},
    {"c_size", "keySize"},
    {"point", "nonce"},
    {"c_size", "nonceSize"},
    {"point", "data"},
    {"c_size", "dataSize"},
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

static const XL6_StdlibParam xl6StdRuntimeSignalCodeParams[] = {
    {"int", "code"}
};

static const XL6_StdlibParam xl6StdRuntimeSignalOn0Params[] = {
    {"int", "code"},
    {"func()", "handler"},
    {"bool", "once"}
};

static const XL6_StdlibParam xl6StdRuntimeSignalOn1Params[] = {
    {"int", "code"},
    {"func(var)", "handler"},
    {"bool", "once"}
};

static const XL6_StdlibParam xl6StdRuntimeSignalSendToParams[] = {
    {"int", "code"},
    {"channel<var>", "ch"}
};

static const XL6_StdlibParam xl6StdSignalOn0Params[] = {
    {"int", "code"},
    {"func()", "handler"}
};

static const XL6_StdlibParam xl6StdSignalOn1Params[] = {
    {"int", "code"},
    {"func(var)", "handler"}
};

static const XL6_StdlibParam xl6StdSignalSendToParams[] = {
    {"channel<var>", "ch"},
    {"int", "code"}
};

static const XL6_StdlibParam xl6StdSignalCancelOnParams[] = {
    {"TaskGroup", "group"},
    {"int", "code"}
};

static const XL6_StdlibParam xl6StdTemplateCompileParams[] = {
    {"string", "text"},
    {"string", "open"},
    {"string", "close"}
};

static const XL6_StdlibParam xl6StdTemplateCompileFileParams[] = {
    {"string", "path"},
    {"string", "open"},
    {"string", "close"}
};

static const XL6_StdlibParam xl6StdTemplateRenderParams[] = {
    {"string", "text"},
    {"var", "data"},
    {"string", "open"},
    {"string", "close"}
};

static const XL6_StdlibParam xl6StdTemplateRenderFileParams[] = {
    {"string", "path"},
    {"var", "data"},
    {"string", "open"},
    {"string", "close"}
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
    {"var", "rest"}
};

static const XL6_StdlibParam xl6StdRuntimeNetEnginePostParams[] = {
    {"point", "engine"},
    {"func()", "task"},
    {"int", "affinity"}
};

static const XL6_StdlibParam xl6StdRuntimeNetEnginePostDelayedParams[] = {
    {"point", "engine"},
    {"func()", "task"},
    {"int", "delayMs"},
    {"int", "affinity"}
};

static const XL6_StdlibParam xl6StdXrtStringCaseParams[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdXrtStringTrimParams[] = {
    {"string", "text"},
    {"string", "chars"}
};

static const XL6_StdlibParam xl6StdXrtStringReplaceParams[] = {
    {"string", "text"},
    {"string", "oldText"},
    {"string", "newText"}
};

static const XL6_StdlibParam xl6StdXrtStringSizeParams[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdXrtStringSliceParams[] = {
    {"string", "text"},
    {"int", "start"},
    {"int", "count"}
};

static const XL6_StdlibParam xl6StdXrtStringCountParams[] = {
    {"string", "text"},
    {"int", "count"}
};

static const XL6_StdlibParam xl6StdXrtStringByteAtParams[] = {
    {"string", "text"},
    {"int", "index"}
};

static const XL6_StdlibParam xl6StdXrtStringRemoveParams[] = {
    {"string", "text"},
    {"string", "part"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtStringInsertParams[] = {
    {"string", "text"},
    {"int", "start"},
    {"string", "part"}
};

static const XL6_StdlibParam xl6StdXrtStringPadParams[] = {
    {"string", "text"},
    {"int", "width"},
    {"string", "pad"}
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
    {"int", "year"}
};

static const XL6_StdlibParam xl6StdXrtYearMonthParams[] = {
    {"int", "year"},
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

static const XL6_StdlibParam xl6StdOsEnvParams[] = {
    {"string", "name"},
    {"string", "defaultValue"}
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

static const XL6_StdlibParam xl6StdRuntimeFileWriteBytesParams[] = {
    {"string", "path"},
    {"point", "data"},
    {"c_size", "size"}
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
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdXrtJsonStringifyParams[] = {
    {"var", "value"},
    {"c_int", "pretty"}
};

static const XL6_StdlibParam xl6StdXrtJsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"c_int", "pretty"}
};

static const XL6_StdlibParam xl6StdJsonStringifyParams[] = {
    {"var", "value"},
    {"bool", "pretty"}
};

static const XL6_StdlibParam xl6StdXrtTableRemoveParams[] = {
    {"var", "table"},
    {"string", "key"},
    {"c_uint", "keyLen"}
};

static const XL6_StdlibParam xl6StdDictRemoveParams[] = {
    {"var", "table"},
    {"string", "key"}
};

static const XL6_StdlibParam xl6StdJsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"bool", "pretty"}
};

static const XL6_StdlibParam xl6StdJsonVisitRuntimeParams[] = {
    {"string", "text"},
    {"func(var) as bool", "visitor"}
};

static const XL6_StdlibParam xl6StdXsonTextOptionsRuntimeParams[] = {
    {"string", "text"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};

static const XL6_StdlibParam xl6StdXsonVisitOptionsRuntimeParams[] = {
    {"string", "text"},
    {"func(var) as bool", "visitor"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};

static const XL6_StdlibParam xl6StdXsonValueOptionsRuntimeParams[] = {
    {"var", "value"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};

static const XL6_StdlibParam xl6StdXsonWriteOptionsRuntimeParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
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

static const XL6_StdlibParam xl6StdXrtNetFamilyParams[] = {
    {"c_int", "family"}
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
    "    return __xl_std_xrt_string_compare(left, right, ignoreCase)\n"
    "end\n";

static const char xl6StdStringLenBody[] =
    "func __xl_std_string_len(string text) as int\n"
    "    return int(__xl_std_xrt_string_char_len(text))\n"
    "end\n";

static const char xl6StdStringByteLenBody[] =
    "func __xl_std_string_byte_len(string text) as int\n"
    "    return int(__xl_std_xrt_string_byte_len(text))\n"
    "end\n";

static const char xl6StdStringEqualsBody[] =
    "func __xl_std_string_equals(string left, string right, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_string_compare(left, right, ignoreCase) == 0\n"
    "end\n";

static const char xl6StdStringContainsBody[] =
    "func __xl_std_string_contains(string text, string part, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_string_contains(text, part, ignoreCase)\n"
    "end\n";

static const char xl6StdStringIndexOfBody[] =
    "func __xl_std_string_index_of(string text, string part, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_string_index_of(text, part, ignoreCase)\n"
    "end\n";

static const char xl6StdStringLastIndexOfBody[] =
    "func __xl_std_string_last_index_of(string text, string part, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_string_last_index_of(text, part, ignoreCase)\n"
    "end\n";

static const char xl6StdStringCountBody[] =
    "func __xl_std_string_count(string text, string part, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_string_count(text, part, ignoreCase)\n"
    "end\n";

static const char xl6StdStringStartsWithBody[] =
    "func __xl_std_string_starts_with(string text, string prefix, bool ignoreCase = false) as bool\n"
    "    return __xl_std_runtime_string_starts_with(text, prefix, ignoreCase)\n"
    "end\n";

static const char xl6StdStringEndsWithBody[] =
    "func __xl_std_string_ends_with(string text, string suffix, bool ignoreCase = false) as bool\n"
    "    return __xl_std_runtime_string_ends_with(text, suffix, ignoreCase)\n"
    "end\n";

static const char xl6StdStringHasAnyBody[] =
    "func __xl_std_string_has_any(string text, string chars) as bool\n"
    "    return __xl_std_xrt_string_has_any(text, chars)\n"
    "end\n";

static const char xl6StdStringLikeBody[] =
    "func __xl_std_string_like(string text, string pattern, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_string_like(text, pattern, ignoreCase)\n"
    "end\n";

static const char xl6StdStringApproxBody[] =
    "func __xl_std_string_approx(string left, string right) as bool\n"
    "    return __xl_std_xrt_string_approx(left, right)\n"
    "end\n";

static const char xl6StdStringSimBody[] =
    "func __xl_std_string_sim(string left, string right) as float\n"
    "    return float(__xl_std_xrt_string_similarity(left, right))\n"
    "end\n";

static const char xl6StdStringIsUtf8Body[] =
    "func __xl_std_string_is_utf8(string text) as bool\n"
    "    return __xl_std_xrt_string_is_utf8(text)\n"
    "end\n";

static const char xl6StdIntFormatBody[] =
    "func __xl_std_int_format(int value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_int_format(value, format))\n"
    "end\n";

static const char xl6StdIntFormatUIntBody[] =
    "func __xl_std_int_format(uint64 value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_uint_format(value, format))\n"
    "end\n";

static const char xl6StdFloatFormatBody[] =
    "func __xl_std_float_format(float value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_num_format(value, format))\n"
    "end\n";

static const char xl6StdStringFormatBodyInt[] =
    "func __xl_std_string_format(int value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_int_format(value, format))\n"
    "end\n";

static const char xl6StdStringFormatBodyUInt[] =
    "func __xl_std_string_format(uint64 value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_uint_format(value, format))\n"
    "end\n";

static const char xl6StdStringFormatBodyFloat[] =
    "func __xl_std_string_format(float value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_num_format(value, format))\n"
    "end\n";

static const char xl6StdStringLowerBody[] =
    "func __xl_std_string_lower(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_lower(text))\n"
    "end\n";

static const char xl6StdStringUpperBody[] =
    "func __xl_std_string_upper(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_upper(text))\n"
    "end\n";

static const char xl6StdStringTrimBody[] =
    "func __xl_std_string_trim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_trim(text, chars))\n"
    "end\n";

static const char xl6StdStringLTrimBody[] =
    "func __xl_std_string_ltrim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_ltrim(text, chars))\n"
    "end\n";

static const char xl6StdStringRTrimBody[] =
    "func __xl_std_string_rtrim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_rtrim(text, chars))\n"
    "end\n";

static const char xl6StdStringFilterBody[] =
    "func __xl_std_string_filter(string text, string chars) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_filter(text, chars))\n"
    "end\n";

static const char xl6StdStringReplaceBody[] =
    "func __xl_std_string_replace(string text, string oldText, string newText) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_replace(text, oldText, newText))\n"
    "end\n";

static const char xl6StdStringSplitBody[] =
    "func __xl_std_string_split(string text, string sep) as array\n"
    "    return array(__xl_std_runtime_string_split(text, sep))\n"
    "end\n";

static const char xl6StdStringSliceBody[] =
    "func __xl_std_string_slice(string text, int start, int count = -1) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_slice(text, start, count))\n"
    "end\n";

static const char xl6StdStringLeftBody[] =
    "func __xl_std_string_left(string text, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_left(text, count))\n"
    "end\n";

static const char xl6StdStringRightBody[] =
    "func __xl_std_string_right(string text, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_right(text, count))\n"
    "end\n";

static const char xl6StdStringMidBody[] =
    "func __xl_std_string_mid(string text, int start, int count = -1) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_slice(text, start, count))\n"
    "end\n";

static const char xl6StdStringCharAtBody[] =
    "func __xl_std_string_char_at(string text, int index) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_char_at(text, index))\n"
    "end\n";

static const char xl6StdStringByteAtBody[] =
    "func __xl_std_string_byte_at(string text, int index) as int\n"
    "    return int(__xl_std_xrt_string_byte_at(text, index))\n"
    "end\n";

static const char xl6StdStringJoinBody[] =
    "func __xl_std_string_join(array values, string sep = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_string_join(values, sep))\n"
    "end\n";

static const char xl6StdStringRepeatBody[] =
    "func __xl_std_string_repeat(string text, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_repeat(text, count))\n"
    "end\n";

static const char xl6StdStringIsEmptyBody[] =
    "func __xl_std_string_is_empty(string text) as bool\n"
    "    return __xl_std_xrt_string_is_empty(text)\n"
    "end\n";

static const char xl6StdStringIsBlankBody[] =
    "func __xl_std_string_is_blank(string text) as bool\n"
    "    return __xl_std_xrt_string_is_blank(text)\n"
    "end\n";

static const char xl6StdStringRemoveBody[] =
    "func __xl_std_string_remove(string text, string part, bool ignoreCase = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_remove(text, part, ignoreCase))\n"
    "end\n";

static const char xl6StdStringRemoveAtBody[] =
    "func __xl_std_string_remove_at(string text, int start, int count) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_remove_at(text, start, count))\n"
    "end\n";

static const char xl6StdStringInsertBody[] =
    "func __xl_std_string_insert(string text, int start, string part) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_insert(text, start, part))\n"
    "end\n";

static const char xl6StdStringPadLeftBody[] =
    "func __xl_std_string_pad_left(string text, int width, string pad = \" \") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_pad_left(text, width, pad))\n"
    "end\n";

static const char xl6StdStringPadRightBody[] =
    "func __xl_std_string_pad_right(string text, int width, string pad = \" \") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_pad_right(text, width, pad))\n"
    "end\n";

static const char xl6StdStringPadCenterBody[] =
    "func __xl_std_string_pad_center(string text, int width, string pad = \" \") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_pad_center(text, width, pad))\n"
    "end\n";

static const char xl6StdStringReverseBody[] =
    "func __xl_std_string_reverse(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_string_reverse(text))\n"
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

static const char xl6StdProcessRunCommandBody[] =
    "func __xl_std_process_run(ProcessCommand command, string input = \"\", int timeoutMs = 0) as ProcessResult\n"
    "    return command.run(input, timeoutMs)\n"
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

static const char xl6StdProcessStartCommandBody[] =
    "func __xl_std_process_start(ProcessCommand command) as Process\n"
    "    return Process(command.startNative())\n"
    "end\n";

static const char xl6StdProcessStartFileBody[] =
    "func __xl_std_process_start_file(string program, array args = []) as Process\n"
    "    return Process(program, args)\n"
    "end\n";

static const char xl6StdProcessTerminalSupportedBody[] =
    "func __xl_std_process_terminal_supported() as bool\n"
    "    return __xl_std_runtime_process_terminal_supported()\n"
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

static const char xl6StdSignalOn0Body[] =
    "func __xl_std_signal_on(int code, func() handler) as SignalHandle\n"
    "    return SignalHandle(__xl_std_runtime_signal_on0(code, handler, false))\n"
    "end\n";

static const char xl6StdSignalOn1Body[] =
    "func __xl_std_signal_on(int code, func(var) handler) as SignalHandle\n"
    "    return SignalHandle(__xl_std_runtime_signal_on1(code, handler, false))\n"
    "end\n";

static const char xl6StdSignalOnce0Body[] =
    "func __xl_std_signal_once(int code, func() handler) as SignalHandle\n"
    "    return SignalHandle(__xl_std_runtime_signal_on0(code, handler, true))\n"
    "end\n";

static const char xl6StdSignalOnce1Body[] =
    "func __xl_std_signal_once(int code, func(var) handler) as SignalHandle\n"
    "    return SignalHandle(__xl_std_runtime_signal_on1(code, handler, true))\n"
    "end\n";

static const char xl6StdSignalSupportedBody[] =
    "func __xl_std_signal_supported(int code) as bool\n"
    "    return __xl_std_runtime_signal_supported(code)\n"
    "end\n";

static const char xl6StdSignalNameBody[] =
    "func __xl_std_signal_name(int code) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_signal_name(code))\n"
    "end\n";

static const char xl6StdSignalRestoreBody[] =
    "func __xl_std_signal_restore(int code) as bool\n"
    "    return __xl_std_runtime_signal_restore(code)\n"
    "end\n";

static const char xl6StdSignalResetBody[] =
    "func __xl_std_signal_reset()\n"
    "    __xl_std_runtime_signal_reset()\n"
    "end\n";

static const char xl6StdSignalRaiseBody[] =
    "func __xl_std_signal_raise(int code) as bool\n"
    "    return __xl_std_runtime_signal_raise(code)\n"
    "end\n";

static const char xl6StdSignalCountBody[] =
    "func __xl_std_signal_count(int code = 0) as int\n"
    "    return __xl_std_runtime_signal_count(code)\n"
    "end\n";

static const char xl6StdSignalReceivedBody[] =
    "func __xl_std_signal_received(int code) as bool\n"
    "    return __xl_std_runtime_signal_received(code)\n"
    "end\n";

static const char xl6StdSignalClearBody[] =
    "func __xl_std_signal_clear(int code = 0)\n"
    "    __xl_std_runtime_signal_clear(code)\n"
    "end\n";

static const char xl6StdSignalSendToBody[] =
    "func __xl_std_signal_send_to(channel<var> ch, int code) as SignalHandle\n"
    "    return SignalHandle(__xl_std_runtime_signal_send_to(code, ch))\n"
    "end\n";

static const char xl6StdSignalCancelOnBody[] =
    "func __xl_std_signal_cancel_on(TaskGroup group, int code) as SignalHandle\n"
    "    func() cancel = func()\n"
    "        group.cancel()\n"
    "    end\n"
    "    return SignalHandle(__xl_std_runtime_signal_on0(code, cancel, false))\n"
    "end\n";

static const char xl6StdTemplateCompileBody[] =
    "func __xl_std_template_compile(string text, string open = \"{\", string close = \"}\") as Template\n"
    "    return Template(text, open, close)\n"
    "end\n";

static const char xl6StdTemplateCompileFileBody[] =
    "func __xl_std_template_compile_file(string path, string open = \"{\", string close = \"}\") as Template\n"
    "    TemplateEngine engine = TemplateEngine()\n"
    "    return engine.compileFile(path, open, close)\n"
    "end\n";

static const char xl6StdTemplateRenderBody[] =
    "func __xl_std_template_render(string text, var data = null, string open = \"{\", string close = \"}\") as string\n"
    "    Template tpl = Template(text, open, close)\n"
    "    return tpl.render(data)\n"
    "end\n";

static const char xl6StdTemplateRenderFileBody[] =
    "func __xl_std_template_render_file(string path, var data = null, string open = \"{\", string close = \"}\") as string\n"
    "    Template tpl = __xl_std_template_compile_file(path, open, close)\n"
    "    return tpl.render(data)\n"
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
    "    return __xl_std_xrt_is_leap_year(year)\n"
    "end\n";

static const char xl6StdTimeDaysInMonthBody[] =
    "func __xl_std_time_days_in_month(int year, int month) as int\n"
    "    return int(__xl_std_xrt_days_in_month(year, c_int(month)))\n"
    "end\n";

static const char xl6StdTimeDaysInYearBody[] =
    "func __xl_std_time_days_in_year(int year) as int\n"
    "    return int(__xl_std_xrt_days_in_year(year))\n"
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
    "    return __xl_std_xrt_path_is_abs(path)\n"
    "end\n";

static const char xl6StdPathNameExtBody[] =
    "func __xl_std_path_name_ext(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_name_ext(path))\n"
    "end\n";

static const char xl6StdPathNameBody[] =
    "func __xl_std_path_name(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_name(path))\n"
    "end\n";

static const char xl6StdPathExtBody[] =
    "func __xl_std_path_ext(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_ext(path))\n"
    "end\n";

static const char xl6StdPathDirBody[] =
    "func __xl_std_path_dir(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_dir(path))\n"
    "end\n";

static const char xl6StdPathJoinBody[] =
    "func __xl_std_path_join(string first, string second, ...) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_path_join(first, second, arg))\n"
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
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_normalize(path))\n"
    "end\n";

static const char xl6StdPathAbsBody[] =
    "func __xl_std_path_abs(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_abs(path))\n"
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

static const char xl6StdOsEnvBody[] =
    "func __xl_std_os_env(string name, string defaultValue = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_os_env(name, defaultValue))\n"
    "end\n";

static const char xl6StdOsPlatformBody[] =
    "func __xl_std_os_platform() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_os_platform())\n"
    "end\n";

static const char xl6StdOsArgsBody[] =
    "func __xl_std_os_args() as array\n"
    "    return array(__xl_std_runtime_os_args())\n"
    "end\n";

static const char xl6StdOsCwdBody[] =
    "func __xl_std_os_cwd() as string\n"
    "    return __xl_std_path_cwd()\n"
    "end\n";

static const char xl6StdOsSetCwdBody[] =
    "func __xl_std_os_set_cwd(string path) as bool\n"
    "    return __xl_std_path_set_cwd(path)\n"
    "end\n";

static const char xl6StdOsExecBody[] =
    "func __xl_std_os_exec(string command, int timeoutMs = 0) as string\n"
    "    return __xl_std_process_exec(command, timeoutMs)\n"
    "end\n";

static const char xl6StdOsRunBody[] =
    "func __xl_std_os_run(string command, int timeoutMs = 0) as ProcessResult\n"
    "    return __xl_std_process_run(command, timeoutMs)\n"
    "end\n";

static const char xl6StdOsStartBody[] =
    "func __xl_std_os_start(string command) as Process\n"
    "    return __xl_std_process_start(command)\n"
    "end\n";

static const char xl6StdOsStartFileBody[] =
    "func __xl_std_os_start_file(string program, array args = []) as Process\n"
    "    return __xl_std_process_start_file(program, args)\n"
    "end\n";

static const char xl6StdOsChainBody[] =
    "func __xl_std_os_chain(string command) as int\n"
    "    return int(__xl_std_runtime_process_exit_code(command, -1))\n"
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
    "func __xl_std_file_read_text(string path, int charset = -2) as string\n"
    "    point retSize = null\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_file_read_text_strict(path, c_int(charset), retSize))\n"
    "end\n";

static const char xl6StdTaskGroupBody[] =
    "func __xl_std_task_group() as TaskGroup\n"
    "    return TaskGroup()\n"
    "end\n";

static const char xl6StdTemplateEngineBody[] =
    "func __xl_std_template_engine() as TemplateEngine\n"
    "    return TemplateEngine()\n"
    "end\n";

static const char xl6StdFileReadBytesBody[] =
    "func __xl_std_file_read_bytes(string path) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_file_read_bytes(path)))\n"
    "end\n";

static const char xl6StdFileWriteBytesBody[] =
    "func __xl_std_file_write_bytes(string path, Buffer data) as bool\n"
    "    return __xl_std_runtime_file_write_bytes(path, data.data(), c_size(data.len()))\n"
    "end\n";

static const char xl6StdFileAppendBytesBody[] =
    "func __xl_std_file_append_bytes(string path, Buffer data) as bool\n"
    "    return __xl_std_runtime_file_append_bytes(path, data.data(), c_size(data.len()))\n"
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
    "    FileInfo info = FileInfo()\n"
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
    "func __xl_std_file_open(string filePath, bool readOnly = false, int charset = -2) as File\n"
    "    return File(filePath, readOnly, charset)\n"
    "end\n";

static const char xl6StdFileReadBytesAsyncBody[] =
    "extern \"xl6_xrt_file_read_bytes_async\" func __xl_std_file_read_bytes_async_native(string path) as future<point>\n"
    "async func __xl_std_file_read_bytes_async(string path) as Buffer\n"
    "    point data = await __xl_std_file_read_bytes_async_native(path)\n"
    "    return Buffer(data)\n"
    "end\n";

static const char xl6StdFileWriteBytesAsyncBody[] =
    "extern \"xl6_xrt_file_write_bytes_async\" func __xl_std_file_write_bytes_async_native(string path, point data, c_size size) as future<bool>\n"
    "func __xl_std_file_write_bytes_async(string path, Buffer data) as future<bool>\n"
    "    return __xl_std_file_write_bytes_async_native(path, data.data(), c_size(data.len()))\n"
    "end\n";

static const char xl6StdFileOpenAsyncBody[] =
    "func __xl_std_file_open_async(string path, int flags = 1, int shareFlags = 7) as AsyncFile\n"
    "    return AsyncFile(path, flags, shareFlags)\n"
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
    "    return __xl_std_xrt_json_parse(text)\n"
    "end\n";

static const char xl6StdJsonValidBody[] =
    "func __xl_std_json_valid(string text) as bool\n"
    "    return __xl_std_xrt_json_valid(text)\n"
    "end\n";

static const char xl6StdJsonReadBody[] =
    "func __xl_std_json_read(string path) as var\n"
    "    return __xl_std_xrt_json_parse_file(path)\n"
    "end\n";

static const char xl6StdJsonStringifyBody[] =
    "func __xl_std_json_stringify(var value, bool pretty = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_json_stringify(value, c_int(pretty)))\n"
    "end\n";

static const char xl6StdJsonWriteBody[] =
    "func __xl_std_json_write(string path, var value, bool pretty = false) as bool\n"
    "    return __xl_std_xrt_json_write_file(path, value, c_int(pretty)) != 0\n"
    "end\n";

static const char xl6StdJsonWriterBody[] =
    "func __xl_std_json_writer(bool pretty = false) as JsonWriter\n"
    "    return JsonWriter(pretty)\n"
    "end\n";

static const char xl6StdXsonWriterBody[] =
    "func __xl_std_xson_writer(bool pretty = false) as XsonWriter\n"
    "    return XsonWriter(pretty)\n"
    "end\n";

static const char xl6StdXsonWriterOptionsBody[] =
    "func __xl_std_xson_writer(XsonOptions options) as XsonWriter\n"
    "    return XsonWriter(options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdJsonVisitBody[] =
    "func __xl_std_json_visit(string text, func(var) as bool visitor) as int\n"
    "    return int(__xl_std_runtime_json_visit(text, visitor))\n"
    "end\n";

static const char xl6StdDictRemoveBody[] =
    "func __xl_std_dict_remove(var table, string key) as bool\n"
    "    return __xl_std_xrt_table_remove(table, key, c_uint(0))\n"
    "end\n";

static const char xl6StdXsonParseBody[] =
    "func __xl_std_xson_parse(string text) as var\n"
    "    return __xl_std_runtime_xson_parse_options(text, false, false, false)\n"
    "end\n";

static const char xl6StdXsonParseOptionsBody[] =
    "func __xl_std_xson_parse(string text, XsonOptions options) as var\n"
    "    return __xl_std_runtime_xson_parse_options(text, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXsonValidBody[] =
    "func __xl_std_xson_valid(string text) as bool\n"
    "    return __xl_std_runtime_xson_valid_options(text, false, false, false)\n"
    "end\n";

static const char xl6StdXsonValidOptionsBody[] =
    "func __xl_std_xson_valid(string text, XsonOptions options) as bool\n"
    "    return __xl_std_runtime_xson_valid_options(text, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXsonReadBody[] =
    "func __xl_std_xson_read(string path) as var\n"
    "    return __xl_std_runtime_xson_read_options(path, false, false, false)\n"
    "end\n";

static const char xl6StdXsonReadOptionsBody[] =
    "func __xl_std_xson_read(string path, XsonOptions options) as var\n"
    "    return __xl_std_runtime_xson_read_options(path, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXsonStringifyBody[] =
    "func __xl_std_xson_stringify(var value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_xson_stringify_options(value, false, false, false))\n"
    "end\n";

static const char xl6StdXsonStringifyOptionsBody[] =
    "func __xl_std_xson_stringify(var value, XsonOptions options) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_xson_stringify_options(value, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode))\n"
    "end\n";

static const char xl6StdXsonWriteBody[] =
    "func __xl_std_xson_write(string path, var value) as bool\n"
    "    return __xl_std_runtime_xson_write_options(path, value, false, false, false)\n"
    "end\n";

static const char xl6StdXsonWriteOptionsBody[] =
    "func __xl_std_xson_write(string path, var value, XsonOptions options) as bool\n"
    "    return __xl_std_runtime_xson_write_options(path, value, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXsonVisitBody[] =
    "func __xl_std_xson_visit(string text, func(var) as bool visitor) as int\n"
    "    return __xl_std_runtime_xson_visit_options(text, visitor, false, false, false)\n"
    "end\n";

static const char xl6StdXsonVisitOptionsBody[] =
    "func __xl_std_xson_visit(string text, func(var) as bool visitor, XsonOptions options) as int\n"
    "    return __xl_std_runtime_xson_visit_options(text, visitor, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXsonOptionsBody[] =
    "func __xl_std_xson_options(bool pretty = false, bool ignoreUnsupportedEncode = false, bool ignoreUnsupportedDecode = false) as XsonOptions\n"
    "    return XsonOptions(pretty, ignoreUnsupportedEncode, ignoreUnsupportedDecode)\n"
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
    "    return Buffer(point(__xl_std_xrt_buffer_from_hex(text, __xl_std_crt_strlen(text))))\n"
    "end\n";

static const char xl6StdCodecBase64EncodeBytesBody[] =
    "func __xl_std_codec_base64_encode_bytes(point data, c_size size) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_base64_encode_bytes(data, size, null))\n"
    "end\n";

static const char xl6StdCodecBase64DecodeBytesBody[] =
    "func __xl_std_codec_base64_decode_bytes(string text) as Buffer\n"
    "    return Buffer(point(__xl_std_xrt_buffer_from_base64(text, __xl_std_crt_strlen(text), null)))\n"
    "end\n";

static const char xl6StdCodecPercentEncodeBody[] =
    "func __xl_std_codec_percent_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_percent_encode(text, __xl_std_crt_strlen(text), null))\n"
    "end\n";

static const char xl6StdCodecPercentDecodeBody[] =
    "func __xl_std_codec_percent_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_percent_decode(text, __xl_std_crt_strlen(text), null))\n"
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
    "    return Buffer(point(__xl_std_runtime_encoding_to_bytes(text, toCharset)))\n"
    "end\n";

static const char xl6StdEncodingFromBytesBody[] =
    "func __xl_std_encoding_from_bytes(point data, c_size size, int fromCharset = 65001) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data, size, fromCharset))\n"
    "end\n";

static const char xl6StdEncodingConvertBytesBody[] =
    "func __xl_std_encoding_convert_bytes(point data, c_size size, int fromCharset, int toCharset) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_encoding_convert_bytes(data, size, fromCharset, toCharset)))\n"
    "end\n";

static const char xl6StdEncodingNameBody[] =
    "func __xl_std_encoding_name(int charset) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_name(charset))\n"
    "end\n";

static const char xl6StdEncodingEncodeUtf8Body[] =
    "func __xl_std_encoding_encode_utf8(string text, bool bom = false) as Buffer\n"
    "    int charset = 65001\n"
    "    if bom then charset += 1073741824 end\n"
    "    return Buffer(point(__xl_std_runtime_encoding_to_bytes(text, charset)))\n"
    "end\n";

static const char xl6StdEncodingEncodeUtf16LeBody[] =
    "func __xl_std_encoding_encode_utf16_le(string text, bool bom = false) as Buffer\n"
    "    int charset = 1200\n"
    "    if bom then charset += 1073741824 end\n"
    "    return Buffer(point(__xl_std_runtime_encoding_to_bytes(text, charset)))\n"
    "end\n";

static const char xl6StdEncodingEncodeUtf16BeBody[] =
    "func __xl_std_encoding_encode_utf16_be(string text, bool bom = false) as Buffer\n"
    "    int charset = 1201\n"
    "    if bom then charset += 1073741824 end\n"
    "    return Buffer(point(__xl_std_runtime_encoding_to_bytes(text, charset)))\n"
    "end\n";

static const char xl6StdEncodingEncodeUtf32LeBody[] =
    "func __xl_std_encoding_encode_utf32_le(string text, bool bom = false) as Buffer\n"
    "    int charset = 65005\n"
    "    if bom then charset += 1073741824 end\n"
    "    return Buffer(point(__xl_std_runtime_encoding_to_bytes(text, charset)))\n"
    "end\n";

static const char xl6StdEncodingEncodeUtf32BeBody[] =
    "func __xl_std_encoding_encode_utf32_be(string text, bool bom = false) as Buffer\n"
    "    int charset = 65006\n"
    "    if bom then charset += 1073741824 end\n"
    "    return Buffer(point(__xl_std_runtime_encoding_to_bytes(text, charset)))\n"
    "end\n";

static const char xl6StdEncodingDecodeUtf8Body[] =
    "func __xl_std_encoding_decode_utf8(Buffer data, bool bom = false) as string\n"
    "    int charset = 65001\n"
    "    if bom then charset += 1073741824 end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data.data(), c_size(data.len()), charset))\n"
    "end\n";

static const char xl6StdEncodingDecodeUtf16LeBody[] =
    "func __xl_std_encoding_decode_utf16_le(Buffer data, bool bom = false) as string\n"
    "    int charset = 1200\n"
    "    if bom then charset += 1073741824 end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data.data(), c_size(data.len()), charset))\n"
    "end\n";

static const char xl6StdEncodingDecodeUtf16BeBody[] =
    "func __xl_std_encoding_decode_utf16_be(Buffer data, bool bom = false) as string\n"
    "    int charset = 1201\n"
    "    if bom then charset += 1073741824 end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data.data(), c_size(data.len()), charset))\n"
    "end\n";

static const char xl6StdEncodingDecodeUtf32LeBody[] =
    "func __xl_std_encoding_decode_utf32_le(Buffer data, bool bom = false) as string\n"
    "    int charset = 65005\n"
    "    if bom then charset += 1073741824 end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data.data(), c_size(data.len()), charset))\n"
    "end\n";

static const char xl6StdEncodingDecodeUtf32BeBody[] =
    "func __xl_std_encoding_decode_utf32_be(Buffer data, bool bom = false) as string\n"
    "    int charset = 65006\n"
    "    if bom then charset += 1073741824 end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_encoding_from_bytes(data.data(), c_size(data.len()), charset))\n"
    "end\n";

static const char xl6StdEncodingSwapUtf16EndianBody[] =
    "func __xl_std_encoding_swap_utf16_endian(Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_encoding_swap_utf16(data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdEncodingSwapUtf32EndianBody[] =
    "func __xl_std_encoding_swap_utf32_endian(Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_encoding_swap_utf32(data.data(), c_size(data.len()))))\n"
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

static const char xl6StdCryptoSha1StreamBody[] =
    "func __xl_std_crypto_sha1_stream() as Sha1\n"
    "    return Sha1()\n"
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

static const char xl6StdCryptoSha1BytesBody[] =
    "func __xl_std_crypto_sha1_bytes(Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_sha1_bytes(data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoSha256BytesBody[] =
    "func __xl_std_crypto_sha256_bytes(Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_sha256_bytes(data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoSha384BytesBody[] =
    "func __xl_std_crypto_sha384_bytes(Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_sha384_bytes(data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoSha512BytesBody[] =
    "func __xl_std_crypto_sha512_bytes(Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_sha512_bytes(data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoHmacSha256BytesBody[] =
    "func __xl_std_crypto_hmac_sha256_bytes(Buffer key, Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_hmac_sha256_bytes(key.data(), c_size(key.len()), data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoHmacSha384BytesBody[] =
    "func __xl_std_crypto_hmac_sha384_bytes(Buffer key, Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_hmac_sha384_bytes(key.data(), c_size(key.len()), data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoHmacSha512BytesBody[] =
    "func __xl_std_crypto_hmac_sha512_bytes(Buffer key, Buffer data) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_hmac_sha512_bytes(key.data(), c_size(key.len()), data.data(), c_size(data.len()))))\n"
    "end\n";

static const char xl6StdCryptoChaCha20BytesBody[] =
    "func __xl_std_crypto_chacha20_bytes(Buffer key, Buffer nonce, Buffer data, int counter = 1) as Buffer\n"
    "    if counter < 0 then\n"
    "        return Buffer()\n"
    "    end\n"
    "    return Buffer(point(__xl_std_runtime_chacha20_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), c_uint(counter))))\n"
    "end\n";

static const char xl6StdCryptoChaCha20Poly1305EncryptBytesBody[] =
    "func __xl_std_crypto_chacha20_poly1305_encrypt_bytes(Buffer key, Buffer nonce, Buffer data, Buffer aad) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_chacha20_poly1305_encrypt_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), aad.data(), c_size(aad.len()))))\n"
    "end\n";

static const char xl6StdCryptoChaCha20Poly1305DecryptBytesBody[] =
    "func __xl_std_crypto_chacha20_poly1305_decrypt_bytes(Buffer key, Buffer nonce, Buffer data, Buffer aad) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_chacha20_poly1305_decrypt_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), aad.data(), c_size(aad.len()))))\n"
    "end\n";

static const char xl6StdCryptoAes128GcmEncryptBytesBody[] =
    "func __xl_std_crypto_aes128_gcm_encrypt_bytes(Buffer key, Buffer nonce, Buffer data, Buffer aad) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_aes128_gcm_encrypt_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), aad.data(), c_size(aad.len()))))\n"
    "end\n";

static const char xl6StdCryptoAes128GcmDecryptBytesBody[] =
    "func __xl_std_crypto_aes128_gcm_decrypt_bytes(Buffer key, Buffer nonce, Buffer data, Buffer aad) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_aes128_gcm_decrypt_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), aad.data(), c_size(aad.len()))))\n"
    "end\n";

static const char xl6StdCryptoAes256GcmEncryptBytesBody[] =
    "func __xl_std_crypto_aes256_gcm_encrypt_bytes(Buffer key, Buffer nonce, Buffer data, Buffer aad) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_aes256_gcm_encrypt_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), aad.data(), c_size(aad.len()))))\n"
    "end\n";

static const char xl6StdCryptoAes256GcmDecryptBytesBody[] =
    "func __xl_std_crypto_aes256_gcm_decrypt_bytes(Buffer key, Buffer nonce, Buffer data, Buffer aad) as Buffer\n"
    "    return Buffer(point(__xl_std_runtime_aes256_gcm_decrypt_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), aad.data(), c_size(aad.len()))))\n"
    "end\n";

static const char xl6StdCryptoHkdfSha256BytesBody[] =
    "func __xl_std_crypto_hkdf_sha256_bytes(Buffer key, Buffer salt, Buffer info, int len = 32) as Buffer\n"
    "    if len <= 0 then\n"
    "        return Buffer()\n"
    "    end\n"
    "    return Buffer(point(__xl_std_runtime_hkdf_sha256_bytes(key.data(), c_size(key.len()), salt.data(), c_size(salt.len()), info.data(), c_size(info.len()), c_size(len))))\n"
    "end\n";

static const char xl6StdCryptoHkdfSha384BytesBody[] =
    "func __xl_std_crypto_hkdf_sha384_bytes(Buffer key, Buffer salt, Buffer info, int len = 32) as Buffer\n"
    "    if len <= 0 then\n"
    "        return Buffer()\n"
    "    end\n"
    "    return Buffer(point(__xl_std_runtime_hkdf_sha384_bytes(key.data(), c_size(key.len()), salt.data(), c_size(salt.len()), info.data(), c_size(info.len()), c_size(len))))\n"
    "end\n";

static const char xl6StdCryptoRandomBytesBody[] =
    "func __xl_std_crypto_random_bytes(int len) as Buffer\n"
    "    if len <= 0 then\n"
    "        return Buffer()\n"
    "    end\n"
    "    return Buffer(point(__xl_std_runtime_random_bytes(c_size(len))))\n"
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

static const char xl6StdCryptoP256KeypairBody[] =
    "func __xl_std_crypto_p256_keypair() as var\n"
    "    return __xl_std_runtime_p256_keypair()\n"
    "end\n";

static const char xl6StdCryptoP256SharedSecretBody[] =
    "func __xl_std_crypto_p256_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_p256_shared_secret_hex(privateKeyHex, __xl_std_crt_strlen(privateKeyHex), publicKeyHex, __xl_std_crt_strlen(publicKeyHex)))\n"
    "end\n";

static const char xl6StdCryptoP384KeypairBody[] =
    "func __xl_std_crypto_p384_keypair() as var\n"
    "    return __xl_std_runtime_p384_keypair()\n"
    "end\n";

static const char xl6StdCryptoP384SharedSecretBody[] =
    "func __xl_std_crypto_p384_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_p384_shared_secret_hex(privateKeyHex, __xl_std_crt_strlen(privateKeyHex), publicKeyHex, __xl_std_crt_strlen(publicKeyHex)))\n"
    "end\n";

static const char xl6StdCryptoEcdsaVerifyBody[] =
    "func __xl_std_crypto_ecdsa_verify(string publicKeyHex, string hashHex, string signatureHex) as bool\n"
    "    return __xl_std_runtime_ecdsa_verify(publicKeyHex, __xl_std_crt_strlen(publicKeyHex), hashHex, __xl_std_crt_strlen(hashHex), signatureHex, __xl_std_crt_strlen(signatureHex))\n"
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
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_xid_make_string())\n"
    "end\n";

static const char xl6StdIdMakeBody[] =
    "func __xl_std_id_make() as Xid\n"
    "    return Xid()\n"
    "end\n";

static const char xl6StdIdDecodeBody[] =
    "func __xl_std_id_decode(string encoded) as Xid\n"
    "    return Xid(encoded)\n"
    "end\n";

static const char xl6StdIdEncodeBody[] =
    "func __xl_std_id_encode(Xid value) as string\n"
    "    return value.encode()\n"
    "end\n";

static const char xl6StdIdCompareBody[] =
    "func __xl_std_id_compare(Xid left, Xid right) as bool\n"
    "    return left.equals(right)\n"
    "end\n";

static const char xl6StdNetLocalIPBody[] =
    "func __xl_std_net_local_ip() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_ip(c_int(0)))\n"
    "end\n";

static const char xl6StdNetLocalMACBody[] =
    "func __xl_std_net_local_mac() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_mac())\n"
    "end\n";

static const char xl6StdNetLocalNameBody[] =
    "func __xl_std_net_local_name() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_name())\n"
    "end\n";

static const char xl6StdNetInterfacesBody[] =
    "func __xl_std_net_interfaces() as NetInterfaceList\n"
    "    return NetInterfaceList()\n"
    "end\n";

static const char xl6StdNetInterfaceIndexBody[] =
    "extern \"xl6_xrt_net_interface_index\" func __xl_std_net_interface_index_native(string name, int family) as int\n"
    "func __xl_std_net_interface_index(string name, int family = 0) as int\n"
    "    return __xl_std_net_interface_index_native(name, family)\n"
    "end\n";

static const char xl6StdNetInterfaceNameBody[] =
    "extern \"xl6_xrt_net_interface_name_by_index\" func __xl_std_net_interface_name_native(int index, int family) as point\n"
    "func __xl_std_net_interface_name(int index, int family = 0) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_net_interface_name_native(index, family))\n"
    "end\n";

static const char xl6StdNetResolveBody[] =
    "func __xl_std_net_resolve(string host, int port = 0) as NetAddress\n"
    "    return NetAddress(host, port)\n"
    "end\n";

static const char xl6StdNetResolveAllBody[] =
    "func __xl_std_net_resolve_all(string host, int port = 0, int family = 0) as NetAddressList\n"
    "    return NetAddressList(host, port, family)\n"
    "end\n";

static const char xl6StdNetResolveAsyncBody[] =
    "extern \"xl6_xrt_net_resolve_async\" func __xl_std_net_resolve_async_native(string host, int port, int family) as future<NetAddressList>\n"
    "func __xl_std_net_resolve_async(string host, int port = 0, int family = 0) as future<NetAddressList>\n"
    "    return __xl_std_net_resolve_async_native(host, port, family)\n"
    "end\n";

static const char xl6StdNetStatsBody[] =
    "extern \"xl6_xrt_net_runtime_stats\" func __xl_std_net_stats_native() as point\n"
    "func __xl_std_net_stats() as NetEngineStats\n"
    "    return NetEngineStats(point(__xl_std_net_stats_native()))\n"
    "end\n";

static const char xl6StdNetResolverStatsBody[] =
    "extern \"xl6_xrt_net_resolver_stats\" func __xl_std_net_resolver_stats_native() as point\n"
    "func __xl_std_net_resolver_stats() as NetResolverStats\n"
    "    return NetResolverStats(point(__xl_std_net_resolver_stats_native()))\n"
    "end\n";

static const char xl6StdNetProxyBody[] =
    "func __xl_std_net_proxy(int type, string host, int port, string user = \"\", string password = \"\") as NetProxy\n"
    "    return NetProxy(type, host, port, user, password)\n"
    "end\n";

static const char xl6StdNetEngineBody[] =
	"func __xl_std_net_engine(int workerCount = 0) as NetEngine\n"
	"    return NetEngine(workerCount)\n"
    "end\n";

static const char xl6StdTcpConnectConfigBody[] =
    "func __xl_std_tcp_connect_config(string host, int port) as TcpConnectConfig\n"
    "    return TcpConnectConfig(host, port)\n"
    "end\n";

static const char xl6StdTcpListenConfigBody[] =
    "func __xl_std_tcp_listen_config(int port, string host = \"127.0.0.1\") as TcpListenConfig\n"
    "    return TcpListenConfig(host, port)\n"
    "end\n";

static const char xl6StdUdpConfigBody[] =
    "func __xl_std_udp_config(int port, string host = \"127.0.0.1\") as UdpConfig\n"
    "    return UdpConfig(host, port)\n"
    "end\n";

static const char xl6StdTlsConnectBody[] =
    "extern \"xl6_xrt_tls_connect\" func __xl_std_tls_connect_native(string host, uint16 port, point config, int timeoutMs) as point\n"
    "func __xl_std_tls_connect(string host, int port, TlsConfig config, int timeoutMs = 5000) as TlsStream\n"
    "    if port <= 0 or port > 65535 then\n"
    "        return TlsStream(point(null))\n"
    "    end\n"
    "    return TlsStream(point(__xl_std_tls_connect_native(host, uint16(port), config.nativeHandle(), timeoutMs)))\n"
    "end\n";

static const char xl6StdTlsConnectAsyncBody[] =
    "extern \"xl6_xrt_tls_connect_async\" func __xl_std_tls_connect_async_native(string host, uint16 port, point config, int timeoutMs) as future<TlsStream>\n"
    "func __xl_std_tls_connect_async(string host, int port, TlsConfig config, int timeoutMs = 5000) as future<TlsStream>\n"
    "    if port <= 0 or port > 65535 then\n"
    "        return __xl_std_tls_connect_async_native(\"\", uint16(0), config.nativeHandle(), timeoutMs)\n"
    "    end\n"
    "    return __xl_std_tls_connect_async_native(host, uint16(port), config.nativeHandle(), timeoutMs)\n"
    "end\n";

static const char xl6StdTlsListenBody[] =
    "extern \"xl6_xrt_tls_listen\" func __xl_std_tls_listen_native(string host, uint16 port, point config, c_uint backlog) as point\n"
    "func __xl_std_tls_listen(int port, TlsConfig config, string host = \"127.0.0.1\", int backlog = 128) as TlsServer\n"
    "    if port < 0 or port > 65535 then\n"
    "        return TlsServer(point(null))\n"
    "    end\n"
    "    if backlog < 0 then\n"
    "        backlog = 0\n"
    "    end\n"
    "    return TlsServer(point(__xl_std_tls_listen_native(host, uint16(port), config.nativeHandle(), c_uint(backlog))))\n"
    "end\n";

static const char xl6StdTcpConnectBody[] =
    "func __xl_std_tcp_connect(string host, int port, int timeoutMs = 5000) as TcpStream\n"
    "    if port <= 0 or port > 65535 then\n"
    "        return TcpStream()\n"
    "    end\n"
    "    return TcpStream(point(__xl_std_xrt_tcp_connect(host, uint16(port), timeoutMs)))\n"
    "end\n";

static const char xl6StdTcpConnectAsyncBody[] =
    "func __xl_std_tcp_connect_async(string host, int port, int timeoutMs = 5000) as future<TcpStream>\n"
    "    if port <= 0 or port > 65535 then\n"
    "        return __xl_std_xrt_tcp_connect_async(\"\", uint16(0), timeoutMs)\n"
    "    end\n"
    "    return __xl_std_xrt_tcp_connect_async(host, uint16(port), timeoutMs)\n"
    "end\n";

static const char xl6StdTcpListenBody[] =
    "func __xl_std_tcp_listen(int port, string host = \"127.0.0.1\", int backlog = 128) as TcpServer\n"
    "    if port < 0 or port > 65535 then\n"
    "        return TcpServer()\n"
    "    end\n"
    "    if backlog < 0 then\n"
    "        backlog = 0\n"
    "    end\n"
    "    return TcpServer(point(__xl_std_xrt_tcp_listen(host, uint16(port), c_uint(backlog))))\n"
    "end\n";

static const char xl6StdUdpBindBody[] =
    "func __xl_std_udp_bind(int port, string host = \"127.0.0.1\") as UdpSocket\n"
    "    if port < 0 or port > 65535 then\n"
    "        return UdpSocket()\n"
    "    end\n"
    "    return UdpSocket(point(__xl_std_xrt_udp_bind(host, uint16(port))))\n"
    "end\n";

static const char xl6StdPrintBody[] =
    "func __xl_std_print(...)\n"
    "    __xl_std_runtime_print(arg, false)\n"
    "end\n";

static const char xl6StdPrintlnBody[] =
    "func __xl_std_println(...)\n"
    "    __xl_std_runtime_print(arg, true)\n"
    "end\n";

static const char xl6StdWriteBody[] =
    "func __xl_std_write(string text)\n"
    "    __xl_std_runtime_write(text)\n"
    "end\n";

static const char xl6StdWritelnBody[] =
    "func __xl_std_writeln(string text = \"\")\n"
    "    __xl_std_runtime_writeln(text)\n"
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
    {"json", "visitComplete", XL6_AST_INT, "0"},
    {"json", "visitStopped", XL6_AST_INT, "1"},
    {"json", "visitInvalid", XL6_AST_INT, "-1"},
    {"xson", "visitComplete", XL6_AST_INT, "0"},
    {"xson", "visitStopped", XL6_AST_INT, "1"},
    {"xson", "visitInvalid", XL6_AST_INT, "-1"},
    {"file", "seekSet", XL6_AST_INT, "0"},
    {"file", "seekCur", XL6_AST_INT, "1"},
    {"file", "seekEnd", XL6_AST_INT, "2"},
    {"file", "asyncRead", XL6_AST_INT, "1"},
    {"file", "asyncWrite", XL6_AST_INT, "2"},
    {"file", "asyncCreate", XL6_AST_INT, "4"},
    {"file", "asyncTruncate", XL6_AST_INT, "8"},
    {"file", "shareRead", XL6_AST_INT, "1"},
    {"file", "shareWrite", XL6_AST_INT, "2"},
    {"file", "shareDelete", XL6_AST_INT, "4"},
    {"process", "stdin", XL6_AST_INT, "0"},
    {"process", "stdout", XL6_AST_INT, "1"},
    {"process", "stderr", XL6_AST_INT, "2"},
    {"process", "stdioInherit", XL6_AST_INT, "0"},
    {"process", "stdioPipe", XL6_AST_INT, "1"},
    {"process", "stdioNull", XL6_AST_INT, "2"},
    {"process", "stateRunning", XL6_AST_INT, "0"},
    {"process", "stateExited", XL6_AST_INT, "1"},
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
    {"net", "proxyNone", XL6_AST_INT, "0"},
    {"net", "proxySocks5", XL6_AST_INT, "1"},
    {"net", "proxyHttpConnect", XL6_AST_INT, "2"},
    {"net", "familyAny", XL6_AST_INT, "0"},
    {"net", "familyIPv4", XL6_AST_INT, "4"},
    {"net", "familyIPv6", XL6_AST_INT, "6"},
    {"net", "resultError", XL6_AST_INT, "-1"},
    {"net", "resultOk", XL6_AST_INT, "0"},
    {"net", "resultAgain", XL6_AST_INT, "1"},
    {"net", "resultClosed", XL6_AST_INT, "2"},
    {"net", "resultTruncated", XL6_AST_INT, "3"},
    {"net", "resultTimeout", XL6_AST_INT, "4"},
    {"net", "resultCancelled", XL6_AST_INT, "5"},
    {"net", "interfaceUp", XL6_AST_INT, "1"},
    {"net", "interfaceRunning", XL6_AST_INT, "2"},
    {"net", "interfaceLoopback", XL6_AST_INT, "4"},
    {"net", "interfaceBroadcast", XL6_AST_INT, "8"},
    {"net", "interfacePointToPoint", XL6_AST_INT, "16"},
    {"net", "interfaceMulticast", XL6_AST_INT, "32"},
    {"signal", "HUP", XL6_AST_INT, "1"},
    {"signal", "INT", XL6_AST_INT, "2"},
    {"signal", "TERM", XL6_AST_INT, "15"},
    {"signal", "BREAK", XL6_AST_INT, "1001"},
    {"signal", "CLOSE", XL6_AST_INT, "1002"},
    {"signal", "LOGOFF", XL6_AST_INT, "1003"},
    {"signal", "SHUTDOWN", XL6_AST_INT, "1004"}
};

const XL6_StdlibFunc xl6StdlibFuncs[] = {
    {"__crt_string", "strlen", "__xl_std_crt_strlen", "strlen", xl6StdStringLenParams, 1, "c_size", NULL},
    {"__xl_runtime", "takeCstr", "__xl_std_runtime_take_cstr", "xl6_xrt_take_cstr", xl6StdRuntimeTakeCstrParams, 1, "string", NULL},
    {"__xl_runtime", "print", "__xl_std_runtime_print", "xl6_xrt_print", xl6StdRuntimePrintParams, 2, "", NULL},
    {"__xl_runtime", "write", "__xl_std_runtime_write", "xl6_xrt_write", xl6StdRuntimeStringTextParams, 1, "", NULL},
    {"__xl_runtime", "writeln", "__xl_std_runtime_writeln", "xl6_xrt_writeln", xl6StdRuntimeStringTextParams, 1, "", NULL},
    {"__xl_runtime", "stringSplit", "__xl_std_runtime_string_split", "xl6_xrt_string_split", xl6StdRuntimeStringSplitParams, 2, "var", NULL},
    {"__xl_runtime", "stringChars", "__xl_std_runtime_string_chars", "xl6_xrt_string_chars", xl6StdRuntimeStringTextParams, 1, "var", NULL},
    {"__xl_runtime", "stringBytes", "__xl_std_runtime_string_bytes", "xl6_xrt_string_bytes", xl6StdRuntimeStringTextParams, 1, "var", NULL},
    {"__xl_runtime", "stringJoin", "__xl_std_runtime_string_join", "xl6_xrt_string_join", xl6StdRuntimeStringJoinParams, 2, "point", NULL},
    {"__xl_runtime", "stringSplitLines", "__xl_std_runtime_string_split_lines", "xl6_xrt_string_split_lines", xl6StdRuntimeStringTextParams, 1, "var", NULL},
    {"__xl_runtime", "stringStartsWith", "__xl_std_runtime_string_starts_with", "xl6_xrt_string_starts_with", xl6StdRuntimeStringEdgeParams, 3, "c_bool", NULL},
    {"__xl_runtime", "stringEndsWith", "__xl_std_runtime_string_ends_with", "xl6_xrt_string_ends_with", xl6StdRuntimeStringEdgeParams, 3, "c_bool", NULL},
    {"__xl_runtime", "fileReadBytes", "__xl_std_runtime_file_read_bytes", "xl6_xrt_buffer_file_read_bytes", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xl_runtime", "fileWriteBytes", "__xl_std_runtime_file_write_bytes", "xl6_xrt_buffer_file_write_bytes", xl6StdRuntimeFileWriteBytesParams, 3, "c_bool", NULL},
    {"__xl_runtime", "fileAppendBytes", "__xl_std_runtime_file_append_bytes", "xl6_xrt_buffer_file_append_bytes", xl6StdRuntimeFileWriteBytesParams, 3, "c_bool", NULL},
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
    {"__xl_runtime", "sha1Bytes", "__xl_std_runtime_sha1_bytes", "xl6_xrt_sha1_bytes", xl6StdRuntimeCryptoBytesParams, 2, "point", NULL},
    {"__xl_runtime", "sha256Bytes", "__xl_std_runtime_sha256_bytes", "xl6_xrt_sha256_bytes", xl6StdRuntimeCryptoBytesParams, 2, "point", NULL},
    {"__xl_runtime", "sha384Bytes", "__xl_std_runtime_sha384_bytes", "xl6_xrt_sha384_bytes", xl6StdRuntimeCryptoBytesParams, 2, "point", NULL},
    {"__xl_runtime", "sha512Bytes", "__xl_std_runtime_sha512_bytes", "xl6_xrt_sha512_bytes", xl6StdRuntimeCryptoBytesParams, 2, "point", NULL},
    {"__xl_runtime", "hmacSha256Bytes", "__xl_std_runtime_hmac_sha256_bytes", "xl6_xrt_hmac_sha256_bytes", xl6StdRuntimeCryptoHmacBytesParams, 4, "point", NULL},
    {"__xl_runtime", "hmacSha384Bytes", "__xl_std_runtime_hmac_sha384_bytes", "xl6_xrt_hmac_sha384_bytes", xl6StdRuntimeCryptoHmacBytesParams, 4, "point", NULL},
    {"__xl_runtime", "hmacSha512Bytes", "__xl_std_runtime_hmac_sha512_bytes", "xl6_xrt_hmac_sha512_bytes", xl6StdRuntimeCryptoHmacBytesParams, 4, "point", NULL},
    {"__xl_runtime", "chacha20Bytes", "__xl_std_runtime_chacha20_bytes", "xl6_xrt_chacha20_bytes", xl6StdRuntimeCryptoChacha20BytesParams, 7, "point", NULL},
    {"__xl_runtime", "chacha20Poly1305EncryptBytes", "__xl_std_runtime_chacha20_poly1305_encrypt_bytes", "xl6_xrt_chacha20_poly1305_encrypt_bytes", xl6StdRuntimeCryptoAeadBytesParams, 8, "point", NULL},
    {"__xl_runtime", "chacha20Poly1305DecryptBytes", "__xl_std_runtime_chacha20_poly1305_decrypt_bytes", "xl6_xrt_chacha20_poly1305_decrypt_bytes", xl6StdRuntimeCryptoAeadBytesParams, 8, "point", NULL},
    {"__xl_runtime", "aes128GcmEncryptBytes", "__xl_std_runtime_aes128_gcm_encrypt_bytes", "xl6_xrt_aes128_gcm_encrypt_bytes", xl6StdRuntimeCryptoAeadBytesParams, 8, "point", NULL},
    {"__xl_runtime", "aes128GcmDecryptBytes", "__xl_std_runtime_aes128_gcm_decrypt_bytes", "xl6_xrt_aes128_gcm_decrypt_bytes", xl6StdRuntimeCryptoAeadBytesParams, 8, "point", NULL},
    {"__xl_runtime", "aes256GcmEncryptBytes", "__xl_std_runtime_aes256_gcm_encrypt_bytes", "xl6_xrt_aes256_gcm_encrypt_bytes", xl6StdRuntimeCryptoAeadBytesParams, 8, "point", NULL},
    {"__xl_runtime", "aes256GcmDecryptBytes", "__xl_std_runtime_aes256_gcm_decrypt_bytes", "xl6_xrt_aes256_gcm_decrypt_bytes", xl6StdRuntimeCryptoAeadBytesParams, 8, "point", NULL},
    {"__xl_runtime", "hkdfSha256Bytes", "__xl_std_runtime_hkdf_sha256_bytes", "xl6_xrt_hkdf_sha256_bytes", xl6StdRuntimeCryptoHkdfBytesParams, 7, "point", NULL},
    {"__xl_runtime", "hkdfSha384Bytes", "__xl_std_runtime_hkdf_sha384_bytes", "xl6_xrt_hkdf_sha384_bytes", xl6StdRuntimeCryptoHkdfBytesParams, 7, "point", NULL},
    {"__xl_runtime", "randomBytes", "__xl_std_runtime_random_bytes", "xl6_xrt_random_bytes", xl6StdRuntimeCryptoRandomParams, 1, "point", NULL},
    {"__xl_runtime", "ed25519Keypair", "__xl_std_runtime_ed25519_keypair", "xl6_xrt_ed25519_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "ed25519PublicKeyHex", "__xl_std_runtime_ed25519_public_key_hex", "xl6_xrt_ed25519_public_key_hex", xl6StdRuntimeCryptoTextHexParams, 2, "point", NULL},
    {"__xl_runtime", "ed25519SignHex", "__xl_std_runtime_ed25519_sign_hex", "xl6_xrt_ed25519_sign_hex", xl6StdRuntimeCryptoSignParams, 4, "point", NULL},
    {"__xl_runtime", "ed25519Verify", "__xl_std_runtime_ed25519_verify", "xl6_xrt_ed25519_verify", xl6StdRuntimeCryptoVerifyParams, 6, "c_bool", NULL},
    {"__xl_runtime", "x25519Keypair", "__xl_std_runtime_x25519_keypair", "xl6_xrt_x25519_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "x25519SharedSecretHex", "__xl_std_runtime_x25519_shared_secret_hex", "xl6_xrt_x25519_shared_secret_hex", xl6StdRuntimeCryptoSharedSecretParams, 4, "point", NULL},
    {"__xl_runtime", "x448Keypair", "__xl_std_runtime_x448_keypair", "xl6_xrt_x448_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "x448SharedSecretHex", "__xl_std_runtime_x448_shared_secret_hex", "xl6_xrt_x448_shared_secret_hex", xl6StdRuntimeCryptoSharedSecretParams, 4, "point", NULL},
    {"__xl_runtime", "p256Keypair", "__xl_std_runtime_p256_keypair", "xl6_xrt_p256_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "p256SharedSecretHex", "__xl_std_runtime_p256_shared_secret_hex", "xl6_xrt_p256_shared_secret_hex", xl6StdRuntimeCryptoSharedSecretParams, 4, "point", NULL},
    {"__xl_runtime", "p384Keypair", "__xl_std_runtime_p384_keypair", "xl6_xrt_p384_keypair", NULL, 0, "var", NULL},
    {"__xl_runtime", "p384SharedSecretHex", "__xl_std_runtime_p384_shared_secret_hex", "xl6_xrt_p384_shared_secret_hex", xl6StdRuntimeCryptoSharedSecretParams, 4, "point", NULL},
    {"__xl_runtime", "ecdsaVerify", "__xl_std_runtime_ecdsa_verify", "xl6_xrt_ecdsa_verify", xl6StdRuntimeCryptoVerifyParams, 6, "c_bool", NULL},
    {"__xl_runtime", "rsaPssVerify", "__xl_std_runtime_rsa_pss_verify", "xl6_xrt_rsa_pss_verify", xl6StdRuntimeCryptoRsaVerifyParams, 8, "c_bool", NULL},
    {"__xl_runtime", "rsaPkcs1Verify", "__xl_std_runtime_rsa_pkcs1_verify", "xl6_xrt_rsa_pkcs1_verify", xl6StdRuntimeCryptoRsaVerifyParams, 8, "c_bool", NULL},
    {"__xl_runtime", "processExec", "__xl_std_runtime_process_exec", "xl6_xrt_process_exec", xl6StdRuntimeProcessExecParams, 2, "point", NULL},
    {"__xl_runtime", "processSuccess", "__xl_std_runtime_process_success", "xl6_xrt_process_success", xl6StdRuntimeProcessExecParams, 2, "c_bool", NULL},
    {"__xl_runtime", "processExitCode", "__xl_std_runtime_process_exit_code", "xl6_xrt_process_exit_code", xl6StdRuntimeProcessExecParams, 2, "c_int", NULL},
    {"__xl_runtime", "processRun", "__xl_std_runtime_process_run", "xl6_xrt_process_run_shell_result", xl6StdRuntimeProcessExecParams, 2, "point", NULL},
    {"__xl_runtime", "processRunFile", "__xl_std_runtime_process_run_file", "xl6_xrt_process_run_file_result", xl6StdRuntimeProcessRunFileParams, 3, "point", NULL},
    {"__xl_runtime", "processPipeline", "__xl_std_runtime_process_pipeline", "xl6_xrt_process_pipeline_run", xl6StdRuntimeProcessPipelineParams, 2, "point", NULL},
    {"__xl_runtime", "processTerminalSupported", "__xl_std_runtime_process_terminal_supported", "xl6_xrt_process_terminal_supported", NULL, 0, "c_bool", NULL},
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
    {"__xl_runtime", "signalOn0", "__xl_std_runtime_signal_on0", "xl6_xrt_signal_on0", xl6StdRuntimeSignalOn0Params, 3, "point", NULL},
    {"__xl_runtime", "signalOn1", "__xl_std_runtime_signal_on1", "xl6_xrt_signal_on1", xl6StdRuntimeSignalOn1Params, 3, "point", NULL},
    {"__xl_runtime", "signalSendTo", "__xl_std_runtime_signal_send_to", "xl6_xrt_signal_send_to", xl6StdRuntimeSignalSendToParams, 2, "point", NULL},
    {"__xl_runtime", "signalSupported", "__xl_std_runtime_signal_supported", "xl6_xrt_signal_supported", xl6StdRuntimeSignalCodeParams, 1, "bool", NULL},
    {"__xl_runtime", "signalName", "__xl_std_runtime_signal_name", "xl6_xrt_signal_name", xl6StdRuntimeSignalCodeParams, 1, "point", NULL},
    {"__xl_runtime", "signalRestore", "__xl_std_runtime_signal_restore", "xl6_xrt_signal_restore", xl6StdRuntimeSignalCodeParams, 1, "bool", NULL},
    {"__xl_runtime", "signalReset", "__xl_std_runtime_signal_reset", "xl6_xrt_signal_reset", NULL, 0, "", NULL},
    {"__xl_runtime", "signalRaise", "__xl_std_runtime_signal_raise", "xl6_xrt_signal_raise", xl6StdRuntimeSignalCodeParams, 1, "bool", NULL},
    {"__xl_runtime", "signalCount", "__xl_std_runtime_signal_count", "xl6_xrt_signal_count", xl6StdRuntimeSignalCodeParams, 1, "int", NULL},
    {"__xl_runtime", "signalReceived", "__xl_std_runtime_signal_received", "xl6_xrt_signal_received", xl6StdRuntimeSignalCodeParams, 1, "bool", NULL},
    {"__xl_runtime", "signalClear", "__xl_std_runtime_signal_clear", "xl6_xrt_signal_clear", xl6StdRuntimeSignalCodeParams, 1, "", NULL},
    {"__xl_runtime", "encodingCharSize", "__xl_std_runtime_encoding_char_size", "xl6_xrt_encoding_char_size", xl6StdRuntimeEncodingCharsetParams, 1, "int", NULL},
    {"__xl_runtime", "encodingIsUtf8Text", "__xl_std_runtime_encoding_is_utf8_text", "xl6_xrt_encoding_is_utf8_text", xl6StdRuntimeEncodingTextParams, 1, "bool", NULL},
    {"__xl_runtime", "encodingIsUtf8Bytes", "__xl_std_runtime_encoding_is_utf8_bytes", "xl6_xrt_encoding_is_utf8_bytes", xl6StdRuntimeEncodingBytesParams, 2, "bool", NULL},
    {"__xl_runtime", "encodingDetectText", "__xl_std_runtime_encoding_detect_text", "xl6_xrt_encoding_detect_text", xl6StdRuntimeEncodingTextDetectParams, 2, "int", NULL},
    {"__xl_runtime", "encodingDetectBytes", "__xl_std_runtime_encoding_detect_bytes", "xl6_xrt_encoding_detect_bytes", xl6StdRuntimeEncodingBytesDetectParams, 3, "int", NULL},
    {"__xl_runtime", "encodingToBytes", "__xl_std_runtime_encoding_to_bytes", "xl6_xrt_encoding_to_bytes", xl6StdRuntimeEncodingToBytesParams, 2, "point", NULL},
    {"__xl_runtime", "encodingFromBytes", "__xl_std_runtime_encoding_from_bytes", "xl6_xrt_encoding_from_bytes", xl6StdRuntimeEncodingFromBytesParams, 3, "point", NULL},
    {"__xl_runtime", "encodingConvertBytes", "__xl_std_runtime_encoding_convert_bytes", "xl6_xrt_encoding_convert_bytes", xl6StdRuntimeEncodingConvertBytesParams, 4, "point", NULL},
    {"__xl_runtime", "encodingName", "__xl_std_runtime_encoding_name", "xl6_xrt_encoding_name", xl6StdRuntimeEncodingCharsetParams, 1, "point", NULL},
    {"__xl_runtime", "encodingSwapUtf16", "__xl_std_runtime_encoding_swap_utf16", "xl6_xrt_encoding_swap_utf16", xl6StdRuntimeEncodingBytesParams, 2, "point", NULL},
    {"__xl_runtime", "encodingSwapUtf32", "__xl_std_runtime_encoding_swap_utf32", "xl6_xrt_encoding_swap_utf32", xl6StdRuntimeEncodingBytesParams, 2, "point", NULL},
    {"__xl_runtime", "dirList", "__xl_std_runtime_dir_list", "xl6_xrt_dir_list", xl6StdRuntimeDirListParams, 3, "var", NULL},
    {"__xl_runtime", "dirGlob", "__xl_std_runtime_dir_glob", "xl6_xrt_dir_glob", xl6StdRuntimeDirGlobParams, 4, "var", NULL},
    {"__xl_runtime", "dirEntries", "__xl_std_runtime_dir_entries", "xl6_xrt_dir_entries", xl6StdRuntimeDirEntriesParams, 5, "var", NULL},
    {"__xl_runtime", "pathJoin", "__xl_std_runtime_path_join", "xl6_xrt_path_join", xl6StdRuntimePathJoinParams, 3, "point", NULL},
    {"__xl_runtime", "netEnginePost", "__xl_std_runtime_net_engine_post", "xl6_xrt_net_engine_post", xl6StdRuntimeNetEnginePostParams, 3, "c_bool", NULL},
    {"__xl_runtime", "netEnginePostDelayed", "__xl_std_runtime_net_engine_post_delayed", "xl6_xrt_net_engine_post_delayed", xl6StdRuntimeNetEnginePostDelayedParams, 4, "c_bool", NULL},
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

    {"__xrt_string", "compare", "__xl_std_xrt_string_compare", "xl6_xrt_string_compare", xl6StdXrtStrCompParams, 3, "c_int", NULL},
    {"__xrt_string", "byteLen", "__xl_std_xrt_string_byte_len", "xl6_xrt_string_byte_len", xl6StdXrtStringSizeParams, 1, "c_size", NULL},
    {"__xrt_string", "charLen", "__xl_std_xrt_string_char_len", "xl6_xrt_string_char_len", xl6StdXrtStringSizeParams, 1, "c_size", NULL},
    {"__xrt_string", "contains", "__xl_std_xrt_string_contains", "xl6_xrt_string_contains", xl6StdXrtInStrParams, 3, "c_bool", NULL},
    {"__xrt_string", "indexOf", "__xl_std_xrt_string_index_of", "xl6_xrt_string_index_of", xl6StdXrtInStrParams, 3, "int", NULL},
    {"__xrt_string", "lastIndexOf", "__xl_std_xrt_string_last_index_of", "xl6_xrt_string_last_index_of", xl6StdXrtInStrParams, 3, "int", NULL},
    {"__xrt_string", "count", "__xl_std_xrt_string_count", "xl6_xrt_string_count", xl6StdXrtInStrParams, 3, "int", NULL},
    {"__xrt_string", "hasAny", "__xl_std_xrt_string_has_any", "xl6_xrt_string_has_any", xl6StdXrtCheckStrParams, 2, "c_bool", NULL},
    {"__xrt_string", "like", "__xl_std_xrt_string_like", "xl6_xrt_string_like", xl6StdXrtStrLikeParams, 3, "c_bool", NULL},
    {"__xrt_string", "approx", "__xl_std_xrt_string_approx", "xl6_xrt_string_approx", xl6StdXrtStrApproxParams, 2, "c_bool", NULL},
    {"__xrt_string", "similarity", "__xl_std_xrt_string_similarity", "xl6_xrt_string_similarity", xl6StdXrtStrApproxParams, 2, "c_double", NULL},
    {"__xrt_string", "isUtf8", "__xl_std_xrt_string_is_utf8", "xl6_xrt_string_is_utf8", xl6StdXrtIsUtf8Params, 1, "c_bool", NULL},
    {"__xrt_string", "intFormat", "__xl_std_xrt_string_int_format", "xl6_xrt_string_int_format", xl6StdXrtIntFormatParams, 2, "point", NULL},
    {"__xrt_string", "uintFormat", "__xl_std_xrt_string_uint_format", "xl6_xrt_string_uint_format", xl6StdXrtUIntFormatParams, 2, "point", NULL},
    {"__xrt_string", "numFormat", "__xl_std_xrt_string_num_format", "xl6_xrt_string_num_format", xl6StdXrtNumFormatParams, 2, "point", NULL},
    {"__xrt_string", "lower", "__xl_std_xrt_string_lower", "xl6_xrt_string_lower", xl6StdXrtStringCaseParams, 1, "point", NULL},
    {"__xrt_string", "upper", "__xl_std_xrt_string_upper", "xl6_xrt_string_upper", xl6StdXrtStringCaseParams, 1, "point", NULL},
    {"__xrt_string", "trim", "__xl_std_xrt_string_trim", "xl6_xrt_string_trim", xl6StdXrtStringTrimParams, 2, "point", NULL},
    {"__xrt_string", "ltrim", "__xl_std_xrt_string_ltrim", "xl6_xrt_string_ltrim", xl6StdXrtStringTrimParams, 2, "point", NULL},
    {"__xrt_string", "rtrim", "__xl_std_xrt_string_rtrim", "xl6_xrt_string_rtrim", xl6StdXrtStringTrimParams, 2, "point", NULL},
    {"__xrt_string", "filter", "__xl_std_xrt_string_filter", "xl6_xrt_string_filter", xl6StdXrtStringTrimParams, 2, "point", NULL},
    {"__xrt_string", "replace", "__xl_std_xrt_string_replace", "xl6_xrt_string_replace", xl6StdXrtStringReplaceParams, 3, "point", NULL},
    {"__xrt_string", "slice", "__xl_std_xrt_string_slice", "xl6_xrt_string_slice", xl6StdXrtStringSliceParams, 3, "point", NULL},
    {"__xrt_string", "left", "__xl_std_xrt_string_left", "xl6_xrt_string_left", xl6StdXrtStringCountParams, 2, "point", NULL},
    {"__xrt_string", "right", "__xl_std_xrt_string_right", "xl6_xrt_string_right", xl6StdXrtStringCountParams, 2, "point", NULL},
    {"__xrt_string", "charAt", "__xl_std_xrt_string_char_at", "xl6_xrt_string_char_at", xl6StdXrtStringByteAtParams, 2, "point", NULL},
    {"__xrt_string", "byteAt", "__xl_std_xrt_string_byte_at", "xl6_xrt_string_byte_at", xl6StdXrtStringByteAtParams, 2, "c_int", NULL},
    {"__xrt_string", "repeat", "__xl_std_xrt_string_repeat", "xl6_xrt_string_repeat", xl6StdXrtStringCountParams, 2, "point", NULL},
    {"__xrt_string", "isEmpty", "__xl_std_xrt_string_is_empty", "xl6_xrt_string_is_empty", xl6StdXrtStringSizeParams, 1, "c_bool", NULL},
    {"__xrt_string", "isBlank", "__xl_std_xrt_string_is_blank", "xl6_xrt_string_is_blank", xl6StdXrtStringSizeParams, 1, "c_bool", NULL},
    {"__xrt_string", "remove", "__xl_std_xrt_string_remove", "xl6_xrt_string_remove", xl6StdXrtStringRemoveParams, 3, "point", NULL},
    {"__xrt_string", "removeAt", "__xl_std_xrt_string_remove_at", "xl6_xrt_string_remove_at", xl6StdXrtStringSliceParams, 3, "point", NULL},
    {"__xrt_string", "insert", "__xl_std_xrt_string_insert", "xl6_xrt_string_insert", xl6StdXrtStringInsertParams, 3, "point", NULL},
    {"__xrt_string", "padLeft", "__xl_std_xrt_string_pad_left", "xl6_xrt_string_pad_left", xl6StdXrtStringPadParams, 3, "point", NULL},
    {"__xrt_string", "padRight", "__xl_std_xrt_string_pad_right", "xl6_xrt_string_pad_right", xl6StdXrtStringPadParams, 3, "point", NULL},
    {"__xrt_string", "padCenter", "__xl_std_xrt_string_pad_center", "xl6_xrt_string_pad_center", xl6StdXrtStringPadParams, 3, "point", NULL},
    {"__xrt_string", "reverse", "__xl_std_xrt_string_reverse", "xl6_xrt_string_reverse", xl6StdXrtStringSizeParams, 1, "point", NULL},
    {"__xrt_math", "sign", "__xl_std_xrt_math_sign", "xrtMathSign", xl6StdMathUnaryFloatParams, 1, "c_int", NULL},
    {"__xrt_time", "dateAdd", "__xl_std_xrt_date_add", "xl6_xrt_time_add", xl6StdXrtDateAddParams, 3, "int", NULL},
    {"__xrt_time", "dateDiff", "__xl_std_xrt_date_diff", "xl6_xrt_time_diff", xl6StdXrtDateDiffParams, 3, "int", NULL},
    {"__xrt_time", "strToTime", "__xl_std_xrt_str_to_time", "xl6_xrt_time_parse_any_value", xl6StdXrtStrToTimeParams, 2, "int", NULL},
    {"__xrt_time", "tryStrToTime", "__xl_std_xrt_try_str_to_time", "xl6_xrt_time_parse_any_try", xl6StdXrtTryStrToTimeParams, 3, "c_bool", NULL},
    {"__xrt_time", "timeToStr", "__xl_std_xrt_time_to_str", "xl6_xrt_time_string_owned", xl6StdXrtTimeToStrParams, 2, "point", NULL},
    {"__xrt_time", "timeFormat", "__xl_std_xrt_time_format", "xl6_xrt_time_format_owned", xl6StdXrtTimeFormatParams, 2, "point", NULL},
    {"__xrt_time", "timeParse", "__xl_std_xrt_time_parse", "xl6_xrt_time_parse_value", xl6StdXrtTimeParseParams, 2, "int", NULL},
    {"__xrt_time", "tryTimeParse", "__xl_std_xrt_try_time_parse", "xl6_xrt_time_parse_try", xl6StdXrtTryTimeParseParams, 3, "c_bool", NULL},
    {"__xrt_time", "relativeTime", "__xl_std_xrt_relative_time", "xl6_xrt_time_relative", xl6StdXrtIntBinaryParams, 2, "point", NULL},
    {"__xrt_time", "dateTimeSerial", "__xl_std_xrt_datetime_serial", "xl6_xrt_time_datetime", xl6StdTimeFromDateTimeParams, 6, "int", NULL},
    {"__xrt_time", "now", "__xl_std_xrt_now", "xl6_xrt_time_now_local", NULL, 0, "int", NULL},
    {"__xrt_time", "date", "__xl_std_xrt_date", "xl6_xrt_time_today", NULL, 0, "int", NULL},
    {"__xrt_time", "time", "__xl_std_xrt_time", "xl6_xrt_time_clock", NULL, 0, "int", NULL},
    {"__xrt_time", "timeSerial", "__xl_std_xrt_time_serial", "xl6_xrt_time_clock_value", xl6StdTimeFromTimeParams, 3, "int", NULL},
    {"__xrt_time", "dateSerial", "__xl_std_xrt_date_serial", "xl6_xrt_time_date", xl6StdTimeFromDateParams, 3, "int", NULL},
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
    {"__xrt_time", "isSameDay", "__xl_std_xrt_is_same_day", "xrtTimeSameDay", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "isSameMonth", "__xl_std_xrt_is_same_month", "xrtTimeSameMonth", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "isSameYear", "__xl_std_xrt_is_same_year", "xrtTimeSameYear", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "inRange", "__xl_std_xrt_time_in_range", "xrtTimeIn", xl6StdXrtIntInRangeParams, 3, "c_bool", NULL},
    {"__xrt_time", "timer", "__xl_std_xrt_timer", "xrtTimer", NULL, 0, "c_double", NULL},
    {"__xrt_time", "sleep", "__xl_std_xrt_sleep", "xrtSleep", xl6StdXrtSleepParams, 1, "", NULL},
    {"__xrt_time", "nowUTC", "__xl_std_xrt_now_utc", "xl6_xrt_time_now_utc", NULL, 0, "int", NULL},
    {"__xrt_time", "timezoneOffset", "__xl_std_xrt_timezone_offset", "xl6_xrt_time_timezone_offset", NULL, 0, "c_int", NULL},
    {"__xrt_time", "rangeOverlap", "__xl_std_xrt_time_range_overlap", "xrtTimeOverlap", xl6StdXrtTimeOverlapParams, 4, "c_bool", NULL},
    {"__xrt_time", "approx", "__xl_std_xrt_time_approx", "xl6_xrt_time_approx", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "toUnix", "__xl_std_xrt_time_to_unix", "xrtTimeUnix", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "fromUnix", "__xl_std_xrt_time_from_unix", "xl6_xrt_time_from_unix", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "utcToLocal", "__xl_std_xrt_utc_to_local", "xl6_xrt_time_utc_to_local", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "localToUTC", "__xl_std_xrt_local_to_utc", "xl6_xrt_time_local_to_utc", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfMonth", "__xl_std_xrt_first_day_of_month", "xl6_xrt_time_first_day_of_month", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "lastDayOfMonth", "__xl_std_xrt_last_day_of_month", "xl6_xrt_time_last_day_of_month", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfYear", "__xl_std_xrt_first_day_of_year", "xl6_xrt_time_first_day_of_year", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "lastDayOfYear", "__xl_std_xrt_last_day_of_year", "xl6_xrt_time_last_day_of_year", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfWeek", "__xl_std_xrt_first_day_of_week", "xl6_xrt_time_first_day_of_week", xl6StdXrtWeekBoundaryParams, 2, "int", NULL},
    {"__xrt_time", "lastDayOfWeek", "__xl_std_xrt_last_day_of_week", "xl6_xrt_time_last_day_of_week", xl6StdXrtWeekBoundaryParams, 2, "int", NULL},
    {"__xrt_time", "weekOfYear", "__xl_std_xrt_week_of_year", "xl6_xrt_time_week_of_year", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "weekOfMonth", "__xl_std_xrt_week_of_month", "xl6_xrt_time_week_of_month", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_random", "rand32", "__xl_std_xrt_rand32", "xrtRand32", NULL, 0, "c_uint", NULL},
    {"__xrt_random", "rand64", "__xl_std_xrt_rand64", "xrtRand64", NULL, 0, "c_ulonglong", NULL},
    {"__xrt_random", "range", "__xl_std_xrt_rand_range", "xl6_xrt_random_range", xl6StdXrtRandRangeParams, 2, "c_int", NULL},
    {"__xrt_random", "seedThread", "__xl_std_xrt_rand_seed_thread", "xl6_xrt_random_seed", xl6StdXrtRandSeedParams, 2, "", NULL},
    {"__xrt_random", "string", "__xl_std_xrt_rand_str", "xl6_xrt_random_string", xl6StdXrtRandStrParams, 3, "point", NULL},
    {"__xrt_path", "exists", "__xl_std_xrt_path_exists", "xrtPathExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "isAbs", "__xl_std_xrt_path_is_abs", "xrtPathIsAbs", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "getNameExt", "__xl_std_xrt_path_get_name_ext", "xrtPathName", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "getName", "__xl_std_xrt_path_get_name", "xrtPathStem", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "getExt", "__xl_std_xrt_path_get_ext", "xrtPathExt", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "getDir", "__xl_std_xrt_path_get_dir", "xrtPathParent", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "random", "__xl_std_xrt_path_random", "xl6_xrt_path_random", xl6StdXrtPathRandomParams, 5, "point", NULL},
    {"__xrt_path", "sep", "__xl_std_xrt_path_sep", "xl6_xrt_path_sep_text", NULL, 0, "point", NULL},
    {"__xrt_path", "normalize", "__xl_std_xrt_path_normalize", "xl6_xrt_path_clean", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "abs", "__xl_std_xrt_path_abs", "xrtPathAbs", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "rel", "__xl_std_xrt_path_rel", "xrtPathRel", xl6StdPathRelParams, 2, "point", NULL},
    {"__xrt_path", "cwd", "__xl_std_xrt_path_cwd", "xrtPathCwd", NULL, 0, "point", NULL},
    {"__xrt_path", "setCwd", "__xl_std_xrt_path_set_cwd", "xrtPathSetCwd", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "home", "__xl_std_xrt_path_home", "xrtPathHome", NULL, 0, "point", NULL},
    {"__xrt_path", "temp", "__xl_std_xrt_path_temp", "xrtPathTemp", NULL, 0, "point", NULL},
    {"__xrt_path", "appDir", "__xl_std_xrt_path_app_dir", "xrtPathAppDir", NULL, 0, "point", NULL},
    {"__xrt_path", "withExt", "__xl_std_xrt_path_with_ext", "xrtPathWithExt", xl6StdPathWithExtParams, 2, "point", NULL},
    {"__xrt_path", "withName", "__xl_std_xrt_path_with_name", "xrtPathWithName", xl6StdPathWithNameParams, 2, "point", NULL},
    {"__xl_runtime", "osEnv", "__xl_std_runtime_os_env", "xl6_xrt_os_env", xl6StdOsEnvParams, 2, "point", NULL},
    {"__xl_runtime", "osPlatform", "__xl_std_runtime_os_platform", "xl6_xrt_os_platform", NULL, 0, "point", NULL},
    {"__xl_runtime", "osArgs", "__xl_std_runtime_os_args", "xl6_xrt_os_args", NULL, 0, "var", NULL},
    {"__xrt_file", "exists", "__xl_std_xrt_file_exists", "xrtFileExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "getSize", "__xl_std_xrt_file_get_size", "xl6_xrt_file_get_size_path", xl6StdPathUnaryParams, 1, "c_size", NULL},
    {"__xrt_file", "setSize", "__xl_std_xrt_file_set_size", "xrtFileSetSize", xl6StdXrtFileSetSizeParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAttr", "__xl_std_xrt_file_get_attr", "xl6_xrt_file_get_attributes", xl6StdPathUnaryParams, 1, "c_int", NULL},
    {"__xrt_file", "setAttr", "__xl_std_xrt_file_set_attr", "xl6_xrt_file_set_attributes", xl6StdXrtFileSetAttrParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAccessTime", "__xl_std_xrt_file_get_access_time", "xl6_xrt_file_get_access_time", xl6StdPathUnaryParams, 1, "int", NULL},
    {"__xrt_file", "getChangeTime", "__xl_std_xrt_file_get_change_time", "xl6_xrt_file_get_change_time", xl6StdPathUnaryParams, 1, "int", NULL},
    {"__xrt_file", "writeAll", "__xl_std_xrt_file_write_all", "xl6_xrt_file_write_text_all", xl6StdXrtFileWriteTextParams, 4, "c_int", NULL},
    {"__xrt_file", "append", "__xl_std_xrt_file_append", "xl6_xrt_file_append_text", xl6StdXrtFileWriteTextParams, 4, "c_int", NULL},
    {"__xrt_file", "readAll", "__xl_std_xrt_file_read_all", "xl6_xrt_file_read_text_strict", xl6StdXrtFileReadTextParams, 3, "point", NULL},
    {"__xrt_file", "readTextStrict", "__xl_std_xrt_file_read_text_strict", "xl6_xrt_file_read_text_strict", xl6StdXrtFileReadTextParams, 3, "point", NULL},
    {"__xrt_file", "delete", "__xl_std_xrt_file_delete", "xl6_xrt_file_delete_status", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "copy", "__xl_std_xrt_file_copy", "xrtFileCopy", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "move", "__xl_std_xrt_file_move", "xrtFileMove", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "rename", "__xl_std_xrt_file_rename", "xl6_xrt_file_rename", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "replace", "__xl_std_xrt_file_replace", "xl6_xrt_file_replace", xl6StdFileReplaceParams, 2, "c_bool", NULL},
    {"__xrt_file", "touch", "__xl_std_xrt_file_touch", "xrtFileTouch", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "writeAllAtomic", "__xl_std_xrt_file_write_all_atomic", "xl6_xrt_file_write_text_atomic", xl6StdXrtFileWriteTextParams, 4, "c_bool", NULL},
    {"__xrt_dir", "exists", "__xl_std_xrt_dir_exists", "xrtDirExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "create", "__xl_std_xrt_dir_create", "xrtDirCreate", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "createAll", "__xl_std_xrt_dir_create_all", "xrtDirCreateAll", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "copy", "__xl_std_xrt_dir_copy", "xrtDirCopy", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_dir", "move", "__xl_std_xrt_dir_move", "xrtDirMove", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_dir", "delete", "__xl_std_xrt_dir_delete", "xrtDirRemoveAll", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "isEmpty", "__xl_std_xrt_dir_is_empty", "xl6_xrt_dir_is_empty", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "count", "__xl_std_xrt_dir_count", "xl6_xrt_dir_count", xl6StdXrtDirCountParams, 3, "int", NULL},
    {"__xrt_dir", "size", "__xl_std_xrt_dir_size", "xl6_xrt_dir_size", xl6StdXrtDirSizeParams, 2, "int", NULL},
    {"__xrt_dir", "clean", "__xl_std_xrt_dir_clean", "xrtDirClean", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "ensureEmpty", "__xl_std_xrt_dir_ensure_empty", "xrtDirEnsureEmpty", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_json", "parse", "__xl_std_xrt_json_parse", "xl6_xrt_json_parse", xl6StdXrtJsonParseParams, 1, "var", NULL},
    {"__xrt_json", "valid", "__xl_std_xrt_json_valid", "xl6_xrt_json_valid", xl6StdXrtJsonParseParams, 1, "c_bool", NULL},
    {"__xrt_json", "parseFile", "__xl_std_xrt_json_parse_file", "xl6_xrt_json_parse_file", xl6StdPathUnaryParams, 1, "var", NULL},
    {"__xrt_json", "stringify", "__xl_std_xrt_json_stringify", "xl6_xrt_json_stringify", xl6StdXrtJsonStringifyParams, 2, "point", NULL},
    {"__xrt_json", "writeFile", "__xl_std_xrt_json_write_file", "xl6_xrt_json_write_file", xl6StdXrtJsonWriteParams, 3, "c_bool", NULL},
    {"__xrt_value", "tableRemove", "__xl_std_xrt_table_remove", "xl6_xrt_std_table_remove", xl6StdXrtTableRemoveParams, 3, "c_bool", NULL},
    {"__runtime", "jsonVisit", "__xl_std_runtime_json_visit", "xl6_xrt_json_visit", xl6StdJsonVisitRuntimeParams, 2, "c_int", NULL},
    {"__runtime", "xsonParseOptions", "__xl_std_runtime_xson_parse_options", "xl6_xrt_xson_parse_options", xl6StdXsonTextOptionsRuntimeParams, 4, "var", NULL},
    {"__runtime", "xsonValidOptions", "__xl_std_runtime_xson_valid_options", "xl6_xrt_xson_valid_options", xl6StdXsonTextOptionsRuntimeParams, 4, "bool", NULL},
    {"__runtime", "xsonReadOptions", "__xl_std_runtime_xson_read_options", "xl6_xrt_xson_read_options", xl6StdXsonTextOptionsRuntimeParams, 4, "var", NULL},
    {"__runtime", "xsonStringifyOptions", "__xl_std_runtime_xson_stringify_options", "xl6_xrt_xson_stringify_options", xl6StdXsonValueOptionsRuntimeParams, 4, "point", NULL},
    {"__runtime", "xsonWriteOptions", "__xl_std_runtime_xson_write_options", "xl6_xrt_xson_write_options", xl6StdXsonWriteOptionsRuntimeParams, 5, "bool", NULL},
    {"__runtime", "xsonVisitOptions", "__xl_std_runtime_xson_visit_options", "xl6_xrt_xson_visit_options", xl6StdXsonVisitOptionsRuntimeParams, 5, "c_int", NULL},
    {"__xrt_codec", "hexEncode", "__xl_std_xrt_hex_encode", "xl6_xrt_hex_encode_text", xl6StdXrtCodecTextParams, 2, "point", NULL},
    {"__xrt_codec", "hexDecode", "__xl_std_xrt_hex_decode", "xl6_xrt_hex_decode_text", xl6StdXrtCodecTextParams, 2, "point", NULL},
    {"__xrt_codec", "base64Encode", "__xl_std_xrt_base64_encode", "xl6_xrt_base64_encode_text", xl6StdXrtCodecBase64Params, 3, "point", NULL},
    {"__xrt_codec", "base64Decode", "__xl_std_xrt_base64_decode", "xl6_xrt_base64_decode_text", xl6StdXrtCodecBase64Params, 3, "point", NULL},
    {"__xrt_codec", "hexEncodeBytes", "__xl_std_xrt_hex_encode_bytes", "xl6_xrt_hex_encode_bytes", xl6StdXrtCodecBytesParams, 2, "point", NULL},
    {"__xrt_codec", "base64EncodeBytes", "__xl_std_xrt_base64_encode_bytes", "xl6_xrt_base64_encode_bytes", xl6StdXrtCodecBase64BytesParams, 3, "point", NULL},
    {"__xrt_codec", "bufferFromHex", "__xl_std_xrt_buffer_from_hex", "xl6_xrt_buffer_from_hex", xl6StdXrtCodecDecodeBufferParams, 2, "point", NULL},
    {"__xrt_codec", "bufferFromBase64", "__xl_std_xrt_buffer_from_base64", "xl6_xrt_buffer_from_base64", xl6StdXrtCodecBase64DecodeBufferParams, 3, "point", NULL},
    {"__xrt_codec", "percentEncode", "__xl_std_xrt_percent_encode", "xl6_xrt_percent_encode_text", xl6StdXrtCodecPercentParams, 3, "point", NULL},
    {"__xrt_codec", "percentDecode", "__xl_std_xrt_percent_decode", "xl6_xrt_percent_decode_text", xl6StdXrtCodecPercentParams, 3, "point", NULL},
    {"__xrt_hash", "u32", "__xl_std_xrt_hash32", "xrtHash32", xl6StdXrtHashParams, 2, "c_uint", NULL},
    {"__xrt_hash", "u32WithSeed", "__xl_std_xrt_hash32_seed", "xrtHash32Seed", xl6StdXrtHash32SeedParams, 3, "c_uint", NULL},
    {"__xrt_hash", "u64", "__xl_std_xrt_hash64", "xrtHash64", xl6StdXrtHashParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64WithSeed", "__xl_std_xrt_hash64_seed", "xrtHash64Seed", xl6StdXrtHash64SeedParams, 3, "c_ulonglong", NULL},
    {"__xrt_id", "makeString", "__xl_std_xrt_xid_make_string", "xrtXidMakeString", NULL, 0, "point", NULL},
    {"__xrt_net", "localIP", "__xl_std_xrt_get_local_ip", "xrtNetLocalAddressString", xl6StdXrtNetFamilyParams, 1, "point", NULL},
    {"__xrt_net", "localMAC", "__xl_std_xrt_get_local_mac", "xrtNetLocalHardwareString", NULL, 0, "point", NULL},
    {"__xrt_net", "localName", "__xl_std_xrt_get_local_name", "xrtNetHostNameString", NULL, 0, "point", NULL},
    {"__xrt_tcp", "connect", "__xl_std_xrt_tcp_connect", "xl6_xrt_tcp_connect", xl6StdTcpConnectParams, 3, "point", NULL},
    {"__xrt_tcp", "connectAsync", "__xl_std_xrt_tcp_connect_async", "xl6_xrt_tcp_connect_async", xl6StdTcpConnectParams, 3, "future<TcpStream>", NULL},
    {"__xrt_tcp", "listen", "__xl_std_xrt_tcp_listen", "xl6_xrt_tcp_listen", xl6StdTcpListenParams, 3, "point", NULL},
    {"__xrt_tcp", "accept", "__xl_std_xrt_tcp_accept", "xl6_xrt_tcp_accept", xl6StdTcpAcceptParams, 2, "point", NULL},
    {"__xrt_tcp", "listenerDestroy", "__xl_std_xrt_tcp_listener_destroy", "xl6_xrt_tcp_listener_destroy", xl6StdNetHandleParams, 1, "", NULL},
    {"__xrt_tcp", "listenerPort", "__xl_std_xrt_tcp_listener_port", "xl6_xrt_tcp_listener_port", xl6StdNetHandleParams, 1, "uint16", NULL},
    {"__xrt_udp", "bind", "__xl_std_xrt_udp_bind", "xl6_xrt_udp_bind", xl6StdUdpBindParams, 2, "point", NULL},
    {"__xrt_udp", "sendText", "__xl_std_xrt_udp_send_text", "xl6_xrt_udp_send_text", xl6StdUdpSendParams, 4, "c_bool", NULL},
    {"__xrt_udp", "recv", "__xl_std_xrt_udp_recv", "xl6_xrt_udp_recv", xl6StdUdpRecvParams, 2, "point", NULL},
    {"__xrt_udp", "recvAsync", "__xl_std_xrt_udp_recv_async", "xl6_xrt_udp_recv_async", xl6StdNetHandleParams, 1, "future<UdpPacket>", NULL},

    {"", "print", "__xl_std_print", NULL, NULL, 0, "", xl6StdPrintBody},
    {"", "println", "__xl_std_println", NULL, NULL, 0, "", xl6StdPrintlnBody},
    {"", "write", "__xl_std_write", NULL, NULL, 0, "", xl6StdWriteBody},
    {"", "writeln", "__xl_std_writeln", NULL, NULL, 0, "", xl6StdWritelnBody},

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
    {"math", "abs", "__xl_std_math_abs", "llabs", xl6StdMathUnaryIntParams, 1, "int", NULL},
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
    {"int", "format", "__xl_std_int_format", NULL, NULL, 0, "string", xl6StdIntFormatBody},
    {"int", "format", "__xl_std_int_format", NULL, NULL, 0, "string", xl6StdIntFormatUIntBody},
    {"float", "format", "__xl_std_float_format", NULL, NULL, 0, "string", xl6StdFloatFormatBody},
    {"string", "format", "__xl_std_string_format", NULL, NULL, 0, "string", xl6StdStringFormatBodyInt},
    {"string", "format", "__xl_std_string_format", NULL, NULL, 0, "string", xl6StdStringFormatBodyUInt},
    {"string", "format", "__xl_std_string_format", NULL, NULL, 0, "string", xl6StdStringFormatBodyFloat},
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

    {"os", "env", "__xl_std_os_env", NULL, xl6StdOsEnvParams, 2, "string", xl6StdOsEnvBody},
    {"os", "platform", "__xl_std_os_platform", NULL, NULL, 0, "string", xl6StdOsPlatformBody},
    {"os", "args", "__xl_std_os_args", NULL, NULL, 0, "array", xl6StdOsArgsBody},
    {"os", "cwd", "__xl_std_os_cwd", NULL, NULL, 0, "string", xl6StdOsCwdBody},
    {"os", "setCwd", "__xl_std_os_set_cwd", NULL, xl6StdPathUnaryParams, 1, "bool", xl6StdOsSetCwdBody},
    {"os", "exec", "__xl_std_os_exec", NULL, NULL, 0, "string", xl6StdOsExecBody},
    {"os", "run", "__xl_std_os_run", NULL, NULL, 0, "ProcessResult", xl6StdOsRunBody},
    {"os", "start", "__xl_std_os_start", NULL, xl6StdPathUnaryParams, 1, "Process", xl6StdOsStartBody},
    {"os", "startFile", "__xl_std_os_start_file", NULL, NULL, 0, "Process", xl6StdOsStartFileBody},
    {"os", "chain", "__xl_std_os_chain", NULL, NULL, 0, "int", xl6StdOsChainBody},

    {"file", "exists", "__xl_std_file_exists", NULL, NULL, 0, "bool", xl6StdFileExistsBody},
    {"file", "size", "__xl_std_file_size", NULL, NULL, 0, "int", xl6StdFileSizeBody},
    {"file", "setSize", "__xl_std_file_set_size", NULL, NULL, 0, "bool", xl6StdFileSetSizeBody},
    {"file", "attr", "__xl_std_file_attr", NULL, NULL, 0, "int", xl6StdFileAttrBody},
    {"file", "setAttr", "__xl_std_file_set_attr", NULL, NULL, 0, "bool", xl6StdFileSetAttrBody},
    {"file", "accessTime", "__xl_std_file_access_time", NULL, NULL, 0, "time", xl6StdFileAccessTimeBody},
    {"file", "changeTime", "__xl_std_file_change_time", NULL, NULL, 0, "time", xl6StdFileChangeTimeBody},
    {"file", "readText", "__xl_std_file_read_text", NULL, xl6StdFileReadTextParams, 2, "string", xl6StdFileReadTextBody},
    {"file", "readBytes", "__xl_std_file_read_bytes", NULL, NULL, 0, "Buffer", xl6StdFileReadBytesBody},
    {"file", "writeText", "__xl_std_file_write_text", NULL, NULL, 0, "bool", xl6StdFileWriteTextBody},
    {"file", "appendText", "__xl_std_file_append_text", NULL, NULL, 0, "bool", xl6StdFileAppendTextBody},
    {"file", "writeBytes", "__xl_std_file_write_bytes", NULL, NULL, 0, "bool", xl6StdFileWriteBytesBody},
    {"file", "appendBytes", "__xl_std_file_append_bytes", NULL, NULL, 0, "bool", xl6StdFileAppendBytesBody},
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
    {"file", "readBytesAsync", "__xl_std_file_read_bytes_async", NULL, NULL, 0, "future<Buffer>", xl6StdFileReadBytesAsyncBody},
    {"file", "writeTextAsync", "__xl_std_file_write_text_async", "xl6_xrt_file_write_text_async_utf8", xl6StdFileWriteTextUtf8Params, 2, "future<bool>", NULL},
    {"file", "writeBytesAsync", "__xl_std_file_write_bytes_async", NULL, NULL, 0, "future<bool>", xl6StdFileWriteBytesAsyncBody},
    {"file", "appendTextAsync", "__xl_std_file_append_text_async", "xl6_xrt_file_append_text_async_utf8", xl6StdFileWriteTextUtf8Params, 2, "future<bool>", NULL},
    {"file", "copyAsync", "__xl_std_file_copy_async", "xl6_xrt_file_copy_async_overwrite", xl6StdFileReplaceParams, 2, "future<bool>", NULL},
    {"file", "moveAsync", "__xl_std_file_move_async", "xl6_xrt_file_move_async_overwrite", xl6StdFileReplaceParams, 2, "future<bool>", NULL},
    {"file", "deleteAsync", "__xl_std_file_delete_async", "xl6_xrt_file_delete_async", xl6StdPathUnaryParams, 1, "future<bool>", NULL},
    {"file", "openAsync", "__xl_std_file_open_async", NULL, NULL, 0, "AsyncFile", xl6StdFileOpenAsyncBody},

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
    {"dir", "matchEntries", "__xl_std_dir_match", NULL, NULL, 0, "array", xl6StdDirMatchBody},
    {"dir", "walk", "__xl_std_dir_walk", NULL, NULL, 0, "array", xl6StdDirWalkBody},
    {"dir", "createAsync", "__xl_std_dir_create_async", "xl6_xrt_dir_create_async", xl6StdPathUnaryParams, 1, "future<bool>", NULL},
    {"dir", "createAllAsync", "__xl_std_dir_create_all_async", "xl6_xrt_dir_create_all_async", xl6StdPathUnaryParams, 1, "future<bool>", NULL},
    {"dir", "copyAsync", "__xl_std_dir_copy_async", "xl6_xrt_dir_copy_async", xl6StdFileReplaceParams, 2, "future<bool>", NULL},
    {"dir", "moveAsync", "__xl_std_dir_move_async", "xl6_xrt_dir_move_async", xl6StdFileReplaceParams, 2, "future<bool>", NULL},
    {"dir", "deleteAsync", "__xl_std_dir_delete_async", "xl6_xrt_dir_delete_async", xl6StdPathUnaryParams, 1, "future<bool>", NULL},

    {"json", "parse", "__xl_std_json_parse", NULL, NULL, 0, "var", xl6StdJsonParseBody},
    {"json", "valid", "__xl_std_json_valid", NULL, NULL, 0, "bool", xl6StdJsonValidBody},
    {"json", "read", "__xl_std_json_read", NULL, NULL, 0, "var", xl6StdJsonReadBody},
    {"json", "stringify", "__xl_std_json_stringify", NULL, xl6StdJsonStringifyParams, 2, "string", xl6StdJsonStringifyBody},
    {"json", "write", "__xl_std_json_write", NULL, xl6StdJsonWriteParams, 3, "bool", xl6StdJsonWriteBody},
    {"json", "writer", "__xl_std_json_writer", NULL, NULL, 0, "JsonWriter", xl6StdJsonWriterBody},
    {"json", "visit", "__xl_std_json_visit", NULL, NULL, 0, "int", xl6StdJsonVisitBody},

    {"dict", "remove", "__xl_std_dict_remove", NULL, xl6StdDictRemoveParams, 2, "bool", xl6StdDictRemoveBody},

    {"xson", "parse", "__xl_std_xson_parse", NULL, NULL, 0, "var", xl6StdXsonParseBody},
    {"xson", "parse", "__xl_std_xson_parse", NULL, NULL, 0, "var", xl6StdXsonParseOptionsBody},
    {"xson", "valid", "__xl_std_xson_valid", NULL, NULL, 0, "bool", xl6StdXsonValidBody},
    {"xson", "valid", "__xl_std_xson_valid", NULL, NULL, 0, "bool", xl6StdXsonValidOptionsBody},
    {"xson", "read", "__xl_std_xson_read", NULL, NULL, 0, "var", xl6StdXsonReadBody},
    {"xson", "read", "__xl_std_xson_read", NULL, NULL, 0, "var", xl6StdXsonReadOptionsBody},
    {"xson", "stringify", "__xl_std_xson_stringify", NULL, NULL, 0, "string", xl6StdXsonStringifyBody},
    {"xson", "stringify", "__xl_std_xson_stringify", NULL, NULL, 0, "string", xl6StdXsonStringifyOptionsBody},
    {"xson", "write", "__xl_std_xson_write", NULL, NULL, 0, "bool", xl6StdXsonWriteBody},
    {"xson", "write", "__xl_std_xson_write", NULL, NULL, 0, "bool", xl6StdXsonWriteOptionsBody},
    {"xson", "writer", "__xl_std_xson_writer", NULL, NULL, 0, "XsonWriter", xl6StdXsonWriterBody},
    {"xson", "writer", "__xl_std_xson_writer", NULL, NULL, 0, "XsonWriter", xl6StdXsonWriterOptionsBody},
    {"xson", "visit", "__xl_std_xson_visit", NULL, NULL, 0, "int", xl6StdXsonVisitBody},
    {"xson", "visit", "__xl_std_xson_visit", NULL, NULL, 0, "int", xl6StdXsonVisitOptionsBody},
    {"xson", "options", "__xl_std_xson_options", NULL, NULL, 0, "XsonOptions", xl6StdXsonOptionsBody},

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

    {"encoding", "charSize", "__xl_std_encoding_char_size", NULL, NULL, 0, "int", xl6StdEncodingCharSizeBody},
    {"encoding", "isUtf8", "__xl_std_encoding_is_utf8", NULL, NULL, 0, "bool", xl6StdEncodingIsUtf8Body},
    {"encoding", "isUtf8Bytes", "__xl_std_encoding_is_utf8_bytes", NULL, NULL, 0, "bool", xl6StdEncodingIsUtf8BytesBody},
    {"encoding", "detect", "__xl_std_encoding_detect", NULL, NULL, 0, "int", xl6StdEncodingDetectBody},
    {"encoding", "detectBytes", "__xl_std_encoding_detect_bytes", NULL, NULL, 0, "int", xl6StdEncodingDetectBytesBody},
    {"encoding", "toBytes", "__xl_std_encoding_to_bytes", NULL, NULL, 0, "Buffer", xl6StdEncodingToBytesBody},
    {"encoding", "fromBytes", "__xl_std_encoding_from_bytes", NULL, NULL, 0, "string", xl6StdEncodingFromBytesBody},
    {"encoding", "convertBytes", "__xl_std_encoding_convert_bytes", NULL, NULL, 0, "Buffer", xl6StdEncodingConvertBytesBody},
    {"encoding", "name", "__xl_std_encoding_name", NULL, NULL, 0, "string", xl6StdEncodingNameBody},
    {"encoding", "encodeUtf8", "__xl_std_encoding_encode_utf8", NULL, NULL, 0, "Buffer", xl6StdEncodingEncodeUtf8Body},
    {"encoding", "encodeUtf16LE", "__xl_std_encoding_encode_utf16_le", NULL, NULL, 0, "Buffer", xl6StdEncodingEncodeUtf16LeBody},
    {"encoding", "encodeUtf16BE", "__xl_std_encoding_encode_utf16_be", NULL, NULL, 0, "Buffer", xl6StdEncodingEncodeUtf16BeBody},
    {"encoding", "encodeUtf32LE", "__xl_std_encoding_encode_utf32_le", NULL, NULL, 0, "Buffer", xl6StdEncodingEncodeUtf32LeBody},
    {"encoding", "encodeUtf32BE", "__xl_std_encoding_encode_utf32_be", NULL, NULL, 0, "Buffer", xl6StdEncodingEncodeUtf32BeBody},
    {"encoding", "decodeUtf8", "__xl_std_encoding_decode_utf8", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf8Body},
    {"encoding", "decodeUtf16LE", "__xl_std_encoding_decode_utf16_le", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf16LeBody},
    {"encoding", "decodeUtf16BE", "__xl_std_encoding_decode_utf16_be", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf16BeBody},
    {"encoding", "decodeUtf32LE", "__xl_std_encoding_decode_utf32_le", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf32LeBody},
    {"encoding", "decodeUtf32BE", "__xl_std_encoding_decode_utf32_be", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf32BeBody},
    {"encoding", "swapUtf16Endian", "__xl_std_encoding_swap_utf16_endian", NULL, NULL, 0, "Buffer", xl6StdEncodingSwapUtf16EndianBody},
    {"encoding", "swapUtf32Endian", "__xl_std_encoding_swap_utf32_endian", NULL, NULL, 0, "Buffer", xl6StdEncodingSwapUtf32EndianBody},

    {"hash", "u32", "__xl_std_hash_u32", NULL, NULL, 0, "uint32", xl6StdHashU32Body},
    {"hash", "u32WithSeed", "__xl_std_hash_u32_with_seed", NULL, NULL, 0, "uint32", xl6StdHashU32WithSeedBody},
    {"hash", "u64", "__xl_std_hash_u64", NULL, NULL, 0, "uint64", xl6StdHashU64Body},
    {"hash", "u64WithSeed", "__xl_std_hash_u64_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64WithSeedBody},
    {"crypto", "sha1", "__xl_std_crypto_sha1", NULL, NULL, 0, "string", xl6StdCryptoSha1Body},
    {"crypto", "sha256", "__xl_std_crypto_sha256", NULL, NULL, 0, "string", xl6StdCryptoSha256Body},
    {"crypto", "sha384", "__xl_std_crypto_sha384", NULL, NULL, 0, "string", xl6StdCryptoSha384Body},
    {"crypto", "sha512", "__xl_std_crypto_sha512", NULL, NULL, 0, "string", xl6StdCryptoSha512Body},
    {"crypto", "hmacSha256", "__xl_std_crypto_hmac_sha256", NULL, NULL, 0, "string", xl6StdCryptoHmacSha256Body},
    {"crypto", "hmacSha384", "__xl_std_crypto_hmac_sha384", NULL, NULL, 0, "string", xl6StdCryptoHmacSha384Body},
    {"crypto", "hmacSha512", "__xl_std_crypto_hmac_sha512", NULL, NULL, 0, "string", xl6StdCryptoHmacSha512Body},
    {"crypto", "sha1Stream", "__xl_std_crypto_sha1_stream", NULL, NULL, 0, "Sha1", xl6StdCryptoSha1StreamBody},
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
    {"crypto", "sha1Bytes", "__xl_std_crypto_sha1_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoSha1BytesBody},
    {"crypto", "sha256Bytes", "__xl_std_crypto_sha256_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoSha256BytesBody},
    {"crypto", "sha384Bytes", "__xl_std_crypto_sha384_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoSha384BytesBody},
    {"crypto", "sha512Bytes", "__xl_std_crypto_sha512_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoSha512BytesBody},
    {"crypto", "hmacSha256Bytes", "__xl_std_crypto_hmac_sha256_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoHmacSha256BytesBody},
    {"crypto", "hmacSha384Bytes", "__xl_std_crypto_hmac_sha384_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoHmacSha384BytesBody},
    {"crypto", "hmacSha512Bytes", "__xl_std_crypto_hmac_sha512_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoHmacSha512BytesBody},
    {"crypto", "chacha20Bytes", "__xl_std_crypto_chacha20_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoChaCha20BytesBody},
    {"crypto", "chacha20Poly1305EncryptBytes", "__xl_std_crypto_chacha20_poly1305_encrypt_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoChaCha20Poly1305EncryptBytesBody},
    {"crypto", "chacha20Poly1305DecryptBytes", "__xl_std_crypto_chacha20_poly1305_decrypt_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoChaCha20Poly1305DecryptBytesBody},
    {"crypto", "aes128GcmEncryptBytes", "__xl_std_crypto_aes128_gcm_encrypt_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoAes128GcmEncryptBytesBody},
    {"crypto", "aes128GcmDecryptBytes", "__xl_std_crypto_aes128_gcm_decrypt_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoAes128GcmDecryptBytesBody},
    {"crypto", "aes256GcmEncryptBytes", "__xl_std_crypto_aes256_gcm_encrypt_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoAes256GcmEncryptBytesBody},
    {"crypto", "aes256GcmDecryptBytes", "__xl_std_crypto_aes256_gcm_decrypt_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoAes256GcmDecryptBytesBody},
    {"crypto", "hkdfSha256Bytes", "__xl_std_crypto_hkdf_sha256_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoHkdfSha256BytesBody},
    {"crypto", "hkdfSha384Bytes", "__xl_std_crypto_hkdf_sha384_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoHkdfSha384BytesBody},
    {"crypto", "randomBytes", "__xl_std_crypto_random_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoRandomBytesBody},
    {"crypto", "ed25519Keypair", "__xl_std_crypto_ed25519_keypair", NULL, NULL, 0, "var", xl6StdCryptoEd25519KeypairBody},
    {"crypto", "ed25519PublicKey", "__xl_std_crypto_ed25519_public_key", NULL, NULL, 0, "string", xl6StdCryptoEd25519PublicKeyBody},
    {"crypto", "ed25519Sign", "__xl_std_crypto_ed25519_sign", NULL, NULL, 0, "string", xl6StdCryptoEd25519SignBody},
    {"crypto", "ed25519Verify", "__xl_std_crypto_ed25519_verify", NULL, NULL, 0, "bool", xl6StdCryptoEd25519VerifyBody},
    {"crypto", "x25519Keypair", "__xl_std_crypto_x25519_keypair", NULL, NULL, 0, "var", xl6StdCryptoX25519KeypairBody},
    {"crypto", "x25519SharedSecret", "__xl_std_crypto_x25519_shared_secret", NULL, NULL, 0, "string", xl6StdCryptoX25519SharedSecretBody},
    {"crypto", "x448Keypair", "__xl_std_crypto_x448_keypair", NULL, NULL, 0, "var", xl6StdCryptoX448KeypairBody},
    {"crypto", "x448SharedSecret", "__xl_std_crypto_x448_shared_secret", NULL, NULL, 0, "string", xl6StdCryptoX448SharedSecretBody},
    {"crypto", "p256Keypair", "__xl_std_crypto_p256_keypair", NULL, NULL, 0, "var", xl6StdCryptoP256KeypairBody},
    {"crypto", "p256SharedSecret", "__xl_std_crypto_p256_shared_secret", NULL, NULL, 0, "string", xl6StdCryptoP256SharedSecretBody},
    {"crypto", "p384Keypair", "__xl_std_crypto_p384_keypair", NULL, NULL, 0, "var", xl6StdCryptoP384KeypairBody},
    {"crypto", "p384SharedSecret", "__xl_std_crypto_p384_shared_secret", NULL, NULL, 0, "string", xl6StdCryptoP384SharedSecretBody},
    {"crypto", "ecdsaVerify", "__xl_std_crypto_ecdsa_verify", NULL, NULL, 0, "bool", xl6StdCryptoEcdsaVerifyBody},
    {"crypto", "rsaPssVerify", "__xl_std_crypto_rsa_pss_verify", NULL, NULL, 0, "bool", xl6StdCryptoRsaPssVerifyBody},
    {"crypto", "rsaPkcs1Verify", "__xl_std_crypto_rsa_pkcs1_verify", NULL, NULL, 0, "bool", xl6StdCryptoRsaPkcs1VerifyBody},

    {"id", "xid", "__xl_std_id_xid", NULL, NULL, 0, "string", xl6StdIdXidBody},
    {"id", "make", "__xl_std_id_make", NULL, NULL, 0, "Xid", xl6StdIdMakeBody},
    {"id", "decode", "__xl_std_id_decode", NULL, NULL, 0, "Xid", xl6StdIdDecodeBody},
    {"id", "encode", "__xl_std_id_encode", NULL, NULL, 0, "string", xl6StdIdEncodeBody},
    {"id", "compare", "__xl_std_id_compare", NULL, NULL, 0, "bool", xl6StdIdCompareBody},

    {"net", "localIP", "__xl_std_net_local_ip", NULL, NULL, 0, "string", xl6StdNetLocalIPBody},
    {"net", "localMAC", "__xl_std_net_local_mac", NULL, NULL, 0, "string", xl6StdNetLocalMACBody},
    {"net", "localName", "__xl_std_net_local_name", NULL, NULL, 0, "string", xl6StdNetLocalNameBody},
    {"net", "interfaces", "__xl_std_net_interfaces", NULL, NULL, 0, "NetInterfaceList", xl6StdNetInterfacesBody},
    {"net", "interfaceIndex", "__xl_std_net_interface_index", NULL, NULL, 0, "int", xl6StdNetInterfaceIndexBody},
    {"net", "interfaceName", "__xl_std_net_interface_name", NULL, NULL, 0, "string", xl6StdNetInterfaceNameBody},
    {"net", "resolve", "__xl_std_net_resolve", NULL, NULL, 0, "NetAddress", xl6StdNetResolveBody},
    {"net", "resolveAll", "__xl_std_net_resolve_all", NULL, NULL, 0, "NetAddressList", xl6StdNetResolveAllBody},
    {"net", "resolveAsync", "__xl_std_net_resolve_async", NULL, NULL, 0, "future<NetAddressList>", xl6StdNetResolveAsyncBody},
    {"net", "stats", "__xl_std_net_stats", NULL, NULL, 0, "NetEngineStats", xl6StdNetStatsBody},
    {"net", "resolverStats", "__xl_std_net_resolver_stats", NULL, NULL, 0, "NetResolverStats", xl6StdNetResolverStatsBody},
    {"net", "proxy", "__xl_std_net_proxy", NULL, NULL, 0, "NetProxy", xl6StdNetProxyBody},
    {"net", "engine", "__xl_std_net_engine", NULL, NULL, 0, "NetEngine", xl6StdNetEngineBody},

    {"tcp", "connect", "__xl_std_tcp_connect", NULL, NULL, 0, "TcpStream", xl6StdTcpConnectBody},
    {"tcp", "connectAsync", "__xl_std_tcp_connect_async", NULL, NULL, 0, "future<TcpStream>", xl6StdTcpConnectAsyncBody},
    {"tcp", "listen", "__xl_std_tcp_listen", NULL, NULL, 0, "TcpServer", xl6StdTcpListenBody},
    {"tcp", "connectConfig", "__xl_std_tcp_connect_config", NULL, NULL, 0, "TcpConnectConfig", xl6StdTcpConnectConfigBody},
    {"tcp", "listenConfig", "__xl_std_tcp_listen_config", NULL, NULL, 0, "TcpListenConfig", xl6StdTcpListenConfigBody},

    {"udp", "bind", "__xl_std_udp_bind", NULL, NULL, 0, "UdpSocket", xl6StdUdpBindBody},
    {"udp", "config", "__xl_std_udp_config", NULL, NULL, 0, "UdpConfig", xl6StdUdpConfigBody},

    {"tls", "connect", "__xl_std_tls_connect", NULL, NULL, 0, "TlsStream", xl6StdTlsConnectBody},
    {"tls", "connectAsync", "__xl_std_tls_connect_async", NULL, NULL, 0, "future<TlsStream>", xl6StdTlsConnectAsyncBody},
    {"tls", "listen", "__xl_std_tls_listen", NULL, NULL, 0, "TlsServer", xl6StdTlsListenBody},

    {"process", "exec", "__xl_std_process_exec", NULL, NULL, 0, "string", xl6StdProcessExecBody},
    {"process", "success", "__xl_std_process_success", NULL, NULL, 0, "bool", xl6StdProcessSuccessBody},
    {"process", "exitCode", "__xl_std_process_exit_code", NULL, NULL, 0, "int", xl6StdProcessExitCodeBody},
    {"process", "run", "__xl_std_process_run", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunBody},
    {"process", "run", "__xl_std_process_run", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunCommandBody},
    {"process", "runFile", "__xl_std_process_run_file", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunFileBody},
    {"process", "cmd", "__xl_std_process_cmd", NULL, NULL, 0, "ProcessCommand", xl6StdProcessCmdBody},
    {"process", "shell", "__xl_std_process_shell", NULL, NULL, 0, "ProcessCommand", xl6StdProcessShellBody},
    {"process", "pipeline", "__xl_std_process_pipeline", NULL, NULL, 0, "PipelineResult", xl6StdProcessPipelineBody},
    {"process", "start", "__xl_std_process_start", NULL, NULL, 0, "Process", xl6StdProcessStartBody},
    {"process", "start", "__xl_std_process_start", NULL, NULL, 0, "Process", xl6StdProcessStartCommandBody},
    {"process", "startFile", "__xl_std_process_start_file", NULL, NULL, 0, "Process", xl6StdProcessStartFileBody},
    {"process", "terminalSupported", "__xl_std_process_terminal_supported", NULL, NULL, 0, "bool", xl6StdProcessTerminalSupportedBody},

    {"task", "group", "__xl_std_task_group", NULL, NULL, 0, "TaskGroup", xl6StdTaskGroupBody},

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

    {"signal", "on", "__xl_std_signal_on", NULL, xl6StdSignalOn0Params, 2, "SignalHandle", xl6StdSignalOn0Body},
    {"signal", "on", "__xl_std_signal_on", NULL, xl6StdSignalOn1Params, 2, "SignalHandle", xl6StdSignalOn1Body},
    {"signal", "once", "__xl_std_signal_once", NULL, xl6StdSignalOn0Params, 2, "SignalHandle", xl6StdSignalOnce0Body},
    {"signal", "once", "__xl_std_signal_once", NULL, xl6StdSignalOn1Params, 2, "SignalHandle", xl6StdSignalOnce1Body},
    {"signal", "supported", "__xl_std_signal_supported", NULL, xl6StdRuntimeSignalCodeParams, 1, "bool", xl6StdSignalSupportedBody},
    {"signal", "name", "__xl_std_signal_name", NULL, xl6StdRuntimeSignalCodeParams, 1, "string", xl6StdSignalNameBody},
    {"signal", "restore", "__xl_std_signal_restore", NULL, xl6StdRuntimeSignalCodeParams, 1, "bool", xl6StdSignalRestoreBody},
    {"signal", "reset", "__xl_std_signal_reset", NULL, NULL, 0, "", xl6StdSignalResetBody},
    {"signal", "raise", "__xl_std_signal_raise", NULL, xl6StdRuntimeSignalCodeParams, 1, "bool", xl6StdSignalRaiseBody},
    {"signal", "count", "__xl_std_signal_count", NULL, xl6StdRuntimeSignalCodeParams, 1, "int", xl6StdSignalCountBody},
    {"signal", "received", "__xl_std_signal_received", NULL, xl6StdRuntimeSignalCodeParams, 1, "bool", xl6StdSignalReceivedBody},
    {"signal", "clear", "__xl_std_signal_clear", NULL, xl6StdRuntimeSignalCodeParams, 1, "", xl6StdSignalClearBody},
    {"signal", "sendTo", "__xl_std_signal_send_to", NULL, xl6StdSignalSendToParams, 2, "SignalHandle", xl6StdSignalSendToBody},
    {"signal", "cancelOn", "__xl_std_signal_cancel_on", NULL, xl6StdSignalCancelOnParams, 2, "SignalHandle", xl6StdSignalCancelOnBody},

    {"template", "compile", "__xl_std_template_compile", NULL, xl6StdTemplateCompileParams, 3, "Template", xl6StdTemplateCompileBody},
    {"template", "compileFile", "__xl_std_template_compile_file", NULL, xl6StdTemplateCompileFileParams, 3, "Template", xl6StdTemplateCompileFileBody},
    {"template", "engine", "__xl_std_template_engine", NULL, NULL, 0, "TemplateEngine", xl6StdTemplateEngineBody},
    {"template", "render", "__xl_std_template_render", NULL, xl6StdTemplateRenderParams, 4, "string", xl6StdTemplateRenderBody},
    {"template", "renderFile", "__xl_std_template_render_file", NULL, xl6StdTemplateRenderFileParams, 4, "string", xl6StdTemplateRenderFileBody}
};


#define XL6_STDLIB_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef char XL6_StdlibConstCountCheck[
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibConsts) == XL6_STDLIB_CONST_COUNT ? 1 : -1];
typedef char XL6_StdlibFuncCountCheck[
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibFuncs) == XL6_STDLIB_FUNC_COUNT ? 1 : -1];

const size_t xl6StdlibConstCount = XL6_STDLIB_CONST_COUNT;
const size_t xl6StdlibFuncCount = XL6_STDLIB_FUNC_COUNT;
