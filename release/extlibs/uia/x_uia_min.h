#ifndef X_UIA_MIN_H
#define X_UIA_MIN_H

#include <windows.h>

#if !defined(__TINYC__)
#include <oleauto.h>
#endif

#ifndef X_UIA_OLEAUTO_MIN_H
#define X_UIA_OLEAUTO_MIN_H
#if defined(__TINYC__)
#ifndef VT_EMPTY
typedef unsigned short VARTYPE;
enum {
	VT_EMPTY = 0,
	VT_I4 = 3,
	VT_BSTR = 8
};
#endif
#ifndef X_UIA_BSTR_DEFINED
#define X_UIA_BSTR_DEFINED
typedef WCHAR OLECHAR;
typedef OLECHAR* BSTR;
#endif
#ifndef X_UIA_SAFEARRAY_DEFINED
#define X_UIA_SAFEARRAY_DEFINED
typedef struct tagSAFEARRAY SAFEARRAY;
#endif
#ifndef X_UIA_VARIANT_DEFINED
#define X_UIA_VARIANT_DEFINED
typedef struct tagVARIANT {
	union {
		struct {
			VARTYPE vt;
			WORD wReserved1;
			WORD wReserved2;
			WORD wReserved3;
			union {
				LONG lVal;
				BSTR bstrVal;
#if defined(_WIN64)
				unsigned char abi_value[16];
#endif
			};
		};
	};
} VARIANT;
#endif
#ifndef X_UIA_OLEAUTO_IMPORT
#define X_UIA_OLEAUTO_IMPORT __declspec(dllimport)
#endif
X_UIA_OLEAUTO_IMPORT BSTR WINAPI SysAllocStringLen(const OLECHAR* text, UINT length);
X_UIA_OLEAUTO_IMPORT void WINAPI SysFreeString(BSTR text);
X_UIA_OLEAUTO_IMPORT HRESULT WINAPI VariantClear(VARIANT* value);
X_UIA_OLEAUTO_IMPORT void WINAPI VariantInit(VARIANT* value);
#endif
#endif

__declspec(dllimport) void WINAPI CoUninitialize(void);

#ifndef CONST_VTBL
#define CONST_VTBL const
#endif

typedef int PROPERTYID;
typedef int PATTERNID;
typedef int CONTROLTYPEID;
typedef int EVENTID;
typedef void* UIA_HWND;

#ifndef COINIT_MULTITHREADED
#define COINIT_MULTITHREADED 0x0
#endif
#ifndef CLSCTX_INPROC_SERVER
#define CLSCTX_INPROC_SERVER 0x1
#endif

typedef struct IUnknown IUnknown;
typedef struct IUIAutomation IUIAutomation;
typedef struct IUIAutomationElement IUIAutomationElement;
typedef struct IUIAutomationElementArray IUIAutomationElementArray;
typedef struct IUIAutomationCondition IUIAutomationCondition;
typedef struct IUIAutomationTreeWalker IUIAutomationTreeWalker;
typedef struct IUIAutomationCacheRequest IUIAutomationCacheRequest;
typedef struct IUIAutomationEventHandler IUIAutomationEventHandler;
typedef struct IUIAutomationPropertyChangedEventHandler IUIAutomationPropertyChangedEventHandler;
typedef struct IUIAutomationStructureChangedEventHandler IUIAutomationStructureChangedEventHandler;
typedef struct IUIAutomationFocusChangedEventHandler IUIAutomationFocusChangedEventHandler;
typedef struct IUIAutomationProxyFactory IUIAutomationProxyFactory;
typedef struct IUIAutomationProxyFactoryEntry IUIAutomationProxyFactoryEntry;
typedef struct IUIAutomationProxyFactoryMapping IUIAutomationProxyFactoryMapping;
typedef struct IUIAutomationInvokePattern IUIAutomationInvokePattern;
typedef struct IUIAutomationValuePattern IUIAutomationValuePattern;
typedef struct IUIAutomationRangeValuePattern IUIAutomationRangeValuePattern;
typedef struct IUIAutomationScrollPattern IUIAutomationScrollPattern;
typedef struct IUIAutomationTextPattern IUIAutomationTextPattern;
typedef struct IUIAutomationTogglePattern IUIAutomationTogglePattern;
typedef struct IUIAutomationWindowPattern IUIAutomationWindowPattern;
typedef struct IUIAutomationTextRange IUIAutomationTextRange;
typedef struct IUIAutomationTextRangeArray IUIAutomationTextRangeArray;

