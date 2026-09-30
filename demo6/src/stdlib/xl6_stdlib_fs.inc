/* Owned File, snapshot metadata and directory resources. */
static const char xl6StdFs_file_tempBody[] =
    "func __xl_std_file_temp(string directory = \"\", string prefix = \"xl-\", string suffix = \".tmp\") as File\n"
    "    return File(directory,prefix,suffix)\n"
    "end\n";

static const char xl6StdFs_dir_rootsBody[] =
    "extern \"xl6_xrt_fs_dir_roots\" func __xl_fs_dir_roots() as array<string>\n"
    "func __xl_std_dir_roots() as array<string>\n"
    "    return __xl_fs_dir_roots()\n"
    "end\n";

static const char xl6StdFs_file_set_timesBody[] =
    "extern \"xl6_xrt_fs_path_set_times\" func __xl_fs_file_set_times(string path, int accessed, int modified, bool setAccessed, bool setModified, bool follow) as bool\n"
    "func __xl_std_file_set_times(string path, int? accessed = null, int? modified = null, bool follow = true) as bool\n"
    "    bool hasAccess = accessed != null\n"
    "    bool hasModify = modified != null\n"
    "    int a = 0\n"
    "    int m = 0\n"
    "    if accessed != null then a = accessed end\n"
    "    if modified != null then m = modified end\n"
    "    return __xl_fs_file_set_times(path,a,m,hasAccess,hasModify,follow)\n"
    "end\n";

static const char xl6StdFs_file_walk_statsBody[] =
    "func __xl_std_file_walk_stats(string path, int flags = 0, int maxDepth = -1) as WalkStats\n"
    "    return WalkStats(path,flags,maxDepth)\n"
    "end\n";

static const char xl6StdFs_dir_statsBody[] =
    "func __xl_std_dir_stats(string path, bool recursive = true) as WalkStats\n"
    "    return WalkStats(path,recursive)\n"
    "end\n";

static const char xl6StdFs_file_copy_treeBody[] =
    "func __xl_std_file_copy_tree(string source, string target, int flags = 0) as TreeStats\n"
    "    return TreeStats(source,target,flags)\n"
    "end\n";

static const char xl6StdFs_file_remove_treeBody[] =
    "func __xl_std_file_remove_tree(string path, bool keepRoot = false) as TreeStats\n"
    "    return TreeStats(path,keepRoot)\n"
    "end\n";

static const char xl6StdFsfilewrite_bytes_atomicBody[] =
    "extern \"xl6_xrt_fs_whole_write\" func __xl_fs_file_write_bytes_atomic(string p0, bytes p1, int p2, int p3) as bool\n"
    "func __xl_std_file_write_bytes_atomic(string path, bytes data, int size = -1) as bool\n"
    "    return __xl_fs_file_write_bytes_atomic(path, data, size, 2)\n"
    "end\n";

static const char xl6StdFsfileread_linkBody[] =
    "extern \"xl6_xrt_fs_link_read\" func __xl_fs_file_read_link(string p0) as string\n"
    "func __xl_std_file_read_link(string path) as string\n"
    "    return __xl_fs_file_read_link(path)\n"
    "end\n";

static const char xl6StdFsfiledelete_linkBody[] =
    "extern \"xl6_xrt_fs_link_delete\" func __xl_fs_file_delete_link(string p0) as bool\n"
    "func __xl_std_file_delete_link(string path) as bool\n"
    "    return __xl_fs_file_delete_link(path)\n"
    "end\n";

static const char xl6StdFsdirremoveBody[] =
    "extern \"xl6_xrt_fs_dir_remove\" func __xl_fs_dir_remove(string p0) as bool\n"
    "func __xl_std_dir_remove(string path) as bool\n"
    "    return __xl_fs_dir_remove(path)\n"
    "end\n";

static const char xl6StdFsfileset_modeBody[] =
    "extern \"xl6_xrt_fs_path_set_mode\" func __xl_fs_file_set_mode(string p0, int p1, bool p2) as bool\n"
    "func __xl_std_file_set_mode(string path, int mode, bool follow = true) as bool\n"
    "    return __xl_fs_file_set_mode(path, mode, follow)\n"
    "end\n";

static const char xl6StdFsfilecreate_linkBody[] =
    "extern \"xl6_xrt_fs_link_create\" func __xl_fs_file_create_link(string p0, string p1, bool p2) as bool\n"
    "func __xl_std_file_create_link(string source, string target, bool directory = false) as bool\n"
    "    return __xl_fs_file_create_link(source, target, directory)\n"
    "end\n";

