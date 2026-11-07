#if !defined(XGE_NO_TEXT)

#include <string.h>
#include <math.h>

static int __xgeSystemFontValidate(xge_font font,const xge_system_font_desc_t* input,xge_system_font_desc_t* output)
{
	if(font==NULL || output==NULL || font->iRefCount>0)return XGE_ERROR_INVALID_ARGUMENT;
	memset(output,0,sizeof(*output));
	output->iSize=sizeof(*output);
	output->iRole=XGE_SYSTEM_FONT_UI;
	if(input!=NULL){
		if(input->iSize<sizeof(*input) || input->iFlags!=0)return XGE_ERROR_INVALID_ARGUMENT;
		*output=*input;
	}
	if(output->iRole<XGE_SYSTEM_FONT_UI || output->iRole>XGE_SYSTEM_FONT_MONOSPACE ||
	   !isfinite(output->fPixelSize) || output->fPixelSize<0.0f ||
	   (output->iWeight!=0 && (output->iWeight<1 || output->iWeight>1000)) ||
	   output->iSlant<XGE_FONT_SLANT_NORMAL || output->iSlant>XGE_FONT_SLANT_OBLIQUE)
		return XGE_ERROR_INVALID_ARGUMENT;
	return XGE_OK;
}

static int __xgeSystemFontLoadPath(xge_font font,const char* path,int faceIndex,float size)
{
	xge_font_face face=NULL;
	xge_font_face_desc_t faceDesc={0};
	xge_font_instance_desc_t instanceDesc={0};
	int status;
	if(!path || !path[0])return XGE_ERROR_RESOURCE_FAILED;
	if(size<=0.0f)size=14.0f;
	faceDesc.iSize=sizeof(faceDesc);
	faceDesc.iFaceIndex=faceIndex;
	status=xgeFontFaceLoad(&face,path,&faceDesc);
	if(status!=XGE_OK)return status;
	instanceDesc.iSize=sizeof(instanceDesc);
	instanceDesc.fPixelSize=size;
	status=xgeFontCreate(font,face,&instanceDesc);
	xgeFontFaceFree(face);
	return status;
}

#if defined(_WIN32) || defined(_WIN64)

#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <dwrite.h>

static const IID __xgeIIDWriteFactory={0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};
static const IID __xgeIIDWriteLocalFontFileLoader={0xb2d9f3ec,0xc9fe,0x4a11,{0xa2,0xec,0xd8,0x62,0x08,0xf7,0xc0,0xa2}};

static int __xgeSystemFontWin32Family(IDWriteFontCollection* collection,const WCHAR* requested,IDWriteFontFamily** family)
{
	static const WCHAR* fallbacks[]={L"Segoe UI",L"Arial",L"Microsoft Sans Serif"};
	UINT32 index=0;
	WINBOOL exists=FALSE;
	HRESULT hr;
	int i;
	*family=NULL;
	if(requested && requested[0]){
		hr=IDWriteFontCollection_FindFamilyName(collection,requested,&index,&exists);
		if(SUCCEEDED(hr) && exists)return SUCCEEDED(IDWriteFontCollection_GetFontFamily(collection,index,family)) ? XGE_OK:XGE_ERROR_RESOURCE_FAILED;
	}
	for(i=0;i<(int)(sizeof(fallbacks)/sizeof(fallbacks[0]));++i){
		hr=IDWriteFontCollection_FindFamilyName(collection,fallbacks[i],&index,&exists);
		if(SUCCEEDED(hr) && exists)return SUCCEEDED(IDWriteFontCollection_GetFontFamily(collection,index,family)) ? XGE_OK:XGE_ERROR_RESOURCE_FAILED;
	}
	return IDWriteFontCollection_GetFontFamilyCount(collection)>0 &&
		SUCCEEDED(IDWriteFontCollection_GetFontFamily(collection,0,family)) ? XGE_OK:XGE_ERROR_RESOURCE_FAILED;
}

