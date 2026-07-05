#include "app/xide_app.h"
#include "app/xide_platform.h"
#include "i18n/xide_i18n.h"
#include "ui/xide_main_window.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define XIDE_BUNDLED_FONT_NAME "Consolas-with-Yahei_Nerd_Font.ttf"
#define XIDE_UI_FONT_SIZE 18.0f
#define XIDE_CODE_FONT_SIZE 18.0f
#define XIDE_FONT_CONFIG_NAME "ide.json"

typedef struct xide_font_config_t {
	char sUiFont[MAX_PATH];
	char sCodeFont[MAX_PATH];
	float fUiSize;
	float fCodeSize;
} xide_font_config_t;

static int xideFileExists(const char* sPath)
{
	FILE* file;
	if ( sPath == NULL || sPath[0] == '\0' ) return 0;
	file = fopen(sPath, "rb");
	if ( file == NULL ) return 0;
	fclose(file);
	return 1;
}

static const char* xideFindExisting(const char** paths, int count)
{
	int i;
	for ( i = 0; i < count; i++ ) {
		if ( xideFileExists(paths[i]) ) return paths[i];
	}
	return NULL;
}

static int xideGetExeDir(char* sBuffer, int iCapacity)
{
	DWORD n;
	char* p;
	if ( sBuffer == NULL || iCapacity <= 0 ) return 0;
	n = GetModuleFileNameA(NULL, sBuffer, (DWORD)iCapacity);
	if ( n == 0 || n >= (DWORD)iCapacity ) return 0;
	p = strrchr(sBuffer, '\\');
	if ( p == NULL ) p = strrchr(sBuffer, '/');
	if ( p == NULL ) return 0;
	*p = '\0';
	return 1;
}

static int xidePathJoin(char* sBuffer, int iCapacity, const char* sBase, const char* sName)
{
	int baseLen;
	int nameLen;
	if ( sBuffer == NULL || sBase == NULL || sName == NULL || iCapacity <= 0 ) return 0;
	baseLen = (int)strlen(sBase);
	nameLen = (int)strlen(sName);
	if ( baseLen + 1 + nameLen >= iCapacity ) return 0;
	memcpy(sBuffer, sBase, (size_t)baseLen + 1);
	if ( baseLen > 0 && sBuffer[baseLen - 1] != '\\' && sBuffer[baseLen - 1] != '/' ) {
		sBuffer[baseLen++] = '\\';
		sBuffer[baseLen] = '\0';
	}
	memcpy(sBuffer + baseLen, sName, (size_t)nameLen + 1);
	return 1;
}

static int xidePathIsAbsolute(const char* sPath)
{
	if ( sPath == NULL || sPath[0] == '\0' ) return 0;
	if ( sPath[0] == '\\' || sPath[0] == '/' ) return 1;
	return ((sPath[0] >= 'A' && sPath[0] <= 'Z') || (sPath[0] >= 'a' && sPath[0] <= 'z')) && sPath[1] == ':';
}

static int xideResolveExePath(char* sBuffer, int iCapacity, const char* sPath)
{
	char exeDir[MAX_PATH];
	if ( sBuffer == NULL || sPath == NULL || sPath[0] == '\0' || iCapacity <= 0 ) return 0;
	if ( xidePathIsAbsolute(sPath) ) {
		if ( (int)strlen(sPath) >= iCapacity ) return 0;
		strcpy(sBuffer, sPath);
		return 1;
	}
	if ( !xideGetExeDir(exeDir, (int)sizeof(exeDir)) ) return 0;
	return xidePathJoin(sBuffer, iCapacity, exeDir, sPath);
}

static const char* xideJsonSkipWs(const char* p, const char* end)
{
	while ( p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ) p++;
	return p;
}

static const char* xideJsonSkipString(const char* p, const char* end)
{
	if ( p >= end || *p != '"' ) return NULL;
	p++;
	while ( p < end ) {
		if ( *p == '\\' ) {
			p += (p + 1 < end) ? 2 : 1;
			continue;
		}
		if ( *p == '"' ) return p + 1;
		p++;
	}
	return NULL;
}

static const char* xideJsonSkipValue(const char* p, const char* end);