typedef int TEXTATTRIBUTEID;

enum TextPatternRangeEndpoint {
	TextPatternRangeEndpoint_Start = 0,
	TextPatternRangeEndpoint_End = 1
};

enum TextUnit {
	TextUnit_Character = 0,
	TextUnit_Format = 1,
	TextUnit_Word = 2,
	TextUnit_Line = 3,
	TextUnit_Paragraph = 4,
	TextUnit_Page = 5,
	TextUnit_Document = 6
};

enum SupportedTextSelection {
	SupportedTextSelection_None = 0,
	SupportedTextSelection_Single = 1,
	SupportedTextSelection_Multiple = 2
};

enum ScrollAmount {
	ScrollAmount_LargeDecrement = 0,
	ScrollAmount_SmallDecrement = 1,
	ScrollAmount_NoAmount = 2,
	ScrollAmount_LargeIncrement = 3,
	ScrollAmount_SmallIncrement = 4
};

enum ToggleState {
	ToggleState_Off = 0,
	ToggleState_On = 1,
	ToggleState_Indeterminate = 2
};

enum WindowVisualState {
	WindowVisualState_Normal = 0,
	WindowVisualState_Maximized = 1,
	WindowVisualState_Minimized = 2
};

enum WindowInteractionState {
	WindowInteractionState_Running = 0,
	WindowInteractionState_Closing = 1,
	WindowInteractionState_ReadyForUserInteraction = 2,
	WindowInteractionState_BlockedByModalWindow = 3,
	WindowInteractionState_NotResponding = 4
};

enum TreeScope {
	TreeScope_Element = 0x1,
	TreeScope_Children = 0x2,
	TreeScope_Descendants = 0x4,
	TreeScope_Parent = 0x8,
	TreeScope_Ancestors = 0x10,
	TreeScope_Subtree = 0x7
};

enum PropertyConditionFlags {
	PropertyConditionFlags_None = 0x0,
	PropertyConditionFlags_IgnoreCase = 0x1
};

enum OrientationType {
	OrientationType_None = 0,
	OrientationType_Horizontal = 1,
	OrientationType_Vertical = 2
};

enum AutomationElementMode {
	AutomationElementMode_None = 0,
	AutomationElementMode_Full = 1
};

#define UIA_InvokePatternId 10000
#define UIA_ValuePatternId 10002
#define UIA_RangeValuePatternId 10003
#define UIA_ScrollPatternId 10004
#define UIA_WindowPatternId 10009
#define UIA_TextPatternId 10014
#define UIA_TogglePatternId 10015
#define UIA_ProcessIdPropertyId 30002
#define UIA_ControlTypePropertyId 30003
#define UIA_NamePropertyId 30005
#define UIA_AutomationIdPropertyId 30011
#define UIA_ClassNamePropertyId 30012
#define UIA_NativeWindowHandlePropertyId 30020
#define UIA_BoundingRectanglePropertyId 30001

