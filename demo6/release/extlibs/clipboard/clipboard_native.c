#include <windows.h>
#include <shellapi.h>
#include <limits.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "include/clipboard.h"

#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x00000080
#endif

#if defined(__TINYC__)
#ifndef CF_HDROP
#define CF_HDROP 15
#endif
#ifndef HDROP
#define HDROP HANDLE
#endif
UINT WINAPI DragQueryFileW(HDROP hDrop, UINT iFile, LPWSTR lpszFile, UINT cch);
#endif

#if defined(_WIN32) || defined(_WIN64)
#define XL_EXPORT __declspec(dllexport)
#else
#define XL_EXPORT
#endif

typedef struct XClipDropFiles {
	DWORD pFiles;
	POINT pt;
	BOOL fNC;
	BOOL fWide;
} XClipDropFiles;

static void xclip_set_error(const char* text);
static void xclip_set_win_error(const char* op);

typedef struct XClipSnapshotItem {
	UINT format;
	int kind;
	SIZE_T size;
	unsigned char* data;
	HANDLE handle;
} XClipSnapshotItem;

#define XCLIP_SNAPSHOT_GLOBAL 1
#define XCLIP_SNAPSHOT_BITMAP 2
#define XCLIP_SNAPSHOT_ENHMETAFILE 3
#define XCLIP_SNAPSHOT_METAFILEPICT 4
#define XCLIP_MAX_IMAGE_BYTES (256u * 1024u * 1024u)
#define XCLIP_MAX_SNAPSHOT_BYTES (256u * 1024u * 1024u)
#define XCLIP_MAX_SNAPSHOT_FORMATS 256u

typedef struct XClipSnapshot {
	uint32_t magic;
	XClipSnapshotItem* snapshot_items;
	size_t snapshot_count;
	size_t total_bytes;
} XClipSnapshot;

#define XCLIP_SNAPSHOT_MAGIC UINT32_C(0x58434c53)

static void xclip_free_handle(int kind, HANDLE handle)
{
	if ( handle == NULL ) return;
	if ( kind == XCLIP_SNAPSHOT_BITMAP ) DeleteObject(handle);
	else if ( kind == XCLIP_SNAPSHOT_ENHMETAFILE ) DeleteEnhMetaFile((HENHMETAFILE)handle);
	else if ( kind == XCLIP_SNAPSHOT_METAFILEPICT ) {
		METAFILEPICT* picture = (METAFILEPICT*)GlobalLock((HGLOBAL)handle);
		if ( picture != NULL ) {
			if ( picture->hMF != NULL ) DeleteMetaFile(picture->hMF);
			GlobalUnlock((HGLOBAL)handle);
		}
		GlobalFree((HGLOBAL)handle);
	}
	else GlobalFree((HGLOBAL)handle);
}

/* CF_METAFILEPICT 的 HGLOBAL 内还持有一个 HMETAFILE，两个句柄都要独立复制。 */
static HGLOBAL xclip_copy_metafile_pict(HGLOBAL source, size_t* out_bytes)
{
	METAFILEPICT* original;
	METAFILEPICT* copy_data;
	HGLOBAL copy;
	UINT bits;
	if ( source == NULL || GlobalSize(source) < sizeof(METAFILEPICT) ) {
		xclip_set_error("invalid metafile picture");
		return NULL;
	}
	original = (METAFILEPICT*)GlobalLock(source);
	if ( original == NULL ) {
		xclip_set_win_error("GlobalLock");
		return NULL;
	}
	bits = original->hMF != NULL ? GetMetaFileBitsEx(original->hMF, 0u, NULL) : 0u;
	if ( bits == 0u || bits > XCLIP_MAX_SNAPSHOT_BYTES - sizeof(METAFILEPICT) ) {
		GlobalUnlock(source);
		xclip_set_error("invalid or oversized metafile picture");
		return NULL;
	}
	copy = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
	if ( copy == NULL ) {
		GlobalUnlock(source);
		xclip_set_win_error("GlobalAlloc");
		return NULL;
	}
	copy_data = (METAFILEPICT*)GlobalLock(copy);
	if ( copy_data == NULL ) {
		GlobalUnlock(source);
		GlobalFree(copy);
		xclip_set_win_error("GlobalLock");
		return NULL;
	}
	*copy_data = *original;
	copy_data->hMF = CopyMetaFileW(original->hMF, NULL);
	GlobalUnlock(source);
	if ( copy_data->hMF == NULL ) {
		GlobalUnlock(copy);
		GlobalFree(copy);
		xclip_set_win_error("CopyMetaFileW");
		return NULL;
	}
	GlobalUnlock(copy);
	if ( out_bytes != NULL ) *out_bytes = (size_t)bits + sizeof(METAFILEPICT);
	return copy;
}

static void xclip_snapshot_clear(XClipSnapshot* snapshot)
{
	size_t i;
	for ( i = 0u; i < snapshot->snapshot_count; ++i ) {
		if ( snapshot->snapshot_items[i].kind != XCLIP_SNAPSHOT_GLOBAL )
			xclip_free_handle(snapshot->snapshot_items[i].kind, snapshot->snapshot_items[i].handle);
		xrtFree(snapshot->snapshot_items[i].data);
	}
	xrtFree(snapshot->snapshot_items);
	snapshot->snapshot_items = NULL;
	snapshot->snapshot_count = 0u;
	snapshot->total_bytes = 0u;
}

