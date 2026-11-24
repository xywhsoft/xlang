#ifndef XLANG_TEST_FOREIGN_CALLBACK_REGION_H
#define XLANG_TEST_FOREIGN_CALLBACK_REGION_H

typedef struct xl_test_callback_rect {
    int iWidth, iHeight;
} xl_test_callback_rect;

typedef int (*xl_test_region_read)(int,int,const void*,int,void*);
typedef int (*xl_test_region_write)(xl_test_callback_rect,void*,int,void*);
int xl_test_callback_region_read(xl_test_region_read,void*,int);
int xl_test_callback_region_write(xl_test_region_write,void*);

#endif