static const CLSID X_UIA_CLSID_CUIAutomation = { 0xff48dba4, 0x60ef, 0x4201, { 0xaa, 0x87, 0x54, 0x10, 0x3e, 0xef, 0x59, 0x4e } };
static const IID X_UIA_IID_IUIAutomation = { 0x30cbe57d, 0xd9d0, 0x452a, { 0xab, 0x13, 0x7a, 0xc5, 0xac, 0x48, 0x25, 0xee } };
static const IID X_UIA_IID_IUIAutomationInvokePattern = { 0xfb377fbe, 0x8ea6, 0x46d5, { 0x9c, 0x73, 0x64, 0x99, 0x64, 0x2d, 0x30, 0x59 } };
static const IID X_UIA_IID_IUIAutomationValuePattern = { 0xa94cd8b1, 0x0844, 0x4cd6, { 0x9d, 0x2d, 0x64, 0x05, 0x37, 0xab, 0x39, 0xe9 } };
static const IID X_UIA_IID_IUIAutomationRangeValuePattern = { 0x59213f4f, 0x7346, 0x49e5, { 0xb1, 0x20, 0x80, 0x55, 0x59, 0x87, 0xa1, 0x48 } };
static const IID X_UIA_IID_IUIAutomationScrollPattern = { 0x88f4d42a, 0xe881, 0x459d, { 0xa7, 0x7c, 0x73, 0xbb, 0xbb, 0x7e, 0x02, 0xdc } };
static const IID X_UIA_IID_IUIAutomationTextPattern = { 0x32eba289, 0x3583, 0x42c9, { 0x9c, 0x59, 0x3b, 0x6d, 0x9a, 0x1e, 0x9b, 0x6a } };
static const IID X_UIA_IID_IUIAutomationTogglePattern = { 0x94cf8058, 0x9b8d, 0x4ab9, { 0x8b, 0xfd, 0x4c, 0xd0, 0xa3, 0x3c, 0x8c, 0x70 } };
static const IID X_UIA_IID_IUIAutomationWindowPattern = { 0x0faef453, 0x9208, 0x43ef, { 0xbb, 0xb2, 0x3b, 0x48, 0x51, 0x77, 0x86, 0x4f } };
static const IID X_UIA_IID_IUIAutomationPropertyChangedEventHandler = { 0x40cd37d4, 0xc756, 0x4b0c, { 0x8c, 0x6f, 0xbd, 0xdf, 0xee, 0xb1, 0x3b, 0x50 } };
static const IID X_UIA_IID_IUIAutomationFocusChangedEventHandler = { 0xc270f6b5, 0x5c69, 0x4290, { 0x97, 0x45, 0x7a, 0x7f, 0x97, 0x16, 0x94, 0x68 } };

typedef struct IUIAutomationConditionVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationCondition*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationCondition*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationCondition*);
} IUIAutomationConditionVtbl;

struct IUIAutomationCondition {
	CONST_VTBL IUIAutomationConditionVtbl* lpVtbl;
};

typedef struct IUIAutomationPropertyChangedEventHandlerVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationPropertyChangedEventHandler*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationPropertyChangedEventHandler*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationPropertyChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *HandlePropertyChangedEvent)(IUIAutomationPropertyChangedEventHandler*, IUIAutomationElement*, PROPERTYID, VARIANT);
} IUIAutomationPropertyChangedEventHandlerVtbl;

struct IUIAutomationPropertyChangedEventHandler {
	CONST_VTBL IUIAutomationPropertyChangedEventHandlerVtbl* lpVtbl;
};

typedef struct IUIAutomationFocusChangedEventHandlerVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationFocusChangedEventHandler*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationFocusChangedEventHandler*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationFocusChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *HandleFocusChangedEvent)(IUIAutomationFocusChangedEventHandler*, IUIAutomationElement*);
} IUIAutomationFocusChangedEventHandlerVtbl;

struct IUIAutomationFocusChangedEventHandler {
	CONST_VTBL IUIAutomationFocusChangedEventHandlerVtbl* lpVtbl;
};

typedef struct IUIAutomationCacheRequestVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationCacheRequest*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationCacheRequest*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationCacheRequest*);
	HRESULT (STDMETHODCALLTYPE *AddProperty)(IUIAutomationCacheRequest*, PROPERTYID);
	HRESULT (STDMETHODCALLTYPE *AddPattern)(IUIAutomationCacheRequest*, PATTERNID);
	HRESULT (STDMETHODCALLTYPE *Clone)(IUIAutomationCacheRequest*, IUIAutomationCacheRequest**);
	HRESULT (STDMETHODCALLTYPE *get_TreeScope)(IUIAutomationCacheRequest*, enum TreeScope*);
	HRESULT (STDMETHODCALLTYPE *put_TreeScope)(IUIAutomationCacheRequest*, enum TreeScope);
	HRESULT (STDMETHODCALLTYPE *get_TreeFilter)(IUIAutomationCacheRequest*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *put_TreeFilter)(IUIAutomationCacheRequest*, IUIAutomationCondition*);
	HRESULT (STDMETHODCALLTYPE *get_AutomationElementMode)(IUIAutomationCacheRequest*, enum AutomationElementMode*);
	HRESULT (STDMETHODCALLTYPE *put_AutomationElementMode)(IUIAutomationCacheRequest*, enum AutomationElementMode);
} IUIAutomationCacheRequestVtbl;

struct IUIAutomationCacheRequest {
	CONST_VTBL IUIAutomationCacheRequestVtbl* lpVtbl;
};

typedef struct IUIAutomationElementArrayVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationElementArray*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationElementArray*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationElementArray*);
	HRESULT (STDMETHODCALLTYPE *get_Length)(IUIAutomationElementArray*, int*);
	HRESULT (STDMETHODCALLTYPE *GetElement)(IUIAutomationElementArray*, int, IUIAutomationElement**);
} IUIAutomationElementArrayVtbl;

struct IUIAutomationElementArray {
	CONST_VTBL IUIAutomationElementArrayVtbl* lpVtbl;
};

typedef struct IUIAutomationTreeWalkerVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationTreeWalker*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationTreeWalker*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationTreeWalker*);
	HRESULT (STDMETHODCALLTYPE *GetParentElement)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetFirstChildElement)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetLastChildElement)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetNextSiblingElement)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetPreviousSiblingElement)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *NormalizeElement)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetParentElementBuildCache)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetFirstChildElementBuildCache)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetLastChildElementBuildCache)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetNextSiblingElementBuildCache)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetPreviousSiblingElementBuildCache)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *NormalizeElementBuildCache)(IUIAutomationTreeWalker*, IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *get_Condition)(IUIAutomationTreeWalker*, IUIAutomationCondition**);
} IUIAutomationTreeWalkerVtbl;

struct IUIAutomationTreeWalker {
	CONST_VTBL IUIAutomationTreeWalkerVtbl* lpVtbl;
};

typedef struct IUIAutomationInvokePatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationInvokePattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationInvokePattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationInvokePattern*);
	HRESULT (STDMETHODCALLTYPE *Invoke)(IUIAutomationInvokePattern*);
} IUIAutomationInvokePatternVtbl;

struct IUIAutomationInvokePattern {
	CONST_VTBL IUIAutomationInvokePatternVtbl* lpVtbl;
};

typedef struct IUIAutomationValuePatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationValuePattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationValuePattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationValuePattern*);
	HRESULT (STDMETHODCALLTYPE *SetValue)(IUIAutomationValuePattern*, BSTR);
	HRESULT (STDMETHODCALLTYPE *get_CurrentValue)(IUIAutomationValuePattern*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsReadOnly)(IUIAutomationValuePattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedValue)(IUIAutomationValuePattern*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsReadOnly)(IUIAutomationValuePattern*, WINBOOL*);
} IUIAutomationValuePatternVtbl;

struct IUIAutomationValuePattern {
	CONST_VTBL IUIAutomationValuePatternVtbl* lpVtbl;
};

typedef struct IUIAutomationRangeValuePatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationRangeValuePattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationRangeValuePattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationRangeValuePattern*);
	HRESULT (STDMETHODCALLTYPE *SetValue)(IUIAutomationRangeValuePattern*, double);
	HRESULT (STDMETHODCALLTYPE *get_CurrentValue)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsReadOnly)(IUIAutomationRangeValuePattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentMaximum)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentMinimum)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentLargeChange)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentSmallChange)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedValue)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsReadOnly)(IUIAutomationRangeValuePattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedMaximum)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedMinimum)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedLargeChange)(IUIAutomationRangeValuePattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedSmallChange)(IUIAutomationRangeValuePattern*, double*);
} IUIAutomationRangeValuePatternVtbl;

struct IUIAutomationRangeValuePattern {
	CONST_VTBL IUIAutomationRangeValuePatternVtbl* lpVtbl;
};

typedef struct IUIAutomationScrollPatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationScrollPattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationScrollPattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationScrollPattern*);
	HRESULT (STDMETHODCALLTYPE *Scroll)(IUIAutomationScrollPattern*, enum ScrollAmount, enum ScrollAmount);
	HRESULT (STDMETHODCALLTYPE *SetScrollPercent)(IUIAutomationScrollPattern*, double, double);
	HRESULT (STDMETHODCALLTYPE *get_CurrentHorizontalScrollPercent)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentVerticalScrollPercent)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentHorizontalViewSize)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentVerticalViewSize)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentHorizontallyScrollable)(IUIAutomationScrollPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentVerticallyScrollable)(IUIAutomationScrollPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedHorizontalScrollPercent)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedVerticalScrollPercent)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedHorizontalViewSize)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedVerticalViewSize)(IUIAutomationScrollPattern*, double*);
	HRESULT (STDMETHODCALLTYPE *get_CachedHorizontallyScrollable)(IUIAutomationScrollPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedVerticallyScrollable)(IUIAutomationScrollPattern*, WINBOOL*);
} IUIAutomationScrollPatternVtbl;

