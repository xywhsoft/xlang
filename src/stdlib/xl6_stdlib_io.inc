const char* xl6StdlibIoReaderSource =
    "class IoReader\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_io_reader_open\" create(string path)\n"
    "    extern \"xl6_xrt_io_reader_bytes\" create(bytes data, int size)\n"
    "    extern \"xl6_xrt_io_reader_stdin\" create()\n"
    "    extern \"xl6_xrt_io_reader_destroy\" destroy()\n"
    "    extern \"xl6_xrt_io_reader_handle\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_io_reader_close\" func closeNative() as bool\n"
    "    extern \"xl6_xrt_io_reader_read\" func readNative(int maxBytes) as bytes\n"
    "    extern \"xl6_xrt_io_reader_exact\" func readExact(int count) as bytes\n"
    "    extern \"xl6_xrt_io_reader_all\" func readAllNative(int maxBytes) as bytes\n"
    "    extern \"xl6_xrt_io_reader_seek\" func seekNative(int offset, int origin) as int\n"
    "    extern \"xl6_xrt_io_reader_tell\" func tell() as int\n"
    "    extern \"xl6_xrt_io_reader_size\" func size() as int\n"
    "    extern \"xl6_xrt_io_reader_eof\" func eof() as bool\n"
    "    extern \"xl6_xrt_io_reader_can_seek\" func canSeek() as bool\n"
    "    extern \"xl6_xrt_io_reader_can_size\" func canSize() as bool\n"
    "    extern \"xl6_xrt_io_reader_closed\" func closed() as bool\n"
    "    func close()\n"
    "        if not self.closeNative() then throw IOError \"reader close failed\" end\n"
    "    end\n"
    "    func readAll(int maxBytes = 67108864) as bytes\n"
    "        return self.readAllNative(maxBytes)\n"
    "    end\n"
    "    func read(int maxBytes) as bytes\n"
    "        return self.readNative(maxBytes)\n"
    "    end\n"
    "    func seek(int offset, int origin = 0) as int\n"
    "        return self.seekNative(offset, origin)\n"
    "    end\n"
    "\n";

const char* xl6StdlibIoReaderBufferSource =
    "    extern \"xl6_xrt_io_reader_buffer\" create(point buffer, int size)\n"
    "    extern \"xl6_xrt_io_reader_into\" func readIntoNative(point target, int maxBytes) as int\n"
    "    func readInto(Buffer target, int maxBytes = 16384) as int\n"
    "        return self.readIntoNative(target.nativeHandle(), maxBytes)\n"
    "    end\n"
    "\n";

const char* xl6StdlibIoWriterSource =
    "class IoWriter\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_io_writer_open\" create(string path, int mode)\n"
    "    extern \"xl6_xrt_io_writer_buffer\" create()\n"
    "    extern \"xl6_xrt_io_writer_standard\" create(int stream)\n"
    "    extern \"xl6_xrt_io_writer_destroy\" destroy()\n"
    "    extern \"xl6_xrt_io_writer_handle\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_io_writer_close\" func closeNative() as bool\n"
    "    extern \"xl6_xrt_io_writer_flush\" func flushNative() as bool\n"
    "    extern \"xl6_xrt_io_writer_to_bytes\" func toBytes() as bytes\n"
    "    extern \"xl6_xrt_io_writer_seek\" func seekNative(int offset, int origin) as int\n"
    "    extern \"xl6_xrt_io_writer_tell\" func tell() as int\n"
    "    extern \"xl6_xrt_io_writer_size\" func size() as int\n"
    "    extern \"xl6_xrt_io_writer_can_seek\" func canSeek() as bool\n"
    "    extern \"xl6_xrt_io_writer_can_size\" func canSize() as bool\n"
    "    extern \"xl6_xrt_io_writer_closed\" func closed() as bool\n"
    "    extern \"xl6_xrt_io_writer_bytes\" func writebytesNative(bytes data, int size) as int\n"
    "    extern \"xl6_xrt_io_writer_all_bytes\" func writeAllbytesNative(bytes data, int size) as bool\n"
    "    func write(bytes data, int size = -1) as int\n"
    "        return self.writebytesNative(data, size)\n"
    "    end\n"
    "    func writeAll(bytes data, int size = -1)\n"
    "        if not self.writeAllbytesNative(data, size) then throw IOError \"stream write failed\" end\n"
    "    end\n"
    "    extern \"xl6_xrt_io_writer_text\" func writetextNative(string data, int size) as int\n"
    "    extern \"xl6_xrt_io_writer_all_text\" func writeAlltextNative(string data, int size) as bool\n"
    "    func write(string data, int size = -1) as int\n"
    "        return self.writetextNative(data, size)\n"
    "    end\n"
    "    func writeAll(string data, int size = -1)\n"
    "        if not self.writeAlltextNative(data, size) then throw IOError \"stream write failed\" end\n"
    "    end\n"
    "    func close()\n"
    "        if not self.closeNative() then throw IOError \"writer close failed\" end\n"
    "    end\n"
    "    func flush()\n"
    "        if not self.flushNative() then throw IOError \"stream flush failed\" end\n"
    "    end\n"
    "    func seek(int offset, int origin = 0) as int\n"
    "        return self.seekNative(offset, origin)\n"
    "    end\n"
    "\n";

