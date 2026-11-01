#ifndef FOREIGN_DECL_CALLBACK_H
#define FOREIGN_DECL_CALLBACK_H
typedef struct foreign_callback_widget foreign_callback_widget;
typedef struct foreign_callback_record { int number; double scale; } foreign_callback_record;
typedef foreign_callback_record (*foreign_callback_record_proc)(foreign_callback_widget*, foreign_callback_record);
typedef foreign_callback_widget* (*foreign_callback_pointer_proc)(foreign_callback_widget*);
void* foreign_callback_widget_at(void);
int foreign_callback_widget_value(foreign_callback_widget* widget);
foreign_callback_record foreign_callback_apply_record(foreign_callback_record_proc callback, foreign_callback_record input);
int foreign_callback_apply_pointer(foreign_callback_pointer_proc callback);
#endif