static const char xl6StdFsfilecreate_hard_linkBody[] =
    "extern \"xl6_xrt_fs_link_hard\" func __xl_fs_file_create_hard_link(string p0, string p1) as bool\n"
    "func __xl_std_file_create_hard_link(string source, string target) as bool\n"
    "    return __xl_fs_file_create_hard_link(source, target)\n"
    "end\n";

static const char xl6StdFsfilecreate_fifoBody[] =
    "extern \"xl6_xrt_fs_fifo_create\" func __xl_fs_file_create_fifo(string p0, int p1) as bool\n"
    "func __xl_std_file_create_fifo(string path, int mode = 438) as bool\n"
    "    return __xl_fs_file_create_fifo(path, mode)\n"
    "end\n";

static const char xl6StdFsdirtempBody[] =
    "extern \"xl6_xrt_fs_dir_temp\" func __xl_fs_dir_temp(string p0, string p1, string p2) as string\n"
    "func __xl_std_dir_temp(string directory = \"\", string prefix = \"xl-\", string suffix = \"\") as string\n"
    "    return __xl_fs_dir_temp(directory, prefix, suffix)\n"
    "end\n";
static const char xl6StdFsPathIsRootBody[] =
    "extern \"xl6_xrt_fs_path_is_root\" func __xl_fs_path_is_root(string path) as bool\n"
    "func __xl_std_path_is_root(string path) as bool\n"
    "    return __xl_fs_path_is_root(path)\n"
    "end\n";

static const char xl6StdFsPathIsRootedBody[] =
    "extern \"xl6_xrt_fs_path_is_rooted\" func __xl_fs_path_is_rooted(string path) as bool\n"
    "func __xl_std_path_is_rooted(string path) as bool\n"
    "    return __xl_fs_path_is_rooted(path)\n"
    "end\n";

static const char xl6StdFsPathStemBody[] =
    "extern \"xl6_xrt_fs_path_stem\" func __xl_fs_path_stem(string path) as string\n"
    "func __xl_std_path_stem(string path) as string\n"
    "    return __xl_fs_path_stem(path)\n"
    "end\n";

static const char xl6StdFsPathParentBody[] =
    "extern \"xl6_xrt_fs_path_parent\" func __xl_fs_path_parent(string path) as string\n"
    "func __xl_std_path_parent(string path) as string\n"
    "    return __xl_fs_path_parent(path)\n"
    "end\n";

static const char xl6StdFsPathRealBody[] =
    "extern \"xl6_xrt_fs_path_real\" func __xl_fs_path_real(string path) as string\n"
    "func __xl_std_path_real(string path) as string\n"
    "    return __xl_fs_path_real(path)\n"
    "end\n";

static const char xl6StdFsPathIsLocalBody[] =
    "extern \"xl6_xrt_fs_path_is_local\" func __xl_fs_path_is_local(string path, int style) as bool\n"
    "func __xl_std_path_is_local(string path, int style = 0) as bool\n"
    "    return __xl_fs_path_is_local(path, style)\n"
    "end\n";

static const char xl6StdFsPathIsSafeBody[] =
    "extern \"xl6_xrt_fs_path_is_safe\" func __xl_fs_path_is_safe(string path, bool directory) as bool\n"
    "func __xl_std_path_is_safe(string path, bool directory = false) as bool\n"
    "    return __xl_fs_path_is_safe(path, directory)\n"
    "end\n";

static const char xl6StdFsPathCleanBody[] =
    "extern \"xl6_xrt_fs_path_clean\" func __xl_fs_path_clean(string path, int style) as string\n"
    "func __xl_std_path_clean(string path, int style = 0) as string\n"
    "    return __xl_fs_path_clean(path, style)\n"
    "end\n";

static const char xl6StdFsPathRelativeBody[] =
    "extern \"xl6_xrt_fs_path_relative\" func __xl_fs_path_relative(string base, string target, int style) as string\n"
    "func __xl_std_path_relative(string base, string target, int style = 0) as string\n"
    "    return __xl_fs_path_relative(base, target, style)\n"
    "end\n";

static const char xl6StdFsPathBuildBody[] =
    "extern \"xl6_xrt_fs_path_build\" func __xl_fs_path_build(array parts, int style) as string\n"
    "func __xl_std_path_build(array parts, int style = 0) as string\n"
    "    return __xl_fs_path_build(parts, style)\n"
    "end\n";

