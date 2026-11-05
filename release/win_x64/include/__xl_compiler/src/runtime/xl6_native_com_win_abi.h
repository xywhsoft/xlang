#ifndef XL6_NATIVE_COM_WIN_ABI_H
#define XL6_NATIVE_COM_WIN_ABI_H
/* Private C ABI for the resident transfer capsule when TinyCC's lean WinSDK
 * has no objidl/oleidl headers. This is not a general COM SDK. Exact slots and
 * records are checked against the system compiler's Windows SDK oracle. */
#if !defined(_WIN32) || defined(__cplusplus)
#error This private COM ABI is Windows C only
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <guiddef.h>

typedef struct IUnknown IUnknown;
typedef struct IDataObject IDataObject;
typedef struct IDropSource IDropSource;
typedef struct IDropTarget IDropTarget;
typedef struct IEnumFORMATETC IEnumFORMATETC;
typedef struct IEnumSTATDATA IEnumSTATDATA;
typedef struct IAdviseSink IAdviseSink;
typedef struct IStream IStream;
typedef struct IStorage IStorage;
typedef WORD CLIPFORMAT;
typedef HANDLE HMETAFILEPICT;
typedef WCHAR OLECHAR;
typedef OLECHAR* LPOLESTR;

#pragma pack(push,8)
typedef struct tagDVTARGETDEVICE {
    DWORD tdSize;
    WORD tdDriverNameOffset,tdDeviceNameOffset,tdPortNameOffset,tdExtDevmodeOffset;
    BYTE tdData[1];
} DVTARGETDEVICE;
typedef struct tagFORMATETC {
    CLIPFORMAT cfFormat;
    DVTARGETDEVICE* ptd;
    DWORD dwAspect;
    LONG lindex;
    DWORD tymed;
} FORMATETC;
typedef FORMATETC* LPFORMATETC;
typedef struct tagSTGMEDIUM {
    DWORD tymed;
    union {
        HBITMAP hBitmap;
        HMETAFILEPICT hMetaFilePict;
        HENHMETAFILE hEnhMetaFile;
        HGLOBAL hGlobal;
        LPOLESTR lpszFileName;
        IStream* pstm;
        IStorage* pstg;
    };
    IUnknown* pUnkForRelease;
} STGMEDIUM;
typedef STGMEDIUM* LPSTGMEDIUM;
typedef struct tagSTATDATA {
    FORMATETC formatetc;
    DWORD advf;
    IAdviseSink* pAdvSink;
    DWORD dwConnection;
} STATDATA;

