#include "foreign_decl_factory.h"
#include <stdlib.h>
#include <assert.h>
struct foreign_factory_widget { int value; };
static int live;
int foreign_factory_config_read(const foreign_factory_config* config) { return config->value; }
int foreign_factory_create(int mode, foreign_factory_widget** output, const foreign_factory_config* config)
{
    assert(output && *output == NULL);
    if (mode == 2) return -7;
    if (mode == 3) return 0;
    *output = malloc(sizeof(**output));
    if (!*output) return -8;
    ++live; (*output)->value = config->value;
    return mode == 1 ? -9 : 0;
}
void foreign_factory_destroy(foreign_factory_widget* widget)
{ assert(widget); --live; free(widget); }
int foreign_factory_read(foreign_factory_widget* widget) { assert(widget); return widget->value; }
int foreign_factory_live(void) { return live; }
