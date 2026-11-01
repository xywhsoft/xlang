#include "foreign_decl_record.h"
#include <stdlib.h>

foreign_decl_rect foreign_decl_shift(foreign_decl_rect rect, int dx, int dy)
{
    rect.x += dx;
    rect.y += dy;
    return rect;
}

foreign_decl_transform foreign_decl_scale(foreign_decl_transform value, double factor)
{
    value.scale *= factor;
    return value;
}

int foreign_decl_rect_area(foreign_decl_rect rect)
{
    return rect.width * rect.height;
}

void foreign_decl_record_expect(int actual, int expected)
{
    if (actual != expected) abort();
}

const foreign_decl_rect* foreign_decl_rect_data(void)
{
    static const foreign_decl_rect values[] = {{1, 2, 3, 4}, {5, 6, 7, 8}};
    return values;
}

int foreign_decl_rect_sum(const foreign_decl_rect* data, size_t count)
{
    if (data != foreign_decl_rect_data() || count != 2) abort();
    return data[0].width + data[1].width;
}
