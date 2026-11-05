/* Compile and execute this exact source with either the real Windows SDK or
 * the private TinyCC ABI. No COM/OS operation is executed. */
#define WIN32_LEAN_AND_MEAN 1
#define CINTERFACE
#define COBJMACROS
#define CONST_VTABLE
#if defined(XL6_COM_ABI_INTERNAL)
#include "src/runtime/xl6_native_com_win_abi.h"
#else
#include <windows.h>
#include <oleidl.h>
#endif
#include <stddef.h>
#include <stdio.h>
#define CHECK(T,F,Expected) _Static_assert(_Generic(((T*)0)->F,Expected:1,default:0),#T "." #F " exact type")
#define METHOD(I,F,N,R,...) typedef R (STDMETHODCALLTYPE* I##_##F##_Expected)(I*,__VA_ARGS__); \
 CHECK(I##Vtbl,F,I##_##F##_Expected);_Static_assert(offsetof(I##Vtbl,F)==(N)*sizeof(void(*)(void)),#I "." #F " slot")
#define METHOD0(I,F,N,R) typedef R (STDMETHODCALLTYPE* I##_##F##_Expected)(I*); \
 CHECK(I##Vtbl,F,I##_##F##_Expected);_Static_assert(offsetof(I##Vtbl,F)==(N)*sizeof(void(*)(void)),#I "." #F " slot")
#define UNKNOWN(I) METHOD(I,QueryInterface,0,HRESULT,REFIID,void**);METHOD0(I,AddRef,1,ULONG);METHOD0(I,Release,2,ULONG);CHECK(I,lpVtbl,const I##Vtbl*)
UNKNOWN(IUnknown);UNKNOWN(IDataObject);UNKNOWN(IDropSource);UNKNOWN(IDropTarget);UNKNOWN(IEnumFORMATETC);UNKNOWN(IEnumSTATDATA);
METHOD(IDataObject,GetData,3,HRESULT,FORMATETC*,STGMEDIUM*);
METHOD(IDataObject,GetDataHere,4,HRESULT,FORMATETC*,STGMEDIUM*);
METHOD(IDataObject,QueryGetData,5,HRESULT,FORMATETC*);
METHOD(IDataObject,GetCanonicalFormatEtc,6,HRESULT,FORMATETC*,FORMATETC*);
METHOD(IDataObject,SetData,7,HRESULT,FORMATETC*,STGMEDIUM*,BOOL);
METHOD(IDataObject,EnumFormatEtc,8,HRESULT,DWORD,IEnumFORMATETC**);
METHOD(IDataObject,DAdvise,9,HRESULT,FORMATETC*,DWORD,IAdviseSink*,DWORD*);
METHOD(IDataObject,DUnadvise,10,HRESULT,DWORD);
METHOD(IDataObject,EnumDAdvise,11,HRESULT,IEnumSTATDATA**);
METHOD(IDropSource,QueryContinueDrag,3,HRESULT,BOOL,DWORD);
METHOD(IDropSource,GiveFeedback,4,HRESULT,DWORD);
METHOD(IDropTarget,DragEnter,3,HRESULT,IDataObject*,DWORD,POINTL,DWORD*);
METHOD(IDropTarget,DragOver,4,HRESULT,DWORD,POINTL,DWORD*);
METHOD0(IDropTarget,DragLeave,5,HRESULT);
METHOD(IDropTarget,Drop,6,HRESULT,IDataObject*,DWORD,POINTL,DWORD*);
#define ENUM(I,T) METHOD(I,Next,3,HRESULT,ULONG,T*,ULONG*);METHOD(I,Skip,4,HRESULT,ULONG);METHOD0(I,Reset,5,HRESULT);METHOD(I,Clone,6,HRESULT,I**)
ENUM(IEnumFORMATETC,FORMATETC);ENUM(IEnumSTATDATA,STATDATA);
CHECK(FORMATETC,cfFormat,CLIPFORMAT);CHECK(FORMATETC,ptd,DVTARGETDEVICE*);CHECK(FORMATETC,dwAspect,DWORD);CHECK(FORMATETC,lindex,LONG);CHECK(FORMATETC,tymed,DWORD);
CHECK(DVTARGETDEVICE,tdSize,DWORD);CHECK(DVTARGETDEVICE,tdDriverNameOffset,WORD);CHECK(DVTARGETDEVICE,tdDeviceNameOffset,WORD);CHECK(DVTARGETDEVICE,tdPortNameOffset,WORD);CHECK(DVTARGETDEVICE,tdExtDevmodeOffset,WORD);
CHECK(STGMEDIUM,tymed,DWORD);CHECK(STGMEDIUM,hBitmap,HBITMAP);CHECK(STGMEDIUM,hMetaFilePict,HMETAFILEPICT);CHECK(STGMEDIUM,hEnhMetaFile,HENHMETAFILE);CHECK(STGMEDIUM,hGlobal,HGLOBAL);CHECK(STGMEDIUM,lpszFileName,LPOLESTR);CHECK(STGMEDIUM,pstm,IStream*);CHECK(STGMEDIUM,pstg,IStorage*);CHECK(STGMEDIUM,pUnkForRelease,IUnknown*);
CHECK(STATDATA,formatetc,FORMATETC);CHECK(STATDATA,advf,DWORD);CHECK(STATDATA,pAdvSink,IAdviseSink*);CHECK(STATDATA,dwConnection,DWORD);
typedef void (WINAPI* ReleaseExpected)(LPSTGMEDIUM);
_Static_assert(_Generic(&ReleaseStgMedium,ReleaseExpected:1,default:0),"ReleaseStgMedium exact import ABI");
_Static_assert(sizeof(void*)==8,"Win64 qualification only");
#define TYPE(T) printf("type " #T " %llu %llu\n",(unsigned long long)sizeof(T),(unsigned long long)_Alignof(T))
#define FIELD(T,F) printf("field " #T "." #F " %llu\n",(unsigned long long)offsetof(T,F))
#define IFACE(I) TYPE(I);TYPE(I##Vtbl);FIELD(I,lpVtbl);FIELD(I##Vtbl,QueryInterface);FIELD(I##Vtbl,AddRef);FIELD(I##Vtbl,Release)
int main(void)
{
    TYPE(HRESULT);TYPE(ULONG);TYPE(GUID);TYPE(POINTL);TYPE(CLIPFORMAT);
    TYPE(FORMATETC);FIELD(FORMATETC,cfFormat);FIELD(FORMATETC,ptd);FIELD(FORMATETC,dwAspect);FIELD(FORMATETC,lindex);FIELD(FORMATETC,tymed);
    TYPE(DVTARGETDEVICE);FIELD(DVTARGETDEVICE,tdSize);FIELD(DVTARGETDEVICE,tdDriverNameOffset);FIELD(DVTARGETDEVICE,tdDeviceNameOffset);FIELD(DVTARGETDEVICE,tdPortNameOffset);FIELD(DVTARGETDEVICE,tdExtDevmodeOffset);FIELD(DVTARGETDEVICE,tdData);
    TYPE(STGMEDIUM);FIELD(STGMEDIUM,tymed);FIELD(STGMEDIUM,hBitmap);FIELD(STGMEDIUM,hMetaFilePict);FIELD(STGMEDIUM,hEnhMetaFile);FIELD(STGMEDIUM,hGlobal);FIELD(STGMEDIUM,lpszFileName);FIELD(STGMEDIUM,pstm);FIELD(STGMEDIUM,pstg);FIELD(STGMEDIUM,pUnkForRelease);
    TYPE(STATDATA);FIELD(STATDATA,formatetc);FIELD(STATDATA,advf);FIELD(STATDATA,pAdvSink);FIELD(STATDATA,dwConnection);
    IFACE(IUnknown);IFACE(IDataObject);IFACE(IDropSource);IFACE(IDropTarget);IFACE(IEnumFORMATETC);IFACE(IEnumSTATDATA);
    FIELD(IDataObjectVtbl,GetData);FIELD(IDataObjectVtbl,GetDataHere);FIELD(IDataObjectVtbl,QueryGetData);FIELD(IDataObjectVtbl,GetCanonicalFormatEtc);FIELD(IDataObjectVtbl,SetData);FIELD(IDataObjectVtbl,EnumFormatEtc);FIELD(IDataObjectVtbl,DAdvise);FIELD(IDataObjectVtbl,DUnadvise);FIELD(IDataObjectVtbl,EnumDAdvise);
    FIELD(IDropSourceVtbl,QueryContinueDrag);FIELD(IDropSourceVtbl,GiveFeedback);FIELD(IDropTargetVtbl,DragEnter);FIELD(IDropTargetVtbl,DragOver);FIELD(IDropTargetVtbl,DragLeave);FIELD(IDropTargetVtbl,Drop);
    FIELD(IEnumFORMATETCVtbl,Next);FIELD(IEnumFORMATETCVtbl,Skip);FIELD(IEnumFORMATETCVtbl,Reset);FIELD(IEnumFORMATETCVtbl,Clone);FIELD(IEnumSTATDATAVtbl,Next);FIELD(IEnumSTATDATAVtbl,Skip);FIELD(IEnumSTATDATAVtbl,Reset);FIELD(IEnumSTATDATAVtbl,Clone);
    return 0;
}