static const char* xideJsonSkipObject(const char* p, const char* end)
{
	if ( p >= end || *p != '{' ) return NULL;
	p++;
	while ( p < end ) {
		p = xideJsonSkipWs(p, end);
		if ( p >= end ) return NULL;
		if ( *p == '}' ) return p + 1;
		p = xideJsonSkipString(p, end);
		if ( p == NULL ) return NULL;
		p = xideJsonSkipWs(p, end);
		if ( p >= end || *p != ':' ) return NULL;
		p = xideJsonSkipValue(xideJsonSkipWs(p + 1, end), end);
		if ( p == NULL ) return NULL;
		p = xideJsonSkipWs(p, end);
		if ( p < end && *p == ',' ) {
			p++;
			continue;
		}
		if ( p < end && *p == '}' ) return p + 1;
		return NULL;
	}
	return NULL;
}

static const char* xideJsonSkipArray(const char* p, const char* end)
{
	if ( p >= end || *p != '[' ) return NULL;
	p++;
	while ( p < end ) {
		p = xideJsonSkipWs(p, end);
		if ( p >= end ) return NULL;
		if ( *p == ']' ) return p + 1;
		p = xideJsonSkipValue(p, end);
		if ( p == NULL ) return NULL;
		p = xideJsonSkipWs(p, end);
		if ( p < end && *p == ',' ) {
			p++;
			continue;
		}
		if ( p < end && *p == ']' ) return p + 1;
		return NULL;
	}
	return NULL;
}

static const char* xideJsonSkipAtom(const char* p, const char* end)
{
	while ( p < end && strchr(" \t\r\n,]}", *p) == NULL ) p++;
	return p;
}

static const char* xideJsonSkipValue(const char* p, const char* end)
{
	p = xideJsonSkipWs(p, end);
	if ( p >= end ) return NULL;
	if ( *p == '"' ) return xideJsonSkipString(p, end);
	if ( *p == '{' ) return xideJsonSkipObject(p, end);
	if ( *p == '[' ) return xideJsonSkipArray(p, end);
	return xideJsonSkipAtom(p, end);
}

static int xideJsonReadString(const char* p, const char* end, char* sBuffer, int iCapacity)
{
	int n = 0;
	if ( p >= end || *p != '"' || sBuffer == NULL || iCapacity <= 0 ) return 0;
	p++;
	while ( p < end ) {
		unsigned char c = (unsigned char)*p++;
		if ( c == '"' ) {
			sBuffer[n] = '\0';
			return 1;
		}
		if ( c == '\\' ) {
			if ( p >= end ) return 0;
			c = (unsigned char)*p++;
			switch ( c ) {
			case '"': break;
			case '\\': break;
			case '/': break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'u':
				if ( end - p < 4 ) return 0;
				p += 4;
				c = '?';
				break;
			default:
				return 0;
			}
		}
		if ( n + 1 >= iCapacity ) return 0;
		sBuffer[n++] = (char)c;
	}
	return 0;
}

static void xideCopyConfigString(char* sDst, int iCapacity, const char* sSrc)
{
	int len;
	if ( sDst == NULL || iCapacity <= 0 || sSrc == NULL ) return;
	len = (int)strlen(sSrc);
	if ( len >= iCapacity ) len = iCapacity - 1;
	memcpy(sDst, sSrc, (size_t)len);
	sDst[len] = '\0';
}

static float xideParseFontSize(const char* sValue, float fDefault)
{
	float value;
	if ( sValue == NULL || sValue[0] == '\0' ) return fDefault;
	value = (float)atof(sValue);
	if ( value < 8.0f || value > 72.0f ) return fDefault;
	return value;
}

static char* xideReadTextFile(const char* sPath, int* pSize)
{
	FILE* file;
	char* data;
	long size;
	int readSize;
	if ( pSize != NULL ) *pSize = 0;
	file = fopen(sPath, "rb");
	if ( file == NULL ) return NULL;
	if ( fseek(file, 0, SEEK_END) != 0 ) {
		fclose(file);
		return NULL;
	}
	size = ftell(file);
	if ( size < 0 || size > 1024 * 1024 ) {
		fclose(file);
		return NULL;
	}
	if ( fseek(file, 0, SEEK_SET) != 0 ) {
		fclose(file);
		return NULL;
	}
	data = (char*)malloc((size_t)size + 1u);
	if ( data == NULL ) {
		fclose(file);
		return NULL;
	}
	readSize = (int)fread(data, 1, (size_t)size, file);
	fclose(file);
	if ( readSize != (int)size ) {
		free(data);
		return NULL;
	}
	data[size] = '\0';
	if ( pSize != NULL ) *pSize = (int)size;
	return data;
}

