/* 命名共享内存只保存原始字节；调用方负责跨进程同步与数据协议。 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/sharedmem_native.h"

#define XSHM_DOMAIN "xlang.extlib.sharedmem"
#define XSHM_LIMIT (64u * 1024u * 1024u)
#define XSHM_MAGIC "XLSHM1\0"

typedef struct xshm_header {
    char Magic[8];
    uint64_t Size;
} xshm_header;

typedef struct xshm_handle {
    char Name[129];
    size_t Size;
    size_t MappingSize;
    unsigned char* Mapping;
    int Owner;
#ifdef _WIN32
    HANDLE Handle;
#else
    int Descriptor;
    char NativeName[138];
#endif
} xshm_handle;

static void xshm_error(xerrkind Kind, const char* Message)
{
    xrtSetErrorInfo(Kind, XSHM_DOMAIN, 0, Message);
}

static bool xshm_name(const char* Name)
{
    size_t Size;
    size_t i;
    if (Name == NULL) return false;
    for (Size = 0u; Size <= 128u && Name[Size] != '\0'; ++Size) {}
    if (Size == 0u || Size > 128u ||
        !xrtUtf8Valid((xstrview){Name, Size}, NULL)) return false;
    for (i = 0u; i < Size; ++i)
        if (Name[i] == '/' || Name[i] == '\\' ||
            (unsigned char)Name[i] < 32u || (unsigned char)Name[i] == 127u)
            return false;
    return true;
}

#ifdef _WIN32
static bool xshm_native_name(const char* Name, wchar_t* Wide, size_t Capacity)
{
    static const wchar_t Prefix[] = L"Local\\xlang.";
    size_t PrefixLength = sizeof(Prefix) / sizeof(Prefix[0]) - 1u;
    int Count;
    if (Capacity <= PrefixLength) return false;
    memcpy(Wide, Prefix, PrefixLength * sizeof(wchar_t));
    Count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        Name, -1, Wide + PrefixLength, (int)(Capacity - PrefixLength));
    return Count > 0;
}
#endif

static xshm_handle* xshm_new(const char* Name)
{
    xshm_handle* Shared = (xshm_handle*)xrtCalloc(1u, sizeof(*Shared));
    if (Shared == NULL) {
        xshm_error(XERR_MEMORY, "could not allocate shared memory handle");
        return NULL;
    }
    memcpy(Shared->Name, Name, strlen(Name) + 1u);
#ifndef _WIN32
    Shared->Descriptor = -1;
    snprintf(Shared->NativeName, sizeof(Shared->NativeName), "/xlang.%s", Name);
#endif
    return Shared;
}

#ifndef _WIN32
static void xshm_unlink_owned(const xshm_handle* Shared)
{
    struct stat Original, Current;
    int Named;
    if (Shared->Descriptor < 0 || fstat(Shared->Descriptor, &Original) != 0)
        return;
    Named = shm_open(Shared->NativeName, O_RDONLY, 0);
    if (Named < 0) return;
    /* 显式 unlink 后名称可能已被重用，不能删除另一个共享段。 */
    if (fstat(Named, &Current) == 0 &&
        Original.st_dev == Current.st_dev &&
        Original.st_ino == Current.st_ino)
        shm_unlink(Shared->NativeName);
    close(Named);
}
#endif