typedef struct IUnknownVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IUnknown*,REFIID,void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(IUnknown*);
    ULONG (STDMETHODCALLTYPE* Release)(IUnknown*);
} IUnknownVtbl;
typedef struct IDataObjectVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDataObject*,REFIID,void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(IDataObject*);
    ULONG (STDMETHODCALLTYPE* Release)(IDataObject*);
    HRESULT (STDMETHODCALLTYPE* GetData)(IDataObject*,FORMATETC*,STGMEDIUM*);
    HRESULT (STDMETHODCALLTYPE* GetDataHere)(IDataObject*,FORMATETC*,STGMEDIUM*);
    HRESULT (STDMETHODCALLTYPE* QueryGetData)(IDataObject*,FORMATETC*);
    HRESULT (STDMETHODCALLTYPE* GetCanonicalFormatEtc)(IDataObject*,FORMATETC*,FORMATETC*);
    HRESULT (STDMETHODCALLTYPE* SetData)(IDataObject*,FORMATETC*,STGMEDIUM*,BOOL);
    HRESULT (STDMETHODCALLTYPE* EnumFormatEtc)(IDataObject*,DWORD,IEnumFORMATETC**);
    HRESULT (STDMETHODCALLTYPE* DAdvise)(IDataObject*,FORMATETC*,DWORD,IAdviseSink*,DWORD*);
    HRESULT (STDMETHODCALLTYPE* DUnadvise)(IDataObject*,DWORD);
    HRESULT (STDMETHODCALLTYPE* EnumDAdvise)(IDataObject*,IEnumSTATDATA**);
} IDataObjectVtbl;
typedef struct IDropSourceVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDropSource*,REFIID,void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(IDropSource*);
    ULONG (STDMETHODCALLTYPE* Release)(IDropSource*);
    HRESULT (STDMETHODCALLTYPE* QueryContinueDrag)(IDropSource*,BOOL,DWORD);
    HRESULT (STDMETHODCALLTYPE* GiveFeedback)(IDropSource*,DWORD);
} IDropSourceVtbl;
typedef struct IDropTargetVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDropTarget*,REFIID,void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(IDropTarget*);
    ULONG (STDMETHODCALLTYPE* Release)(IDropTarget*);
    HRESULT (STDMETHODCALLTYPE* DragEnter)(IDropTarget*,IDataObject*,DWORD,POINTL,DWORD*);
    HRESULT (STDMETHODCALLTYPE* DragOver)(IDropTarget*,DWORD,POINTL,DWORD*);
    HRESULT (STDMETHODCALLTYPE* DragLeave)(IDropTarget*);
    HRESULT (STDMETHODCALLTYPE* Drop)(IDropTarget*,IDataObject*,DWORD,POINTL,DWORD*);
} IDropTargetVtbl;
typedef struct IEnumFORMATETCVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IEnumFORMATETC*,REFIID,void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(IEnumFORMATETC*);
    ULONG (STDMETHODCALLTYPE* Release)(IEnumFORMATETC*);
    HRESULT (STDMETHODCALLTYPE* Next)(IEnumFORMATETC*,ULONG,FORMATETC*,ULONG*);
    HRESULT (STDMETHODCALLTYPE* Skip)(IEnumFORMATETC*,ULONG);
    HRESULT (STDMETHODCALLTYPE* Reset)(IEnumFORMATETC*);
    HRESULT (STDMETHODCALLTYPE* Clone)(IEnumFORMATETC*,IEnumFORMATETC**);
} IEnumFORMATETCVtbl;
typedef struct IEnumSTATDATAVtbl {
    HRESULT (STDMETHODCALLTYPE* QueryInterface)(IEnumSTATDATA*,REFIID,void**);
    ULONG (STDMETHODCALLTYPE* AddRef)(IEnumSTATDATA*);
    ULONG (STDMETHODCALLTYPE* Release)(IEnumSTATDATA*);
    HRESULT (STDMETHODCALLTYPE* Next)(IEnumSTATDATA*,ULONG,STATDATA*,ULONG*);
    HRESULT (STDMETHODCALLTYPE* Skip)(IEnumSTATDATA*,ULONG);
    HRESULT (STDMETHODCALLTYPE* Reset)(IEnumSTATDATA*);
    HRESULT (STDMETHODCALLTYPE* Clone)(IEnumSTATDATA*,IEnumSTATDATA**);
} IEnumSTATDATAVtbl;
#if defined(CONST_VTABLE)
#define XL6_COM_CONST_VTBL const
#else
#define XL6_COM_CONST_VTBL
#endif
struct IUnknown {XL6_COM_CONST_VTBL IUnknownVtbl* lpVtbl;};
struct IDataObject {XL6_COM_CONST_VTBL IDataObjectVtbl* lpVtbl;};
struct IDropSource {XL6_COM_CONST_VTBL IDropSourceVtbl* lpVtbl;};
struct IDropTarget {XL6_COM_CONST_VTBL IDropTargetVtbl* lpVtbl;};
struct IEnumFORMATETC {XL6_COM_CONST_VTBL IEnumFORMATETCVtbl* lpVtbl;};
struct IEnumSTATDATA {XL6_COM_CONST_VTBL IEnumSTATDATAVtbl* lpVtbl;};
#undef XL6_COM_CONST_VTBL
#pragma pack(pop)

