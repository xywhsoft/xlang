/*
 * Windows SDK compatibility declarations used by the TSF backend.
 *
 * Some older MinGW SDKs expose ITfUIElementMgr but omit the candidate-list
 * interfaces added to later Windows SDK headers. Keep the declarations local
 * to XGE so the public ABI and non-Windows builds remain unaffected.
 */
#ifndef XGE_IME_WIN32_COMPAT_H
#define XGE_IME_WIN32_COMPAT_H

#ifndef TF_INVALID_UIELEMENTID
#define TF_INVALID_UIELEMENTID ((DWORD)-1)
#endif
#ifndef TF_CLUIE_DOCUMENTMGR
#define TF_CLUIE_DOCUMENTMGR 0x00000001u
#define TF_CLUIE_COUNT 0x00000002u
#define TF_CLUIE_SELECTION 0x00000004u
#define TF_CLUIE_STRING 0x00000008u
#define TF_CLUIE_PAGEINDEX 0x00000010u
#define TF_CLUIE_CURRENTPAGE 0x00000020u
#endif

static const IID XGE_IID_ITfCandidateListUIElement = {
	0xea1ea138, 0x19df, 0x11d7,
	{ 0xa6, 0xd2, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c }
};

static const IID XGE_IID_ITfCandidateListUIElementBehavior = {
	0x85fad185, 0x58ce, 0x497a,
	{ 0x94, 0x60, 0x35, 0x53, 0x66, 0xb6, 0x4b, 0x9a }
};

#ifndef __ITfCandidateListUIElement_INTERFACE_DEFINED__
#define __ITfCandidateListUIElement_INTERFACE_DEFINED__

#ifndef __ITfCandidateListUIElement_FWD_DEFINED__
#define __ITfCandidateListUIElement_FWD_DEFINED__
typedef struct ITfCandidateListUIElement ITfCandidateListUIElement;
#endif

typedef struct ITfCandidateListUIElementVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ITfCandidateListUIElement*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(ITfCandidateListUIElement*);
	ULONG (STDMETHODCALLTYPE *Release)(ITfCandidateListUIElement*);
	HRESULT (STDMETHODCALLTYPE *GetDescription)(ITfCandidateListUIElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *GetGUID)(ITfCandidateListUIElement*, GUID*);
	HRESULT (STDMETHODCALLTYPE *Show)(ITfCandidateListUIElement*, BOOL);
	HRESULT (STDMETHODCALLTYPE *IsShown)(ITfCandidateListUIElement*, BOOL*);
	HRESULT (STDMETHODCALLTYPE *GetUpdatedFlags)(ITfCandidateListUIElement*, DWORD*);
	HRESULT (STDMETHODCALLTYPE *GetDocumentMgr)(ITfCandidateListUIElement*, ITfDocumentMgr**);
	HRESULT (STDMETHODCALLTYPE *GetCount)(ITfCandidateListUIElement*, UINT*);
	HRESULT (STDMETHODCALLTYPE *GetSelection)(ITfCandidateListUIElement*, UINT*);
	HRESULT (STDMETHODCALLTYPE *GetString)(ITfCandidateListUIElement*, UINT, BSTR*);
	HRESULT (STDMETHODCALLTYPE *GetPageIndex)(ITfCandidateListUIElement*, UINT*, UINT, UINT*);
	HRESULT (STDMETHODCALLTYPE *SetPageIndex)(ITfCandidateListUIElement*, UINT*, UINT);
	HRESULT (STDMETHODCALLTYPE *GetCurrentPage)(ITfCandidateListUIElement*, UINT*);
} ITfCandidateListUIElementVtbl;

struct ITfCandidateListUIElement {
	const ITfCandidateListUIElementVtbl* lpVtbl;
};
#endif

#ifndef __ITfCandidateListUIElementBehavior_INTERFACE_DEFINED__
#define __ITfCandidateListUIElementBehavior_INTERFACE_DEFINED__

#ifndef __ITfCandidateListUIElementBehavior_FWD_DEFINED__
#define __ITfCandidateListUIElementBehavior_FWD_DEFINED__
typedef struct ITfCandidateListUIElementBehavior ITfCandidateListUIElementBehavior;
#endif

typedef struct ITfCandidateListUIElementBehaviorVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ITfCandidateListUIElementBehavior*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(ITfCandidateListUIElementBehavior*);
	ULONG (STDMETHODCALLTYPE *Release)(ITfCandidateListUIElementBehavior*);
	HRESULT (STDMETHODCALLTYPE *GetDescription)(ITfCandidateListUIElementBehavior*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *GetGUID)(ITfCandidateListUIElementBehavior*, GUID*);
	HRESULT (STDMETHODCALLTYPE *Show)(ITfCandidateListUIElementBehavior*, BOOL);
	HRESULT (STDMETHODCALLTYPE *IsShown)(ITfCandidateListUIElementBehavior*, BOOL*);
	HRESULT (STDMETHODCALLTYPE *GetUpdatedFlags)(ITfCandidateListUIElementBehavior*, DWORD*);
	HRESULT (STDMETHODCALLTYPE *GetDocumentMgr)(ITfCandidateListUIElementBehavior*, ITfDocumentMgr**);
	HRESULT (STDMETHODCALLTYPE *GetCount)(ITfCandidateListUIElementBehavior*, UINT*);
	HRESULT (STDMETHODCALLTYPE *GetSelection)(ITfCandidateListUIElementBehavior*, UINT*);
	HRESULT (STDMETHODCALLTYPE *GetString)(ITfCandidateListUIElementBehavior*, UINT, BSTR*);
	HRESULT (STDMETHODCALLTYPE *GetPageIndex)(ITfCandidateListUIElementBehavior*, UINT*, UINT, UINT*);
	HRESULT (STDMETHODCALLTYPE *SetPageIndex)(ITfCandidateListUIElementBehavior*, UINT*, UINT);
	HRESULT (STDMETHODCALLTYPE *GetCurrentPage)(ITfCandidateListUIElementBehavior*, UINT*);
	HRESULT (STDMETHODCALLTYPE *SetSelection)(ITfCandidateListUIElementBehavior*, UINT);
	HRESULT (STDMETHODCALLTYPE *Finalize)(ITfCandidateListUIElementBehavior*);
	HRESULT (STDMETHODCALLTYPE *Abort)(ITfCandidateListUIElementBehavior*);
} ITfCandidateListUIElementBehaviorVtbl;

struct ITfCandidateListUIElementBehavior {
	const ITfCandidateListUIElementBehaviorVtbl* lpVtbl;
};
#endif

#endif