const char* xl6StdlibIoWriterBufferSource =
    "    extern \"xl6_xrt_io_writer_write_buffer\" func writeBufferNative(point data, int size) as int\n"
    "    extern \"xl6_xrt_io_writer_all_buffer\" func writeAllBufferNative(point data, int size) as bool\n"
    "    func write(Buffer data, int size = -1) as int\n"
    "        return self.writeBufferNative(data.nativeHandle(), size)\n"
    "    end\n"
    "    func writeAll(Buffer data, int size = -1)\n"
    "        if not self.writeAllBufferNative(data.nativeHandle(), size) then throw IOError \"stream write failed\" end\n"
    "    end\n"
    "\n";

const char* xl6StdlibIoLinesSource =
    "class IoLineReader\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_io_lines_create\" create(point reader, int maxLineBytes)\n"
    "    extern \"xl6_xrt_io_lines_destroy\" destroy()\n"
    "    extern \"xl6_xrt_io_lines_close\" func closeNative() as bool\n"
    "    extern \"xl6_xrt_io_lines_advance\" func advance() as bool\n"
    "    extern \"xl6_xrt_io_lines_bytes\" func lineBytes() as bytes\n"
    "    extern \"xl6_xrt_io_lines_ending\" func ending() as int\n"
    "    func __iter__() as IoLineReader\n"
    "        return self\n"
    "    end\n"
    "    func close()\n"
    "        if not self.closeNative() then throw IOError \"line reader close failed\" end\n"
    "    end\n"
    "    func next() as (bool, bytes, int)\n"
    "        if not self.advance() then return false, b\"\", 0 end\n"
    "        return true, self.lineBytes(), self.ending()\n"
    "    end\n"
    "    func __next__() as (bool, bytes)\n"
    "        if not self.advance() then return false, b\"\" end\n"
    "        return true, self.lineBytes()\n"
    "    end\n"
    "end\n"
    "\n";

const char* xl6StdlibIoProtocolsSource =
    "protocol IoReadable\n"
    "    func read(int maxBytes) as bytes\n"
    "end\n"
    "protocol IoWritable\n"
    "    func writeAll(bytes data, int size)\n"
    "end\n"
    "\n";

static const char openReaderBody[] =
    "func __xl_std_io_openReader(string path) as IoReader\n"
    "    return IoReader(path)\n"
    "end\n"
    "\n";

static const char openWriterBody[] =
    "func __xl_std_io_openWriter(string path, int mode = 1) as IoWriter\n"
    "    return IoWriter(path, mode)\n"
    "end\n"
    "\n";

static const char fromBytesBody[] =
    "func __xl_std_io_fromBytes(bytes data, int size = -1) as IoReader\n"
    "    return IoReader(data, size)\n"
    "end\n"
    "\n";

static const char fromBufferBody[] =
    "func __xl_std_io_fromBuffer(Buffer data, int size = -1) as IoReader\n"
    "    return IoReader(data.nativeHandle(), size)\n"
    "end\n"
    "\n";

static const char bufferWriterBody[] =
    "func __xl_std_io_bufferWriter() as IoWriter\n"
    "    return IoWriter()\n"
    "end\n"
    "\n";

static const char discardWriterBody[] =
    "func __xl_std_io_discardWriter() as IoWriter\n"
    "    return IoWriter(3)\n"
    "end\n"
    "\n";

static const char stdinBody[] =
    "func __xl_std_io_stdin() as IoReader\n"
    "    return IoReader()\n"
    "end\n"
    "\n";

static const char stdoutBody[] =
    "func __xl_std_io_stdout() as IoWriter\n"
    "    return IoWriter(1)\n"
    "end\n"
    "\n";

