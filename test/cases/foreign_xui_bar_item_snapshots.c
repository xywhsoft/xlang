#undef NDEBUG
#include "test/cases/foreign_xui_bar_item_snapshots.h"
#include "test/cases/foreign_xui_render_backend.c"

static xmemdebugsnapshot before;

void barItemBegin(void)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
}

void barItemCheckBalance(void)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
}

void barItemNativeChecks(xui_widget menu, xui_widget menuBar, xui_widget toolbar, xui_widget statusBar)
{
    xui_menu_item_t menuItem;
    xui_menubar_item_t menuBarItem;
    xui_toolbar_item_t toolbarItem;
    xui_statusbar_item_t statusItem;
    barItemBegin();
    for (int i = 0; i < 1000; ++i) {
        assert(xuiMenuGetItemDescriptor(menu, 1, &menuItem) == XUI_OK);
        assert(xuiMenuBarGetItemDescriptor(menuBar, 0, &menuBarItem) == XUI_OK);
        assert(xuiToolbarGetItemDescriptor(toolbar, 0, &toolbarItem) == XUI_OK);
        assert(xuiStatusBarGetItemDescriptor(statusBar, 1, &statusItem) == XUI_OK);
        assert(menuItem.pSubmenu == xuiMenuGetItem(menu, 1)->pSubmenu);
        assert(menuBarItem.pMenu == xuiMenuBarGetItem(menuBar, 0)->pMenu);
        assert(toolbarItem.pIcon == xuiToolbarGetItem(toolbar, 0)->pIcon);
        assert(statusItem.fValue == xuiStatusBarGetItem(statusBar, 1)->fValue);
    }
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(after.ReallocCount == before.ReallocCount);
    assert(xuiMenuGetItemDescriptor(menu, -1, &menuItem) == XUI_ERROR_INVALID_ARGUMENT && menuItem.pSubmenu);
    assert(xuiMenuBarGetItemDescriptor(menuBar, -1, &menuBarItem) == XUI_ERROR_INVALID_ARGUMENT && menuBarItem.pMenu);
    assert(xuiToolbarGetItemDescriptor(toolbar, -1, &toolbarItem) == XUI_ERROR_INVALID_ARGUMENT && toolbarItem.pIcon);
    assert(xuiStatusBarGetItemDescriptor(statusBar, -1, &statusItem) == XUI_ERROR_INVALID_ARGUMENT && statusItem.fValue == 65);
}