static int __xgeSystemFontWin32(xge_font font,const xge_system_font_desc_t* desc)
{
	NONCLIENTMETRICSW metrics;
	LOGFONTW logFont;
	IDWriteFactory* factory=NULL;
	IDWriteFontCollection* collection=NULL;
	IDWriteFontFamily* family=NULL;
	IDWriteFont* selected=NULL;
	IDWriteFontFace* face=NULL;
	IDWriteFontFile** files=NULL;
	IDWriteFontFileLoader* loader=NULL;
	IDWriteLocalFontFileLoader* localLoader=NULL;
	UINT32 fileCount=0,pathLength=0,keySize=0;
	const void* key=NULL;
	WCHAR* widePath=NULL;
	char* path=NULL;
	float size=desc->fPixelSize;
	int status=XGE_ERROR_RESOURCE_FAILED;
	HRESULT hr;
	UINT32 i;

	memset(&metrics,0,sizeof(metrics));
	metrics.cbSize=sizeof(metrics);
	memset(&logFont,0,sizeof(logFont));
	if(SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0))logFont=metrics.lfMessageFont;
	else{
		logFont.lfHeight=-14;
		logFont.lfWeight=FW_NORMAL;
		wcscpy(logFont.lfFaceName,L"Segoe UI");
	}
	if(desc->iRole==XGE_SYSTEM_FONT_MONOSPACE)wcscpy(logFont.lfFaceName,L"Consolas");
	if(size<=0.0f)size=(float)(logFont.lfHeight<0 ? -logFont.lfHeight:logFont.lfHeight);
	if(size<=0.0f)size=14.0f;

	hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,&__xgeIIDWriteFactory,(IUnknown**)&factory);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteFactory_GetSystemFontCollection(factory,&collection,FALSE);
	if(FAILED(hr))goto cleanup;
	status=__xgeSystemFontWin32Family(collection,logFont.lfFaceName,&family);
	if(status!=XGE_OK)goto cleanup;
	/* Family lookup succeeded; later COM failures must still report failure. */
	status=XGE_ERROR_RESOURCE_FAILED;
	hr=IDWriteFontFamily_GetFirstMatchingFont(family,
		(DWRITE_FONT_WEIGHT)(desc->iWeight ? desc->iWeight:(logFont.lfWeight ? logFont.lfWeight:FW_NORMAL)),
		DWRITE_FONT_STRETCH_NORMAL,
		desc->iSlant==XGE_FONT_SLANT_ITALIC ? DWRITE_FONT_STYLE_ITALIC:
		desc->iSlant==XGE_FONT_SLANT_OBLIQUE ? DWRITE_FONT_STYLE_OBLIQUE:
		logFont.lfItalic ? DWRITE_FONT_STYLE_ITALIC:DWRITE_FONT_STYLE_NORMAL,&selected);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteFont_CreateFontFace(selected,&face);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteFontFace_GetFiles(face,&fileCount,NULL);
	if(FAILED(hr) || fileCount==0)goto cleanup;
	files=(IDWriteFontFile**)xrtCalloc(fileCount,sizeof(*files));
	if(!files){status=XGE_ERROR_OUT_OF_MEMORY;goto cleanup;}
	hr=IDWriteFontFace_GetFiles(face,&fileCount,files);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteFontFile_GetLoader(files[0],&loader);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteFontFileLoader_QueryInterface(loader,&__xgeIIDWriteLocalFontFileLoader,(void**)&localLoader);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteFontFile_GetReferenceKey(files[0],&key,&keySize);
	if(FAILED(hr))goto cleanup;
	hr=IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(localLoader,key,keySize,&pathLength);
	if(FAILED(hr))goto cleanup;
	widePath=(WCHAR*)xrtMalloc(((size_t)pathLength+1u)*sizeof(*widePath));
	if(!widePath){status=XGE_ERROR_OUT_OF_MEMORY;goto cleanup;}
	hr=IDWriteLocalFontFileLoader_GetFilePathFromKey(localLoader,key,keySize,widePath,pathLength+1u);
	if(FAILED(hr))goto cleanup;
	i=WideCharToMultiByte(CP_UTF8,0,widePath,-1,NULL,0,NULL,NULL);
	if(i==0)goto cleanup;
	path=(char*)xrtMalloc(i);
	if(!path){status=XGE_ERROR_OUT_OF_MEMORY;goto cleanup;}
	if(WideCharToMultiByte(CP_UTF8,0,widePath,-1,path,(int)i,NULL,NULL)==0)goto cleanup;
	status=__xgeSystemFontLoadPath(font,path,(int)IDWriteFontFace_GetIndex(face),size);

