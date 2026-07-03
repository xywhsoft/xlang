#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef unsigned int XL_ArtifactUInt32;
typedef long long XL_ArtifactInt64;
typedef const char* XL_ArtifactStr;
typedef void* XL_ArtifactPtr;

typedef struct XL_ArtifactXValueStruct {
	union {
		struct {
			XL_ArtifactUInt32 Type:4;
			XL_ArtifactUInt32 Reserve:1;
			XL_ArtifactUInt32 IsStatic:1;
			XL_ArtifactUInt32 RefCount:26;
		};
		volatile XL_ArtifactUInt32 Header;
	};
	XL_ArtifactUInt32 Size;
	union {
		int vBool;
		XL_ArtifactInt64 vInt;
		double vFloat;
		XL_ArtifactStr vText;
		XL_ArtifactPtr vPoint;
	};
} *XL_ArtifactXValue;

#define XL_ARTIFACT_XVO_DT_NULL 0
#define XL_ARTIFACT_XVO_DT_BOOL 1
#define XL_ARTIFACT_XVO_DT_INT 2
#define XL_ARTIFACT_XVO_DT_FLOAT 3
#define XL_ARTIFACT_XVO_DT_TEXT 4
#define XL_ARTIFACT_XVO_DT_POINT 6

extern XL_ArtifactXValue xvoCreateArray(void);
extern XL_ArtifactXValue xvoCreateText(void* sVal, XL_ArtifactUInt32 iSize, int bColloc);
extern int xvoArrayAppendValue(XL_ArtifactXValue pArr, XL_ArtifactXValue pVal, int bUnrefVal);
extern XL_ArtifactUInt32 xvoArrayItemCount(XL_ArtifactXValue pArr);
extern XL_ArtifactXValue xvoArrayGetValue(XL_ArtifactXValue pArr, XL_ArtifactUInt32 iIndex);

typedef struct XClipDropFiles {
	DWORD pFiles;
	POINT pt;
	BOOL fNC;
	BOOL fWide;
} XClipDropFiles;

static char g_xclip_error[512];
static char* g_xclip_text = NULL;
static size_t g_xclip_text_cap = 0u;

static void xclip_set_error(const char* text);

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

static XClipSnapshotItem* g_xclip_snapshot_items = NULL;
static size_t g_xclip_snapshot_count = 0u;
static int g_xclip_snapshot_valid = 0;

static void xclip_snapshot_clear(void)
{
	size_t i;
	for ( i = 0u; i < g_xclip_snapshot_count; ++i ) {
		if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_BITMAP && g_xclip_snapshot_items[i].handle != NULL ) {
			DeleteObject(g_xclip_snapshot_items[i].handle);
		} else if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_ENHMETAFILE && g_xclip_snapshot_items[i].handle != NULL ) {
			DeleteEnhMetaFile((HENHMETAFILE)g_xclip_snapshot_items[i].handle);
		}
		free(g_xclip_snapshot_items[i].data);
	}
	free(g_xclip_snapshot_items);
	g_xclip_snapshot_items = NULL;
	g_xclip_snapshot_count = 0u;
	g_xclip_snapshot_valid = 0;
}