static const char stderrBody[] =
    "func __xl_std_io_stderr() as IoWriter\n"
    "    return IoWriter(2)\n"
    "end\n"
    "\n";

static const char linesBody[] =
    "func __xl_std_io_lines(IoReader source, int maxLineBytes = 1048576) as IoLineReader\n"
    "    return IoLineReader(source.nativeHandle(), maxLineBytes)\n"
    "end\n"
    "\n";

static const char write_buffer_fileBody[] =
    "func __xl_std_io_write_file(string path, Buffer data, int mode = 1, int size = -1)\n"
    "    if not __xl_std_runtime_io_write_buffer_file(path, data.nativeHandle(), mode, size) then throw IOError \"file write failed\" end\n"
    "end\n"
    "\n";

static const char copyBody[] =
    "func __xl_std_io_copy(IoReader source, IoWriter target) as int\n"
    "    return __xl_std_runtime_io_copy(source.nativeHandle(), target.nativeHandle(), -1, 0)\n"
    "end\n"
    "\n";

static const char copyProtocolBody[] =
    "func __xl_std_io_copy(IoReadable source, IoWritable target) as int\n"
    "    int limit = -1\n"
    "    int total = 0\n"
    "    while limit < 0 or total < limit\n"
    "        int request = 16384\n"
    "        if limit >= 0 and limit - total < request then request = limit - total end\n"
    "        bytes chunk = source.read(request)\n"
    "        if chunk.length > request then throw IOError \"reader exceeded requested length\" end\n"
    "        if chunk.length == 0 then\n"
    "            return total\n"
    "        end\n"
    "        if total > 9223372036854775807 - chunk.length then throw RangeError \"copy size overflow\" end\n"
    "        target.writeAll(chunk, -1)\n"
    "        total += chunk.length\n"
    "    end\n"
    "    return total\n"
    "end\n"
    "\n";

static const char copyExactBody[] =
    "func __xl_std_io_copyExact(IoReader source, IoWriter target, int count) as int\n"
    "    return __xl_std_runtime_io_copy(source.nativeHandle(), target.nativeHandle(), count, 1)\n"
    "end\n"
    "\n";

static const char copyExactProtocolBody[] =
    "func __xl_std_io_copyExact(IoReadable source, IoWritable target, int count) as int\n"
    "    int limit = count\n"
    "    if limit < 0 then throw RangeError \"copy limit is invalid\" end\n"
    "    int total = 0\n"
    "    while limit < 0 or total < limit\n"
    "        int request = 16384\n"
    "        if limit >= 0 and limit - total < request then request = limit - total end\n"
    "        bytes chunk = source.read(request)\n"
    "        if chunk.length > request then throw IOError \"reader exceeded requested length\" end\n"
    "        if chunk.length == 0 then\n"
    "            throw IOError \"unexpected end of stream\"\n"
    "            return total\n"
    "        end\n"
    "        if total > 9223372036854775807 - chunk.length then throw RangeError \"copy size overflow\" end\n"
    "        target.writeAll(chunk, -1)\n"
    "        total += chunk.length\n"
    "    end\n"
    "    return total\n"
    "end\n"
    "\n";

static const char copyUpToBody[] =
    "func __xl_std_io_copyUpTo(IoReader source, IoWriter target, int count) as int\n"
    "    return __xl_std_runtime_io_copy(source.nativeHandle(), target.nativeHandle(), count, 2)\n"
    "end\n"
    "\n";

static const char copyUpToProtocolBody[] =
    "func __xl_std_io_copyUpTo(IoReadable source, IoWritable target, int count) as int\n"
    "    int limit = count\n"
    "    if limit < 0 then throw RangeError \"copy limit is invalid\" end\n"
    "    int total = 0\n"
    "    while limit < 0 or total < limit\n"
    "        int request = 16384\n"
    "        if limit >= 0 and limit - total < request then request = limit - total end\n"
    "        bytes chunk = source.read(request)\n"
    "        if chunk.length > request then throw IOError \"reader exceeded requested length\" end\n"
    "        if chunk.length == 0 then\n"
    "            return total\n"
    "        end\n"
    "        if total > 9223372036854775807 - chunk.length then throw RangeError \"copy size overflow\" end\n"
    "        target.writeAll(chunk, -1)\n"
    "        total += chunk.length\n"
    "    end\n"
    "    return total\n"
    "end\n"
    "\n";