static const char xl6StdFsPathListSepBody[] =
    "extern \"xl6_xrt_fs_path_list_sep\" func __xl_fs_path_list_sep() as string\n"
    "func __xl_std_path_list_sep() as string\n"
    "    return __xl_fs_path_list_sep()\n"
    "end\n";

static const char xl6StdFsPathExecutableBody[] =
    "extern \"xl6_xrt_fs_path_executable\" func __xl_fs_path_executable() as string\n"
    "func __xl_std_path_executable() as string\n"
    "    return __xl_fs_path_executable()\n"
    "end\n";

static const char xl6StdFsPathparseBody[] =
    "func __xl_std_path_parse(string path, int style = 0) as PathParts\n"
    "    return PathParts(path,style)\n"
    "end\n";

static const char xl6StdFsPathcomponentsBody[] =
    "func __xl_std_path_components(string path, int style = 0) as PathIterator\n"
    "    return PathIterator(path,style)\n"
    "end\n";

const char* xl6StdlibFsPathSource =
    "class PathParts\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_path_parts_create\" create(string path, int style)\n"
    "    extern \"xl6_xrt_fs_path_parts_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_path_parts_text\" func textNative(int field) as string\n"
    "    extern \"xl6_xrt_fs_path_parts_number\" func numberNative(int field) as int\n"
    "    get root() as string\n"
    "        return self.textNative(0)\n"
    "    end\n"
    "    get parent() as string\n"
    "        return self.textNative(1)\n"
    "    end\n"
    "    get name() as string\n"
    "        return self.textNative(2)\n"
    "    end\n"
    "    get stem() as string\n"
    "        return self.textNative(3)\n"
    "    end\n"
    "    get ext() as string\n"
    "        return self.textNative(4)\n"
    "    end\n"
    "    get flags() as int\n"
    "        return self.numberNative(0)\n"
    "    end\n"
    "    get rootKind() as int\n"
    "        return self.numberNative(1)\n"
    "    end\n"
    "end\n"
    "class PathComponent\n"
    "    string text\n"
    "    int kind\n"
    "    create(string text, int kind)\n"
    "        self.text = text\n"
    "        self.kind = kind\n"
    "    end\n"
    "end\n"
    "class PathIterator\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_path_iter_create\" create(string path, int style)\n"
    "    extern \"xl6_xrt_fs_path_iter_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_path_iter_advance\" func advance() as bool\n"
    "    extern \"xl6_xrt_fs_path_iter_text\" get text() as string\n"
    "    extern \"xl6_xrt_fs_path_iter_kind\" get kind() as int\n"
    "    func __iter__() as PathIterator\n"
    "        return self\n"
    "    end\n"
    "    func __next__() as (bool,PathComponent?)\n"
    "        if not self.advance() then return false,null end\n"
    "        return true,PathComponent(self.text,self.kind)\n"
    "    end\n"
    "end\n";
