#include "src/stdlib/xl6_stdlib_data.h"
#include "src/stdlib/xl6_stdlib_atomic.inc"
#include "src/stdlib/xl6_stdlib_concurrent.inc"
#include "src/stdlib/xl6_stdlib_executor.inc"
#include "src/stdlib/xl6_stdlib_task_pool.inc"
#include "src/stdlib/xl6_stdlib_asn1.inc"
#include "src/stdlib/xl6_stdlib_io.inc"
#include "src/stdlib/xl6_stdlib_text.inc"
#include "src/stdlib/xl6_stdlib_unicode.inc"
#include "src/stdlib/xl6_stdlib_encoding.inc"
#include "src/stdlib/xl6_stdlib_regex.inc"
#include "src/stdlib/xl6_stdlib_pattern.inc"
#include "src/stdlib/xl6_stdlib_time.inc"
#include "src/stdlib/xl6_stdlib_console.inc"
#include "src/stdlib/xl6_stdlib_vfs.inc"
#include "src/stdlib/xl6_stdlib_fs.inc"
#include "src/stdlib/xl6_stdlib_fs_walk.inc"
#include "src/stdlib/xl6_stdlib_fs_async.inc"
#include "src/stdlib/xl6_stdlib_fixed_stack.inc"
#include "src/stdlib/xl6_stdlib_compress.inc"
#include "src/stdlib/xl6_stdlib_net_backends.inc"

const char* xl6StdlibFloatClassSource =
    "enum FloatClass as int32\n"
    "    nan = 0\n"
    "    infinite = 1\n"
    "    normal = 2\n"
    "    subnormal = 3\n"
    "    zero = 4\n"
    "end\n";

const char* xl6StdlibNumberParseFlagsSource =
    "enum NumberParseFlags as int\n"
    "    none = 0\n    allowWhitespace = 1\n    allowPrefix = 2\n"
    "    allowSeparator = 4\n    allowSpecial = 8\nend\n";
const char* xl6StdlibIntegerTextFlagsSource =
    "enum IntegerTextFlags as int\n"
    "    none = 0\n    upper = 1\n    prefix = 2\n    plus = 4\nend\n";
const char* xl6StdlibFloatTextFlagsSource =
    "enum FloatTextFlags as int\n    none = 0\n    compact = 1\nend\n";

const char* xl6StdlibCoreClassSource =
    "class DynamicFields\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_dynamic_fields_create\" create()\n"
    "    extern \"xl6_xrt_dynamic_fields_wrap\" create(point handle)\n"
    "    extern \"xl6_xrt_dynamic_fields_from_value\" create(var value)\n"
    "    extern \"xl6_xrt_dynamic_fields_clone\" create(DynamicFields source)\n"
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
    "    extern \"xl6_xrt_dynamic_fields_merge\" func merge(DynamicFields source, bool replace = true) as bool\n"
    "\n"
    "    get length() as int\n"
    "        return self.countNative()\n"
    "    end\n"
    "\n"
    "    get capacity() as int\n"
    "        return self.capacityNative()\n"
    "    end\n"
    "\n"
    "    func clone() as DynamicFields\n"
    "        return DynamicFields(self)\n"
    "    end\n"
    "\n"
    "    func __clone__() as DynamicFields\n"
    "        return DynamicFields(self)\n"
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
    "    extern \"xl6_xrt_rng_create_default\" create(uint64 seed)\n"
    "    extern \"xl6_xrt_rng_destroy\" destroy()\n"
    "    extern \"xl6_xrt_rng_seed\" func seedNative(uint64 seed, uint64 seq)\n"
    "    extern \"xl6_xrt_rng_u32\" func u32Native() as c_uint\n"
    "    extern \"xl6_xrt_rng_u64\" func u64() as uint64\n"
    "    extern \"xl6_xrt_rng_below\" func below(uint64 bound) as uint64\n"
    "    extern \"xl6_xrt_rng_range\" func range(int min, int max) as int\n"
    "    extern \"xl6_xrt_rng_range_closed\" func rangeClosed(int min, int max) as int\n"
    "    extern \"xl6_xrt_rng_real\" func real() as float\n"
    "    extern \"xl6_xrt_rng_bytes\" func bytes(int count) as bytes\n"
    "    extern \"xl6_xrt_rng_string\" func stringNative(string alphabet, c_size actualSize, int alphabetSize, int count) as point\n"
    "    extern \"xl6_xrt_rng_shuffle_array\" func shuffleNative(array values) as bool\n"
    "\n"
    "    func seed(uint64 seed, uint64 seq = 1)\n"
    "        self.seedNative(seed, seq)\n"
    "    end\n"
    "\n"
    "    func u32() as uint32\n"
    "        return uint32(self.u32Native())\n"
    "    end\n"
    "\n"
    "    func string(int length = 16, string alphabet = \"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_\", int alphabetSize = -1) as string\n"
    "        if length < 0 or alphabetSize < -1 then throw RangeError \"random string length is invalid\" end\n"
    "        return __xl_std_runtime_take_cstr(self.stringNative(alphabet, c_size(__xl_std_xrt_string_byte_len(alphabet)), alphabetSize, length))\n"
    "    end\n"
    "\n"
    "    func shuffle(array values)\n"
    "        if not self.shuffleNative(values) then throw RuntimeError \"random array shuffle failed\" end\n"
    "    end\n"
    "\n";

const char* xl6StdlibRandomBufferMethodsSource =
    "    extern \"xl6_xrt_rng_fill_buffer\" func fillBytesNative(point target) as bool\n"
    "    extern \"xl6_xrt_rng_shuffle_buffer\" func shuffleBufferNative(point target) as bool\n"
    "\n"
    "    func fillBytes(Buffer target)\n"
    "        if not self.fillBytesNative(target.nativeHandle()) then throw RuntimeError \"random buffer fill failed\" end\n"
    "    end\n"
    "\n"
    "    func shuffleBuffer(Buffer target)\n"
    "        if not self.shuffleBufferNative(target.nativeHandle()) then throw RuntimeError \"random buffer shuffle failed\" end\n"
    "    end\n";

const char* xl6StdlibRandomClassEndSource = "end\n";

const char* xl6StdlibBufferClassSource =
    "enum HexFlags as int\n"
    "    None = 0\n"
    "    Upper = 1\n"
    "    IgnoreSpace = 2\n"
    "end\n"
    "enum Base64Flags as int\n"
    "    None = 0\n"
    "    Url = 1\n"
    "    NoPadding = 2\n"
    "    IgnoreSpace = 4\n"
    "    OptionalPadding = 8\n"
    "end\n"
    "class BufferIter\n"
    "    private native point handle\n"
    "    extern \"xl6BufferIteratorCreateV1\" create(Buffer source, int size = -1)\n"
    "    extern \"xl6BufferIteratorDestroyV1\" destroy()\n"
    "    private extern \"xl6BufferIteratorHasNextV1\" func hasNextNative() as bool\n"
    "    private extern \"xl6BufferIteratorNextV1\" func nextNative() as uint8\n"
    "    extern \"xl6BufferIteratorCloseV1\" func close()\n"
    "    extern \"xl6BufferIteratorClosedV1\" get closed() as bool\n"
    "\n"
    "    func __next__() as (bool, uint8)\n"
    "        if not self.hasNextNative() then\n"
    "            return false, uint8(0)\n"
    "        end\n"
    "        return true, self.nextNative()\n"
    "    end\n"
    "end\n"
    "\n"
    "class Buffer\n"
    "    native point handle\n"
    "    extern \"xl6BufferCreateV1\" create()\n"
    "    extern \"xl6BufferCreateReserveV1\" create(int capacity)\n"
    "    extern \"xl6BufferAdoptV1\" create(point handle)\n"
    "    extern \"xl6BufferFromTextV1\" create(string text, int size = -1)\n"
    "    extern \"xl6BufferFromBytesV1\" create(bytes data, int size = -1)\n"
    "    extern \"xl6BufferCopyV1\" create(Buffer source, int size = -1)\n"
    "    extern \"xl6BufferDestroyV1\" destroy()\n"
    "    static extern \"xl6BufferFromHexV1\" func fromHex(string text, int flags = 0, int size = -1) as Buffer\n"
    "    extern \"xl6BufferHexV1\" func hex(int flags = 0, int size = -1) as string\n"
    "    static extern \"xl6BufferFromBase64V1\" func fromBase64(string text, int flags = 0, string alphabet = \"\", int size = -1, int alphabetSize = -1) as Buffer\n"
    "    extern \"xl6BufferBase64V1\" func base64(int flags = 0, string alphabet = \"\", int size = -1, int alphabetSize = -1) as string\n"
    "    extern \"xl6BufferLengthV1\" func len() as int\n"
    "    extern \"xl6BufferCapacityV1\" func capacity() as int\n"
    "    extern \"xl6BufferReserveV1\" func reserve(int size) as bool\n"
    "    extern \"xl6BufferClearV1\" func clear() as bool\n"
    "    extern \"xl6BufferResizeV1\" func resize(int size, int fill = 0) as bool\n"
    "    extern \"xl6BufferTrimV1\" func trim() as bool\n"
    "    extern \"xl6BufferAssignTextV1\" func assign(string text, int size = -1) as bool\n"
    "    extern \"xl6BufferAssignBytesV1\" func assign(bytes data, int size = -1) as bool\n"
    "    extern \"xl6BufferAssignBufferV1\" func assign(Buffer source, int size = -1) as bool\n"
    "    extern \"xl6BufferAppendTextV1\" func append(string text, int size = -1) as bool\n"
    "    extern \"xl6BufferAppendBytesV1\" func append(bytes data, int size = -1) as bool\n"
    "    extern \"xl6BufferAppendBufferV1\" func append(Buffer source, int size = -1) as bool\n"
    "    extern \"xl6BufferInsertTextV1\" func insert(int index, string text, int size = -1) as bool\n"
    "    extern \"xl6BufferInsertBytesV1\" func insert(int index, bytes data, int size = -1) as bool\n"
    "    extern \"xl6BufferInsertBufferV1\" func insert(int index, Buffer source, int size = -1) as bool\n"
    "    extern \"xl6BufferRemoveV1\" func remove(int index, int count = -1) as bool\n"
    "    extern \"xl6BufferBytesV1\" func toBytes(int size = -1) as bytes\n"
    "    extern \"xl6BufferTextV1\" func text(int size = -1) as string\n"
    "    extern \"xl6NumberBufferIntFormatV1\" func appendFormat(int value, string format = \"\", int size = -1) as bool\n"
    "    extern \"xl6NumberBufferUIntFormatV1\" func appendFormat(uint64 value, string format = \"\", int size = -1) as bool\n"
    "    extern \"xl6NumberBufferFloatFormatV1\" func appendFormat(float value, string format = \"\", int size = -1) as bool\n"
    "    extern \"xl6NumberBufferIntStringV1\" func appendNumber(int value, int base = 10, int flags = 0) as bool\n"
    "    extern \"xl6NumberBufferUIntStringV1\" func appendNumber(uint64 value, int base = 10, int flags = 0) as bool\n"
    "    extern \"xl6NumberBufferFloatStringV1\" func appendNumber(float value, int flags = 0) as bool\n"
    "    extern \"xl6BufferAppendRawV1\" func appendRaw(point data, int size) as bool\n"
    "    extern \"xl6BufferInsertRawV1\" func insertBytes(int index, point data, int size) as bool\n"
    "    extern \"xl6BufferAppendByteV1\" func appendByte(int value) as bool\n"
    "    extern \"xl6BufferByteAtV1\" func byteAt(int index) as uint8\n"
    "    extern \"xl6BufferSetByteV1\" func setByte(int index, int value) as bool\n"
    "    extern \"xl6BufferCopyFromV1\" func copyFrom(int targetOffset, Buffer source, int sourceOffset = 0, int count = -1) as bool\n"
    "    extern \"xl6BufferCopyWithinV1\" func copyWithin(int targetOffset, int sourceOffset, int count) as bool\n"
    "    extern \"xl6BufferFillV1\" func fill(int value, int offset = 0, int count = -1) as bool\n"
    "    extern \"xl6BufferCompareV1\" func compare(Buffer other, int size = -1, int otherSize = -1) as int\n"
    "    extern \"xl6BufferIndexOfV1\" func indexOf(Buffer needle, int start = 0, int size = -1, int needleSize = -1) as int\n"
    "    extern \"xl6BufferDataV1\" func data() as point\n"
    "    extern \"xl6BufferHandleV1\" func nativeHandle() as point\n"
    "\n"
    "    func __len__() as int\n"
    "        return self.len()\n"
    "    end\n"
    "\n"
    "    func __item__(int index) as uint8\n"
    "        return self.byteAt(index)\n"
    "    end\n"
    "\n"
    "    func __setItem__(int index, int value)\n"
    "        self.setByte(index, value)\n"
    "    end\n"
    "\n"
    "    func __iter__() as BufferIter\n"
    "        return BufferIter(self)\n"
    "    end\n"
    "\n"
    "    func __eq__(Buffer other) as bool\n"
    "        return self.compare(other) == 0\n"
    "    end\n"
    "\n"
    "    func __ne__(Buffer other) as bool\n"
    "        return not self.__eq__(other)\n"
    "    end\n"
    "end\n";

const char* xl6StdlibXidClassSource =
    "class Xid\n"
    "    create()\n"
    "        throw ValueError \"use id.make, id.parse or id.fromBytes\"\n"
    "    end\n"
    "\n"
    "    func toBytes() as bytes\n"
    "        return __xl_std_xrt_xid_to_bytes(self)\n"
    "    end\n"
    "\n"
    "    func timestampMicros() as int\n"
    "        return __xl_std_xrt_xid_timestamp(self)\n"
    "    end\n"
    "\n"
    "    func compare(Xid other) as int\n"
    "        return __xl_std_xrt_xid_compare(self, other)\n"
    "    end\n"
    "\n"
    "    func isZero() as bool\n"
    "        return __xl_std_xrt_xid_is_zero(self)\n"
    "    end\n"
    "\n"
    "    func __eq__(Xid other) as bool\n"
    "        return __xl_std_xrt_xid_equal(self, other)\n"
    "    end\n"
    "\n"
    "    func __toString__() as string\n"
    "        return __xl_std_xrt_xid_to_string(self)\n"
    "    end\n"
    "end\n";

#include "src/stdlib/xl6_stdlib_log.inc"

#include "src/stdlib/xl6_stdlib_signal.inc"

#include "src/stdlib/xl6_stdlib_template.inc"

#include "src/stdlib/xl6_stdlib_task_group.inc"

#include "xl6_stdlib_crypto.inc"

#include "xl6_stdlib_net_address.inc"
#include "xl6_stdlib_net_resolver.inc"
#include "xl6_stdlib_net_buffer.inc"

const char* xl6StdlibSipHasher64ClassSource =
    "class SipHasher64\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_siphasher64_create\" create(bytes key)\n"
    "    extern \"xl6_xrt_siphasher64_adopt\" create(point handle)\n"
    "    extern \"xl6_xrt_siphasher64_destroy\" destroy()\n"
    "    extern \"xl6_xrt_siphasher64_update_string\" func updateStringNative(string data, c_size actual, int size) as bool\n"
    "    extern \"xl6_xrt_siphasher64_update_bytes\" func updateBytesNative(bytes data, int size) as bool\n"
    "    extern \"xl6_xrt_siphasher64_digest\" func digestNative() as c_ulonglong\n"
    "    extern \"xl6_xrt_siphasher64_reset\" func resetNative()\n"
    "    extern \"xl6_xrt_siphasher64_clone\" func cloneNative() as point\n"
    "    extern \"xl6_xrt_siphasher64_input_bytes\" func inputBytesNative() as c_ulonglong\n"
    "    func update(string data, int size = -1)\n"
    "        if size < -1 then throw RangeError \"siphash size is invalid\" end\n"
    "        if not self.updateStringNative(data, __xl_std_xrt_string_byte_len(data), size) then throw RangeError \"siphash input is too large\" end\n"
    "    end\n"
    "    func update(bytes data, int size = -1)\n"
    "        if size < -1 then throw RangeError \"siphash size is invalid\" end\n"
    "        if not self.updateBytesNative(data, size) then throw RangeError \"siphash input is too large\" end\n"
    "    end\n"
    "    func digest() as uint64\n"
    "        return uint64(self.digestNative())\n"
    "    end\n"
    "    func reset()\n"
    "        self.resetNative()\n"
    "    end\n"
    "    func clone() as SipHasher64\n"
    "        return SipHasher64(point(self.cloneNative()))\n"
    "    end\n"
    "    func inputBytes() as uint64\n"
    "        return uint64(self.inputBytesNative())\n"
    "    end\n";

const char* xl6StdlibSipHasher64BufferMethodSource =
    "    extern \"xl6_xrt_siphasher64_update_raw\" func updateRawNative(point data, c_size actual, int size) as bool\n"
    "    func updateBuffer(Buffer data, int size = -1)\n"
    "        if size < -1 then throw RangeError \"siphash size is invalid\" end\n"
    "        if not self.updateRawNative(data.data(), c_size(data.len()), size) then throw RangeError \"siphash input is too large\" end\n"
    "    end\n";

const char* xl6StdlibSipHasher64ClassEndSource = "end\n";



#include "xl6_stdlib_net_interfaces.inc"



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



#include "xl6_stdlib_net_engine.inc"
#include "xl6_stdlib_net_socket.inc"






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



#include "src/stdlib/xl6_stdlib_net_proxy.inc"
#include "src/stdlib/xl6_stdlib_tcp_options.inc"






#include "src/stdlib/xl6_stdlib_tcp.inc"

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
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tcp(transport.nativeHandle(), upgrade.nativeHandle(), null, handler, c_size(prefix))))\n"
    "    end\n"
    "\n"
    "    static func attachTcpWithConfig(TcpStream transport, WsUpgrade upgrade, WebSocketConfig config, func(int, int, int, uint64, point, c_size) handler, int prefix = 0) as WebSocket\n"
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tcp(transport.nativeHandle(), upgrade.nativeHandle(), config.nativeHandle(), handler, c_size(prefix))))\n"
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
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tls(transport.nativeHandle(), upgrade.nativeHandle(), null, handler, c_size(prefix))))\n"
    "    end\n"
    "\n"
    "    static func attachTlsWithConfig(TlsStream transport, WsUpgrade upgrade, WebSocketConfig config, func(int, int, int, uint64, point, c_size) handler, int prefix = 0) as WebSocket\n"
    "        return WebSocket(point(__xl_std_runtime_ws_stream_attach_tls(transport.nativeHandle(), upgrade.nativeHandle(), config.nativeHandle(), handler, c_size(prefix))))\n"
    "    end\n";



const char* xl6StdlibWebSocketClassEndSource =
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
    "    extern \"xl6_xrt_json_writer_uint\" func writeUInt(uint64 value) as bool\n"
    "    extern \"xl6_xrt_json_writer_uint_key\" func writeUInt(string key, uint64 value) as bool\n"
    "    extern \"xl6_xrt_json_writer_float\" func writeFloat(float value) as bool\n"
    "    extern \"xl6_xrt_json_writer_float_key\" func writeFloat(string key, float value) as bool\n"
    "    extern \"xl6_xrt_json_writer_string\" func writeString(string value) as bool\n"
    "    extern \"xl6_xrt_json_writer_string_key\" func writeString(string key, string value) as bool\n"
    "    extern \"xl6_xrt_json_writer_value\" func writeValue(var value) as bool\n"
    "    extern \"xl6_xrt_json_writer_value_key\" func writeValue(string key, var value) as bool\n"
    "    extern \"xl6_xrt_json_writer_finish\" func finish() as string\n"
    "\n"
    "end\n"
    "\n"
    "class XlonWriter\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_xlon_writer_create_default\" create()\n"
    "    extern \"xl6_xrt_xlon_writer_create_pretty\" create(bool pretty)\n"
    "    extern \"xl6_xrt_xlon_writer_create_options\" create(bool pretty, bool ignoreUnsupportedEncode, bool ignoreUnsupportedDecode)\n"
    "    extern \"xl6_xrt_xlon_writer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_xlon_writer_begin_array\" func beginArray() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_array_key\" func beginArray(string key) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_array_index\" func beginArray(int index) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_end_array\" func endArray() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_list\" func beginList() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_list_key\" func beginList(string key) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_list_index\" func beginList(int index) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_end_list\" func endList() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_dict\" func beginDict() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_dict_key\" func beginDict(string key) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_dict_index\" func beginDict(int index) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_end_dict\" func endDict() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_set\" func beginSet() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_set_key\" func beginSet(string key) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_begin_set_index\" func beginSet(int index) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_end_set\" func endSet() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_null\" func writeNull() as bool\n"
    "    extern \"xl6_xrt_xlon_writer_null_key\" func writeNull(string key) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_null_index\" func writeNull(int index) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_bool\" func writeBool(bool value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_bool_key\" func writeBool(string key, bool value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_bool_index\" func writeBool(int index, bool value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_int\" func writeInt(int value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_int_key\" func writeInt(string key, int value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_int_index\" func writeInt(int index, int value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_uint\" func writeUInt(uint64 value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_uint_key\" func writeUInt(string key, uint64 value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_uint_index\" func writeUInt(int index, uint64 value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_char\" func writeChar(char value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_char_key\" func writeChar(string key, char value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_char_index\" func writeChar(int index, char value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_float\" func writeFloat(float value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_float_key\" func writeFloat(string key, float value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_float_index\" func writeFloat(int index, float value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_string\" func writeString(string value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_string_key\" func writeString(string key, string value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_string_index\" func writeString(int index, string value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_bytes\" func writeBytes(bytes value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_bytes_key\" func writeBytes(string key, bytes value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_bytes_index\" func writeBytes(int index, bytes value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_tag\" func writeTag(string tag, string payload) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_tag_key\" func writeTag(string key, string tag, string payload) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_tag_index\" func writeTag(int index, string tag, string payload) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_time\" func writeTimeRaw(int value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_time_key\" func writeTimeRaw(string key, int value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_time_index\" func writeTimeRaw(int index, int value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_value\" func writeValue(var value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_value_key\" func writeValue(string key, var value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_value_index\" func writeValue(int index, var value) as bool\n"
    "    extern \"xl6_xrt_xlon_writer_finish\" func finish() as string\n"
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
    "end\n"
    "\n"
    "class XlonOptions\n"
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

#include "src/stdlib/xl6_stdlib_process.inc"

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

static const XL6_StdlibParam xl6StdMathPowParams[] = {
    {"float", "x"},
    {"float", "y"}
};

static const XL6_StdlibParam xl6StdMathTernaryFloatParams[] = {
    {"float", "x"},
    {"float", "y"},
    {"float", "z"}
};

static const XL6_StdlibParam xl6StdMathScaleNativeParams[] = {
    {"float", "x"},
    {"c_int", "exponent"}
};

static const XL6_StdlibParam xl6StdMathAtan2Params[] = {
    {"float", "y"},
    {"float", "x"}
};

static const XL6_StdlibParam xl6StdMathBinaryFloatParams[] = {
    {"float", "a"},
    {"float", "b"}
};

static const XL6_StdlibParam xl6StdMathIsCloseParams[] = {
    {"float", "a"},
    {"float", "b"},
    {"float", "absTolerance"},
    {"float", "relTolerance"}
};

