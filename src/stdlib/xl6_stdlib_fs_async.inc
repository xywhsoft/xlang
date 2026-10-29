const char* xl6StdlibAsyncFileClassSource =
    "class AsyncFile\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_async_open\" create(string path, int flags, int mode, int share)\n"
    "    extern \"xl6_xrt_fs_async_adopt\" create(File file)\n"
    "    extern \"xl6_xrt_fs_async_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_async_close\" func close() as future<bool>\n"
    "    extern \"xl6_xrt_fs_async_read\" func readAt(uint64 offset, int size) as future<bytes>\n"
    "    extern \"xl6_xrt_fs_async_write\" func writeNative(uint64 offset, bytes data, int size) as future<uint64>\n"
    "    func writeAt(uint64 offset, bytes data, int size = -1) as future<uint64>\n"
    "        return self.writeNative(offset,data,size)\n"
    "    end\n"
    "    extern \"xl6_xrt_fs_async_flush\" func flush() as future<bool>\n"
    "    extern \"xl6_xrt_fs_async_size\" func size() as future<uint64>\n"
    "    extern \"xl6_xrt_fs_async_resize\" func resize(uint64 size) as future<uint64>\n"
    "    extern \"xl6_xrt_fs_async_flags\" get flags() as int\n"
    "    extern \"xl6_xrt_fs_async_closed\" get closed() as bool\n"
    "end\n";

const char* xl6StdlibFsAsyncStatsSource =
    "extern \"xl6_xrt_fs_async_tree_copy\" func __xl_fs_async_stats_copy(string source,string target,int flags) as future<var>\n"
    "extern \"xl6_xrt_fs_async_tree_remove\" func __xl_fs_async_stats_remove(string path,bool keepRoot) as future<var>\n"
    "extern \"xl6_xrt_fs_async_tree_stats\" func __xl_fs_async_stats_stats(string path,bool recursive) as future<var>\n";
static const char xl6StdFsAsync_file_openAsyncBody[] =
    "func __xl_std_file_open_async(string path, int flags = 1, int mode = 438, int share = 7) as AsyncFile\n"
    "    return AsyncFile(path,flags,mode,share)\n"
    "end\n";

static const char xl6StdFsAsync_file_takeAsyncBody[] =
    "func __xl_std_file_take_async(File file) as AsyncFile\n"
    "    return AsyncFile(file)\n"
    "end\n";

static const char xl6StdFsAsync_file_readBytesAsyncBody[] =
    "extern \"xl6_xrt_fs_async_whole_read\" func __xl_std_file_read_bytes_async_native(string path,int limit) as future<bytes>\n"
    "func __xl_std_file_read_bytes_async(string path, int limit = 1073741824) as future<bytes>\n"
    "    return __xl_std_file_read_bytes_async_native(path,limit)\n"
    "end\n";

static const char xl6StdFsAsync_file_writeBytesAsyncBody[] =
    "extern \"xl6_xrt_fs_async_whole_write\" func __xl_std_file_write_bytes_async_native(string path,bytes data,int size,int operation) as future<bool>\n"
    "func __xl_std_file_write_bytes_async(string path, bytes data, int size = -1) as future<bool>\n"
    "    return __xl_std_file_write_bytes_async_native(path,data,size,0)\n"
    "end\n";

static const char xl6StdFsAsync_file_appendBytesAsyncBody[] =
    "extern \"xl6_xrt_fs_async_whole_write\" func __xl_std_file_append_bytes_async_native(string path,bytes data,int size,int operation) as future<bool>\n"
    "func __xl_std_file_append_bytes_async(string path, bytes data, int size = -1) as future<bool>\n"
    "    return __xl_std_file_append_bytes_async_native(path,data,size,1)\n"
    "end\n";

static const char xl6StdFsAsync_file_writeBytesAtomicAsyncBody[] =
    "extern \"xl6_xrt_fs_async_whole_write\" func __xl_std_file_write_bytes_atomic_async_native(string path,bytes data,int size,int operation) as future<bool>\n"
    "func __xl_std_file_write_bytes_atomic_async(string path, bytes data, int size = -1) as future<bool>\n"
    "    return __xl_std_file_write_bytes_atomic_async_native(path,data,size,2)\n"
    "end\n";

