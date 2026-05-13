#include <windows.h>
#include <ole2.h>
#include <uiautomation.h>

int main(void)
{
	IUIAutomation* automation = NULL;
	HRESULT hr;

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if ( FAILED(hr) && (hr != RPC_E_CHANGED_MODE) ) {
		return 1;
	}
	hr = CoCreateInstance(
		&CLSID_CUIAutomation,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IUIAutomation,
		(void**)&automation
	);
	if ( SUCCEEDED(hr) && (automation != NULL) ) {
		automation->lpVtbl->Release(automation);
	}
	CoUninitialize();
	return SUCCEEDED(hr) ? 0 : 2;
}