struct IUIAutomationScrollPattern {
	CONST_VTBL IUIAutomationScrollPatternVtbl* lpVtbl;
};

typedef struct IUIAutomationTextRangeVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationTextRange*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationTextRange*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationTextRange*);
	HRESULT (STDMETHODCALLTYPE *Clone)(IUIAutomationTextRange*, IUIAutomationTextRange**);
	HRESULT (STDMETHODCALLTYPE *Compare)(IUIAutomationTextRange*, IUIAutomationTextRange*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *CompareEndpoints)(IUIAutomationTextRange*, enum TextPatternRangeEndpoint, IUIAutomationTextRange*, enum TextPatternRangeEndpoint, int*);
	HRESULT (STDMETHODCALLTYPE *ExpandToEnclosingUnit)(IUIAutomationTextRange*, enum TextUnit);
	HRESULT (STDMETHODCALLTYPE *FindAttribute)(IUIAutomationTextRange*, TEXTATTRIBUTEID, VARIANT, WINBOOL, IUIAutomationTextRange**);
	HRESULT (STDMETHODCALLTYPE *FindText)(IUIAutomationTextRange*, BSTR, WINBOOL, WINBOOL, IUIAutomationTextRange**);
	HRESULT (STDMETHODCALLTYPE *GetAttributeValue)(IUIAutomationTextRange*, TEXTATTRIBUTEID, VARIANT*);
	HRESULT (STDMETHODCALLTYPE *GetBoundingRectangles)(IUIAutomationTextRange*, SAFEARRAY**);
	HRESULT (STDMETHODCALLTYPE *GetEnclosingElement)(IUIAutomationTextRange*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetText)(IUIAutomationTextRange*, int, BSTR*);
	HRESULT (STDMETHODCALLTYPE *Move)(IUIAutomationTextRange*, enum TextUnit, int, int*);
	HRESULT (STDMETHODCALLTYPE *MoveEndpointByUnit)(IUIAutomationTextRange*, enum TextPatternRangeEndpoint, enum TextUnit, int, int*);
	HRESULT (STDMETHODCALLTYPE *MoveEndpointByRange)(IUIAutomationTextRange*, enum TextPatternRangeEndpoint, IUIAutomationTextRange*, enum TextPatternRangeEndpoint);
	HRESULT (STDMETHODCALLTYPE *Select)(IUIAutomationTextRange*);
	HRESULT (STDMETHODCALLTYPE *AddToSelection)(IUIAutomationTextRange*);
	HRESULT (STDMETHODCALLTYPE *RemoveFromSelection)(IUIAutomationTextRange*);
	HRESULT (STDMETHODCALLTYPE *ScrollIntoView)(IUIAutomationTextRange*, WINBOOL);
	HRESULT (STDMETHODCALLTYPE *GetChildren)(IUIAutomationTextRange*, IUIAutomationElementArray**);
} IUIAutomationTextRangeVtbl;

struct IUIAutomationTextRange {
	CONST_VTBL IUIAutomationTextRangeVtbl* lpVtbl;
};

typedef struct IUIAutomationTextPatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationTextPattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationTextPattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationTextPattern*);
	HRESULT (STDMETHODCALLTYPE *RangeFromPoint)(IUIAutomationTextPattern*, POINT, IUIAutomationTextRange**);
	HRESULT (STDMETHODCALLTYPE *RangeFromChild)(IUIAutomationTextPattern*, IUIAutomationElement*, IUIAutomationTextRange**);
	HRESULT (STDMETHODCALLTYPE *GetSelection)(IUIAutomationTextPattern*, IUIAutomationTextRangeArray**);
	HRESULT (STDMETHODCALLTYPE *GetVisibleRanges)(IUIAutomationTextPattern*, IUIAutomationTextRangeArray**);
	HRESULT (STDMETHODCALLTYPE *get_DocumentRange)(IUIAutomationTextPattern*, IUIAutomationTextRange**);
	HRESULT (STDMETHODCALLTYPE *get_SupportedTextSelection)(IUIAutomationTextPattern*, enum SupportedTextSelection*);
} IUIAutomationTextPatternVtbl;

