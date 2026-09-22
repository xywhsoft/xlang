#include "../../include/xhttp_protocol_package.h"
#include "../../include/xrt/mime.h"

#include <string.h>



#if defined(XHTTP_FEATURE_MIME_TYPES)

/* 设置 MIME 类型查表的参数错误。 */
static void __xhttpMimeTypesInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT, "xhttp.mime", 1, "invalid argument"
	);
}

/* 内置表只保存静态文本，查询路径不需要初始化、锁和堆分配。 */
typedef struct xrt_mime_type_entry {
	cstr Extension;
	xstrview Type;
} xrt_mime_type_entry;



/*
	扩展名按 ASCII 小写字典序排列。
	集合覆盖常见 Web、文档、归档、字体、图像、音频和视频资产。
*/
static const xrt_mime_type_entry __g_XrtMimeTypes[] = {
	{ "3g2", XRT_STR_INIT("video/3gpp2") },
	{ "3gp", XRT_STR_INIT("video/3gpp") },
	{ "7z", XRT_STR_INIT("application/x-7z-compressed") },
	{ "aac", XRT_STR_INIT("audio/aac") },
	{ "apng", XRT_STR_INIT("image/apng") },
	{ "atom", XRT_STR_INIT("application/atom+xml") },
	{ "avi", XRT_STR_INIT("video/x-msvideo") },
	{ "avif", XRT_STR_INIT("image/avif") },
	{ "bin", XRT_STR_INIT("application/octet-stream") },
	{ "bmp", XRT_STR_INIT("image/bmp") },
	{ "bz", XRT_STR_INIT("application/x-bzip") },
	{ "bz2", XRT_STR_INIT("application/x-bzip2") },
	{ "cbor", XRT_STR_INIT("application/cbor") },
	{ "cer", XRT_STR_INIT("application/pkix-cert") },
	{ "css", XRT_STR_INIT("text/css; charset=utf-8") },
	{ "csv", XRT_STR_INIT("text/csv; charset=utf-8") },
	{ "doc", XRT_STR_INIT("application/msword") },
	{ "docx", XRT_STR_INIT("application/vnd.openxmlformats-officedocument.wordprocessingml.document") },
	{ "eot", XRT_STR_INIT("application/vnd.ms-fontobject") },
	{ "epub", XRT_STR_INIT("application/epub+zip") },
	{ "flac", XRT_STR_INIT("audio/flac") },
	{ "geojson", XRT_STR_INIT("application/geo+json") },
	{ "gif", XRT_STR_INIT("image/gif") },
	{ "gz", XRT_STR_INIT("application/gzip") },
	{ "heic", XRT_STR_INIT("image/heic") },
	{ "heif", XRT_STR_INIT("image/heif") },
	{ "htm", XRT_STR_INIT("text/html; charset=utf-8") },
	{ "html", XRT_STR_INIT("text/html; charset=utf-8") },
	{ "ico", XRT_STR_INIT("image/vnd.microsoft.icon") },
	{ "ics", XRT_STR_INIT("text/calendar; charset=utf-8") },
	{ "jar", XRT_STR_INIT("application/java-archive") },
	{ "jfif", XRT_STR_INIT("image/jpeg") },
	{ "jpeg", XRT_STR_INIT("image/jpeg") },
	{ "jpg", XRT_STR_INIT("image/jpeg") },
	{ "js", XRT_STR_INIT("text/javascript; charset=utf-8") },
	{ "json", XRT_STR_INIT("application/json") },
	{ "jsonld", XRT_STR_INIT("application/ld+json") },
	{ "log", XRT_STR_INIT("text/plain; charset=utf-8") },
	{ "m4a", XRT_STR_INIT("audio/mp4") },
	{ "m4v", XRT_STR_INIT("video/mp4") },
	{ "map", XRT_STR_INIT("application/json") },
	{ "md", XRT_STR_INIT("text/markdown; charset=utf-8") },
	{ "mid", XRT_STR_INIT("audio/midi") },
	{ "midi", XRT_STR_INIT("audio/midi") },
	{ "mjs", XRT_STR_INIT("text/javascript; charset=utf-8") },
	{ "mov", XRT_STR_INIT("video/quicktime") },
	{ "mp3", XRT_STR_INIT("audio/mpeg") },
	{ "mp4", XRT_STR_INIT("video/mp4") },
	{ "mpeg", XRT_STR_INIT("video/mpeg") },
	{ "mpg", XRT_STR_INIT("video/mpeg") },
	{ "oga", XRT_STR_INIT("audio/ogg") },
	{ "ogg", XRT_STR_INIT("audio/ogg") },
	{ "ogv", XRT_STR_INIT("video/ogg") },
	{ "ogx", XRT_STR_INIT("application/ogg") },
	{ "opus", XRT_STR_INIT("audio/opus") },
	{ "otf", XRT_STR_INIT("font/otf") },
	{ "pdf", XRT_STR_INIT("application/pdf") },
	{ "png", XRT_STR_INIT("image/png") },
	{ "ppt", XRT_STR_INIT("application/vnd.ms-powerpoint") },
	{ "pptx", XRT_STR_INIT("application/vnd.openxmlformats-officedocument.presentationml.presentation") },
	{ "rar", XRT_STR_INIT("application/vnd.rar") },
	{ "rss", XRT_STR_INIT("application/rss+xml") },
	{ "rtf", XRT_STR_INIT("application/rtf") },
	{ "sql", XRT_STR_INIT("application/sql") },
	{ "svg", XRT_STR_INIT("image/svg+xml") },
	{ "tar", XRT_STR_INIT("application/x-tar") },
	{ "tif", XRT_STR_INIT("image/tiff") },
	{ "tiff", XRT_STR_INIT("image/tiff") },
	{ "toml", XRT_STR_INIT("application/toml") },
	{ "ts", XRT_STR_INIT("video/mp2t") },
	{ "ttf", XRT_STR_INIT("font/ttf") },
	{ "txt", XRT_STR_INIT("text/plain; charset=utf-8") },
	{ "vtt", XRT_STR_INIT("text/vtt; charset=utf-8") },
	{ "wasm", XRT_STR_INIT("application/wasm") },
	{ "wav", XRT_STR_INIT("audio/wav") },
	{ "weba", XRT_STR_INIT("audio/webm") },
	{ "webm", XRT_STR_INIT("video/webm") },
	{ "webmanifest", XRT_STR_INIT("application/manifest+json") },
	{ "webp", XRT_STR_INIT("image/webp") },
	{ "woff", XRT_STR_INIT("font/woff") },
	{ "woff2", XRT_STR_INIT("font/woff2") },
	{ "xhtml", XRT_STR_INIT("application/xhtml+xml") },
	{ "xls", XRT_STR_INIT("application/vnd.ms-excel") },
	{ "xlsx", XRT_STR_INIT("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") },
	{ "xml", XRT_STR_INIT("application/xml") },
	{ "xsd", XRT_STR_INIT("application/xml") },
	{ "xsl", XRT_STR_INIT("application/xml") },
	{ "yaml", XRT_STR_INIT("application/yaml") },
	{ "yml", XRT_STR_INIT("application/yaml") },
	{ "zip", XRT_STR_INIT("application/zip") },
	{ "zst", XRT_STR_INIT("application/zstd") }
};



