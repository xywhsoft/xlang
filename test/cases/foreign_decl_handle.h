#ifndef XL6_TEST_FOREIGN_DECL_HANDLE_H
#define XL6_TEST_FOREIGN_DECL_HANDLE_H

typedef struct foreign_decl_widget_data* foreign_decl_widget;

void* foreign_decl_widget_at(int index);
foreign_decl_widget foreign_decl_identity(foreign_decl_widget widget);
int foreign_decl_child_count(foreign_decl_widget widget);
int foreign_decl_add_child(foreign_decl_widget parent, foreign_decl_widget child);
void foreign_decl_expect(int actual, int expected);

#endif