struct IUIAutomationTextPattern {
	CONST_VTBL IUIAutomationTextPatternVtbl* lpVtbl;
};

typedef struct IUIAutomationTogglePatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationTogglePattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationTogglePattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationTogglePattern*);
	HRESULT (STDMETHODCALLTYPE *Toggle)(IUIAutomationTogglePattern*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentToggleState)(IUIAutomationTogglePattern*, enum ToggleState*);
	HRESULT (STDMETHODCALLTYPE *get_CachedToggleState)(IUIAutomationTogglePattern*, enum ToggleState*);
} IUIAutomationTogglePatternVtbl;

struct IUIAutomationTogglePattern {
	CONST_VTBL IUIAutomationTogglePatternVtbl* lpVtbl;
};

typedef struct IUIAutomationWindowPatternVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationWindowPattern*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationWindowPattern*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationWindowPattern*);
	HRESULT (STDMETHODCALLTYPE *Close)(IUIAutomationWindowPattern*);
	HRESULT (STDMETHODCALLTYPE *WaitForInputIdle)(IUIAutomationWindowPattern*, int, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *SetWindowVisualState)(IUIAutomationWindowPattern*, enum WindowVisualState);
	HRESULT (STDMETHODCALLTYPE *get_CurrentCanMaximize)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentCanMinimize)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsModal)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsTopmost)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentWindowVisualState)(IUIAutomationWindowPattern*, enum WindowVisualState*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentWindowInteractionState)(IUIAutomationWindowPattern*, enum WindowInteractionState*);
	HRESULT (STDMETHODCALLTYPE *get_CachedCanMaximize)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedCanMinimize)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsModal)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsTopmost)(IUIAutomationWindowPattern*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedWindowVisualState)(IUIAutomationWindowPattern*, enum WindowVisualState*);
	HRESULT (STDMETHODCALLTYPE *get_CachedWindowInteractionState)(IUIAutomationWindowPattern*, enum WindowInteractionState*);
} IUIAutomationWindowPatternVtbl;

struct IUIAutomationWindowPattern {
	CONST_VTBL IUIAutomationWindowPatternVtbl* lpVtbl;
};

