#include "app/xide_platform.h"

#ifndef XGE_KEY_LEFT_SHIFT
#define XGE_KEY_LEFT_SHIFT 340
#define XGE_KEY_LEFT_CONTROL 341
#define XGE_KEY_LEFT_ALT 342
#define XGE_KEY_RIGHT_SHIFT 344
#define XGE_KEY_RIGHT_CONTROL 345
#define XGE_KEY_RIGHT_ALT 346
#endif

#ifndef XGE_KEY_F3
#define XGE_KEY_F3 292
#endif

static uint32_t xideReadButtons(void)
{
	uint32_t buttons = 0;
	if ( xgeMouseDown(XGE_MOUSE_LEFT) ) buttons |= XUI_POINTER_BUTTON_LEFT;
	if ( xgeMouseDown(XGE_MOUSE_RIGHT) ) buttons |= XUI_POINTER_BUTTON_RIGHT;
	if ( xgeMouseDown(XGE_MOUSE_MIDDLE) ) buttons |= XUI_POINTER_BUTTON_MIDDLE;
	return buttons;
}

static uint32_t xideReadModifiers(void)
{
	uint32_t modifiers = 0;
	if ( xgeKeyDown(XGE_KEY_LEFT_SHIFT) || xgeKeyDown(XGE_KEY_RIGHT_SHIFT) ) modifiers |= XUI_MOD_SHIFT;
	if ( xgeKeyDown(XGE_KEY_LEFT_CONTROL) || xgeKeyDown(XGE_KEY_RIGHT_CONTROL) ) modifiers |= XUI_MOD_CTRL;
	if ( xgeKeyDown(XGE_KEY_LEFT_ALT) || xgeKeyDown(XGE_KEY_RIGHT_ALT) ) modifiers |= XUI_MOD_ALT;
	return modifiers;
}

static int xideMapKey(int key)
{
	switch ( key ) {
	case XGE_KEY_ENTER: return XUI_KEY_ENTER;
	case XGE_KEY_TAB: return XUI_KEY_TAB;
	case XGE_KEY_BACKSPACE: return XUI_KEY_BACKSPACE;
	case XGE_KEY_DELETE: return XUI_KEY_DELETE;
	case XGE_KEY_LEFT: return XUI_KEY_LEFT;
	case XGE_KEY_RIGHT: return XUI_KEY_RIGHT;
	case XGE_KEY_UP: return XUI_KEY_UP;
	case XGE_KEY_DOWN: return XUI_KEY_DOWN;
	case XGE_KEY_PAGE_UP: return XUI_KEY_PAGE_UP;
	case XGE_KEY_PAGE_DOWN: return XUI_KEY_PAGE_DOWN;
	case XGE_KEY_HOME: return XUI_KEY_HOME;
	case XGE_KEY_END: return XUI_KEY_END;
	case XGE_KEY_F3: return XUI_KEY_F3;
	case XGE_KEY_MENU: return XUI_KEY_CONTEXT_MENU;
	default: return 0;
	}
}

