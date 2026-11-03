#ifndef FOREIGN_TEXT_VIEW_H
#define FOREIGN_TEXT_VIEW_H
#include <stddef.h>
typedef struct xstrview xstrview;
typedef struct foreign_text_widget { char data[3]; } foreign_text_widget;
foreign_text_widget* foreign_text_widget_get(void);
xstrview foreign_text_widget_echo(foreign_text_widget*,xstrview);
xstrview foreign_text_widget_text(foreign_text_widget*);
void foreign_text_widget_set(foreign_text_widget*,xstrview);
#endif