typedef struct IUIAutomationElementVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomationElement*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomationElement*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomationElement*);
	HRESULT (STDMETHODCALLTYPE *SetFocus)(IUIAutomationElement*);
	HRESULT (STDMETHODCALLTYPE *GetRuntimeId)(IUIAutomationElement*, SAFEARRAY**);
	HRESULT (STDMETHODCALLTYPE *FindFirst)(IUIAutomationElement*, enum TreeScope, IUIAutomationCondition*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *FindAll)(IUIAutomationElement*, enum TreeScope, IUIAutomationCondition*, IUIAutomationElementArray**);
	HRESULT (STDMETHODCALLTYPE *FindFirstBuildCache)(IUIAutomationElement*, enum TreeScope, IUIAutomationCondition*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *FindAllBuildCache)(IUIAutomationElement*, enum TreeScope, IUIAutomationCondition*, IUIAutomationCacheRequest*, IUIAutomationElementArray**);
	HRESULT (STDMETHODCALLTYPE *BuildUpdatedCache)(IUIAutomationElement*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetCurrentPropertyValue)(IUIAutomationElement*, PROPERTYID, VARIANT*);
	HRESULT (STDMETHODCALLTYPE *GetCurrentPropertyValueEx)(IUIAutomationElement*, PROPERTYID, WINBOOL, VARIANT*);
	HRESULT (STDMETHODCALLTYPE *GetCachedPropertyValue)(IUIAutomationElement*, PROPERTYID, VARIANT*);
	HRESULT (STDMETHODCALLTYPE *GetCachedPropertyValueEx)(IUIAutomationElement*, PROPERTYID, WINBOOL, VARIANT*);
	HRESULT (STDMETHODCALLTYPE *GetCurrentPatternAs)(IUIAutomationElement*, PATTERNID, REFIID, void**);
	HRESULT (STDMETHODCALLTYPE *GetCachedPatternAs)(IUIAutomationElement*, PATTERNID, REFIID, void**);
	HRESULT (STDMETHODCALLTYPE *GetCurrentPattern)(IUIAutomationElement*, PATTERNID, IUnknown**);
	HRESULT (STDMETHODCALLTYPE *GetCachedPattern)(IUIAutomationElement*, PATTERNID, IUnknown**);
	HRESULT (STDMETHODCALLTYPE *GetCachedParent)(IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetCachedChildren)(IUIAutomationElement*, IUIAutomationElementArray**);
	HRESULT (STDMETHODCALLTYPE *get_CurrentProcessId)(IUIAutomationElement*, int*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentControlType)(IUIAutomationElement*, CONTROLTYPEID*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentLocalizedControlType)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentName)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentAcceleratorKey)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentAccessKey)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentHasKeyboardFocus)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsKeyboardFocusable)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsEnabled)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentAutomationId)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentClassName)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentHelpText)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentCulture)(IUIAutomationElement*, int*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsControlElement)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsContentElement)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsPassword)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentNativeWindowHandle)(IUIAutomationElement*, UIA_HWND*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentItemType)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsOffscreen)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentOrientation)(IUIAutomationElement*, enum OrientationType*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentFrameworkId)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsRequiredForForm)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentItemStatus)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentBoundingRectangle)(IUIAutomationElement*, RECT*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentLabeledBy)(IUIAutomationElement*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *get_CurrentAriaRole)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentAriaProperties)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentIsDataValidForForm)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CurrentControllerFor)(IUIAutomationElement*, IUIAutomationElementArray**);
	HRESULT (STDMETHODCALLTYPE *get_CurrentDescribedBy)(IUIAutomationElement*, IUIAutomationElementArray**);
	HRESULT (STDMETHODCALLTYPE *get_CurrentFlowsTo)(IUIAutomationElement*, IUIAutomationElementArray**);
	HRESULT (STDMETHODCALLTYPE *get_CurrentProviderDescription)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedProcessId)(IUIAutomationElement*, int*);
	HRESULT (STDMETHODCALLTYPE *get_CachedControlType)(IUIAutomationElement*, CONTROLTYPEID*);
	HRESULT (STDMETHODCALLTYPE *get_CachedLocalizedControlType)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedName)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedAcceleratorKey)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedAccessKey)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedHasKeyboardFocus)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsKeyboardFocusable)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsEnabled)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedAutomationId)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedClassName)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedHelpText)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedCulture)(IUIAutomationElement*, int*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsControlElement)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsContentElement)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsPassword)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedNativeWindowHandle)(IUIAutomationElement*, UIA_HWND*);
	HRESULT (STDMETHODCALLTYPE *get_CachedItemType)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsOffscreen)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedOrientation)(IUIAutomationElement*, enum OrientationType*);
	HRESULT (STDMETHODCALLTYPE *get_CachedFrameworkId)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedIsRequiredForForm)(IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *get_CachedItemStatus)(IUIAutomationElement*, BSTR*);
	HRESULT (STDMETHODCALLTYPE *get_CachedBoundingRectangle)(IUIAutomationElement*, RECT*);
} IUIAutomationElementVtbl;

struct IUIAutomationElement {
	CONST_VTBL IUIAutomationElementVtbl* lpVtbl;
};

typedef struct IUIAutomationVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(IUIAutomation*, REFIID, void**);
	ULONG (STDMETHODCALLTYPE *AddRef)(IUIAutomation*);
	ULONG (STDMETHODCALLTYPE *Release)(IUIAutomation*);
	HRESULT (STDMETHODCALLTYPE *CompareElements)(IUIAutomation*, IUIAutomationElement*, IUIAutomationElement*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *CompareRuntimeIds)(IUIAutomation*, SAFEARRAY*, SAFEARRAY*, WINBOOL*);
	HRESULT (STDMETHODCALLTYPE *GetRootElement)(IUIAutomation*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *ElementFromHandle)(IUIAutomation*, UIA_HWND, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *ElementFromPoint)(IUIAutomation*, POINT, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetFocusedElement)(IUIAutomation*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetRootElementBuildCache)(IUIAutomation*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *ElementFromHandleBuildCache)(IUIAutomation*, UIA_HWND, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *ElementFromPointBuildCache)(IUIAutomation*, POINT, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *GetFocusedElementBuildCache)(IUIAutomation*, IUIAutomationCacheRequest*, IUIAutomationElement**);
	HRESULT (STDMETHODCALLTYPE *CreateTreeWalker)(IUIAutomation*, IUIAutomationCondition*, IUIAutomationTreeWalker**);
	HRESULT (STDMETHODCALLTYPE *get_ControlViewWalker)(IUIAutomation*, IUIAutomationTreeWalker**);
	HRESULT (STDMETHODCALLTYPE *get_ContentViewWalker)(IUIAutomation*, IUIAutomationTreeWalker**);
	HRESULT (STDMETHODCALLTYPE *get_RawViewWalker)(IUIAutomation*, IUIAutomationTreeWalker**);
	HRESULT (STDMETHODCALLTYPE *get_RawViewCondition)(IUIAutomation*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *get_ControlViewCondition)(IUIAutomation*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *get_ContentViewCondition)(IUIAutomation*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateCacheRequest)(IUIAutomation*, IUIAutomationCacheRequest**);
	HRESULT (STDMETHODCALLTYPE *CreateTrueCondition)(IUIAutomation*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateFalseCondition)(IUIAutomation*, IUIAutomationCondition**);
	/* TCC 的 Win64 大结构体按值传参不符合这里的 COM ABI，实际传递的是临时结构地址。 */