cleanup:
	xrtFree(path);
	xrtFree(widePath);
	if(localLoader)IDWriteLocalFontFileLoader_Release(localLoader);
	if(loader)IDWriteFontFileLoader_Release(loader);
	if(files){for(i=0;i<fileCount;++i)if(files[i])IDWriteFontFile_Release(files[i]);xrtFree(files);}
	if(face)IDWriteFontFace_Release(face);
	if(selected)IDWriteFont_Release(selected);
	if(family)IDWriteFontFamily_Release(family);
	if(collection)IDWriteFontCollection_Release(collection);
	if(factory)IDWriteFactory_Release(factory);
	return status;
}

#elif defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)

#include <dlfcn.h>

typedef unsigned char FcChar8;
typedef int FcBool;
typedef int FcResult;
typedef struct _FcPattern FcPattern;

static int __xgeSystemFontFcWeight(int weight)
{
	if(weight<=100)return 0;
	if(weight<=200)return 40;
	if(weight<=300)return 50;
	if(weight<=400)return 80;
	if(weight<=500)return 100;
	if(weight<=600)return 180;
	if(weight<=700)return 200;
	if(weight<=800)return 205;
	return 210;
}

static int __xgeSystemFontLinux(xge_font font,const xge_system_font_desc_t* desc)
{
	void* library=dlopen("libfontconfig.so.1",RTLD_LAZY|RTLD_LOCAL);
	FcPattern* pattern=NULL;
	FcPattern* match=NULL;
	FcChar8* file=NULL;
	FcResult result=0;
	int index=0,status=XGE_ERROR_UNSUPPORTED;
	double pixelSize=0.0;
	FcPattern* (*nameParse)(const FcChar8*);
	FcBool (*configSubstitute)(void*,FcPattern*,int);
	void (*defaultSubstitute)(FcPattern*);
	FcPattern* (*fontMatch)(void*,FcPattern*,FcResult*);
	FcResult (*getString)(const FcPattern*,const char*,int,FcChar8**);
	FcResult (*getInteger)(const FcPattern*,const char*,int,int*);
	FcResult (*getDouble)(const FcPattern*,const char*,int,double*);
	FcBool (*addString)(FcPattern*,const char*,const FcChar8*);
	FcBool (*addInteger)(FcPattern*,const char*,int);
	void (*patternDestroy)(FcPattern*);
	const char* family=desc->iRole==XGE_SYSTEM_FONT_MONOSPACE ? "monospace":"sans-serif";
	if(!library)return XGE_ERROR_UNSUPPORTED;
#define XGE_FC_LOAD(target,name) do{*(void**)(&target)=dlsym(library,name);if(!(target))goto cleanup;}while(0)
	XGE_FC_LOAD(nameParse,"FcNameParse");
	XGE_FC_LOAD(configSubstitute,"FcConfigSubstitute");
	XGE_FC_LOAD(defaultSubstitute,"FcDefaultSubstitute");
	XGE_FC_LOAD(fontMatch,"FcFontMatch");
	XGE_FC_LOAD(getString,"FcPatternGetString");
	XGE_FC_LOAD(getInteger,"FcPatternGetInteger");
	XGE_FC_LOAD(getDouble,"FcPatternGetDouble");
	XGE_FC_LOAD(addString,"FcPatternAddString");
	XGE_FC_LOAD(addInteger,"FcPatternAddInteger");
	XGE_FC_LOAD(patternDestroy,"FcPatternDestroy");
#undef XGE_FC_LOAD
	pattern=nameParse((const FcChar8*)family);
	if(!pattern){status=XGE_ERROR_OUT_OF_MEMORY;goto cleanup;}
	if(desc->sLanguage && desc->sLanguage[0])addString(pattern,"lang",(const FcChar8*)desc->sLanguage);
	if(desc->iWeight)addInteger(pattern,"weight",__xgeSystemFontFcWeight(desc->iWeight));
	if(desc->iSlant==XGE_FONT_SLANT_ITALIC)addInteger(pattern,"slant",100);
	else if(desc->iSlant==XGE_FONT_SLANT_OBLIQUE)addInteger(pattern,"slant",110);
	if(!configSubstitute(NULL,pattern,0)){status=XGE_ERROR_RESOURCE_FAILED;goto cleanup;}
	defaultSubstitute(pattern);
	match=fontMatch(NULL,pattern,&result);
	if(!match){status=XGE_ERROR_RESOURCE_FAILED;goto cleanup;}
	if(getString(match,"file",0,&file)!=0 || !file){status=XGE_ERROR_RESOURCE_FAILED;goto cleanup;}
	(void)getInteger(match,"index",0,&index);
	(void)getDouble(match,"pixelsize",0,&pixelSize);
	status=__xgeSystemFontLoadPath(font,(const char*)file,index,desc->fPixelSize>0.0f ? desc->fPixelSize:(pixelSize>0.0 ? (float)pixelSize:14.0f));
cleanup:
	if(match && patternDestroy)patternDestroy(match);
	if(pattern && patternDestroy)patternDestroy(pattern);
	dlclose(library);
	return status;
}