static int xideJsonFindMember(const char* objectStart, const char* end, const char* sKey, const char** ppValue, const char** ppValueEnd)
{
	char key[128];
	const char* p;
	const char* keyEnd;
	const char* value;
	const char* valueEnd;
	if ( ppValue != NULL ) *ppValue = NULL;
	if ( ppValueEnd != NULL ) *ppValueEnd = NULL;
	if ( objectStart == NULL || end == NULL || sKey == NULL ) return 0;
	p = xideJsonSkipWs(objectStart, end);
	if ( p >= end || *p != '{' ) return 0;
	p++;
	while ( p < end ) {
		p = xideJsonSkipWs(p, end);
		if ( p >= end || *p == '}' ) return 0;
		keyEnd = xideJsonSkipString(p, end);
		if ( keyEnd == NULL || !xideJsonReadString(p, keyEnd, key, (int)sizeof(key)) ) return 0;
		p = xideJsonSkipWs(keyEnd, end);
		if ( p >= end || *p != ':' ) return 0;
		value = xideJsonSkipWs(p + 1, end);
		valueEnd = xideJsonSkipValue(value, end);
		if ( valueEnd == NULL ) return 0;
		if ( strcmp(key, sKey) == 0 ) {
			if ( ppValue != NULL ) *ppValue = value;
			if ( ppValueEnd != NULL ) *ppValueEnd = valueEnd;
			return 1;
		}
		p = xideJsonSkipWs(valueEnd, end);
		if ( p < end && *p == ',' ) {
			p++;
			continue;
		}
		if ( p < end && *p == '}' ) return 0;
		return 0;
	}
	return 0;
}

static int xideJsonGetObjectString(const char* objectStart, const char* end, const char* sKey, char* sBuffer, int iCapacity)
{
	const char* value;
	const char* valueEnd;
	if ( !xideJsonFindMember(objectStart, end, sKey, &value, &valueEnd) ) return 0;
	value = xideJsonSkipWs(value, valueEnd);
	return xideJsonReadString(value, valueEnd, sBuffer, iCapacity);
}

static int xideJsonGetObjectFloat(const char* objectStart, const char* end, const char* sKey, float* pValue)
{
	const char* value;
	const char* valueEnd;
	char temp[64];
	int len;
	if ( pValue == NULL ) return 0;
	if ( !xideJsonFindMember(objectStart, end, sKey, &value, &valueEnd) ) return 0;
	value = xideJsonSkipWs(value, valueEnd);
	if ( value < valueEnd && *value == '"' ) {
		if ( !xideJsonReadString(value, valueEnd, temp, (int)sizeof(temp)) ) return 0;
		*pValue = xideParseFontSize(temp, *pValue);
		return 1;
	}
	len = (int)(valueEnd - value);
	if ( len <= 0 || len >= (int)sizeof(temp) ) return 0;
	memcpy(temp, value, (size_t)len);
	temp[len] = '\0';
	*pValue = xideParseFontSize(temp, *pValue);
	return 1;
}

static void xideFontConfigDefault(xide_font_config_t* pConfig)
{
	if ( pConfig == NULL ) return;
	memset(pConfig, 0, sizeof(*pConfig));
	xideCopyConfigString(pConfig->sUiFont, (int)sizeof(pConfig->sUiFont), "res\\" XIDE_BUNDLED_FONT_NAME);
	xideCopyConfigString(pConfig->sCodeFont, (int)sizeof(pConfig->sCodeFont), "res\\" XIDE_BUNDLED_FONT_NAME);
	pConfig->fUiSize = XIDE_UI_FONT_SIZE;
	pConfig->fCodeSize = XIDE_CODE_FONT_SIZE;
}

static int xideGetOptionDir(char* sBuffer, int iCapacity)
{
	char exeDir[MAX_PATH];
	if ( !xideGetExeDir(exeDir, (int)sizeof(exeDir)) ) return 0;
	return xidePathJoin(sBuffer, iCapacity, exeDir, "option");
}

