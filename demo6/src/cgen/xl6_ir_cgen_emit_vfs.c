/* Only selected ABI adapters are emitted; all filesystem policy stays in XRT. */
_Static_assert(XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_ID - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE < 64,
    "VFS operation selection must fit its closed mask");
static void xl6IrCgenEmitVfsRuntime(XL6_Output* out, uint64_t operations)
{
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_CREATE - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_create(void) { return xrtVfsCreate(); }\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_DESTROY - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void xl6_xrt_vfs_destroy(void* handle)\n"
            "{\n"
            "    xerror* error = xrtTakeError();\n"
            "    xrtVfsDestroy((xvfs)handle);\n"
            "    xrtClearError(); if (error != NULL) xrtSetErrorTake(error);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_READ - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "xvalue* xl6_xrt_vfs_read(void* vfs, xlir_string path, int64_t limit)\n"
            "{\n"
            "    const char* name;\n"
            "    size_t size = 0;\n"
            "    unsigned char* data;\n"
            "    xvalue* result;\n"
            "    if (limit < 0 || (uint64_t)limit > SIZE_MAX) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "    name = xlir_string_cstr(path); if (name == NULL) return NULL;\n"
            "    data = xrtVfsReadAllLimit((xvfs)vfs, name, (size_t)limit, &size);\n"
            "    if (data == NULL) return NULL;\n"
            "    result = xrtValueBytesTake(&data, size);\n"
            "    if (data != NULL) xrtFree(data);\n"
            "    return result;\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_CREATE - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_memory_create(void) { return xrtVfsMemoryCreate(); }\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_DESTROY - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void xl6_xrt_vfs_memory_destroy(void* handle)\n"
            "{\n"
            "    xerror* error = xrtTakeError();\n"
            "    xrtVfsMemoryDestroy((xvfsmemory)handle);\n"
            "    xrtClearError(); if (error != NULL) xrtSetErrorTake(error);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_PUT_BYTES - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "bool xl6_xrt_vfs_memory_put_bytes(void* memory, xlir_string path, const xvalue* value, int64_t limit)\n"
            "{\n"
            "    const char* name;\n"
            "    xbytesview data = {0};\n"
            "    if (limit < -1) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "    if (!xrtValueGetBytes(value, &data)) { xrtSetErrorKind(XERR_TYPE); return false; }\n"
            "    if (limit >= 0 && (uint64_t)limit < data.Size) data.Size = (size_t)limit;\n"
            "    name = xlir_string_cstr(path);\n"
            "    return name != NULL && xrtVfsMemoryPutCopy((xvfsmemory)memory, name, data.Data, data.Size);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_PUT_STRING - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "bool xl6_xrt_vfs_memory_put_string(void* memory, xlir_string path, xlir_string data, int64_t limit)\n"
            "{\n"
            "    const char* name;\n"
            "    if (limit < -1) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "    if (limit >= 0 && (uint64_t)limit < data.Size) data.Size = (size_t)limit;\n"
            "    name = xlir_string_cstr(path);\n"
            "    return name != NULL && xrtVfsMemoryPutCopy((xvfsmemory)memory, name, data.Data, data.Size);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MEMORY_SEAL - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "bool xl6_xrt_vfs_memory_seal(void* memory) { return xrtVfsMemorySeal((xvfsmemory)memory); }\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_DISK_CREATE - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_disk_create(xlir_string root, int64_t access)\n"
            "{\n"
            "    const char* path;\n"
            "    if (access <= 0 || (uint64_t)access > XVFS_DISK_ACCESS) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }\n"
            "    path = xlir_string_cstr(root);\n"
            "    return path != NULL ? xrtVfsDiskCreate(path, (uint32_t)access) : NULL;\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_DISK_DESTROY - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void xl6_xrt_vfs_disk_destroy(void* handle)\n"
            "{\n"
            "    xerror* error = xrtTakeError();\n"
            "    xrtVfsDiskDestroy((xvfsdisk)handle);\n"
            "    xrtClearError(); if (error != NULL) xrtSetErrorTake(error);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_PACK_CREATE - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_pack_create(xlir_string path, int64_t offset, int64_t length,\n"
            "    int64_t format, int64_t cacheBytes, int64_t maxEntryBytes, int64_t maxDecodedBytes)\n"
            "{\n"
            "    const char* name;\n"
            "    xfile source;\n"
            "    xvfspack pack;\n"
            "    xvfspackoptions options;\n"
            "    uint64_t size;\n"
            "    xerror* error;\n"
            "    if (offset < 0 || length < -1 || format < 0 || format > 2 ||\n"
            "        cacheBytes < 0 || maxEntryBytes <= 0 || maxDecodedBytes <= 0) {\n"
            "        xrtSetErrorKind(XERR_RANGE); return NULL;\n"
            "    }\n"
            "    name = xlir_string_cstr(path); if (name == NULL) return NULL;\n"
            "    source = xrtOpen(name, XFILE_READ); if (source == NULL) return NULL;\n"
            "    if (!xrtFileSize(source, &size) || (uint64_t)offset > size ||\n"
            "        (length >= 0 && (uint64_t)length > size - (uint64_t)offset)) {\n"
            "        if (xrtGetError() == NULL) xrtSetErrorKind(XERR_RANGE);\n"
            "        error = xrtTakeError(); (void)xrtClose(source);\n"
            "        xrtClearError(); if (error != NULL) xrtSetErrorTake(error); return NULL;\n"
            "    }\n"
            "    xrtVfsPackOptionsInit(&options);\n"
            "    options.Format = (xvfspackformat)format;\n"
            "    options.CacheBytes = (uint64_t)cacheBytes;\n"
            "    options.MaxEntryBytes = (uint64_t)maxEntryBytes;\n"
            "    options.MaxDecodedBytes = (uint64_t)maxDecodedBytes;\n"
            "    pack = xrtVfsPackCreate(source, (uint64_t)offset,\n"
            "        length < 0 ? size - (uint64_t)offset : (uint64_t)length, &options);\n"
            "    if (pack == NULL) {\n"
            "        error = xrtTakeError(); (void)xrtClose(source);\n"
            "        xrtClearError(); if (error != NULL) xrtSetErrorTake(error);\n"
            "    }\n"
            "    return pack;\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_PACK_DESTROY - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void xl6_xrt_vfs_pack_destroy(void* handle)\n"
            "{\n"
            "    xerror* error = xrtTakeError();\n"
            "    xrtVfsPackDestroy((xvfspack)handle);\n"
            "    xrtClearError(); if (error != NULL) xrtSetErrorTake(error);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_PACK_FORMAT - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "int64_t xl6_xrt_vfs_pack_format(void* pack) { return (int64_t)xrtVfsPackFormat((xvfspack)pack); }\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_MEMORY - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_mount_memory(void* vfs, void* provider, xlir_string prefix, int64_t priority, int64_t caseMode)\n"
            "{\n"
            "    const char* path;\n"
            "    if (priority < INT32_MIN || priority > INT32_MAX || caseMode < 0 || caseMode > 1) {\n"
            "        xrtSetErrorKind(XERR_RANGE); return NULL;\n"
            "    }\n"
            "    path = xlir_string_cstr(prefix);\n"
            "    return path != NULL ? xrtVfsMemoryMount((xvfs)vfs, path, (int32_t)priority, (xvfscase)caseMode, (xvfsmemory)provider, 0u) : NULL;\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_DISK - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_mount_disk(void* vfs, void* provider, xlir_string prefix, int64_t priority, int64_t caseMode)\n"
            "{\n"
            "    const char* path;\n"
            "    if (priority < INT32_MIN || priority > INT32_MAX || caseMode < 0 || caseMode > 1) {\n"
            "        xrtSetErrorKind(XERR_RANGE); return NULL;\n"
            "    }\n"
            "    path = xlir_string_cstr(prefix);\n"
            "    return path != NULL ? xrtVfsDiskMount((xvfs)vfs, path, (int32_t)priority, (xvfscase)caseMode, (xvfsdisk)provider, 0u) : NULL;\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_PACK - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void* xl6_xrt_vfs_mount_pack(void* vfs, void* provider, xlir_string prefix, int64_t priority, int64_t caseMode)\n"
            "{\n"
            "    const char* path;\n"
            "    if (priority < INT32_MIN || priority > INT32_MAX || caseMode < 0 || caseMode > 1) {\n"
            "        xrtSetErrorKind(XERR_RANGE); return NULL;\n"
            "    }\n"
            "    path = xlir_string_cstr(prefix);\n"
            "    return path != NULL ? xrtVfsPackMount((xvfs)vfs, path, (int32_t)priority, (xvfscase)caseMode, (xvfspack)provider, 0u) : NULL;\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_DESTROY - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "void xl6_xrt_vfs_mount_destroy(void* handle)\n"
            "{\n"
            "    xerror* error = xrtTakeError();\n"
            "    xrtVfsMountDestroy((xvfsmount)handle);\n"
            "    xrtClearError(); if (error != NULL) xrtSetErrorTake(error);\n"
            "}\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_UNMOUNT - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "bool xl6_xrt_vfs_unmount(void* mount) { return xrtVfsUnmount((xvfsmount)mount); }\n", out);
    if (operations & (UINT64_C(1) << (XL6_IR_RUNTIME_BRIDGE_VFS_MOUNT_ID - XL6_IR_RUNTIME_BRIDGE_VFS_CREATE)))
        xl6OutputPuts(
            "uint64_t xl6_xrt_vfs_mount_id(void* mount) { return xrtVfsMountId((xvfsmount)mount); }\n", out);
}