static const char xl6StdFsAsync_file_readTextAsyncBody[] =
    "extern \"xl6_xrt_fs_async_text_read\" func __xl_std_file_read_text_async_native(string path,int encoding,int policy,int limit) as future<string>\n"
    "func __xl_std_file_read_text_async(string path, int encoding = 0, int policy = 0, int limit = 1073741824) as future<string>\n"
    "    return __xl_std_file_read_text_async_native(path,encoding,policy,limit)\n"
    "end\n";

static const char xl6StdFsAsync_file_writeTextAsyncBody[] =
    "extern \"xl6_xrt_fs_async_text_write\" func __xl_std_file_write_text_async_native(string path,string text,int encoding,int policy,bool bom,int size,int operation) as future<bool>\n"
    "func __xl_std_file_write_text_async(string path,string text,int encoding = 1,int policy = 0,bool bom = false,int size = -1) as future<bool>\n"
    "    return __xl_std_file_write_text_async_native(path,text,encoding,policy,bom,size,0)\n"
    "end\n";

static const char xl6StdFsAsync_file_appendTextAsyncBody[] =
    "extern \"xl6_xrt_fs_async_text_write\" func __xl_std_file_append_text_async_native(string path,string text,int encoding,int policy,bool bom,int size,int operation) as future<bool>\n"
    "func __xl_std_file_append_text_async(string path,string text,int encoding = 1,int policy = 0,int size = -1) as future<bool>\n"
    "    return __xl_std_file_append_text_async_native(path,text,encoding,policy,false,size,1)\n"
    "end\n";

static const char xl6StdFsAsync_file_writeTextAtomicAsyncBody[] =
    "extern \"xl6_xrt_fs_async_text_write\" func __xl_std_file_write_text_atomic_async_native(string path,string text,int encoding,int policy,bool bom,int size,int operation) as future<bool>\n"
    "func __xl_std_file_write_text_atomic_async(string path,string text,int encoding = 1,int policy = 0,bool bom = false,int size = -1) as future<bool>\n"
    "    return __xl_std_file_write_text_atomic_async_native(path,text,encoding,policy,bom,size,2)\n"
    "end\n";

static const char xl6StdFsAsync_file_copyAsyncBody[] =
    "extern \"xl6_xrt_fs_async_copy\" func __xl_std_file_copy_async_native(string source,string target,bool replace) as future<bool>\n"
    "func __xl_std_file_copy_async(string source,string target,bool replace = false) as future<bool>\n"
    "    return __xl_std_file_copy_async_native(source,target,replace)\n"
    "end\n";

static const char xl6StdFsAsync_file_moveAsyncBody[] =
    "extern \"xl6_xrt_fs_async_move\" func __xl_std_file_move_async_native(string source,string target,bool replace) as future<bool>\n"
    "func __xl_std_file_move_async(string source,string target,bool replace = false) as future<bool>\n"
    "    return __xl_std_file_move_async_native(source,target,replace)\n"
    "end\n";

static const char xl6StdFsAsync_file_deleteAsyncBody[] =
    "extern \"xl6_xrt_fs_async_delete\" func __xl_std_file_delete_async_native(string path) as future<bool>\n"
    "func __xl_std_file_delete_async(string path) as future<bool>\n"
    "    return __xl_std_file_delete_async_native(path)\n"
    "end\n";

static const char xl6StdFsAsync_dir_createAsyncBody[] =
    "extern \"xl6_xrt_fs_async_dir_create\" func __xl_std_dir_create_async_native(string path,int mode,bool all) as future<bool>\n"
    "func __xl_std_dir_create_async(string path,int mode = -1) as future<bool>\n"
    "    return __xl_std_dir_create_async_native(path,mode,false)\n"
    "end\n";