const char* xl6StdlibFsFileSource =
    "class File\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_file_temp\" create(string directory, string prefix, string suffix)\n"
    "    extern \"xl6_xrt_fs_file_temp_path\" get tempPath() as string\n"
    "    extern \"xl6_xrt_fs_file_temp_keep\" func keep() as bool\n"
    "    extern \"xl6_xrt_fs_file_open\" create(string path, int flags, int mode, int share)\n"
    "    extern \"xl6_xrt_fs_file_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_file_close\" func close() as bool\n"
    "    extern \"xl6_xrt_fs_file_closed\" get closed() as bool\n"
    "    extern \"xl6_xrt_fs_file_tell\" get pos() as uint64\n"
    "    extern \"xl6_xrt_fs_file_size\" get size() as uint64\n"
    "    extern \"xl6_xrt_fs_file_flags\" get flags() as int\n"
    "    extern \"xl6_xrt_fs_file_capabilities\" get capabilities() as uint64\n"
    "    extern \"xl6_xrt_fs_root_open_file\" create(FileRoot root, string path, int flags, int mode, int share)\n"
    "    func reader() as IoReader\n"
    "        return IoReader(self)\n"
    "    end\n"
    "    func writer() as IoWriter\n"
    "        return IoWriter(self)\n"
    "    end\n"
    "    func map(uint64 offset = uint64(0), int size = 0, int flags = 1) as MappedFile\n"
    "        return MappedFile(self, offset, size, flags)\n"
    "    end\n"
    "    func lock(int mode = 2, uint64 offset = uint64(0), uint64 size = uint64(0), bool wait = false) as FileLock\n"
    "        return FileLock(self, mode, offset, size, wait)\n"
    "    end\n"
    "    extern \"xl6_xrt_fs_file_native\" func native() as int\n"
    "    extern \"xl6_xrt_fs_file_handle\" func nativeHandle() as point\n"
    "    extern \"xl6_xrt_fs_file_read_text\" func readTextNative(int count, int encoding, int policy) as string\n"
    "    func readText(int count = -1, int encoding = 0, int policy = 0) as string\n"
    "        return self.readTextNative(count, encoding, policy)\n"
    "    end\n"
    "    get eof() as bool\n"
    "        return self.pos >= self.size\n"
    "    end\n"
    "    func setEOF() as bool\n"
    "        return self.resize(self.pos)\n"
    "    end\n"
    "    extern \"xl6_xrt_fs_file_seek\" func seekNative(int offset, int origin) as uint64\n"
    "    extern \"xl6_xrt_fs_file_resize\" func resize(uint64 size) as bool\n"
    "    extern \"xl6_xrt_fs_file_flush\" func flush() as bool\n"
    "    extern \"xl6_xrt_fs_file_read\" func readNative(int count, bool exact, int offset) as bytes\n"
    "    extern \"xl6_xrt_fs_file_write\" func writeNative(bytes data, int size, bool full, int offset) as int\n"
    "    extern \"xl6_xrt_fs_file_write_text\" func writeTextNative(string data, int size) as int\n"
    "    func seek(int offset, int origin = 0) as uint64\n"
    "        return self.seekNative(offset, origin)\n"
    "    end\n"
    "    func read(int count) as bytes\n"
    "        return self.readNative(count, false, -1)\n"
    "    end\n"
    "    func readBytes(int count) as bytes\n"
    "        return self.readNative(count, false, -1)\n"
    "    end\n"
    "    func readExact(int count) as bytes\n"
    "        return self.readNative(count, true, -1)\n"
    "    end\n"
    "    func readAt(int offset, int count) as bytes\n"
    "        if offset < 0 then throw RangeError \"negative file offset\" end\n"
    "        return self.readNative(count, false, offset)\n"
    "    end\n"
    "    func readAtExact(int offset, int count) as bytes\n"
    "        if offset < 0 then throw RangeError \"negative file offset\" end\n"
    "        return self.readNative(count, true, offset)\n"
    "    end\n"
    "    func write(bytes data, int size = -1) as int\n"
    "        return self.writeNative(data, size, false, -1)\n"
    "    end\n"
    "    func writeAll(bytes data, int size = -1) as int\n"
    "        return self.writeNative(data, size, true, -1)\n"
    "    end\n"
    "    func writeAt(int offset, bytes data, int size = -1) as int\n"
    "        if offset < 0 then throw RangeError \"negative file offset\" end\n"
    "        return self.writeNative(data, size, false, offset)\n"
    "    end\n"
    "    func writeAtAll(int offset, bytes data, int size = -1) as int\n"
    "        if offset < 0 then throw RangeError \"negative file offset\" end\n"
    "        return self.writeNative(data, size, true, offset)\n"
    "    end\n"
    "    func writeText(string data, int size = -1) as int\n"
    "        return self.writeTextNative(data, size)\n"
    "    end\n"
    "    func write(string data, int size = -1) as int\n"
    "        return self.writeTextNative(data, size)\n"
    "    end\n"
    "    func stat() as FileStat\n"
    "        return FileStat(self)\n"
    "    end\n";
const char* xl6StdlibFsFileVfsSource =
    "    extern \"xl6_xrt_fs_vfs_open\" create(Vfs fs, string path, int flags, int mode, int share)\n";