#elif defined(__ANDROID__)

#include <dlfcn.h>
#include <stdbool.h>

typedef struct AFontMatcher AFontMatcher;
typedef struct AFont AFont;

static int __xgeSystemFontAndroid(xge_font font,const xge_system_font_desc_t* desc)
{
	void* library=dlopen("libandroid.so",RTLD_LAZY|RTLD_LOCAL);
	AFontMatcher* matcher=NULL;
	AFont* selected=NULL;
	uint16_t sample=0x0041u;
	uint32_t runLength=0;
	int status=XGE_ERROR_UNSUPPORTED;
	AFontMatcher* (*matcherCreate)(void);
	void (*matcherDestroy)(AFontMatcher*);
	void (*matcherSetLocales)(AFontMatcher*,const char*);
	void (*matcherSetStyle)(AFontMatcher*,uint16_t,bool);
	AFont* (*matcherMatch)(const AFontMatcher*,const char*,const uint16_t*,uint32_t,uint32_t*);
	const char* (*fontPath)(const AFont*);
	size_t (*fontIndex)(const AFont*);
	void (*fontClose)(AFont*);
	const char* family=desc->iRole==XGE_SYSTEM_FONT_MONOSPACE ? "monospace":"sans-serif";
	if(!library)return XGE_ERROR_UNSUPPORTED;
#define XGE_AF_LOAD(target,name) do{*(void**)(&target)=dlsym(library,name);if(!(target))goto cleanup;}while(0)
	XGE_AF_LOAD(matcherCreate,"AFontMatcher_create");
	XGE_AF_LOAD(matcherDestroy,"AFontMatcher_destroy");
	XGE_AF_LOAD(matcherSetLocales,"AFontMatcher_setLocales");
	XGE_AF_LOAD(matcherSetStyle,"AFontMatcher_setStyle");
	XGE_AF_LOAD(matcherMatch,"AFontMatcher_match");
	XGE_AF_LOAD(fontPath,"AFont_getFontFilePath");
	XGE_AF_LOAD(fontIndex,"AFont_getCollectionIndex");
	XGE_AF_LOAD(fontClose,"AFont_close");
#undef XGE_AF_LOAD
	matcher=matcherCreate();
	if(!matcher){status=XGE_ERROR_RESOURCE_FAILED;goto cleanup;}
	if(desc->sLanguage && desc->sLanguage[0])matcherSetLocales(matcher,desc->sLanguage);
	matcherSetStyle(matcher,(uint16_t)(desc->iWeight ? desc->iWeight:XGE_FONT_WEIGHT_NORMAL),desc->iSlant!=XGE_FONT_SLANT_NORMAL);
	selected=matcherMatch(matcher,family,&sample,1,&runLength);
	if(!selected){status=XGE_ERROR_RESOURCE_FAILED;goto cleanup;}
	status=__xgeSystemFontLoadPath(font,fontPath(selected),(int)fontIndex(selected),desc->fPixelSize>0.0f ? desc->fPixelSize:14.0f);
cleanup:
	if(selected && fontClose)fontClose(selected);
	if(matcher && matcherDestroy)matcherDestroy(matcher);
	dlclose(library);
	return status;
}

#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

static int __xgeSystemFontAppleNameEquals(const unsigned char* name,int length,int utf16,const char* expected)
{
	int i=0,j=0;
	if(!name || !expected)return 0;
	if(utf16){
		if((length&1)!=0)return 0;
		while(i<length && expected[j]){
			if(name[i]!=0 || name[i+1]!=(unsigned char)expected[j])return 0;
			i+=2;j++;
		}
		return i==length && expected[j]=='\0';
	}
	return (int)strlen(expected)==length && memcmp(name,expected,(size_t)length)==0;
}