static int xclip_snapshot_append(XClipSnapshot* snapshot, UINT format, const void* data, SIZE_T size)
{
	XClipSnapshotItem* next;
	unsigned char* copy;
	if ( data == NULL || size == 0u ) {
		return 1;
	}
	if ( snapshot->snapshot_count >= XCLIP_MAX_SNAPSHOT_FORMATS ||
		size > XCLIP_MAX_SNAPSHOT_BYTES - snapshot->total_bytes ) {
		xclip_set_error("clipboard snapshot limit exceeded");
		return 0;
	}
	copy = (unsigned char*)xrtMalloc((size_t)size);
	if ( copy == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	memcpy(copy, data, (size_t)size);
	next = (XClipSnapshotItem*)xrtRealloc(snapshot->snapshot_items, (snapshot->snapshot_count + 1u) * sizeof(XClipSnapshotItem));
	if ( next == NULL ) {
		xrtFree(copy);
		xclip_set_error("out of memory");
		return 0;
	}
	snapshot->snapshot_items = next;
	snapshot->snapshot_items[snapshot->snapshot_count].format = format;
	snapshot->snapshot_items[snapshot->snapshot_count].kind = XCLIP_SNAPSHOT_GLOBAL;
	snapshot->snapshot_items[snapshot->snapshot_count].size = size;
	snapshot->snapshot_items[snapshot->snapshot_count].data = copy;
	snapshot->snapshot_items[snapshot->snapshot_count].handle = NULL;
	snapshot->snapshot_count++;
	snapshot->total_bytes += (size_t)size;
	return 1;
}

static int xclip_snapshot_append_handle(XClipSnapshot* snapshot, UINT format, HANDLE handle, int kind, size_t bytes)
{
	XClipSnapshotItem* next;
	if ( handle == NULL ) {
		return 1;
	}
	if ( snapshot->snapshot_count >= XCLIP_MAX_SNAPSHOT_FORMATS ||
		bytes > XCLIP_MAX_SNAPSHOT_BYTES - snapshot->total_bytes ) {
		xclip_free_handle(kind, handle);
		xclip_set_error("clipboard snapshot limit exceeded");
		return 0;
	}
	next = (XClipSnapshotItem*)xrtRealloc(snapshot->snapshot_items, (snapshot->snapshot_count + 1u) * sizeof(XClipSnapshotItem));
	if ( next == NULL ) {
		xclip_free_handle(kind, handle);
		xclip_set_error("out of memory");
		return 0;
	}
	snapshot->snapshot_items = next;
	snapshot->snapshot_items[snapshot->snapshot_count].format = format;
	snapshot->snapshot_items[snapshot->snapshot_count].kind = kind;
	snapshot->snapshot_items[snapshot->snapshot_count].size = 0u;
	snapshot->snapshot_items[snapshot->snapshot_count].data = NULL;
	snapshot->snapshot_items[snapshot->snapshot_count].handle = handle;
	snapshot->snapshot_count++;
	snapshot->total_bytes += bytes;
	return 1;
}

static void xclip_set_error(const char* text)
{
	if ( text == NULL || text[0] == '\0' ) errno = 0;
	else if ( strstr(text, "out of memory") != NULL ) errno = ENOMEM;
	else if ( strstr(text, "not available") != NULL || strstr(text, "empty") != NULL ) errno = ENOENT;
	else if ( strstr(text, "invalid") != NULL || strstr(text, "must be") != NULL ||
		strstr(text, "is null") != NULL ) errno = EINVAL;
	else if ( strstr(text, "limit") != NULL || strstr(text, "too large") != NULL ) errno = ERANGE;
	else errno = EIO;
}

static void xclip_set_xrt_error(void)
{
	const xerror* error = xrtGetError();
	switch ( error != NULL ? xrtErrorKind(error) : XERR_IO ) {
	case XERR_MEMORY: errno = ENOMEM; break;
	case XERR_NOT_FOUND: errno = ENOENT; break;
	case XERR_ARGUMENT:
	case XERR_TYPE:
	case XERR_VALUE: errno = EINVAL; break;
	case XERR_RANGE: errno = ERANGE; break;
	default: errno = EIO; break;
	}
}


/* 创建字符串 Value 并把所有权移入数组。 */
static int xclip_array_append_text(xvalue* pArray, char** pText)
{
	xvalue* pValue;

	pValue = xrtValueStringTake(pText, strlen(*pText));
	if ( pValue == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	if ( !xrtValueArrayAppendTake(pArray, &pValue) ) {
		xrtValueRelease(pValue);
		xclip_set_error("cannot append clipboard value");
		return 0;
	}
	return 1;
}

static void xclip_set_win_error(const char* op)
{
	DWORD code = GetLastError();
	(void)op;
	errno = code == ERROR_NOT_ENOUGH_MEMORY ? ENOMEM :
		(code == ERROR_INVALID_PARAMETER ? EINVAL :
		(code == ERROR_FILE_NOT_FOUND ? ENOENT : EIO));
}

static xvalue* xclip_take_text(char* text)
{
	xvalue* result = xrtValueStringTake(&text, strlen(text));
	xrtFree(text);
	return result;
}

static xvalue* xclip_value_from_owned(char* text)
{
	xvalue* result;
	if ( text == NULL ) return NULL;
	result = xclip_take_text(text);
	if ( result == NULL ) xclip_set_xrt_error();
	return result;
}

/* 两处 JSON 写入器不产生内嵌 NUL，末尾补零后直接移交缓冲。 */
static char* xclip_buffer_text_owned(xbuffer* buffer)
{
	char* text;
	if ( !xrtBufferAppendByte(buffer, 0u) ) {
		xrtBufferUnit(buffer);
		return NULL;
	}
	text = (char*)xrtBufferTake(buffer, NULL, NULL);
	xrtBufferUnit(buffer);
	return text;
}

static int xclip_text_append_len(xbuffer* buffer, const char* text, size_t len)
{
	if ( text == NULL || len == 0u ) return 1;
	if ( xrtBufferAppend(buffer, (xbytesview){ (const uint8*)text, len }) ) return 1;
	xclip_set_error("clipboard text result is too large or out of memory");
	return 0;
}

static int xclip_text_append(xbuffer* buffer, const char* text)
{
	return xclip_text_append_len(buffer, text, text != NULL ? strlen(text) : 0u);
}

static int xclip_text_appendf(xbuffer* buffer, const char* fmt, ...)
{
	char stack[256];
	va_list args;
	int n;
	char* heap;
	int ok;

	va_start(args, fmt);
	n = vsnprintf(stack, sizeof(stack), fmt, args);
	va_end(args);
	if ( n < 0 ) {
		return 0;
	}
	if ( (size_t)n < sizeof(stack) ) {
		return xclip_text_append_len(buffer, stack, (size_t)n);
	}
	heap = (char*)xrtMalloc((size_t)n + 1u);
	if ( heap == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	va_start(args, fmt);
	vsnprintf(heap, (size_t)n + 1u, fmt, args);
	va_end(args);
	ok = xclip_text_append_len(buffer, heap, (size_t)n);
	xrtFree(heap);
	return ok;
}

static int xclip_json_escape_append(xbuffer* buffer, const char* text)
{
	const unsigned char* p = (const unsigned char*)text;
	if ( p == NULL ) {
		return 1;
	}
	while ( *p != '\0' ) {
		char tmp[8];
		int ok;
		if ( *p == '"' || *p == '\\' ) {
			tmp[0] = '\\';
			tmp[1] = (char)*p;
			tmp[2] = '\0';
			ok = xclip_text_append(buffer, tmp);
		} else if ( *p == '\n' ) {
			ok = xclip_text_append(buffer, "\\n");
		} else if ( *p == '\r' ) {
			ok = xclip_text_append(buffer, "\\r");
		} else if ( *p == '\t' ) {
			ok = xclip_text_append(buffer, "\\t");
		} else if ( *p < 0x20u ) {
			snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned int)*p);
			ok = xclip_text_append(buffer, tmp);
		} else {
			ok = xclip_text_append_len(buffer, (const char*)p, 1u);
		}
		if ( !ok ) return 0;
		++p;
	}
	return 1;
}

static int xclip_open(void)
{
	int i;
	for ( i = 0; i < 25; ++i ) {
		if ( OpenClipboard(NULL) ) {
			return 1;
		}
		Sleep(10);
	}
	xclip_set_win_error("OpenClipboard");
	return 0;
}

static wchar_t* xclip_utf8_to_wide_alloc(const char* src)
{
	wchar_t* out;
	int n;
	if ( src == NULL ) {
		src = "";
	}
	n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, NULL, 0);
	if ( n <= 0 ) {
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out = (wchar_t*)xrtCalloc((size_t)n, sizeof(wchar_t));
	if ( out == NULL ) {
		xclip_set_error("out of memory");
		return NULL;
	}
	if ( MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, out, n) <= 0 ) {
		xrtFree(out);
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	return out;
}

static wchar_t* xclip_xvalue_text_to_wide_alloc(const xvalue* pValue)
{
	xstrview Text;
	wchar_t* out;
	int n;
	int size;
	if ( xrtValueType(pValue) != XVALUE_STRING ||
		!xrtValueGetString(pValue, &Text) ||
		(Text.Size > (size_t)INT_MAX) ||
		(Text.Size != 0u && memchr(Text.Data, '\0', Text.Size) != NULL) ) {
		xclip_set_error("invalid file path item");
		return NULL;
	}
	if ( Text.Size == 0u ) {
		out = (wchar_t*)xrtCalloc(1u, sizeof(wchar_t));
		if ( out == NULL ) {
			xclip_set_error("out of memory");
		}
		return out;
	}
	size = (int)Text.Size;
	n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.Data, size, NULL, 0);
	if ( n <= 0 ) {
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out = (wchar_t*)xrtCalloc((size_t)n + 1u, sizeof(wchar_t));
	if ( out == NULL ) {
		xclip_set_error("out of memory");
		return NULL;
	}
	if ( MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Text.Data, size, out, n) <= 0 ) {
		xrtFree(out);
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out[n] = L'\0';
	return out;
}

static char* xclip_wide_to_utf8(const wchar_t* src)
{
	if ( src == NULL ) {
		xclip_set_error("wide text is null");
		return NULL;
	}
	return xrtUtf16To8((const uint16*)src, NULL);
}

static char* xclip_multibyte_to_utf8(const char* src, UINT code_page)
{
	wchar_t* wide;
	int n;
	char* out;
	if ( src == NULL ) {
		xclip_set_error("text is null");
		return NULL;
	}
	n = MultiByteToWideChar(code_page, 0, src, -1, NULL, 0);
	if ( n <= 0 ) {
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	wide = (wchar_t*)xrtCalloc((size_t)n, sizeof(wchar_t));
	if ( wide == NULL ) {
		xclip_set_error("out of memory");
		return NULL;
	}
	if ( MultiByteToWideChar(code_page, 0, src, -1, wide, n) <= 0 ) {
		xrtFree(wide);
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out = xclip_wide_to_utf8(wide);
	xrtFree(wide);
	return out;
}

static const char* xclip_standard_format_name(UINT fmt)
{
	switch ( fmt ) {
	case CF_TEXT: return "CF_TEXT";
	case CF_BITMAP: return "CF_BITMAP";
	case CF_METAFILEPICT: return "CF_METAFILEPICT";
	case CF_SYLK: return "CF_SYLK";
	case CF_DIF: return "CF_DIF";
	case CF_TIFF: return "CF_TIFF";
	case CF_OEMTEXT: return "CF_OEMTEXT";
	case CF_DIB: return "CF_DIB";
	case CF_PALETTE: return "CF_PALETTE";
	case CF_PENDATA: return "CF_PENDATA";
	case CF_RIFF: return "CF_RIFF";
	case CF_WAVE: return "CF_WAVE";
	case CF_UNICODETEXT: return "CF_UNICODETEXT";
	case CF_ENHMETAFILE: return "CF_ENHMETAFILE";
	case CF_HDROP: return "CF_HDROP";
	case CF_LOCALE: return "CF_LOCALE";
	case CF_DIBV5: return "CF_DIBV5";
	default: return NULL;
	}
}

static char* xclip_format_name_alloc(UINT fmt)
{
	const char* standard = xclip_standard_format_name(fmt);
	char fallback[32];
	char* name;
	size_t capacity = 256u;
	if ( standard != NULL ) {
		name = xrtStrDup(standard);
		if ( name == NULL ) xclip_set_error("out of memory");
		return name;
	}
	// Win32 会截断过小的缓冲区；接近边界时重新读取完整名称。
	while ( capacity <= 65536u ) {
		wchar_t* wide = (wchar_t*)xrtCalloc(capacity, sizeof(wchar_t));
		int length;
		if ( wide == NULL ) {
			xclip_set_error("out of memory");
			return NULL;
		}
		length = GetClipboardFormatNameW(fmt, wide, (int)capacity);
		if ( length > 0 && (size_t)length >= capacity - 1u ) {
			xrtFree(wide);
			capacity *= 2u;
			continue;
		}
		if ( length > 0 ) {
			name = xclip_wide_to_utf8(wide);
			xrtFree(wide);
			if ( name == NULL ) xclip_set_error("invalid clipboard format name");
			return name;
		}
		xrtFree(wide);
		snprintf(fallback, sizeof(fallback), "FORMAT_%u", (unsigned int)fmt);
		name = xrtStrDup(fallback);
		if ( name == NULL ) xclip_set_error("out of memory");
		return name;
	}
	xclip_set_error("clipboard format name exceeds the supported limit");
	return NULL;
}

static void xclip_write_u32_le(unsigned char* p, DWORD value)
{
	p[0] = (unsigned char)(value & 0xffu);
	p[1] = (unsigned char)((value >> 8) & 0xffu);
	p[2] = (unsigned char)((value >> 16) & 0xffu);
	p[3] = (unsigned char)((value >> 24) & 0xffu);
}

static DWORD xclip_read_u32_le(const unsigned char* p)
{
	return ((DWORD)p[0]) | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

static DWORD xclip_dib_pixel_offset(const void* dib, SIZE_T dib_size)
{
	const BITMAPINFOHEADER* bi = (const BITMAPINFOHEADER*)dib;
	DWORD header_size;
	DWORD color_count = 0u;
	DWORD masks_size = 0u;
	uint64_t offset;
	if ( dib == NULL || dib_size < sizeof(BITMAPINFOHEADER) ) {
		return 0u;
	}
	header_size = bi->biSize;
	if ( header_size < sizeof(BITMAPINFOHEADER) || header_size > dib_size ) {
		return 0u;
	}
	if ( bi->biCompression == BI_BITFIELDS && header_size == sizeof(BITMAPINFOHEADER) ) {
		masks_size = 3u * sizeof(DWORD);
	}
	if ( bi->biClrUsed != 0 ) {
		color_count = bi->biClrUsed;
	} else if ( bi->biBitCount > 0 && bi->biBitCount <= 8 ) {
		color_count = 1u << bi->biBitCount;
	}
	offset = (uint64_t)header_size + masks_size +
		(uint64_t)color_count * sizeof(RGBQUAD);
	if ( offset > dib_size || offset > UINT32_MAX ) {
		return 0u;
	}
	return (DWORD)offset;
}

/* BMP 文件剥掉 14 字节文件头后直接成为 CF_DIB，不能保留未声明的像素间隙。 */
static int xclip_validate_bmp(const unsigned char* file, DWORD size)
{
	const BITMAPINFOHEADER* bi;
	DWORD offset;
	uint64_t row_bits, row_bytes, pixel_bytes, height;
	if ( size < 14u + sizeof(BITMAPINFOHEADER) || file[0] != 'B' ||
		file[1] != 'M' || xclip_read_u32_le(file + 2) != size ) {
		return 0;
	}
	bi = (const BITMAPINFOHEADER*)(file + 14);
	offset = xclip_dib_pixel_offset(file + 14, size - 14u);
	if ( offset == 0u || xclip_read_u32_le(file + 10) != 14u + offset ||
		bi->biWidth <= 0 || bi->biHeight == 0 || bi->biHeight == INT_MIN ||
		bi->biPlanes != 1 ||
		!(bi->biBitCount == 1 || bi->biBitCount == 4 || bi->biBitCount == 8 ||
		  bi->biBitCount == 16 || bi->biBitCount == 24 || bi->biBitCount == 32) ||
		!(bi->biCompression == BI_RGB || bi->biCompression == BI_BITFIELDS) ||
		(bi->biCompression == BI_BITFIELDS &&
		 bi->biBitCount != 16 && bi->biBitCount != 32) ) {
		return 0;
	}
	height = bi->biHeight < 0 ? (uint64_t)-(int64_t)bi->biHeight :
		(uint64_t)bi->biHeight;
	row_bits = (uint64_t)bi->biWidth * bi->biBitCount;
	row_bytes = ((row_bits + 31u) / 32u) * 4u;
	pixel_bytes = row_bytes * height;
	return pixel_bytes <= size - 14u - offset &&
		(bi->biSizeImage == 0u || bi->biSizeImage >= pixel_bytes);
}

static HGLOBAL xclip_bitmap_to_dib(HBITMAP bitmap)
{
	BITMAP bm;
	BITMAPINFOHEADER* bi;
	HGLOBAL hglobal;
	void* memory;
	unsigned char* pixels;
	HDC dc;
	int height_abs;
	DWORD row_stride;
	DWORD pixel_bytes;
	DWORD total_bytes;
	uint64_t required;
	if ( bitmap == NULL ) {
		xclip_set_error("bitmap handle is null");
		return NULL;
	}
	if ( GetObjectW(bitmap, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 ||
		bm.bmHeight == 0 || bm.bmHeight == INT_MIN ) {
		xclip_set_win_error("GetObjectW");
		return NULL;
	}
	height_abs = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
	required = (uint64_t)bm.bmWidth * 4u * (uint64_t)height_abs +
		sizeof(BITMAPINFOHEADER);
	if ( required > XCLIP_MAX_IMAGE_BYTES ) {
		xclip_set_error("clipboard bitmap is too large");
		return NULL;
	}
	row_stride = (DWORD)bm.bmWidth * 4u;
	pixel_bytes = row_stride * (DWORD)height_abs;
	total_bytes = (DWORD)sizeof(BITMAPINFOHEADER) + pixel_bytes;
	hglobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total_bytes);
	if ( hglobal == NULL ) {
		xclip_set_win_error("GlobalAlloc");
		return NULL;
	}
	memory = GlobalLock(hglobal);
	if ( memory == NULL ) {
		GlobalFree(hglobal);
		xclip_set_win_error("GlobalLock");
		return NULL;
	}
	bi = (BITMAPINFOHEADER*)memory;
	bi->biSize = sizeof(BITMAPINFOHEADER);
	bi->biWidth = bm.bmWidth;
	bi->biHeight = -height_abs;
	bi->biPlanes = 1;
	bi->biBitCount = 32;
	bi->biCompression = BI_RGB;
	bi->biSizeImage = pixel_bytes;
	pixels = (unsigned char*)memory + sizeof(BITMAPINFOHEADER);
	dc = GetDC(NULL);
	if ( dc == NULL ) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
		xclip_set_win_error("GetDC");
		return NULL;
	}
	if ( GetDIBits(dc, bitmap, 0, (UINT)height_abs, pixels, (BITMAPINFO*)bi, DIB_RGB_COLORS) == 0 ) {
		ReleaseDC(NULL, dc);
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
		xclip_set_win_error("GetDIBits");
		return NULL;
	}
	ReleaseDC(NULL, dc);
	GlobalUnlock(hglobal);
	xclip_set_error("");
	return hglobal;
}

static unsigned char* xclip_bmp_from_dib(const void* dib, SIZE_T dib_size,
	DWORD* out_size)
{
	DWORD pixel_offset;
	DWORD file_size;
	DWORD bmp_offset;
	unsigned char* file_data;
	*out_size = 0u;
	if ( dib == NULL || dib_size < sizeof(BITMAPINFOHEADER) ||
		dib_size > XCLIP_MAX_IMAGE_BYTES ) {
		xclip_set_error("invalid DIB data");
		return NULL;
	}
	pixel_offset = xclip_dib_pixel_offset(dib, dib_size);
	if ( pixel_offset == 0u ) {
		xclip_set_error("unsupported DIB header");
		return NULL;
	}
	file_size = 14u + (DWORD)dib_size;
	bmp_offset = 14u + pixel_offset;
	file_data = (unsigned char*)xrtMalloc((size_t)file_size);
	if ( file_data == NULL ) {
		xclip_set_error("out of memory");
		return NULL;
	}
	memset(file_data, 0, 14u);
	file_data[0] = 'B';
	file_data[1] = 'M';
	xclip_write_u32_le(file_data + 2, file_size);
	xclip_write_u32_le(file_data + 10, bmp_offset);
	memcpy(file_data + 14u, dib, (size_t)dib_size);
	*out_size = file_size;
	return file_data;
}

static HGLOBAL xclip_get_clipboard_dib(UINT* out_format, int* out_owned)
{
	HGLOBAL hglobal = NULL;
	if ( out_format != NULL ) {
		*out_format = 0u;
	}
	if ( out_owned != NULL ) {
		*out_owned = 0;
	}
	if ( IsClipboardFormatAvailable(CF_DIBV5) ) {
		hglobal = (HGLOBAL)GetClipboardData(CF_DIBV5);
		if ( hglobal != NULL ) {
			if ( out_format != NULL ) {
				*out_format = CF_DIBV5;
			}
			return hglobal;
		}
	}
	if ( IsClipboardFormatAvailable(CF_DIB) ) {
		hglobal = (HGLOBAL)GetClipboardData(CF_DIB);
		if ( hglobal != NULL ) {
			if ( out_format != NULL ) {
				*out_format = CF_DIB;
			}
			return hglobal;
		}
	}
	if ( IsClipboardFormatAvailable(CF_BITMAP) ) {
		HBITMAP bitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
		hglobal = xclip_bitmap_to_dib(bitmap);
		if ( hglobal != NULL ) {
			if ( out_format != NULL ) {
				*out_format = CF_BITMAP;
			}
			if ( out_owned != NULL ) {
				*out_owned = 1;
			}
			return hglobal;
		}
	}
	xclip_set_error("clipboard image is not available");
	return NULL;
}

XL_EXPORT int64_t x_clipboard_available(void)
{
	xclip_set_error("");
	return 1;
}

XL_EXPORT const char* x_clipboard_version(void)
{
	return "clipboard 0.1.0";
}

XL_EXPORT const char* x_clipboard_last_error(void)
{
	switch ( errno ) {
	case 0: return "";
	case ENOMEM: return "clipboard: out of memory";
	case ENOENT: return "clipboard: data is not available";
	case EINVAL: return "clipboard: invalid input or data";
	case ERANGE: return "clipboard: size limit exceeded";
	default: return "clipboard: operation failed";
	}
}

XL_EXPORT int64_t x_clipboard_clear(void)
{
	int ok;
	if ( !xclip_open() ) {
		return 0;
	}
	ok = EmptyClipboard() ? 1 : 0;
	if ( !ok ) {
		xclip_set_win_error("EmptyClipboard");
	} else {
		xclip_set_error("");
	}
	CloseClipboard();
	return ok;
}

XL_EXPORT void* x_clipboard_snapshot(void)
{
	XClipSnapshot capture;
	XClipSnapshot* snapshot;
	UINT fmt = 0u;
	int ok = 1;
	memset(&capture, 0, sizeof(capture));
	if ( !xclip_open() ) {
		return NULL;
	}
	for (;;) {
		SetLastError(NO_ERROR);
		fmt = EnumClipboardFormats(fmt);
		if ( fmt == 0u ) {
			if ( GetLastError() != NO_ERROR ) {
				xclip_set_win_error("EnumClipboardFormats");
				ok = 0;
			}
			break;
		}
		HANDLE hdata = GetClipboardData(fmt);
		SIZE_T size;
		void* data;
		if ( hdata == NULL ) {
			xclip_set_win_error("GetClipboardData");
			ok = 0;
			break;
		}
		if ( fmt == CF_BITMAP ) {
			BITMAP bitmap;
			uint64_t height;
			size_t bytes;
			if ( GetObjectW(hdata, sizeof(bitmap), &bitmap) < (int)sizeof(bitmap) ||
				bitmap.bmWidthBytes <= 0 ) {
				xclip_set_error("invalid clipboard bitmap");
				ok = 0;
				break;
			}
			height = bitmap.bmHeight < 0 ? (uint64_t)(-(int64_t)bitmap.bmHeight) : (uint64_t)bitmap.bmHeight;
			if ( height > XCLIP_MAX_SNAPSHOT_BYTES / (size_t)bitmap.bmWidthBytes ) {
				xclip_set_error("clipboard snapshot limit exceeded");
				ok = 0;
				break;
			}
			bytes = (size_t)bitmap.bmWidthBytes * (size_t)height;
			if ( capture.snapshot_count >= XCLIP_MAX_SNAPSHOT_FORMATS ||
				bytes > XCLIP_MAX_SNAPSHOT_BYTES - capture.total_bytes ) {
				xclip_set_error("clipboard snapshot limit exceeded");
				ok = 0;
				break;
			}
			HANDLE copy = CopyImage(hdata, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			if ( copy == NULL ) {
				xclip_set_win_error("CopyImage");
				ok = 0;
				break;
			}
			if ( !xclip_snapshot_append_handle(&capture, fmt, copy, XCLIP_SNAPSHOT_BITMAP, bytes) ) {
				ok = 0;
				break;
			}
			continue;
		}
		if ( fmt == CF_ENHMETAFILE ) {
			UINT bytes = GetEnhMetaFileBits((HENHMETAFILE)hdata, 0u, NULL);
			if ( bytes == 0u ) {
				xclip_set_win_error("GetEnhMetaFileBits");
				ok = 0;
				break;
			}
			if ( capture.snapshot_count >= XCLIP_MAX_SNAPSHOT_FORMATS ||
				bytes > XCLIP_MAX_SNAPSHOT_BYTES - capture.total_bytes ) {
				xclip_set_error("clipboard snapshot limit exceeded");
				ok = 0;
				break;
			}
			HENHMETAFILE copy = CopyEnhMetaFileW((HENHMETAFILE)hdata, NULL);
			if ( copy == NULL ) {
				xclip_set_win_error("CopyEnhMetaFileW");
				ok = 0;
				break;
			}
			if ( !xclip_snapshot_append_handle(&capture, fmt, (HANDLE)copy, XCLIP_SNAPSHOT_ENHMETAFILE, bytes) ) {
				ok = 0;
				break;
			}
			continue;
		}
		if ( fmt == CF_METAFILEPICT ) {
			size_t bytes = 0u;
			HGLOBAL copy = xclip_copy_metafile_pict((HGLOBAL)hdata, &bytes);
			if ( copy == NULL ) { ok = 0; break; }
			if ( !xclip_snapshot_append_handle(&capture, fmt, copy,
				XCLIP_SNAPSHOT_METAFILEPICT, bytes) ) { ok = 0; break; }
			continue;
		}
		/* GDI/owner-display 格式不是可复制的 HGLOBAL 数据。 */
		if ( fmt == CF_PALETTE ||
			fmt == CF_OWNERDISPLAY || fmt == CF_DSPBITMAP ||
			fmt == CF_DSPMETAFILEPICT || fmt == CF_DSPENHMETAFILE ) {
			xclip_set_error("clipboard format cannot be snapshotted");
			ok = 0;
			break;
		}
		size = GlobalSize((HGLOBAL)hdata);
		if ( size == 0u ) {
			xclip_set_error("clipboard format has no HGLOBAL data");
			ok = 0;
			break;
		}
		data = GlobalLock((HGLOBAL)hdata);
		if ( data == NULL ) {
			xclip_set_win_error("GlobalLock");
			ok = 0;
			break;
		}
		if ( !xclip_snapshot_append(&capture, fmt, data, size) ) {
			GlobalUnlock((HGLOBAL)hdata);
			ok = 0;
			break;
		}
		GlobalUnlock((HGLOBAL)hdata);
	}
	CloseClipboard();
	if ( !ok ) {
		xclip_snapshot_clear(&capture);
		return NULL;
	}
	snapshot = (XClipSnapshot*)xrtMalloc(sizeof(*snapshot));
	if ( snapshot == NULL ) {
		xclip_snapshot_clear(&capture);
		xclip_set_error("out of memory");
		return NULL;
	}
	*snapshot = capture;
	snapshot->magic = XCLIP_SNAPSHOT_MAGIC;
	xclip_set_error("");
	return snapshot;
}

XL_EXPORT int64_t x_clipboard_restore(void* handle)
{
	XClipSnapshot* snapshot = (XClipSnapshot*)handle;
	HANDLE* copies;
	size_t i;
	int ok = 0;
	if ( snapshot == NULL || snapshot->magic != XCLIP_SNAPSHOT_MAGIC ) {
		xclip_set_error("clipboard snapshot is not available");
		return 0;
	}
	if ( snapshot->snapshot_count > SIZE_MAX / sizeof(HANDLE) ) {
		xclip_set_error("clipboard snapshot is too large");
		return 0;
	}
	copies = (HANDLE*)xrtCalloc(snapshot->snapshot_count == 0u ? 1u : snapshot->snapshot_count, sizeof(HANDLE));
	if ( copies == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	/* 先准备所有待转交句柄，避免清空剪贴板之后再遇到分配失败。 */
	for ( i = 0u; i < snapshot->snapshot_count; ++i ) {
		HANDLE hdata = NULL;
		if ( snapshot->snapshot_items[i].kind == XCLIP_SNAPSHOT_GLOBAL ) {
			void* memory;
			hdata = GlobalAlloc(GMEM_MOVEABLE, snapshot->snapshot_items[i].size);
			if ( hdata == NULL ) {
				xclip_set_win_error("GlobalAlloc");
				goto cleanup;
			}
			memory = GlobalLock((HGLOBAL)hdata);
			if ( memory == NULL ) {
				GlobalFree((HGLOBAL)hdata);
				xclip_set_win_error("GlobalLock");
				goto cleanup;
			}
			memcpy(memory, snapshot->snapshot_items[i].data, (size_t)snapshot->snapshot_items[i].size);
			GlobalUnlock((HGLOBAL)hdata);
		} else if ( snapshot->snapshot_items[i].kind == XCLIP_SNAPSHOT_BITMAP ) {
			hdata = CopyImage(snapshot->snapshot_items[i].handle, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			if ( hdata == NULL ) {
				xclip_set_win_error("CopyImage");
				goto cleanup;
			}
		} else if ( snapshot->snapshot_items[i].kind == XCLIP_SNAPSHOT_ENHMETAFILE ) {
			hdata = (HANDLE)CopyEnhMetaFileW((HENHMETAFILE)snapshot->snapshot_items[i].handle, NULL);
			if ( hdata == NULL ) {
				xclip_set_win_error("CopyEnhMetaFileW");
				goto cleanup;
			}
		} else if ( snapshot->snapshot_items[i].kind == XCLIP_SNAPSHOT_METAFILEPICT ) {
			hdata = (HANDLE)xclip_copy_metafile_pict(
				(HGLOBAL)snapshot->snapshot_items[i].handle, NULL);
			if ( hdata == NULL ) goto cleanup;
		} else {
			xclip_set_error("unsupported snapshot format");
			goto cleanup;
		}
		copies[i] = hdata;
	}
	if ( !xclip_open() ) goto cleanup;
	if ( !EmptyClipboard() ) {
		xclip_set_win_error("EmptyClipboard");
		CloseClipboard();
		goto cleanup;
	}
	for ( i = 0u; i < snapshot->snapshot_count; ++i ) {
		if ( SetClipboardData(snapshot->snapshot_items[i].format, copies[i]) == NULL ) {
			xclip_set_win_error("SetClipboardData");
			break;
		}
		copies[i] = NULL;
	}
	CloseClipboard();
	ok = i == snapshot->snapshot_count;
cleanup:
	for ( i = 0u; i < snapshot->snapshot_count; ++i )
		xclip_free_handle(snapshot->snapshot_items[i].kind, copies[i]);
	xrtFree(copies);
	if ( ok ) xclip_set_error("");
	return ok;
}

XL_EXPORT int64_t x_clipboard_snapshot_release(void* handle)
{
	XClipSnapshot* snapshot = (XClipSnapshot*)handle;
	if ( snapshot == NULL || snapshot->magic != XCLIP_SNAPSHOT_MAGIC ) return 0;
	snapshot->magic = 0u;
	xclip_snapshot_clear(snapshot);
	xrtFree(snapshot);
	return 1;
}

XL_EXPORT int64_t x_clipboard_sequence_number(void)
{
	return (int64_t)GetClipboardSequenceNumber();
}

XL_EXPORT int64_t x_clipboard_count_formats(void)
{
	return CountClipboardFormats();
}

XL_EXPORT xvalue* x_clipboard_formats(void)
{
	xvalue* out = xrtValueArray();
	UINT fmt = 0u;
	if ( out == NULL ) {
		return NULL;
	}
	if ( !xclip_open() ) {
		xrtValueRelease(out);
		return NULL;
	}
	for (;;) {
		SetLastError(NO_ERROR);
		fmt = EnumClipboardFormats(fmt);
		if ( fmt == 0u ) {
			if ( GetLastError() != NO_ERROR ) {
				xclip_set_win_error("EnumClipboardFormats");
				CloseClipboard();
				xrtValueRelease(out);
				return NULL;
			}
			break;
		}
		char* name = xclip_format_name_alloc(fmt);
		int appended = name != NULL && xclip_array_append_text(out, &name);
		xrtFree(name);
		if ( !appended ) {
			CloseClipboard();
			xrtValueRelease(out);
			return NULL;
		}
	}
	xclip_set_error("");
	CloseClipboard();
	return out;
}

static char* xclip_formats_json_owned(void)
{
	xbuffer buffer;
	char* result;
	UINT fmt = 0u;
	int first = 1;
	if ( !xrtBufferInit(&buffer) ) return NULL;
	if ( !xclip_text_append(&buffer, "[") ) goto failed_without_clipboard;
	if ( !xclip_open() ) goto failed_without_clipboard;
	for (;;) {
		char* name;
		int appended;
		SetLastError(NO_ERROR);
		fmt = EnumClipboardFormats(fmt);
		if ( fmt == 0u ) {
			if ( GetLastError() != NO_ERROR ) {
				xclip_set_win_error("EnumClipboardFormats");
				goto fail;
			}
			break;
		}
		if ( !first && !xclip_text_append(&buffer, ",") ) goto fail;
		name = xclip_format_name_alloc(fmt);
		if ( name == NULL ) goto fail;
		first = 0;
		appended = xclip_text_appendf(&buffer, "{\"id\":%u,\"name\":\"", (unsigned int)fmt) &&
			xclip_json_escape_append(&buffer, name) && xclip_text_append(&buffer, "\"}");
		xrtFree(name);
		if ( !appended ) goto fail;
	}
	CloseClipboard();
	if ( !xclip_text_append(&buffer, "]") ) goto failed_without_clipboard;
	result = xclip_buffer_text_owned(&buffer);
	if ( result == NULL ) return NULL;
	xclip_set_error("");
	return result;
fail:
	CloseClipboard();
failed_without_clipboard:
	xrtBufferUnit(&buffer);
	return xrtStrDup("[]");
}

XL_EXPORT void* x_clipboard_formats_json_owned(void)
{
	return xclip_formats_json_owned();
}

XL_EXPORT xvalue* x_clipboard_formats_json(void)
{
	return xclip_value_from_owned(xclip_formats_json_owned());
}

static char* xclip_format_name_owned(int64_t format_id)
{
	char* name;
	if ( format_id <= 0 || format_id > UINT_MAX ) {
		xclip_set_error("format id must be positive");
		return xrtStrDup("");
	}
	name = xclip_format_name_alloc((UINT)format_id);
	if ( name == NULL ) return NULL;
	xclip_set_error("");
	return name;
}

XL_EXPORT void* x_clipboard_format_name_owned(int64_t format_id)
{
	return xclip_format_name_owned(format_id);
}

XL_EXPORT xvalue* x_clipboard_format_name(int64_t format_id)
{
	return xclip_value_from_owned(xclip_format_name_owned(format_id));
}

XL_EXPORT int64_t x_clipboard_register_format(const char* name)
{
	wchar_t* wide = xclip_utf8_to_wide_alloc(name);
	UINT id;
	if ( wide == NULL ) {
		return 0;
	}
	id = RegisterClipboardFormatW(wide);
	xrtFree(wide);
	if ( id == 0u ) {
		xclip_set_win_error("RegisterClipboardFormatW");
		return 0;
	}
	xclip_set_error("");
	return (int)id;
}

XL_EXPORT int64_t x_clipboard_is_format_available(int64_t format_id)
{
	if ( format_id <= 0 || format_id > UINT_MAX ) {
		return 0;
	}
	return IsClipboardFormatAvailable((UINT)format_id) ? 1 : 0;
}

XL_EXPORT int64_t x_clipboard_has_text(void)
{
	return (IsClipboardFormatAvailable(CF_UNICODETEXT) ||
		IsClipboardFormatAvailable(CF_TEXT) ||
		IsClipboardFormatAvailable(CF_OEMTEXT)) ? 1 : 0;
}

XL_EXPORT int64_t x_clipboard_has_image(void)
{
	return (IsClipboardFormatAvailable(CF_DIBV5) ||
		IsClipboardFormatAvailable(CF_DIB) ||
		IsClipboardFormatAvailable(CF_BITMAP)) ? 1 : 0;
}

XL_EXPORT int64_t x_clipboard_has_files(void)
{
	return IsClipboardFormatAvailable(CF_HDROP) ? 1 : 0;
}

static char* xclip_get_text_owned(void)
{
	HGLOBAL hglobal;
	void* data;
	char* out;
	SIZE_T size;
	if ( !xclip_open() ) {
		return xrtStrDup("");
	}
	if ( IsClipboardFormatAvailable(CF_UNICODETEXT) ) {
		hglobal = (HGLOBAL)GetClipboardData(CF_UNICODETEXT);
		if ( hglobal == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GetClipboardData");
			return xrtStrDup("");
		}
		data = GlobalLock(hglobal);
		if ( data == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GlobalLock");
			return xrtStrDup("");
		}
		size = GlobalSize(hglobal);
		if ( size < sizeof(wchar_t) || size % sizeof(wchar_t) != 0u ||
			wmemchr((const wchar_t*)data, L'\0', size / sizeof(wchar_t)) == NULL ) {
			GlobalUnlock(hglobal);
			CloseClipboard();
			xclip_set_error("clipboard Unicode text is not terminated");
			return xrtStrDup("");
		}
		out = xclip_wide_to_utf8((const wchar_t*)data);
		GlobalUnlock(hglobal);
		CloseClipboard();
		if ( out == NULL ) {
			return xrtStrDup("");
		}
		xclip_set_error("");
		return out;
	}
	if ( IsClipboardFormatAvailable(CF_TEXT) || IsClipboardFormatAvailable(CF_OEMTEXT) ) {
		UINT fmt = IsClipboardFormatAvailable(CF_TEXT) ? CF_TEXT : CF_OEMTEXT;
		UINT code_page = fmt == CF_OEMTEXT ? CP_OEMCP : CP_ACP;
		hglobal = (HGLOBAL)GetClipboardData(fmt);
		if ( hglobal == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GetClipboardData");
			return xrtStrDup("");
		}
		data = GlobalLock(hglobal);
		if ( data == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GlobalLock");
			return xrtStrDup("");
		}
		size = GlobalSize(hglobal);
		if ( size == 0u || memchr(data, '\0', size) == NULL ) {
			GlobalUnlock(hglobal);
			CloseClipboard();
			xclip_set_error("clipboard text is not terminated");
			return xrtStrDup("");
		}
		out = xclip_multibyte_to_utf8((const char*)data, code_page);
		GlobalUnlock(hglobal);
		CloseClipboard();
		if ( out == NULL ) {
			return xrtStrDup("");
		}
		xclip_set_error("");
		return out;
	}
	CloseClipboard();
	xclip_set_error("clipboard text is not available");
	return xrtStrDup("");
}

/* XLang 直接接管 UTF-8 文本，避免经 xvalue 再复制一次。 */
XL_EXPORT void* x_clipboard_get_text_owned(void)
{
	return xclip_get_text_owned();
}

XL_EXPORT xvalue* x_clipboard_get_text(void)
{
	return xclip_value_from_owned(xclip_get_text_owned());
}

XL_EXPORT int64_t x_clipboard_set_text(const char* text)
{
	wchar_t* wide = xclip_utf8_to_wide_alloc(text);
	size_t bytes;
	HGLOBAL hglobal;
	void* memory;
	if ( wide == NULL ) {
		return 0;
	}
	bytes = ((size_t)lstrlenW(wide) + 1u) * sizeof(wchar_t);
	hglobal = GlobalAlloc(GMEM_MOVEABLE, bytes);
	if ( hglobal == NULL ) {
		xrtFree(wide);
		xclip_set_win_error("GlobalAlloc");
		return 0;
	}
	memory = GlobalLock(hglobal);
	if ( memory == NULL ) {
		xrtFree(wide);
		GlobalFree(hglobal);
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	memcpy(memory, wide, bytes);
	GlobalUnlock(hglobal);
	xrtFree(wide);
	if ( !xclip_open() ) {
		GlobalFree(hglobal);
		return 0;
	}
	if ( !EmptyClipboard() || SetClipboardData(CF_UNICODETEXT, hglobal) == NULL ) {
		GlobalFree(hglobal);
		CloseClipboard();
		xclip_set_win_error("SetClipboardData");
		return 0;
	}
	CloseClipboard();
	xclip_set_error("");
	return 1;
}

XL_EXPORT xvalue* x_clipboard_get_files(void)
{
	xvalue* out = xrtValueArray();
	HDROP drop;
	UINT count;
	UINT i;
	if ( out == NULL ) {
		return NULL;
	}
	if ( !xclip_open() ) {
		xrtValueRelease(out);
		return NULL;
	}
	if ( !IsClipboardFormatAvailable(CF_HDROP) ) {
		CloseClipboard();
		xclip_set_error("clipboard files are not available");
		return out;
	}
	drop = (HDROP)GetClipboardData(CF_HDROP);
	if ( drop == NULL ) {
		CloseClipboard();
		xclip_set_win_error("GetClipboardData");
		xrtValueRelease(out);
		return NULL;
	}
	count = DragQueryFileW(drop, 0xFFFFFFFFu, NULL, 0u);
	for ( i = 0u; i < count; ++i ) {
		UINT chars = DragQueryFileW(drop, i, NULL, 0u);
		wchar_t* wide = (wchar_t*)xrtCalloc((size_t)chars + 1u, sizeof(wchar_t));
		if ( wide == NULL ) {
			xclip_set_error("out of memory");
			CloseClipboard();
			xrtValueRelease(out);
			return NULL;
		}
		if ( DragQueryFileW(drop, i, wide, chars + 1u) != chars ) {
			xrtFree(wide);
			xclip_set_win_error("DragQueryFileW");
			CloseClipboard();
			xrtValueRelease(out);
			return NULL;
		}
		char* utf8 = xclip_wide_to_utf8(wide);
		int appended = utf8 != NULL && xclip_array_append_text(out, &utf8);
		xrtFree(wide);
		xrtFree(utf8);
		if ( !appended ) {
			CloseClipboard();
			xrtValueRelease(out);
			return NULL;
		}
	}
	CloseClipboard();
	xclip_set_error("");
	return out;
}

static char* xclip_get_file_owned(void)
{
	HDROP drop;
	UINT chars;
	wchar_t* wide;
	char* out;
	if ( !xclip_open() ) {
		return xrtStrDup("");
	}
	if ( !IsClipboardFormatAvailable(CF_HDROP) ) {
		CloseClipboard();
		xclip_set_error("clipboard files are not available");
		return xrtStrDup("");
	}
	drop = (HDROP)GetClipboardData(CF_HDROP);
	if ( drop == NULL || DragQueryFileW(drop, 0xFFFFFFFFu, NULL, 0u) == 0u ) {
		CloseClipboard();
		xclip_set_error("clipboard file list is empty");
		return xrtStrDup("");
	}
	chars = DragQueryFileW(drop, 0u, NULL, 0u);
	wide = (wchar_t*)xrtCalloc((size_t)chars + 1u, sizeof(wchar_t));
	if ( wide == NULL ) {
		CloseClipboard();
		xclip_set_error("out of memory");
		return xrtStrDup("");
	}
	DragQueryFileW(drop, 0u, wide, chars + 1u);
	out = xclip_wide_to_utf8(wide);
	xrtFree(wide);
	CloseClipboard();
	if ( out == NULL ) {
		return xrtStrDup("");
	}
	xclip_set_error("");
	return out;
}

XL_EXPORT void* x_clipboard_get_file_owned(void)
{
	return xclip_get_file_owned();
}

XL_EXPORT xvalue* x_clipboard_get_file(void)
{
	return xclip_value_from_owned(xclip_get_file_owned());
}

static int xclip_set_files_wide(wchar_t** paths, size_t count)
{
	SIZE_T total_chars = 1u;
	const SIZE_T limit_chars = (XCLIP_MAX_SNAPSHOT_BYTES - sizeof(XClipDropFiles)) /
		sizeof(wchar_t);
	SIZE_T bytes;
	HGLOBAL hglobal;
	void* memory;
	XClipDropFiles* drop;
	wchar_t* dst;
	size_t i;
	if ( paths == NULL || count == 0u ) {
		xclip_set_error("file list is empty");
		return 0;
	}
	for ( i = 0u; i < count; ++i ) {
		SIZE_T length;
		if ( paths[i] == NULL || paths[i][0] == L'\0' ) {
			xclip_set_error("invalid file path");
			return 0;
		}
		length = (SIZE_T)lstrlenW(paths[i]);
		if ( total_chars >= limit_chars || length >= limit_chars - total_chars ) {
			xclip_set_error("file list size limit exceeded");
			return 0;
		}
		total_chars += length + 1u;
	}
	bytes = sizeof(XClipDropFiles) + total_chars * sizeof(wchar_t);
	hglobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
	if ( hglobal == NULL ) {
		xclip_set_win_error("GlobalAlloc");
		return 0;
	}
	memory = GlobalLock(hglobal);
	if ( memory == NULL ) {
		GlobalFree(hglobal);
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	drop = (XClipDropFiles*)memory;
	drop->pFiles = sizeof(XClipDropFiles);
	drop->fWide = TRUE;
	dst = (wchar_t*)((unsigned char*)memory + sizeof(XClipDropFiles));
	for ( i = 0u; i < count; ++i ) {
		int len = lstrlenW(paths[i]);
		memcpy(dst, paths[i], ((size_t)len + 1u) * sizeof(wchar_t));
		dst += len + 1;
	}
	*dst = L'\0';
	GlobalUnlock(hglobal);
	if ( !xclip_open() ) {
		GlobalFree(hglobal);
		return 0;
	}
	if ( !EmptyClipboard() || SetClipboardData(CF_HDROP, hglobal) == NULL ) {
		GlobalFree(hglobal);
		CloseClipboard();
		xclip_set_win_error("SetClipboardData");
		return 0;
	}
	CloseClipboard();
	xclip_set_error("");
	return 1;
}

XL_EXPORT int64_t x_clipboard_set_files(xvalue* paths_value)
{
	size_t count;
	wchar_t** paths;
	size_t i;
	int ok;
	if ( xrtValueType(paths_value) != XVALUE_ARRAY ) {
		xclip_set_error("invalid file list");
		return 0;
	}
	count = xrtValueCount(paths_value);
	if ( count == 0u ) {
		xclip_set_error("invalid file list");
		return 0;
	}
	if ( count > SIZE_MAX / sizeof(*paths) ) {
		xclip_set_error("file list size limit exceeded");
		return 0;
	}
	paths = (wchar_t**)xrtCalloc((size_t)count, sizeof(wchar_t*));
	if ( paths == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	for ( i = 0u; i < count; ++i ) {
		paths[i] = xclip_xvalue_text_to_wide_alloc(
			xrtValueArrayGet(paths_value, i));
		if ( paths[i] == NULL ) {
			size_t j;
			for ( j = 0u; j < count; ++j ) {
				xrtFree(paths[j]);
			}
			xrtFree(paths);
			return 0;
		}
	}
	ok = xclip_set_files_wide(paths, count);
	for ( i = 0u; i < count; ++i ) {
		xrtFree(paths[i]);
	}
	xrtFree(paths);
	return ok;
}

XL_EXPORT int64_t x_clipboard_set_file(const char* path)
{
	wchar_t* wide = xclip_utf8_to_wide_alloc(path);
	wchar_t* paths[1];
	int ok;
	if ( wide == NULL ) {
		return 0;
	}
	paths[0] = wide;
	ok = xclip_set_files_wide(paths, 1u);
	xrtFree(wide);
	return ok;
}

XL_EXPORT int64_t x_clipboard_save_image(const char* path)
{
	HGLOBAL hglobal;
	void* dib;
	SIZE_T size;
	UINT fmt;
	int owned = 0;
	int ok;
	DWORD bmp_size = 0u;
	unsigned char* bmp;
	if ( path == NULL || path[0] == '\0' ) {
		xclip_set_error("invalid output path");
		return 0;
	}
	if ( !xclip_open() ) {
		return 0;
	}
	hglobal = xclip_get_clipboard_dib(&fmt, &owned);
	(void)fmt;
	if ( hglobal == NULL ) {
		CloseClipboard();
		return 0;
	}
	size = GlobalSize(hglobal);
	dib = GlobalLock(hglobal);
	if ( dib == NULL || size == 0u ) {
		if ( dib != NULL ) {
			GlobalUnlock(hglobal);
		}
		if ( owned ) {
			GlobalFree(hglobal);
		}
		CloseClipboard();
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	bmp = xclip_bmp_from_dib(dib, size, &bmp_size);
	GlobalUnlock(hglobal);
	if ( owned ) {
		GlobalFree(hglobal);
	}
	CloseClipboard();
	if ( bmp == NULL ) {
		return 0;
	}
	ok = xrtFileWriteAtomic(path, (xbytesview){ bmp, bmp_size });
	xrtFree(bmp);
	if ( !ok ) {
		xclip_set_xrt_error();
	}
	if ( ok ) {
		xclip_set_error("");
	}
	return ok;
}

XL_EXPORT int64_t x_clipboard_set_image(const char* path)
{
	unsigned char* file_data;
	size_t read_size = 0u;
	DWORD file_size;
	DWORD dib_size;
	HGLOBAL hglobal;
	void* memory;
	int ok;
	if ( path == NULL || path[0] == '\0' ) {
		xclip_set_error("invalid input path");
		return 0;
	}
	file_data = xrtFileReadAllLimit(path, XCLIP_MAX_IMAGE_BYTES, &read_size);
	if ( file_data == NULL ) {
		xclip_set_xrt_error();
		return 0;
	}
	file_size = (DWORD)read_size;
	if ( !xclip_validate_bmp(file_data, file_size) ) {
		xrtFree(file_data);
		xclip_set_error("invalid or unsupported BMP file");
		return 0;
	}
	dib_size = file_size - 14u;
	hglobal = GlobalAlloc(GMEM_MOVEABLE, dib_size);
	if ( hglobal == NULL ) {
		xrtFree(file_data);
		xclip_set_win_error("GlobalAlloc");
		return 0;
	}
	memory = GlobalLock(hglobal);
	if ( memory == NULL ) {
		GlobalFree(hglobal);
		xrtFree(file_data);
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	memcpy(memory, file_data + 14u, dib_size);
	GlobalUnlock(hglobal);
	xrtFree(file_data);
	if ( !xclip_open() ) {
		GlobalFree(hglobal);
		return 0;
	}
	ok = EmptyClipboard() && SetClipboardData(CF_DIB, hglobal) != NULL;
	if ( !ok ) {
		GlobalFree(hglobal);
		xclip_set_win_error("SetClipboardData");
	} else {
		xclip_set_error("");
	}
	CloseClipboard();
	return ok ? 1 : 0;
}

static char* xclip_image_info_json_owned(void)
{
	HGLOBAL hglobal;
	UINT fmt = 0u;
	int owned = 0;
	void* dib;
	SIZE_T size;
	const BITMAPINFOHEADER* bi;
	const char* fmt_name;
	xbuffer buffer;
	char* result;
	if ( !xclip_open() ) {
		return xrtStrDup("{\"available\":false}");
	}
	hglobal = xclip_get_clipboard_dib(&fmt, &owned);
	if ( hglobal == NULL ) {
		CloseClipboard();
		return xrtStrDup("{\"available\":false}");
	}
	size = GlobalSize(hglobal);
	dib = GlobalLock(hglobal);
	if ( dib == NULL || size < sizeof(BITMAPINFOHEADER) ) {
		if ( dib != NULL ) {
			GlobalUnlock(hglobal);
		}
		if ( owned ) {
			GlobalFree(hglobal);
		}
		CloseClipboard();
		xclip_set_win_error("GlobalLock");
		return xrtStrDup("{\"available\":false}");
	}
	bi = (const BITMAPINFOHEADER*)dib;
	if ( xclip_dib_pixel_offset(dib, size) == 0u || bi->biWidth <= 0 ||
		bi->biHeight == 0 || bi->biHeight == INT_MIN || bi->biPlanes != 1 ) {
		GlobalUnlock(hglobal);
		if ( owned ) {
			GlobalFree(hglobal);
		}
		CloseClipboard();
		xclip_set_error("invalid clipboard DIB");
		return xrtStrDup("{\"available\":false}");
	}
	fmt_name = xclip_standard_format_name(fmt);
	if ( !xrtBufferInit(&buffer) ) {
		GlobalUnlock(hglobal);
		if ( owned ) GlobalFree(hglobal);
		CloseClipboard();
		return NULL;
	}
	if ( !xclip_text_append(&buffer, "{\"available\":true,\"format\":\"") ||
		!xclip_json_escape_append(&buffer, fmt_name) ||
		!xclip_text_appendf(&buffer, "\",\"width\":%ld,\"height\":%ld,\"bit_count\":%u,\"compression\":%lu,\"bytes\":%lu}",
		(long)bi->biWidth,
		(long)(bi->biHeight < 0 ? -bi->biHeight : bi->biHeight),
		(unsigned int)bi->biBitCount,
		(unsigned long)bi->biCompression,
		(unsigned long)size) ) {
		GlobalUnlock(hglobal);
		if ( owned ) GlobalFree(hglobal);
		CloseClipboard();
		xrtBufferUnit(&buffer);
		return xrtStrDup("{\"available\":false}");
	}
	GlobalUnlock(hglobal);
	if ( owned ) {
		GlobalFree(hglobal);
	}
	CloseClipboard();
	result = xclip_buffer_text_owned(&buffer);
	if ( result == NULL ) return NULL;
	xclip_set_error("");
	return result;
}

XL_EXPORT void* x_clipboard_image_info_json_owned(void)
{
	return xclip_image_info_json_owned();
}

XL_EXPORT xvalue* x_clipboard_image_info_json(void)
{
	return xclip_value_from_owned(xclip_image_info_json_owned());
}
