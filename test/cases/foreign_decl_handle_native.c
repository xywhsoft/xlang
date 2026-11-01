/* Keep the explicit provider distinct from xlang's same-stem auto sidecar. */
#include "foreign_decl_handle.h"
#include <stdlib.h>

struct foreign_decl_widget_data {
    int value;
    int children;
};
static struct foreign_decl_widget_data widgets[2] = {{41, 0}, {1, 0}};

void* foreign_decl_widget_at(int index)
{
    if (index < 0 || index >= 2) abort();
    return &widgets[index];
}
foreign_decl_widget foreign_decl_identity(foreign_decl_widget widget)
{ return widget; }
int foreign_decl_child_count(foreign_decl_widget widget)
{ if (!widget) abort(); return widget->children; }
int foreign_decl_add_child(foreign_decl_widget parent, foreign_decl_widget child)
{
    if (parent != &widgets[0] || child != &widgets[1]) abort();
    parent->children += child->value;
    return parent->children;
}
void foreign_decl_expect(int actual, int expected)
{ if (actual != expected) abort(); }