static const char xl6StdFsAsync_dir_createAllAsyncBody[] =
    "extern \"xl6_xrt_fs_async_dir_create\" func __xl_std_dir_create_all_async_native(string path,int mode,bool all) as future<bool>\n"
    "func __xl_std_dir_create_all_async(string path,int mode = -1) as future<bool>\n"
    "    return __xl_std_dir_create_all_async_native(path,mode,true)\n"
    "end\n";

static const char xl6StdFsAsync_dir_removeAsyncBody[] =
    "extern \"xl6_xrt_fs_async_dir_remove\" func __xl_std_dir_remove_async_native(string path) as future<bool>\n"
    "func __xl_std_dir_remove_async(string path) as future<bool>\n"
    "    return __xl_std_dir_remove_async_native(path)\n"
    "end\n";

static const char xl6StdFsAsync_dir_emptyAsyncBody[] =
    "extern \"xl6_xrt_fs_async_dir_empty\" func __xl_std_dir_empty_async_native(string path) as future<bool>\n"
    "func __xl_std_dir_empty_async(string path) as future<bool>\n"
    "    return __xl_std_dir_empty_async_native(path)\n"
    "end\n";

static const char xl6StdFsAsync_dir_ensureEmptyAsyncBody[] =
    "extern \"xl6_xrt_fs_async_tree_ensure\" func __xl_std_dir_ensure_empty_async_native(string path) as future<bool>\n"
    "func __xl_std_dir_ensure_empty_async(string path) as future<bool>\n"
    "    return __xl_std_dir_ensure_empty_async_native(path)\n"
    "end\n";

static const char xl6StdFsAsync_dir_moveAsyncBody[] =
    "extern \"xl6_xrt_fs_async_tree_move\" func __xl_std_dir_move_async_native(string source,string target,bool replace) as future<bool>\n"
    "func __xl_std_dir_move_async(string source,string target,bool replace = false) as future<bool>\n"
    "    return __xl_std_dir_move_async_native(source,target,replace)\n"
    "end\n";

static const char xl6StdFsAsync_dir_sizeAsyncBody[] =
    "extern \"xl6_xrt_fs_async_tree_size\" func __xl_std_dir_size_async_native(string path,bool recursive) as future<uint64>\n"
    "func __xl_std_dir_size_async(string path,bool recursive = true) as future<uint64>\n"
    "    return __xl_std_dir_size_async_native(path,recursive)\n"
    "end\n";

static const char xl6StdFsAsync_file_copyTreeAsyncBody[] =
    "async func __xl_std_file_copy_tree_async(string source,string target,int flags = 0) as TreeStats\n"
    "    return TreeStats(await __xl_fs_async_stats_copy(source,target,flags))\n"
    "end\n";

static const char xl6StdFsAsync_file_removeTreeAsyncBody[] =
    "async func __xl_std_file_remove_tree_async(string path,bool keepRoot = false) as TreeStats\n"
    "    return TreeStats(await __xl_fs_async_stats_remove(path,keepRoot))\n"
    "end\n";

static const char xl6StdFsAsync_dir_copyAsyncBody[] =
    "async func __xl_std_dir_copy_async(string source,string target,bool replace = false) as TreeStats\n"
    "    return TreeStats(await __xl_fs_async_stats_copy(source,target,int(replace)))\n"
    "end\n";

static const char xl6StdFsAsync_dir_deleteAsyncBody[] =
    "async func __xl_std_dir_delete_async(string path) as TreeStats\n"
    "    return TreeStats(await __xl_fs_async_stats_remove(path,false))\n"
    "end\n";

static const char xl6StdFsAsync_dir_cleanAsyncBody[] =
    "async func __xl_std_dir_clean_async(string path) as TreeStats\n"
    "    return TreeStats(await __xl_fs_async_stats_remove(path,true))\n"
    "end\n";

static const char xl6StdFsAsync_dir_statsAsyncBody[] =
    "async func __xl_std_dir_stats_async(string path,bool recursive = true) as WalkStats\n"
    "    return WalkStats(await __xl_fs_async_stats_stats(path,recursive))\n"
    "end\n";