static int xideGetFontConfigPath(char* sBuffer, int iCapacity)
{
	char optionDir[MAX_PATH];
	if ( !xideGetOptionDir(optionDir, (int)sizeof(optionDir)) ) return 0;
	return xidePathJoin(sBuffer, iCapacity, optionDir, XIDE_FONT_CONFIG_NAME);
}

static void xideFontConfigWriteDefault(const xide_font_config_t* pConfig)
{
	char optionDir[MAX_PATH];
	char configPath[MAX_PATH];
	FILE* file;
	if ( pConfig == NULL ) return;
	if ( !xideGetOptionDir(optionDir, (int)sizeof(optionDir)) ) return;
	(void)CreateDirectoryA(optionDir, NULL);
	if ( !xideGetFontConfigPath(configPath, (int)sizeof(configPath)) ) return;
	file = fopen(configPath, "wb");
	if ( file == NULL ) return;
	fprintf(file,
		"{\n"
		"  \"font\": {\n"
		"    \"ui\": {\n"
		"      \"path\": \"");
	for ( const char* p = pConfig->sUiFont; *p != '\0'; p++ ) {
		if ( *p == '\\' || *p == '"' ) fputc('\\', file);
		fputc(*p, file);
	}
	fprintf(file,
		"\",\n"
		"      \"size\": %.0f\n"
		"    },\n"
		"    \"code\": {\n"
		"      \"path\": \"",
		pConfig->fUiSize);
	for ( const char* p = pConfig->sCodeFont; *p != '\0'; p++ ) {
		if ( *p == '\\' || *p == '"' ) fputc('\\', file);
		fputc(*p, file);
	}
	fprintf(file,
		"\",\n"
		"      \"size\": %.0f\n"
		"    }\n"
		"  }\n"
		"}\n",
		pConfig->fCodeSize);
	fclose(file);
}

static void xideFontConfigLoad(xide_font_config_t* pConfig)
{
	char configPath[MAX_PATH];
	char* json;
	const char* end;
	const char* fontObj;
	const char* fontEnd;
	const char* uiObj;
	const char* uiEnd;
	const char* codeObj;
	const char* codeEnd;
	int size;
	xideFontConfigDefault(pConfig);
	if ( pConfig == NULL ) return;
	if ( !xideGetFontConfigPath(configPath, (int)sizeof(configPath)) ) return;
	json = xideReadTextFile(configPath, &size);
	if ( json == NULL ) {
		xideFontConfigWriteDefault(pConfig);
		return;
	}
	end = json + size;
	if ( xideJsonFindMember(json, end, "font", &fontObj, &fontEnd) ) {
		if ( xideJsonFindMember(fontObj, fontEnd, "ui", &uiObj, &uiEnd) ) {
			(void)xideJsonGetObjectString(uiObj, uiEnd, "path", pConfig->sUiFont, (int)sizeof(pConfig->sUiFont));
			(void)xideJsonGetObjectFloat(uiObj, uiEnd, "size", &pConfig->fUiSize);
		}
		if ( xideJsonFindMember(fontObj, fontEnd, "code", &codeObj, &codeEnd) ) {
			(void)xideJsonGetObjectString(codeObj, codeEnd, "path", pConfig->sCodeFont, (int)sizeof(pConfig->sCodeFont));
			(void)xideJsonGetObjectFloat(codeObj, codeEnd, "size", &pConfig->fCodeSize);
		}
	}
	free(json);
}

static const char* xideFindBundledFont(void)
{
	static char sPath[MAX_PATH];
	if ( !xideGetExeDir(sPath, (int)sizeof(sPath)) ) return NULL;
	if ( (int)strlen(sPath) + (int)strlen("\\res\\" XIDE_BUNDLED_FONT_NAME) >= (int)sizeof(sPath) ) return NULL;
	strcat(sPath, "\\res\\" XIDE_BUNDLED_FONT_NAME);
	return xideFileExists(sPath) ? sPath : NULL;
}

