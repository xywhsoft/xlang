#ifndef FOREIGN_DECL_FACTORY_H
#define FOREIGN_DECL_FACTORY_H
typedef struct foreign_factory_widget foreign_factory_widget;
typedef struct foreign_factory_config { int value; } foreign_factory_config;
int foreign_factory_config_read(const foreign_factory_config* config);
int foreign_factory_create(int mode, foreign_factory_widget** output, const foreign_factory_config* config);
void foreign_factory_destroy(foreign_factory_widget* widget);
int foreign_factory_read(foreign_factory_widget* widget);
int foreign_factory_live(void);
void* foreign_factory_token_open(void);
int foreign_factory_token_calls(void);
struct xvalue;
int foreign_factory_create_token(struct xvalue*,foreign_factory_widget**,const foreign_factory_config*);
#endif