void* xshm_create(const char* Name, int64_t Size)
{
    xshm_handle* Shared;
    size_t MappingSize;
    xrtClearError();
    if (!xshm_name(Name) || Size <= 0 || (uint64_t)Size > XSHM_LIMIT) {
        xshm_error(XERR_ARGUMENT, "invalid shared memory name or size");
        return NULL;
    }
    MappingSize = sizeof(xshm_header) + (size_t)Size;
    Shared = xshm_new(Name);
    if (Shared == NULL) return NULL;
#ifdef _WIN32
    {
        wchar_t Wide[150];
        DWORD Status;
        if (!xshm_native_name(Name, Wide, 150u)) goto fail;
        Shared->Handle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL,
            PAGE_READWRITE, 0u, (DWORD)MappingSize, Wide);
        Status = GetLastError();
        if (Shared->Handle == NULL || Status == ERROR_ALREADY_EXISTS) {
            xshm_error(Status == ERROR_ALREADY_EXISTS ? XERR_EXISTS : XERR_IO,
                "could not create exclusive named mapping");
            goto fail;
        }
        Shared->Mapping = (unsigned char*)MapViewOfFile(Shared->Handle,
            FILE_MAP_ALL_ACCESS, 0u, 0u, MappingSize);
        if (Shared->Mapping == NULL) goto fail;
    }
#else
    Shared->Descriptor = shm_open(Shared->NativeName,
        O_CREAT | O_EXCL | O_RDWR, 0600);
    if (Shared->Descriptor < 0) {
        xshm_error(errno == EEXIST ? XERR_EXISTS : XERR_IO,
            "could not create exclusive named mapping");
        goto fail;
    }
    if (ftruncate(Shared->Descriptor, (off_t)MappingSize) != 0) goto fail;
    Shared->Mapping = (unsigned char*)mmap(NULL, MappingSize,
        PROT_READ | PROT_WRITE, MAP_SHARED, Shared->Descriptor, 0);
    if (Shared->Mapping == MAP_FAILED) {
        Shared->Mapping = NULL;
        goto fail;
    }
#endif
    Shared->Size = (size_t)Size;
    Shared->MappingSize = MappingSize;
    Shared->Owner = 1;
    /* 新建匿名页/共享段本来就是零填充，仅初始化协议头，避免预触碰全部页。 */
    memset(Shared->Mapping, 0, sizeof(xshm_header));
    memcpy(Shared->Mapping, XSHM_MAGIC, 8u);
    ((xshm_header*)Shared->Mapping)->Size = (uint64_t)Size;
    return Shared;

fail:
    if (xrtGetError() == NULL)
        xshm_error(XERR_IO, "could not map shared memory");
#ifdef _WIN32
    if (Shared->Mapping != NULL) UnmapViewOfFile(Shared->Mapping);
    if (Shared->Handle != NULL) CloseHandle(Shared->Handle);
#else
    if (Shared->Mapping != NULL) munmap(Shared->Mapping, MappingSize);
    if (Shared->Descriptor >= 0) {
        xshm_unlink_owned(Shared);
        close(Shared->Descriptor);
    }
#endif
    xrtFree(Shared);
    return NULL;
}

void* xshm_open(const char* Name)
{
    xshm_handle* Shared;
    xshm_header Header;
    xrtClearError();
    if (!xshm_name(Name)) {
        xshm_error(XERR_ARGUMENT, "invalid shared memory name");
        return NULL;
    }
    Shared = xshm_new(Name);
    if (Shared == NULL) return NULL;
#ifdef _WIN32
    {
        wchar_t Wide[150];
        void* HeaderView;
        if (!xshm_native_name(Name, Wide, 150u)) goto fail;
        Shared->Handle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, Wide);
        if (Shared->Handle == NULL) {
            xshm_error(GetLastError() == ERROR_FILE_NOT_FOUND ? XERR_NOT_FOUND : XERR_IO,
                "named mapping was not found or could not be opened");
            goto fail;
        }
        HeaderView = MapViewOfFile(Shared->Handle,
            FILE_MAP_READ, 0u, 0u, sizeof(Header));
        if (HeaderView == NULL) goto fail;
        memcpy(&Header, HeaderView, sizeof(Header));
        UnmapViewOfFile(HeaderView);
        if (memcmp(Header.Magic, XSHM_MAGIC, 8u) != 0 ||
            Header.Size == 0u || Header.Size > XSHM_LIMIT) goto fail;
        Shared->MappingSize = sizeof(Header) + (size_t)Header.Size;
        Shared->Mapping = (unsigned char*)MapViewOfFile(Shared->Handle,
            FILE_MAP_ALL_ACCESS, 0u, 0u, Shared->MappingSize);
        if (Shared->Mapping == NULL) goto fail;
    }