static int xclip_snapshot_append(UINT format, const void* data, SIZE_T size)
{
	XClipSnapshotItem* next;
	unsigned char* copy;
	if ( data == NULL || size == 0u ) {
		return 1;
	}
	copy = (unsigned char*)malloc((size_t)size);
	if ( copy == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	memcpy(copy, data, (size_t)size);
	next = (XClipSnapshotItem*)realloc(g_xclip_snapshot_items, (g_xclip_snapshot_count + 1u) * sizeof(XClipSnapshotItem));
	if ( next == NULL ) {
		free(copy);
		xclip_set_error("out of memory");
		return 0;
	}
	g_xclip_snapshot_items = next;
	g_xclip_snapshot_items[g_xclip_snapshot_count].format = format;
	g_xclip_snapshot_items[g_xclip_snapshot_count].kind = XCLIP_SNAPSHOT_GLOBAL;
	g_xclip_snapshot_items[g_xclip_snapshot_count].size = size;
	g_xclip_snapshot_items[g_xclip_snapshot_count].data = copy;
	g_xclip_snapshot_items[g_xclip_snapshot_count].handle = NULL;
	g_xclip_snapshot_count++;
	return 1;
}

static int xclip_snapshot_append_handle(UINT format, HANDLE handle, int kind)
{
	XClipSnapshotItem* next;
	if ( handle == NULL ) {
		return 1;
	}
	next = (XClipSnapshotItem*)realloc(g_xclip_snapshot_items, (g_xclip_snapshot_count + 1u) * sizeof(XClipSnapshotItem));
	if ( next == NULL ) {
		if ( kind == XCLIP_SNAPSHOT_BITMAP ) {
			DeleteObject(handle);
		} else if ( kind == XCLIP_SNAPSHOT_ENHMETAFILE ) {
			DeleteEnhMetaFile((HENHMETAFILE)handle);
		}
		xclip_set_error("out of memory");
		return 0;
	}
	g_xclip_snapshot_items = next;
	g_xclip_snapshot_items[g_xclip_snapshot_count].format = format;
	g_xclip_snapshot_items[g_xclip_snapshot_count].kind = kind;
	g_xclip_snapshot_items[g_xclip_snapshot_count].size = 0u;
	g_xclip_snapshot_items[g_xclip_snapshot_count].data = NULL;
	g_xclip_snapshot_items[g_xclip_snapshot_count].handle = handle;
	g_xclip_snapshot_count++;
	return 1;
}

static void xclip_copy_cstr(char* out, size_t out_size, const char* text)
{
	size_t len;
	if ( out == NULL || out_size == 0u ) {
		return;
	}
	if ( text == NULL ) {
		text = "";
	}
	len = strlen(text);
	if ( len >= out_size ) {
		len = out_size - 1u;
	}
	if ( len != 0u ) {
		memcpy(out, text, len);
	}
	out[len] = '\0';
}

static void xclip_set_error(const char* text)
{
	xclip_copy_cstr(g_xclip_error, sizeof(g_xclip_error), text);
}

static void xclip_set_win_error(const char* op)
{
	DWORD code = GetLastError();
	snprintf(g_xclip_error, sizeof(g_xclip_error), "%s failed: %lu", op != NULL ? op : "Win32 API", (unsigned long)code);
}

static int xclip_ensure_text(size_t cap)
{
	char* next;
	if ( cap == 0u ) {
		cap = 1u;
	}
	if ( g_xclip_text_cap >= cap && g_xclip_text != NULL ) {
		return 1;
	}
	next = (char*)realloc(g_xclip_text, cap);
	if ( next == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	g_xclip_text = next;
	g_xclip_text_cap = cap;
	return 1;
}

static const char* xclip_set_text_buffer(const char* text)
{
	size_t len = text != NULL ? strlen(text) : 0u;
	if ( !xclip_ensure_text(len + 1u) ) {
		return "";
	}
	if ( len != 0u ) {
		memcpy(g_xclip_text, text, len);
	}
	g_xclip_text[len] = '\0';
	return g_xclip_text;
}

static void xclip_text_reset(void)
{
	if ( xclip_ensure_text(1u) ) {
		g_xclip_text[0] = '\0';
	}
}

static int xclip_text_append_len(const char* text, size_t len)
{
	size_t old_len;
	if ( text == NULL || len == 0u ) {
		return 1;
	}
	old_len = g_xclip_text != NULL ? strlen(g_xclip_text) : 0u;
	if ( !xclip_ensure_text(old_len + len + 1u) ) {
		return 0;
	}
	memcpy(g_xclip_text + old_len, text, len);
	g_xclip_text[old_len + len] = '\0';
	return 1;
}

static int xclip_text_append(const char* text)
{
	return xclip_text_append_len(text, text != NULL ? strlen(text) : 0u);
}

static int xclip_text_appendf(const char* fmt, ...)
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
		return xclip_text_append_len(stack, (size_t)n);
	}
	heap = (char*)malloc((size_t)n + 1u);
	if ( heap == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	va_start(args, fmt);
	vsnprintf(heap, (size_t)n + 1u, fmt, args);
	va_end(args);
	ok = xclip_text_append_len(heap, (size_t)n);
	free(heap);
	return ok;
}

static void xclip_json_escape_append(const char* text)
{
	const unsigned char* p = (const unsigned char*)text;
	if ( p == NULL ) {
		return;
	}
	while ( *p != '\0' ) {
		char tmp[8];
		if ( *p == '"' || *p == '\\' ) {
			tmp[0] = '\\';
			tmp[1] = (char)*p;
			tmp[2] = '\0';
			xclip_text_append(tmp);
		} else if ( *p == '\n' ) {
			xclip_text_append("\\n");
		} else if ( *p == '\r' ) {
			xclip_text_append("\\r");
		} else if ( *p == '\t' ) {
			xclip_text_append("\\t");
		} else if ( *p < 0x20u ) {
			snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned int)*p);
			xclip_text_append(tmp);
		} else {
			xclip_text_append_len((const char*)p, 1u);
		}
		++p;
	}
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
	UINT cp = CP_UTF8;
	if ( src == NULL ) {
		src = "";
	}
	n = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
	if ( n <= 0 ) {
		cp = CP_ACP;
		n = MultiByteToWideChar(cp, 0, src, -1, NULL, 0);
	}
	if ( n <= 0 ) {
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out = (wchar_t*)calloc((size_t)n, sizeof(wchar_t));
	if ( out == NULL ) {
		xclip_set_error("out of memory");
		return NULL;
	}
	if ( MultiByteToWideChar(cp, 0, src, -1, out, n) <= 0 ) {
		free(out);
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	return out;
}

static wchar_t* xclip_xvalue_text_to_wide_alloc(XL_ArtifactXValue value)
{
	wchar_t* out;
	int n;
	UINT cp = CP_UTF8;
	int size;
	if ( value == NULL || value->Type != XL_ARTIFACT_XVO_DT_TEXT || value->vText == NULL ) {
		xclip_set_error("file path item is not a string");
		return NULL;
	}
	size = (int)value->Size;
	n = MultiByteToWideChar(CP_UTF8, 0, value->vText, size, NULL, 0);
	if ( n <= 0 ) {
		cp = CP_ACP;
		n = MultiByteToWideChar(cp, 0, value->vText, size, NULL, 0);
	}
	if ( n < 0 ) {
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out = (wchar_t*)calloc((size_t)n + 1u, sizeof(wchar_t));
	if ( out == NULL ) {
		xclip_set_error("out of memory");
		return NULL;
	}
	if ( n > 0 && MultiByteToWideChar(cp, 0, value->vText, size, out, n) <= 0 ) {
		free(out);
		xclip_set_win_error("MultiByteToWideChar");
		return NULL;
	}
	out[n] = L'\0';
	return out;
}

static const char* xclip_wide_to_utf8(const wchar_t* src)
{
	int n;
	if ( src == NULL ) {
		return xclip_set_text_buffer("");
	}
	n = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
	if ( n <= 0 ) {
		xclip_set_win_error("WideCharToMultiByte");
		return xclip_set_text_buffer("");
	}
	if ( !xclip_ensure_text((size_t)n) ) {
		return "";
	}
	if ( WideCharToMultiByte(CP_UTF8, 0, src, -1, g_xclip_text, n, NULL, NULL) <= 0 ) {
		xclip_set_win_error("WideCharToMultiByte");
		return xclip_set_text_buffer("");
	}
	return g_xclip_text;
}

static int xclip_wide_to_utf8_buf(const wchar_t* src, char* dst, size_t dst_size)
{
	int n;
	if ( dst == NULL || dst_size == 0u ) {
		return 0;
	}
	dst[0] = '\0';
	if ( src == NULL ) {
		return 1;
	}
	n = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, (int)dst_size, NULL, NULL);
	if ( n <= 0 ) {
		n = WideCharToMultiByte(CP_ACP, 0, src, -1, dst, (int)dst_size, NULL, NULL);
	}
	dst[dst_size - 1u] = '\0';
	return n > 0;
}

static const char* xclip_multibyte_to_utf8(const char* src, UINT code_page)
{
	wchar_t* wide;
	int n;
	const char* out;
	if ( src == NULL ) {
		return xclip_set_text_buffer("");
	}
	n = MultiByteToWideChar(code_page, 0, src, -1, NULL, 0);
	if ( n <= 0 ) {
		xclip_set_win_error("MultiByteToWideChar");
		return xclip_set_text_buffer("");
	}
	wide = (wchar_t*)calloc((size_t)n, sizeof(wchar_t));
	if ( wide == NULL ) {
		xclip_set_error("out of memory");
		return xclip_set_text_buffer("");
	}
	if ( MultiByteToWideChar(code_page, 0, src, -1, wide, n) <= 0 ) {
		free(wide);
		xclip_set_win_error("MultiByteToWideChar");
		return xclip_set_text_buffer("");
	}
	out = xclip_wide_to_utf8(wide);
	free(wide);
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

static void xclip_format_name_buf(UINT fmt, char* out, size_t out_size)
{
	const char* standard = xclip_standard_format_name(fmt);
	wchar_t wide[256];
	if ( out == NULL || out_size == 0u ) {
		return;
	}
	out[0] = '\0';
	if ( standard != NULL ) {
		xclip_copy_cstr(out, out_size, standard);
		return;
	}
	if ( GetClipboardFormatNameW(fmt, wide, (int)(sizeof(wide) / sizeof(wide[0]))) > 0 ) {
		xclip_wide_to_utf8_buf(wide, out, out_size);
		return;
	}
	snprintf(out, out_size, "FORMAT_%u", (unsigned int)fmt);
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
	DWORD offset;
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
	offset = header_size + masks_size + color_count * sizeof(RGBQUAD);
	if ( offset > dib_size ) {
		return 0u;
	}
	return offset;
}

static int xclip_write_file_wide(const wchar_t* path, const void* data, DWORD size)
{
	HANDLE file;
	DWORD written = 0u;
	BOOL ok;
	if ( path == NULL || path[0] == L'\0' || (data == NULL && size != 0u) ) {
		xclip_set_error("invalid output path or data");
		return 0;
	}
	file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if ( file == INVALID_HANDLE_VALUE ) {
		xclip_set_win_error("CreateFileW");
		return 0;
	}
	ok = WriteFile(file, data, size, &written, NULL);
	CloseHandle(file);
	if ( !ok || written != size ) {
		xclip_set_win_error("WriteFile");
		return 0;
	}
	return 1;
}

static unsigned char* xclip_read_file_wide(const wchar_t* path, DWORD* out_size)
{
	HANDLE file;
	DWORD size_high = 0u;
	DWORD size_low;
	DWORD read_count = 0u;
	unsigned char* data;
	if ( out_size != NULL ) {
		*out_size = 0u;
	}
	if ( path == NULL || path[0] == L'\0' || out_size == NULL ) {
		xclip_set_error("invalid input path");
		return NULL;
	}
	file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if ( file == INVALID_HANDLE_VALUE ) {
		xclip_set_win_error("CreateFileW");
		return NULL;
	}
	size_low = GetFileSize(file, &size_high);
	if ( size_low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR ) {
		CloseHandle(file);
		xclip_set_win_error("GetFileSize");
		return NULL;
	}
	if ( size_high != 0u || size_low == 0u ) {
		CloseHandle(file);
		xclip_set_error("file is empty or too large");
		return NULL;
	}
	data = (unsigned char*)malloc((size_t)size_low);
	if ( data == NULL ) {
		CloseHandle(file);
		xclip_set_error("out of memory");
		return NULL;
	}
	if ( !ReadFile(file, data, size_low, &read_count, NULL) || read_count != size_low ) {
		free(data);
		CloseHandle(file);
		xclip_set_win_error("ReadFile");
		return NULL;
	}
	CloseHandle(file);
	*out_size = size_low;
	return data;
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
	if ( bitmap == NULL ) {
		xclip_set_error("bitmap handle is null");
		return NULL;
	}
	if ( GetObjectW(bitmap, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight == 0 ) {
		xclip_set_win_error("GetObjectW");
		return NULL;
	}
	height_abs = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
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

static int xclip_save_dib_as_bmp(const wchar_t* path, const void* dib, SIZE_T dib_size)
{
	DWORD pixel_offset;
	DWORD file_size;
	DWORD bmp_offset;
	unsigned char* file_data;
	int ok;
	if ( dib == NULL || dib_size < sizeof(BITMAPINFOHEADER) || dib_size > 0x7ffff000u ) {
		xclip_set_error("invalid DIB data");
		return 0;
	}
	pixel_offset = xclip_dib_pixel_offset(dib, dib_size);
	if ( pixel_offset == 0u ) {
		xclip_set_error("unsupported DIB header");
		return 0;
	}
	file_size = 14u + (DWORD)dib_size;
	bmp_offset = 14u + pixel_offset;
	file_data = (unsigned char*)malloc((size_t)file_size);
	if ( file_data == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	memset(file_data, 0, 14u);
	file_data[0] = 'B';
	file_data[1] = 'M';
	xclip_write_u32_le(file_data + 2, file_size);
	xclip_write_u32_le(file_data + 10, bmp_offset);
	memcpy(file_data + 14u, dib, (size_t)dib_size);
	ok = xclip_write_file_wide(path, file_data, file_size);
	free(file_data);
	return ok;
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

XL_EXPORT int x_clipboard_available(void)
{
	return 1;
}

XL_EXPORT const char* x_clipboard_version(void)
{
	return "clipboard 0.1.0";
}

XL_EXPORT const char* x_clipboard_last_error(void)
{
	return g_xclip_error;
}

XL_EXPORT int x_clipboard_clear(void)
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

XL_EXPORT int x_clipboard_snapshot(void)
{
	UINT fmt = 0u;
	int ok = 1;
	xclip_snapshot_clear();
	if ( !xclip_open() ) {
		return 0;
	}
	while ( (fmt = EnumClipboardFormats(fmt)) != 0u ) {
		HANDLE hdata = GetClipboardData(fmt);
		SIZE_T size;
		void* data;
		if ( hdata == NULL ) {
			continue;
		}
		if ( fmt == CF_BITMAP ) {
			HANDLE copy = CopyImage(hdata, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			if ( copy == NULL ) {
				xclip_set_win_error("CopyImage");
				ok = 0;
				break;
			}
			if ( !xclip_snapshot_append_handle(fmt, copy, XCLIP_SNAPSHOT_BITMAP) ) {
				ok = 0;
				break;
			}
			continue;
		}
		if ( fmt == CF_ENHMETAFILE ) {
			HENHMETAFILE copy = CopyEnhMetaFileW((HENHMETAFILE)hdata, NULL);
			if ( copy == NULL ) {
				xclip_set_win_error("CopyEnhMetaFileW");
				ok = 0;
				break;
			}
			if ( !xclip_snapshot_append_handle(fmt, (HANDLE)copy, XCLIP_SNAPSHOT_ENHMETAFILE) ) {
				ok = 0;
				break;
			}
			continue;
		}
		size = GlobalSize((HGLOBAL)hdata);
		if ( size == 0u ) {
			continue;
		}
		data = GlobalLock((HGLOBAL)hdata);
		if ( data == NULL ) {
			continue;
		}
		if ( !xclip_snapshot_append(fmt, data, size) ) {
			GlobalUnlock((HGLOBAL)hdata);
			ok = 0;
			break;
		}
		GlobalUnlock((HGLOBAL)hdata);
	}
	CloseClipboard();
	if ( !ok ) {
		xclip_snapshot_clear();
		return 0;
	}
	g_xclip_snapshot_valid = 1;
	xclip_set_error("");
	return 1;
}

XL_EXPORT int x_clipboard_restore(void)
{
	size_t i;
	int ok = 1;
	if ( !g_xclip_snapshot_valid ) {
		xclip_set_error("clipboard snapshot is not available");
		return 0;
	}
	if ( !xclip_open() ) {
		return 0;
	}
	if ( !EmptyClipboard() ) {
		CloseClipboard();
		xclip_set_win_error("EmptyClipboard");
		return 0;
	}
	for ( i = 0u; i < g_xclip_snapshot_count; ++i ) {
		HANDLE hdata = NULL;
		if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_GLOBAL ) {
			void* memory;
			hdata = GlobalAlloc(GMEM_MOVEABLE, g_xclip_snapshot_items[i].size);
			if ( hdata == NULL ) {
				xclip_set_win_error("GlobalAlloc");
				ok = 0;
				break;
			}
			memory = GlobalLock((HGLOBAL)hdata);
			if ( memory == NULL ) {
				GlobalFree((HGLOBAL)hdata);
				xclip_set_win_error("GlobalLock");
				ok = 0;
				break;
			}
			memcpy(memory, g_xclip_snapshot_items[i].data, (size_t)g_xclip_snapshot_items[i].size);
			GlobalUnlock((HGLOBAL)hdata);
		} else if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_BITMAP ) {
			hdata = CopyImage(g_xclip_snapshot_items[i].handle, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			if ( hdata == NULL ) {
				xclip_set_win_error("CopyImage");
				ok = 0;
				break;
			}
		} else if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_ENHMETAFILE ) {
			hdata = (HANDLE)CopyEnhMetaFileW((HENHMETAFILE)g_xclip_snapshot_items[i].handle, NULL);
			if ( hdata == NULL ) {
				xclip_set_win_error("CopyEnhMetaFileW");
				ok = 0;
				break;
			}
		}
		if ( hdata != NULL && SetClipboardData(g_xclip_snapshot_items[i].format, hdata) == NULL ) {
			if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_GLOBAL ) {
				GlobalFree((HGLOBAL)hdata);
			} else if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_BITMAP ) {
				DeleteObject(hdata);
			} else if ( g_xclip_snapshot_items[i].kind == XCLIP_SNAPSHOT_ENHMETAFILE ) {
				DeleteEnhMetaFile((HENHMETAFILE)hdata);
			}
			xclip_set_win_error("SetClipboardData");
			ok = 0;
			break;
		}
	}
	CloseClipboard();
	if ( ok ) {
		xclip_set_error("");
	}
	return ok;
}

XL_EXPORT int x_clipboard_sequence_number(void)
{
	return (int)GetClipboardSequenceNumber();
}

XL_EXPORT int x_clipboard_count_formats(void)
{
	return CountClipboardFormats();
}

XL_EXPORT XL_ArtifactXValue x_clipboard_formats(void)
{
	XL_ArtifactXValue out = xvoCreateArray();
	UINT fmt = 0u;
	if ( out == NULL ) {
		return NULL;
	}
	if ( !xclip_open() ) {
		return out;
	}
	SetLastError(NO_ERROR);
	while ( (fmt = EnumClipboardFormats(fmt)) != 0u ) {
		char name[256];
		xclip_format_name_buf(fmt, name, sizeof(name));
		xvoArrayAppendValue(out, xvoCreateText(name, (XL_ArtifactUInt32)strlen(name), 0), 1);
	}
	if ( GetLastError() != NO_ERROR ) {
		xclip_set_win_error("EnumClipboardFormats");
	} else {
		xclip_set_error("");
	}
	CloseClipboard();
	return out;
}

XL_EXPORT const char* x_clipboard_formats_json(void)
{
	UINT fmt = 0u;
	int first = 1;
	xclip_text_reset();
	xclip_text_append("[");
	if ( !xclip_open() ) {
		xclip_text_append("]");
		return g_xclip_text != NULL ? g_xclip_text : "[]";
	}
	SetLastError(NO_ERROR);
	while ( (fmt = EnumClipboardFormats(fmt)) != 0u ) {
		char name[256];
		xclip_format_name_buf(fmt, name, sizeof(name));
		if ( !first ) {
			xclip_text_append(",");
		}
		first = 0;
		xclip_text_appendf("{\"id\":%u,\"name\":\"", (unsigned int)fmt);
		xclip_json_escape_append(name);
		xclip_text_append("\"}");
	}
	if ( GetLastError() != NO_ERROR ) {
		xclip_set_win_error("EnumClipboardFormats");
	} else {
		xclip_set_error("");
	}
	CloseClipboard();
	xclip_text_append("]");
	return g_xclip_text != NULL ? g_xclip_text : "[]";
}

XL_EXPORT const char* x_clipboard_format_name(int format_id)
{
	char name[256];
	if ( format_id <= 0 ) {
		xclip_set_error("format id must be positive");
		return xclip_set_text_buffer("");
	}
	xclip_format_name_buf((UINT)format_id, name, sizeof(name));
	xclip_set_error("");
	return xclip_set_text_buffer(name);
}

XL_EXPORT int x_clipboard_register_format(const char* name)
{
	wchar_t* wide = xclip_utf8_to_wide_alloc(name);
	UINT id;
	if ( wide == NULL ) {
		return 0;
	}
	id = RegisterClipboardFormatW(wide);
	free(wide);
	if ( id == 0u ) {
		xclip_set_win_error("RegisterClipboardFormatW");
		return 0;
	}
	xclip_set_error("");
	return (int)id;
}

XL_EXPORT int x_clipboard_is_format_available(int format_id)
{
	if ( format_id <= 0 ) {
		return 0;
	}
	return IsClipboardFormatAvailable((UINT)format_id) ? 1 : 0;
}

XL_EXPORT int x_clipboard_has_text(void)
{
	return (IsClipboardFormatAvailable(CF_UNICODETEXT) ||
		IsClipboardFormatAvailable(CF_TEXT) ||
		IsClipboardFormatAvailable(CF_OEMTEXT)) ? 1 : 0;
}

XL_EXPORT int x_clipboard_has_image(void)
{
	return (IsClipboardFormatAvailable(CF_DIBV5) ||
		IsClipboardFormatAvailable(CF_DIB) ||
		IsClipboardFormatAvailable(CF_BITMAP)) ? 1 : 0;
}

XL_EXPORT int x_clipboard_has_files(void)
{
	return IsClipboardFormatAvailable(CF_HDROP) ? 1 : 0;
}

XL_EXPORT const char* x_clipboard_get_text(void)
{
	HGLOBAL hglobal;
	void* data;
	const char* out;
	if ( !xclip_open() ) {
		return xclip_set_text_buffer("");
	}
	if ( IsClipboardFormatAvailable(CF_UNICODETEXT) ) {
		hglobal = (HGLOBAL)GetClipboardData(CF_UNICODETEXT);
		if ( hglobal == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GetClipboardData");
			return xclip_set_text_buffer("");
		}
		data = GlobalLock(hglobal);
		if ( data == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GlobalLock");
			return xclip_set_text_buffer("");
		}
		out = xclip_wide_to_utf8((const wchar_t*)data);
		GlobalUnlock(hglobal);
		CloseClipboard();
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
			return xclip_set_text_buffer("");
		}
		data = GlobalLock(hglobal);
		if ( data == NULL ) {
			CloseClipboard();
			xclip_set_win_error("GlobalLock");
			return xclip_set_text_buffer("");
		}
		out = xclip_multibyte_to_utf8((const char*)data, code_page);
		GlobalUnlock(hglobal);
		CloseClipboard();
		xclip_set_error("");
		return out;
	}
	CloseClipboard();
	xclip_set_error("clipboard text is not available");
	return xclip_set_text_buffer("");
}

XL_EXPORT int x_clipboard_set_text(const char* text)
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
		free(wide);
		xclip_set_win_error("GlobalAlloc");
		return 0;
	}
	memory = GlobalLock(hglobal);
	if ( memory == NULL ) {
		free(wide);
		GlobalFree(hglobal);
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	memcpy(memory, wide, bytes);
	GlobalUnlock(hglobal);
	free(wide);
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

XL_EXPORT XL_ArtifactXValue x_clipboard_get_files(void)
{
	XL_ArtifactXValue out = xvoCreateArray();
	HDROP drop;
	UINT count;
	UINT i;
	if ( out == NULL ) {
		return NULL;
	}
	if ( !xclip_open() ) {
		return out;
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
		return out;
	}
	count = DragQueryFileW(drop, 0xFFFFFFFFu, NULL, 0u);
	for ( i = 0u; i < count; ++i ) {
		UINT chars = DragQueryFileW(drop, i, NULL, 0u);
		wchar_t* wide = (wchar_t*)calloc((size_t)chars + 1u, sizeof(wchar_t));
		if ( wide != NULL ) {
			char* utf8;
			int n;
			DragQueryFileW(drop, i, wide, chars + 1u);
			n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
			if ( n > 0 ) {
				utf8 = (char*)malloc((size_t)n);
				if ( utf8 != NULL ) {
					WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, n, NULL, NULL);
					xvoArrayAppendValue(out, xvoCreateText(utf8, (XL_ArtifactUInt32)strlen(utf8), 0), 1);
					free(utf8);
				}
			}
			free(wide);
		}
	}
	CloseClipboard();
	xclip_set_error("");
	return out;
}

XL_EXPORT const char* x_clipboard_get_file(void)
{
	HDROP drop;
	UINT chars;
	wchar_t* wide;
	const char* out;
	if ( !xclip_open() ) {
		return xclip_set_text_buffer("");
	}
	if ( !IsClipboardFormatAvailable(CF_HDROP) ) {
		CloseClipboard();
		xclip_set_error("clipboard files are not available");
		return xclip_set_text_buffer("");
	}
	drop = (HDROP)GetClipboardData(CF_HDROP);
	if ( drop == NULL || DragQueryFileW(drop, 0xFFFFFFFFu, NULL, 0u) == 0u ) {
		CloseClipboard();
		xclip_set_error("clipboard file list is empty");
		return xclip_set_text_buffer("");
	}
	chars = DragQueryFileW(drop, 0u, NULL, 0u);
	wide = (wchar_t*)calloc((size_t)chars + 1u, sizeof(wchar_t));
	if ( wide == NULL ) {
		CloseClipboard();
		xclip_set_error("out of memory");
		return xclip_set_text_buffer("");
	}
	DragQueryFileW(drop, 0u, wide, chars + 1u);
	out = xclip_wide_to_utf8(wide);
	free(wide);
	CloseClipboard();
	xclip_set_error("");
	return out;
}

static int xclip_set_files_wide(wchar_t** paths, XL_ArtifactUInt32 count)
{
	SIZE_T total_chars = 1u;
	SIZE_T bytes;
	HGLOBAL hglobal;
	void* memory;
	XClipDropFiles* drop;
	wchar_t* dst;
	XL_ArtifactUInt32 i;
	if ( paths == NULL || count == 0u ) {
		xclip_set_error("file list is empty");
		return 0;
	}
	for ( i = 0u; i < count; ++i ) {
		if ( paths[i] == NULL || paths[i][0] == L'\0' ) {
			xclip_set_error("file path is empty");
			return 0;
		}
		total_chars += (SIZE_T)lstrlenW(paths[i]) + 1u;
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

XL_EXPORT int x_clipboard_set_files(XL_ArtifactXValue paths_value)
{
	XL_ArtifactUInt32 count = xvoArrayItemCount(paths_value);
	wchar_t** paths;
	XL_ArtifactUInt32 i;
	int ok;
	if ( count == 0u ) {
		xclip_set_error("file list is empty");
		return 0;
	}
	paths = (wchar_t**)calloc((size_t)count, sizeof(wchar_t*));
	if ( paths == NULL ) {
		xclip_set_error("out of memory");
		return 0;
	}
	for ( i = 0u; i < count; ++i ) {
		paths[i] = xclip_xvalue_text_to_wide_alloc(xvoArrayGetValue(paths_value, i));
		if ( paths[i] == NULL ) {
			XL_ArtifactUInt32 j;
			for ( j = 0u; j < count; ++j ) {
				free(paths[j]);
			}
			free(paths);
			return 0;
		}
	}
	ok = xclip_set_files_wide(paths, count);
	for ( i = 0u; i < count; ++i ) {
		free(paths[i]);
	}
	free(paths);
	return ok;
}

XL_EXPORT int x_clipboard_set_file(const char* path)
{
	wchar_t* wide = xclip_utf8_to_wide_alloc(path);
	wchar_t* paths[1];
	int ok;
	if ( wide == NULL ) {
		return 0;
	}
	paths[0] = wide;
	ok = xclip_set_files_wide(paths, 1u);
	free(wide);
	return ok;
}

XL_EXPORT int x_clipboard_save_image(const char* path)
{
	wchar_t* wide_path;
	HGLOBAL hglobal;
	void* dib;
	SIZE_T size;
	UINT fmt;
	int owned = 0;
	int ok;
	wide_path = xclip_utf8_to_wide_alloc(path);
	if ( wide_path == NULL ) {
		return 0;
	}
	if ( !xclip_open() ) {
		free(wide_path);
		return 0;
	}
	hglobal = xclip_get_clipboard_dib(&fmt, &owned);
	(void)fmt;
	if ( hglobal == NULL ) {
		CloseClipboard();
		free(wide_path);
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
		free(wide_path);
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	ok = xclip_save_dib_as_bmp(wide_path, dib, size);
	GlobalUnlock(hglobal);
	if ( owned ) {
		GlobalFree(hglobal);
	}
	CloseClipboard();
	free(wide_path);
	if ( ok ) {
		xclip_set_error("");
	}
	return ok;
}

XL_EXPORT int x_clipboard_set_image(const char* path)
{
	wchar_t* wide_path;
	unsigned char* file_data;
	DWORD file_size;
	DWORD dib_size;
	HGLOBAL hglobal;
	void* memory;
	int ok;
	wide_path = xclip_utf8_to_wide_alloc(path);
	if ( wide_path == NULL ) {
		return 0;
	}
	file_data = xclip_read_file_wide(wide_path, &file_size);
	free(wide_path);
	if ( file_data == NULL ) {
		return 0;
	}
	if ( file_size < 14u || file_data[0] != 'B' || file_data[1] != 'M' ) {
		free(file_data);
		xclip_set_error("set_image currently expects a BMP file");
		return 0;
	}
	if ( xclip_read_u32_le(file_data + 10) >= file_size || xclip_read_u32_le(file_data + 10) < 14u ) {
		free(file_data);
		xclip_set_error("invalid BMP file");
		return 0;
	}
	dib_size = file_size - 14u;
	if ( dib_size < sizeof(BITMAPINFOHEADER) ) {
		free(file_data);
		xclip_set_error("invalid BMP DIB data");
		return 0;
	}
	hglobal = GlobalAlloc(GMEM_MOVEABLE, dib_size);
	if ( hglobal == NULL ) {
		free(file_data);
		xclip_set_win_error("GlobalAlloc");
		return 0;
	}
	memory = GlobalLock(hglobal);
	if ( memory == NULL ) {
		GlobalFree(hglobal);
		free(file_data);
		xclip_set_win_error("GlobalLock");
		return 0;
	}
	memcpy(memory, file_data + 14u, dib_size);
	GlobalUnlock(hglobal);
	free(file_data);
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

XL_EXPORT const char* x_clipboard_image_info_json(void)
{
	HGLOBAL hglobal;
	UINT fmt = 0u;
	int owned = 0;
	void* dib;
	SIZE_T size;
	const BITMAPINFOHEADER* bi;
	char fmt_name[64];
	if ( !xclip_open() ) {
		return xclip_set_text_buffer("{\"available\":false}");
	}
	hglobal = xclip_get_clipboard_dib(&fmt, &owned);
	if ( hglobal == NULL ) {
		CloseClipboard();
		return xclip_set_text_buffer("{\"available\":false}");
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
		return xclip_set_text_buffer("{\"available\":false}");
	}
	bi = (const BITMAPINFOHEADER*)dib;
	xclip_format_name_buf(fmt, fmt_name, sizeof(fmt_name));
	xclip_text_reset();
	xclip_text_append("{\"available\":true,\"format\":\"");
	xclip_json_escape_append(fmt_name);
	xclip_text_appendf("\",\"width\":%ld,\"height\":%ld,\"bit_count\":%u,\"compression\":%lu,\"bytes\":%lu}",
		(long)bi->biWidth,
		(long)(bi->biHeight < 0 ? -bi->biHeight : bi->biHeight),
		(unsigned int)bi->biBitCount,
		(unsigned long)bi->biCompression,
		(unsigned long)size);
	GlobalUnlock(hglobal);
	if ( owned ) {
		GlobalFree(hglobal);
	}
	CloseClipboard();
	xclip_set_error("");
	return g_xclip_text != NULL ? g_xclip_text : "{\"available\":true}";
}