static int __xgeSystemFontAppleFaceIndex(const char* path,const char* postScriptName)
{
	xge_resource_t resource={0};
	stbtt_fontinfo info;
	const unsigned char* name;
	int count,index,length,status;
	if(!postScriptName || !postScriptName[0])return 0;
	status=xgeResourceLoad(path,&resource);
	if(status!=XGE_OK)return 0;
	count=stbtt_GetNumberOfFonts((const unsigned char*)resource.pData);
	for(index=0;index<count;++index){
		int offset=stbtt_GetFontOffsetForIndex((const unsigned char*)resource.pData,index);
		if(offset<0 || !stbtt_InitFont(&info,(const unsigned char*)resource.pData,offset))continue;
		name=stbtt_GetFontNameString(&info,&length,3,1,0x0409,6);
		if(__xgeSystemFontAppleNameEquals(name,length,1,postScriptName))break;
		name=stbtt_GetFontNameString(&info,&length,3,10,0x0409,6);
		if(__xgeSystemFontAppleNameEquals(name,length,1,postScriptName))break;
		name=stbtt_GetFontNameString(&info,&length,1,0,0,6);
		if(__xgeSystemFontAppleNameEquals(name,length,0,postScriptName))break;
	}
	xgeResourceFree(&resource);
	return index<count ? index:0;
}

static int __xgeSystemFontApple(xge_font font,const xge_system_font_desc_t* desc)
{
	CTFontUIFontType role=kCTFontUIFontSystem;
	CFStringRef language=NULL;
	CTFontRef systemFont=NULL;
	CFTypeRef value=NULL;
	char path[4096];
	char postScriptName[256]={0};
	float size=desc->fPixelSize;
	int faceIndex=0,status=XGE_ERROR_RESOURCE_FAILED;
	if(desc->iRole==XGE_SYSTEM_FONT_DOCUMENT)role=kCTFontUIFontUser;
	else if(desc->iRole==XGE_SYSTEM_FONT_MONOSPACE)role=kCTFontUIFontUserFixedPitch;
	if(desc->sLanguage && desc->sLanguage[0])language=CFStringCreateWithCString(NULL,desc->sLanguage,kCFStringEncodingUTF8);
	systemFont=CTFontCreateUIFontForLanguage(role,size>0.0f ? size:0.0f,language);
	if(!systemFont)goto cleanup;
	if(size<=0.0f)size=(float)CTFontGetSize(systemFont);
	value=CTFontCopyAttribute(systemFont,kCTFontURLAttribute);
	if(!value || CFGetTypeID(value)!=CFURLGetTypeID() ||
	   !CFURLGetFileSystemRepresentation((CFURLRef)value,1,(UInt8*)path,sizeof(path)))goto cleanup;
	{
		CFStringRef name=CTFontCopyPostScriptName(systemFont);
		if(name){
			(void)CFStringGetCString(name,postScriptName,sizeof(postScriptName),kCFStringEncodingUTF8);
			CFRelease(name);
		}
	}
	faceIndex=__xgeSystemFontAppleFaceIndex(path,postScriptName);
	status=__xgeSystemFontLoadPath(font,path,faceIndex,size);
cleanup:
	if(value)CFRelease(value);
	if(systemFont)CFRelease(systemFont);
	if(language)CFRelease(language);
	return status;
}

#endif

int xgeFontLoadSystem(xge_font font,const xge_system_font_desc_t* input)
{
	xge_system_font_desc_t desc;
	int status=__xgeSystemFontValidate(font,input,&desc);
	if(status!=XGE_OK)return status;
#if defined(_WIN32) || defined(_WIN64)
	return __xgeSystemFontWin32(font,&desc);
#elif defined(__linux__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
	return __xgeSystemFontLinux(font,&desc);
#elif defined(__ANDROID__)
	return __xgeSystemFontAndroid(font,&desc);
#elif defined(__APPLE__)
	return __xgeSystemFontApple(font,&desc);
#else
	(void)desc;
	return XGE_ERROR_UNSUPPORTED;
#endif
}

#else

int xgeFontLoadSystem(xge_font font,const xge_system_font_desc_t* desc)
{(void)font;(void)desc;return XGE_ERROR_UNSUPPORTED;}

#endif