const char* xl6StdlibFsStatSource =
    "class FileStat\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_info_path\" create(string path, bool follow)\n"
    "    extern \"xl6_xrt_fs_info_file\" create(File file)\n"
    "    extern \"xl6_xrt_fs_root_stat\" create(FileRoot root, string path, bool follow)\n"
    "    extern \"xl6_xrt_fs_info_directory\" create(DirectoryIterator iterator)\n"
    "    extern \"xl6_xrt_fs_info_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_info_type\" get type() as int\n"
    "    extern \"xl6_xrt_fs_info_available\" get available() as int\n"
    "    extern \"xl6_xrt_fs_info_mode\" get mode() as int\n"
    "    extern \"xl6_xrt_fs_info_attributes\" get attributes() as int\n"
    "    extern \"xl6_xrt_fs_info_size\" get size() as uint64\n"
    "    extern \"xl6_xrt_fs_info_device\" get device() as uint64\n"
    "    extern \"xl6_xrt_fs_info_identity\" get identity() as uint64\n"
    "    extern \"xl6_xrt_fs_info_link_count\" get linkCount() as uint64\n"
    "    extern \"xl6_xrt_fs_info_accessed\" get accessed() as int\n"
    "    extern \"xl6_xrt_fs_info_modified\" get modified() as int\n"
    "    extern \"xl6_xrt_fs_info_created\" get created() as int\n"
    "    extern \"xl6_xrt_fs_info_changed\" get changed() as int\n"
    "    extern \"xl6_xrt_fs_info_name\" get name() as string\n"
    "    extern \"xl6_xrt_fs_info_name_bytes\" get nameBytes() as bytes\n"
    "    extern \"xl6_xrt_fs_info_path_text\" get path() as string\n"
    "    extern \"xl6_xrt_fs_info_utf8\" get validUtf8() as bool\n";
const char* xl6StdlibFsStatVfsSource =
    "    extern \"xl6_xrt_fs_info_vfs\" create(Vfs fs, string path, bool follow)\n";
const char* xl6StdlibFsDirectorySource =
    "class DirectoryIterator\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_dir_open\" create(string path, int flags)\n"
    "    extern \"xl6_xrt_fs_root_open_dir\" create(FileRoot root, string path, int flags)\n"
    "    extern \"xl6_xrt_fs_dir_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_dir_close\" func close() as bool\n"
    "    extern \"xl6_xrt_fs_dir_advance\" func advance() as bool\n"
    "    extern \"xl6_xrt_fs_dir_path\" get path() as string\n"
    "    func current() as FileStat\n"
    "        return FileStat(self)\n"
    "    end\n"
    "    func __iter__() as DirectoryIterator\n"
    "        return self\n"
    "    end\n"
    "    func __next__() as (bool, FileStat?)\n"
    "        if not self.advance() then return false, null end\n"
    "        return true, self.current()\n"
    "    end\n";
const char* xl6StdlibFsDirectoryVfsSource =
    "    extern \"xl6_xrt_fs_vfs_dir_open\" create(Vfs fs, string path, int flags)\n";
static const char xl6StdFsStatBody[] =
    "func __xl_std_file_stat(string path, bool follow = true) as FileStat\n"
    "    return FileStat(path, follow)\n"
    "end\n";
static const char xl6StdFsDirectoryBody[] =
    "func __xl_std_dir_open(string path, int flags = 0) as DirectoryIterator\n"
    "    return DirectoryIterator(path, flags)\n"
    "end\n";