static const XL6_StdlibParam xl6StdMathIntNearParams[] = {
    {"int", "a"},
    {"int", "b"},
    {"uint64", "tolerance"}
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


static const XL6_StdlibParam xl6StdXrtInStrParams[] = {
    {"string", "text"},
    {"string", "part"},
    {"c_bool", "ignoreCase"}
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

static const XL6_StdlibParam xl6StdRuntimeLogLevelParams[] = {
    {"int", "level"}
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
    {"int", "min"},
    {"int", "max"}
};

static const XL6_StdlibParam xl6StdXrtRandBelowParams[] = {
    {"uint64", "bound"}
};

static const XL6_StdlibParam xl6StdXrtRandCountParams[] = {
    {"int", "count"}
};

static const XL6_StdlibParam xl6StdXrtRandBufferParams[] = {
    {"point", "target"}
};

static const XL6_StdlibParam xl6StdXrtRandArrayParams[] = {
    {"array", "values"}
};

static const XL6_StdlibParam xl6StdXrtRandSeedParams[] = {
    {"uint64", "seed"},
    {"uint64", "seq"}
};

static const XL6_StdlibParam xl6StdXrtRandStrParams[] = {
    {"string", "alphabet"},
    {"c_size", "actualSize"},
    {"int", "alphabetSize"},
    {"int", "count"}
};

static const XL6_StdlibParam xl6StdPathUnaryParams[] = {
    {"string", "path"}
};

static const XL6_StdlibParam xl6StdConsoleStreamParams[] = {
    {"c_int", "stream"}
};
static const XL6_StdlibParam xl6StdConsoleIntStreamParams[] = {{"int", "stream"}};
static const XL6_StdlibParam xl6StdConsoleLimitParams[] = {{"int", "maxBytes"}};
static const XL6_StdlibParam xl6StdConsoleStyledParams[] = {
    {"string", "text"}, {"int", "stream"}, {"int", "fg"}, {"int", "bg"}, {"int", "attributes"}
};

static const XL6_StdlibParam xl6StdIoReadFileParams[] = {
    {"string", "path"}, {"int", "maxBytes"}
};
static const XL6_StdlibParam xl6StdIoCopyParams[] = {
    {"point", "source"}, {"point", "target"}, {"int", "limit"}, {"int", "mode"}
};
static const XL6_StdlibParam xl6StdIoWriteBufferFileParams[] = {
    {"string", "path"}, {"point", "data"}, {"int", "mode"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdIoWriteFileParams[] = {
    {"string", "path"}, {"bytes", "data"}, {"int", "mode"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdIoWriteTextFileParams[] = {
    {"string", "path"}, {"string", "data"}, {"int", "mode"}, {"int", "size"}
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

static const XL6_StdlibParam xl6StdLinesValueParams[] = {
    {"var", "records"}
};

static const XL6_StdlibParam xl6StdLinesWriteParams[] = {
    {"string", "path"},
    {"var", "records"}
};

static const XL6_StdlibParam xl6StdConfiguredReadParams[] = {
    {"string", "text"}, {"var", "options"}
};
static const XL6_StdlibParam xl6StdConfiguredWriteParams[] = {
    {"string", "path"}, {"var", "value"}, {"var", "options"}
};

static const XL6_StdlibParam xl6StdXlonTextOptionsRuntimeParams[] = {
    {"string", "text"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};

static const XL6_StdlibParam xl6StdXlonVisitOptionsRuntimeParams[] = {
    {"string", "text"},
    {"func(var) as bool", "visitor"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};

static const XL6_StdlibParam xl6StdXlonValueOptionsRuntimeParams[] = {
    {"var", "value"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};

static const XL6_StdlibParam xl6StdXlonWriteOptionsRuntimeParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"bool", "pretty"},
    {"bool", "ignoreUnsupportedEncode"},
    {"bool", "ignoreUnsupportedDecode"}
};


static const XL6_StdlibParam xl6StdXidCountParams[] = {
    {"int", "count"}
};
static const XL6_StdlibParam xl6StdXidTextParams[] = {
    {"string", "text"}
};
static const XL6_StdlibParam xl6StdXidBytesParams[] = {
    {"bytes", "data"}
};
static const XL6_StdlibParam xl6StdXidValueParams[] = {
    {"Xid", "value"}
};
static const XL6_StdlibParam xl6StdXidPairParams[] = {
    {"Xid", "left"}, {"Xid", "right"}
};

static const XL6_StdlibParam xl6StdHashStringParams[] = {
    {"string", "data"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashStringSeed32Params[] = {
    {"string", "data"}, {"c_uint", "seed"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashStringSeed64Params[] = {
    {"string", "data"}, {"c_ulonglong", "seed"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashBytesParams[] = {
    {"bytes", "data"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashBytesSeed32Params[] = {
    {"bytes", "data"}, {"c_uint", "seed"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashBytesSeed64Params[] = {
    {"bytes", "data"}, {"c_ulonglong", "seed"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashRawParams[] = {
    {"point", "data"}, {"c_size", "actualSize"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashRawSeed32Params[] = {
    {"point", "data"}, {"c_size", "actualSize"}, {"c_uint", "seed"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdHashRawSeed64Params[] = {
    {"point", "data"}, {"c_size", "actualSize"}, {"c_ulonglong", "seed"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdSipHashStringParams[] = {
    {"string", "data"}, {"bytes", "key"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdSipHashBytesParams[] = {
    {"bytes", "data"}, {"bytes", "key"}, {"int", "size"}
};
static const XL6_StdlibParam xl6StdSipHashRawParams[] = {
    {"point", "data"}, {"c_size", "actualSize"}, {"bytes", "key"}, {"int", "size"}
};


static const char xl6StdMathSignBody[] =
    "func __xl_std_math_sign(float x) as int\n"
    "    return int(__xl_std_xrt_math_sign(x))\n"
    "end\n";

/* llabs(INT64_MIN) is undefined in C. The language result has no positive
 * representation, so reject that one operand before negating. */
static const char xl6StdMathAbsIntBody[] =
    "func __xl_std_math_abs(int x) as int\n"
    "    if x == -9223372036854775808 then\n"
    "        throw RangeError \"absolute value overflows int\"\n"
    "    end\n"
    "    if x < 0 then\n"
    "        return -x\n"
    "    end\n"
    "    return x\n"
    "end\n";
static const char xl6StdMathAbsInt8Body[] =
    "func __xl_std_math_abs(int8 x) as int8\n"
    "    if x == int8(-128) then throw RangeError \"absolute value overflows int8\" end\n"
    "    if x < 0 then return int8(-int(x)) end\n"
    "    return x\n"
    "end\n";
static const char xl6StdMathAbsInt16Body[] =
    "func __xl_std_math_abs(int16 x) as int16\n"
    "    if x == int16(-32768) then throw RangeError \"absolute value overflows int16\" end\n"
    "    if x < 0 then return int16(-int(x)) end\n"
    "    return x\n"
    "end\n";
static const char xl6StdMathAbsInt32Body[] =
    "func __xl_std_math_abs(int32 x) as int32\n"
    "    if x == int32(-2147483648) then throw RangeError \"absolute value overflows int32\" end\n"
    "    if x < 0 then return int32(-int(x)) end\n"
    "    return x\n"
    "end\n";

/* C's exponent and ilogb result use int; the public API uses xlang int and
 * makes narrowing explicit at the declaration boundary. */
static const char xl6StdMathLdexpBody[] =
    "func __xl_std_math_ldexp(float x, int exponent) as float\n"
    "    c_int nativeExponent = c_int(exponent)\n"
    "    if int(nativeExponent) != exponent then\n"
    "        throw RangeError \"exponent is outside C int range\"\n"
    "    end\n"
    "    return __xl_std_math_ldexp_native(x, nativeExponent)\n"
    "end\n";
static const char xl6StdMathScaleByPowerOfTwoBody[] =
    "func __xl_std_math_scale_by_power_of_two(float x, int exponent) as float\n"
    "    c_int nativeExponent = c_int(exponent)\n"
    "    if int(nativeExponent) != exponent then\n"
    "        throw RangeError \"exponent is outside C int range\"\n"
    "    end\n"
    "    return __xl_std_math_scalbn_native(x, nativeExponent)\n"
    "end\n";
static const char xl6StdMathIlogbBody[] =
    "func __xl_std_math_ilogb(float x) as int\n"
    "    return int(__xl_std_math_ilogb_native(x))\n"
    "end\n";
static const char xl6StdMathDivRemBody[] =
    "func __xl_std_math_div_rem(int numerator, int denominator) as (int, int)\n"
    "    return numerator / denominator, numerator % denominator\n"
    "end\n";

/* Parse is a closed scalar ABI. tryParse is only language exception/tuple
 * glue; it catches input/range errors, not runtime resource failures. */
static const char xl6StdIntParseBody[] =
    "extern \"xl6NumberIntParseV1\" func __xl_std_int_parse(string text, int base = 10, int flags = 0, int size = -1) as int\n";
static const char xl6StdIntTryParseBody[] =
    "func __xl_std_int_try_parse(string text, int base = 10, int flags = 0, int size = -1) as (bool, int)\n"
    "    try\n"
    "        return true, __xl_std_int_parse(text, base, flags, size)\n"
    "    catch ValueError e\n"
    "        return false, 0\n"
    "    catch RangeError e\n"
    "        return false, 0\n"
    "    end\n"
    "end\n";
static const char xl6StdUInt64ParseBody[] =
    "extern \"xl6NumberUIntParseV1\" func __xl_std_uint64_parse(string text, int base = 10, int flags = 0, int size = -1) as uint64\n";
static const char xl6StdUInt64TryParseBody[] =
    "func __xl_std_uint64_try_parse(string text, int base = 10, int flags = 0, int size = -1) as (bool, uint64)\n"
    "    try\n"
    "        return true, __xl_std_uint64_parse(text, base, flags, size)\n"
    "    catch ValueError e\n"
    "        return false, uint64(0)\n"
    "    catch RangeError e\n"
    "        return false, uint64(0)\n"
    "    end\n"
    "end\n";
static const char xl6StdFloatParseBody[] =
    "extern \"xl6NumberFloatParseV1\" func __xl_std_float_parse(string text, int flags = 8, int size = -1) as float\n";
static const char xl6StdFloatTryParseBody[] =
    "func __xl_std_float_try_parse(string text, int flags = 8, int size = -1) as (bool, float)\n"
    "    try\n"
    "        return true, __xl_std_float_parse(text, flags, size)\n"
    "    catch ValueError e\n"
    "        return false, 0.0\n"
    "    catch RangeError e\n"
    "        return false, 0.0\n"
    "    end\n"
    "end\n";

#define XL6_STD_WIDTH_PARSE(TYPE, SYMBOL) \
    "extern \"" #SYMBOL "\" func __xl_std_" #TYPE \
    "_parse(string text, int base = 10, int flags = 0, int size = -1) as " #TYPE "\n"
#define XL6_STD_WIDTH_TRY_PARSE(TYPE) \
    "func __xl_std_" #TYPE "_try_parse(string text, int base = 10, int flags = 0, int size = -1) as (bool, " #TYPE ")\n" \
    "    try\n" \
    "        return true, __xl_std_" #TYPE "_parse(text, base, flags, size)\n" \
    "    catch ValueError e\n" \
    "        return false, " #TYPE "(0)\n" \
    "    catch RangeError e\n" \
    "        return false, " #TYPE "(0)\n" \
    "    end\n" \
    "end\n"
static const char xl6StdInt8ParseBody[] =
    XL6_STD_WIDTH_PARSE(int8, xl6NumberInt8ParseV1);
static const char xl6StdInt16ParseBody[] =
    XL6_STD_WIDTH_PARSE(int16, xl6NumberInt16ParseV1);
static const char xl6StdInt32ParseBody[] =
    XL6_STD_WIDTH_PARSE(int32, xl6NumberInt32ParseV1);
static const char xl6StdInt64ParseBody[] =
    XL6_STD_WIDTH_PARSE(int64, xl6NumberInt64ParseV1);
static const char xl6StdUInt8ParseBody[] =
    XL6_STD_WIDTH_PARSE(uint8, xl6NumberUInt8ParseV1);
static const char xl6StdUInt16ParseBody[] =
    XL6_STD_WIDTH_PARSE(uint16, xl6NumberUInt16ParseV1);
static const char xl6StdUInt32ParseBody[] =
    XL6_STD_WIDTH_PARSE(uint32, xl6NumberUInt32ParseV1);
static const char xl6StdInt8TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(int8);
static const char xl6StdInt16TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(int16);
static const char xl6StdInt32TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(int32);
static const char xl6StdInt64TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(int64);
static const char xl6StdUInt8TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(uint8);
static const char xl6StdUInt16TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(uint16);
static const char xl6StdUInt32TryParseBody[] = XL6_STD_WIDTH_TRY_PARSE(uint32);
#undef XL6_STD_WIDTH_PARSE
#undef XL6_STD_WIDTH_TRY_PARSE
static const char xl6StdIntStringBody[] =
    "extern \"xl6NumberIntStringV1\" func __xl_std_int_to_string(int value, int base = 10, int flags = 0) as string\n";
static const char xl6StdUIntStringBody[] =
    "extern \"xl6NumberUIntStringV1\" func __xl_std_int_to_string(uint64 value, int base = 10, int flags = 0) as string\n";
static const char xl6StdFloatStringBody[] =
    "extern \"xl6NumberFloatStringV1\" func __xl_std_float_to_string(float value, int flags = 0) as string\n";
/* The C output-pointer ABI for these private declarations is supplied by the
 * standard-library catalog after parsing, not by public parameter syntax. */
static const char xl6StdMathFrexpBody[] =
    "extern \"frexp\" @no_raise @nonblocking func __xl_std_math_frexp_native(float x, c_int exponent) as float\n"
    "func __xl_std_math_frexp(float x) as (float, int)\n"
    "    if x == 0.0 then\n"
    "        return x, 0\n"
    "    end\n"
    "    c_int exponent = 0\n"
    "    float fraction = __xl_std_math_frexp_native(x, exponent)\n"
    "    return fraction, int(exponent)\n"
    "end\n";
static const char xl6StdMathModfBody[] =
    "extern \"modf\" @no_raise @nonblocking func __xl_std_math_modf_native(float x, float integerPart) as float\n"
    "func __xl_std_math_modf(float x) as (float, float)\n"
    "    if x == 0.0 then\n"
    "        return x, x\n"
    "    end\n"
    "    float integerPart = 0.0\n"
    "    float fractionalPart = __xl_std_math_modf_native(x, integerPart)\n"
    "    return fractionalPart, integerPart\n"
    "end\n";
static const char xl6StdMathRemQuoBody[] =
    "extern \"remquo\" @no_raise @nonblocking func __xl_std_math_rem_quo_native(float x, float y, c_int quotient) as float\n"
    "func __xl_std_math_rem_quo(float x, float y) as (float, int)\n"
    "    c_int quotient = 0\n"
    "    float remainder = __xl_std_math_rem_quo_native(x, y, quotient)\n"
    "    return remainder, int(quotient)\n"
    "end\n";
static const char xl6StdMathSignBitBody[] =
    "func __xl_std_math_sign_bit(float x) as bool\n"
    "    return __xl_std_math_copy_sign(1.0, x) < 0.0\n"
    "end\n";
/* xlang float is IEEE binary64. These two exactly rounded literals bound
 * the normal finite range without an extra libm call or platform FP_* ABI. */
static const char xl6StdMathIsNormalBody[] =
    "func __xl_std_math_is_normal(float x) as bool\n"
    "    float magnitude = __xl_std_math_abs(x)\n"
    "    return magnitude >= 2.2250738585072014e-308 and magnitude <= 1.7976931348623157e308\n"
    "end\n";
static const char xl6StdMathIsSubnormalBody[] =
    "func __xl_std_math_is_subnormal(float x) as bool\n"
    "    float magnitude = __xl_std_math_abs(x)\n"
    "    return magnitude > 0.0 and magnitude < 2.2250738585072014e-308\n"
    "end\n";
static const char xl6StdMathClassifyBody[] =
    "func __xl_std_math_classify(float x) as FloatClass\n"
    "    if x != x then\n"
    "        return FloatClass.nan\n"
    "    end\n"
    "    if x == 0.0 then\n"
    "        return FloatClass.zero\n"
    "    end\n"
    "    float magnitude = __xl_std_math_abs(x)\n"
    "    if magnitude > 1.7976931348623157e308 then\n"
    "        return FloatClass.infinite\n"
    "    end\n"
    "    if magnitude < 2.2250738585072014e-308 then\n"
    "        return FloatClass.subnormal\n"
    "    end\n"
    "    return FloatClass.normal\n"
    "end\n";
static const char xl6StdMathIsUnorderedBody[] =
    "func __xl_std_math_is_unordered(float x, float y) as bool\n"
    "    return x != x or y != y\n"
    "end\n";

/* Comparator-driven heap sort has bounded O(log n) auxiliary state and does
 * not erase T to var or pass managed elements through C qsort's void* ABI.
 * The public callback returns negative/zero/positive; it may raise. */
static const char xl6StdArraySortBody[] =
    "func __xl_std_array_sift_down<T>(array<T> values, int root, int limit, func(T, T) as int compare)\n"
    "    while root < limit / 2\n"
    "        int child = root * 2 + 1\n"
    "        if child + 1 < limit and compare(values[child], values[child + 1]) < 0 then\n"
    "            child += 1\n"
    "        end\n"
    "        if compare(values[root], values[child]) >= 0 then\n"
    "            return\n"
    "        end\n"
    "        T temp = values[root]\n"
    "        values[root] = values[child]\n"
    "        values[child] = temp\n"
    "        root = child\n"
    "    end\n"
    "end\n"
    "func __xl_std_array_sort<T>(array<T> values, func(T, T) as int compare)\n"
    "    int count = values.length\n"
    "    int start = count / 2\n"
    "    while start > 0\n"
    "        start -= 1\n"
    "        __xl_std_array_sift_down::<T>(values, start, count, compare)\n"
    "    end\n"
    "    int limit = count\n"
    "    while limit > 1\n"
    "        limit -= 1\n"
    "        T temp = values[0]\n"
    "        values[0] = values[limit]\n"
    "        values[limit] = temp\n"
    "        __xl_std_array_sift_down::<T>(values, 0, limit, compare)\n"
    "    end\n"
    "end\n";

static const char xl6StdArraySortByBody[] =
    "func __xl_std_array_sift_down_by<T, K>(array<T> values, array<K> keys, int root, int limit, func(K, K) as int compare)\n"
    "    while root < limit / 2\n"
    "        int child = root * 2 + 1\n"
    "        if child + 1 < limit and compare(keys[child], keys[child + 1]) < 0 then\n"
    "            child += 1\n"
    "        end\n"
    "        if compare(keys[root], keys[child]) >= 0 then\n"
    "            return\n"
    "        end\n"
    "        K keyTemp = keys[root]\n"
    "        keys[root] = keys[child]\n"
    "        keys[child] = keyTemp\n"
    "        T valueTemp = values[root]\n"
    "        values[root] = values[child]\n"
    "        values[child] = valueTemp\n"
    "        root = child\n"
    "    end\n"
    "end\n"
    "func __xl_std_array_sort_by<T, K>(array<T> values, func(T) as K keyOfValue, func(K, K) as int compare)\n"
    "    array<K> keys = []\n"
    "    int i = 0\n"
    "    while i < values.length\n"
    "        if not keys.push(keyOfValue(values[i])) then\n"
    "            throw RuntimeError \"sortBy key allocation failed\"\n"
    "        end\n"
    "        i += 1\n"
    "    end\n"
    "    int count = values.length\n"
    "    int start = count / 2\n"
    "    while start > 0\n"
    "        start -= 1\n"
    "        __xl_std_array_sift_down_by::<T, K>(values, keys, start, count, compare)\n"
    "    end\n"
    "    int limit = count\n"
    "    while limit > 1\n"
    "        limit -= 1\n"
    "        K keyTemp = keys[0]\n"
    "        keys[0] = keys[limit]\n"
    "        keys[limit] = keyTemp\n"
    "        T valueTemp = values[0]\n"
    "        values[0] = values[limit]\n"
    "        values[limit] = valueTemp\n"
    "        __xl_std_array_sift_down_by::<T, K>(values, keys, 0, limit, compare)\n"
    "    end\n"
    "end\n";

static const char xl6StdArrayBinarySearchBody[] =
    "func __xl_std_array_binary_search<T>(array<T> values, T needle, func(T, T) as int compare) as int\n"
    "    int low = 0\n"
    "    int high = values.length\n"
    "    while low < high\n"
    "        int middle = low + (high - low) / 2\n"
    "        int order = compare(values[middle], needle)\n"
    "        if order < 0 then\n"
    "            low = middle + 1\n"
    "        else\n"
    "            high = middle\n"
    "        end\n"
    "    end\n"
    "    if low < values.length and compare(values[low], needle) == 0 then\n"
    "        return low\n"
    "    end\n"
    "    return -1\n"
    "end\n";

/* ASCII is defined on Unicode scalar values, never on the process C locale.
 * Source bodies keep its small predicates visible to ordinary XLIR/Cgen. */
static const char xl6StdAsciiIsAlphaBody[] =
    "func __xl_std_ascii_is_alpha(char c) as bool\n"
    "    return (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z')\n"
    "end\n";
static const char xl6StdAsciiIsDigitBody[] =
    "func __xl_std_ascii_is_digit(char c) as bool\n"
    "    return c >= '0' and c <= '9'\n"
    "end\n";
static const char xl6StdAsciiIsAlphaNumericBody[] =
    "func __xl_std_ascii_is_alphanumeric(char c) as bool\n"
    "    return (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z') or (c >= '0' and c <= '9')\n"
    "end\n";
static const char xl6StdAsciiIsHexDigitBody[] =
    "func __xl_std_ascii_is_hex_digit(char c) as bool\n"
    "    return (c >= '0' and c <= '9') or (c >= 'A' and c <= 'F') or (c >= 'a' and c <= 'f')\n"
    "end\n";
static const char xl6StdAsciiIsWhitespaceBody[] =
    "func __xl_std_ascii_is_whitespace(char c) as bool\n"
    "    return c == ' ' or (c >= '\\t' and c <= '\\r')\n"
    "end\n";
static const char xl6StdAsciiIsBlankBody[] =
    "func __xl_std_ascii_is_blank(char c) as bool\n"
    "    return c == ' ' or c == '\\t'\n"
    "end\n";
static const char xl6StdAsciiIsControlBody[] =
    "func __xl_std_ascii_is_control(char c) as bool\n"
    "    return c < ' ' or c == 127\n"
    "end\n";
static const char xl6StdAsciiIsPrintableBody[] =
    "func __xl_std_ascii_is_printable(char c) as bool\n"
    "    return c >= ' ' and c <= '~'\n"
    "end\n";
static const char xl6StdAsciiIsGraphicalBody[] =
    "func __xl_std_ascii_is_graphical(char c) as bool\n"
    "    return c >= '!' and c <= '~'\n"
    "end\n";
static const char xl6StdAsciiIsPunctuationBody[] =
    "func __xl_std_ascii_is_punctuation(char c) as bool\n"
    "    return (c >= '!' and c <= '/') or (c >= ':' and c <= '@') or (c >= '[' and c <= '`') or (c >= '{' and c <= '~')\n"
    "end\n";
static const char xl6StdAsciiIsLowerBody[] =
    "func __xl_std_ascii_is_lower(char c) as bool\n"
    "    return c >= 'a' and c <= 'z'\n"
    "end\n";
static const char xl6StdAsciiIsUpperBody[] =
    "func __xl_std_ascii_is_upper(char c) as bool\n"
    "    return c >= 'A' and c <= 'Z'\n"
    "end\n";
static const char xl6StdAsciiToLowerBody[] =
    "func __xl_std_ascii_to_lower(char c) as char\n"
    "    if c >= 'A' and c <= 'Z' then\n"
    "        return char(c + 32)\n"
    "    end\n"
    "    return c\n"
    "end\n";
static const char xl6StdAsciiToUpperBody[] =
    "func __xl_std_ascii_to_upper(char c) as char\n"
    "    if c >= 'a' and c <= 'z' then\n"
    "        return char(c - 32)\n"
    "    end\n"
    "    return c\n"
    "end\n";

static const char xl6StdStringCompareBody[] =
    "extern \"xl6TextCompareV1\" func __xl_std_string_compare(string left, string right, bool ignoreCase = false, int leftSize = -1, int rightSize = -1) as int\n";

static const char xl6StdStringLenBody[] =
    "extern \"xl6TextLengthV1\" func __xl_std_string_len(string text, int size = -1) as int\n";

static const char xl6StdStringByteLenBody[] =
    "extern \"xl6TextByteLengthV1\" func __xl_std_string_byte_len(string text, int size = -1) as int\n";

static const char xl6StdStringEqualsBody[] =
    "extern \"xl6TextEqualsV1\" func __xl_std_string_equals(string left, string right, bool ignoreCase = false, int leftSize = -1, int rightSize = -1) as bool\n";

static const char xl6StdStringContainsBody[] =
    "extern \"xl6TextContainsV1\" func __xl_std_string_contains(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as bool\n";

static const char xl6StdStringIndexOfBody[] =
    "extern \"xl6TextIndexOfV1\" func __xl_std_string_index_of(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as int\n";

static const char xl6StdStringLastIndexOfBody[] =
    "extern \"xl6TextLastIndexOfV1\" func __xl_std_string_last_index_of(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as int\n";

static const char xl6StdStringCountBody[] =
    "extern \"xl6TextCountV1\" func __xl_std_string_count(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as int\n";

static const char xl6StdStringStartsWithBody[] =
    "extern \"xl6TextStartsWithV1\" func __xl_std_string_starts_with(string text, string prefix, bool ignoreCase = false, int size = -1, int prefixSize = -1) as bool\n";

static const char xl6StdStringEndsWithBody[] =
    "extern \"xl6TextEndsWithV1\" func __xl_std_string_ends_with(string text, string suffix, bool ignoreCase = false, int size = -1, int suffixSize = -1) as bool\n";

static const char xl6StdStringHasAnyBody[] =
    "extern \"xl6TextHasAnyV1\" func __xl_std_string_has_any(string text, string chars, int size = -1, int charsSize = -1) as bool\n";

static const char xl6StdStringLikeBody[] =
    "extern \"xl6TextLikeV1\" func __xl_std_string_like(string text, string pattern, bool ignoreCase = false, int size = -1, int patternSize = -1) as bool\n";

static const char xl6StdStringApproxBody[] =
    "extern \"xl6TextApproxV1\" func __xl_std_string_approx(string left, string right, float tolerance = 0.95, int leftSize = -1, int rightSize = -1) as bool\n";

static const char xl6StdStringSimBody[] =
    "extern \"xl6TextSimilarityV1\" func __xl_std_string_sim(string left, string right, int leftSize = -1, int rightSize = -1) as float\n";

static const char xl6StdStringIsUtf8Body[] =
    "extern \"xl6TextIsUtf8V1\" func __xl_std_string_is_utf8(string text, int size = -1) as bool\n";

static const char xl6StdIntFormatBody[] =
    "extern \"xl6NumberIntFormatV1\" func __xl_std_int_format(int value, string format = \"\", int size = -1) as string\n";

static const char xl6StdIntFormatUIntBody[] =
    "extern \"xl6NumberUIntFormatV1\" func __xl_std_int_format(uint64 value, string format = \"\", int size = -1) as string\n";

static const char xl6StdFloatFormatBody[] =
    "extern \"xl6NumberFloatFormatV1\" func __xl_std_float_format(float value, string format = \"\", int size = -1) as string\n";

static const char xl6StdStringFormatBodyInt[] =
    "extern \"xl6NumberIntFormatV1\" func __xl_std_string_format(int value, string format = \"\", int size = -1) as string\n";

static const char xl6StdStringFormatBodyUInt[] =
    "extern \"xl6NumberUIntFormatV1\" func __xl_std_string_format(uint64 value, string format = \"\", int size = -1) as string\n";

static const char xl6StdStringFormatBodyFloat[] =
    "extern \"xl6NumberFloatFormatV1\" func __xl_std_string_format(float value, string format = \"\", int size = -1) as string\n";

static const char xl6StdStringLowerBody[] =
    "extern \"xl6TextLowerV1\" func __xl_std_string_lower(string text, int size = -1) as string\n";

static const char xl6StdStringUpperBody[] =
    "extern \"xl6TextUpperV1\" func __xl_std_string_upper(string text, int size = -1) as string\n";

static const char xl6StdStringTrimBody[] =
    "extern \"xl6TextTrimV1\" func __xl_std_string_trim(string text, string chars = \"\", int size = -1, int charsSize = -1) as string\n";

static const char xl6StdStringLTrimBody[] =
    "extern \"xl6TextLTrimV1\" func __xl_std_string_ltrim(string text, string chars = \"\", int size = -1, int charsSize = -1) as string\n";

static const char xl6StdStringRTrimBody[] =
    "extern \"xl6TextRTrimV1\" func __xl_std_string_rtrim(string text, string chars = \"\", int size = -1, int charsSize = -1) as string\n";

static const char xl6StdStringFilterBody[] =
    "extern \"xl6TextFilterV1\" func __xl_std_string_filter(string text, string chars, int size = -1, int charsSize = -1) as string\n";

static const char xl6StdStringReplaceBody[] =
    "extern \"xl6TextReplaceV1\" func __xl_std_string_replace(string text, string oldText, string newText, int size = -1, int oldSize = -1, int newSize = -1) as string\n";

static const char xl6StdStringSplitBody[] =
    "extern \"xl6TextSplitV2\" func __xl_std_string_split(string text, string sep, int size = -1, int sepSize = -1) as array<string>\n";

static const char xl6StdStringSliceBody[] =
    "extern \"xl6TextSliceV1\" func __xl_std_string_slice(string text, int start, int count = -1, int size = -1) as string\n";

static const char xl6StdStringLeftBody[] =
    "extern \"xl6TextLeftV1\" func __xl_std_string_left(string text, int count, int size = -1) as string\n";

static const char xl6StdStringRightBody[] =
    "extern \"xl6TextRightV1\" func __xl_std_string_right(string text, int count, int size = -1) as string\n";

static const char xl6StdStringMidBody[] =
    "extern \"xl6TextSliceV1\" func __xl_std_string_mid(string text, int start, int count = -1, int size = -1) as string\n";

static const char xl6StdStringCharAtBody[] =
    "extern \"xl6TextCharAtV1\" func __xl_std_string_char_at(string text, int index, int size = -1) as char\n";

static const char xl6StdStringByteAtBody[] =
    "extern \"xl6TextByteAtV1\" func __xl_std_string_byte_at(string text, int index, int size = -1) as uint8\n";

static const char xl6StdStringJoinBody[] =
    "extern \"xl6TextJoinV1\" func __xl_std_string_join(array<string> values, string sep = \"\", int sepSize = -1, int itemSize = -1) as string\n";

static const char xl6StdStringRepeatBody[] =
    "extern \"xl6TextRepeatV1\" func __xl_std_string_repeat(string text, int count, int size = -1) as string\n";

static const char xl6StdStringIsEmptyBody[] =
    "extern \"xl6TextIsEmptyV1\" func __xl_std_string_is_empty(string text, int size = -1) as bool\n";

static const char xl6StdStringIsBlankBody[] =
    "extern \"xl6TextIsBlankV1\" func __xl_std_string_is_blank(string text, int size = -1) as bool\n";

static const char xl6StdStringRemoveBody[] =
    "extern \"xl6TextRemoveV1\" func __xl_std_string_remove(string text, string part, bool ignoreCase = false, int size = -1, int partSize = -1) as string\n";

static const char xl6StdStringRemoveAtBody[] =
    "extern \"xl6TextRemoveAtV1\" func __xl_std_string_remove_at(string text, int start, int count, int size = -1) as string\n";

static const char xl6StdStringInsertBody[] =
    "extern \"xl6TextInsertV1\" func __xl_std_string_insert(string text, int start, string part, int size = -1, int partSize = -1) as string\n";

static const char xl6StdStringPadLeftBody[] =
    "extern \"xl6TextPadLeftV1\" func __xl_std_string_pad_left(string text, int width, string pad = \" \", int size = -1, int padSize = -1) as string\n";

static const char xl6StdStringPadRightBody[] =
    "extern \"xl6TextPadRightV1\" func __xl_std_string_pad_right(string text, int width, string pad = \" \", int size = -1, int padSize = -1) as string\n";

static const char xl6StdStringPadCenterBody[] =
    "extern \"xl6TextPadCenterV1\" func __xl_std_string_pad_center(string text, int width, string pad = \" \", int size = -1, int padSize = -1) as string\n";

static const char xl6StdStringReverseBody[] =
    "extern \"xl6TextReverseV1\" func __xl_std_string_reverse(string text, int size = -1) as string\n";

static const char xl6StdStringSplitLinesBody[] =
    "extern \"xl6TextLinesV2\" func __xl_std_string_split_lines(string text, int size = -1) as array<string>\n";

static const char xl6StdStringFieldsBody[] =
    "extern \"xl6TextFieldsV2\" func __xl_std_string_fields(string text, int size = -1) as array<string>\n";

static const char xl6StdStringCharsBody[] =
    "extern \"xl6TextCharsV2\" func __xl_std_string_chars(string text, int size = -1) as array<char>\n";

static const char xl6StdStringBytesBody[] =
    "extern \"xl6TextBytesV1\" func __xl_std_string_bytes(string text, int size = -1) as bytes\n";

static const char xl6StdStringGlobBody[] =
    "extern \"xl6TextGlobV1\" func __xl_std_string_glob(string text, string pattern, int flags = 0, int size = -1, int patternSize = -1) as bool\n";
static const char xl6StdStringDistanceBody[] =
    "extern \"xl6TextDistanceV1\" func __xl_std_string_distance(string left, string right, int limit = -1, int leftSize = -1, int rightSize = -1) as int\n";

static const char xl6StdProcessExecBody[] =
    "func __xl_std_process_exec(string command, int timeoutMs = -1, int size = -1) as string\n    return ProcessCommand(command, size).run(b\"\", timeoutMs).stdoutText()\nend\n";

static const char xl6StdProcessSuccessBody[] =
    "func __xl_std_process_success(string command, int timeoutMs = -1, int size = -1) as bool\n    return ProcessCommand(command, size).run(b\"\", timeoutMs).success\nend\n";

static const char xl6StdProcessExitCodeBody[] =
    "func __xl_std_process_exit_code(string command, int timeoutMs = -1, int size = -1) as int\n    return ProcessCommand(command, size).run(b\"\", timeoutMs).exitCode\nend\n";

static const char xl6StdProcessRunBody[] =
    "func __xl_std_process_run(string command, int timeoutMs = -1, int size = -1) as ProcessResult\n    return ProcessCommand(command, size).run(b\"\", timeoutMs)\nend\n";

static const char xl6StdProcessRunCommandBody[] =
    "func __xl_std_process_run(ProcessCommand command, bytes input = b\"\", int timeoutMs = -1, int size = -1) as ProcessResult\n"
    "    return command.run(input, timeoutMs, size)\nend\n";
static const char xl6StdProcessRunOptionsBody[] =
    "func __xl_std_process_run(ProcessCommand command, ProcessRunOptions options, bytes input = b\"\", int size = -1) as ProcessResult\n"
    "    return command.run(options, input, size)\nend\n";
static const char xl6StdProcessRunFileBody[] =
    "func __xl_std_process_run_file(string program, array<string> args = [], int timeoutMs = -1, int size = -1, int argSize = -1) as ProcessResult\n"
    "    return ProcessCommand(program, args, size, argSize).run(b\"\", timeoutMs)\nend\n";
static const char xl6StdProcessCmdBody[] =
    "func __xl_std_process_cmd(string program, array<string> args = [], int size = -1, int argSize = -1) as ProcessCommand\n"
    "    return ProcessCommand(program, args, size, argSize)\nend\n";
static const char xl6StdProcessShellBody[] =
    "func __xl_std_process_shell(string command, int size = -1) as ProcessCommand\n"
    "    return ProcessCommand(command, size)\nend\n";

static const char xl6StdProcessPipelineBody[] =
    "func __xl_std_process_pipeline(array<ProcessCommand> commands, bytes input = b\"\", int timeoutMs = -1, int size = -1) as PipelineResult\n"
    "    auto pipeline = ProcessPipeline()\n"
    "    for command in commands\n        pipeline.add(command)\n    end\n"
    "    return pipeline.run(input, timeoutMs, size)\n"
    "end\n";
static const char xl6StdProcessPipelineOptionsBody[] =
    "func __xl_std_process_pipeline(array<ProcessCommand> commands, ProcessRunOptions options, bytes input = b\"\", int size = -1) as PipelineResult\n"
    "    auto pipeline = ProcessPipeline()\n"
    "    for command in commands\n        pipeline.add(command)\n    end\n"
    "    return pipeline.run(options, input, size)\n"
    "end\n";

static const char xl6StdProcessStartBody[] =
    "func __xl_std_process_start(string command, int size = -1) as Process\n"
    "    return Process(ProcessCommand(command, size))\n"
    "end\n";

static const char xl6StdProcessStartCommandBody[] =
    "func __xl_std_process_start(ProcessCommand command) as Process\n"
    "    return Process(command)\n"
    "end\n";

static const char xl6StdProcessStartFileBody[] =
    "func __xl_std_process_start_file(string program, array<string> args = [], int size = -1, int argSize = -1) as Process\n"
    "    return Process(ProcessCommand(program, args, size, argSize))\n"
    "end\n";

static const char xl6StdProcessTerminalSupportedBody[] =
    "extern \"xl6ProcessTerminalSupportedV1\" @no_raise func __xl_std_process_terminal_supported() as bool\n";

static const char xl6StdProcessOpenBody[] =
    "extern \"xl6ProcessOpenV1\" func __xl_std_process_open(string target, int size = -1) as bool\n";


static const char xl6StdSignalOn0Body[] =
    "func __xl_std_signal_on(int code, func() handler, int capacity = 64) as SignalHandle\n"
    "    return SignalHandle(SignalSubscription(code, false, capacity), func(SignalEvent event) as bool\n"
    "        handler()\n        return true\n    end, true)\nend\n";

static const char xl6StdSignalOn1Body[] =
    "func __xl_std_signal_on(int code, func(SignalEvent) handler, int capacity = 64) as SignalHandle\n"
    "    return SignalHandle(SignalSubscription(code, false, capacity), func(SignalEvent event) as bool\n"
    "        handler(event)\n        return true\n    end, true)\nend\n";

static const char xl6StdSignalOnce0Body[] =
    "func __xl_std_signal_once(int code, func() handler, int capacity = 64) as SignalHandle\n"
    "    return SignalHandle(SignalSubscription(code, true, capacity), func(SignalEvent event) as bool\n"
    "        handler()\n        return true\n    end, true, true)\nend\n";

static const char xl6StdSignalOnce1Body[] =
    "func __xl_std_signal_once(int code, func(SignalEvent) handler, int capacity = 64) as SignalHandle\n"
    "    return SignalHandle(SignalSubscription(code, true, capacity), func(SignalEvent event) as bool\n"
    "        handler(event)\n        return true\n    end, true, true)\nend\n";

static const char xl6StdSignalSupportedBody[] =
    "extern \"xl6SignalSupportedV1\" func __xl_std_signal_supported(int code) as bool\n";

static const char xl6StdSignalNameBody[] =
    "extern \"xl6SignalNameV1\" func __xl_std_signal_name(int code) as string\n";

static const char xl6StdSignalIgnoreBody[] =
    "extern \"xl6SignalIgnoreV1\" func __xl_std_signal_ignore(int code) as bool\n";

static const char xl6StdSignalRestoreBody[] =
    "extern \"xl6SignalRestoreV1\" func __xl_std_signal_restore(int code) as bool\n";

static const char xl6StdSignalRestoreAllBody[] =
    "extern \"xl6SignalRestoreAllV1\" func __xl_std_signal_restore_all() as bool\n";

static const char xl6StdSignalHealthyBody[] =
    "extern \"xl6SignalHealthyV1\" func __xl_std_signal_healthy() as bool\n";

static const char xl6StdSignalResetBody[] =
    "extern \"xl6SignalResetV1\" func __xl_std_signal_reset()\n";

static const char xl6StdSignalRaiseBody[] =
    "extern \"xl6SignalRaiseV1\" func __xl_std_signal_raise(int code) as bool\n";

static const char xl6StdSignalCountBody[] =
    "extern \"xl6SignalCountV1\" func __xl_std_signal_count(int code = 0) as uint64\n";

static const char xl6StdSignalReceivedBody[] =
    "extern \"xl6SignalReceivedV1\" func __xl_std_signal_received(int code) as bool\n";

static const char xl6StdSignalClearBody[] =
    "extern \"xl6SignalClearV1\" func __xl_std_signal_clear(int code = 0)\n";

static const char xl6StdSignalSendToBody[] =
    "func __xl_std_signal_send_to(channel<SignalEvent> ch, int code, int capacity = 64) as SignalHandle\n"
    "    return SignalHandle(SignalSubscription(code, false, capacity), func(SignalEvent event) as bool\n"
    "        if ch.isClosed() then throw RuntimeError \"signal route channel is closed\" end\n"
    "        return ch.trySend(event)\n    end, false)\nend\n";

static const char xl6StdSignalCancelOnBody[] =
    "func __xl_std_signal_cancel_on(TaskGroup group, int code, int capacity = 64) as SignalHandle\n"
    "    return SignalHandle(SignalSubscription(code, false, capacity), func(SignalEvent event) as bool\n"
    "        group.cancel()\n        return true\n    end, false)\nend\n";

static const char xl6StdTimeFromDateTimeBody[] =
    "extern \"xl6TimeFromDateTimeV1\" func __xl_std_time_from_datetime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int microsecond = 0, int offset = 0) as time\n";

static const char xl6StdTimeNowBody[] =
    "extern \"xl6TimeNowV1\" func __xl_std_time_now() as time\n";

static const char xl6StdTimeTodayBody[] =
    "func __xl_std_time_today() as time\n"
    "    return __xl_std_time_date_part(__xl_std_time_now())\n"
    "end\n";

static const char xl6StdTimeClockBody[] =
    "extern \"xl6TimeMonotonicV1\" func __xl_std_time_clock() as uint64\n";

static const char xl6StdTimeFromTimeBody[] =
    "extern \"xl6TimeFromTimeV1\" func __xl_std_time_from_time(int hour, int minute, int second, int microsecond = 0) as time\n";

static const char xl6StdTimeFromDateBody[] =
    "extern \"xl6TimeFromDateV1\" func __xl_std_time_from_date(int year, int month, int day) as time\n";

static const char xl6StdTimeParseBody[] =
    "extern \"xl6TimeParseAnyV1\" func __xl_std_time_parse(string text, int size = -1) as time\n";

static const char xl6StdTimeParsePatternBody[] =
    "extern \"xl6TimeParsePatternV1\" func __xl_std_time_parse_pattern(string text, string pattern, int size = -1, int patternSize = -1) as time\n";

static const char xl6StdTimeTryParseBody[] =
    "extern \"xl6TimeTryParseAnyV1\" func __xl_std_time_try_parse(string text, int size = -1) as (bool, time)\n";

static const char xl6StdTimeTryParsePatternBody[] =
    "extern \"xl6TimeTryParsePatternV1\" func __xl_std_time_try_parse_pattern(string text, string pattern, int size = -1, int patternSize = -1) as (bool, time)\n";

static const char xl6StdTimeYearBody[] =
    "extern \"xl6TimeYearV1\" @pure @no_raise @nonblocking func __xl_std_time_year(time value) as int\n";

static const char xl6StdTimeMonthBody[] =
    "extern \"xl6TimeMonthV1\" @pure @no_raise @nonblocking func __xl_std_time_month(time value) as int\n";

static const char xl6StdTimeDayBody[] =
    "extern \"xl6TimeDayV1\" @pure @no_raise @nonblocking func __xl_std_time_day(time value) as int\n";

static const char xl6StdTimeHourBody[] =
    "extern \"xl6TimeHourV1\" @pure @no_raise @nonblocking func __xl_std_time_hour(time value) as int\n";

static const char xl6StdTimeMinuteBody[] =
    "extern \"xl6TimeMinuteV1\" @pure @no_raise @nonblocking func __xl_std_time_minute(time value) as int\n";

static const char xl6StdTimeSecondBody[] =
    "extern \"xl6TimeSecondV1\" @pure @no_raise @nonblocking func __xl_std_time_second(time value) as int\n";

static const char xl6StdTimeWeekdayBody[] =
    "extern \"xl6TimeWeekdayV1\" @pure @no_raise @nonblocking func __xl_std_time_weekday(time value) as int\n";

static const char xl6StdTimeDayOfYearBody[] =
    "extern \"xl6TimeDayOfYearV1\" @pure @no_raise @nonblocking func __xl_std_time_day_of_year(time value) as int\n";

static const char xl6StdTimeQuarterBody[] =
    "extern \"xl6TimeQuarterV1\" @pure @no_raise @nonblocking func __xl_std_time_quarter(time value) as int\n";

static const char xl6StdTimeIsLeapYearBody[] =
    "extern \"xl6TimeLeapV1\" @pure @no_raise @nonblocking func __xl_std_time_is_leap_year(int year) as bool\n";

static const char xl6StdTimeDaysInMonthBody[] =
    "extern \"xl6TimeDaysInMonthV1\" func __xl_std_time_days_in_month(int year, int month) as int\n";

static const char xl6StdTimeDaysInYearBody[] =
    "extern \"xl6TimeDaysInYearV1\" @pure @no_raise @nonblocking func __xl_std_time_days_in_year(int year) as int\n";

static const char xl6StdTimeDatePartBody[] =
    "extern \"xl6TimeDatePartV1\" func __xl_std_time_date_part(time value) as time\n";

static const char xl6StdTimeTimePartBody[] =
    "extern \"xl6TimeTimePartV1\" @pure @no_raise @nonblocking func __xl_std_time_time_part(time value) as int\n";

static const char xl6StdTimeIsSameDayBody[] =
    "extern \"xl6TimeSameDayV1\" @pure @no_raise @nonblocking func __xl_std_time_is_same_day(time left, time right) as bool\n";

static const char xl6StdTimeIsSameMonthBody[] =
    "extern \"xl6TimeSameMonthV1\" @pure @no_raise @nonblocking func __xl_std_time_is_same_month(time left, time right) as bool\n";

static const char xl6StdTimeIsSameYearBody[] =
    "extern \"xl6TimeSameYearV1\" @pure @no_raise @nonblocking func __xl_std_time_is_same_year(time left, time right) as bool\n";

static const char xl6StdTimeInRangeBody[] =
    "extern \"xl6TimeInV1\" @pure @no_raise @nonblocking func __xl_std_time_in_range(time value, time start, time endValue) as bool\n";

static const char xl6StdTimeAddYearsBody[] =
    "func __xl_std_time_add_years(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 9)\n"
    "end\n";

static const char xl6StdTimeAddMonthsBody[] =
    "func __xl_std_time_add_months(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 7)\n"
    "end\n";

static const char xl6StdTimeAddDaysBody[] =
    "func __xl_std_time_add_days(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 5)\n"
    "end\n";

static const char xl6StdTimeAddHoursBody[] =
    "func __xl_std_time_add_hours(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 4)\n"
    "end\n";

static const char xl6StdTimeAddMinutesBody[] =
    "func __xl_std_time_add_minutes(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 3)\n"
    "end\n";

static const char xl6StdTimeAddSecondsBody[] =
    "func __xl_std_time_add_seconds(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 2)\n"
    "end\n";

static const char xl6StdTimeAddWeeksBody[] =
    "func __xl_std_time_add_weeks(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 6)\n"
    "end\n";

static const char xl6StdTimeAddQuartersBody[] =
    "func __xl_std_time_add_quarters(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 8)\n"
    "end\n";

static const char xl6StdTimeDiffYearsBody[] =
    "func __xl_std_time_diff_years(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 9)\n"
    "end\n";

static const char xl6StdTimeDiffMonthsBody[] =
    "func __xl_std_time_diff_months(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 7)\n"
    "end\n";

static const char xl6StdTimeDiffDaysBody[] =
    "func __xl_std_time_diff_days(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 5)\n"
    "end\n";

static const char xl6StdTimeDiffHoursBody[] =
    "func __xl_std_time_diff_hours(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 4)\n"
    "end\n";

static const char xl6StdTimeDiffMinutesBody[] =
    "func __xl_std_time_diff_minutes(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 3)\n"
    "end\n";

static const char xl6StdTimeDiffSecondsBody[] =
    "func __xl_std_time_diff_seconds(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 2)\n"
    "end\n";

static const char xl6StdTimeDiffWeeksBody[] =
    "func __xl_std_time_diff_weeks(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 6)\n"
    "end\n";

static const char xl6StdTimeDiffQuartersBody[] =
    "func __xl_std_time_diff_quarters(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 8)\n"
    "end\n";

static const char xl6StdTimeTimerBody[] =
    "extern \"xl6TimeTimerV1\" func __xl_std_time_timer() as float\n";

static const char xl6StdTimeSleepBody[] =
    "extern \"xl6TimeSleepV1\" func __xl_std_time_sleep(int milliseconds)\n";

static const char xl6StdTimeNowUtcBody[] =
    "extern \"xl6TimeNowV1\" func __xl_std_time_now_utc() as time\n";

static const char xl6StdTimeTimezoneOffsetBody[] =
    "extern \"xl6TimeTimezoneOffsetV1\" func __xl_time_offset_native(time value) as int\n"
    "func __xl_std_time_timezone_offset(time value) as int\n"
    "    return __xl_time_offset_native(value)\n"
    "end\n"
    "func __xl_std_time_timezone_offset() as int\n"
    "    return __xl_time_offset_native(__xl_std_time_now())\n"
    "end\n";

static const char xl6StdTimeRangeOverlapBody[] =
    "extern \"xl6TimeOverlapV1\" @pure @no_raise @nonblocking func __xl_std_time_range_overlap(time start1, time end1, time start2, time end2) as bool\n";

static const char xl6StdTimeApproxBody[] =
    "extern \"xl6TimeNearV1\" @pure @no_raise @nonblocking func __xl_std_time_approx(time left, time right, uint64 tolerance = 0) as bool\n";

static const char xl6StdTimeToUnixBody[] =
    "extern \"xl6TimeUnixV1\" @pure @no_raise @nonblocking func __xl_std_time_to_unix(time value) as int\n";

static const char xl6StdTimeFromUnixBody[] =
    "extern \"xl6TimeFromUnixV1\" func __xl_std_time_from_unix(int seconds) as time\n";

static const char xl6StdTimeFromUnixMsBody[] =
    "extern \"xl6TimeFromUnixMsV1\" func __xl_std_time_from_unix_ms(int milliseconds) as time\n";
static const char xl6StdTimeToUnixMsBody[] =
    "extern \"xl6TimeUnixMsV1\" @pure @no_raise @nonblocking func __xl_std_time_to_unix_ms(time value) as int\n";
static const char xl6StdTimeMicrosecondBody[] =
    "extern \"xl6TimeMicrosecondV1\" @pure @no_raise @nonblocking func __xl_std_time_microsecond(time value) as int\n";
static const char xl6StdTimeMonotonicBody[] =
    "extern \"xl6TimeMonotonicV1\" func __xl_std_time_monotonic() as uint64\n";
static const char xl6StdTimeSleepUsBody[] =
    "extern \"xl6TimeSleepUsV1\" func __xl_std_time_sleep_us(uint64 microseconds)\n";
static const char xl6StdTimeSleepUntilBody[] =
    "extern \"xl6TimeSleepUntilV1\" func __xl_std_time_sleep_until(uint64 deadline)\n";
static const char xl6StdTimeAddUsBody[] =
    "func __xl_std_time_add_us(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 0)\n"
    "end\n";
static const char xl6StdTimeAddMsBody[] =
    "func __xl_std_time_add_ms(time value, int amount) as time\n"
    "    return __xl_std_xrt_date_add(value, amount, 1)\n"
    "end\n";
static const char xl6StdTimeDiffUsBody[] =
    "func __xl_std_time_diff_us(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 0)\n"
    "end\n";
static const char xl6StdTimeDiffMsBody[] =
    "func __xl_std_time_diff_ms(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(left, right, 1)\n"
    "end\n";

static const char xl6StdTimeUtcToLocalBody[] =
    "func __xl_std_time_utc_to_local(time value) as DateTime\n"
    "    return DateTime(value, true)\n"
    "end\n";

static const char xl6StdTimeLocalToUtcBody[] =
    "extern \"xl6TimeFromLocalV1\" func __xl_time_local_to_utc_native(DateTime value, int fold) as time\n"
    "func __xl_std_time_local_to_utc(DateTime value, TimeFold fold = TimeFold.reject) as time\n"
    "    return __xl_time_local_to_utc_native(value, int(fold))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfMonthBody[] =
    "func __xl_std_time_first_day_of_month(time value) as time\n"
    "    auto start, finish = __xl_std_time_month_range(value)\n"
    "    return start\n"
    "end\n";

static const char xl6StdTimeLastDayOfMonthBody[] =
    "func __xl_std_time_last_day_of_month(time value) as time\n"
    "    auto start, finish = __xl_std_time_month_range(value)\n"
    "    return __xl_std_xrt_date_add(finish, -1, 5)\n"
    "end\n";

static const char xl6StdTimeFirstDayOfYearBody[] =
    "func __xl_std_time_first_day_of_year(time value) as time\n"
    "    auto start, finish = __xl_std_time_year_range(value)\n"
    "    return start\n"
    "end\n";

static const char xl6StdTimeLastDayOfYearBody[] =
    "func __xl_std_time_last_day_of_year(time value) as time\n"
    "    auto start, finish = __xl_std_time_year_range(value)\n"
    "    return __xl_std_xrt_date_add(finish, -1, 5)\n"
    "end\n";

static const char xl6StdTimeFirstDayOfWeekBody[] =
    "func __xl_std_time_first_day_of_week(time value, int startDay = 1) as time\n"
    "    auto start, finish = __xl_std_time_week_range(value, startDay)\n"
    "    return start\n"
    "end\n";

static const char xl6StdTimeLastDayOfWeekBody[] =
    "func __xl_std_time_last_day_of_week(time value, int startDay = 1) as time\n"
    "    auto start, finish = __xl_std_time_week_range(value, startDay)\n"
    "    return __xl_std_xrt_date_add(finish, -1, 5)\n"
    "end\n";

static const char xl6StdTimeWeekOfYearBody[] =
    "func __xl_std_time_week_of_year(time value) as int\n"
    "    auto year, week, weekday = __xl_std_time_iso_week(value)\n"
    "    return week\n"
    "end\n";

static const char xl6StdTimeWeekOfMonthBody[] =
    "func __xl_std_time_week_of_month(time value, int startDay = 1) as int\n"
    "    if startDay < 0 or startDay > 6 then throw RangeError \"Invalid first weekday\" end\n"
    "    auto start, finish = __xl_std_time_month_range(value)\n"
    "    int padding = (__xl_std_time_weekday(start) - startDay + 7) % 7\n"
    "    return (padding + __xl_std_time_day(value) - 1) / 7 + 1\n"
    "end\n";

static const char xl6StdTimeFormatBody[] =
    "extern \"xl6TimeFormatV1\" func __xl_std_time_format(time value, string pattern = \"%F %T\", int offset = 0, int patternSize = -1) as string\n";

static const char xl6StdTimeFormatPatternBody[] =
    "extern \"xl6TimeFormatV1\" func __xl_std_time_format_pattern(time value, string pattern, int offset = 0, int patternSize = -1) as string\n";

static const char xl6StdTimeDateStringBody[] =
    "func __xl_std_time_date_string(time value, int offset = 0) as string\n"
    "    return __xl_std_time_format(value, \"%F\", offset)\n"
    "end\n";

static const char xl6StdTimeTimeStringBody[] =
    "func __xl_std_time_time_string(time value, int offset = 0) as string\n"
    "    return __xl_std_time_format(value, \"%T\", offset)\n"
    "end\n";

static const char xl6StdTimeRFC3339Body[] =
    "extern \"xl6TimeRFC3339V1\" func __xl_std_time_rfc3339(time value, int offset = 0) as string\n";
static const char xl6StdTimeHTTPDateBody[] =
    "extern \"xl6TimeHTTPDateV1\" func __xl_std_time_http_date(time value) as string\n";
static const char xl6StdTimeParseRFC3339Body[] =
    "extern \"xl6TimeParseRFC3339V1\" func __xl_std_time_parse_rfc3339(string text, int size = -1) as time\n";
static const char xl6StdTimeTryParseRFC3339Body[] =
    "extern \"xl6TimeTryParseRFC3339V1\" func __xl_std_time_try_parse_rfc3339(string text, int size = -1) as (bool, time)\n";
static const char xl6StdTimeParseHTTPDateBody[] =
    "extern \"xl6TimeParseHTTPDateV1\" func __xl_std_time_parse_http_date(string text, int size = -1) as time\n";
static const char xl6StdTimeTryParseHTTPDateBody[] =
    "extern \"xl6TimeTryParseHTTPDateV1\" func __xl_std_time_try_parse_http_date(string text, int size = -1) as (bool, time)\n";

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
    "    return __xl_std_xrt_rand_range(min, max)\n"
    "end\n";

static const char xl6StdRandomRangeClosedBody[] =
    "func __xl_std_random_range_closed(int min, int max) as int\n"
    "    return __xl_std_xrt_rand_range_closed(min, max)\n"
    "end\n";

static const char xl6StdRandomBelowBody[] =
    "func __xl_std_random_below(uint64 bound) as uint64\n"
    "    return __xl_std_xrt_rand_below(bound)\n"
    "end\n";

static const char xl6StdRandomRealBody[] =
    "func __xl_std_random_real() as float\n"
    "    return __xl_std_xrt_rand_real()\n"
    "end\n";

static const char xl6StdRandomBytesBody[] =
    "func __xl_std_random_bytes(int count) as bytes\n"
    "    if count < 0 then throw RangeError \"random byte count is invalid\" end\n"
    "    return __xl_std_xrt_rand_bytes(count)\n"
    "end\n";

static const char xl6StdRandomFillBytesBody[] =
    "func __xl_std_random_fill_bytes(Buffer target)\n"
    "    if not __xl_std_xrt_rand_fill_buffer(target.nativeHandle()) then throw RuntimeError \"random buffer fill failed\" end\n"
    "end\n";

static const char xl6StdRandomShuffleBody[] =
    "func __xl_std_random_shuffle(array values)\n"
    "    if not __xl_std_xrt_rand_shuffle_array(values) then throw RuntimeError \"random array shuffle failed\" end\n"
    "end\n";

static const char xl6StdRandomShuffleBufferBody[] =
    "func __xl_std_random_shuffle_buffer(Buffer target)\n"
    "    if not __xl_std_xrt_rand_shuffle_buffer(target.nativeHandle()) then throw RuntimeError \"random buffer shuffle failed\" end\n"
    "end\n";

static const char xl6StdRandomSeedBody[] =
    "func __xl_std_random_seed(uint64 seed, uint64 seq = 1)\n"
    "    __xl_std_xrt_rand_seed_thread(seed, seq)\n"
    "end\n";

static const char xl6StdRandomStringBody[] =
    "func __xl_std_random_string(int length = 16, string alphabet = \"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_\", int alphabetSize = -1) as string\n"
    "    if length < 0 or alphabetSize < -1 then throw RangeError \"random string length is invalid\" end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_rand_str(alphabet, __xl_std_xrt_string_byte_len(alphabet), alphabetSize, length))\n"
    "end\n";

static const char xl6StdPathExistsBody[] =
    "extern \"xl6_xrt_fs_path_exists\" func __xl_fs_path_exists(string p0) as bool\n"
    "func __xl_std_path_exists(string path) as bool\n"
    "    return __xl_fs_path_exists(path)\n"
    "end\n";

static const char xl6StdPathIsAbsBody[] =
    "extern \"xl6_xrt_fs_path_is_abs\" func __xl_fs_path_is_abs(string path) as bool\n"
    "func __xl_std_path_is_abs(string path) as bool\n"
    "    return __xl_fs_path_is_abs(path)\n"
    "end\n";

static const char xl6StdPathNameExtBody[] =
    "extern \"xl6_xrt_fs_path_name\" func __xl_fs_path_name_ext(string path) as string\n"
    "func __xl_std_path_name_ext(string path) as string\n"
    "    return __xl_fs_path_name_ext(path)\n"
    "end\n";

static const char xl6StdPathNameBody[] =
    "extern \"xl6_xrt_fs_path_name\" func __xl_fs_path_name(string path) as string\n"
    "func __xl_std_path_name(string path) as string\n"
    "    return __xl_fs_path_name(path)\n"
    "end\n";

static const char xl6StdPathExtBody[] =
    "extern \"xl6_xrt_fs_path_ext\" func __xl_fs_path_ext(string path) as string\n"
    "func __xl_std_path_ext(string path) as string\n"
    "    return __xl_fs_path_ext(path)\n"
    "end\n";

static const char xl6StdPathDirBody[] =
    "extern \"xl6_xrt_fs_path_parent\" func __xl_fs_path_dir(string path) as string\n"
    "func __xl_std_path_dir(string path) as string\n"
    "    return __xl_fs_path_dir(path)\n"
    "end\n";

static const char xl6StdPathJoinBody[] =
    "extern \"xl6_xrt_fs_path_build\" func __xl_fs_path_join_parts(array parts, int style) as string\n"
    "func __xl_std_path_join(string first, string second, ...) as string\n"
    "    array parts = [first, second]\n"
    "    for item in arg\n"
    "        parts.append(item)\n"
    "    end\n"
    "    return __xl_fs_path_join_parts(parts, 0)\n"
    "end\n";

static const char xl6StdPathRandomBody[] =
    "func __xl_std_path_random(string head = \"\", string foot = \"\", int len = 12) as string\n"
    "    return head + __xl_std_random_string(len) + foot\n"
    "end\n";

static const char xl6StdPathSepBody[] =
    "extern \"xl6_xrt_fs_path_sep\" func __xl_fs_path_sep() as string\n"
    "func __xl_std_path_sep() as string\n"
    "    return __xl_fs_path_sep()\n"
    "end\n";

static const char xl6StdPathNormalizeBody[] =
    "extern \"xl6_xrt_fs_path_clean\" func __xl_fs_path_normalize(string path, int style) as string\n"
    "func __xl_std_path_normalize(string path, int style = 0) as string\n"
    "    return __xl_fs_path_normalize(path, style)\n"
    "end\n";

static const char xl6StdPathAbsBody[] =
    "extern \"xl6_xrt_fs_path_abs\" func __xl_fs_path_abs(string path) as string\n"
    "func __xl_std_path_abs(string path) as string\n"
    "    return __xl_fs_path_abs(path)\n"
    "end\n";

static const char xl6StdPathRelBody[] =
    "extern \"xl6_xrt_fs_path_rel\" func __xl_fs_path_rel(string base, string target) as string\n"
    "func __xl_std_path_rel(string base, string target) as string\n"
    "    return __xl_fs_path_rel(base, target)\n"
    "end\n";

static const char xl6StdPathCwdBody[] =
    "extern \"xl6_xrt_fs_path_cwd\" func __xl_fs_path_cwd() as string\n"
    "func __xl_std_path_cwd() as string\n"
    "    return __xl_fs_path_cwd()\n"
    "end\n";

static const char xl6StdPathSetCwdBody[] =
    "extern \"xl6_xrt_fs_path_set_cwd\" func __xl_fs_path_set_cwd(string path) as bool\n"
    "func __xl_std_path_set_cwd(string path) as bool\n"
    "    return __xl_fs_path_set_cwd(path)\n"
    "end\n";

static const char xl6StdOsEnvBody[] =
    "func __xl_std_os_env(string name, string defaultValue = \"\", int size = -1) as string\n"
    "    return __xl_std_runtime_env_get(name, size) ?? defaultValue\n"
    "end\n";

static const char xl6StdEnvGetBody[] =
    "extern \"xl6EnvGetV1\" func __xl_std_runtime_env_get(string name, int size = -1) as string?\n";
static const char xl6StdEnvHasBody[] =
    "extern \"xl6EnvHasV1\" func __xl_std_runtime_env_has(string name, int size = -1) as bool\n";
static const char xl6StdEnvSetBody[] =
    "extern \"xl6EnvSetV1\" func __xl_std_runtime_env_set(string name, string value, int nameSize = -1, int valueSize = -1)\n";
static const char xl6StdEnvRemoveBody[] =
    "extern \"xl6EnvRemoveV1\" func __xl_std_runtime_env_remove(string name, int size = -1)\n";

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
    "func __xl_std_os_exec(string command, int timeoutMs = -1, int size = -1) as string\n"
    "    return __xl_std_process_exec(command, timeoutMs, size)\n"
    "end\n";

static const char xl6StdOsRunBody[] =
    "func __xl_std_os_run(string command, int timeoutMs = -1, int size = -1) as ProcessResult\n"
    "    return __xl_std_process_run(command, timeoutMs, size)\n"
    "end\n";

static const char xl6StdOsStartBody[] =
    "func __xl_std_os_start(string command, int size = -1) as Process\n"
    "    return __xl_std_process_start(command, size)\n"
    "end\n";

static const char xl6StdOsStartFileBody[] =
    "func __xl_std_os_start_file(string program, array<string> args = [], int size = -1, int argSize = -1) as Process\n"
    "    return __xl_std_process_start_file(program, args, size, argSize)\n"
    "end\n";

static const char xl6StdOsChainBody[] =
    "func __xl_std_os_chain(string command) as int\n"
    "    return process.exitCode(command)\n"
    "end\n";

static const char xl6StdPathHomeBody[] =
    "extern \"xl6_xrt_fs_path_home\" func __xl_fs_path_home() as string\n"
    "func __xl_std_path_home() as string\n"
    "    return __xl_fs_path_home()\n"
    "end\n";

static const char xl6StdPathTempBody[] =
    "extern \"xl6_xrt_fs_path_temp\" func __xl_fs_path_temp() as string\n"
    "func __xl_std_path_temp() as string\n"
    "    return __xl_fs_path_temp()\n"
    "end\n";

static const char xl6StdPathAppDirBody[] =
    "extern \"xl6_xrt_fs_path_app_dir\" func __xl_fs_path_app_dir() as string\n"
    "func __xl_std_path_app_dir() as string\n"
    "    return __xl_fs_path_app_dir()\n"
    "end\n";

static const char xl6StdPathWithExtBody[] =
    "extern \"xl6_xrt_fs_path_with_ext\" func __xl_fs_path_with_ext(string path, string extension) as string\n"
    "func __xl_std_path_with_ext(string path, string extension) as string\n"
    "    return __xl_fs_path_with_ext(path, extension)\n"
    "end\n";

static const char xl6StdPathWithNameBody[] =
    "extern \"xl6_xrt_fs_path_with_name\" func __xl_fs_path_with_name(string path, string name) as string\n"
    "func __xl_std_path_with_name(string path, string name) as string\n"
    "    return __xl_fs_path_with_name(path, name)\n"
    "end\n";

static const char xl6StdFileExistsBody[] =
    "extern \"xl6_xrt_fs_file_exists\" func __xl_fs_file_exists(string p0) as bool\n"
    "func __xl_std_file_exists(string path) as bool\n"
    "    return __xl_fs_file_exists(path)\n"
    "end\n";

static const char xl6StdFileSizeBody[] =
    "extern \"xl6_xrt_fs_path_size\" func __xl_fs_file_size(string p0) as uint64\n"
    "func __xl_std_file_size(string path) as uint64\n"
    "    return __xl_fs_file_size(path)\n"
    "end\n";

static const char xl6StdFileSetSizeBody[] =
    "extern \"xl6_xrt_fs_path_set_size\" func __xl_fs_file_set_size(string p0, uint64 p1) as bool\n"
    "func __xl_std_file_set_size(string path, uint64 size) as bool\n"
    "    return __xl_fs_file_set_size(path, size)\n"
    "end\n";

static const char xl6StdFileAttrBody[] =
    "extern \"xl6_xrt_fs_path_attributes\" func __xl_fs_file_attr(string p0) as int\n"
    "func __xl_std_file_attr(string path) as int\n"
    "    return __xl_fs_file_attr(path)\n"
    "end\n";

static const char xl6StdFileSetAttrBody[] =
    "extern \"xl6_xrt_fs_path_set_attributes\" func __xl_fs_file_set_attr(string p0, int p1) as bool\n"
    "func __xl_std_file_set_attr(string path, int attributes) as bool\n"
    "    return __xl_fs_file_set_attr(path, attributes)\n"
    "end\n";

static const char xl6StdFileAccessTimeBody[] =
    "func __xl_std_file_access_time(string path) as time\n"
    "    auto metadata = __xl_std_file_stat(path)\n"
    "    if (metadata.available & 4) == 0 then throw FileError(\"file access time metadata is unavailable\") end\n"
    "    return time(metadata.accessed)\n"
    "end\n";

static const char xl6StdFileChangeTimeBody[] =
    "func __xl_std_file_change_time(string path) as time\n"
    "    auto metadata = __xl_std_file_stat(path)\n"
    "    if (metadata.available & 8) == 0 then throw FileError(\"file modification time metadata is unavailable\") end\n"
    "    return time(metadata.modified)\n"
    "end\n";

static const char xl6StdFileWriteTextBody[] =
    "extern \"xl6_xrt_fs_text_write\" func __xl_fs_file_write_text(string p0, string p1, int p2, int p3, bool p4, int p5, bool p6) as bool\n"
    "func __xl_std_file_write_text(string path, string text, int encoding = 1, int policy = 0, bool bom = false, int size = -1) as bool\n"
    "    return __xl_fs_file_write_text(path, text, encoding, policy, bom, size, false)\n"
    "end\n";

static const char xl6StdFileAppendTextBody[] =
    "extern \"xl6_xrt_fs_text_append\" func __xl_fs_file_append_text(string p0, string p1, int p2, int p3, int p4) as bool\n"
    "func __xl_std_file_append_text(string path, string text, int encoding = 1, int policy = 0, int size = -1) as bool\n"
    "    return __xl_fs_file_append_text(path, text, encoding, policy, size)\n"
    "end\n";

static const char xl6StdFileReadTextBody[] =
    "extern \"xl6_xrt_fs_text_read\" func __xl_fs_file_read_text(string p0, int p1, int p2, int p3) as string\n"
    "func __xl_std_file_read_text(string path, int encoding = 0, int policy = 0, int maxBytes = 67108864) as string\n"
    "    return __xl_fs_file_read_text(path, encoding, policy, maxBytes)\n"
    "end\n";

static const char xl6StdTaskGroupBody[] =
    "func __xl_std_task_group(int limit = 0, int cancelOn = 0) as TaskGroup\n"
    "    return TaskGroup(limit, cancelOn)\n"
    "end\n";

static const char xl6StdFileReadBytesBody[] =
    "extern \"xl6_xrt_fs_whole_read\" func __xl_fs_file_read_bytes(string p0, int p1) as bytes\n"
    "func __xl_std_file_read_bytes(string path, int maxBytes = 67108864) as bytes\n"
    "    return __xl_fs_file_read_bytes(path, maxBytes)\n"
    "end\n";

static const char xl6StdFileWriteBytesBody[] =
    "extern \"xl6_xrt_fs_whole_write\" func __xl_fs_file_write_bytes(string p0, bytes p1, int p2, int p3) as bool\n"
    "func __xl_std_file_write_bytes(string path, bytes data, int size = -1) as bool\n"
    "    return __xl_fs_file_write_bytes(path, data, size, 0)\n"
    "end\n";

static const char xl6StdFileAppendBytesBody[] =
    "extern \"xl6_xrt_fs_whole_write\" func __xl_fs_file_append_bytes(string p0, bytes p1, int p2, int p3) as bool\n"
    "func __xl_std_file_append_bytes(string path, bytes data, int size = -1) as bool\n"
    "    return __xl_fs_file_append_bytes(path, data, size, 1)\n"
    "end\n";

static const char xl6StdFileCopyBody[] =
    "extern \"xl6_xrt_fs_file_copy\" func __xl_fs_file_copy(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_file_copy(string source, string target, bool replace = true) as bool\n"
    "    return __xl_fs_file_copy(source, target, replace)\n"
    "end\n";

static const char xl6StdFileMoveBody[] =
    "extern \"xl6_xrt_fs_file_move\" func __xl_fs_file_move(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_file_move(string source, string target, bool replace = true) as bool\n"
    "    return __xl_fs_file_move(source, target, replace)\n"
    "end\n";

static const char xl6StdFileDeleteBody[] =
    "extern \"xl6_xrt_fs_file_delete\" func __xl_fs_file_delete(string p0) as bool\n"
    "func __xl_std_file_delete(string path) as bool\n"
    "    return __xl_fs_file_delete(path)\n"
    "end\n";

static const char xl6StdFileTouchBody[] =
    "extern \"xl6_xrt_fs_file_touch\" func __xl_fs_file_touch(string p0) as bool\n"
    "func __xl_std_file_touch(string path) as bool\n"
    "    return __xl_fs_file_touch(path)\n"
    "end\n";

static const char xl6StdFileRenameBody[] =
    "extern \"xl6_xrt_fs_file_rename\" func __xl_fs_file_rename(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_file_rename(string source, string target, bool replace = false) as bool\n"
    "    return __xl_fs_file_rename(source, target, replace)\n"
    "end\n";

static const char xl6StdFileReplaceBody[] =
    "extern \"xl6_xrt_fs_file_rename\" func __xl_fs_file_replace(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_file_replace(string source, string target) as bool\n"
    "    return __xl_fs_file_replace(source, target, true)\n"
    "end\n";

static const char xl6StdFileWriteTextAtomicBody[] =
    "extern \"xl6_xrt_fs_text_write\" func __xl_fs_file_write_text_atomic(string p0, string p1, int p2, int p3, bool p4, int p5, bool p6) as bool\n"
    "func __xl_std_file_write_text_atomic(string path, string text, int encoding = 1, int policy = 0, bool bom = false, int size = -1) as bool\n"
    "    return __xl_fs_file_write_text_atomic(path, text, encoding, policy, bom, size, true)\n"
    "end\n";

static const char xl6StdFileInfoBody[] =
    "func __xl_std_file_info(string path, bool follow = true) as FileStat\n"
    "    return FileStat(path,follow)\n"
    "end\n";

static const char xl6StdFileOpenBody[] =
    "func __xl_std_file_open(string filePath, int flags = 1, int mode = 438, int share = 7) as File\n"
    "    return File(filePath, flags, mode, share)\n"
    "end\n";


static const char xl6StdDirExistsBody[] =
    "extern \"xl6_xrt_fs_dir_exists\" func __xl_fs_dir_exists(string p0) as bool\n"
    "func __xl_std_dir_exists(string path) as bool\n"
    "    return __xl_fs_dir_exists(path)\n"
    "end\n";

static const char xl6StdDirCreateBody[] =
    "extern \"xl6_xrt_fs_dir_create\" func __xl_fs_dir_create(string p0, int p1, bool p2) as bool\n"
    "func __xl_std_dir_create(string path, int mode = 511) as bool\n"
    "    return __xl_fs_dir_create(path, mode, false)\n"
    "end\n";

static const char xl6StdDirCreateAllBody[] =
    "extern \"xl6_xrt_fs_dir_create\" func __xl_fs_dir_create_all(string p0, int p1, bool p2) as bool\n"
    "func __xl_std_dir_create_all(string path, int mode = 511) as bool\n"
    "    return __xl_fs_dir_create_all(path, mode, true)\n"
    "end\n";

static const char xl6StdDirCopyBody[] =
    "extern \"xl6_xrt_fs_dir_copy\" func __xl_fs_dir_copy(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_dir_copy(string source, string target, bool replace = true) as bool\n"
    "    return __xl_fs_dir_copy(source, target, replace)\n"
    "end\n";

static const char xl6StdDirMoveBody[] =
    "extern \"xl6_xrt_fs_dir_move\" func __xl_fs_dir_move(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_dir_move(string source, string target, bool replace = true) as bool\n"
    "    return __xl_fs_dir_move(source, target, replace)\n"
    "end\n";

static const char xl6StdDirDeleteBody[] =
    "extern \"xl6_xrt_fs_dir_remove_all\" func __xl_fs_dir_delete(string path, bool keepRoot) as bool\n"
    "func __xl_std_dir_delete(string path) as bool\n"
    "    return __xl_fs_dir_delete(path,false)\n"
    "end\n";

static const char xl6StdDirListBody[] =
    "extern \"xl6_xrt_fs_dir_list\" func __xl_fs_dir_list(string path, bool recursive, int kind, int maxEntries) as array<string>\n"
    "func __xl_std_dir_list(string path, bool recursive = false, int maxEntries = 1048576) as array<string>\n"
    "    return __xl_fs_dir_list(path,recursive,0,maxEntries)\n"
    "end\n";

static const char xl6StdDirFilesBody[] =
    "extern \"xl6_xrt_fs_dir_list\" func __xl_fs_dir_files(string path, bool recursive, int kind, int maxEntries) as array<string>\n"
    "func __xl_std_dir_files(string path, bool recursive = false, int maxEntries = 1048576) as array<string>\n"
    "    return __xl_fs_dir_files(path,recursive,1,maxEntries)\n"
    "end\n";

static const char xl6StdDirDirsBody[] =
    "extern \"xl6_xrt_fs_dir_list\" func __xl_fs_dir_dirs(string path, bool recursive, int kind, int maxEntries) as array<string>\n"
    "func __xl_std_dir_dirs(string path, bool recursive = false, int maxEntries = 1048576) as array<string>\n"
    "    return __xl_fs_dir_dirs(path,recursive,2,maxEntries)\n"
    "end\n";

static const char xl6StdDirEmptyBody[] =
    "extern \"xl6_xrt_fs_dir_empty\" func __xl_fs_dir_empty(string p0) as bool\n"
    "func __xl_std_dir_empty(string path) as bool\n"
    "    return __xl_fs_dir_empty(path)\n"
    "end\n";

static const char xl6StdDirCountBody[] =
    "func __xl_std_dir_count(string path, bool recursive = false) as uint64\n"
    "    auto stats = WalkStats(path,recursive)\n"
    "    return stats.items - uint64(1)\n"
    "end\n";

static const char xl6StdDirSizeBody[] =
    "extern \"xl6_xrt_fs_dir_size\" func __xl_fs_dir_size(string path, bool recursive) as uint64\n"
    "func __xl_std_dir_size(string path, bool recursive = true) as uint64\n"
    "    return __xl_fs_dir_size(path,recursive)\n"
    "end\n";

static const char xl6StdDirCleanBody[] =
    "extern \"xl6_xrt_fs_dir_remove_all\" func __xl_fs_dir_clean(string path, bool keepRoot) as bool\n"
    "func __xl_std_dir_clean(string path) as bool\n"
    "    return __xl_fs_dir_clean(path,true)\n"
    "end\n";

static const char xl6StdDirEnsureEmptyBody[] =
    "extern \"xl6_xrt_fs_dir_ensure_empty\" func __xl_fs_dir_ensure_empty(string path) as bool\n"
    "func __xl_std_dir_ensure_empty(string path) as bool\n"
    "    return __xl_fs_dir_ensure_empty(path)\n"
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

static const char xl6StdXlonWriterBody[] =
    "func __xl_std_xlon_writer(bool pretty = false) as XlonWriter\n"
    "    return XlonWriter(pretty)\n"
    "end\n";

static const char xl6StdXlonWriterOptionsBody[] =
    "func __xl_std_xlon_writer(XlonOptions options) as XlonWriter\n"
    "    return XlonWriter(options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdJsonVisitBody[] =
    "func __xl_std_json_visit(string text, func(var) as bool visitor) as int\n"
    "    return int(__xl_std_runtime_json_visit(text, visitor))\n"
    "end\n";

static const char xl6StdJsonlStringifyBody[] =
    "func __xl_std_jsonl_stringify(var records) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_jsonl_stringify(records))\n"
    "end\n";

static const char xl6StdXlonlStringifyBody[] =
    "func __xl_std_xlonl_stringify(var records) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_xlonl_stringify(records))\n"
    "end\n";

#define XL6_CONFIG_STRINGIFY_BODY(Format) \
    "extern \"xl6_xrt_" Format "_stringify_configured\" func __xl_std_xrt_" Format "_stringify_configured(var value, var options) as point\n" \
    "func __xl_std_" Format "_stringify(var value, var options) as string\n" \
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_" Format "_stringify_configured(value, options))\n" \
    "end\n"
static const char xl6StdJsonStringifyConfiguredBody[] = XL6_CONFIG_STRINGIFY_BODY("json");
static const char xl6StdXlonStringifyConfiguredBody[] = XL6_CONFIG_STRINGIFY_BODY("xlon");
static const char xl6StdJsonlStringifyConfiguredBody[] = XL6_CONFIG_STRINGIFY_BODY("jsonl");
static const char xl6StdXlonlStringifyConfiguredBody[] = XL6_CONFIG_STRINGIFY_BODY("xlonl");
#undef XL6_CONFIG_STRINGIFY_BODY

#define XL6_CONFIG_WRITE_TO_BODY(Format) \
    "extern \"xl6_xrt_" Format "_write_to_configured\" func __xl_std_xrt_" Format "_write_to_configured(var value, func(var) as bool sink, var options) as c_int\n" \
    "func __xl_std_" Format "_write_to(var value, func(var) as bool sink, var options = null) as bool\n" \
    "    return __xl_std_xrt_" Format "_write_to_configured(value, sink, options) != 0\n" \
    "end\n"
static const char xl6StdJsonWriteToBody[] = XL6_CONFIG_WRITE_TO_BODY("json");
static const char xl6StdXlonWriteToBody[] = XL6_CONFIG_WRITE_TO_BODY("xlon");
static const char xl6StdJsonlWriteToBody[] = XL6_CONFIG_WRITE_TO_BODY("jsonl");
static const char xl6StdXlonlWriteToBody[] = XL6_CONFIG_WRITE_TO_BODY("xlonl");
#undef XL6_CONFIG_WRITE_TO_BODY

static const char xl6StdJsonQuoteBody[] =
    "extern \"xl6_xrt_json_quote_configured\" func __xl_std_xrt_json_quote_configured(string text, var options) as point\n"
    "func __xl_std_json_quote(string text, var options = null) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_json_quote_configured(text, options))\n"
    "end\n";
static const char xl6StdJsonQuoteToBody[] =
    "extern \"xl6_xrt_json_quote_to_configured\" func __xl_std_xrt_json_quote_to_configured(string text, func(var) as bool sink, var options) as c_int\n"
    "func __xl_std_json_quote_to(string text, func(var) as bool sink, var options = null) as bool\n"
    "    return __xl_std_xrt_json_quote_to_configured(text, sink, options) != 0\n"
    "end\n";
static const char xl6StdJsonVisitConfiguredBody[] =
    "extern \"xl6_xrt_json_visit_configured\" func __xl_std_xrt_json_visit_configured(string text, func(var) as bool visitor, var options) as c_int\n"
    "func __xl_std_json_visit(string text, func(var) as bool visitor, var options) as int\n"
    "    return int(__xl_std_xrt_json_visit_configured(text, visitor, options))\n"
    "end\n";
static const char xl6StdXlonVisitConfiguredBody[] =
    "extern \"xl6_xrt_xlon_visit_configured\" func __xl_std_xrt_xlon_visit_configured(string text, func(var) as bool visitor, var options) as c_int\n"
    "func __xl_std_xlon_visit(string text, func(var) as bool visitor, var options) as int\n"
    "    return int(__xl_std_xrt_xlon_visit_configured(text, visitor, options))\n"
    "end\n";

#define XL6_CONFIG_VALID_BODY(Format) \
    "extern \"xl6_xrt_" Format "_valid_configured\" func __xl_std_xrt_" Format "_valid_configured(string text, var options) as c_bool\n" \
    "func __xl_std_" Format "_valid(string text, var options) as bool\n" \
    "    return __xl_std_xrt_" Format "_valid_configured(text, options)\n" \
    "end\n"
static const char xl6StdJsonValidConfiguredBody[] = XL6_CONFIG_VALID_BODY("json");
static const char xl6StdXlonValidConfiguredBody[] = XL6_CONFIG_VALID_BODY("xlon");
static const char xl6StdJsonlValidConfiguredBody[] = XL6_CONFIG_VALID_BODY("jsonl");
static const char xl6StdXlonlValidConfiguredBody[] = XL6_CONFIG_VALID_BODY("xlonl");
#undef XL6_CONFIG_VALID_BODY

static const char xl6StdXlonParseWithBody[] =
    "extern \"xl6_xrt_xlon_decode_configured\" func __xl_std_xrt_xlon_decode_configured(string text, func(var) as var decoder, var options) as var\n"
    "func __xl_std_xlon_parse_with(string text, func(var) as var decoder, var options = null) as var\n"
    "    return __xl_std_xrt_xlon_decode_configured(text, decoder, options)\n"
    "end\n";

static const char xl6StdDictRemoveBody[] =
    "func __xl_std_dict_remove(var table, string key) as bool\n"
    "    return __xl_std_xrt_table_remove(table, key)\n"
    "end\n";

static const char xl6StdXlonParseBody[] =
    "func __xl_std_xlon_parse(string text) as var\n"
    "    return __xl_std_runtime_xlon_parse_options(text, false, false, false)\n"
    "end\n";

static const char xl6StdXlonParseOptionsBody[] =
    "func __xl_std_xlon_parse(string text, XlonOptions options) as var\n"
    "    return __xl_std_runtime_xlon_parse_options(text, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXlonValidBody[] =
    "func __xl_std_xlon_valid(string text) as bool\n"
    "    return __xl_std_runtime_xlon_valid_options(text, false, false, false)\n"
    "end\n";

static const char xl6StdXlonValidOptionsBody[] =
    "func __xl_std_xlon_valid(string text, XlonOptions options) as bool\n"
    "    return __xl_std_runtime_xlon_valid_options(text, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXlonReadBody[] =
    "func __xl_std_xlon_read(string path) as var\n"
    "    return __xl_std_runtime_xlon_read_options(path, false, false, false)\n"
    "end\n";

static const char xl6StdXlonReadOptionsBody[] =
    "func __xl_std_xlon_read(string path, XlonOptions options) as var\n"
    "    return __xl_std_runtime_xlon_read_options(path, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXlonStringifyBody[] =
    "func __xl_std_xlon_stringify(var value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_xlon_stringify_options(value, false, false, false))\n"
    "end\n";

static const char xl6StdXlonStringifyOptionsBody[] =
    /* A language facade must keep T until PROJECT has selected its schema.
     * Encoding options map to the same configured native contract; decode
     * policy is deliberately not forwarded to the writer. */
    "func __xl_std_xlon_stringify<T>(T value, XlonOptions options) as string\n"
    "    int unsupported = 0\n"
    "    if options.ignoreUnsupportedEncode\n"
    "        unsupported = 1\n"
    "    end\n"
    "    return __xl_std_xlon_stringify(value, {\"pretty\": options.pretty, \"unsupported\": unsupported})\n"
    "end\n";

static const char xl6StdXlonWriteBody[] =
    "func __xl_std_xlon_write(string path, var value) as bool\n"
    "    return __xl_std_runtime_xlon_write_options(path, value, false, false, false)\n"
    "end\n";

static const char xl6StdXlonWriteOptionsBody[] =
    "func __xl_std_xlon_write<T>(string path, T value, XlonOptions options) as bool\n"
    "    int unsupported = 0\n"
    "    if options.ignoreUnsupportedEncode\n"
    "        unsupported = 1\n"
    "    end\n"
    "    return __xl_std_xlon_write(path, value, {\"pretty\": options.pretty, \"unsupported\": unsupported})\n"
    "end\n";

static const char xl6StdXlonVisitBody[] =
    "func __xl_std_xlon_visit(string text, func(var) as bool visitor) as int\n"
    "    return __xl_std_runtime_xlon_visit_options(text, visitor, false, false, false)\n"
    "end\n";

static const char xl6StdXlonVisitOptionsBody[] =
    "func __xl_std_xlon_visit(string text, func(var) as bool visitor, XlonOptions options) as int\n"
    "    return __xl_std_runtime_xlon_visit_options(text, visitor, options.pretty, options.ignoreUnsupportedEncode, options.ignoreUnsupportedDecode)\n"
    "end\n";

static const char xl6StdXlonOptionsBody[] =
    "func __xl_std_xlon_options(bool pretty = false, bool ignoreUnsupportedEncode = false, bool ignoreUnsupportedDecode = false) as XlonOptions\n"
    "    return XlonOptions(pretty, ignoreUnsupportedEncode, ignoreUnsupportedDecode)\n"
    "end\n";

#include "xl6_stdlib_codec.inc"


static const XL6_StdlibParam xl6StdCrc32TextParams[] = {{"uint64","previous"},{"string","data"},{"int","size"}};
static const XL6_StdlibParam xl6StdCrc32BytesParams[] = {{"uint64","previous"},{"bytes","data"},{"int","size"}};
static const XL6_StdlibParam xl6StdCrc32BufferParams[] = {{"uint64","previous"},{"Buffer","data"},{"int","size"}};
static const char xl6StdCrc32TextBody[] =
    "func __xl_std_crc32_checksum(string data, int size = -1) as uint32\n"
    "    return __xl_crc32_text(uint64(0), data, size)\nend\n";
static const char xl6StdCrc32BytesBody[] =
    "func __xl_std_crc32_checksum(bytes data, int size = -1) as uint32\n"
    "    return __xl_crc32_bytes(uint64(0), data, size)\nend\n";
static const char xl6StdCrc32BufferBody[] =
    "func __xl_std_crc32_checksum_buffer(Buffer data, int size = -1) as uint32\n"
    "    return __xl_crc32_buffer(uint64(0), data, size)\nend\n";
static const char xl6StdCrc32UpdateTextBody[] =
    "func __xl_std_crc32_update(uint32 previous, string data, int size = -1) as uint32\n"
    "    return __xl_crc32_text(uint64(previous), data, size)\nend\n";
static const char xl6StdCrc32UpdateBytesBody[] =
    "func __xl_std_crc32_update(uint32 previous, bytes data, int size = -1) as uint32\n"
    "    return __xl_crc32_bytes(uint64(previous), data, size)\nend\n";
static const char xl6StdCrc32UpdateBufferBody[] =
    "func __xl_std_crc32_update_buffer(uint32 previous, Buffer data, int size = -1) as uint32\n"
    "    return __xl_crc32_buffer(uint64(previous), data, size)\nend\n";

static const char xl6StdHashU32Body[] =
    "func __xl_std_hash_u32(string data, int size = -1) as uint32\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash32(data, size)\n"
    "end\n";

static const char xl6StdHashU32WithSeedBody[] =
    "func __xl_std_hash_u32_with_seed(string data, uint32 seed, int size = -1) as uint32\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash32_seed(data, c_uint(seed), size)\n"
    "end\n";

static const char xl6StdHashU64Body[] =
    "func __xl_std_hash_u64(string data, int size = -1) as uint64\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash64(data, size)\n"
    "end\n";

static const char xl6StdHashU64WithSeedBody[] =
    "func __xl_std_hash_u64_with_seed(string data, uint64 seed, int size = -1) as uint64\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash64_seed(data, c_ulonglong(seed), size)\n"
    "end\n";

static const char xl6StdHashU32BytesBody[] =
    "func __xl_std_hash_u32(bytes data, int size = -1) as uint32\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash32_bytes(data, size)\n"
    "end\n";
static const char xl6StdHashU32BytesWithSeedBody[] =
    "func __xl_std_hash_u32_with_seed(bytes data, uint32 seed, int size = -1) as uint32\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash32_bytes_seed(data, c_uint(seed), size)\n"
    "end\n";
static const char xl6StdHashU64BytesBody[] =
    "func __xl_std_hash_u64(bytes data, int size = -1) as uint64\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash64_bytes(data, size)\n"
    "end\n";
static const char xl6StdHashU64BytesWithSeedBody[] =
    "func __xl_std_hash_u64_with_seed(bytes data, uint64 seed, int size = -1) as uint64\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash64_bytes_seed(data, c_ulonglong(seed), size)\n"
    "end\n";

static const char xl6StdHashU32BufferBody[] =
    "func __xl_std_hash_u32_buffer(Buffer data, int size = -1) as uint32\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash32_raw(data.data(), c_size(data.len()), size)\n"
    "end\n";
static const char xl6StdHashU32BufferWithSeedBody[] =
    "func __xl_std_hash_u32_buffer_with_seed(Buffer data, uint32 seed, int size = -1) as uint32\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash32_raw_seed(data.data(), c_size(data.len()), c_uint(seed), size)\n"
    "end\n";
static const char xl6StdHashU64BufferBody[] =
    "func __xl_std_hash_u64_buffer(Buffer data, int size = -1) as uint64\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash64_raw(data.data(), c_size(data.len()), size)\n"
    "end\n";
static const char xl6StdHashU64BufferWithSeedBody[] =
    "func __xl_std_hash_u64_buffer_with_seed(Buffer data, uint64 seed, int size = -1) as uint64\n"
    "    if size < -1 then throw RangeError \"hash size is invalid\" end\n"
    "    return __xl_std_xrt_hash64_raw_seed(data.data(), c_size(data.len()), c_ulonglong(seed), size)\n"
    "end\n";

static const char xl6StdSipHash64StringBody[] =
    "func __xl_std_siphash64(string data, bytes key, int size = -1) as uint64\n"
    "    if key.length != 16 then throw ValueError \"SipHash key must be 16 bytes\" end\n"
    "    if size < -1 then throw RangeError \"siphash size is invalid\" end\n"
    "    return uint64(__xl_std_xrt_siphash64_string(data, key, size))\n"
    "end\n";
static const char xl6StdSipHash64BytesBody[] =
    "func __xl_std_siphash64(bytes data, bytes key, int size = -1) as uint64\n"
    "    if key.length != 16 then throw ValueError \"SipHash key must be 16 bytes\" end\n"
    "    if size < -1 then throw RangeError \"siphash size is invalid\" end\n"
    "    return uint64(__xl_std_xrt_siphash64_bytes(data, key, size))\n"
    "end\n";
static const char xl6StdSipHash64BufferBody[] =
    "func __xl_std_siphash64_buffer(Buffer data, bytes key, int size = -1) as uint64\n"
    "    if key.length != 16 then throw ValueError \"SipHash key must be 16 bytes\" end\n"
    "    if size < -1 then throw RangeError \"siphash size is invalid\" end\n"
    "    return uint64(__xl_std_xrt_siphash64_raw(data.data(), c_size(data.len()), key, size))\n"
    "end\n";
static const char xl6StdSipHasher64Body[] =
    "func __xl_std_siphasher64(bytes key) as SipHasher64\n"
    "    if key.length != 16 then throw ValueError \"SipHash key must be 16 bytes\" end\n"
    "    return SipHasher64(key)\n"
    "end\n";

static const char xl6StdCryptoRandomHexBody[] =
    "func __xl_std_crypto_random_hex(int byteCount) as string\n"
    "    if byteCount < 0 then throw RangeError \"secure random byte count is invalid\" end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_secure_hex(byteCount))\n"
    "end\n";

static const char xl6StdCryptoRandomStringBody[] =
    "func __xl_std_crypto_random_string(int length = 22, string alphabet = \"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_\", int alphabetSize = -1) as string\n"
    "    if length < 0 or alphabetSize < -1 then throw RangeError \"secure random string length is invalid\" end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_secure_string(alphabet, __xl_std_xrt_string_byte_len(alphabet), alphabetSize, length))\n"
    "end\n";

static const char xl6StdCryptoFillRandomBytesBody[] =
    "func __xl_std_crypto_fill_random_bytes(Buffer target)\n"
    "    if not __xl_std_xrt_secure_fill_buffer(target.nativeHandle()) then throw RuntimeError \"secure random buffer fill failed\" end\n"
    "end\n";

static const char xl6StdCryptoChaCha20EncryptBody[] =
    "func __xl_std_crypto_chacha20_encrypt(string key, string nonce, string text, int counter = 1) as string\n"
    "    if counter < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    c_size __xl_size_key = __xl_std_xrt_string_byte_len(key)\n"
    "    c_size __xl_size_nonce = __xl_std_xrt_string_byte_len(nonce)\n"
    "    c_size __xl_size_text = __xl_std_xrt_string_byte_len(text)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_chacha20_encrypt_hex(key, __xl_size_key, nonce, __xl_size_nonce, text, __xl_size_text, c_uint(counter)))\n"
    "end\n";

static const char xl6StdCryptoChaCha20DecryptBody[] =
    "func __xl_std_crypto_chacha20_decrypt(string key, string nonce, string cipherHex, int counter = 1) as string\n"
    "    if counter < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    c_size __xl_size_key = __xl_std_xrt_string_byte_len(key)\n"
    "    c_size __xl_size_nonce = __xl_std_xrt_string_byte_len(nonce)\n"
    "    c_size __xl_size_cipherHex = __xl_std_xrt_string_byte_len(cipherHex)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_chacha20_decrypt_text(key, __xl_size_key, nonce, __xl_size_nonce, cipherHex, __xl_size_cipherHex, c_uint(counter)))\n"
    "end\n";

static const char xl6StdCryptoChaCha20BytesBody[] =
    "func __xl_std_crypto_chacha20_bytes(Buffer key, Buffer nonce, Buffer data, int counter = 1) as Buffer\n"
    "    if counter < 0 then\n"
    "        return Buffer()\n"
    "    end\n"
    "    return Buffer(point(__xl_std_runtime_chacha20_bytes(key.data(), c_size(key.len()), nonce.data(), c_size(nonce.len()), data.data(), c_size(data.len()), c_uint(counter))))\n"
    "end\n";

static const char xl6StdCryptoRandomBytesBody[] =
    "func __xl_std_crypto_random_bytes(int count) as bytes\n"
    "    if count < 0 then throw RangeError \"secure random byte count is invalid\" end\n"
    "    return __xl_std_xrt_secure_bytes(count)\n"
    "end\n";

static const char xl6StdCryptoEd25519KeypairBody[] =
    "func __xl_std_crypto_ed25519_keypair() as var\n"
    "    return __xl_std_runtime_ed25519_keypair()\n"
    "end\n";

static const char xl6StdCryptoEd25519PublicKeyBody[] =
    "func __xl_std_crypto_ed25519_public_key(string seedHex) as string\n"
    "    c_size __xl_size_seedHex = __xl_std_xrt_string_byte_len(seedHex)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_ed25519_public_key_hex(seedHex, __xl_size_seedHex))\n"
    "end\n";

static const char xl6StdCryptoEd25519SignBody[] =
    "func __xl_std_crypto_ed25519_sign(string seedHex, string text) as string\n"
    "    c_size __xl_size_seedHex = __xl_std_xrt_string_byte_len(seedHex)\n"
    "    c_size __xl_size_text = __xl_std_xrt_string_byte_len(text)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_ed25519_sign_hex(seedHex, __xl_size_seedHex, text, __xl_size_text))\n"
    "end\n";

static const char xl6StdCryptoEd25519VerifyBody[] =
    "func __xl_std_crypto_ed25519_verify(string publicKeyHex, string text, string signatureHex) as bool\n"
    "    c_size __xl_size_publicKeyHex = __xl_std_xrt_string_byte_len(publicKeyHex)\n"
    "    c_size __xl_size_text = __xl_std_xrt_string_byte_len(text)\n"
    "    c_size __xl_size_signatureHex = __xl_std_xrt_string_byte_len(signatureHex)\n"
    "    return __xl_std_runtime_ed25519_verify(publicKeyHex, __xl_size_publicKeyHex, text, __xl_size_text, signatureHex, __xl_size_signatureHex)\n"
    "end\n";

static const char xl6StdCryptoX25519KeypairBody[] =
    "func __xl_std_crypto_x25519_keypair() as var\n"
    "    return __xl_std_runtime_x25519_keypair()\n"
    "end\n";

static const char xl6StdCryptoX25519SharedSecretBody[] =
    "func __xl_std_crypto_x25519_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    c_size __xl_size_privateKeyHex = __xl_std_xrt_string_byte_len(privateKeyHex)\n"
    "    c_size __xl_size_publicKeyHex = __xl_std_xrt_string_byte_len(publicKeyHex)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_x25519_shared_secret_hex(privateKeyHex, __xl_size_privateKeyHex, publicKeyHex, __xl_size_publicKeyHex))\n"
    "end\n";

static const char xl6StdCryptoX448KeypairBody[] =
    "func __xl_std_crypto_x448_keypair() as var\n"
    "    return __xl_std_runtime_x448_keypair()\n"
    "end\n";

static const char xl6StdCryptoX448SharedSecretBody[] =
    "func __xl_std_crypto_x448_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    c_size __xl_size_privateKeyHex = __xl_std_xrt_string_byte_len(privateKeyHex)\n"
    "    c_size __xl_size_publicKeyHex = __xl_std_xrt_string_byte_len(publicKeyHex)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_x448_shared_secret_hex(privateKeyHex, __xl_size_privateKeyHex, publicKeyHex, __xl_size_publicKeyHex))\n"
    "end\n";

static const char xl6StdCryptoP256KeypairBody[] =
    "func __xl_std_crypto_p256_keypair() as var\n"
    "    return __xl_std_runtime_p256_keypair()\n"
    "end\n";

static const char xl6StdCryptoP256SharedSecretBody[] =
    "func __xl_std_crypto_p256_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    c_size __xl_size_privateKeyHex = __xl_std_xrt_string_byte_len(privateKeyHex)\n"
    "    c_size __xl_size_publicKeyHex = __xl_std_xrt_string_byte_len(publicKeyHex)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_p256_shared_secret_hex(privateKeyHex, __xl_size_privateKeyHex, publicKeyHex, __xl_size_publicKeyHex))\n"
    "end\n";

static const char xl6StdCryptoP384KeypairBody[] =
    "func __xl_std_crypto_p384_keypair() as var\n"
    "    return __xl_std_runtime_p384_keypair()\n"
    "end\n";

static const char xl6StdCryptoP384SharedSecretBody[] =
    "func __xl_std_crypto_p384_shared_secret(string privateKeyHex, string publicKeyHex) as string\n"
    "    c_size __xl_size_privateKeyHex = __xl_std_xrt_string_byte_len(privateKeyHex)\n"
    "    c_size __xl_size_publicKeyHex = __xl_std_xrt_string_byte_len(publicKeyHex)\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_p384_shared_secret_hex(privateKeyHex, __xl_size_privateKeyHex, publicKeyHex, __xl_size_publicKeyHex))\n"
    "end\n";

static const char xl6StdCryptoEcdsaVerifyBody[] =
    "func __xl_std_crypto_ecdsa_verify(string publicKeyHex, string hashHex, string signatureHex) as bool\n"
    "    c_size __xl_size_publicKeyHex = __xl_std_xrt_string_byte_len(publicKeyHex)\n"
    "    c_size __xl_size_hashHex = __xl_std_xrt_string_byte_len(hashHex)\n"
    "    c_size __xl_size_signatureHex = __xl_std_xrt_string_byte_len(signatureHex)\n"
    "    return __xl_std_runtime_ecdsa_verify(publicKeyHex, __xl_size_publicKeyHex, hashHex, __xl_size_hashHex, signatureHex, __xl_size_signatureHex)\n"
    "end\n";

static const char xl6StdCryptoRsaPssVerifyBody[] =
    "func __xl_std_crypto_rsa_pss_verify(string hashHex, string signatureHex, string modulusHex, string exponentHex) as bool\n"
    "    c_size __xl_size_hashHex = __xl_std_xrt_string_byte_len(hashHex)\n"
    "    c_size __xl_size_signatureHex = __xl_std_xrt_string_byte_len(signatureHex)\n"
    "    c_size __xl_size_modulusHex = __xl_std_xrt_string_byte_len(modulusHex)\n"
    "    c_size __xl_size_exponentHex = __xl_std_xrt_string_byte_len(exponentHex)\n"
    "    return __xl_std_runtime_rsa_pss_verify(hashHex, __xl_size_hashHex, signatureHex, __xl_size_signatureHex, modulusHex, __xl_size_modulusHex, exponentHex, __xl_size_exponentHex)\n"
    "end\n";

static const char xl6StdCryptoRsaPkcs1VerifyBody[] =
    "func __xl_std_crypto_rsa_pkcs1_verify(string hashHex, string signatureHex, string modulusHex, string exponentHex) as bool\n"
    "    c_size __xl_size_hashHex = __xl_std_xrt_string_byte_len(hashHex)\n"
    "    c_size __xl_size_signatureHex = __xl_std_xrt_string_byte_len(signatureHex)\n"
    "    c_size __xl_size_modulusHex = __xl_std_xrt_string_byte_len(modulusHex)\n"
    "    c_size __xl_size_exponentHex = __xl_std_xrt_string_byte_len(exponentHex)\n"
    "    return __xl_std_runtime_rsa_pkcs1_verify(hashHex, __xl_size_hashHex, signatureHex, __xl_size_signatureHex, modulusHex, __xl_size_modulusHex, exponentHex, __xl_size_exponentHex)\n"
    "end\n";

static const char xl6StdIdTryParseBody[] =
    "func __xl_std_id_try_parse(string text) as Xid?\n"
    "    try\n"
    "        return __xl_std_id_parse(text)\n"
    "    catch e\n"
    "        if e.kind == \"ValueError\" then return null end\n"
    "        throw\n"
    "    end\n"
    "end\n";

static const char xl6StdNetLocalIPBody[] =
    "extern \"xl6NetLocalIPV1\" func __xl_std_net_local_ip(int family = 0) as string\n";
static const char xl6StdNetLocalAddressBody[] =
    "extern \"xl6NetLocalAddressV1\" func __xl_std_net_local_address(int family = 0) as NetAddress\n";

static const char xl6StdNetLocalMACBody[] =
    "extern \"xl6NetLocalMACV1\" func __xl_std_net_local_mac() as string\n";

static const char xl6StdNetLocalNameBody[] =
    "extern \"xl6NetHostNameV1\" func __xl_std_net_local_name() as string\n";


static const char xl6StdNetInterfaceIndexBody[] =
    "extern \"xl6NetInterfaceIndexV1\" func __xl_std_net_interface_index(string name, int family = 0) as int\n";

static const char xl6StdNetInterfaceNameBody[] =
    "extern \"xl6NetInterfaceNameByIndexV1\" func __xl_std_net_interface_name(int index, int family = 0) as string\n";

static const char xl6StdNetResolveBody[] =
    "func __xl_std_net_resolve(string host, int port = 0) as NetAddress\n"
    "    return NetAddress(host, port)\n"
    "end\n";

static const char xl6StdNetResolveAllBody[] =
    "func __xl_std_net_resolve_all(string host, int port = 0, int family = 0) as NetAddressList\n"
    "    return NetAddressList(host, port, family)\n"
    "end\n";

static const char xl6StdNetResolveAsyncBody[] =
    "func __xl_std_net_resolve_async(string host, int port = 0, int family = 0) as future<NetAddressList>\n"
    "    return __xl_std_net_default_resolver.resolveAsync(host, port, family)\n"
    "end\n";

static const char xl6StdNetStatsBody[] =
    "extern \"xl6NetEngineSnapshotV1\" @nonblocking func __xl_std_net_stats(NetEngine engine) as NetEngineStats\n";

static const char xl6StdNetResolverStatsBody[] =
    "func __xl_std_net_resolver_stats() as NetResolverStats\n"
    "    return __xl_std_net_default_resolver.snapshot()\n"
    "end\n";

static const char xl6StdNetProxyBody[] =
    "func __xl_std_net_proxy(NetProxyType type, string host, int port, string user = \"\", string password = \"\", NetProxyAuth auth = NetProxyAuth.automatic) as NetProxy\n"
    "    return NetProxy(type, host, port, user, password, auth)\n"
    "end\n"
    "func __xl_std_net_proxy(NetProxyType type, string host, int port, bytes user, bytes password, NetProxyAuth auth = NetProxyAuth.automatic) as NetProxy\n"
    "    return NetProxy(type, host, port, user, password, auth)\n"
    "end\n";

static const char xl6StdNetEngineBody[] =
    "extern \"xl6NetEngineCreateWorkersV1\" func __xl_std_net_engine(int workerCount = 0) as NetEngine\n";

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
    "func __xl_std_tcp_connect(NetEngine engine, NetAddress remote) as TcpStream\n"
    "    return TcpStream(engine, remote)\n"
    "end\n"
    "func __xl_std_tcp_connect(NetEngine engine, NetAddress remote, TcpStreamOptions options, int worker = 0) as TcpStream\n"
    "    return TcpStream(engine, remote, options, worker)\n"
    "end\n";

static const char xl6StdTcpConnectAsyncBody[] =
    "func __xl_std_tcp_connect_async(NetEngine engine, NetAddress remote) as future<TcpStream>\n"
    "    TcpStream stream = TcpStream(engine, remote)\n"
    "    return stream.connectedAsync()\n"
    "end\n"
    "func __xl_std_tcp_connect_async(NetEngine engine, NetAddress remote, TcpStreamOptions options, int worker = 0) as future<TcpStream>\n"
    "    TcpStream stream = TcpStream(engine, remote, options, worker)\n"
    "    return stream.connectedAsync()\n"
    "end\n";

static const char xl6StdTcpListenBody[] =
    "func __xl_std_tcp_listen(NetEngine engine, TcpListenOptions options) as TcpListener\n"
    "    return TcpListener(engine, options)\n"
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
    "    if not __xl_std_runtime_print(arg, false) then throw IOError \"stdout print failed\" end\n"
    "end\n";

static const char xl6StdPrintlnBody[] =
    "func __xl_std_println(...)\n"
    "    if not __xl_std_runtime_print(arg, true) then throw IOError \"stdout print failed\" end\n"
    "end\n";

static const char xl6StdWriteBody[] =
    "func __xl_std_write(string text)\n"
    "    if not __xl_std_runtime_write(text) then\n"
    "        throw IOError \"stdout write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdWritelnBody[] =
    "func __xl_std_writeln(string text = \"\")\n"
    "    if not __xl_std_runtime_writeln(text) then\n"
    "        throw IOError \"stdout write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdConsoleWriteBody[] =
    "func __xl_std_console_write(string text)\n"
    "    if not __xl_std_runtime_write(text) then\n"
    "        throw IOError \"stdout write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdConsoleWriteLineBody[] =
    "func __xl_std_console_write_line(string text = \"\")\n"
    "    if not __xl_std_runtime_writeln(text) then\n"
    "        throw IOError \"stdout write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdConsoleErrorBody[] =
    "func __xl_std_console_error(string text)\n"
    "    if not __xl_std_runtime_console_error(text) then\n"
    "        throw IOError \"stderr write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdConsoleErrorLineBody[] =
    "func __xl_std_console_error_line(string text = \"\")\n"
    "    if not __xl_std_runtime_console_error_line(text) then\n"
    "        throw IOError \"stderr write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdConsoleFlushBody[] =
    "func __xl_std_console_flush()\n"
    "    bool stdoutOk = __xl_std_runtime_console_flush(c_int(1))\n"
    "    bool stderrOk = __xl_std_runtime_console_flush(c_int(2))\n"
    "    if not stdoutOk then\n"
    "        throw IOError \"stdout flush failed\"\n"
    "    end\n"
    "    if not stderrOk then\n"
    "        throw IOError \"stderr flush failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdIoReadFileBody[] =
    "func __xl_std_io_read_file(string path, int maxBytes = 67108864) as bytes\n"
    "    if maxBytes < 0 then throw RangeError \"file read limit is invalid\" end\n"
    "    return __xl_std_runtime_io_read_file(path, maxBytes)\n"
    "end\n";

static const char xl6StdIoWriteFileBody[] =
    "func __xl_std_io_write_file(string path, bytes data, int mode = 1, int size = -1)\n"
    "    if not __xl_std_runtime_io_write_file(path, data, mode, size) then\n"
    "        throw IOError \"file write failed\"\n"
    "    end\n"
    "end\n";

static const char xl6StdIoWriteTextFileBody[] =
    "func __xl_std_io_write_file(string path, string data, int mode = 1, int size = -1)\n"
    "    if not __xl_std_runtime_io_write_text_file(path, data, mode, size) then\n"
    "        throw IOError \"file write failed\"\n"
    "    end\n"
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
    {"net.capability","none",XL6_AST_INT,"0"},
    {"net.capability","readiness",XL6_AST_INT,"1"},
    {"net.capability","completion",XL6_AST_INT,"2"},
    {"net.capability","oneShot",XL6_AST_INT,"4"},
    {"net.capability","edge",XL6_AST_INT,"8"},
    {"net.capability","batch",XL6_AST_INT,"16"},
    {"net.capability","wake",XL6_AST_INT,"32"},
    {"net.capability","post",XL6_AST_INT,"64"},
    {"net.capability","cancel",XL6_AST_INT,"128"},
    {"net.capability","readProbe",XL6_AST_INT,"256"},
    {"net.capability","datagramError",XL6_AST_INT,"512"},
    {"net.capability","sendFile",XL6_AST_INT,"1024"},
    {"net.capability","fileIO",XL6_AST_INT,"2048"},
    {"concurrent", "relaxed", XL6_AST_INT, "0"},
    {"concurrent", "acquire", XL6_AST_INT, "1"},
    {"concurrent", "release", XL6_AST_INT, "2"},
    {"concurrent", "acqRel", XL6_AST_INT, "3"},
    {"concurrent", "seqCst", XL6_AST_INT, "4"},
    {"codec", "hexUpper", XL6_AST_INT, "1"},
    {"compress", "raw", XL6_AST_INT, "0"},
    {"compress", "zlib", XL6_AST_INT, "1"},
    {"compress", "gzip", XL6_AST_INT, "2"},
    {"compress", "httpDeflate", XL6_AST_INT, "3"},
    {"compress", "flushNone", XL6_AST_INT, "0"},
    {"compress", "flushSync", XL6_AST_INT, "1"},
    {"compress", "flushFull", XL6_AST_INT, "2"},
    {"compress", "flushFinish", XL6_AST_INT, "3"},
    {"compress", "strategyDefault", XL6_AST_INT, "0"},
    {"compress", "strategyFiltered", XL6_AST_INT, "1"},
    {"compress", "strategyHuffman", XL6_AST_INT, "2"},
    {"compress", "strategyRle", XL6_AST_INT, "3"},
    {"compress", "strategyFixed", XL6_AST_INT, "4"},
    {"codec", "hexIgnoreSpace", XL6_AST_INT, "2"},
    {"codec", "base64Url", XL6_AST_INT, "1"},
    {"codec", "base64NoPadding", XL6_AST_INT, "2"},
    {"codec", "base64IgnoreSpace", XL6_AST_INT, "4"},
    {"codec", "base64OptionalPadding", XL6_AST_INT, "8"},
    {"unicode", "complete", XL6_AST_INT, "0"},
    {"unicode", "more", XL6_AST_INT, "1"},
    {"unicode", "invalid", XL6_AST_INT, "2"},
    {"file", "walkContinue", XL6_AST_INT, "0"},
    {"file", "walkSkip", XL6_AST_INT, "1"},
    {"file", "walkStop", XL6_AST_INT, "2"},
    {"file", "walkError", XL6_AST_INT, "3"},
    {"file", "walkAbort", XL6_AST_INT, "0"},
    {"file", "walkSkipError", XL6_AST_INT, "1"},
    {"file", "walkStopError", XL6_AST_INT, "2"},
    {"console", "stdoutStream", XL6_AST_INT, "1"},
    {"queueStatus", "ok", XL6_AST_INT, "0"},
    {"queueStatus", "empty", XL6_AST_INT, "1"},
    {"queueStatus", "full", XL6_AST_INT, "2"},
    {"queueStatus", "closed", XL6_AST_INT, "3"},
    {"queueStatus", "timeout", XL6_AST_INT, "4"},
    {"channelStatus", "ok", XL6_AST_INT, "0"},
    {"channelStatus", "empty", XL6_AST_INT, "1"},
    {"channelStatus", "full", XL6_AST_INT, "2"},
    {"channelStatus", "closed", XL6_AST_INT, "3"},
    {"channelStatus", "timeout", XL6_AST_INT, "4"},
    {"channelStatus", "cancelled", XL6_AST_INT, "5"},
    {"taskGroupCancelOn", "none", XL6_AST_INT, "0"},
    {"taskGroupCancelOn", "failed", XL6_AST_INT, "1"},
    {"taskGroupCancelOn", "cancelled", XL6_AST_INT, "2"},
    {"taskGroupCancelOn", "closed", XL6_AST_INT, "4"},
    {"taskGroupCancelOn", "stopped", XL6_AST_INT, "7"},
    {"taskGroupState", "pending", XL6_AST_INT, "0"},
    {"taskGroupState", "resolved", XL6_AST_INT, "1"},
    {"taskGroupState", "failed", XL6_AST_INT, "2"},
    {"taskGroupState", "cancelled", XL6_AST_INT, "3"},
    {"taskGroupState", "closed", XL6_AST_INT, "4"},
    {"console", "stderrStream", XL6_AST_INT, "2"},
    {"console", "colorNone", XL6_AST_INT, "0"},
    {"console", "colorBasic", XL6_AST_INT, "1"},
    {"console", "color256", XL6_AST_INT, "2"},
    {"console", "colorTrue", XL6_AST_INT, "3"},
    {"console", "eventText", XL6_AST_INT, "1"},
    {"console", "eventKey", XL6_AST_INT, "2"},
    {"console", "eventResize", XL6_AST_INT, "3"},
    {"console", "eventPaste", XL6_AST_INT, "4"},
    {"console", "eventMouse", XL6_AST_INT, "5"},
    {"console", "eventClosed", XL6_AST_INT, "6"},
    {"console", "modShift", XL6_AST_INT, "1"},
    {"console", "modCtrl", XL6_AST_INT, "2"},
    {"console", "modAlt", XL6_AST_INT, "4"},
    {"console", "modMeta", XL6_AST_INT, "8"},
    {"console", "styleBold", XL6_AST_INT, "1"},
    {"console", "styleDim", XL6_AST_INT, "2"},
    {"console", "styleItalic", XL6_AST_INT, "4"},
    {"console", "styleUnderline", XL6_AST_INT, "8"},
    {"console", "styleReverse", XL6_AST_INT, "16"},
    {"console", "clearScreen", XL6_AST_INT, "0"},
    {"console", "clearLine", XL6_AST_INT, "1"},
    {"console", "clearToEnd", XL6_AST_INT, "2"},
    {"console", "keyEscape", XL6_AST_INT, "1114112"},
    {"console", "keyEnter", XL6_AST_INT, "1114113"},
    {"console", "keyTab", XL6_AST_INT, "1114114"},
    {"console", "keyBackspace", XL6_AST_INT, "1114115"},
    {"console", "keyUp", XL6_AST_INT, "1114116"},
    {"console", "keyDown", XL6_AST_INT, "1114117"},
    {"console", "keyLeft", XL6_AST_INT, "1114118"},
    {"console", "keyRight", XL6_AST_INT, "1114119"},
    {"console", "keyHome", XL6_AST_INT, "1114120"},
    {"console", "keyEnd", XL6_AST_INT, "1114121"},
    {"console", "keyInsert", XL6_AST_INT, "1114122"},
    {"console", "keyDelete", XL6_AST_INT, "1114123"},
    {"console", "keyPageUp", XL6_AST_INT, "1114124"},
    {"console", "keyPageDown", XL6_AST_INT, "1114125"},
    {"console", "keyF1", XL6_AST_INT, "1114126"},
    {"io", "createNew", XL6_AST_INT, "0"},
    {"io", "truncate", XL6_AST_INT, "1"},
    {"io", "seekStart", XL6_AST_INT, "0"},
    {"io", "seekCurrent", XL6_AST_INT, "1"},
    {"io", "seekEnd", XL6_AST_INT, "2"},
    {"io", "lineNone", XL6_AST_INT, "0"},
    {"io", "lineLf", XL6_AST_INT, "1"},
    {"io", "lineCrLf", XL6_AST_INT, "2"},
    {"io", "append", XL6_AST_INT, "2"},
    {"math", "pi", XL6_AST_FLOAT, "3.14159265358979323846264338327950288"},
    {"math", "tau", XL6_AST_FLOAT, "6.28318530717958647692528676655900576"},
    {"math", "e", XL6_AST_FLOAT, "2.71828182845904523536028747135266250"},
    {"math", "nan", XL6_AST_FLOAT, "(0.0/0.0)"},
    {"math", "inf", XL6_AST_FLOAT, "(1.0/0.0)"},
    {"log", "textTime", XL6_AST_INT, "1"},
    {"log", "textLevel", XL6_AST_INT, "2"},
    {"log", "textLogger", XL6_AST_INT, "4"},
    {"log", "textSource", XL6_AST_INT, "8"},
    {"log", "textThread", XL6_AST_INT, "16"},
    {"log", "textFields", XL6_AST_INT, "32"},
    {"log", "textNewline", XL6_AST_INT, "64"},
    {"log", "textRawMessage", XL6_AST_INT, "128"},
    {"log", "jsonTime", XL6_AST_INT, "1"},
    {"log", "jsonLevel", XL6_AST_INT, "2"},
    {"log", "jsonLogger", XL6_AST_INT, "4"},
    {"log", "jsonMessage", XL6_AST_INT, "8"},
    {"log", "jsonSource", XL6_AST_INT, "16"},
    {"log", "jsonThread", XL6_AST_INT, "32"},
    {"log", "jsonFields", XL6_AST_INT, "64"},
    {"log", "jsonNewline", XL6_AST_INT, "128"},
    {"json", "visitComplete", XL6_AST_INT, "0"},
    {"json", "visitStopped", XL6_AST_INT, "1"},
    {"json", "visitInvalid", XL6_AST_INT, "-1"},
    {"xlon", "visitComplete", XL6_AST_INT, "0"},
    {"xlon", "visitStopped", XL6_AST_INT, "1"},
    {"xlon", "visitInvalid", XL6_AST_INT, "-1"},
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
    {"encoding", "unknown", XL6_AST_INT, "0"},
    {"encoding", "utf8", XL6_AST_INT, "1"},
    {"encoding", "utf16LE", XL6_AST_INT, "2"},
    {"encoding", "utf16BE", XL6_AST_INT, "3"},
    {"encoding", "utf32LE", XL6_AST_INT, "4"},
    {"encoding", "utf32BE", XL6_AST_INT, "5"},
    {"encoding", "strict", XL6_AST_INT, "0"},
    {"encoding", "replaceInvalid", XL6_AST_INT, "1"},
    {"encoding", "keepBom", XL6_AST_INT, "0"},
    {"encoding", "stripBom", XL6_AST_INT, "1"},
    {"encoding", "requireBom", XL6_AST_INT, "2"},
    {"encoding", "rejectBom", XL6_AST_INT, "3"},
    {"encoding", "writeBom", XL6_AST_INT, "4"},
    {"tcc", "MEMORY", XL6_AST_INT, "1"},
    {"tcc", "EXE", XL6_AST_INT, "2"},
    {"tcc", "OBJ", XL6_AST_INT, "3"},
    {"tcc", "DLL", XL6_AST_INT, "4"},
    {"tcc", "PREPROCESS", XL6_AST_INT, "5"},
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
    {"net","backendCompiled","__xl_std_net_backend_compiled",NULL,NULL,0,"bool",xl6StdNetBackendCompiledSource},
    {"net","backendCapabilities","__xl_std_net_backend_capabilities",NULL,NULL,0,"int",xl6StdNetBackendCapabilitiesSource},
    {"net","backendName","__xl_std_net_backend_name",NULL,NULL,0,"string",xl6StdNetBackendNameSource},
    {"net","defaultBackend","__xl_std_net_default_backend","xl6NetDefaultBackendV1",NULL,0,"NetBackend",NULL},
    {"unicode", "isScalar", "__xl_std_unicode_scalar", NULL, NULL, 0, "bool", xl6StdUnicodeScalarBody},
    {"unicode", "toString", "__xl_std_unicode_char_string", NULL, NULL, 0, "string", xl6StdUnicodeCharStringBody},
    {"unicode", "byteOffset", "__xl_std_unicode_byte_offset", NULL, NULL, 0, "int", xl6StdUnicodeByteOffsetBody},
    {"unicode", "scalarIndex", "__xl_std_unicode_scalar_index", NULL, NULL, 0, "int", xl6StdUnicodeScalarIndexBody},
    {"unicode", "validate", "__xl_std_unicode_validate", NULL, NULL, 0, NULL, xl6StdUnicodeValidateTextBody},
    {"unicode", "validate", "__xl_std_unicode_validate", NULL, NULL, 0, NULL, xl6StdUnicodeValidateBytesBody},
    {"unicode", "validator", "__xl_std_unicode_validator", NULL, NULL, 0, "Utf8Validator", xl6StdUnicodeValidatorBody},
    {"regex", "compile", "__xl_std_regex_compile", NULL, NULL, 0, "Regex", xl6StdRegexCompileBody},
    {"regex", "escape", "__xl_std_regex_escape", NULL, NULL, 0, "string", xl6StdRegexEscapeBody},
    {"regex", "set", "__xl_std_regex_set", NULL, NULL, 0, "RegexSet", xl6StdRegexSetBody},
    {"regex", "errorOffset", "__xl_std_regex_error_offset", NULL, NULL, 0, "int", xl6StdRegexErrorOffsetBody},
    {"regex", "errorPatternIndex", "__xl_std_regex_error_index", NULL, NULL, 0, "int", xl6StdRegexErrorIndexBody},
    {"pattern", "compile", "__xl_std_pattern_compile", NULL, NULL, 0, "Pattern", xl6StdPatternCompileBody},
    {"pattern", "errorOffset", "__xl_std_pattern_error_offset", NULL, NULL, 0, "int", xl6StdPatternErrorOffsetBody},
    {"pattern", "errorPatternIndex", "__xl_std_pattern_error_index", NULL, NULL, 0, "int", xl6StdPatternErrorIndexBody},
    {"html", "escapeText", "__xl_std_html_escape_text", NULL, NULL, 0, "string", xl6StdHtmlEscapeTextBody},
    {"html", "escapeAttribute", "__xl_std_html_escape_attribute", NULL, NULL, 0, "string", xl6StdHtmlEscapeAttributeBody},
    {"file", "takeAsync", "__xl_std_file_take_async", NULL, NULL, 0, "AsyncFile", xl6StdFsAsync_file_takeAsyncBody},
    {"file", "appendBytesAsync", "__xl_std_file_append_bytes_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_appendBytesAsyncBody},
    {"file", "writeBytesAtomicAsync", "__xl_std_file_write_bytes_atomic_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_writeBytesAtomicAsyncBody},
    {"file", "writeTextAtomicAsync", "__xl_std_file_write_text_atomic_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_writeTextAtomicAsyncBody},
    {"dir", "removeAsync", "__xl_std_dir_remove_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_dir_removeAsyncBody},
    {"dir", "emptyAsync", "__xl_std_dir_empty_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_dir_emptyAsyncBody},
    {"dir", "ensureEmptyAsync", "__xl_std_dir_ensure_empty_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_dir_ensureEmptyAsyncBody},
    {"dir", "sizeAsync", "__xl_std_dir_size_async", NULL, NULL, 0, "future<uint64>", xl6StdFsAsync_dir_sizeAsyncBody},
    {"file", "copyTreeAsync", "__xl_std_file_copy_tree_async", NULL, NULL, 0, "future<TreeStats>", xl6StdFsAsync_file_copyTreeAsyncBody},
    {"file", "removeTreeAsync", "__xl_std_file_remove_tree_async", NULL, NULL, 0, "future<TreeStats>", xl6StdFsAsync_file_removeTreeAsyncBody},
    {"dir", "cleanAsync", "__xl_std_dir_clean_async", NULL, NULL, 0, "future<TreeStats>", xl6StdFsAsync_dir_cleanAsyncBody},
    {"dir", "statsAsync", "__xl_std_dir_stats_async", NULL, NULL, 0, "future<WalkStats>", xl6StdFsAsync_dir_statsAsyncBody},
    {"__xl_runtime", "takeCstr", "__xl_std_runtime_take_cstr", "xl6_xrt_take_cstr", xl6StdRuntimeTakeCstrParams, 1, "string", NULL},
    {"__xl_runtime", "print", "__xl_std_runtime_print", "xl6_xrt_print", xl6StdRuntimePrintParams, 2, "bool", NULL},
    {"__xl_runtime", "write", "__xl_std_runtime_write", "xl6_xrt_write", xl6StdRuntimeStringTextParams, 1, "c_bool", NULL},
    {"__xl_runtime", "writeln", "__xl_std_runtime_writeln", "xl6_xrt_writeln", xl6StdRuntimeStringTextParams, 1, "c_bool", NULL},
    {"__xl_runtime", "consoleError", "__xl_std_runtime_console_error", "xl6_xrt_console_error", xl6StdRuntimeStringTextParams, 1, "c_bool", NULL},
    {"__xl_runtime", "consoleErrorLine", "__xl_std_runtime_console_error_line", "xl6_xrt_console_error_line", xl6StdRuntimeStringTextParams, 1, "c_bool", NULL},
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
    {"__xl_runtime", "chacha20EncryptHex", "__xl_std_runtime_chacha20_encrypt_hex", "xl6_xrt_chacha20_encrypt_hex", xl6StdRuntimeCryptoChacha20Params, 7, "point", NULL},
    {"__xl_runtime", "chacha20DecryptText", "__xl_std_runtime_chacha20_decrypt_text", "xl6_xrt_chacha20_decrypt_text", xl6StdRuntimeCryptoChacha20Params, 7, "point", NULL},
    {"__xl_runtime", "chacha20Bytes", "__xl_std_runtime_chacha20_bytes", "xl6_xrt_chacha20_bytes", xl6StdRuntimeCryptoChacha20BytesParams, 7, "point", NULL},
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
    {"__xl_runtime", "logDefault", "__xl_std_runtime_log_default", "xl6LogDefaultV1", NULL, 0, "point", NULL},
    {"__xl_runtime", "logReleaseDefault", "__xl_std_runtime_log_release_default", "xl6LogReleaseDefaultV1", xl6StdLogSetDefaultParams, 1, "bool", NULL},
    {"__xl_runtime", "logTextValidate", "__xl_std_runtime_log_text_validate", "xl6LogTextValidateV1", xl6StdLogTextValidateParams, 3, "bool", NULL},
    {"__xl_runtime", "logJsonValidate", "__xl_std_runtime_log_json_validate", "xl6LogJsonValidateV1", xl6StdLogJsonValidateParams, 5, "bool", NULL},
    {"__xl_runtime", "logDefaultRecord", "__xl_std_runtime_log_default_record", "xl6LogDefaultRecordV1", xl6StdLogRecordParams, 1, "LogResult", NULL},
    {"__xl_runtime", "logDefaultWrite", "__xl_std_runtime_log_default_write", "xl6LogDefaultWriteV1", xl6StdLogDefaultWriteParams, 3, "LogResult", NULL},
    {"__xl_runtime", "logDefaultFlush", "__xl_std_runtime_log_default_flush", "xl6LogDefaultFlushV1", NULL, 0, "bool", NULL},
    {"__xl_runtime", "logSetDefault", "__xl_std_runtime_log_set_default", "xl6LogSetDefaultV1", xl6StdLogSetDefaultParams, 1, "bool", NULL},
    {"__xl_runtime", "logStats", "__xl_std_runtime_log_stats", "xl6LogStatsV1", xl6StdLogStatsParams, 2, "point", NULL},
    {"__xl_runtime", "logLevelName", "__xl_std_runtime_log_level_name", "xl6LogLevelNameV1", xl6StdRuntimeLogLevelParams, 1, "string", NULL},









    {"__xl_runtime", "dirList", "__xl_std_runtime_dir_list", "xl6_xrt_dir_list", xl6StdRuntimeDirListParams, 3, "var", NULL},
    {"__xl_runtime", "dirGlob", "__xl_std_runtime_dir_glob", "xl6_xrt_dir_glob", xl6StdRuntimeDirGlobParams, 4, "var", NULL},
    {"__xl_runtime", "dirEntries", "__xl_std_runtime_dir_entries", "xl6_xrt_dir_entries", xl6StdRuntimeDirEntriesParams, 5, "var", NULL},
    {"__xl_runtime", "pathJoin", "__xl_std_runtime_path_join", "xl6_xrt_path_join", xl6StdRuntimePathJoinParams, 3, "point", NULL},
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

    {"__xrt_string", "byteLen", "__xl_std_xrt_string_byte_len", "xl6_xrt_string_byte_len", xl6StdXrtStringSizeParams, 1, "c_size", NULL},
    {"__xrt_string", "charLen", "__xl_std_xrt_string_char_len", "xl6_xrt_string_char_len", xl6StdXrtStringSizeParams, 1, "c_size", NULL},
    {"__xrt_string", "contains", "__xl_std_xrt_string_contains", "xl6_xrt_string_contains", xl6StdXrtInStrParams, 3, "c_bool", NULL},
    {"__xrt_string", "indexOf", "__xl_std_xrt_string_index_of", "xl6_xrt_string_index_of", xl6StdXrtInStrParams, 3, "int", NULL},
    {"__xrt_string", "replace", "__xl_std_xrt_string_replace", "xl6_xrt_string_replace", xl6StdXrtStringReplaceParams, 3, "point", NULL},
    {"__xrt_string", "slice", "__xl_std_xrt_string_slice", "xl6_xrt_string_slice", xl6StdXrtStringSliceParams, 3, "point", NULL},
    {"__xrt_string", "byteAt", "__xl_std_xrt_string_byte_at", "xl6_xrt_string_byte_at", xl6StdXrtStringByteAtParams, 2, "c_int", NULL},
    {"__xrt_string", "repeat", "__xl_std_xrt_string_repeat", "xl6_xrt_string_repeat", xl6StdXrtStringCountParams, 2, "point", NULL},
    {"__xrt_math", "sign", "__xl_std_xrt_math_sign", "xrtMathSign", xl6StdMathUnaryFloatParams, 1, "c_int", NULL},
    {"__xrt_time", "dateAdd", "__xl_std_xrt_date_add", NULL, NULL, 0, "time", "extern \"xl6TimeAddV1\" func __xl_std_xrt_date_add(time value, int amount, int unit) as time\n"},
    {"__xrt_time", "dateDiff", "__xl_std_xrt_date_diff", NULL, NULL, 0, "int", "extern \"xl6TimeDiffV1\" func __xl_std_xrt_date_diff(time left, time right, int unit) as int\n"},
    {"__xrt_random", "rand32", "__xl_std_xrt_rand32", "xl6_xrt_random_u32", NULL, 0, "c_uint", NULL},
    {"__xrt_random", "rand64", "__xl_std_xrt_rand64", "xl6_xrt_random_u64", NULL, 0, "uint64", NULL},
    {"__xrt_random", "below", "__xl_std_xrt_rand_below", "xl6_xrt_random_below", xl6StdXrtRandBelowParams, 1, "uint64", NULL},
    {"__xrt_random", "range", "__xl_std_xrt_rand_range", "xl6_xrt_random_range", xl6StdXrtRandRangeParams, 2, "int", NULL},
    {"__xrt_random", "rangeClosed", "__xl_std_xrt_rand_range_closed", "xl6_xrt_random_range_closed", xl6StdXrtRandRangeParams, 2, "int", NULL},
    {"__xrt_random", "real", "__xl_std_xrt_rand_real", "xl6_xrt_random_real", NULL, 0, "float", NULL},
    {"__xrt_random", "bytes", "__xl_std_xrt_rand_bytes", "xl6_xrt_random_bytes", xl6StdXrtRandCountParams, 1, "bytes", NULL},
    {"__xrt_random", "fillBuffer", "__xl_std_xrt_rand_fill_buffer", "xl6_xrt_random_fill_buffer", xl6StdXrtRandBufferParams, 1, "bool", NULL},
    {"__xrt_random", "shuffleArray", "__xl_std_xrt_rand_shuffle_array", "xl6_xrt_random_shuffle_array", xl6StdXrtRandArrayParams, 1, "bool", NULL},
    {"__xrt_random", "shuffleBuffer", "__xl_std_xrt_rand_shuffle_buffer", "xl6_xrt_random_shuffle_buffer", xl6StdXrtRandBufferParams, 1, "bool", NULL},
    {"__xrt_random", "seedThread", "__xl_std_xrt_rand_seed_thread", "xl6_xrt_random_seed", xl6StdXrtRandSeedParams, 2, "", NULL},
    {"__xrt_random", "string", "__xl_std_xrt_rand_str", "xl6_xrt_random_string", xl6StdXrtRandStrParams, 4, "point", NULL},
    {"__xrt_secure_random", "bytes", "__xl_std_xrt_secure_bytes", "xl6_xrt_secure_bytes", xl6StdXrtRandCountParams, 1, "bytes", NULL},
    {"__xrt_secure_random", "fillBuffer", "__xl_std_xrt_secure_fill_buffer", "xl6_xrt_secure_fill_buffer", xl6StdXrtRandBufferParams, 1, "bool", NULL},
    {"__xrt_secure_random", "hex", "__xl_std_xrt_secure_hex", "xl6_xrt_secure_hex", xl6StdXrtRandCountParams, 1, "point", NULL},
    {"__xrt_secure_random", "string", "__xl_std_xrt_secure_string", "xl6_xrt_secure_string", xl6StdXrtRandStrParams, 4, "point", NULL},
    {"__xrt_path", "exists", "__xl_std_xrt_path_exists", "xrtPathExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "isAbs", "__xl_std_xrt_path_is_abs", "xrtPathIsAbs", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "getNameExt", "__xl_std_xrt_path_get_name_ext", "xrtPathName", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "getName", "__xl_std_xrt_path_get_name", "xrtPathStem", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "getExt", "__xl_std_xrt_path_get_ext", "xrtPathExt", xl6StdPathUnaryParams, 1, "point", NULL},
    {"__xrt_path", "getDir", "__xl_std_xrt_path_get_dir", "xrtPathParent", xl6StdPathUnaryParams, 1, "point", NULL},
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
    {"__xl_runtime", "osPlatform", "__xl_std_runtime_os_platform", "xl6_xrt_os_platform", NULL, 0, "point", NULL},
    {"__xl_runtime", "osArgs", "__xl_std_runtime_os_args", "xl6_xrt_os_args", NULL, 0, "var", NULL},
    {"__xrt_file", "exists", "__xl_std_xrt_file_exists", "xrtFileExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "getSize", "__xl_std_xrt_file_get_size", "xl6_xrt_file_get_size_path", xl6StdPathUnaryParams, 1, "c_size", NULL},
    {"__xrt_file", "setSize", "__xl_std_xrt_file_set_size", "xrtFileSetSize", xl6StdXrtFileSetSizeParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAttr", "__xl_std_xrt_file_get_attr", "xl6_xrt_file_get_attributes", xl6StdPathUnaryParams, 1, "c_int", NULL},
    {"__xrt_file", "setAttr", "__xl_std_xrt_file_set_attr", "xl6_xrt_file_set_attributes", xl6StdXrtFileSetAttrParams, 2, "c_bool", NULL},
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
    {"__xrt_value", "tableRemove", "__xl_std_xrt_table_remove", "xl6DictValueRemoveV1", xl6StdDictRemoveParams, 2, "bool", NULL},
    {"__runtime", "jsonVisit", "__xl_std_runtime_json_visit", "xl6_xrt_json_visit", xl6StdJsonVisitRuntimeParams, 2, "c_int", NULL},
    {"__xrt_jsonl", "stringify", "__xl_std_xrt_jsonl_stringify", "xl6_xrt_jsonl_stringify", xl6StdLinesValueParams, 1, "point", NULL},
    {"__xrt_xlonl", "stringify", "__xl_std_xrt_xlonl_stringify", "xl6_xrt_xlonl_stringify", xl6StdLinesValueParams, 1, "point", NULL},
    {"__runtime", "xlonParseOptions", "__xl_std_runtime_xlon_parse_options", "xl6_xrt_xlon_parse_options", xl6StdXlonTextOptionsRuntimeParams, 4, "var", NULL},
    {"__runtime", "xlonValidOptions", "__xl_std_runtime_xlon_valid_options", "xl6_xrt_xlon_valid_options", xl6StdXlonTextOptionsRuntimeParams, 4, "bool", NULL},
    {"__runtime", "xlonReadOptions", "__xl_std_runtime_xlon_read_options", "xl6_xrt_xlon_read_options", xl6StdXlonTextOptionsRuntimeParams, 4, "var", NULL},
    {"__runtime", "xlonStringifyOptions", "__xl_std_runtime_xlon_stringify_options", "xl6_xrt_xlon_stringify_options", xl6StdXlonValueOptionsRuntimeParams, 4, "point", NULL},
    {"__runtime", "xlonWriteOptions", "__xl_std_runtime_xlon_write_options", "xl6_xrt_xlon_write_options", xl6StdXlonWriteOptionsRuntimeParams, 5, "bool", NULL},
    {"__runtime", "xlonVisitOptions", "__xl_std_runtime_xlon_visit_options", "xl6_xrt_xlon_visit_options", xl6StdXlonVisitOptionsRuntimeParams, 5, "c_int", NULL},
    {"__xrt_hash", "u32", "__xl_std_xrt_hash32", "xl6_xrt_hash32_string", xl6StdHashStringParams, 2, "c_uint", NULL},
    {"__xrt_hash", "u32WithSeed", "__xl_std_xrt_hash32_seed", "xl6_xrt_hash32_string_seed", xl6StdHashStringSeed32Params, 3, "c_uint", NULL},
    {"__xrt_hash", "u64", "__xl_std_xrt_hash64", "xl6_xrt_hash64_string", xl6StdHashStringParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64WithSeed", "__xl_std_xrt_hash64_seed", "xl6_xrt_hash64_string_seed", xl6StdHashStringSeed64Params, 3, "c_ulonglong", NULL},
    {"__xrt_hash", "u32Bytes", "__xl_std_xrt_hash32_bytes", "xl6_xrt_hash32_bytes", xl6StdHashBytesParams, 2, "c_uint", NULL},
    {"__xrt_hash", "u32BytesWithSeed", "__xl_std_xrt_hash32_bytes_seed", "xl6_xrt_hash32_bytes_seed", xl6StdHashBytesSeed32Params, 3, "c_uint", NULL},
    {"__xrt_hash", "u64Bytes", "__xl_std_xrt_hash64_bytes", "xl6_xrt_hash64_bytes", xl6StdHashBytesParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64BytesWithSeed", "__xl_std_xrt_hash64_bytes_seed", "xl6_xrt_hash64_bytes_seed", xl6StdHashBytesSeed64Params, 3, "c_ulonglong", NULL},
    {"__xrt_hash", "u32Raw", "__xl_std_xrt_hash32_raw", "xl6_xrt_hash32_raw", xl6StdHashRawParams, 3, "c_uint", NULL},
    {"__xrt_hash", "u32RawWithSeed", "__xl_std_xrt_hash32_raw_seed", "xl6_xrt_hash32_raw_seed", xl6StdHashRawSeed32Params, 4, "c_uint", NULL},
    {"__xrt_hash", "u64Raw", "__xl_std_xrt_hash64_raw", "xl6_xrt_hash64_raw", xl6StdHashRawParams, 3, "c_ulonglong", NULL},
    {"__xrt_hash", "u64RawWithSeed", "__xl_std_xrt_hash64_raw_seed", "xl6_xrt_hash64_raw_seed", xl6StdHashRawSeed64Params, 4, "c_ulonglong", NULL},
    {"__xrt_siphash", "hash64String", "__xl_std_xrt_siphash64_string", "xl6_xrt_siphash64_string", xl6StdSipHashStringParams, 3, "c_ulonglong", NULL},
    {"__xrt_siphash", "hash64Bytes", "__xl_std_xrt_siphash64_bytes", "xl6_xrt_siphash64_bytes", xl6StdSipHashBytesParams, 3, "c_ulonglong", NULL},
    {"__xrt_siphash", "hash64Raw", "__xl_std_xrt_siphash64_raw", "xl6_xrt_siphash64_raw", xl6StdSipHashRawParams, 4, "c_ulonglong", NULL},
    {"__xrt_id", "toString", "__xl_std_xrt_xid_to_string", "xl6_xrt_xid_to_string", xl6StdXidValueParams, 1, "string", NULL},
    {"__xrt_id", "toBytes", "__xl_std_xrt_xid_to_bytes", "xl6_xrt_xid_to_bytes", xl6StdXidValueParams, 1, "bytes", NULL},
    {"__xrt_id", "timestamp", "__xl_std_xrt_xid_timestamp", "xl6_xrt_xid_timestamp", xl6StdXidValueParams, 1, "int", NULL},
    {"__xrt_id", "compare", "__xl_std_xrt_xid_compare", "xl6_xrt_xid_compare", xl6StdXidPairParams, 2, "int", NULL},
    {"__xrt_id", "equal", "__xl_std_xrt_xid_equal", "xl6_xrt_xid_equal", xl6StdXidPairParams, 2, "bool", NULL},
    {"__xrt_id", "isZero", "__xl_std_xrt_xid_is_zero", "xl6_xrt_xid_is_zero", xl6StdXidValueParams, 1, "bool", NULL},
    {"__xrt_udp", "bind", "__xl_std_xrt_udp_bind", "xl6_xrt_udp_bind", xl6StdUdpBindParams, 2, "point", NULL},
    {"__xrt_udp", "sendText", "__xl_std_xrt_udp_send_text", "xl6_xrt_udp_send_text", xl6StdUdpSendParams, 4, "c_bool", NULL},
    {"__xrt_udp", "recv", "__xl_std_xrt_udp_recv", "xl6_xrt_udp_recv", xl6StdUdpRecvParams, 2, "point", NULL},
    {"__xrt_udp", "recvAsync", "__xl_std_xrt_udp_recv_async", "xl6_xrt_udp_recv_async", xl6StdNetHandleParams, 1, "future<UdpPacket>", NULL},

    {"", "print", "__xl_std_print", NULL, NULL, 0, "", xl6StdPrintBody},
    {"", "println", "__xl_std_println", NULL, NULL, 0, "", xl6StdPrintlnBody},
    {"", "write", "__xl_std_write", NULL, NULL, 0, "", xl6StdWriteBody},
    {"", "writeln", "__xl_std_writeln", NULL, NULL, 0, "", xl6StdWritelnBody},

    {"__xl_runtime", "consoleFlush", "__xl_std_runtime_console_flush", "xl6_xrt_console_flush", xl6StdConsoleStreamParams, 1, "c_bool", NULL},
    {"console", "write", "__xl_std_console_write", NULL, NULL, 0, "", xl6StdConsoleWriteBody},
    {"console", "writeLine", "__xl_std_console_write_line", NULL, NULL, 0, "", xl6StdConsoleWriteLineBody},
    {"console", "error", "__xl_std_console_error", NULL, NULL, 0, "", xl6StdConsoleErrorBody},
    {"console", "errorLine", "__xl_std_console_error_line", NULL, NULL, 0, "", xl6StdConsoleErrorLineBody},
    {"console", "flush", "__xl_std_console_flush", NULL, NULL, 0, "", xl6StdConsoleFlushBody},
    {"__xl_runtime", "consoleReadLine", "__xl_std_runtime_console_read_line", "xl6_xrt_console_read_line", xl6StdConsoleLimitParams, 1, "string?", NULL},
    {"__xl_runtime", "consoleReadChar", "__xl_std_runtime_console_read_char", "xl6_xrt_console_read_char", NULL, 0, "int", NULL},
    {"__xl_runtime", "consoleIsTerminal", "__xl_std_runtime_console_is_terminal", "xl6_xrt_console_is_terminal", xl6StdConsoleIntStreamParams, 1, "bool", NULL},
    {"__xl_runtime", "consoleSize", "__xl_std_runtime_console_size", "xl6_xrt_console_size", xl6StdConsoleIntStreamParams, 1, "int", NULL},
    {"__xl_runtime", "consoleColor", "__xl_std_runtime_console_color", "xl6_xrt_console_color", xl6StdConsoleIntStreamParams, 1, "int", NULL},
    {"__xl_runtime", "consoleStyled", "__xl_std_runtime_console_styled", "xl6_xrt_console_styled", xl6StdConsoleStyledParams, 5, "bool", NULL},
    {"console", "readLine", "__xl_std_console_readLine", NULL, NULL, 0, "string?", readLineBody},
    {"console", "readChar", "__xl_std_console_readChar", NULL, NULL, 0, "char?", readCharBody},
    {"console", "isTerminal", "__xl_std_console_isTerminal", NULL, NULL, 0, "bool", isTerminalBody},
    {"console", "size", "__xl_std_console_size", NULL, NULL, 0, "(int, int)", sizeBody},
    {"console", "colorMode", "__xl_std_console_colorMode", NULL, NULL, 0, "int", colorModeBody},
    {"console", "writeStyled", "__xl_std_console_writeStyled", NULL, NULL, 0, "", writeStyledBody},
    {"console", "errorStyled", "__xl_std_console_errorStyled", NULL, NULL, 0, "", errorStyledBody},
    {"console", "rgb", "__xl_std_console_rgb", NULL, NULL, 0, "int", rgbBody},
    {"console", "openSession", "__xl_std_console_openSession", NULL, NULL, 0, "ConsoleSession", openSessionBody},

    {"__xl_runtime", "ioReadFile", "__xl_std_runtime_io_read_file", "xl6_xrt_io_read_file", xl6StdIoReadFileParams, 2, "bytes", NULL},
    {"__xl_runtime", "ioWriteFile", "__xl_std_runtime_io_write_file", "xl6_xrt_io_write_file", xl6StdIoWriteFileParams, 4, "bool", NULL},
    {"__xl_runtime", "ioWriteTextFile", "__xl_std_runtime_io_write_text_file", "xl6_xrt_io_write_text_file", xl6StdIoWriteTextFileParams, 4, "bool", NULL},
    {"io", "readFile", "__xl_std_io_read_file", NULL, NULL, 0, "bytes", xl6StdIoReadFileBody},
    {"__xl_runtime", "ioCopy", "__xl_std_runtime_io_copy", "xl6_xrt_io_copy", xl6StdIoCopyParams, 4, "int", NULL},
    {"__xl_runtime", "ioWriteBufferFile", "__xl_std_runtime_io_write_buffer_file", "xl6_xrt_io_write_buffer_file", xl6StdIoWriteBufferFileParams, 4, "bool", NULL},
    {"vfs", "create", "__xl_std_vfs_create", NULL, NULL, 0, "Vfs", xl6StdVfscreateBody},
    {"vfs", "memory", "__xl_std_vfs_memory", NULL, NULL, 0, "MemoryFS", xl6StdVfsmemoryBody},
    {"vfs", "disk", "__xl_std_vfs_disk", NULL, NULL, 0, "DiskFS", xl6StdVfsdiskBody},
    {"vfs", "pack", "__xl_std_vfs_pack", NULL, NULL, 0, "PackFS", xl6StdVfspackBody},
    {"vfs", "packOptions", "__xl_std_vfs_pack_options", NULL, NULL, 0, "PackOptions", xl6StdVfsPackOptionsBody},
    {"vfs", "takePack", "__xl_std_vfs_take_pack", NULL, NULL, 0, "PackFS", xl6StdVfsTakePackBody},
    {"io", "openReader", "__xl_std_io_openReader", NULL, NULL, 0, "IoReader", openReaderBody},
    {"io", "openWriter", "__xl_std_io_openWriter", NULL, NULL, 0, "IoWriter", openWriterBody},
    {"io", "fromBytes", "__xl_std_io_fromBytes", NULL, NULL, 0, "IoReader", fromBytesBody},
    {"io", "fromBuffer", "__xl_std_io_fromBuffer", NULL, NULL, 0, "IoReader", fromBufferBody},
    {"io", "bufferWriter", "__xl_std_io_bufferWriter", NULL, NULL, 0, "IoWriter", bufferWriterBody},
    {"io", "discardWriter", "__xl_std_io_discardWriter", NULL, NULL, 0, "IoWriter", discardWriterBody},
    {"io", "stdin", "__xl_std_io_stdin", NULL, NULL, 0, "IoReader", stdinBody},
    {"io", "stdout", "__xl_std_io_stdout", NULL, NULL, 0, "IoWriter", stdoutBody},
    {"io", "stderr", "__xl_std_io_stderr", NULL, NULL, 0, "IoWriter", stderrBody},
    {"io", "lines", "__xl_std_io_lines", NULL, NULL, 0, "IoLineReader", linesBody},
    {"io", "writeFile", "__xl_std_io_write_file", NULL, NULL, 0, "", write_buffer_fileBody},
    {"io", "copy", "__xl_std_io_copy", NULL, NULL, 0, "int", copyBody},
    {"io", "copy", "__xl_std_io_copy", NULL, NULL, 0, "int", copyProtocolBody},
    {"io", "copyExact", "__xl_std_io_copyExact", NULL, NULL, 0, "int", copyExactBody},
    {"io", "copyExact", "__xl_std_io_copyExact", NULL, NULL, 0, "int", copyExactProtocolBody},
    {"io", "copyUpTo", "__xl_std_io_copyUpTo", NULL, NULL, 0, "int", copyUpToBody},
    {"io", "copyUpTo", "__xl_std_io_copyUpTo", NULL, NULL, 0, "int", copyUpToProtocolBody},
    {"io", "writeFile", "__xl_std_io_write_file", NULL, NULL, 0, "", xl6StdIoWriteFileBody},
    {"io", "writeFile", "__xl_std_io_write_file", NULL, NULL, 0, "", xl6StdIoWriteTextFileBody},

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
    {"math", "round", "__xl_std_math_round", "round", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "roundCurrent", "__xl_std_math_round_current", "nearbyint", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "abs", "__xl_std_math_abs", "fabs", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "abs", "__xl_std_math_abs", NULL, NULL, 0, "int", xl6StdMathAbsIntBody},
    {"math", "abs", "__xl_std_math_abs", NULL, NULL, 0, "int8", xl6StdMathAbsInt8Body},
    {"math", "abs", "__xl_std_math_abs", NULL, NULL, 0, "int16", xl6StdMathAbsInt16Body},
    {"math", "abs", "__xl_std_math_abs", NULL, NULL, 0, "int32", xl6StdMathAbsInt32Body},
    {"math", "sinh", "__xl_std_math_sinh", "sinh", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "cosh", "__xl_std_math_cosh", "cosh", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "tanh", "__xl_std_math_tanh", "tanh", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "asinh", "__xl_std_math_asinh", "asinh", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "acosh", "__xl_std_math_acosh", "acosh", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "atanh", "__xl_std_math_atanh", "atanh", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "erf", "__xl_std_math_erf", "erf", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "erfc", "__xl_std_math_erfc", "erfc", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "gamma", "__xl_std_math_gamma", "tgamma", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "logGamma", "__xl_std_math_log_gamma", "lgamma", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "fma", "__xl_std_math_fma", "fma", xl6StdMathTernaryFloatParams, 3, "float", NULL},
    {"math", "copySign", "__xl_std_math_copy_sign", "copysign", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "nextAfter", "__xl_std_math_next_after", "nextafter", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "remainder", "__xl_std_math_remainder", "remainder", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "positiveDiff", "__xl_std_math_positive_diff", "fdim", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "minNumber", "__xl_std_math_min_number", "fmin", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "maxNumber", "__xl_std_math_max_number", "fmax", xl6StdMathPowParams, 2, "float", NULL},
    {"__crt_math", "ldexpNative", "__xl_std_math_ldexp_native", "ldexp", xl6StdMathScaleNativeParams, 2, "float", NULL},
    {"__crt_math", "scalbnNative", "__xl_std_math_scalbn_native", "scalbn", xl6StdMathScaleNativeParams, 2, "float", NULL},
    {"__crt_math", "ilogbNative", "__xl_std_math_ilogb_native", "ilogb", xl6StdMathUnaryFloatParams, 1, "c_int", NULL},
    {"math", "ldexp", "__xl_std_math_ldexp", NULL, NULL, 0, "float", xl6StdMathLdexpBody},
    {"math", "scaleByPowerOfTwo", "__xl_std_math_scale_by_power_of_two", NULL, NULL, 0, "float", xl6StdMathScaleByPowerOfTwoBody},
    {"math", "logb", "__xl_std_math_logb", "logb", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "ilogb", "__xl_std_math_ilogb", NULL, NULL, 0, "int", xl6StdMathIlogbBody},
    {"math", "divRem", "__xl_std_math_div_rem", NULL, NULL, 0, "(int, int)", xl6StdMathDivRemBody},
    {"math", "frexp", "__xl_std_math_frexp", NULL, NULL, 0, "(float, int)", xl6StdMathFrexpBody},
    {"math", "modf", "__xl_std_math_modf", NULL, NULL, 0, "(float, float)", xl6StdMathModfBody},
    {"math", "remQuo", "__xl_std_math_rem_quo", NULL, NULL, 0, "(float, int)", xl6StdMathRemQuoBody},
    {"math", "signBit", "__xl_std_math_sign_bit", NULL, NULL, 0, "bool", xl6StdMathSignBitBody},
    {"math", "isNormal", "__xl_std_math_is_normal", NULL, NULL, 0, "bool", xl6StdMathIsNormalBody},
    {"math", "isSubnormal", "__xl_std_math_is_subnormal", NULL, NULL, 0, "bool", xl6StdMathIsSubnormalBody},
    {"math", "classify", "__xl_std_math_classify", NULL, NULL, 0, "FloatClass", xl6StdMathClassifyBody},
    {"math", "isUnordered", "__xl_std_math_is_unordered", NULL, NULL, 0, "bool", xl6StdMathIsUnorderedBody},
    {"array", "sort", "__xl_std_array_sort", NULL, NULL, 0, "", xl6StdArraySortBody},
    {"array", "sortBy", "__xl_std_array_sort_by", NULL, NULL, 0, "", xl6StdArraySortByBody},
    {"array", "binarySearch", "__xl_std_array_binary_search", NULL, NULL, 0, "int", xl6StdArrayBinarySearchBody},
    {"ascii", "isAlpha", "__xl_std_ascii_is_alpha", NULL, NULL, 0, "bool", xl6StdAsciiIsAlphaBody},
    {"ascii", "isDigit", "__xl_std_ascii_is_digit", NULL, NULL, 0, "bool", xl6StdAsciiIsDigitBody},
    {"ascii", "isAlphaNumeric", "__xl_std_ascii_is_alphanumeric", NULL, NULL, 0, "bool", xl6StdAsciiIsAlphaNumericBody},
    {"ascii", "isHexDigit", "__xl_std_ascii_is_hex_digit", NULL, NULL, 0, "bool", xl6StdAsciiIsHexDigitBody},
    {"ascii", "isWhitespace", "__xl_std_ascii_is_whitespace", NULL, NULL, 0, "bool", xl6StdAsciiIsWhitespaceBody},
    {"ascii", "isBlank", "__xl_std_ascii_is_blank", NULL, NULL, 0, "bool", xl6StdAsciiIsBlankBody},
    {"ascii", "isControl", "__xl_std_ascii_is_control", NULL, NULL, 0, "bool", xl6StdAsciiIsControlBody},
    {"ascii", "isPrintable", "__xl_std_ascii_is_printable", NULL, NULL, 0, "bool", xl6StdAsciiIsPrintableBody},
    {"ascii", "isGraphical", "__xl_std_ascii_is_graphical", NULL, NULL, 0, "bool", xl6StdAsciiIsGraphicalBody},
    {"ascii", "isPunctuation", "__xl_std_ascii_is_punctuation", NULL, NULL, 0, "bool", xl6StdAsciiIsPunctuationBody},
    {"ascii", "isLower", "__xl_std_ascii_is_lower", NULL, NULL, 0, "bool", xl6StdAsciiIsLowerBody},
    {"ascii", "isUpper", "__xl_std_ascii_is_upper", NULL, NULL, 0, "bool", xl6StdAsciiIsUpperBody},
    {"ascii", "toLower", "__xl_std_ascii_to_lower", NULL, NULL, 0, "char", xl6StdAsciiToLowerBody},
    {"ascii", "toUpper", "__xl_std_ascii_to_upper", NULL, NULL, 0, "char", xl6StdAsciiToUpperBody},
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
    {"math", "isClose", "__xl_std_math_is_close", "xrtMathNear", xl6StdMathIsCloseParams, 4, "bool", NULL},
    {"math", "intNear", "__xl_std_math_int_near", "xrtMathIntNear", xl6StdMathIntNearParams, 3, "bool", NULL},

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
    {"string", "glob", "__xl_std_string_glob", NULL, NULL, 0, "bool", xl6StdStringGlobBody},
    {"string", "distance", "__xl_std_string_distance", NULL, NULL, 0, "int", xl6StdStringDistanceBody},
    {"int", "format", "__xl_std_int_format", NULL, NULL, 0, "string", xl6StdIntFormatBody},
    {"int", "format", "__xl_std_int_format", NULL, NULL, 0, "string", xl6StdIntFormatUIntBody},
    {"float", "format", "__xl_std_float_format", NULL, NULL, 0, "string", xl6StdFloatFormatBody},
    {"int", "toString", "__xl_std_int_to_string", NULL, NULL, 0, "string", xl6StdIntStringBody},
    {"int", "toString", "__xl_std_int_to_string", NULL, NULL, 0, "string", xl6StdUIntStringBody},
    {"float", "toString", "__xl_std_float_to_string", NULL, NULL, 0, "string", xl6StdFloatStringBody},
    {"int", "parse", "__xl_std_int_parse", NULL, NULL, 0, "int", xl6StdIntParseBody},
    {"int", "tryParse", "__xl_std_int_try_parse", NULL, NULL, 0, "(bool, int)", xl6StdIntTryParseBody},
    {"uint64", "parse", "__xl_std_uint64_parse", NULL, NULL, 0, "uint64", xl6StdUInt64ParseBody},
    {"uint64", "tryParse", "__xl_std_uint64_try_parse", NULL, NULL, 0, "(bool, uint64)", xl6StdUInt64TryParseBody},
    {"float", "parse", "__xl_std_float_parse", NULL, NULL, 0, "float", xl6StdFloatParseBody},
    {"float", "tryParse", "__xl_std_float_try_parse", NULL, NULL, 0, "(bool, float)", xl6StdFloatTryParseBody},
    {"int8", "parse", "__xl_std_int8_parse", NULL, NULL, 0, "int8", xl6StdInt8ParseBody},
    {"int8", "tryParse", "__xl_std_int8_try_parse", NULL, NULL, 0, "(bool, int8)", xl6StdInt8TryParseBody},
    {"int16", "parse", "__xl_std_int16_parse", NULL, NULL, 0, "int16", xl6StdInt16ParseBody},
    {"int16", "tryParse", "__xl_std_int16_try_parse", NULL, NULL, 0, "(bool, int16)", xl6StdInt16TryParseBody},
    {"int32", "parse", "__xl_std_int32_parse", NULL, NULL, 0, "int32", xl6StdInt32ParseBody},
    {"int32", "tryParse", "__xl_std_int32_try_parse", NULL, NULL, 0, "(bool, int32)", xl6StdInt32TryParseBody},
    {"int64", "parse", "__xl_std_int64_parse", NULL, NULL, 0, "int64", xl6StdInt64ParseBody},
    {"int64", "tryParse", "__xl_std_int64_try_parse", NULL, NULL, 0, "(bool, int64)", xl6StdInt64TryParseBody},
    {"uint8", "parse", "__xl_std_uint8_parse", NULL, NULL, 0, "uint8", xl6StdUInt8ParseBody},
    {"uint8", "tryParse", "__xl_std_uint8_try_parse", NULL, NULL, 0, "(bool, uint8)", xl6StdUInt8TryParseBody},
    {"uint16", "parse", "__xl_std_uint16_parse", NULL, NULL, 0, "uint16", xl6StdUInt16ParseBody},
    {"uint16", "tryParse", "__xl_std_uint16_try_parse", NULL, NULL, 0, "(bool, uint16)", xl6StdUInt16TryParseBody},
    {"uint32", "parse", "__xl_std_uint32_parse", NULL, NULL, 0, "uint32", xl6StdUInt32ParseBody},
    {"uint32", "tryParse", "__xl_std_uint32_try_parse", NULL, NULL, 0, "(bool, uint32)", xl6StdUInt32TryParseBody},
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
    {"string", "split", "__xl_std_string_split", NULL, NULL, 0, "array<string>", xl6StdStringSplitBody},
    {"string", "slice", "__xl_std_string_slice", NULL, NULL, 0, "string", xl6StdStringSliceBody},
    {"string", "left", "__xl_std_string_left", NULL, NULL, 0, "string", xl6StdStringLeftBody},
    {"string", "right", "__xl_std_string_right", NULL, NULL, 0, "string", xl6StdStringRightBody},
    {"string", "mid", "__xl_std_string_mid", NULL, NULL, 0, "string", xl6StdStringMidBody},
    {"string", "charAt", "__xl_std_string_char_at", NULL, NULL, 0, "char", xl6StdStringCharAtBody},
    {"string", "byteAt", "__xl_std_string_byte_at", NULL, NULL, 0, "uint8", xl6StdStringByteAtBody},
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
    {"string", "splitLines", "__xl_std_string_split_lines", NULL, NULL, 0, "array<string>", xl6StdStringSplitLinesBody},
    {"string", "fields", "__xl_std_string_fields", NULL, NULL, 0, "array<string>", xl6StdStringFieldsBody},
    {"string", "splitIter", "__xl_std_string_splitIter", NULL, NULL, 0, "StringIterator", xl6StdStringSplitIterBody},
    {"string", "linesIter", "__xl_std_string_linesIter", NULL, NULL, 0, "StringIterator", xl6StdStringLinesIterBody},
    {"string", "fieldsIter", "__xl_std_string_fieldsIter", NULL, NULL, 0, "StringIterator", xl6StdStringFieldsIterBody},
    {"string", "chars", "__xl_std_string_chars", NULL, NULL, 0, "array<char>", xl6StdStringCharsBody},
    {"string", "bytes", "__xl_std_string_bytes", NULL, NULL, 0, "bytes", xl6StdStringBytesBody},

    {"time", "now", "__xl_std_time_now", NULL, NULL, 0, "time", xl6StdTimeNowBody},
    {"time", "split", "__xl_std_time_split", NULL, NULL, 0, "DateTime", xl6StdTimeSplitBody},
    {"time", "local", "__xl_std_time_local", NULL, NULL, 0, "DateTime", xl6StdTimeLocalBody},
    {"time", "fromLocal", "__xl_std_time_from_local", NULL, NULL, 0, "time", xl6StdTimeFromLocalBody},
    {"time", "monthRange", "__xl_std_time_month_range", NULL, NULL, 0, "(time, time)", xl6StdTimeMonthRangeBody},
    {"time", "yearRange", "__xl_std_time_year_range", NULL, NULL, 0, "(time, time)", xl6StdTimeYearRangeBody},
    {"time", "weekRange", "__xl_std_time_week_range", NULL, NULL, 0, "(time, time)", xl6StdTimeWeekRangeBody},
    {"time", "isoWeek", "__xl_std_time_iso_week", NULL, NULL, 0, "(int, int, int)", xl6StdTimeISOWeekBody},
    {"time", "today", "__xl_std_time_today", NULL, NULL, 0, "time", xl6StdTimeTodayBody},
    {"time", "clock", "__xl_std_time_clock", NULL, NULL, 0, "uint64", xl6StdTimeClockBody},
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
    {"time", "timePart", "__xl_std_time_time_part", NULL, NULL, 0, "int", xl6StdTimeTimePartBody},
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
    {"time", "fromUnixMs", "__xl_std_time_from_unix_ms", NULL, NULL, 0, "time", xl6StdTimeFromUnixMsBody},
    {"time", "toUnixMs", "__xl_std_time_to_unix_ms", NULL, NULL, 0, "int", xl6StdTimeToUnixMsBody},
    {"time", "microsecond", "__xl_std_time_microsecond", NULL, NULL, 0, "int", xl6StdTimeMicrosecondBody},
    {"time", "monotonic", "__xl_std_time_monotonic", NULL, NULL, 0, "uint64", xl6StdTimeMonotonicBody},
    {"time", "sleepMicroseconds", "__xl_std_time_sleep_us", NULL, NULL, 0, "", xl6StdTimeSleepUsBody},
    {"time", "sleepUntil", "__xl_std_time_sleep_until", NULL, NULL, 0, "", xl6StdTimeSleepUntilBody},
    {"time", "addMicroseconds", "__xl_std_time_add_us", NULL, NULL, 0, "time", xl6StdTimeAddUsBody},
    {"time", "addMilliseconds", "__xl_std_time_add_ms", NULL, NULL, 0, "time", xl6StdTimeAddMsBody},
    {"time", "diffMicroseconds", "__xl_std_time_diff_us", NULL, NULL, 0, "int", xl6StdTimeDiffUsBody},
    {"time", "diffMilliseconds", "__xl_std_time_diff_ms", NULL, NULL, 0, "int", xl6StdTimeDiffMsBody},
    {"time", "utcToLocal", "__xl_std_time_utc_to_local", NULL, NULL, 0, "DateTime", xl6StdTimeUtcToLocalBody},
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
    {"time", "parseFields", "__xl_std_time_parse_fields", NULL, NULL, 0, "DateTime", xl6StdTimeParseFieldsBody},
    {"time", "rfc3339", "__xl_std_time_rfc3339", NULL, NULL, 0, "string", xl6StdTimeRFC3339Body},
    {"time", "httpDate", "__xl_std_time_http_date", NULL, NULL, 0, "string", xl6StdTimeHTTPDateBody},
    {"time", "parseRFC3339", "__xl_std_time_parse_rfc3339", NULL, NULL, 0, "time", xl6StdTimeParseRFC3339Body},
    {"time", "tryParseRFC3339", "__xl_std_time_try_parse_rfc3339", NULL, NULL, 0, "(bool,time)", xl6StdTimeTryParseRFC3339Body},
    {"time", "parseHTTPDate", "__xl_std_time_parse_http_date", NULL, NULL, 0, "time", xl6StdTimeParseHTTPDateBody},
    {"time", "tryParseHTTPDate", "__xl_std_time_try_parse_http_date", NULL, NULL, 0, "(bool,time)", xl6StdTimeTryParseHTTPDateBody},

    {"random", "u32", "__xl_std_random_u32", NULL, NULL, 0, "uint32", xl6StdRandomU32Body},
    {"random", "u64", "__xl_std_random_u64", NULL, NULL, 0, "uint64", xl6StdRandomU64Body},
    {"random", "below", "__xl_std_random_below", NULL, NULL, 0, "uint64", xl6StdRandomBelowBody},
    {"random", "range", "__xl_std_random_range", NULL, NULL, 0, "int", xl6StdRandomRangeBody},
    {"random", "rangeClosed", "__xl_std_random_range_closed", NULL, NULL, 0, "int", xl6StdRandomRangeClosedBody},
    {"random", "real", "__xl_std_random_real", NULL, NULL, 0, "float", xl6StdRandomRealBody},
    {"random", "bytes", "__xl_std_random_bytes", NULL, NULL, 0, "bytes", xl6StdRandomBytesBody},
    {"random", "fillBytes", "__xl_std_random_fill_bytes", NULL, NULL, 0, "", xl6StdRandomFillBytesBody},
    {"random", "shuffle", "__xl_std_random_shuffle", NULL, NULL, 0, "", xl6StdRandomShuffleBody},
    {"random", "shuffleBuffer", "__xl_std_random_shuffle_buffer", NULL, NULL, 0, "", xl6StdRandomShuffleBufferBody},
    {"random", "seed", "__xl_std_random_seed", NULL, NULL, 0, "", xl6StdRandomSeedBody},
    {"random", "string", "__xl_std_random_string", NULL, NULL, 0, "string", xl6StdRandomStringBody},

    {"path", "exists", "__xl_std_path_exists", NULL, NULL, 0, "bool", xl6StdPathExistsBody},
    {"path", "isRoot", "__xl_std_path_is_root", NULL, NULL, 0, "bool", xl6StdFsPathIsRootBody},
    {"path", "isRooted", "__xl_std_path_is_rooted", NULL, NULL, 0, "bool", xl6StdFsPathIsRootedBody},
    {"path", "stem", "__xl_std_path_stem", NULL, NULL, 0, "string", xl6StdFsPathStemBody},
    {"path", "parent", "__xl_std_path_parent", NULL, NULL, 0, "string", xl6StdFsPathParentBody},
    {"path", "real", "__xl_std_path_real", NULL, NULL, 0, "string", xl6StdFsPathRealBody},
    {"path", "isLocal", "__xl_std_path_is_local", NULL, NULL, 0, "bool", xl6StdFsPathIsLocalBody},
    {"path", "isSafeEntry", "__xl_std_path_is_safe", NULL, NULL, 0, "bool", xl6StdFsPathIsSafeBody},
    {"path", "clean", "__xl_std_path_clean", NULL, NULL, 0, "string", xl6StdFsPathCleanBody},
    {"path", "relative", "__xl_std_path_relative", NULL, NULL, 0, "string", xl6StdFsPathRelativeBody},
    {"path", "build", "__xl_std_path_build", NULL, NULL, 0, "string", xl6StdFsPathBuildBody},
    {"path", "listSep", "__xl_std_path_list_sep", NULL, NULL, 0, "string", xl6StdFsPathListSepBody},
    {"path", "executable", "__xl_std_path_executable", NULL, NULL, 0, "string", xl6StdFsPathExecutableBody},
    {"path", "parse", "__xl_std_path_parse", NULL, NULL, 0, "PathParts", xl6StdFsPathparseBody},
    {"path", "components", "__xl_std_path_components", NULL, NULL, 0, "PathIterator", xl6StdFsPathcomponentsBody},
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

    {"os", "env", "__xl_std_os_env", NULL, NULL, 0, "string", xl6StdOsEnvBody},
    {"env", "get", "__xl_std_runtime_env_get", NULL, NULL, 0, "string?", xl6StdEnvGetBody},
    {"env", "has", "__xl_std_runtime_env_has", NULL, NULL, 0, "bool", xl6StdEnvHasBody},
    {"env", "set", "__xl_std_runtime_env_set", NULL, NULL, 0, "", xl6StdEnvSetBody},
    {"env", "remove", "__xl_std_runtime_env_remove", NULL, NULL, 0, "", xl6StdEnvRemoveBody},
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
    {"file", "size", "__xl_std_file_size", NULL, NULL, 0, "uint64", xl6StdFileSizeBody},
    {"file", "setSize", "__xl_std_file_set_size", NULL, NULL, 0, "bool", xl6StdFileSetSizeBody},
    {"file", "attr", "__xl_std_file_attr", NULL, NULL, 0, "int", xl6StdFileAttrBody},
    {"file", "setAttr", "__xl_std_file_set_attr", NULL, NULL, 0, "bool", xl6StdFileSetAttrBody},
    {"file", "accessTime", "__xl_std_file_access_time", NULL, NULL, 0, "time", xl6StdFileAccessTimeBody},
    {"file", "changeTime", "__xl_std_file_change_time", NULL, NULL, 0, "time", xl6StdFileChangeTimeBody},
    {"file", "readText", "__xl_std_file_read_text", NULL, NULL, 0, "string", xl6StdFileReadTextBody},
    {"file", "readBytes", "__xl_std_file_read_bytes", NULL, NULL, 0, "bytes", xl6StdFileReadBytesBody},
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
    {"file", "info", "__xl_std_file_info", NULL, NULL, 0, "FileStat", xl6StdFileInfoBody},
    {"file", "open", "__xl_std_file_open", NULL, NULL, 0, "File", xl6StdFileOpenBody},
    {"file", "stat", "__xl_std_file_stat", NULL, NULL, 0, "FileStat", xl6StdFsStatBody},
    {"file", "root", "__xl_std_file_root", NULL, NULL, 0, "FileRoot", xl6StdFsRootBody},
    {"file", "writeBytesAtomic", "__xl_std_file_write_bytes_atomic", NULL, NULL, 0, "bool", xl6StdFsfilewrite_bytes_atomicBody},
    {"file", "readLink", "__xl_std_file_read_link", NULL, NULL, 0, "string", xl6StdFsfileread_linkBody},
    {"file", "deleteLink", "__xl_std_file_delete_link", NULL, NULL, 0, "bool", xl6StdFsfiledelete_linkBody},
    {"dir", "remove", "__xl_std_dir_remove", NULL, NULL, 0, "bool", xl6StdFsdirremoveBody},
    {"file", "setMode", "__xl_std_file_set_mode", NULL, NULL, 0, "bool", xl6StdFsfileset_modeBody},
    {"file", "createLink", "__xl_std_file_create_link", NULL, NULL, 0, "bool", xl6StdFsfilecreate_linkBody},
    {"file", "createHardLink", "__xl_std_file_create_hard_link", NULL, NULL, 0, "bool", xl6StdFsfilecreate_hard_linkBody},
    {"file", "createFifo", "__xl_std_file_create_fifo", NULL, NULL, 0, "bool", xl6StdFsfilecreate_fifoBody},
    {"dir", "temp", "__xl_std_dir_temp", NULL, NULL, 0, "string", xl6StdFsdirtempBody},
    {"file", "temp", "__xl_std_file_temp", NULL, NULL, 0, "File", xl6StdFs_file_tempBody},
    {"dir", "roots", "__xl_std_dir_roots", NULL, NULL, 0, "array<string>", xl6StdFs_dir_rootsBody},
    {"file", "setTimes", "__xl_std_file_set_times", NULL, NULL, 0, "bool", xl6StdFs_file_set_timesBody},
    {"file", "walkStats", "__xl_std_file_walk_stats", NULL, NULL, 0, "WalkStats", xl6StdFs_file_walk_statsBody},
    {"file", "walk", "__xl_std_file_walk", NULL, NULL, 0, "WalkStats", xl6StdFsWalkBody},
    {"dir", "stats", "__xl_std_dir_stats", NULL, NULL, 0, "WalkStats", xl6StdFs_dir_statsBody},
    {"file", "copyTree", "__xl_std_file_copy_tree", NULL, NULL, 0, "TreeStats", xl6StdFs_file_copy_treeBody},
    {"file", "removeTree", "__xl_std_file_remove_tree", NULL, NULL, 0, "TreeStats", xl6StdFs_file_remove_treeBody},
    {"dir", "open", "__xl_std_dir_open", NULL, NULL, 0, "DirectoryIterator", xl6StdFsDirectoryBody},
    {"file", "readTextAsync", "__xl_std_file_read_text_async", NULL, NULL, 0, "future<string>", xl6StdFsAsync_file_readTextAsyncBody},
    {"file", "readBytesAsync", "__xl_std_file_read_bytes_async", NULL, NULL, 0, "future<bytes>", xl6StdFsAsync_file_readBytesAsyncBody},
    {"file", "writeTextAsync", "__xl_std_file_write_text_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_writeTextAsyncBody},
    {"file", "writeBytesAsync", "__xl_std_file_write_bytes_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_writeBytesAsyncBody},
    {"file", "appendTextAsync", "__xl_std_file_append_text_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_appendTextAsyncBody},
    {"file", "copyAsync", "__xl_std_file_copy_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_copyAsyncBody},
    {"file", "moveAsync", "__xl_std_file_move_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_moveAsyncBody},
    {"file", "deleteAsync", "__xl_std_file_delete_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_file_deleteAsyncBody},
    {"file", "openAsync", "__xl_std_file_open_async", NULL, NULL, 0, "AsyncFile", xl6StdFsAsync_file_openAsyncBody},

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
    {"dir", "count", "__xl_std_dir_count", NULL, NULL, 0, "uint64", xl6StdDirCountBody},
    {"dir", "size", "__xl_std_dir_size", NULL, NULL, 0, "uint64", xl6StdDirSizeBody},
    {"dir", "clean", "__xl_std_dir_clean", NULL, NULL, 0, "bool", xl6StdDirCleanBody},
    {"dir", "ensureEmpty", "__xl_std_dir_ensure_empty", NULL, NULL, 0, "bool", xl6StdDirEnsureEmptyBody},
    {"dir", "glob", "__xl_std_dir_glob", NULL, NULL, 0, "array<string>", xl6StdFsDirGlobBody},
    {"dir", "entries", "__xl_std_dir_entries", NULL, NULL, 0, "array<WalkEntry>", xl6StdFsDirentriesBody},
    {"dir", "matchEntries", "__xl_std_dir_match", NULL, NULL, 0, "array<WalkEntry>", xl6StdFsDirmatchBody},
    {"dir", "walk", "__xl_std_dir_walk", NULL, NULL, 0, "array<WalkEntry>", xl6StdFsDirwalkBody},
    {"dir", "createAsync", "__xl_std_dir_create_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_dir_createAsyncBody},
    {"dir", "createAllAsync", "__xl_std_dir_create_all_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_dir_createAllAsyncBody},
    {"dir", "copyAsync", "__xl_std_dir_copy_async", NULL, NULL, 0, "future<TreeStats>", xl6StdFsAsync_dir_copyAsyncBody},
    {"dir", "moveAsync", "__xl_std_dir_move_async", NULL, NULL, 0, "future<bool>", xl6StdFsAsync_dir_moveAsyncBody},
    {"dir", "deleteAsync", "__xl_std_dir_delete_async", NULL, NULL, 0, "future<TreeStats>", xl6StdFsAsync_dir_deleteAsyncBody},

    {"json", "parse", "__xl_std_json_parse", NULL, NULL, 0, "var", xl6StdJsonParseBody},
    {"json", "valid", "__xl_std_json_valid", NULL, NULL, 0, "bool", xl6StdJsonValidBody},
    {"json", "read", "__xl_std_json_read", NULL, NULL, 0, "var", xl6StdJsonReadBody},
    {"json", "stringify", "__xl_std_json_stringify", NULL, xl6StdJsonStringifyParams, 2, "string", xl6StdJsonStringifyBody},
    {"json", "write", "__xl_std_json_write", NULL, xl6StdJsonWriteParams, 3, "bool", xl6StdJsonWriteBody},
    {"json", "writer", "__xl_std_json_writer", NULL, NULL, 0, "JsonWriter", xl6StdJsonWriterBody},
    {"json", "visit", "__xl_std_json_visit", NULL, NULL, 0, "int", xl6StdJsonVisitBody},
    {"json", "parse", "__xl_std_json_parse", "xl6_xrt_json_read_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"json", "read", "__xl_std_json_read", "xl6_xrt_json_read_file_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"json", "stringify", "__xl_std_json_stringify", NULL, NULL, 0, "string", xl6StdJsonStringifyConfiguredBody},
    {"json", "write", "__xl_std_json_write", "xl6_xrt_json_write_file_configured", xl6StdConfiguredWriteParams, 3, "bool", NULL},
    {"json", "writeTo", "__xl_std_json_write_to", NULL, NULL, 0, "bool", xl6StdJsonWriteToBody},
    {"json", "quote", "__xl_std_json_quote", NULL, NULL, 0, "string", xl6StdJsonQuoteBody},
    {"json", "quoteTo", "__xl_std_json_quote_to", NULL, NULL, 0, "bool", xl6StdJsonQuoteToBody},
    {"json", "visit", "__xl_std_json_visit", NULL, NULL, 0, "int", xl6StdJsonVisitConfiguredBody},
    {"json", "valid", "__xl_std_json_valid", NULL, NULL, 0, "bool", xl6StdJsonValidConfiguredBody},

    {"jsonl", "parse", "__xl_std_jsonl_parse", "xl6_xrt_jsonl_parse", xl6StdXrtJsonParseParams, 1, "var", NULL},
    {"jsonl", "valid", "__xl_std_jsonl_valid", "xl6_xrt_jsonl_valid", xl6StdXrtJsonParseParams, 1, "c_bool", NULL},
    {"jsonl", "read", "__xl_std_jsonl_read", "xl6_xrt_jsonl_read_file", xl6StdPathUnaryParams, 1, "var", NULL},
    {"jsonl", "stringify", "__xl_std_jsonl_stringify", NULL, NULL, 0, "string", xl6StdJsonlStringifyBody},
    {"jsonl", "write", "__xl_std_jsonl_write", "xl6_xrt_jsonl_write_file", xl6StdLinesWriteParams, 2, "bool", NULL},
    {"jsonl", "parse", "__xl_std_jsonl_parse", "xl6_xrt_jsonl_read_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"jsonl", "read", "__xl_std_jsonl_read", "xl6_xrt_jsonl_read_file_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"jsonl", "stringify", "__xl_std_jsonl_stringify", NULL, NULL, 0, "string", xl6StdJsonlStringifyConfiguredBody},
    {"jsonl", "write", "__xl_std_jsonl_write", "xl6_xrt_jsonl_write_file_configured", xl6StdConfiguredWriteParams, 3, "bool", NULL},
    {"jsonl", "writeTo", "__xl_std_jsonl_write_to", NULL, NULL, 0, "bool", xl6StdJsonlWriteToBody},
    {"jsonl", "valid", "__xl_std_jsonl_valid", NULL, NULL, 0, "bool", xl6StdJsonlValidConfiguredBody},

    {"xlonl", "parse", "__xl_std_xlonl_parse", "xl6_xrt_xlonl_parse", xl6StdXrtJsonParseParams, 1, "var", NULL},
    {"xlonl", "valid", "__xl_std_xlonl_valid", "xl6_xrt_xlonl_valid", xl6StdXrtJsonParseParams, 1, "c_bool", NULL},
    {"xlonl", "read", "__xl_std_xlonl_read", "xl6_xrt_xlonl_read_file", xl6StdPathUnaryParams, 1, "var", NULL},
    {"xlonl", "stringify", "__xl_std_xlonl_stringify", NULL, NULL, 0, "string", xl6StdXlonlStringifyBody},
    {"xlonl", "write", "__xl_std_xlonl_write", "xl6_xrt_xlonl_write_file", xl6StdLinesWriteParams, 2, "bool", NULL},
    {"xlonl", "parse", "__xl_std_xlonl_parse", "xl6_xrt_xlonl_read_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"xlonl", "read", "__xl_std_xlonl_read", "xl6_xrt_xlonl_read_file_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"xlonl", "stringify", "__xl_std_xlonl_stringify", NULL, NULL, 0, "string", xl6StdXlonlStringifyConfiguredBody},
    {"xlonl", "write", "__xl_std_xlonl_write", "xl6_xrt_xlonl_write_file_configured", xl6StdConfiguredWriteParams, 3, "bool", NULL},
    {"xlonl", "writeTo", "__xl_std_xlonl_write_to", NULL, NULL, 0, "bool", xl6StdXlonlWriteToBody},
    {"xlonl", "valid", "__xl_std_xlonl_valid", NULL, NULL, 0, "bool", xl6StdXlonlValidConfiguredBody},

    {"dict", "remove", "__xl_std_dict_remove", NULL, xl6StdDictRemoveParams, 2, "bool", xl6StdDictRemoveBody},

    {"xlon", "parse", "__xl_std_xlon_parse", NULL, NULL, 0, "var", xl6StdXlonParseBody},
    {"xlon", "parse", "__xl_std_xlon_parse", NULL, NULL, 0, "var", xl6StdXlonParseOptionsBody},
    {"xlon", "valid", "__xl_std_xlon_valid", NULL, NULL, 0, "bool", xl6StdXlonValidBody},
    {"xlon", "valid", "__xl_std_xlon_valid", NULL, NULL, 0, "bool", xl6StdXlonValidOptionsBody},
    {"xlon", "read", "__xl_std_xlon_read", NULL, NULL, 0, "var", xl6StdXlonReadBody},
    {"xlon", "read", "__xl_std_xlon_read", NULL, NULL, 0, "var", xl6StdXlonReadOptionsBody},
    {"xlon", "stringify", "__xl_std_xlon_stringify", NULL, NULL, 0, "string", xl6StdXlonStringifyBody},
    {"xlon", "stringify", "__xl_std_xlon_stringify", NULL, NULL, 0, "string", xl6StdXlonStringifyOptionsBody},
    {"xlon", "write", "__xl_std_xlon_write", NULL, NULL, 0, "bool", xl6StdXlonWriteBody},
    {"xlon", "write", "__xl_std_xlon_write", NULL, NULL, 0, "bool", xl6StdXlonWriteOptionsBody},
    {"xlon", "writer", "__xl_std_xlon_writer", NULL, NULL, 0, "XlonWriter", xl6StdXlonWriterBody},
    {"xlon", "writer", "__xl_std_xlon_writer", NULL, NULL, 0, "XlonWriter", xl6StdXlonWriterOptionsBody},
    {"xlon", "visit", "__xl_std_xlon_visit", NULL, NULL, 0, "int", xl6StdXlonVisitBody},
    {"xlon", "visit", "__xl_std_xlon_visit", NULL, NULL, 0, "int", xl6StdXlonVisitOptionsBody},
    {"xlon", "options", "__xl_std_xlon_options", NULL, NULL, 0, "XlonOptions", xl6StdXlonOptionsBody},
    {"xlon", "parseWith", "__xl_std_xlon_parse_with", NULL, NULL, 0, "var", xl6StdXlonParseWithBody},
    {"xlon", "parse", "__xl_std_xlon_parse", "xl6_xrt_xlon_read_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"xlon", "read", "__xl_std_xlon_read", "xl6_xrt_xlon_read_file_configured", xl6StdConfiguredReadParams, 2, "var", NULL},
    {"xlon", "stringify", "__xl_std_xlon_stringify", NULL, NULL, 0, "string", xl6StdXlonStringifyConfiguredBody},
    {"xlon", "write", "__xl_std_xlon_write", "xl6_xrt_xlon_write_file_configured", xl6StdConfiguredWriteParams, 3, "bool", NULL},
    {"xlon", "writeTo", "__xl_std_xlon_write_to", NULL, NULL, 0, "bool", xl6StdXlonWriteToBody},
    {"xlon", "valid", "__xl_std_xlon_valid", NULL, NULL, 0, "bool", xl6StdXlonValidConfiguredBody},
    {"xlon", "visit", "__xl_std_xlon_visit", NULL, NULL, 0, "int", xl6StdXlonVisitConfiguredBody},

    {"__crc32", "text", "__xl_crc32_text", "xl6Crc32StringV1", xl6StdCrc32TextParams, 3, "uint32", NULL},
    {"__crc32", "bytes", "__xl_crc32_bytes", "xl6Crc32BytesV1", xl6StdCrc32BytesParams, 3, "uint32", NULL},
    {"__crc32", "buffer", "__xl_crc32_buffer", "xl6Crc32BufferV1", xl6StdCrc32BufferParams, 3, "uint32", NULL},
    {"crc32", "checksum", "__xl_std_crc32_checksum", NULL, NULL, 0, "uint32", xl6StdCrc32TextBody},
    {"crc32", "checksum", "__xl_std_crc32_checksum", NULL, NULL, 0, "uint32", xl6StdCrc32BytesBody},
    {"crc32", "checksumBuffer", "__xl_std_crc32_checksum_buffer", NULL, NULL, 0, "uint32", xl6StdCrc32BufferBody},
    {"crc32", "update", "__xl_std_crc32_update", NULL, NULL, 0, "uint32", xl6StdCrc32UpdateTextBody},
    {"crc32", "update", "__xl_std_crc32_update", NULL, NULL, 0, "uint32", xl6StdCrc32UpdateBytesBody},
    {"crc32", "updateBuffer", "__xl_std_crc32_update_buffer", NULL, NULL, 0, "uint32", xl6StdCrc32UpdateBufferBody},
    {"codec", "hexEncode", "__xl_std_codec_hex_encode", NULL, NULL, 0, "string", xl6StdCodecHexEncodeBody},
    {"compress", "encode", "__xl_std_compress_encode", NULL, NULL, 0, "bytes", xl6StdCompressEncodeBody},
    {"compress", "encode", "__xl_std_compress_encode", NULL, NULL, 0, "bytes", xl6StdCompressEncodeBytesBody},
    {"compress", "encodeBuffer", "__xl_std_compress_encodeBuffer", NULL, NULL, 0, "bytes", xl6StdCompressEncodeBufferBody},
    {"compress", "decode", "__xl_std_compress_decode", NULL, NULL, 0, "bytes", xl6StdCompressDecodeBody},
    {"compress", "decodeBuffer", "__xl_std_compress_decodeBuffer", NULL, NULL, 0, "bytes", xl6StdCompressDecodeBufferBody},
    {"compress", "deflater", "__xl_std_compress_deflater", NULL, NULL, 0, "Deflater", xl6StdCompressDeflaterBody},
    {"compress", "inflater", "__xl_std_compress_inflater", NULL, NULL, 0, "Inflater", xl6StdCompressInflaterBody},
    {"codec", "hexEncode", "__xl_std_codec_hex_encode", NULL, NULL, 0, "string", xl6StdCodecHexEncodeValueBody},
    {"codec", "hexDecode", "__xl_std_codec_hex_decode", NULL, NULL, 0, "bytes", xl6StdCodecHexDecodeBody},
    {"codec", "base64Encode", "__xl_std_codec_base64_encode", NULL, NULL, 0, "string", xl6StdCodecBase64EncodeBody},
    {"codec", "base64Encode", "__xl_std_codec_base64_encode", NULL, NULL, 0, "string", xl6StdCodecBase64EncodeValueBody},
    {"codec", "base64Decode", "__xl_std_codec_base64_decode", NULL, NULL, 0, "bytes", xl6StdCodecBase64DecodeBody},
    {"codec", "percentEncode", "__xl_std_codec_percent_encode", NULL, NULL, 0, "string", xl6StdCodecPercentEncodeBody},
    {"codec", "percentEncode", "__xl_std_codec_percent_encode", NULL, NULL, 0, "string", xl6StdCodecPercentEncodeValueBody},
    {"codec", "percentDecode", "__xl_std_codec_percent_decode", NULL, NULL, 0, "bytes", xl6StdCodecPercentDecodeBody},

    {"encoding", "encode", "__xl_std_encoding_encode", NULL, NULL, 0, "bytes", xl6StdEncodingEncodeBody},
    {"encoding", "decode", "__xl_std_encoding_decode", NULL, NULL, 0, "string", xl6StdEncodingDecodeBody},
    {"encoding", "transcode", "__xl_std_encoding_transcode", NULL, NULL, 0, "bytes", xl6StdEncodingTranscodeBody},
    {"encoding", "bom", "__xl_std_encoding_bom", NULL, NULL, 0, "int", xl6StdEncodingBomBody},
    {"encoding", "bomSize", "__xl_std_encoding_bomSize", NULL, NULL, 0, "int", xl6StdEncodingBomSizeBody},
    {"encoding", "bomBytes", "__xl_std_encoding_bomBytes", NULL, NULL, 0, "bytes", xl6StdEncodingBomBytesBody},
    {"encoding", "unitSize", "__xl_std_encoding_unitSize", NULL, NULL, 0, "int", xl6StdEncodingUnitSizeBody},
    {"encoding", "name", "__xl_std_encoding_name", NULL, NULL, 0, "string", xl6StdEncodingNameBody},
    {"encoding", "encodeUtf8", "__xl_std_encoding_encode_Utf8", NULL, NULL, 0, "bytes", xl6StdEncodingEncodeUtf8Body},
    {"encoding", "decodeUtf8", "__xl_std_encoding_decode_Utf8", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf8Body},
    {"encoding", "encodeUtf16LE", "__xl_std_encoding_encode_Utf16LE", NULL, NULL, 0, "bytes", xl6StdEncodingEncodeUtf16LEBody},
    {"encoding", "decodeUtf16LE", "__xl_std_encoding_decode_Utf16LE", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf16LEBody},
    {"encoding", "encodeUtf16BE", "__xl_std_encoding_encode_Utf16BE", NULL, NULL, 0, "bytes", xl6StdEncodingEncodeUtf16BEBody},
    {"encoding", "decodeUtf16BE", "__xl_std_encoding_decode_Utf16BE", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf16BEBody},
    {"encoding", "encodeUtf32LE", "__xl_std_encoding_encode_Utf32LE", NULL, NULL, 0, "bytes", xl6StdEncodingEncodeUtf32LEBody},
    {"encoding", "decodeUtf32LE", "__xl_std_encoding_decode_Utf32LE", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf32LEBody},
    {"encoding", "encodeUtf32BE", "__xl_std_encoding_encode_Utf32BE", NULL, NULL, 0, "bytes", xl6StdEncodingEncodeUtf32BEBody},
    {"encoding", "decodeUtf32BE", "__xl_std_encoding_decode_Utf32BE", NULL, NULL, 0, "string", xl6StdEncodingDecodeUtf32BEBody},
    {"encoding", "isUtf8", "__xl_std_encoding_is_utf8", NULL, NULL, 0, "bool", xl6StdEncodingIsUtf8Body},
    {"encoding", "isUtf8", "__xl_std_encoding_is_utf8", NULL, NULL, 0, "bool", xl6StdEncodingIsUtf8BytesBody},
    {"unicode", "decode", "__xl_std_unicode_decode", NULL, NULL, 0, "string", xl6StdUnicodeDecodeBody},
    {"unicode", "repair", "__xl_std_unicode_repair", NULL, NULL, 0, "string", xl6StdUnicodeRepairBody},
    {"encoding", "detect", "__xl_std_encoding_detect", NULL, NULL, 0, NULL, xl6StdEncodingDetectTextBody},
    {"encoding", "detect", "__xl_std_encoding_detect", NULL, NULL, 0, NULL, xl6StdEncodingDetectBytesBody},

    {"hash", "u32", "__xl_std_hash_u32", NULL, NULL, 0, "uint32", xl6StdHashU32Body},
    {"hash", "u32", "__xl_std_hash_u32", NULL, NULL, 0, "uint32", xl6StdHashU32BytesBody},
    {"hash", "u32Buffer", "__xl_std_hash_u32_buffer", NULL, NULL, 0, "uint32", xl6StdHashU32BufferBody},
    {"hash", "u32WithSeed", "__xl_std_hash_u32_with_seed", NULL, NULL, 0, "uint32", xl6StdHashU32WithSeedBody},
    {"hash", "u32WithSeed", "__xl_std_hash_u32_with_seed", NULL, NULL, 0, "uint32", xl6StdHashU32BytesWithSeedBody},
    {"hash", "u32BufferWithSeed", "__xl_std_hash_u32_buffer_with_seed", NULL, NULL, 0, "uint32", xl6StdHashU32BufferWithSeedBody},
    {"hash", "u64", "__xl_std_hash_u64", NULL, NULL, 0, "uint64", xl6StdHashU64Body},
    {"hash", "u64", "__xl_std_hash_u64", NULL, NULL, 0, "uint64", xl6StdHashU64BytesBody},
    {"hash", "u64Buffer", "__xl_std_hash_u64_buffer", NULL, NULL, 0, "uint64", xl6StdHashU64BufferBody},
    {"hash", "u64WithSeed", "__xl_std_hash_u64_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64WithSeedBody},
    {"hash", "u64WithSeed", "__xl_std_hash_u64_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64BytesWithSeedBody},
    {"hash", "u64BufferWithSeed", "__xl_std_hash_u64_buffer_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64BufferWithSeedBody},
    {"siphash", "hash64", "__xl_std_siphash64", NULL, NULL, 0, "uint64", xl6StdSipHash64StringBody},
    {"siphash", "hash64", "__xl_std_siphash64", NULL, NULL, 0, "uint64", xl6StdSipHash64BytesBody},
    {"siphash", "hash64Buffer", "__xl_std_siphash64_buffer", NULL, NULL, 0, "uint64", xl6StdSipHash64BufferBody},
    {"siphash", "hasher", "__xl_std_siphasher64", NULL, NULL, 0, "SipHasher64", xl6StdSipHasher64Body},
    {"crypto", "chacha20Encrypt", "__xl_std_crypto_chacha20_encrypt", NULL, NULL, 0, "string", xl6StdCryptoChaCha20EncryptBody},
    {"crypto", "chacha20Decrypt", "__xl_std_crypto_chacha20_decrypt", NULL, NULL, 0, "string", xl6StdCryptoChaCha20DecryptBody},
    {"cryptoLegacy", "md5", "__xl_std_cryptoLegacy_md5", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoMd5TextV1},
    {"cryptoLegacy", "md5Hex", "__xl_std_cryptoLegacy_md5Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoMd5TextHexV1},
    {"cryptoLegacy", "md5", "__xl_std_cryptoLegacy_md5", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoMd5BytesV1},
    {"cryptoLegacy", "md5Hex", "__xl_std_cryptoLegacy_md5Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoMd5BytesHexV1},
    {"cryptoLegacy", "md5Buffer", "__xl_std_cryptoLegacy_md5Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoMd5BufferV1},
    {"cryptoLegacy", "md5BufferHex", "__xl_std_cryptoLegacy_md5BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoMd5BufferHexV1},
    {"cryptoLegacy", "md5Stream", "__xl_std_cryptoLegacy_md5Stream", NULL, NULL, 0, "LegacyMd5", xl6StdResidentMd5Stream},
    {"cryptoLegacy", "sha1", "__xl_std_cryptoLegacy_sha1", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha1TextV1},
    {"cryptoLegacy", "sha1Hex", "__xl_std_cryptoLegacy_sha1Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha1TextHexV1},
    {"cryptoLegacy", "sha1", "__xl_std_cryptoLegacy_sha1", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha1BytesV1},
    {"cryptoLegacy", "sha1Hex", "__xl_std_cryptoLegacy_sha1Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha1BytesHexV1},
    {"cryptoLegacy", "sha1Buffer", "__xl_std_cryptoLegacy_sha1Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha1BufferV1},
    {"cryptoLegacy", "sha1BufferHex", "__xl_std_cryptoLegacy_sha1BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha1BufferHexV1},
    {"cryptoLegacy", "sha1Stream", "__xl_std_cryptoLegacy_sha1Stream", NULL, NULL, 0, "LegacySha1", xl6StdResidentSha1Stream},
    {"crypto", "sha224", "__xl_std_crypto_sha224", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha224TextV1},
    {"crypto", "sha224Hex", "__xl_std_crypto_sha224Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha224TextHexV1},
    {"crypto", "sha224", "__xl_std_crypto_sha224", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha224BytesV1},
    {"crypto", "sha224Hex", "__xl_std_crypto_sha224Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha224BytesHexV1},
    {"crypto", "sha224Buffer", "__xl_std_crypto_sha224Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha224BufferV1},
    {"crypto", "sha224BufferHex", "__xl_std_crypto_sha224BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha224BufferHexV1},
    {"crypto", "sha224Stream", "__xl_std_crypto_sha224Stream", NULL, NULL, 0, "Sha224", xl6StdResidentSha224Stream},
    {"crypto", "sha256", "__xl_std_crypto_sha256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha256TextV1},
    {"crypto", "sha256Hex", "__xl_std_crypto_sha256Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha256TextHexV1},
    {"crypto", "sha256", "__xl_std_crypto_sha256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha256BytesV1},
    {"crypto", "sha256Hex", "__xl_std_crypto_sha256Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha256BytesHexV1},
    {"crypto", "sha256Buffer", "__xl_std_crypto_sha256Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha256BufferV1},
    {"crypto", "sha256BufferHex", "__xl_std_crypto_sha256BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha256BufferHexV1},
    {"crypto", "sha256Stream", "__xl_std_crypto_sha256Stream", NULL, NULL, 0, "Sha256", xl6StdResidentSha256Stream},
    {"crypto", "sha384", "__xl_std_crypto_sha384", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha384TextV1},
    {"crypto", "sha384Hex", "__xl_std_crypto_sha384Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha384TextHexV1},
    {"crypto", "sha384", "__xl_std_crypto_sha384", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha384BytesV1},
    {"crypto", "sha384Hex", "__xl_std_crypto_sha384Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha384BytesHexV1},
    {"crypto", "sha384Buffer", "__xl_std_crypto_sha384Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha384BufferV1},
    {"crypto", "sha384BufferHex", "__xl_std_crypto_sha384BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha384BufferHexV1},
    {"crypto", "sha384Stream", "__xl_std_crypto_sha384Stream", NULL, NULL, 0, "Sha384", xl6StdResidentSha384Stream},
    {"crypto", "sha512", "__xl_std_crypto_sha512", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha512TextV1},
    {"crypto", "sha512Hex", "__xl_std_crypto_sha512Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha512TextHexV1},
    {"crypto", "sha512", "__xl_std_crypto_sha512", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha512BytesV1},
    {"crypto", "sha512Hex", "__xl_std_crypto_sha512Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha512BytesHexV1},
    {"crypto", "sha512Buffer", "__xl_std_crypto_sha512Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha512BufferV1},
    {"crypto", "sha512BufferHex", "__xl_std_crypto_sha512BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha512BufferHexV1},
    {"crypto", "sha512Stream", "__xl_std_crypto_sha512Stream", NULL, NULL, 0, "Sha512", xl6StdResidentSha512Stream},
    {"crypto", "sha512_256", "__xl_std_crypto_sha512_256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha512_256TextV1},
    {"crypto", "sha512_256Hex", "__xl_std_crypto_sha512_256Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha512_256TextHexV1},
    {"crypto", "sha512_256", "__xl_std_crypto_sha512_256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha512_256BytesV1},
    {"crypto", "sha512_256Hex", "__xl_std_crypto_sha512_256Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha512_256BytesHexV1},
    {"crypto", "sha512_256Buffer", "__xl_std_crypto_sha512_256Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoSha512_256BufferV1},
    {"crypto", "sha512_256BufferHex", "__xl_std_crypto_sha512_256BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoSha512_256BufferHexV1},
    {"crypto", "sha512_256Stream", "__xl_std_crypto_sha512_256Stream", NULL, NULL, 0, "Sha512_256", xl6StdResidentSha512_256Stream},
    {"crypto", "hmacSha256", "__xl_std_crypto_hmacSha256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha256TextV1},
    {"crypto", "hmacSha256Hex", "__xl_std_crypto_hmacSha256Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha256TextHexV1},
    {"crypto", "hmacSha256", "__xl_std_crypto_hmacSha256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha256BytesV1},
    {"crypto", "hmacSha256Hex", "__xl_std_crypto_hmacSha256Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha256BytesHexV1},
    {"crypto", "hmacSha256Buffer", "__xl_std_crypto_hmacSha256Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha256BufferV1},
    {"crypto", "hmacSha256BufferHex", "__xl_std_crypto_hmacSha256BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha256BufferHexV1},
    {"crypto", "hmacSha256Stream", "__xl_std_crypto_hmacSha256Stream", NULL, NULL, 0, "HmacSha256", xl6StdResidentHmacSha256Stream},
    {"crypto", "hmacSha384", "__xl_std_crypto_hmacSha384", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha384TextV1},
    {"crypto", "hmacSha384Hex", "__xl_std_crypto_hmacSha384Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha384TextHexV1},
    {"crypto", "hmacSha384", "__xl_std_crypto_hmacSha384", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha384BytesV1},
    {"crypto", "hmacSha384Hex", "__xl_std_crypto_hmacSha384Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha384BytesHexV1},
    {"crypto", "hmacSha384Buffer", "__xl_std_crypto_hmacSha384Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha384BufferV1},
    {"crypto", "hmacSha384BufferHex", "__xl_std_crypto_hmacSha384BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha384BufferHexV1},
    {"crypto", "hmacSha384Stream", "__xl_std_crypto_hmacSha384Stream", NULL, NULL, 0, "HmacSha384", xl6StdResidentHmacSha384Stream},
    {"crypto", "hmacSha512", "__xl_std_crypto_hmacSha512", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha512TextV1},
    {"crypto", "hmacSha512Hex", "__xl_std_crypto_hmacSha512Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha512TextHexV1},
    {"crypto", "hmacSha512", "__xl_std_crypto_hmacSha512", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha512BytesV1},
    {"crypto", "hmacSha512Hex", "__xl_std_crypto_hmacSha512Hex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha512BytesHexV1},
    {"crypto", "hmacSha512Buffer", "__xl_std_crypto_hmacSha512Buffer", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHmacSha512BufferV1},
    {"crypto", "hmacSha512BufferHex", "__xl_std_crypto_hmacSha512BufferHex", NULL, NULL, 0, "string", xl6StdResidentxl6CryptoHmacSha512BufferHexV1},
    {"crypto", "hmacSha512Stream", "__xl_std_crypto_hmacSha512Stream", NULL, NULL, 0, "HmacSha512", xl6StdResidentHmacSha512Stream},
    {"crypto", "pbkdf2Sha256", "__xl_std_crypto_pbkdf2Sha256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoPbkdf2Sha256V1},
    {"crypto", "hkdfSha256", "__xl_std_crypto_hkdfSha256", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha256V1},
    {"crypto", "hkdfSha256Extract", "__xl_std_crypto_hkdfSha256Extract", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha256ExtractV1},
    {"crypto", "hkdfSha256Expand", "__xl_std_crypto_hkdfSha256Expand", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha256ExpandV1},
    {"crypto", "pbkdf2Sha384", "__xl_std_crypto_pbkdf2Sha384", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoPbkdf2Sha384V1},
    {"crypto", "hkdfSha384", "__xl_std_crypto_hkdfSha384", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha384V1},
    {"crypto", "hkdfSha384Extract", "__xl_std_crypto_hkdfSha384Extract", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha384ExtractV1},
    {"crypto", "hkdfSha384Expand", "__xl_std_crypto_hkdfSha384Expand", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha384ExpandV1},
    {"crypto", "pbkdf2Sha512", "__xl_std_crypto_pbkdf2Sha512", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoPbkdf2Sha512V1},
    {"crypto", "hkdfSha512", "__xl_std_crypto_hkdfSha512", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha512V1},
    {"crypto", "hkdfSha512Extract", "__xl_std_crypto_hkdfSha512Extract", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha512ExtractV1},
    {"crypto", "hkdfSha512Expand", "__xl_std_crypto_hkdfSha512Expand", NULL, NULL, 0, "bytes", xl6StdResidentxl6CryptoHkdfSha512ExpandV1},
    {"crypto", "timingSafeEqual", "__xl_std_crypto_timingSafeEqual", NULL, NULL, 0, "bool", xl6StdResidentxl6CryptoTimingSafeEqualV1},
    {"asn1", "parse", "__xl_std_asn1_parse", NULL, NULL, 0, "DerValue", xl6StdResidentxl6DerParseV1},
    {"pem", "parse", "__xl_std_pem_parse", NULL, NULL, 0, "PemBlock", xl6StdResidentxl6PemFindV1},
    {"pem", "encode", "__xl_std_pem_encode", NULL, NULL, 0, "string", xl6StdResidentxl6PemEncodeV1},
    {"pem", "decode", "__xl_std_pem_decode", NULL, NULL, 0, "bytes", xl6StdResidentPemDecode},
    {"crypto", "randomHex", "__xl_std_crypto_random_hex", NULL, NULL, 0, "string", xl6StdCryptoRandomHexBody},
    {"crypto", "randomString", "__xl_std_crypto_random_string", NULL, NULL, 0, "string", xl6StdCryptoRandomStringBody},
    {"crypto", "fillRandomBytes", "__xl_std_crypto_fill_random_bytes", NULL, NULL, 0, "", xl6StdCryptoFillRandomBytesBody},
    {"crypto", "chacha20Bytes", "__xl_std_crypto_chacha20_bytes", NULL, NULL, 0, "Buffer", xl6StdCryptoChaCha20BytesBody},
    {"crypto", "randomBytes", "__xl_std_crypto_random_bytes", NULL, NULL, 0, "bytes", xl6StdCryptoRandomBytesBody},
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

    {"id", "xid", "__xl_std_id_xid", "xl6_xrt_xid_make_string", NULL, 0, "string", NULL},
    {"id", "make", "__xl_std_id_make", "xl6_xrt_xid_make", NULL, 0, "Xid", NULL},
    {"id", "makeMany", "__xl_std_id_make_many", "xl6_xrt_xid_make_many", xl6StdXidCountParams, 1, "array<Xid>", NULL},
    {"id", "parse", "__xl_std_id_parse", "xl6_xrt_xid_parse", xl6StdXidTextParams, 1, "Xid", NULL},
    {"id", "tryParse", "__xl_std_id_try_parse", NULL, NULL, 0, "Xid?", xl6StdIdTryParseBody},
    {"id", "fromBytes", "__xl_std_id_from_bytes", "xl6_xrt_xid_from_bytes", xl6StdXidBytesParams, 1, "Xid", NULL},

    {"net", "localIP", "__xl_std_net_local_ip", NULL, NULL, 0, "string", xl6StdNetLocalIPBody},
    {"net", "localMAC", "__xl_std_net_local_mac", NULL, NULL, 0, "string", xl6StdNetLocalMACBody},
    {"net", "localName", "__xl_std_net_local_name", NULL, NULL, 0, "string", xl6StdNetLocalNameBody},
    {"net", "hostName", "__xl_std_net_host_name", "xl6NetHostNameV1", NULL, 0, "string", NULL},
    {"net", "localAddress", "__xl_std_net_local_address", NULL, NULL, 0, "NetAddress", xl6StdNetLocalAddressBody},
    {"net", "localHardware", "__xl_std_net_local_hardware", "xl6NetLocalHardwareV1", NULL, 0, "bytes", NULL},
    {"net", "interfaces", "__xl_std_net_interfaces", "xl6NetInterfacesCreateV1", NULL, 0, "NetInterfaceList", NULL},
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
    {"tcp", "dialAsync", "__xl_std_tcp_dial_async", NULL, NULL, 0, "future<TcpStream>", xl6StdTcpDialAsyncBody},
    {"tcp", "dial", "__xl_std_tcp_dial", NULL, NULL, 0, "TcpStream", xl6StdTcpDialBody},
    {"tcp", "listen", "__xl_std_tcp_listen", NULL, NULL, 0, "TcpListener", xl6StdTcpListenBody},

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
    {"process", "run", "__xl_std_process_run", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunOptionsBody},
    {"process", "runFile", "__xl_std_process_run_file", NULL, NULL, 0, "ProcessResult", xl6StdProcessRunFileBody},
    {"process", "cmd", "__xl_std_process_cmd", NULL, NULL, 0, "ProcessCommand", xl6StdProcessCmdBody},
    {"process", "shell", "__xl_std_process_shell", NULL, NULL, 0, "ProcessCommand", xl6StdProcessShellBody},
    {"process", "pipeline", "__xl_std_process_pipeline", NULL, NULL, 0, "PipelineResult", xl6StdProcessPipelineBody},
    {"process", "pipeline", "__xl_std_process_pipeline", NULL, NULL, 0, "PipelineResult", xl6StdProcessPipelineOptionsBody},
    {"process", "start", "__xl_std_process_start", NULL, NULL, 0, "Process", xl6StdProcessStartBody},
    {"process", "start", "__xl_std_process_start", NULL, NULL, 0, "Process", xl6StdProcessStartCommandBody},
    {"process", "startFile", "__xl_std_process_start_file", NULL, NULL, 0, "Process", xl6StdProcessStartFileBody},
    {"process", "terminalSupported", "__xl_std_process_terminal_supported", NULL, NULL, 0, "bool", xl6StdProcessTerminalSupportedBody},
    {"process", "open", "__xl_std_process_open", NULL, NULL, 0, "bool", xl6StdProcessOpenBody},

    {"task", "group", "__xl_std_task_group", NULL, NULL, 0, "TaskGroup", xl6StdTaskGroupBody},

    {"log", "create", "__xl_std_log_create", NULL, NULL, 0, "Logger", xl6StdLogCreateBody},
    {"log", "textFileSink", "__xl_std_log_textFileSink", NULL, NULL, 0, "LogSink", xl6StdLogTextFileSinkBody},
    {"log", "jsonFileSink", "__xl_std_log_jsonFileSink", NULL, NULL, 0, "LogSink", xl6StdLogJsonFileSinkBody},
    {"log", "asyncSink", "__xl_std_log_asyncSink", NULL, NULL, 0, "LogSink", xl6StdLogAsyncSinkBody},
    {"log", "ringSink", "__xl_std_log_ringSink", NULL, NULL, 0, "LogSink", xl6StdLogRingSinkBody},
    {"log", "customSink", "__xl_std_log_customSink", NULL, NULL, 0, "LogSink", xl6StdLogCustomSinkBody},
    {"log", "shutdown", "__xl_std_log_shutdown", NULL, NULL, 0, NULL, xl6StdLogShutdownBody},
    {"log", "record", "__xl_std_log_record", NULL, NULL, 0, "LogRecord", xl6StdLogRecordBody},
    {"log", "writeRecord", "__xl_std_log_writeRecord", NULL, NULL, 0, "LogResult", xl6StdLogWriteRecordBody},
    {"log", "text", "__xl_std_log_text", NULL, NULL, 0, "string", xl6StdLogTextBody},
    {"log", "json", "__xl_std_log_json", NULL, NULL, 0, "string", xl6StdLogJsonBody},
    {"log", "textToBuffer", "__xl_std_log_textToBuffer", NULL, NULL, 0, "int", xl6StdLogTextBufferBody},
    {"log", "printf", "__xl_std_log_printf", "xl6LogDefaultPrintfV1", xl6StdLogPrintfParams, 3, "LogResult", NULL},
    {"log", "textToWriter", "__xl_std_log_textToWriter", NULL, NULL, 0, "int", xl6StdLogTextWriterBody},
    {"log", "jsonToWriter", "__xl_std_log_jsonToWriter", NULL, NULL, 0, "int", xl6StdLogJsonWriterBody},
    {"log", "textWriterSink", "__xl_std_log_textWriterSink", NULL, NULL, 0, "LogSink", xl6StdLogTextWriterSinkBody},
    {"log", "jsonWriterSink", "__xl_std_log_jsonWriterSink", NULL, NULL, 0, "LogSink", xl6StdLogJsonWriterSinkBody},
    {"log", "jsonToBuffer", "__xl_std_log_jsonToBuffer", NULL, NULL, 0, "int", xl6StdLogJsonBufferBody},
    {"log", "consoleSink", "__xl_std_log_consoleSink", NULL, NULL, 0, "LogSink", xl6StdLogConsoleSinkBody},
    {"log", "default", "__xl_std_log_default", NULL, NULL, 0, "Logger?", xl6StdLogDefaultBody},
    {"log", "setDefault", "__xl_std_log_setDefault", NULL, NULL, 0, "", xl6StdLogSetDefaultBody},
    {"log", "levelName", "__xl_std_log_level_name", NULL, NULL, 0, "string", xl6StdLogLevelNameBody},
    {"log", "write", "__xl_std_log_write", NULL, NULL, 0, "LogResult", xl6StdLogWriteBody},
    {"log", "trace", "__xl_std_log_trace", NULL, NULL, 0, "LogResult", xl6StdLogtraceBody},
    {"log", "debug", "__xl_std_log_debug", NULL, NULL, 0, "LogResult", xl6StdLogdebugBody},
    {"log", "info", "__xl_std_log_info", NULL, NULL, 0, "LogResult", xl6StdLoginfoBody},
    {"log", "warn", "__xl_std_log_warn", NULL, NULL, 0, "LogResult", xl6StdLogwarnBody},
    {"log", "error", "__xl_std_log_error", NULL, NULL, 0, "LogResult", xl6StdLogerrorBody},
    {"log", "fatal", "__xl_std_log_fatal", NULL, NULL, 0, "LogResult", xl6StdLogfatalBody},
    {"log", "flush", "__xl_std_log_flush", NULL, NULL, 0, "", xl6StdLogFlushBody},

    {"signal", "on", "__xl_std_signal_on", NULL, NULL, 0, "SignalHandle", xl6StdSignalOn0Body},
    {"signal", "on", "__xl_std_signal_on", NULL, NULL, 0, "SignalHandle", xl6StdSignalOn1Body},
    {"signal", "once", "__xl_std_signal_once", NULL, NULL, 0, "SignalHandle", xl6StdSignalOnce0Body},
    {"signal", "once", "__xl_std_signal_once", NULL, NULL, 0, "SignalHandle", xl6StdSignalOnce1Body},
    {"signal", "supported", "__xl_std_signal_supported", NULL, NULL, 0, "bool", xl6StdSignalSupportedBody},
    {"signal", "name", "__xl_std_signal_name", NULL, NULL, 0, "string", xl6StdSignalNameBody},
    {"signal", "ignore", "__xl_std_signal_ignore", NULL, NULL, 0, "bool", xl6StdSignalIgnoreBody},
    {"signal", "restore", "__xl_std_signal_restore", NULL, NULL, 0, "bool", xl6StdSignalRestoreBody},
    {"signal", "restoreAll", "__xl_std_signal_restore_all", NULL, NULL, 0, "bool", xl6StdSignalRestoreAllBody},
    {"signal", "healthy", "__xl_std_signal_healthy", NULL, NULL, 0, "bool", xl6StdSignalHealthyBody},
    {"signal", "reset", "__xl_std_signal_reset", NULL, NULL, 0, "", xl6StdSignalResetBody},
    {"signal", "raise", "__xl_std_signal_raise", NULL, NULL, 0, "bool", xl6StdSignalRaiseBody},
    {"signal", "count", "__xl_std_signal_count", NULL, NULL, 0, "uint64", xl6StdSignalCountBody},
    {"signal", "received", "__xl_std_signal_received", NULL, NULL, 0, "bool", xl6StdSignalReceivedBody},
    {"signal", "clear", "__xl_std_signal_clear", NULL, NULL, 0, "", xl6StdSignalClearBody},
    {"signal", "sendTo", "__xl_std_signal_send_to", NULL, NULL, 0, "SignalHandle", xl6StdSignalSendToBody},
    {"signal", "cancelOn", "__xl_std_signal_cancel_on", NULL, NULL, 0, "SignalHandle", xl6StdSignalCancelOnBody},

    {"template", "compile", "__xl_std_template_compile", NULL, NULL, 0, "Template", xl6StdTemplateCompileBody},
    {"template", "compileFile", "__xl_std_template_compile_file", NULL, NULL, 0, "Template", xl6StdTemplateCompileFileBody},
    {"template", "engine", "__xl_std_template_engine", NULL, NULL, 0, "TemplateEngine", xl6StdTemplateEngineBody},
    {"template", "render", "__xl_std_template_render", NULL, NULL, 0, "string", xl6StdTemplateRenderBody},
    {"template", "renderFile", "__xl_std_template_render_file", NULL, NULL, 0, "string", xl6StdTemplateRenderFileBody},
    {"template", "compileFileIn", "__xl_std_template_compile_file_in", NULL, NULL, 0, "Template", xl6StdTemplateCompileVfsBody},
    {"template", "renderFileIn", "__xl_std_template_render_file_in", NULL, NULL, 0, "string", xl6StdTemplateRenderVfsBody},
    {"template", "errorLocation", "__xl_std_template_error_location", NULL, NULL, 0, "TemplateLocation?", xl6StdTemplateErrorLocationBody},
};

const XL6_StdlibForeignContract xl6StdlibForeignContracts[] = {
    {"__xl_crc32_text", "xl6Crc32StringV1", XL6_STDLIB_FOREIGN_EFFECT_PURE | XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
    {"__xl_crc32_bytes", "xl6Crc32BytesV1", XL6_STDLIB_FOREIGN_EFFECT_PURE | XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
    {"__xl_crc32_buffer", "xl6Crc32BufferV1", XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
#define XL6_HASH_VALUE_EFFECTS (XL6_STDLIB_FOREIGN_EFFECT_PURE | \
    XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE | XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING)
#define XL6_HASH_BUFFER_EFFECTS (XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE | \
    XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING)
    {"__xl_std_xrt_hash32", "xl6_xrt_hash32_string", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash32_seed", "xl6_xrt_hash32_string_seed", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash64", "xl6_xrt_hash64_string", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash64_seed", "xl6_xrt_hash64_string_seed", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash32_bytes", "xl6_xrt_hash32_bytes", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash32_bytes_seed", "xl6_xrt_hash32_bytes_seed", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash64_bytes", "xl6_xrt_hash64_bytes", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash64_bytes_seed", "xl6_xrt_hash64_bytes_seed", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_hash32_raw", "xl6_xrt_hash32_raw", XL6_HASH_BUFFER_EFFECTS},
    {"__xl_std_xrt_hash32_raw_seed", "xl6_xrt_hash32_raw_seed", XL6_HASH_BUFFER_EFFECTS},
    {"__xl_std_xrt_hash64_raw", "xl6_xrt_hash64_raw", XL6_HASH_BUFFER_EFFECTS},
    {"__xl_std_xrt_hash64_raw_seed", "xl6_xrt_hash64_raw_seed", XL6_HASH_BUFFER_EFFECTS},
    {"__xl_std_xrt_siphash64_string", "xl6_xrt_siphash64_string", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_siphash64_bytes", "xl6_xrt_siphash64_bytes", XL6_HASH_VALUE_EFFECTS},
    {"__xl_std_xrt_siphash64_raw", "xl6_xrt_siphash64_raw", XL6_HASH_BUFFER_EFFECTS},
#undef XL6_HASH_VALUE_EFFECTS
#undef XL6_HASH_BUFFER_EFFECTS
    {"__xl_std_math_is_close", "xrtMathNear",
     XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
    {"__xl_std_math_int_near", "xrtMathIntNear",
     XL6_STDLIB_FOREIGN_EFFECT_PURE |
     XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE |
     XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
    {"__xl_std_math_sin", "sin",
     XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE |
     XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
    {"__xl_std_math_abs", "fabs",
     XL6_STDLIB_FOREIGN_EFFECT_PURE |
     XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE |
     XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING},
#define XL6_CRT_MATH_CONTRACT(name, symbol) \
    {"__xl_std_math_" name, symbol, \
     XL6_STDLIB_FOREIGN_EFFECT_NO_RAISE | \
     XL6_STDLIB_FOREIGN_EFFECT_NONBLOCKING}
    XL6_CRT_MATH_CONTRACT("cos", "cos"),
    XL6_CRT_MATH_CONTRACT("tan", "tan"),
    XL6_CRT_MATH_CONTRACT("asin", "asin"),
    XL6_CRT_MATH_CONTRACT("acos", "acos"),
    XL6_CRT_MATH_CONTRACT("atan", "atan"),
    XL6_CRT_MATH_CONTRACT("atan2", "atan2"),
    XL6_CRT_MATH_CONTRACT("sqrt", "sqrt"),
    XL6_CRT_MATH_CONTRACT("pow", "pow"),
    XL6_CRT_MATH_CONTRACT("exp", "exp"),
    XL6_CRT_MATH_CONTRACT("log", "log"),
    XL6_CRT_MATH_CONTRACT("log10", "log10"),
    XL6_CRT_MATH_CONTRACT("floor", "floor"),
    XL6_CRT_MATH_CONTRACT("ceil", "ceil"),
    XL6_CRT_MATH_CONTRACT("round", "round"),
    XL6_CRT_MATH_CONTRACT("round_current", "nearbyint"),
    XL6_CRT_MATH_CONTRACT("sinh", "sinh"),
    XL6_CRT_MATH_CONTRACT("cosh", "cosh"),
    XL6_CRT_MATH_CONTRACT("tanh", "tanh"),
    XL6_CRT_MATH_CONTRACT("asinh", "asinh"),
    XL6_CRT_MATH_CONTRACT("acosh", "acosh"),
    XL6_CRT_MATH_CONTRACT("atanh", "atanh"),
    XL6_CRT_MATH_CONTRACT("erf", "erf"),
    XL6_CRT_MATH_CONTRACT("erfc", "erfc"),
    XL6_CRT_MATH_CONTRACT("gamma", "tgamma"),
    XL6_CRT_MATH_CONTRACT("log_gamma", "lgamma"),
    XL6_CRT_MATH_CONTRACT("fma", "fma"),
    XL6_CRT_MATH_CONTRACT("copy_sign", "copysign"),
    XL6_CRT_MATH_CONTRACT("next_after", "nextafter"),
    XL6_CRT_MATH_CONTRACT("remainder", "remainder"),
    XL6_CRT_MATH_CONTRACT("positive_diff", "fdim"),
    XL6_CRT_MATH_CONTRACT("min_number", "fmin"),
    XL6_CRT_MATH_CONTRACT("max_number", "fmax"),
    XL6_CRT_MATH_CONTRACT("ldexp_native", "ldexp"),
    XL6_CRT_MATH_CONTRACT("scalbn_native", "scalbn"),
    XL6_CRT_MATH_CONTRACT("logb", "logb"),
    XL6_CRT_MATH_CONTRACT("ilogb_native", "ilogb")
#undef XL6_CRT_MATH_CONTRACT
};


#define XL6_STDLIB_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef char XL6_StdlibConstCountCheck[
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibConsts) == XL6_STDLIB_CONST_COUNT ? 1 : -1];
typedef char XL6_StdlibFuncCountCheck[
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibFuncs) == XL6_STDLIB_FUNC_COUNT ? 1 : -1];

const size_t xl6StdlibConstCount = XL6_STDLIB_CONST_COUNT;
const size_t xl6StdlibFuncCount = XL6_STDLIB_FUNC_COUNT;
const size_t xl6StdlibForeignContractCount =
    XL6_STDLIB_ARRAY_COUNT(xl6StdlibForeignContracts);
