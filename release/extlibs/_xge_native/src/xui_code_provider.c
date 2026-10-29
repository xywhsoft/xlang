#include "../xui.h"

#include <string.h>

struct xui_code_provider_set_t {
	xui_code_completion_proc onComplete;
	void* pCompleteUser;
	xui_code_hover_proc onHover;
	void* pHoverUser;
	xui_code_signature_proc onSignature;
	void* pSignatureUser;
	xui_code_command_proc onCommand;
	void* pCommandUser;
	xui_code_command_enabled_proc onCommandEnabled;
	void* pCommandEnabledUser;
};

XUI_API int xuiCodeProviderSetCreate(xui_code_provider_set* ppProviders)
{
	xui_code_provider_set pProviders;

	if ( ppProviders == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppProviders = NULL;
	pProviders = (xui_code_provider_set)xrtMalloc(sizeof(*pProviders));
	if ( pProviders == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memset(pProviders, 0, sizeof(*pProviders));
	*ppProviders = pProviders;
	return XUI_OK;
}

XUI_API void xuiCodeProviderSetDestroy(xui_code_provider_set pProviders)
{
	xrtFree(pProviders);
}

XUI_API void xuiCodeProviderSetClear(xui_code_provider_set pProviders)
{
	if ( pProviders == NULL ) return;
	memset(pProviders, 0, sizeof(*pProviders));
}

XUI_API int xuiCodeProviderSetCompletion(xui_code_provider_set pProviders, xui_code_completion_proc onComplete, void* pUser)
{
	if ( pProviders == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProviders->onComplete = onComplete;
	pProviders->pCompleteUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeProviderSetHover(xui_code_provider_set pProviders, xui_code_hover_proc onHover, void* pUser)
{
	if ( pProviders == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProviders->onHover = onHover;
	pProviders->pHoverUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeProviderSetSignature(xui_code_provider_set pProviders, xui_code_signature_proc onSignature, void* pUser)
{
	if ( pProviders == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProviders->onSignature = onSignature;
	pProviders->pSignatureUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeProviderSetCommand(xui_code_provider_set pProviders, xui_code_command_proc onCommand, void* pUser)
{
	if ( pProviders == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProviders->onCommand = onCommand;
	pProviders->pCommandUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeProviderSetCommandEnabled(xui_code_provider_set pProviders, xui_code_command_enabled_proc onEnabled, void* pUser)
{
	if ( pProviders == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProviders->onCommandEnabled = onEnabled;
	pProviders->pCommandEnabledUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCodeProviderRequestCompletion(xui_code_provider_set pProviders, xui_widget pWidget, int iOffset, const char* sPrefix, xui_code_completion_item_t* pItems, int iItemCapacity, int* pItemCount)
{
	if ( (pProviders == NULL) || (pItemCount == NULL) || (iOffset < 0) || (iItemCapacity < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pItemCount = 0;
	if ( pProviders->onComplete == NULL ) return XUI_ERROR_UNSUPPORTED;
	return pProviders->onComplete((xui_widget_t*)pWidget, iOffset, (sPrefix != NULL) ? sPrefix : "", pItems, iItemCapacity, pItemCount, pProviders->pCompleteUser);
}

XUI_API int xuiCodeProviderRequestHover(xui_code_provider_set pProviders, xui_widget pWidget, int iOffset, xui_code_hover_t* pHover)
{
	if ( (pProviders == NULL) || (pHover == NULL) || (iOffset < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pHover, 0, sizeof(*pHover));
	pHover->iSize = sizeof(*pHover);
	if ( pProviders->onHover == NULL ) return XUI_ERROR_UNSUPPORTED;
	return pProviders->onHover((xui_widget_t*)pWidget, iOffset, pHover, pProviders->pHoverUser);
}

XUI_API int xuiCodeProviderRequestSignature(xui_code_provider_set pProviders, xui_widget pWidget, int iOffset, xui_code_signature_help_t* pHelp)
{
	if ( (pProviders == NULL) || (pHelp == NULL) || (iOffset < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pHelp, 0, sizeof(*pHelp));
	pHelp->iSize = sizeof(*pHelp);
	if ( pProviders->onSignature == NULL ) return XUI_ERROR_UNSUPPORTED;
	return pProviders->onSignature((xui_widget_t*)pWidget, iOffset, pHelp, pProviders->pSignatureUser);
}

XUI_API int xuiCodeProviderExecuteCommand(xui_code_provider_set pProviders, xui_widget pWidget, int iCommand, const void* pCommandData, int* pHandled)
{
	if ( (pProviders == NULL) || (pHandled == NULL) || (iCommand <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pHandled = 0;
	if ( pProviders->onCommand == NULL ) return XUI_ERROR_UNSUPPORTED;
	return pProviders->onCommand((xui_widget_t*)pWidget, iCommand, pCommandData, pHandled, pProviders->pCommandUser);
}

XUI_API int xuiCodeProviderCanExecuteCommand(xui_code_provider_set pProviders, xui_widget pWidget, int iCommand, int* pEnabled)
{
	if ( (pProviders == NULL) || (pEnabled == NULL) || (iCommand <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pEnabled = 0;
	if ( pProviders->onCommandEnabled == NULL ) return XUI_ERROR_UNSUPPORTED;
	return pProviders->onCommandEnabled((xui_widget_t*)pWidget, iCommand, pEnabled, pProviders->pCommandEnabledUser);
}
