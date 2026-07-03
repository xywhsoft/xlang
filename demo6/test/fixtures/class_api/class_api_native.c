#include <stdint.h>

typedef struct XL_Class_____xl6__import__class__api__Point {
    int64_t x;
    int64_t y;
} XL_Class_____xl6__import__class__api__Point;

XL_Class_____xl6__import__class__api__Point xl6_class_api_origin = {11, 31};

XL_Class_____xl6__import__class__api__Point xl6_class_api_make(int64_t x, int64_t y)
{
    XL_Class_____xl6__import__class__api__Point point;
    point.x = x;
    point.y = y;
    return point;
}

int64_t xl6_class_api_sum(XL_Class_____xl6__import__class__api__Point point)
{
    return point.x + point.y;
}