static const char* xideFindUiFont(const xide_font_config_t* pConfig)
{
	static char configured[MAX_PATH];
	const char* bundled = xideFindBundledFont();
	static const char* paths[] = {
		"C:\\Windows\\Fonts\\segoeui.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\arial.ttf"
	};
	if ( pConfig != NULL && xideResolveExePath(configured, (int)sizeof(configured), pConfig->sUiFont) && xideFileExists(configured) ) return configured;
	if ( bundled != NULL ) return bundled;
	return xideFindExisting(paths, (int)(sizeof(paths) / sizeof(paths[0])));
}

static const char* xideFindCodeFont(const xide_font_config_t* pConfig)
{
	static char configured[MAX_PATH];
	const char* bundled = xideFindBundledFont();
	static const char* paths[] = {
		"C:\\Windows\\Fonts\\CascadiaMono.ttf",
		"C:\\Windows\\Fonts\\CascadiaCode.ttf",
		"C:\\Windows\\Fonts\\consola.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc"
	};
	if ( pConfig != NULL && xideResolveExePath(configured, (int)sizeof(configured), pConfig->sCodeFont) && xideFileExists(configured) ) return configured;
	if ( bundled != NULL ) return bundled;
	return xideFindExisting(paths, (int)(sizeof(paths) / sizeof(paths[0])));
}

static const char* xideFindFallbackFont(void)
{
	const char* bundled = xideFindBundledFont();
	static const char* paths[] = {
		"C:\\Windows\\Fonts\\NotoSansSC-VF.ttf",
		"C:\\Windows\\Fonts\\msyh.ttc",
		"C:\\Windows\\Fonts\\simhei.ttf",
		"C:\\Windows\\Fonts\\simsun.ttc"
	};
	if ( bundled != NULL ) return bundled;
	return xideFindExisting(paths, (int)(sizeof(paths) / sizeof(paths[0])));
}

static void xideUsage(void)
{
	printf("usage: xlang_ide [--frames N] [--seconds N] [--lang en|zh|ru|ja|fr|es] [--check-language-rules] [--check-workspace]\n");
}