#else
    {
        struct stat Info;
        Shared->Descriptor = shm_open(Shared->NativeName, O_RDWR, 0);
        if (Shared->Descriptor < 0) {
            xshm_error(errno == ENOENT ? XERR_NOT_FOUND : XERR_IO,
                "named mapping was not found or could not be opened");
            goto fail;
        }
        if (fstat(Shared->Descriptor, &Info) != 0 ||
            Info.st_size < (off_t)(sizeof(Header) + 1u) ||
            Info.st_size > (off_t)(sizeof(Header) + XSHM_LIMIT)) goto fail;
        Shared->MappingSize = (size_t)Info.st_size;
        Shared->Mapping = (unsigned char*)mmap(NULL, Shared->MappingSize,
            PROT_READ | PROT_WRITE, MAP_SHARED, Shared->Descriptor, 0);
        if (Shared->Mapping == MAP_FAILED) {
            Shared->Mapping = NULL;
            goto fail;
        }
        memcpy(&Header, Shared->Mapping, sizeof(Header));
        if (memcmp(Header.Magic, XSHM_MAGIC, 8u) != 0 ||
            Header.Size != Shared->MappingSize - sizeof(Header)) goto fail;
    }
#endif
    if (memcmp(Shared->Mapping, &Header, sizeof(Header)) != 0) goto fail;
    Shared->Size = (size_t)Header.Size;
    return Shared;

fail:
    if (xrtGetError() == NULL)
        xshm_error(XERR_IO, "could not open valid named mapping");
#ifdef _WIN32
    if (Shared->Mapping != NULL) UnmapViewOfFile(Shared->Mapping);
    if (Shared->Handle != NULL) CloseHandle(Shared->Handle);
#else
    if (Shared->Mapping != NULL) munmap(Shared->Mapping, Shared->MappingSize);
    if (Shared->Descriptor >= 0) close(Shared->Descriptor);
#endif
    xrtFree(Shared);
    return NULL;
}

void* xshm_adopt(void* Shared)
{
    return Shared;
}

void xshm_close(void* Handle)
{
    xshm_handle* Shared = (xshm_handle*)Handle;
    if (Shared == NULL) return;
#ifdef _WIN32
    if (Shared->Mapping != NULL) UnmapViewOfFile(Shared->Mapping);
    if (Shared->Handle != NULL) CloseHandle(Shared->Handle);
#else
    if (Shared->Mapping != NULL) munmap(Shared->Mapping, Shared->MappingSize);
    if (Shared->Owner) xshm_unlink_owned(Shared);
    if (Shared->Descriptor >= 0) close(Shared->Descriptor);
#endif
    xrtFree(Shared);
}

int xshm_unlink(const char* Name)
{
    xrtClearError();
    if (!xshm_name(Name)) {
        xshm_error(XERR_ARGUMENT, "invalid shared memory name");
        return 0;
    }
#ifdef _WIN32
    xshm_error(XERR_UNSUPPORTED,
        "Windows mapping names disappear when the last handle closes");
    return 0;
#else
    {
        char NativeName[138];
        snprintf(NativeName, sizeof(NativeName), "/xlang.%s", Name);
        if (shm_unlink(NativeName) != 0) {
            xshm_error(errno == ENOENT ? XERR_NOT_FOUND : XERR_IO,
                "could not unlink named mapping");
            return 0;
        }
    }
    return 1;
#endif
}

int64_t xshm_size(void* Handle)
{
    xrtClearError();
    if (Handle == NULL) {
        xshm_error(XERR_ARGUMENT, "shared memory handle is null");
        return 0;
    }
    return (int64_t)((xshm_handle*)Handle)->Size;
}

