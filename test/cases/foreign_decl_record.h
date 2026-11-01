#ifndef FOREIGN_DECL_RECORD_H
#define FOREIGN_DECL_RECORD_H
#include <stddef.h>

typedef struct foreign_decl_rect {
    int x;
    int y;
    int width;
    int height;
} foreign_decl_rect;

typedef struct foreign_decl_transform {
    foreign_decl_rect bounds;
    double scale;
} foreign_decl_transform;

foreign_decl_rect foreign_decl_shift(foreign_decl_rect rect, int dx, int dy);
foreign_decl_transform foreign_decl_scale(foreign_decl_transform value, double factor);
int foreign_decl_rect_area(foreign_decl_rect rect);
void foreign_decl_record_expect(int actual, int expected);
const foreign_decl_rect* foreign_decl_rect_data(void);
int foreign_decl_rect_sum(const foreign_decl_rect* data, size_t count);
#endif
