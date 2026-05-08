#include <windows.h>
#include <objbase.h>

int pkg_mixed_probe(void)
{
	return 321;
}

int pkg_mixed_add(int left, int right)
{
	return left + right;
}

int pkg_mixed_com_ready(void)
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if ( SUCCEEDED(hr) ) {
		CoUninitialize();
		return 1;
	}
	return (hr == RPC_E_CHANGED_MODE) ? 1 : 0;
}