const char* xl6StdlibFsResourceSource =
    "class FileRoot\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_root_create\" create(string path)\n"
    "    extern \"xl6_xrt_fs_root_open_root\" create(FileRoot root, string path)\n"
    "    extern \"xl6_xrt_fs_root_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_root_close\" func close() as bool\n"
    "    extern \"xl6_xrt_fs_root_path\" get path() as string\n"
    "    extern \"xl6_xrt_fs_root_mkdir\" func createDirNative(string path, int mode) as bool\n"
    "    extern \"xl6_xrt_fs_root_remove\" func remove(string path) as bool\n"
    "    extern \"xl6_xrt_fs_root_read_link\" func readLink(string path) as string\n"
    "    extern \"xl6_xrt_fs_root_symlink\" func createLinkNative(string target, string path, bool directory) as bool\n"
    "    extern \"xl6_xrt_fs_root_hardlink\" func createHardLink(string source, string path) as bool\n"
    "    extern \"xl6_xrt_fs_root_fifo\" func createFifoNative(string path, int mode) as bool\n"
    "    extern \"xl6_xrt_fs_root_mode\" func setModeNative(string path, bool follow, int mode) as bool\n"
    "    func open(string path, int flags = 1, int mode = 438, int share = 7) as File\n"
    "        return File(self, path, flags, mode, share)\n"
    "    end\n"
    "    func openDir(string path, int flags = 0) as DirectoryIterator\n"
    "        return DirectoryIterator(self, path, flags)\n"
    "    end\n"
    "    func openRoot(string path) as FileRoot\n"
    "        return FileRoot(self, path)\n"
    "    end\n"
    "    func stat(string path, bool follow = true) as FileStat\n"
    "        return FileStat(self, path, follow)\n"
    "    end\n"
    "    func createDir(string path, int mode = 511) as bool\n"
    "        return self.createDirNative(path, mode)\n"
    "    end\n"
    "    func createLink(string target, string path, bool directory = false) as bool\n"
    "        return self.createLinkNative(target, path, directory)\n"
    "    end\n"
    "    func createFifo(string path, int mode = 438) as bool\n"
    "        return self.createFifoNative(path, mode)\n"
    "    end\n"
    "    func setMode(string path, int mode, bool follow = true) as bool\n"
    "        return self.setModeNative(path, follow, mode)\n"
    "    end\n"
    "end\n"
    "class MappedFile\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_map_create\" create(File file, uint64 offset, int size, int flags)\n"
    "    extern \"xl6_xrt_fs_map_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_map_close\" func close() as bool\n"
    "    extern \"xl6_xrt_fs_map_size\" get size() as uint64\n"
    "    extern \"xl6_xrt_fs_map_bytes\" func bytesNative(int offset, int size) as bytes\n"
    "    extern \"xl6_xrt_fs_map_write\" func writeNative(int offset, bytes data, int size) as int\n"
    "    extern \"xl6_xrt_fs_map_flush\" func flushNative(int offset, int size) as bool\n"
    "    func bytes(int offset = 0, int size = -1) as bytes\n"
    "        return self.bytesNative(offset, size)\n"
    "    end\n"
    "    func write(int offset, bytes data, int size = -1) as int\n"
    "        return self.writeNative(offset, data, size)\n"
    "    end\n"
    "    func flush(int offset = 0, int size = -1) as bool\n"
    "        return self.flushNative(offset, size)\n"
    "    end\n"
    "end\n"
    "class FileLock\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_lock_create\" create(File file, int mode, uint64 offset, uint64 size, bool wait)\n"
    "    extern \"xl6_xrt_fs_lock_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_lock_close\" func close() as bool\n"
    "end\n";
static const char xl6StdFsRootBody[] =
    "func __xl_std_file_root(string path) as FileRoot\n"
    "    return FileRoot(path)\n"
    "end\n";
const char* xl6StdlibFsTreeSource =
    "class TreeStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_tree_copy\" create(string source, string target, int flags)\n"
    "    extern \"xl6_xrt_fs_tree_remove\" create(string path, bool keepRoot)\n"
    "    extern \"xl6_xrt_fs_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_stats_value\" func valueNative(int field) as uint64\n"
    "    extern \"xl6_xrt_fs_stats_stopped\" get stopped() as bool\n"
    "    get items() as uint64\n"
    "        return self.valueNative(0)\n"
    "    end\n"
    "    get files() as uint64\n"
    "        return self.valueNative(1)\n"
    "    end\n"
    "    get directories() as uint64\n"
    "        return self.valueNative(2)\n"
    "    end\n"
    "    get links() as uint64\n"
    "        return self.valueNative(3)\n"
    "    end\n"
    "    get others() as uint64\n"
    "        return self.valueNative(4)\n"
    "    end\n"
    "    get bytes() as uint64\n"
    "        return self.valueNative(5)\n"
    "    end\n"
    "end\n"
    "class WalkStats\n"
    "    native point handle\n"
    "    extern \"xl6_xrt_fs_walk_stats\" create(string path, int flags, int maxDepth)\n"
    "    extern \"xl6_xrt_fs_walk_visit\" create(string path, func(var) as int visit, func(var) as int onError, int flags, int maxDepth)\n"
    "    extern \"xl6_xrt_fs_dir_stats\" create(string path, bool recursive)\n"
    "    extern \"xl6_xrt_fs_stats_destroy\" destroy()\n"
    "    extern \"xl6_xrt_fs_stats_value\" func valueNative(int field) as uint64\n"
    "    extern \"xl6_xrt_fs_stats_stopped\" get stopped() as bool\n"
    "    get items() as uint64\n"
    "        return self.valueNative(0)\n"
    "    end\n"
    "    get files() as uint64\n"
    "        return self.valueNative(1)\n"
    "    end\n"
    "    get directories() as uint64\n"
    "        return self.valueNative(2)\n"
    "    end\n"
    "    get links() as uint64\n"
    "        return self.valueNative(3)\n"
    "    end\n"
    "    get others() as uint64\n"
    "        return self.valueNative(4)\n"
    "    end\n"
    "    get bytes() as uint64\n"
    "        return self.valueNative(5)\n"
    "    end\n"
    "end\n";