int xideAppParseArgs(xide_app_t* pApp, int argc, char** argv)
{
	int i;
	int lang;
	if ( pApp == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	pApp->iLanguageId = XUI_LANGUAGE_ZH;
	for ( i = 1; i < argc; i++ ) {
		if ( strcmp(argv[i], "--frames") == 0 ) {
			if ( i + 1 >= argc ) return XGE_ERROR_INVALID_ARGUMENT;
			pApp->iMaxFrames = atoi(argv[++i]);
			if ( pApp->iMaxFrames <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
		} else if ( strncmp(argv[i], "--frames=", 9) == 0 ) {
			pApp->iMaxFrames = atoi(argv[i] + 9);
			if ( pApp->iMaxFrames <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
		} else if ( strcmp(argv[i], "--seconds") == 0 ) {
			if ( i + 1 >= argc ) return XGE_ERROR_INVALID_ARGUMENT;
			pApp->fMaxSeconds = atof(argv[++i]);
			if ( pApp->fMaxSeconds <= 0.0 ) return XGE_ERROR_INVALID_ARGUMENT;
		} else if ( strncmp(argv[i], "--seconds=", 10) == 0 ) {
			pApp->fMaxSeconds = atof(argv[i] + 10);
			if ( pApp->fMaxSeconds <= 0.0 ) return XGE_ERROR_INVALID_ARGUMENT;
		} else if ( strcmp(argv[i], "--lang") == 0 ) {
			if ( i + 1 >= argc ) return XGE_ERROR_INVALID_ARGUMENT;
			lang = xideI18nLanguageFromCode(argv[++i]);
			if ( lang < 0 ) return XGE_ERROR_INVALID_ARGUMENT;
			pApp->iLanguageId = lang;
		} else if ( strncmp(argv[i], "--lang=", 7) == 0 ) {
			lang = xideI18nLanguageFromCode(argv[i] + 7);
			if ( lang < 0 ) return XGE_ERROR_INVALID_ARGUMENT;
			pApp->iLanguageId = lang;
		} else if ( strcmp(argv[i], "--check-language-rules") == 0 ) {
			pApp->bCheckLanguageRules = 1;
		} else if ( strcmp(argv[i], "--check-workspace") == 0 ) {
			pApp->bCheckWorkspace = 1;
		} else if ( strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ) {
			xideUsage();
			return 1;
		} else {
			return XGE_ERROR_INVALID_ARGUMENT;
		}
	}
	return XGE_OK;
}

static int xideCreateTargetSurface(xide_app_t* pApp, int w, int h)
{
	xui_surface_desc_t desc;
	xui_surface surface = NULL;
	int ret;
	if ( pApp == NULL || w <= 0 || h <= 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&desc, 0, sizeof(desc));
	desc.iKind = XUI_SURFACE_KIND_TEXTURE;
	desc.iWidth = w;
	desc.iHeight = h;
	desc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
	desc.iFlags = XUI_SURFACE_ALPHA_PREMULTIPLIED | XUI_SURFACE_USAGE_TARGET;
	ret = pApp->tProxy.surfaceCreate(&pApp->tProxy, &surface, &desc);
	if ( ret != XUI_OK ) return ret;
	if ( pApp->pTarget != NULL ) (void)pApp->tProxy.surfaceDestroy(&pApp->tProxy, pApp->pTarget);
	pApp->pTarget = surface;
	return XUI_OK;
}

static int xideSyncViewport(xide_app_t* pApp)
{
	int w;
	int h;
	int ret;
	w = xgeGetWidth();
	h = xgeGetHeight();
	if ( w <= 0 ) w = XIDE_WINDOW_W;
	if ( h <= 0 ) h = XIDE_WINDOW_H;
	if ( pApp->pTarget != NULL && pApp->iViewportW == w && pApp->iViewportH == h ) return XUI_OK;
	ret = xuiInputViewport(pApp->pContext, (float)w, (float)h);
	if ( ret != XUI_OK ) return ret;
	ret = xideCreateTargetSurface(pApp, w, h);
	if ( ret != XUI_OK ) return ret;
	pApp->iViewportW = w;
	pApp->iViewportH = h;
	xideMainWindowLayout(pApp);
	xgeRenderRequest();
	return XUI_OK;
}

int xideAppCreate(xide_app_t* pApp)
{
	xide_font_config_t fontConfig;
	const char* uiFont;
	const char* codeFont;
	const char* fallbackFont;
	int ret;
	xideWorkspaceInit(&pApp->tWorkspace);
	if ( !xideWorkspaceCreateDefault(&pApp->tWorkspace) ) return XUI_ERROR;
	xideFontConfigLoad(&fontConfig);
	pApp->tProxy = xuiProxyXge();
	ret = xuiSetDefaultLanguage(pApp->iLanguageId);
	if ( ret != XUI_OK ) return ret;
	ret = xuiCreate(&pApp->pContext);
	if ( ret != XUI_OK ) return ret;
	ret = xuiSetLanguage(pApp->pContext, pApp->iLanguageId);
	if ( ret != XUI_OK ) return ret;
	xgeSetTitle(xideTr(pApp->pContext, XIDE_TR_APP_TITLE));
	ret = xuiSetProxy(pApp->pContext, &pApp->tProxy);
	if ( ret != XUI_OK ) return ret;
	ret = xuiCodeFindScopeCreate(&pApp->pFindScope);
	if ( ret != XUI_OK ) return ret;
	pApp->iViewportW = xgeGetWidth() > 0 ? xgeGetWidth() : XIDE_WINDOW_W;
	pApp->iViewportH = xgeGetHeight() > 0 ? xgeGetHeight() : XIDE_WINDOW_H;
	ret = xuiInputViewport(pApp->pContext, (float)pApp->iViewportW, (float)pApp->iViewportH);
	if ( ret != XUI_OK ) return ret;
	ret = xideCreateTargetSurface(pApp, pApp->iViewportW, pApp->iViewportH);
	if ( ret != XUI_OK ) return ret;
	uiFont = xideFindUiFont(&fontConfig);
	if ( uiFont == NULL ) return XUI_ERROR_FILE_NOT_FOUND;
	ret = pApp->tProxy.fontLoadFile(&pApp->tProxy, &pApp->pUiFont, uiFont, fontConfig.fUiSize, XUI_FONT_FORMAT_TTF);
	if ( ret != XUI_OK ) return ret;
	fallbackFont = xideFindFallbackFont();
	if ( fallbackFont != NULL ) (void)xgeFontFallbackSet(fallbackFont, fontConfig.fCodeSize);
	codeFont = xideFindCodeFont(&fontConfig);
	if ( codeFont != NULL ) (void)pApp->tProxy.fontLoadFile(&pApp->tProxy, &pApp->pCodeFont, codeFont, fontConfig.fCodeSize, XUI_FONT_FORMAT_TTF);
	xgeFontFallbackClear();
	if ( pApp->pCodeFont == NULL ) pApp->pCodeFont = pApp->pUiFont;
	(void)xuiSetDefaultFont(pApp->pContext, pApp->pUiFont);
	return xideMainWindowCreate(pApp);
}

void xideAppDestroy(xide_app_t* pApp)
{
	if ( pApp == NULL ) return;
	if ( pApp->pFindScope != NULL ) xuiCodeFindScopeDestroy(pApp->pFindScope);
	if ( pApp->pContext != NULL ) xuiDestroy(pApp->pContext);
	if ( pApp->pCodeFont != NULL && pApp->pCodeFont != pApp->pUiFont ) pApp->tProxy.fontDestroy(&pApp->tProxy, pApp->pCodeFont);
	if ( pApp->pUiFont != NULL ) pApp->tProxy.fontDestroy(&pApp->tProxy, pApp->pUiFont);
	if ( pApp->pTarget != NULL ) pApp->tProxy.surfaceDestroy(&pApp->tProxy, pApp->pTarget);
	xideWorkspaceClear(&pApp->tWorkspace);
	memset(pApp, 0, sizeof(*pApp));
}

static int xideAppCountTokenKind(const xui_code_token_t* pTokens, int iCount, int iKind)
{
	int i;
	int count = 0;
	for ( i = 0; i < iCount; i++ ) {
		if ( pTokens[i].iKind == iKind ) count++;
	}
	return count;
}

int xideAppCheckLanguageRules(xide_app_t* pApp)
{
	static const char* sample =
		"#doc\n"
		"desc: language rule smoke\n"
		"#end\n"
		"class Player\n"
		"    private int hp = 0\n"
		"    get value() as int\n"
		"        return self.hp\n"
		"    end\n"
		"\n"
		"    func add(int n) as int\n"
		"        if n > 0 then\n"
		"            return self.hp + n\n"
		"        end\n"
		"        return self.hp\n"
		"    end\n"
		"end\n"
		"\n"
		"/*\n"
		"    block comment\n"
		"*/\n"
		"func main(...) as int\n"
		"    print(\"hello\")\n"
		"    return 0\n"
		"end\n";
	xui_code_language_t lang;
	xui_code_token_t tokens[512];
	xui_code_fold_range_t folds[64];
	int tokenCount = 0;
	int foldCount = 0;
	int ret;
	char error[256];
	if ( pApp == NULL || pApp->pEditor == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	ret = xuiCodeEditSetText(pApp->pEditor, sample);
	if ( ret != XUI_OK ) return ret;
	memset(&lang, 0, sizeof(lang));
	ret = xuiCodeLanguageRegistryFind(xuiCodeEditGetLanguageRegistry(pApp->pEditor), "xlang", &lang);
	if ( ret != XUI_OK ) return ret;
	memset(error, 0, sizeof(error));
	ret = xuiCodeLanguageLex(&lang, xuiCodeEditGetDocument(pApp->pEditor), 0, -1, tokens, (int)(sizeof(tokens) / sizeof(tokens[0])), &tokenCount, error, (int)sizeof(error));
	if ( ret != XUI_OK ) return ret;
	ret = xuiCodeLanguageFold(&lang, xuiCodeEditGetDocument(pApp->pEditor), folds, (int)(sizeof(folds) / sizeof(folds[0])), &foldCount);
	if ( ret != XUI_OK ) return ret;
	ret = xuiCodeFoldStateBuildFromProvider(xuiCodeEditGetFoldState(pApp->pEditor), xuiCodeEditGetDocument(pApp->pEditor), lang.onFold, lang.pUser);
	if ( ret != XUI_OK ) return ret;
	if ( xideAppCountTokenKind(tokens, tokenCount, XUI_CODE_TOKEN_KEYWORD) < 10 ) return XUI_ERROR;
	if ( xideAppCountTokenKind(tokens, tokenCount, XUI_CODE_TOKEN_TYPE) < 4 ) return XUI_ERROR;
	if ( xideAppCountTokenKind(tokens, tokenCount, XUI_CODE_TOKEN_PREPROCESSOR) < 2 ) return XUI_ERROR;
	if ( xideAppCountTokenKind(tokens, tokenCount, XUI_CODE_TOKEN_COMMENT) < 1 ) return XUI_ERROR;
	if ( foldCount < 6 ) return XUI_ERROR;
	printf("xlang_ide language-rules tokens=%d folds=%d\n", tokenCount, foldCount);
	return XUI_OK;
}

int xideAppCheckWorkspace(xide_app_t* pApp)
{
	int modelCount;
	int treeCount;
	int visibleCount;
	const xui_tree_view_node_t* node;
	if ( pApp == NULL || pApp->pProjectTree == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	modelCount = xideWorkspaceGetNodeCount(&pApp->tWorkspace);
	treeCount = xuiTreeViewGetNodeCount(pApp->pProjectTree);
	visibleCount = xuiTreeViewGetVisibleCount(pApp->pProjectTree);
	node = xuiTreeViewGetNodeById(pApp->pProjectTree, pApp->tWorkspace.iNodeId);
	if ( modelCount <= 0 || treeCount != modelCount || visibleCount <= 0 || node == NULL ) return XUI_ERROR;
	printf("xlang_ide workspace nodes=%d visible=%d projects=%d files=%d\n",
		treeCount,
		visibleCount,
		pApp->tWorkspace.iProjectCount,
		pApp->tWorkspace.iProjectCount > 0 ? pApp->tWorkspace.arrProjects[0].iFileCount : 0);
	return XUI_OK;
}

int xideAppFrame(void* pUser)
{
	xide_app_t* pApp = (xide_app_t*)pUser;
	xui_rect_i_t fullRect;
	xui_rect_t rect;
	int ret;
	if ( pApp == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	ret = xgeBegin();
	if ( ret != XGE_OK ) return ret;
	ret = xideSyncViewport(pApp);
	if ( ret != XUI_OK ) return ret;
	ret = xidePlatformHandleInput(pApp);
	if ( ret != XUI_OK ) return ret;
	ret = xuiDispatchPendingEvents(pApp->pContext);
	if ( ret != XUI_OK ) return ret;
	xideMainWindowLayout(pApp);
	ret = xuiLayout(pApp->pContext);
	if ( ret != XUI_OK ) return ret;
	ret = xuiUpdate(pApp->pContext, xgeGetDelta());
	if ( ret != XUI_OK ) return ret;
	if ( pApp->pOutputTerminal != NULL ) (void)xuiTerminalFlush(pApp->pOutputTerminal);
	ret = pApp->tProxy.surfaceClear(&pApp->tProxy, pApp->pTarget, XUI_COLOR_RGBA(238, 242, 247, 255));
	if ( ret != XUI_OK ) return ret;
	fullRect = (xui_rect_i_t){0, 0, pApp->iViewportW, pApp->iViewportH};
	ret = xuiRender(pApp->pContext, pApp->pTarget, &fullRect, 1);
	if ( ret != XUI_OK ) return ret;
	xgeClear(XUI_COLOR_RGBA(18, 23, 32, 255));
	rect = (xui_rect_t){0.0f, 0.0f, (float)pApp->iViewportW, (float)pApp->iViewportH};
	ret = pApp->tProxy.surfaceDraw(&pApp->tProxy, pApp->pTarget, rect, rect, XUI_COLOR_WHITE, XUI_SURFACE_DRAW_SCREEN_SPACE);
	if ( ret == XGE_OK ) ret = xgeEnd();
	if ( ret != XGE_OK ) return ret;
	pApp->iFrame++;
	if ( (pApp->iMaxFrames > 0 && pApp->iFrame >= pApp->iMaxFrames) ||
	     (pApp->fMaxSeconds > 0.0 && xgeTimer() >= pApp->fMaxSeconds) ) {
		printf("xlang_ide skeleton-summary frames=%d dockWindows=%d panes=%d\n",
			pApp->iFrame,
			pApp->pDockPanel != NULL ? xuiDockPanelGetWindowCount(pApp->pDockPanel) : 0,
			pApp->pDockPanel != NULL ? xuiDockPanelGetPaneCount(pApp->pDockPanel) : 0);
		xgeQuit();
	}
	return XGE_OK;
}
