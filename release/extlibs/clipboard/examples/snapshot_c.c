/* 快照拥有自己的副本；释放前可以多次恢复。 */
#include <stdio.h>
#include <string.h>
#include "../include/clipboard.h"
#ifdef __TINYC__
/* 快照范例仍须在隔离剪贴板的子进程中运行。 */
#pragma comment(lib, "user32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "shell32")
#include "../clipboard_native.c"
#endif

static int text_is(const char* expected)
{
	xvalue* value = x_clipboard_get_text();
	xstrview text;
	size_t size = strlen(expected);
	int equal = value != NULL && xrtValueGetString(value, &text) &&
		text.Size == size && memcmp(text.Data, expected, size) == 0;
	xrtValueRelease(value);
	return equal;
}

int main(void)
{
	void* saved;
	if ( !text_is("before") ) return 1;
	saved = x_clipboard_snapshot();
	if ( saved == NULL ) return 1;
	if ( !x_clipboard_set_text("temporary") || !x_clipboard_restore(saved) ||
		!text_is("before") ) {
		x_clipboard_snapshot_release(saved);
		return 1;
	}
	x_clipboard_snapshot_release(saved);
	if ( !x_clipboard_set_text("after") ) return 1;
	puts("clipboard snapshot ok");
	return 0;
}