static int xideSendKeyboard(xide_app_t* pApp)
{
	static const int keys[] = {
		XGE_KEY_ENTER, XGE_KEY_TAB, XGE_KEY_BACKSPACE, XGE_KEY_DELETE,
		XGE_KEY_LEFT, XGE_KEY_RIGHT, XGE_KEY_UP, XGE_KEY_DOWN,
		XGE_KEY_PAGE_UP, XGE_KEY_PAGE_DOWN, XGE_KEY_HOME, XGE_KEY_END,
		'A', 'C', 'F', 'H', 'S', 'V', 'X', 'Y', 'Z', '/',
		XGE_KEY_F3, XGE_KEY_MENU
	};
	uint32_t modifiers;
	uint32_t result;
	uint32_t text;
	int key;
	int i;
	int ret;
	modifiers = xideReadModifiers();
	ret = xuiInputSetModifiers(pApp->pContext, modifiers);
	if ( ret != XUI_OK ) return ret;
	if ( xgeKeyPressed(XGE_KEY_ESCAPE) ) xgeQuit();
	for ( i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++ ) {
		key = xideMapKey(keys[i]);
		if ( key == 0 ) key = keys[i];
		if ( xgeKeyPressed(keys[i]) ) {
			result = 0u;
			ret = xuiInputKeyDownEx(pApp->pContext, key, modifiers, &result);
			if ( ret != XUI_OK ) return ret;
			if ( (result & XUI_INPUT_RESULT_CONSUMED) != 0u ) xgeInputConsumeKey(keys[i]);
		}
		if ( xgeKeyReleased(keys[i]) ) {
			result = 0u;
			ret = xuiInputKeyUpEx(pApp->pContext, key, modifiers, &result);
			if ( ret != XUI_OK ) return ret;
			if ( (result & XUI_INPUT_RESULT_CONSUMED) != 0u ) xgeInputConsumeKey(keys[i]);
		}
	}
	while ( (text = xgeTextGet()) != 0 ) {
		if ( (modifiers & (XUI_MOD_CTRL | XUI_MOD_ALT)) == 0u ) {
			ret = xuiInputTextEx(pApp->pContext, text, NULL);
			if ( ret != XUI_OK ) return ret;
		}
	}
	return XUI_OK;
}

int xidePlatformHandleInput(xide_app_t* pApp)
{
	float x;
	float y;
	float wheelX;
	float wheelY;
	uint32_t buttons;
	uint32_t pressed;
	uint32_t released;
	int ret;
	if ( pApp == NULL || pApp->pContext == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	xgeMouseGet(&x, &y);
	xgeMouseGetWheel(&wheelX, &wheelY);
	buttons = xideReadButtons();
	if ( !pApp->bHasMouse || pApp->fLastMouseX != x || pApp->fLastMouseY != y || pApp->iLastButtons != buttons ) {
		ret = xuiInputPointerMove(pApp->pContext, x, y, buttons);
		if ( ret != XUI_OK ) return ret;
	}
	pressed = buttons & ~pApp->iLastButtons;
	released = pApp->iLastButtons & ~buttons;
	if ( (pressed & XUI_POINTER_BUTTON_LEFT) != 0 ) {
		ret = xuiInputPointerDown(pApp->pContext, x, y, XUI_POINTER_BUTTON_LEFT, buttons);
		if ( ret != XUI_OK ) return ret;
	}
	if ( (pressed & XUI_POINTER_BUTTON_RIGHT) != 0 ) {
		ret = xuiInputPointerDown(pApp->pContext, x, y, XUI_POINTER_BUTTON_RIGHT, buttons);
		if ( ret != XUI_OK ) return ret;
	}
	if ( (released & XUI_POINTER_BUTTON_LEFT) != 0 ) {
		ret = xuiInputPointerUp(pApp->pContext, x, y, XUI_POINTER_BUTTON_LEFT, buttons);
		if ( ret != XUI_OK ) return ret;
	}
	if ( (released & XUI_POINTER_BUTTON_RIGHT) != 0 ) {
		ret = xuiInputPointerUp(pApp->pContext, x, y, XUI_POINTER_BUTTON_RIGHT, buttons);
		if ( ret != XUI_OK ) return ret;
	}
	if ( wheelX != 0.0f || wheelY != 0.0f ) {
		ret = xuiInputPointerWheel(pApp->pContext, x, y, wheelX, wheelY, buttons);
		if ( ret != XUI_OK ) return ret;
	}
	ret = xideSendKeyboard(pApp);
	if ( ret != XUI_OK ) return ret;
	pApp->bHasMouse = 1;
	pApp->fLastMouseX = x;
	pApp->fLastMouseY = y;
	pApp->iLastButtons = buttons;
	return XUI_OK;
}