const char* xshm_name_of(void* Handle)
{
    xrtClearError();
    if (Handle == NULL) {
        xshm_error(XERR_ARGUMENT, "shared memory handle is null");
        return "";
    }
    return ((xshm_handle*)Handle)->Name;
}

static bool xshm_range(xshm_handle* Shared, int64_t Offset, size_t Length)
{
    if (Shared == NULL || Offset < 0 ||
        (uint64_t)Offset > Shared->Size ||
        Length > Shared->Size - (size_t)Offset) {
        xshm_error(XERR_RANGE, "shared memory range is out of bounds");
        return false;
    }
    return true;
}

int xshm_write(void* Handle, int64_t Offset, const void* Data, size_t Length)
{
    xshm_handle* Shared = (xshm_handle*)Handle;
    xrtClearError();
    if (!xshm_range(Shared, Offset, Length) ||
        (Data == NULL && Length != 0u)) {
        if (Data == NULL && Length != 0u)
            xshm_error(XERR_ARGUMENT, "write data is null");
        return 0;
    }
    if (Length != 0u)
        memcpy(Shared->Mapping + sizeof(xshm_header) + (size_t)Offset,
            Data, Length);
    return 1;
}

int xshm_write_abi(void* Handle, int64_t Offset, void* Data, size_t Length)
{
    return xshm_write(Handle, Offset, Data, Length);
}

void* xshm_read(void* Handle, int64_t Offset, size_t Length)
{
    xshm_handle* Shared = (xshm_handle*)Handle;
    xbuffer* Result;
    xrtClearError();
    if (!xshm_range(Shared, Offset, Length)) return NULL;
    Result = xrtBufferCreate();
    if (Result == NULL ||
        (Length != 0u && !xrtBufferAppend(Result,
            (xbytesview){ Shared->Mapping + sizeof(xshm_header) +
                (size_t)Offset, Length }))) {
        xrtBufferDestroy(Result);
        xshm_error(XERR_MEMORY, "could not copy shared memory bytes");
        return NULL;
    }
    return Result;
}

int xshm_write_int64(void* Handle, int64_t Offset, int64_t Value)
{
    unsigned char Bytes[8];
    uint64_t Bits = (uint64_t)Value;
    unsigned i;
    for (i = 0u; i < 8u; ++i) Bytes[i] = (unsigned char)(Bits >> (i * 8u));
    return xshm_write(Handle, Offset, Bytes, sizeof(Bytes));
}

int xshm_read_int64(void* Handle, int64_t Offset, int64_t* Value)
{
    xshm_handle* Shared = (xshm_handle*)Handle;
    uint64_t Bits = 0u;
    unsigned i;
    xrtClearError();
    if (Value == NULL) {
        xshm_error(XERR_ARGUMENT, "read destination is null");
        return 0;
    }
    if (!xshm_range(Shared, Offset, 8u)) return 0;
    for (i = 0u; i < 8u; ++i)
        Bits |= (uint64_t)Shared->Mapping[sizeof(xshm_header) +
            (size_t)Offset + i] << (i * 8u);
    memcpy(Value, &Bits, sizeof(Bits));
    return 1;
}

int64_t xshm_read_int64_value(void* Handle, int64_t Offset)
{
    int64_t Value = 0;
    xshm_read_int64(Handle, Offset, &Value);
    return Value;
}

int xshm_zero(void* Handle)
{
    xshm_handle* Shared = (xshm_handle*)Handle;
    xrtClearError();
    if (Shared == NULL) {
        xshm_error(XERR_ARGUMENT, "shared memory handle is null");
        return 0;
    }
    memset(Shared->Mapping + sizeof(xshm_header), 0, Shared->Size);
    return 1;
}

int xshm_write_text(void* handle, int64_t offset, xstrview text)
{
    return xshm_write(handle, offset, text.Data, text.Size);
}
