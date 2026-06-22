#include <stdint.h>

typedef struct XL_Class_____xl6__import__class__inherit__api__Entity {
    int64_t id;
} XL_Class_____xl6__import__class__inherit__api__Entity;

typedef struct XL_Class_____xl6__import__class__inherit__api__NamedPoint {
    XL_Class_____xl6__import__class__inherit__api__Entity base;
    int64_t x;
    int64_t y;
} XL_Class_____xl6__import__class__inherit__api__NamedPoint;

XL_Class_____xl6__import__class__inherit__api__NamedPoint xl6_class_inherit_api_make(int64_t id, int64_t x, int64_t y)
{
    XL_Class_____xl6__import__class__inherit__api__NamedPoint point;
    point.base.id = id;
    point.x = x;
    point.y = y;
    return point;
}

int64_t xl6_class_inherit_api_score(XL_Class_____xl6__import__class__inherit__api__NamedPoint point)
{
    return point.base.id + point.x + point.y;
}