/* 按 ASCII 规则折叠扩展名中的大写字母。 */
static unsigned char __xrtMimeLower(
	unsigned char iByte
)
{
	if ( (iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(
			iByte + ((unsigned char)'a' - (unsigned char)'A')
		);
	}
	return iByte;
}



/* 比较借用扩展名与表内小写零结尾扩展名。 */
static int __xrtMimeExtCompare(
	xstrview Extension,
	cstr sExpected
)
{
	size_t i = 0;

	while ( (i < Extension.Size) && (sExpected[i] != '\0') ) {
		unsigned char iActual = __xrtMimeLower(
			(unsigned char)Extension.Data[i]
		);
		unsigned char iWanted = (unsigned char)sExpected[i];

		if ( iActual < iWanted ) {
			return -1;
		}
		if ( iActual > iWanted ) {
			return 1;
		}
		i++;
	}
	if ( i < Extension.Size ) {
		return 1;
	}
	if ( sExpected[i] != '\0' ) {
		return -1;
	}
	return 0;
}



/* 按扩展名查询内置媒体类型。 */
XRT_API xstrview xrtMimeByExt(
	xstrview Extension
)
{
	size_t iLeft = 0;
	size_t iRight = sizeof(__g_XrtMimeTypes) /
		sizeof(__g_XrtMimeTypes[0]);

	if ( !xrtMemRangeValid(
		Extension.Data,
		Extension.Size
	) ) {
		__xhttpMimeTypesInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	if ( (Extension.Size != 0) &&
		(Extension.Data[0] == '.') ) {
		Extension.Data++;
		Extension.Size--;
	}
	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2u);
		int iOrder = __xrtMimeExtCompare(
			Extension,
			__g_XrtMimeTypes[iMiddle].Extension
		);

		if ( iOrder < 0 ) {
			iRight = iMiddle;
		} else if ( iOrder > 0 ) {
			iLeft = iMiddle + 1u;
		} else {
			return __g_XrtMimeTypes[iMiddle].Type;
		}
	}
	return (xstrview){ NULL, 0 };
}



/* 从最后一个路径段提取最后一个扩展名并查询。 */
XRT_API xstrview xrtMimeByPath(
	xstrview Path
)
{
	size_t iName = 0;
	size_t iDot = XRT_NPOS;
	size_t i;

	if ( !xrtMemRangeValid(Path.Data, Path.Size) ) {
		__xhttpMimeTypesInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	for ( i = 0; i < Path.Size; i++ ) {
		if ( (Path.Data[i] == '/') ||
			(Path.Data[i] == '\\') ) {
			iName = i + 1u;
			iDot = XRT_NPOS;
		} else if ( Path.Data[i] == '.' ) {
			iDot = i;
		}
	}
	if ( (iDot == XRT_NPOS) ||
		(iDot == iName) ||
		(iDot + 1u == Path.Size) ) {
		return (xstrview){ NULL, 0 };
	}
	return xrtMimeByExt((xstrview){
		Path.Data + iDot + 1u,
		Path.Size - iDot - 1u
	});
}



/* 为经典 C 字符串路径提供未知类型回退。 */
XRT_API cstr xrtMime(
	cstr sPath
)
{
	xstrview Type;

	if ( sPath == NULL ) {
		return "application/octet-stream";
	}
	Type = xrtMimeByPath((xstrview){
		sPath,
		strlen(sPath)
	});
	if ( Type.Size == 0 ) {
		return "application/octet-stream";
	}
	return Type.Data;
}

#endif
