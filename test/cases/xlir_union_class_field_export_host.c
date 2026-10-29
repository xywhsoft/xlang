#include <stdint.h>

int main(void)
{
    xvalue* box;
    int result = 0;
    if ( !xlir_module_init() ) return 1;
    box = xl6_box_make(42);
    if ( box == NULL ) return 2;
    if ( xl6_box_pick(box) != 42 ) result = 3;
    xrtValueRelease(box);
    return result;
}
