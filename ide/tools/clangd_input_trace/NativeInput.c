/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#define WIN32_LEAN_AND_MEAN
#include "NativeInput.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>

static int fail(unsigned long *error, DWORD code) {
  if (error) *error = code;
  return 0;
}
int xlangNativeStatus(void *handle, xlang_native_stamp *stamp,
                     unsigned long *error) {
  BY_HANDLE_FILE_INFORMATION info;
  if (GetFileType(handle) != FILE_TYPE_DISK)
    return fail(error, ERROR_NOT_SUPPORTED);
  if (!GetFileInformationByHandle(handle, &info))
    return fail(error, GetLastError());
  if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    return fail(error, ERROR_NOT_SUPPORTED);
  memset(stamp, 0, sizeof(*stamp));
  stamp->index = ((uint64_t)info.nFileIndexHigh << 32) | info.nFileIndexLow;
  stamp->size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;
  stamp->modified = ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32) |
                    info.ftLastWriteTime.dwLowDateTime;
  stamp->volume = info.dwVolumeSerialNumber;
  stamp->attributes = info.dwFileAttributes;
  if (error) *error = 0;
  return 1;
}
int xlangNativeOpen(const char *path, int read, void **handle,
                    xlang_native_stamp *stamp, unsigned long *error) {
  *handle = NULL;
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
  if (!n || n > 32768) return fail(error, ERROR_INVALID_NAME);
  wchar_t *wide = malloc(((size_t)n + 8) * sizeof(*wide));
  if (!wide) return fail(error, ERROR_NOT_ENOUGH_MEMORY);
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, n);
  for (int i = 0; i < n; ++i) if (wide[i] == L'/') wide[i] = L'\\';
  /* Extended absolute paths preserve Unicode and long path support. */
  if (n >= 4 && wide[1] == L':' && wide[2] == L'\\') {
    memmove(wide + 4, wide, (size_t)n * sizeof(*wide));
    memcpy(wide, L"\\\\?\\", 4 * sizeof(*wide));
  } else if (n >= 3 && wide[0] == L'\\' && wide[1] == L'\\' &&
             wide[2] != L'?') {
    memmove(wide + 8, wide + 2, ((size_t)n - 2) * sizeof(*wide));
    memcpy(wide, L"\\\\?\\UNC\\", 8 * sizeof(*wide));
  }
  DWORD attrs = GetFileAttributesW(wide);
  DWORD code = GetLastError();
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    free(wide);
    return fail(error, code);
  }
  if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) ||
      (read && (attrs & FILE_ATTRIBUTE_DIRECTORY))) {
    free(wide);
    return fail(error, ERROR_NOT_SUPPORTED);
  }
  HANDLE file = CreateFileW(wide, read ? GENERIC_READ : FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  code = GetLastError();
  free(wide);
  if (file == INVALID_HANDLE_VALUE) return fail(error, code);
  if (!xlangNativeStatus(file, stamp, error)) {
    CloseHandle(file);
    return 0;
  }
  *handle = file;
  return 1;
}
int xlangNativeRead(void *handle, char **bytes, size_t *size,
                   xlang_native_stamp *stamp, unsigned long *error) {
  xlang_native_stamp before, after;
  *bytes = NULL;
  *size = 0;
  if (!xlangNativeStatus(handle, &before, error)) return 0;
  if ((before.attributes & FILE_ATTRIBUTE_DIRECTORY) ||
      before.size > 512ULL * 1024 * 1024)
    return fail(error, ERROR_NOT_SUPPORTED);
  char *data = malloc((size_t)before.size + 1);
  if (!data) return fail(error, ERROR_NOT_ENOUGH_MEMORY);
  LARGE_INTEGER zero = {0};
  if (!SetFilePointerEx(handle, zero, NULL, FILE_BEGIN)) {
    DWORD code = GetLastError();
    free(data);
    return fail(error, code);
  }
  size_t total = 0;
  while (total < before.size) {
    DWORD chunk = before.size - total > 1024 * 1024 ? 1024 * 1024 :
                  (DWORD)(before.size - total), got = 0;
    if (!ReadFile(handle, data + total, chunk, &got, NULL) || !got) {
      DWORD code = got ? GetLastError() : ERROR_RETRY;
      free(data);
      return fail(error, code);
    }
    total += got;
  }
  data[total] = 0;
  if (!xlangNativeStatus(handle, &after, error) ||
      memcmp(&before, &after, sizeof(before))) {
    free(data);
    return fail(error, ERROR_RETRY);
  }
  *stamp = after;
  *bytes = data;
  *size = total;
  if (error) *error = 0;
  return 1;
}
void xlangNativeClose(void *handle) { if (handle) CloseHandle(handle); }
void xlangNativeFree(void *bytes) { free(bytes); }