#if defined(__TINYC__) && defined(_WIN64)
	HRESULT (STDMETHODCALLTYPE *CreatePropertyCondition)(IUIAutomation*, PROPERTYID, const VARIANT*, IUIAutomationCondition**);
#else
	HRESULT (STDMETHODCALLTYPE *CreatePropertyCondition)(IUIAutomation*, PROPERTYID, VARIANT, IUIAutomationCondition**);
#endif
	HRESULT (STDMETHODCALLTYPE *CreatePropertyConditionEx)(IUIAutomation*, PROPERTYID, VARIANT, enum PropertyConditionFlags, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateAndCondition)(IUIAutomation*, IUIAutomationCondition*, IUIAutomationCondition*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateAndConditionFromArray)(IUIAutomation*, SAFEARRAY*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateAndConditionFromNativeArray)(IUIAutomation*, IUIAutomationCondition**, int, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateOrCondition)(IUIAutomation*, IUIAutomationCondition*, IUIAutomationCondition*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateOrConditionFromArray)(IUIAutomation*, SAFEARRAY*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateOrConditionFromNativeArray)(IUIAutomation*, IUIAutomationCondition**, int, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *CreateNotCondition)(IUIAutomation*, IUIAutomationCondition*, IUIAutomationCondition**);
	HRESULT (STDMETHODCALLTYPE *AddAutomationEventHandler)(IUIAutomation*, EVENTID, IUIAutomationElement*, enum TreeScope, IUIAutomationCacheRequest*, IUIAutomationEventHandler*);
	HRESULT (STDMETHODCALLTYPE *RemoveAutomationEventHandler)(IUIAutomation*, EVENTID, IUIAutomationElement*, IUIAutomationEventHandler*);
	HRESULT (STDMETHODCALLTYPE *AddPropertyChangedEventHandlerNativeArray)(IUIAutomation*, IUIAutomationElement*, enum TreeScope, IUIAutomationCacheRequest*, IUIAutomationPropertyChangedEventHandler*, PROPERTYID*, int);
	HRESULT (STDMETHODCALLTYPE *AddPropertyChangedEventHandler)(IUIAutomation*, IUIAutomationElement*, enum TreeScope, IUIAutomationCacheRequest*, IUIAutomationPropertyChangedEventHandler*, SAFEARRAY*);
	HRESULT (STDMETHODCALLTYPE *RemovePropertyChangedEventHandler)(IUIAutomation*, IUIAutomationElement*, IUIAutomationPropertyChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *AddStructureChangedEventHandler)(IUIAutomation*, IUIAutomationElement*, enum TreeScope, IUIAutomationCacheRequest*, IUIAutomationStructureChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *RemoveStructureChangedEventHandler)(IUIAutomation*, IUIAutomationElement*, IUIAutomationStructureChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *AddFocusChangedEventHandler)(IUIAutomation*, IUIAutomationCacheRequest*, IUIAutomationFocusChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *RemoveFocusChangedEventHandler)(IUIAutomation*, IUIAutomationFocusChangedEventHandler*);
	HRESULT (STDMETHODCALLTYPE *RemoveAllEventHandlers)(IUIAutomation*);
} IUIAutomationVtbl;

struct IUIAutomation {
	CONST_VTBL IUIAutomationVtbl* lpVtbl;
};

#endif
