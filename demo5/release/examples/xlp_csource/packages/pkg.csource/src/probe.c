#include <stdint.h>
#include <windows.h>
#include <objbase.h>

int pkg_csource_probe(void)
{
	return 123;
}

int pkg_csource_add(int left, int right)
{
	return left + right;
}

int pkg_csource_com_ready(void)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if ( SUCCEEDED(hr) ) {
		CoUninitialize();
		return 1;
	}
	return hr == RPC_E_CHANGED_MODE ? 1 : 0;
}
