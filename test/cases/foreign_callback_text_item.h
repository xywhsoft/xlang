#ifndef FOREIGN_CALLBACK_TEXT_ITEM_H
#define FOREIGN_CALLBACK_TEXT_ITEM_H

#include "xge3_runtime.h"

typedef struct xl_test_inline_text_t {
    char data[4];
    char sentinel;
    int size;
} xl_test_inline_text_t;

typedef int (*xl_test_inline_text_proc)(const xl_test_inline_text_t*,void*);

int xl_test_callback_text_item(xui_proxy,int);
int xl_test_inline_text(xl_test_inline_text_proc,void*,int);

#endif
