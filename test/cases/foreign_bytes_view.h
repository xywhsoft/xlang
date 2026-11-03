#ifndef FOREIGN_BYTES_VIEW_H
#define FOREIGN_BYTES_VIEW_H
#include <stddef.h>
typedef struct xbytesview xbytesview;
typedef struct foreign_bytes_widget { unsigned char data[4]; } foreign_bytes_widget;
foreign_bytes_widget* foreign_bytes_widget_get(void);
xbytesview foreign_bytes_widget_echo(foreign_bytes_widget*,xbytesview);
xbytesview foreign_bytes_widget_data(foreign_bytes_widget*);
void foreign_bytes_widget_set(foreign_bytes_widget*,xbytesview);
#endif
