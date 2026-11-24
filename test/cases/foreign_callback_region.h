#ifndef XLANG_TEST_FOREIGN_CALLBACK_REGION_H
#define XLANG_TEST_FOREIGN_CALLBACK_REGION_H

typedef struct xl_test_callback_rect {
    int iWidth, iHeight;
} xl_test_callback_rect;

typedef struct xl_test_callback_width {
    int mode;
    int* calls;
} xl_test_callback_width;

typedef struct xl_test_callback_height {
    int value;
} xl_test_callback_height;

typedef int (*xl_test_region_read)(int,int,const void*,int,void*);
typedef int (*xl_test_region_write)(xl_test_callback_rect,void*,int,void*);
typedef int (*xl_test_region_observed)(const xl_test_callback_width*,const xl_test_callback_height*,unsigned char*,int,void*);
int xl_test_callback_region_read(xl_test_region_read,void*,int);
int xl_test_callback_region_write(xl_test_region_write,void*);
int xl_test_callback_region_observed(xl_test_region_observed,void*,int);
int xl_test_callback_region_extent(const xl_test_callback_width*,const xl_test_callback_height*,xl_test_callback_rect*);

#endif