__declspec(dllimport) void WINAPI ReleaseStgMedium(LPSTGMEDIUM);
#ifdef COBJMACROS
#define IUnknown_QueryInterface(s,i,o) ((s)->lpVtbl->QueryInterface((s),(i),(o)))
#define IUnknown_AddRef(s) ((s)->lpVtbl->AddRef((s)))
#define IUnknown_Release(s) ((s)->lpVtbl->Release((s)))
#define IDataObject_QueryInterface(s,i,o) ((s)->lpVtbl->QueryInterface((s),(i),(o)))
#define IDataObject_AddRef(s) ((s)->lpVtbl->AddRef((s)))
#define IDataObject_Release(s) ((s)->lpVtbl->Release((s)))
#define IDataObject_GetData(s,f,m) ((s)->lpVtbl->GetData((s),(f),(m)))
#define IDataObject_GetDataHere(s,f,m) ((s)->lpVtbl->GetDataHere((s),(f),(m)))
#define IDataObject_QueryGetData(s,f) ((s)->lpVtbl->QueryGetData((s),(f)))
#define IDataObject_GetCanonicalFormatEtc(s,i,o) ((s)->lpVtbl->GetCanonicalFormatEtc((s),(i),(o)))
#define IDataObject_SetData(s,f,m,r) ((s)->lpVtbl->SetData((s),(f),(m),(r)))
#define IDataObject_EnumFormatEtc(s,d,o) ((s)->lpVtbl->EnumFormatEtc((s),(d),(o)))
#define IDataObject_DAdvise(s,f,a,k,c) ((s)->lpVtbl->DAdvise((s),(f),(a),(k),(c)))
#define IDataObject_DUnadvise(s,c) ((s)->lpVtbl->DUnadvise((s),(c)))
#define IDataObject_EnumDAdvise(s,o) ((s)->lpVtbl->EnumDAdvise((s),(o)))
#define IDropSource_QueryInterface(s,i,o) ((s)->lpVtbl->QueryInterface((s),(i),(o)))
#define IDropSource_AddRef(s) ((s)->lpVtbl->AddRef((s)))
#define IDropSource_Release(s) ((s)->lpVtbl->Release((s)))
#define IDropSource_QueryContinueDrag(s,e,k) ((s)->lpVtbl->QueryContinueDrag((s),(e),(k)))
#define IDropSource_GiveFeedback(s,e) ((s)->lpVtbl->GiveFeedback((s),(e)))
#define IDropTarget_QueryInterface(s,i,o) ((s)->lpVtbl->QueryInterface((s),(i),(o)))
#define IDropTarget_AddRef(s) ((s)->lpVtbl->AddRef((s)))
#define IDropTarget_Release(s) ((s)->lpVtbl->Release((s)))
#define IDropTarget_DragEnter(s,d,k,p,e) ((s)->lpVtbl->DragEnter((s),(d),(k),(p),(e)))
#define IDropTarget_DragOver(s,k,p,e) ((s)->lpVtbl->DragOver((s),(k),(p),(e)))
#define IDropTarget_DragLeave(s) ((s)->lpVtbl->DragLeave((s)))
#define IDropTarget_Drop(s,d,k,p,e) ((s)->lpVtbl->Drop((s),(d),(k),(p),(e)))
#define IEnumFORMATETC_QueryInterface(s,i,o) ((s)->lpVtbl->QueryInterface((s),(i),(o)))
#define IEnumFORMATETC_AddRef(s) ((s)->lpVtbl->AddRef((s)))
#define IEnumFORMATETC_Release(s) ((s)->lpVtbl->Release((s)))
#define IEnumFORMATETC_Next(s,n,e,f) ((s)->lpVtbl->Next((s),(n),(e),(f)))
#define IEnumFORMATETC_Skip(s,n) ((s)->lpVtbl->Skip((s),(n)))
#define IEnumFORMATETC_Reset(s) ((s)->lpVtbl->Reset((s)))
#define IEnumFORMATETC_Clone(s,o) ((s)->lpVtbl->Clone((s),(o)))
#define IEnumSTATDATA_QueryInterface(s,i,o) ((s)->lpVtbl->QueryInterface((s),(i),(o)))
#define IEnumSTATDATA_AddRef(s) ((s)->lpVtbl->AddRef((s)))
#define IEnumSTATDATA_Release(s) ((s)->lpVtbl->Release((s)))
#define IEnumSTATDATA_Next(s,n,e,f) ((s)->lpVtbl->Next((s),(n),(e),(f)))
#define IEnumSTATDATA_Skip(s,n) ((s)->lpVtbl->Skip((s),(n)))
#define IEnumSTATDATA_Reset(s) ((s)->lpVtbl->Reset((s)))
#define IEnumSTATDATA_Clone(s,o) ((s)->lpVtbl->Clone((s),(o)))
#endif
#endif
