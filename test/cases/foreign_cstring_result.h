#ifndef FOREIGN_CSTRING_RESULT_H
#define FOREIGN_CSTRING_RESULT_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct foreign_cstring_widget { char text[64]; } foreign_cstring_widget;
const char* foreign_cstring_result(int mode);
const char* foreign_cstring_optional(int mode);
const char* foreign_cstring_echo(const char* input);
foreign_cstring_widget* foreign_cstring_widget_get(void);
const char* foreign_cstring_widget_text(foreign_cstring_widget* widget);
const char* foreign_cstring_widget_echo(foreign_cstring_widget* widget,const char* input);
void foreign_cstring_widget_set(foreign_cstring_widget* widget,const char* input);
void foreign_cstring_begin(void);
int foreign_cstring_allocations(void);
int foreign_cstring_calls(void);
void foreign_cstring_reset(void);
#ifdef __cplusplus
}
#endif
#endif
